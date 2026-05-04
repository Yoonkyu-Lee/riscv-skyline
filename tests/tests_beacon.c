// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// tests_beacon.c -- beacon-related test cases for the test bench
//
// 6 cases (matches AG report):
//   test_start_beacon, test_draw_beacon_simple, test_draw_beacon_null,
//   test_draw_beacon_clipping_side, test_draw_beacon_clipping_bottom,
//   test_draw_beacon_outside

#include <stdint.h>
#include <stddef.h>
#include "test_framework.h"
#include "skyline.h"
#include "fb.h"

// ---- shared image data -----------------------------------------------
// 8x8 = 64 colors, used by all draw_beacon tests.  Distinct values so
// any wrong index/stride shows up immediately as a pixel mismatch.

static const uint16_t bcn_img[8 * 8] = {
    0x1100, 0x2200, 0x3300, 0x4400, 0x5500, 0x6600, 0x7700, 0x8800,
    0x1101, 0x2201, 0x3301, 0x4401, 0x5501, 0x6601, 0x7701, 0x8801,
    0x1102, 0x2202, 0x3302, 0x4402, 0x5502, 0x6602, 0x7702, 0x8802,
    0x1103, 0x2203, 0x3303, 0x4403, 0x5503, 0x6603, 0x7703, 0x8803,
    0x1104, 0x2204, 0x3304, 0x4404, 0x5504, 0x6604, 0x7704, 0x8804,
    0x1105, 0x2205, 0x3305, 0x4405, 0x5505, 0x6605, 0x7705, 0x8805,
    0x1106, 0x2206, 0x3306, 0x4406, 0x5506, 0x6606, 0x7706, 0x8806,
    0x1107, 0x2207, 0x3307, 0x4407, 0x5507, 0x6607, 0x7707, 0x8807,
};

// ---- 1. test_start_beacon (5pt) --------------------------------------

static int test_start_beacon(struct test_result *r) {
    REQUIRE_IMPL(start_beacon, r);

    clobber_call((void (*)())start_beacon,
                 (uint64_t)(uintptr_t)bcn_img,
                 200, 80, 4, 30, 10);

    if (skyline_beacon.img    != bcn_img) { r->fail_reason = "skyline_beacon.img wrong";    return 0; }
    if (skyline_beacon.x      != 200)     { r->fail_reason = "skyline_beacon.x != 200";     return 0; }
    if (skyline_beacon.y      != 80)      { r->fail_reason = "skyline_beacon.y != 80";      return 0; }
    if (skyline_beacon.dia    != 4)       { r->fail_reason = "skyline_beacon.dia != 4";     return 0; }
    if (skyline_beacon.period != 30)      { r->fail_reason = "skyline_beacon.period != 30"; return 0; }
    if (skyline_beacon.ontime != 10)      { r->fail_reason = "skyline_beacon.ontime != 10"; return 0; }

    r->passed = 1;
    return 1;
}

// ---- helper for draw_beacon tests ------------------------------------
// Build a beacon struct, run ref + actual draw, compare.

static int run_draw_beacon(struct test_result *r,
                           uint16_t x, uint16_t y, uint8_t dia,
                           uint16_t period, uint16_t ontime,
                           uint64_t t,
                           const char *fail_msg) {
    fb_clear_both(0);
    struct skyline_beacon b = {
        .img    = bcn_img,
        .x      = x,
        .y      = y,
        .dia    = dia,
        .period = period,
        .ontime = ontime,
    };

    ref_draw_beacon(fb_expected, t, &b);
    clobber_call((void (*)())draw_beacon,
                 (uint64_t)(uintptr_t)fb_actual,
                 t,
                 (uint64_t)(uintptr_t)&b,
                 0, 0, 0);

    if (fb_diff_count() != 0) {
        r->fail_reason = fail_msg;
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 2. test_draw_beacon_simple (2pt) --------------------------------
// fully-on-screen, t inside ontime window.

static int test_draw_beacon_simple(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    // period=10, ontime=10 -> always drawn for any t
    return run_draw_beacon(r, 100, 100, 8, 10, 10, /*t=*/0,
                           "draw_beacon pixel mismatch (simple)");
}

// ---- 3. test_draw_beacon_null (1pt) ----------------------------------

static int test_draw_beacon_null(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    fb_clear_both(0);
    clobber_call((void (*)())draw_beacon,
                 (uint64_t)(uintptr_t)fb_actual,
                 0,
                 0,           // NULL beacon
                 0, 0, 0);
    if (fb_diff_count() != 0) {
        r->fail_reason = "draw_beacon(fb, t, NULL) modified fb";
        return 0;
    }
    r->passed = 1;
    return 1;
}

// ---- 4. test_draw_beacon_clipping_side (4pt) -------------------------

static int test_draw_beacon_clipping_side(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    // x=636, dia=8 -> x+dia=644 > 640: clip 4 cols on the right
    return run_draw_beacon(r, 636, 100, 8, 10, 10, 0,
                           "draw_beacon pixel mismatch (clipping side)");
}

// ---- 5. test_draw_beacon_clipping_bottom (4pt) -----------------------

static int test_draw_beacon_clipping_bottom(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    // y=476, dia=8 -> y+dia=484 > 480: clip 4 rows at the bottom
    return run_draw_beacon(r, 100, 476, 8, 10, 10, 0,
                           "draw_beacon pixel mismatch (clipping bottom)");
}

// ---- 6. test_draw_beacon_outside (4pt) -------------------------------

static int test_draw_beacon_outside(struct test_result *r) {
    REQUIRE_IMPL(draw_beacon, r);

    // Entirely off-screen -- fb must remain all zero
    return run_draw_beacon(r, 700, 500, 8, 10, 10, 0,
                           "draw_beacon outside-screen modified fb");
}

// ---- registry --------------------------------------------------------

static const struct test_entry beacon_tests[] = {
    { "test_start_beacon",              test_start_beacon,              5 },
    { "test_draw_beacon_simple",        test_draw_beacon_simple,        2 },
    { "test_draw_beacon_null",          test_draw_beacon_null,          1 },
    { "test_draw_beacon_clipping_side", test_draw_beacon_clipping_side, 4 },
    { "test_draw_beacon_clipping_bottom",test_draw_beacon_clipping_bottom,4 },
    { "test_draw_beacon_outside",       test_draw_beacon_outside,       4 },
};

const struct test_entry *get_beacon_tests(int *n_out) {
    *n_out = sizeof beacon_tests / sizeof beacon_tests[0];
    return beacon_tests;
}
