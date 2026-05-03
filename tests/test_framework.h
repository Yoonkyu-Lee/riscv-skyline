// test_framework.h -- shared declarations for the MP1 test bench
//
// Phase C scope: adds setjmp buffer + fault recovery state.  Will continue
// to grow in later phases (clobber mask, fb diff helpers, test registry).

#ifndef _TEST_FRAMEWORK_H_
#define _TEST_FRAMEWORK_H_

#include <stdint.h>

// ---- skyline globals (defined in test_globals.c) -------------------------

#include "skyline.h"

void test_globals_reset(void);

// ---- weak stub address symbols (defined in test_stubs.S) -----------------
//
// When mp1.S provides a strong definition for a function, ELF linking
// resolves the public symbol (e.g. add_star) to the strong version.  The
// _stub_<fn> alias still points into test_stubs.o.  When mp1.S does NOT
// provide the function, the public symbol resolves to the weak stub at the
// same address as _stub_<fn>.  The test runner detects "not implemented" by
// pointer comparison: if (&fn == &_stub_<fn>) then mp1.S is missing it.

extern char _stub_add_star;
extern char _stub_remove_star;
extern char _stub_draw_star;
extern char _stub_add_window;
extern char _stub_remove_window;
extern char _stub_draw_window;
extern char _stub_start_beacon;
extern char _stub_draw_beacon;

// ---- setjmp / longjmp (defined in setjmp.S) ------------------------------
//
// 13 doublewords: ra, sp, s0..s11.  Used by tests to recover from
// unexpected synchronous faults (NULL deref, misalignment, illegal
// instruction, etc.).  See setjmp.S for register layout.

struct test_jmp_buf {
    uint64_t ra;
    uint64_t sp;
    uint64_t s[12];
};

int  test_setjmp(struct test_jmp_buf *buf);
void test_longjmp(struct test_jmp_buf *buf, int val) __attribute__((noreturn));

// ---- fault recovery state (defined in halt_replace.c) --------------------
//
// When test_recover_armed != 0, fault_handler captures the cause and mepc,
// then calls test_longjmp(&test_recover_buf, 1) instead of panicking.
//
// Test runner pattern:
//   if (test_setjmp(&test_recover_buf) == 0) {
//       test_recover_armed = 1;
//       call_function_under_test(...);
//       test_recover_armed = 0;
//       /* PASS path */
//   } else {
//       test_recover_armed = 0;
//       /* FAIL path; cause = test_recover_cause, addr = test_recover_mepc */
//   }

extern volatile int      test_recover_armed;
extern volatile int      test_recover_cause;
extern volatile uint64_t test_recover_mepc;
extern struct test_jmp_buf test_recover_buf;

// ---- clobber detection (defined in clobber.S) ----------------------------
//
// Wraps a single call to /fn/ with up to 6 args (max needed: start_beacon).
// Sets clobber_mask: bit N set => sN was clobbered by /fn/ (12 bits, s0-s11).
//
// Test runner pattern:
//   clobber_mask = 0;
//   ret = clobber_call((void(*)())fn, a0, a1, a2, a3, a4, a5);
//   if (clobber_mask) record_clobber_violation(test_name, clobber_mask);

uint64_t clobber_call(void (*fn)(),
                      uint64_t a0, uint64_t a1, uint64_t a2,
                      uint64_t a3, uint64_t a4, uint64_t a5);
extern volatile uint32_t clobber_mask;

// ---- test runner (defined in test_runner.c) ------------------------------
//
// Each test fills out a struct test_result and returns 0/1 (informational).
// The actual scoring is taken from result.passed * result.max_score.

struct test_result {
    const char *name;            // test name (e.g. "test_add_single_star")
    int         max_score;       // points if passed
    int         passed;          // 0 = fail, 1 = pass
    int         score;           // result.passed * max_score (set by runner)
    const char *fail_reason;     // static string describing failure (or NULL)
};

typedef int (*test_fn_t)(struct test_result *r);

struct test_entry {
    const char *name;
    test_fn_t   fn;
    int         max_score;
};

// Walk an array of test_entry, run each via run_test(), accumulate score
// into *score_acc / *max_acc, return number that passed.  Penalty
// detection (clobber, missing impl) recorded separately.
void run_test_group(const char *header,
                    const struct test_entry *entries, int n,
                    int *score_acc, int *max_acc);

// Per-test driver -- public so groups (or extras) can call directly.
void run_test(const struct test_entry *entry,
              int *score_acc, int *max_acc);

// --- "function not implemented" check -------------------------------------
//
// Use as the first statement of every test so a pristine mp1.S produces
// FAIL with a clear reason instead of running into the bare ret stub.
//
// Implementation note: GCC will constant-fold a direct address comparison
// of two distinctly-named extern symbols to "always different".  We route
// through a runtime helper (addr_eq) defined in test_runner.c so the
// compiler must actually load both pointers and compare them.
//
// Example: REQUIRE_IMPL(add_star, r);

int addr_eq(const void *a, const void *b);

#define REQUIRE_IMPL(fn_, r_) do {                                     \
        if (addr_eq((const void *)&(fn_),                              \
                    (const void *)&_stub_##fn_)) {                     \
            (r_)->passed      = 0;                                     \
            (r_)->score       = 0;                                     \
            (r_)->fail_reason = "function not implemented (weak stub)";\
            return 0;                                                  \
        }                                                              \
    } while (0)

// ---- declarations of test groups (defined in tests_*.c) -----------------

// Returns a pointer to a static array of test_entry; *n_out is set to the
// length.  Phase F1: 1 entry each.  Later phases extend.

extern const struct test_entry *get_star_tests(int *n_out);
extern const struct test_entry *get_window_tests(int *n_out);
extern const struct test_entry *get_beacon_tests(int *n_out);
extern const struct test_entry *get_extra_tests(int *n_out);

#endif // _TEST_FRAMEWORK_H_
