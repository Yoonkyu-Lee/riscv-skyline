// tests_extra.c -- bonus / diagnostic tests beyond the 24 AG baseline
//
// These don't contribute to the 85pt score; they exist to surface bugs the
// original AG didn't probe (overflow, struct layout, defensive coding,
// clipping arithmetic, stress).  Reported in a separate "Extras" section.

#include <stdint.h>
#include <stddef.h>
#include "test_framework.h"
#include "skyline.h"
#include "fb.h"

// ---- helpers (some duplicate of star/window helpers; kept local) ----

static int star_list_length_local(void) {
    int n = 0;
    for (struct skyline_star *s = skyline_star_list; s; s = s->next) n++;
    return n;
}

static int window_array_has_local(uint16_t x, uint16_t y,
                                  uint8_t w, uint8_t h, uint16_t color) {
    for (int i = 0; i < skyline_win_cnt; i++) {
        const struct skyline_window *e = &skyline_windows[i];
        if (e->x == x && e->y == y && e->w == w && e->h == h && e->color == color)
            return 1;
    }
    return 0;
}

// ---- 1. extra_star_stress -------------------------------------------
// Add 1000 stars then remove them all; the list must end empty.  Catches
// missing free, head/tail pointer bugs, and small malloc/free imbalances.

static int extra_star_stress(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);
    REQUIRE_IMPL(remove_star, r);

    const int N = 1000;
    for (int i = 0; i < N; i++)
        clobber_call((void (*)())add_star, i % SKYLINE_WIDTH, i % SKYLINE_HEIGHT,
                     (uint16_t)(i & 0xFFFF), 0, 0, 0);

    if (star_list_length_local() != N) {
        r->fail_reason = "list length wrong after N add_star";
        return 0;
    }

    // Note: we may add multiple stars at the same (x,y) when N > screen
    // area or when the modulo collides (it can with 1000 inserts and
    // 640x480).  remove_star is only required to remove "any one" star
    // at that coordinate.  We just check that the list eventually
    // empties when we call remove_star repeatedly.
    for (int i = 0; i < N; i++)
        clobber_call((void (*)())remove_star, i % SKYLINE_WIDTH, i % SKYLINE_HEIGHT,
                     0, 0, 0, 0);

    if (skyline_star_list != NULL) {
        r->fail_reason = "list not empty after N remove_star calls";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 2. extra_window_overflow ---------------------------------------
// Add SKYLINE_WIN_MAX + 1 windows; the last one must be silently
// ignored.  win_cnt must clamp at SKYLINE_WIN_MAX.

static int extra_window_overflow(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);

    for (int i = 0; i < SKYLINE_WIN_MAX + 1; i++)
        clobber_call((void (*)())add_window,
                     (uint16_t)(i % SKYLINE_WIDTH),
                     (uint16_t)(i % SKYLINE_HEIGHT),
                     1, 1, (uint16_t)(i & 0xFFFF), 0);

    if (skyline_win_cnt != SKYLINE_WIN_MAX) {
        r->fail_reason = "win_cnt did not clamp at SKYLINE_WIN_MAX";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 3. extra_window_remove_compaction ------------------------------
// Add 5 windows, remove the middle one, verify the *prefix* indices
// 0..3 contain valid (non-removed) windows.  i.e. the array remains
// contiguous from the start with no holes.

static int extra_window_remove_compaction(struct test_result *r) {
    REQUIRE_IMPL(add_window, r);
    REQUIRE_IMPL(remove_window, r);

    static const struct { uint16_t x, y; uint8_t w, h; uint16_t c; } pts[] = {
        {  10,  20,  4,  4, 0x001F },
        { 100, 100, 30, 20, 0x07E0 },
        { 200, 300, 50, 60, 0xF800 },   // <-- removed
        { 600, 460, 10,  5, 0xFFFF },
        { 320, 240, 16, 16, 0x4444 },
    };
    int n = sizeof pts / sizeof pts[0];

    for (int i = 0; i < n; i++)
        clobber_call((void (*)())add_window, pts[i].x, pts[i].y,
                     pts[i].w, pts[i].h, pts[i].c, 0);
    clobber_call((void (*)())remove_window, 200, 300, 0, 0, 0, 0);

    if (skyline_win_cnt != n - 1) {
        r->fail_reason = "win_cnt wrong after remove";
        return 0;
    }
    // Each surviving window must appear somewhere in [0..n-2].
    for (int i = 0; i < n; i++) {
        int should_have = (pts[i].x != 200 || pts[i].y != 300);
        int has = window_array_has_local(pts[i].x, pts[i].y,
                                         pts[i].w, pts[i].h, pts[i].c);
        if (should_have != has) {
            r->fail_reason = "compaction left a hole or moved a wrong entry";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// ---- 4. extra_draw_star_corners -------------------------------------
// Single-pixel writes at the four exact corners of the screen.

static int extra_draw_star_corners(struct test_result *r) {
    REQUIRE_IMPL(draw_star, r);

    fb_clear_both(0);
    static const struct skyline_star corners[] = {
        { .next=NULL, .x =   0, .y =   0, .color = 0x1111 },
        { .next=NULL, .x = 639, .y =   0, .color = 0x2222 },
        { .next=NULL, .x =   0, .y = 479, .color = 0x3333 },
        { .next=NULL, .x = 639, .y = 479, .color = 0x4444 },
    };
    int n = sizeof corners / sizeof corners[0];

    for (int i = 0; i < n; i++) {
        ref_draw_star(fb_expected, &corners[i]);
        clobber_call((void (*)())draw_star,
                     (uint64_t)(uintptr_t)fb_actual,
                     (uint64_t)(uintptr_t)&corners[i], 0, 0, 0, 0);
    }

    if (fb_diff_count() != 0) {
        r->fail_reason = "corner pixels mismatch";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 5. extra_draw_window_full_screen -------------------------------
// w=255, h=255 (uint8_t max).  Stresses zero-extension of the byte
// fields when iterating the rectangle in mp1.S.

static int extra_draw_window_full_screen(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 0, .y = 0, .w = 255, .h = 255, .color = 0x07E0 };

    ref_draw_window(fb_expected, &w);
    clobber_call((void (*)())draw_window,
                 (uint64_t)(uintptr_t)fb_actual,
                 (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "uint8_t-max-sized window pixel mismatch";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 6. extra_beacon_period_zero ------------------------------------
// Spec doesn't specify behavior; defensive impls will skip drawing.
// Test passes iff the call returns without raising a fault.  The
// recovery infra will mark FAIL with a fault reason if a div-by-zero
// trap fires.

static const uint16_t small_img[] = {
    0xAAAA, 0xBBBB,
    0xCCCC, 0xDDDD,
};

static int extra_beacon_period_zero(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    fb_clear_both(0);
    struct skyline_beacon b = {
        .img    = small_img,
        .x      = 100, .y = 100,
        .dia    = 2,
        .period = 0,
        .ontime = 0,
    };
    clobber_call((void (*)())draw_beacon,
                 (uint64_t)(uintptr_t)fb_actual,
                 0,
                 (uint64_t)(uintptr_t)&b, 0, 0, 0);
    // If we got here, no fault.  The output is implementation-defined
    // (period=0 is undefined per spec); we just require no crash.
    r->passed = 1;
    return 1;
}

// ---- 7. extra_beacon_t_wrap -----------------------------------------
// Huge t value -- tests 64-bit modulo correctness in mp1.S's
// implementation of (t % period < ontime).

static int extra_beacon_t_wrap(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    fb_clear_both(0);
    struct skyline_beacon b = {
        .img    = small_img,
        .x      = 100, .y = 100,
        .dia    = 2,
        .period = 100,
        .ontime = 50,
    };
    uint64_t huge_t = (uint64_t)0xFFFFFFFFFFFFFFFEull;

    ref_draw_beacon(fb_expected, huge_t, &b);
    clobber_call((void (*)())draw_beacon,
                 (uint64_t)(uintptr_t)fb_actual,
                 huge_t,
                 (uint64_t)(uintptr_t)&b, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_beacon(huge t) doesn't match ref (modulo bug?)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 8. extra_start_beacon_struct_layout ----------------------------
// After start_beacon, read the global struct byte-by-byte and verify
// the field offsets match what skyline.h says.  Catches misaligned
// stores or wrong offsets in mp1.S.

static int extra_start_beacon_struct_layout(struct test_result *r) {
    REQUIRE_IMPL(start_beacon, r);

    // Sentinel arguments
    static const uint16_t img_sent[1] = { 0 };
    clobber_call((void (*)())start_beacon,
                 (uint64_t)(uintptr_t)img_sent,
                 0xABCDu, 0xCAFEu, 0x55u, 0x1234u, 0x5678u);

    if (skyline_beacon.img    != img_sent)         { r->fail_reason = "img field offset/value wrong";    return 0; }
    if (skyline_beacon.x      != 0xABCDu)          { r->fail_reason = "x field offset/value wrong";      return 0; }
    if (skyline_beacon.y      != 0xCAFEu)          { r->fail_reason = "y field offset/value wrong";      return 0; }
    if (skyline_beacon.dia    != 0x55u)            { r->fail_reason = "dia field offset/value wrong";    return 0; }
    if (skyline_beacon.period != 0x1234u)          { r->fail_reason = "period field offset/value wrong"; return 0; }
    if (skyline_beacon.ontime != 0x5678u)          { r->fail_reason = "ontime field offset/value wrong"; return 0; }

    r->passed = 1;
    return 1;
}

// ---- 9. extra_clipping_negative -------------------------------------
// uint16_t x = 0xFFF0 (huge unsigned value, "off the right edge").
// Some buggy impls treat this as signed, get a "negative" coord,
// and try to draw at large positive virtual address -> fault.
// Ref drawer correctly clips; the fb must remain all zero.

static int extra_clipping_negative(struct test_result *r) {
    REQUIRE_IMPL(draw_window, r);

    fb_clear_both(0);
    struct skyline_window w = { .x = 0xFFF0, .y = 100, .w = 5, .h = 5, .color = 0xBEEF };

    ref_draw_window(fb_expected, &w);   // ref draws nothing
    clobber_call((void (*)())draw_window,
                 (uint64_t)(uintptr_t)fb_actual,
                 (uint64_t)(uintptr_t)&w, 0, 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = "huge-x window incorrectly modified fb";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 10. extra_alternating_add_remove -------------------------------
// Add then remove the same star in a tight loop.  Catches incorrect
// list head pointer updates (e.g. dangling pointer to freed node).

static int extra_alternating_add_remove(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);
    REQUIRE_IMPL(remove_star, r);

    for (int i = 0; i < 100; i++) {
        clobber_call((void (*)())add_star, 50, 50, 0xF800, 0, 0, 0);
        if (skyline_star_list == NULL) {
            r->fail_reason = "list NULL right after add_star";
            return 0;
        }
        clobber_call((void (*)())remove_star, 50, 50, 0, 0, 0, 0);
        if (skyline_star_list != NULL) {
            r->fail_reason = "list non-NULL right after matched remove";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// ---- registry -------------------------------------------------------

static const struct test_entry extra_tests[] = {
    { "extra_star_stress",                   extra_star_stress,                   1 },
    { "extra_window_overflow",               extra_window_overflow,               1 },
    { "extra_window_remove_compaction",      extra_window_remove_compaction,      1 },
    { "extra_draw_star_corners",             extra_draw_star_corners,             1 },
    { "extra_draw_window_full_screen",       extra_draw_window_full_screen,       1 },
    { "extra_beacon_period_zero",            extra_beacon_period_zero,            1 },
    { "extra_beacon_t_wrap",                 extra_beacon_t_wrap,                 1 },
    { "extra_start_beacon_struct_layout",    extra_start_beacon_struct_layout,    1 },
    { "extra_clipping_negative",             extra_clipping_negative,             1 },
    { "extra_alternating_add_remove",        extra_alternating_add_remove,        1 },
};

const struct test_entry *get_extra_tests(int *n_out) {
    *n_out = sizeof extra_tests / sizeof extra_tests[0];
    return extra_tests;
}
