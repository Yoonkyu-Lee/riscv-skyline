// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// test_main.c -- entry point for the test bench
//
// Replaces demo.o's main().  Phase C adds a recovery smoke test that
// proves trap-based fault recovery via setjmp/longjmp works end-to-end.
// Phase F will fill in the actual test runner loop.

#include <stdint.h>
#include "console.h"
#include "halt.h"
#include "test_framework.h"
#include "fb.h"

extern void memory_init(void);

// ---- recovery smoke test -------------------------------------------------
//
// Deliberately dereferences a NULL pointer with recovery armed.  The MMU
// (or memory layout, since address 0 is unmapped in the qemu virt machine)
// raises a load access fault.  trap.s saves the trap frame and calls
// fault_handler, which longjmps back into test_setjmp's call site with a
// non-zero return value.  We confirm by checking that we landed on the
// "else" branch with the expected cause code.

static void recovery_smoke_test(void) {
    kprintf("[recovery smoke test]\n");

    // Use a `volatile` indirection so the optimizer can't eliminate the
    // fault.  Reading from address 0 is invalid in qemu's virt machine.
    volatile int * null_ptr = (volatile int *)0;

    if (test_setjmp(&test_recover_buf) == 0) {
        test_recover_armed = 1;
        int unused = *null_ptr;        // expected to fault
        (void)unused;
        // If we get here, no fault was raised -- recovery infra is broken.
        test_recover_armed = 0;
        kprintf("  FAIL: NULL deref did not fault\n\n");
        return;
    }
    // longjmp landed here.  Recovery state captured by fault_handler.
    kprintf("  recovered from fault: cause=%d mepc=0x%lx\n",
            test_recover_cause, (long)test_recover_mepc);
    kprintf("  PASS\n\n");
}

// ---- clobber wrapper smoke test ------------------------------------------
//
// Verifies clobber_call distinguishes ABI-conforming from ABI-violating
// callees.  Helpers live in clobber.S:
//   clobber_test_good   -- bare ret, touches no callee-saved
//   clobber_test_bad_s0 -- writes garbage into s0 and rets without restore
//
// Expected: good case -> mask == 0; bad case -> mask & 1 != 0.

extern void clobber_test_good(void);
extern void clobber_test_bad_s0(void);

static void clobber_smoke_test(void) {
    kprintf("[clobber smoke test]\n");

    clobber_mask = 0;
    clobber_call((void (*)())clobber_test_good, 0, 0, 0, 0, 0, 0);
    if (clobber_mask == 0) {
        kprintf("  good fn:  mask=0x%x  PASS\n", (unsigned)clobber_mask);
    } else {
        kprintf("  good fn:  mask=0x%x  FAIL (false positive)\n",
                (unsigned)clobber_mask);
    }

    clobber_mask = 0;
    clobber_call((void (*)())clobber_test_bad_s0, 0, 0, 0, 0, 0, 0);
    if (clobber_mask & 0x1) {
        kprintf("  bad  fn:  mask=0x%x  PASS (s0 clobber detected)\n",
                (unsigned)clobber_mask);
    } else {
        kprintf("  bad  fn:  mask=0x%x  FAIL (missed s0 clobber)\n",
                (unsigned)clobber_mask);
    }
    kprintf("\n");
}

// ---- framebuffer smoke test ---------------------------------------------
//
// Verifies fb_clear/fb_diff_count and the reference drawers do what they
// claim:  (1) cleared buffers diff to 0,  (2) writing different pixels
// to actual vs expected produces diff,  (3) reference drawer + identical
// manual write produces no diff.

static void fb_smoke_test(void) {
    kprintf("[fb smoke test]\n");

    // (1) cleared -> no diff
    fb_clear_both(0);
    int d = fb_diff_count();
    if (d == 0) kprintf("  cleared: diff=0  PASS\n");
    else        kprintf("  cleared: diff=%d  FAIL\n", d);

    // (2) write differing pixels -> diff > 0
    fb_actual  [10 + 5 * SKYLINE_WIDTH] = 0xBEEF;
    fb_expected[10 + 5 * SKYLINE_WIDTH] = 0xCAFE;
    d = fb_diff_count();
    if (d == 1) kprintf("  one-pixel mismatch: diff=1  PASS\n");
    else        kprintf("  one-pixel mismatch: diff=%d  FAIL (expected 1)\n", d);

    // (3) ref drawer + identical write -> no diff
    fb_clear_both(0);
    struct skyline_star s = { .next = NULL, .x = 100, .y = 50, .color = 0xF800 };
    ref_draw_star(fb_expected, &s);
    fb_actual[100 + 50 * SKYLINE_WIDTH] = 0xF800;  // mimic correct draw_star
    d = fb_diff_count();
    if (d == 0) kprintf("  ref_draw_star: diff=0  PASS\n");
    else        kprintf("  ref_draw_star: diff=%d  FAIL\n", d);

    kprintf("\n");
}

// String for the banner -- kept as a separate function so the runner can be
// extended without touching boot order.
static void print_banner(void) {
    kprintf("\n");
    kprintf("==========================================\n");
    kprintf("[riscv-skyline test bench]\n");
    kprintf("==========================================\n");
    kprintf("\n");
}

// Probe each of the 8 weak-stub addresses against the publicly-visible
// symbol; print a one-line status.  This already gives a useful signal
// before any real tests exist: if a function name shows MISSING, the
// student hasn't written it yet.
//
// add_star, remove_star, etc. are declared in skyline.h (included via
// test_framework.h).  We take their addresses and compare to the
// _stub_<fn> addresses defined in test_stubs.S.
static void print_impl_status(void) {
    struct entry { const char * name; const void * fn; const void * stub; };

    struct entry e[] = {
        { "add_star",      (const void *)&add_star,      (const void *)&_stub_add_star      },
        { "remove_star",   (const void *)&remove_star,   (const void *)&_stub_remove_star   },
        { "draw_star",     (const void *)&draw_star,     (const void *)&_stub_draw_star     },
        { "add_window",    (const void *)&add_window,    (const void *)&_stub_add_window    },
        { "remove_window", (const void *)&remove_window, (const void *)&_stub_remove_window },
        { "draw_window",   (const void *)&draw_window,   (const void *)&_stub_draw_window   },
        { "start_beacon",  (const void *)&start_beacon,  (const void *)&_stub_start_beacon  },
        { "draw_beacon",   (const void *)&draw_beacon,   (const void *)&_stub_draw_beacon   },
    };

    kprintf("[mp1.S implementation status]\n");
    for (size_t i = 0; i < sizeof e / sizeof e[0]; i++) {
        int missing = (e[i].fn == e[i].stub);
        kprintf("  %s: %s\n", e[i].name,
                missing ? "MISSING (weak stub)" : "implemented");
    }
    kprintf("\n");
}

void main(void) {
    console_init();
    memory_init();

    print_banner();
    test_globals_reset();
    recovery_smoke_test();
    clobber_smoke_test();
    fb_smoke_test();
    print_impl_status();

    // ---- run baseline tests --------------------------------------------
    int score = 0, max = 0;

    int n;
    const struct test_entry *e;

    e = get_star_tests(&n);
    run_test_group("Star tests", e, n, &score, &max);

    e = get_window_tests(&n);
    run_test_group("Window tests", e, n, &score, &max);

    e = get_beacon_tests(&n);
    run_test_group("Beacon tests", e, n, &score, &max);

    // ---- baseline summary ----------------------------------------------
    extern volatile int  test_clobber_penalty_applied;
    extern const char   *test_first_clobber_test;

    int penalty = 0;
    if (test_clobber_penalty_applied)
        penalty = 15;

    kprintf("==========================================\n");
    kprintf("Functionality: %d/%d\n", score, max);
    if (penalty)
        kprintf("Penalties: -%d (CLOBBER, first triggered by %s)\n",
                penalty, test_first_clobber_test ? test_first_clobber_test : "?");
    else
        kprintf("Penalties: none\n");
    kprintf("Subtotal: %d/%d\n", score - penalty, max);
    kprintf("==========================================\n\n");

    // ---- run extras (informational, not part of the 85pt) --------------
    int extra_score = 0, extra_max = 0;
    e = get_extra_tests(&n);
    run_test_group("Extra (bonus / diagnostic)", e, n, &extra_score, &extra_max);

    kprintf("==========================================\n");
    kprintf("Extras: %d/%d passed\n", extra_score, extra_max);
    kprintf("Total (baseline only): %d/%d\n", score - penalty, max);
    kprintf("==========================================\n");

    halt_success();
}
