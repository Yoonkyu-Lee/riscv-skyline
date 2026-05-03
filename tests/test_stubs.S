        # test_stubs.S -- weak stubs for the 8 mp1.S student functions
        #
        # Purpose:
        #   When mp1.S is pristine (only skyline_init defined), the linker
        #   would otherwise reject test.elf with undefined references for
        #   add_star..draw_beacon.  Each stub here is .weak, so a strong
        #   definition from mp1.S overrides it automatically; otherwise the
        #   weak version is used and test.elf links.
        #
        # Detection:
        #   Each stub has a parallel global symbol _stub_<fn> at the same
        #   address.  The runner does &fn == &_stub_<fn> -- if equal, the
        #   weak stub is in play and mp1.S did not implement the function.
        #   No flag bytes, no preflight calls.
        #
        # Body:
        #   A bare `ret`.  We do NOT want side effects (a panic, a write to
        #   a global, etc.) because the runner detects missing-impl by
        #   address before even calling the function.

        .text

        .weak   add_star
        .type   add_star, @function
add_star:
        .global _stub_add_star
        .type   _stub_add_star, @function
_stub_add_star:
        ret

        .weak   remove_star
        .type   remove_star, @function
remove_star:
        .global _stub_remove_star
        .type   _stub_remove_star, @function
_stub_remove_star:
        ret

        .weak   draw_star
        .type   draw_star, @function
draw_star:
        .global _stub_draw_star
        .type   _stub_draw_star, @function
_stub_draw_star:
        ret

        .weak   add_window
        .type   add_window, @function
add_window:
        .global _stub_add_window
        .type   _stub_add_window, @function
_stub_add_window:
        ret

        .weak   remove_window
        .type   remove_window, @function
remove_window:
        .global _stub_remove_window
        .type   _stub_remove_window, @function
_stub_remove_window:
        ret

        .weak   draw_window
        .type   draw_window, @function
draw_window:
        .global _stub_draw_window
        .type   _stub_draw_window, @function
_stub_draw_window:
        ret

        .weak   start_beacon
        .type   start_beacon, @function
start_beacon:
        .global _stub_start_beacon
        .type   _stub_start_beacon, @function
_stub_start_beacon:
        ret

        .weak   draw_beacon
        .type   draw_beacon, @function
draw_beacon:
        .global _stub_draw_beacon
        .type   _stub_draw_beacon, @function
_stub_draw_beacon:
        ret

        .end
