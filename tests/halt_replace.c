// halt_replace.c -- drop-in replacement for halt.c with fault recovery
//
// This module replaces halt.o for test.elf.  It exports the same four
// symbols (halt_success, halt_failure, panic, fault_handler), but adds
// a recoverable path through fault_handler so individual tests can cause
// synchronous faults (NULL deref, misalignment) without bringing down
// the whole bench.
//
// Recovery protocol:
//   1. Test runner calls test_setjmp(&test_recover_buf) -- saves frame.
//   2. Runner sets test_recover_armed = 1 just before invoking the
//      function under test.
//   3. If the function faults, trap.s pushes a trap frame and calls
//      fault_handler.  fault_handler sees armed != 0, captures the
//      cause + mepc into globals, and longjmps back to the runner.
//   4. Runner clears test_recover_armed after the call.

#include <stdint.h>
#include <stddef.h>
#include "halt.h"
#include "trap.h"
#include "console.h"
#include "test_framework.h"

// ---- recovery state ------------------------------------------------------

volatile int      test_recover_armed = 0;
volatile int      test_recover_cause = 0;
volatile uint64_t test_recover_mepc  = 0;
struct test_jmp_buf test_recover_buf;

// ---- halt / panic --------------------------------------------------------

void halt_success(void) {
    *(int *)0x100000 = 0x5555;  // qemu virt test device: success exit
    for (;;) continue;
}

void halt_failure(void) {
    *(int *)0x100000 = 0x3333;  // qemu virt test device: failure exit
    for (;;) continue;
}

void panic(const char * msg) {
    if (msg != NULL)
        console_puts(msg);
    halt_failure();
}

// ---- fault_handler -------------------------------------------------------
//
// Called from trap.s (_trap_entry) on any synchronous exception
// (mcause MSB clear).  If recovery is armed, capture cause + mepc and
// longjmp back to the runner.  Otherwise fall back to original behavior.

void fault_handler(int code, struct trap_frame * tfr) {
    if (test_recover_armed) {
        test_recover_cause = code;
        test_recover_mepc  = tfr->mepc;
        test_recover_armed = 0;          // disarm before unwinding
        test_longjmp(&test_recover_buf, 1);
        // unreachable
    }
    kprintf("PANIC Unhandled fault %d at 0x%lx\n", code, (long)tfr->mepc);
    panic(NULL);
}
