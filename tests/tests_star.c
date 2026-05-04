// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// tests_star.c -- star-related test cases for the test bench
//
// 8 cases (matches AG report):
//   test_add_single_star, test_add_multiple_stars,
//   test_remove_single_star, test_remove_multiple_star,
//   test_remove_star_not_in_list,
//   test_draw_star_simple, test_draw_star_null, test_draw_star_complex
//
// Each test starts with REQUIRE_IMPL(fn, r) so that a pristine mp1.S
// produces a clean "function not implemented" failure rather than running
// the (no-op) weak stub.

#include <stdint.h>
#include <stddef.h>
#include "test_framework.h"
#include "skyline.h"
#include "fb.h"

// ---- helpers --------------------------------------------------------

// Walk the linked list at skyline_star_list and count nodes.
static int star_list_length(void) {
    int n = 0;
    for (struct skyline_star *s = skyline_star_list; s; s = s->next)
        n++;
    return n;
}

// Return 1 iff the linked list contains a node with these (x,y,color).
static int star_list_has(uint16_t x, uint16_t y, uint16_t color) {
    for (struct skyline_star *s = skyline_star_list; s; s = s->next) {
        if (s->x == x && s->y == y && s->color == color)
            return 1;
    }
    return 0;
}

// ---- individual tests -----------------------------------------------

// 1. test_add_single_star (4pt) -- one add_star, list head matches.
static int test_add_single_star(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);

    clobber_call((void (*)())add_star, 100, 50, 0xF800, 0, 0, 0);

    if (skyline_star_list == NULL) {
        r->fail_reason = "skyline_star_list is NULL after add_star";
        return 0;
    }
    if (star_list_length() != 1) {
        r->fail_reason = "list length != 1 after one add_star";
        return 0;
    }
    if (!star_list_has(100, 50, 0xF800)) {
        r->fail_reason = "list does not contain (100,50,0xF800)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// 2. test_add_multiple_stars (6pt) -- 5 different stars; verify all
//    present, list length is 5.
static int test_add_multiple_stars(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);

    static const struct { uint16_t x, y, c; } pts[] = {
        {  10,  20, 0x001F },   // blue
        { 100, 200, 0x07E0 },   // green
        { 320, 240, 0xF800 },   // red
        { 638, 478, 0xFFFF },   // white near corner
        {   0,   0, 0x4444 },   // black-ish at origin
    };
    int n = sizeof pts / sizeof pts[0];

    for (int i = 0; i < n; i++)
        clobber_call((void (*)())add_star, pts[i].x, pts[i].y, pts[i].c, 0, 0, 0);

    if (star_list_length() != n) {
        r->fail_reason = "list length wrong after multiple add_star";
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (!star_list_has(pts[i].x, pts[i].y, pts[i].c)) {
            r->fail_reason = "an inserted star is missing from the list";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// 3. test_remove_single_star (2pt) -- add 1, remove same coords, empty.
static int test_remove_single_star(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);
    REQUIRE_IMPL(remove_star, r);

    clobber_call((void (*)())add_star,    100, 50, 0xF800, 0, 0, 0);
    clobber_call((void (*)())remove_star, 100, 50, 0, 0, 0, 0);

    if (skyline_star_list != NULL) {
        r->fail_reason = "list not empty after remove of only star";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// 4. test_remove_multiple_star (4pt) -- add 5, remove the middle one,
//    verify remaining 4 still present and length is 4.
static int test_remove_multiple_star(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);
    REQUIRE_IMPL(remove_star, r);

    static const struct { uint16_t x, y, c; } pts[] = {
        {  10,  20, 0x001F },
        { 100, 200, 0x07E0 },
        { 320, 240, 0xF800 },   // <-- target for removal
        { 638, 478, 0xFFFF },
        {   0,   0, 0x4444 },
    };
    int n = sizeof pts / sizeof pts[0];

    for (int i = 0; i < n; i++)
        clobber_call((void (*)())add_star, pts[i].x, pts[i].y, pts[i].c, 0, 0, 0);

    clobber_call((void (*)())remove_star, 320, 240, 0, 0, 0, 0);

    if (star_list_length() != n - 1) {
        r->fail_reason = "list length != n-1 after one remove";
        return 0;
    }
    for (int i = 0; i < n; i++) {
        int should_have = (pts[i].x != 320 || pts[i].y != 240);
        int has = star_list_has(pts[i].x, pts[i].y, pts[i].c);
        if (should_have && !has) {
            r->fail_reason = "remove also affected a star it shouldn't";
            return 0;
        }
        if (!should_have && has) {
            r->fail_reason = "removed star still in list";
            return 0;
        }
    }
    r->passed = 1;
    return 1;
}

// 5. test_remove_star_not_in_list (4pt) -- two phases:
//    a) empty list, remove_star(...) must not crash
//    b) non-empty list, remove with non-matching coords -> list unchanged
static int test_remove_star_not_in_list(struct test_result *r) {
    REQUIRE_IMPL(add_star, r);
    REQUIRE_IMPL(remove_star, r);

    // (a) empty list
    clobber_call((void (*)())remove_star, 100, 50, 0, 0, 0, 0);
    if (skyline_star_list != NULL) {
        r->fail_reason = "remove on empty list created a node?";
        return 0;
    }

    // (b) non-empty, no match
    clobber_call((void (*)())add_star,    100, 50, 0xF800, 0, 0, 0);
    clobber_call((void (*)())remove_star, 999, 999, 0, 0, 0, 0);
    if (star_list_length() != 1) {
        r->fail_reason = "non-matching remove changed list length";
        return 0;
    }
    if (!star_list_has(100, 50, 0xF800)) {
        r->fail_reason = "non-matching remove dropped the existing star";
        return 0;
    }

    r->passed = 1;
    return 1;
}

// 6. test_draw_star_simple (4pt) -- draw one star to fb, compare against
//    ref_draw_star.
static int test_draw_star_simple(struct test_result *r) {
    REQUIRE_IMPL(draw_star, r);

    fb_clear_both(0);
    struct skyline_star s = { .next = NULL, .x = 100, .y = 50, .color = 0xF800 };

    ref_draw_star(fb_expected, &s);
    clobber_call((void (*)())draw_star, (uint64_t)(uintptr_t)fb_actual,
                                        (uint64_t)(uintptr_t)&s, 0, 0, 0, 0);

    int d = fb_diff_count();
    if (d != 0) {
        r->fail_reason = "draw_star produced wrong fbuf";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// 7. test_draw_star_null (1pt) -- draw_star(fb, NULL) must not crash.
static int test_draw_star_null(struct test_result *r) {
    REQUIRE_IMPL(draw_star, r);

    fb_clear_both(0);
    clobber_call((void (*)())draw_star, (uint64_t)(uintptr_t)fb_actual,
                                        0, 0, 0, 0, 0);
    // If we got here, no fault was raised.  fb should still be all zero
    // (NULL star draws nothing).
    int d = fb_diff_count();
    if (d != 0) {
        r->fail_reason = "draw_star(fb, NULL) modified fb (should be no-op)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// 8. test_draw_star_complex (5pt) -- multiple stars, draw each, verify
//    the resulting fb matches ref drawing.  Uses both inside-screen and
//    out-of-screen coords to verify clipping in draw_star itself.
static int test_draw_star_complex(struct test_result *r) {
    REQUIRE_IMPL(draw_star, r);

    fb_clear_both(0);
    static const struct skyline_star stars[] = {
        { .next=NULL, .x =   0, .y =   0, .color = 0x4444 },  // top-left
        { .next=NULL, .x = 639, .y = 479, .color = 0xFFFF },  // bottom-right
        { .next=NULL, .x = 320, .y = 240, .color = 0x07E0 },  // center
        { .next=NULL, .x = 100, .y = 200, .color = 0x001F },  // arbitrary
        { .next=NULL, .x = 640, .y =   0, .color = 0xDEAD },  // OFF-screen X (clip)
        { .next=NULL, .x =   0, .y = 480, .color = 0xBEEF },  // OFF-screen Y (clip)
    };
    int n = sizeof stars / sizeof stars[0];

    for (int i = 0; i < n; i++) {
        ref_draw_star(fb_expected, &stars[i]);
        clobber_call((void (*)())draw_star,
                     (uint64_t)(uintptr_t)fb_actual,
                     (uint64_t)(uintptr_t)&stars[i], 0, 0, 0, 0);
    }

    int d = fb_diff_count();
    if (d != 0) {
        r->fail_reason = "draw_star complex (clipping or wrong color)";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- registry -------------------------------------------------------

static const struct test_entry star_tests[] = {
    { "test_add_single_star",       test_add_single_star,       4 },
    { "test_add_multiple_stars",    test_add_multiple_stars,    6 },
    { "test_remove_single_star",    test_remove_single_star,    2 },
    { "test_remove_multiple_star",  test_remove_multiple_star,  4 },
    { "test_remove_star_not_in_list", test_remove_star_not_in_list, 4 },
    { "test_draw_star_simple",      test_draw_star_simple,      4 },
    { "test_draw_star_null",        test_draw_star_null,        1 },
    { "test_draw_star_complex",     test_draw_star_complex,     5 },
};

const struct test_entry *get_star_tests(int *n_out) {
    *n_out = sizeof star_tests / sizeof star_tests[0];
    return star_tests;
}
