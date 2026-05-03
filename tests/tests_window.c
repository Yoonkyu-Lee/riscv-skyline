// tests_window.c -- window-related test cases for the MP1 test bench
//
// 10 cases (matches AG report):
//   test_add_one_window, test_add_multiple_windows,
//   test_remove_one_window, test_remove_multiple_window,
//   test_remove_window_not_in_array,
//   test_draw_window_simple, test_draw_window_null,
//   test_draw_window_clipping_side, test_draw_window_clipping_bottom,
//   test_draw_window_outside

#include <stdint.h>
#include <stddef.h>
#include "test_framework.h"
#include "skyline.h"
#include "fb.h"

// ---- helpers --------------------------------------------------------

// Return 1 iff index 0..win_cnt-1 contains a window matching all 5 fields.
static int window_array_has(uint16_t x, uint16_t y,
                            uint8_t w, uint8_t h, uint16_t color) {
    for (int i = 0; i < skyline_win_cnt; i++) {
        const struct skyline_window *e = &skyline_windows[i];
        if (e->x == x && e->y == y && e->w == w && e->h == h && e->color == color)
            return 1;
    }
    return 0;
}

// ---- 1. test_add_one_window (4pt) -----------------------------------

static int test_add_one_window(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);

    clobber_call((void (*)())add_window, 100, 50, 30, 20, 0xF800, 0);

    if (skyline_win_cnt != 1) {
        r->fail_reason = "skyline_win_cnt != 1 after one add_window";
        return 0;
    }
    if (!window_array_has(100, 50, 30, 20, 0xF800)) {
        r->fail_reason = "windows[0] does not match the inserted fields";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 2. test_add_multiple_windows (6pt) -----------------------------

static int test_add_multiple_windows(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);

    static const struct { uint16_t x, y; uint8_t w, h; uint16_t c; } pts[] = {
        {  10,  20,  4,  4, 0x001F },
        { 100, 100, 30, 20, 0x07E0 },
        { 200, 300, 50, 60, 0xF800 },
        { 600, 460, 10,  5, 0xFFFF },
    };
    int n = sizeof pts / sizeof pts[0];

    for (int i = 0; i < n; i++)
        clobber_call((void (*)())add_window, pts[i].x, pts[i].y,
                     pts[i].w, pts[i].h, pts[i].c, 0);

    if (skyline_win_cnt != n) {
        r->fail_reason = "skyline_win_cnt wrong after multiple add_window";
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (!window_array_has(pts[i].x, pts[i].y, pts[i].w, pts[i].h, pts[i].c)) {
            r->fail_reason = "an inserted window is missing from the array";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// ---- 3. test_remove_one_window (2pt) --------------------------------

static int test_remove_one_window(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);
    REQUIRE_IMPL(remove_window, r);

    clobber_call((void (*)())add_window,    100, 50, 30, 20, 0xF800, 0);
    clobber_call((void (*)())remove_window, 100, 50, 0, 0, 0, 0);

    if (skyline_win_cnt != 0) {
        r->fail_reason = "skyline_win_cnt != 0 after removing the only window";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 4. test_remove_multiple_window (4pt) ---------------------------
// add 5, remove the middle one, verify the remaining 4 are still in
// the contiguous prefix [0..cnt-1].  Order within that prefix does not
// matter.

static int test_remove_multiple_window(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);
    REQUIRE_IMPL(remove_window, r);

    static const struct { uint16_t x, y; uint8_t w, h; uint16_t c; } pts[] = {
        {  10,  20,  4,  4, 0x001F },
        { 100, 100, 30, 20, 0x07E0 },
        { 200, 300, 50, 60, 0xF800 },   // <-- target for removal
        { 600, 460, 10,  5, 0xFFFF },
        { 320, 240, 16, 16, 0x4444 },
    };
    int n = sizeof pts / sizeof pts[0];

    for (int i = 0; i < n; i++)
        clobber_call((void (*)())add_window, pts[i].x, pts[i].y,
                     pts[i].w, pts[i].h, pts[i].c, 0);

    clobber_call((void (*)())remove_window, 200, 300, 0, 0, 0, 0);

    if (skyline_win_cnt != n - 1) {
        r->fail_reason = "skyline_win_cnt != n-1 after one remove";
        return 0;
    }
    for (int i = 0; i < n; i++) {
        int should_have = (pts[i].x != 200 || pts[i].y != 300);
        int has = window_array_has(pts[i].x, pts[i].y, pts[i].w, pts[i].h, pts[i].c);
        if (should_have && !has) {
            r->fail_reason = "remove also dropped a window it shouldn't";
            return 0;
        }
        if (!should_have && has) {
            r->fail_reason = "removed window still in array";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// ---- 5. test_remove_window_not_in_array (4pt) -----------------------

static int test_remove_window_not_in_array(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);
    REQUIRE_IMPL(remove_window, r);

    // (a) empty array
    clobber_call((void (*)())remove_window, 100, 50, 0, 0, 0, 0);
    if (skyline_win_cnt != 0) {
        r->fail_reason = "remove on empty array changed win_cnt";
        return 0;
    }

    // (b) non-empty, no match
    clobber_call((void (*)())add_window,    100, 50, 30, 20, 0xF800, 0);
    clobber_call((void (*)())remove_window, 999, 999, 0, 0, 0, 0);
    if (skyline_win_cnt != 1) {
        r->fail_reason = "non-matching remove changed win_cnt";
        return 0;
    }
    if (!window_array_has(100, 50, 30, 20, 0xF800)) {
        r->fail_reason = "non-matching remove dropped the existing window";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 6. test_draw_window_simple (2pt) -------------------------------
// fully-on-screen rectangle.

static int test_draw_window_simple(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 100, .y = 50, .w = 30, .h = 20, .color = 0xF800 };

    ref_draw_window(fb_expected, &w);
    clobber_call((void (*)())draw_window, (uint64_t)(uintptr_t)fb_actual,
                                          (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_window pixel mismatch (simple)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 7. test_draw_window_null (1pt) ---------------------------------

static int test_draw_window_null(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    clobber_call((void (*)())draw_window, (uint64_t)(uintptr_t)fb_actual,
                                          0, 0, 0, 0, 0);
    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_window(fb, NULL) modified fb";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 8. test_draw_window_clipping_side (4pt) ------------------------
// rectangle extends past the right edge -- only left portion drawn.

static int test_draw_window_clipping_side(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 620, .y = 100, .w = 50, .h = 20, .color = 0x07E0 };
    // x+w = 670 > 640 -> 20 columns clipped on the right

    ref_draw_window(fb_expected, &w);
    clobber_call((void (*)())draw_window, (uint64_t)(uintptr_t)fb_actual,
                                          (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_window pixel mismatch (clipping side)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 9. test_draw_window_clipping_bottom (4pt) ----------------------

static int test_draw_window_clipping_bottom(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 100, .y = 470, .w = 30, .h = 50, .color = 0x001F };
    // y+h = 520 > 480 -> 40 rows clipped at the bottom

    ref_draw_window(fb_expected, &w);
    clobber_call((void (*)())draw_window, (uint64_t)(uintptr_t)fb_actual,
                                          (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_window pixel mismatch (clipping bottom)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 10. test_draw_window_outside (4pt) -----------------------------
// rectangle entirely off-screen -- fb must remain unchanged.

static int test_draw_window_outside(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 700, .y = 500, .w = 30, .h = 20, .color = 0xFFFF };

    ref_draw_window(fb_expected, &w);   // ref renders nothing
    clobber_call((void (*)())draw_window, (uint64_t)(uintptr_t)fb_actual,
                                          (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_window outside-screen modified fb";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- registry -------------------------------------------------------

static const struct test_entry window_tests[] = {
    { "test_add_one_window",            test_add_one_window,            4 },
    { "test_add_multiple_windows",      test_add_multiple_windows,      6 },
    { "test_remove_one_window",         test_remove_one_window,         2 },
    { "test_remove_multiple_window",    test_remove_multiple_window,    4 },
    { "test_remove_window_not_in_array",test_remove_window_not_in_array,4 },
    { "test_draw_window_simple",        test_draw_window_simple,        2 },
    { "test_draw_window_null",          test_draw_window_null,          1 },
    { "test_draw_window_clipping_side", test_draw_window_clipping_side, 4 },
    { "test_draw_window_clipping_bottom",test_draw_window_clipping_bottom,4 },
    { "test_draw_window_outside",       test_draw_window_outside,       4 },
};

const struct test_entry *get_window_tests(int *n_out) {
    *n_out = sizeof window_tests / sizeof window_tests[0];
    return window_tests;
}
