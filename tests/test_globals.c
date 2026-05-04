// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// test_globals.c -- skyline globals for the test bench
//
// In the original distribution these were defined inside demo.o (which we
// don't link for test.elf).  Same names + sizes so mp1.S resolves cleanly.

#include <stdint.h>
#include <stddef.h>
#include "skyline.h"
#include "string.h"
#include "test_framework.h"

struct skyline_star * skyline_star_list;
struct skyline_window skyline_windows[SKYLINE_WIN_MAX];
uint16_t skyline_win_cnt;
struct skyline_beacon skyline_beacon;

// Reset all four globals to a known state between tests.  Note this leaks
// any nodes currently chained off skyline_star_list -- that's intentional
// for the foundation phase; the per-test setup walks and frees the list
// when needed (see Phase F).

void test_globals_reset(void) {
    skyline_star_list = NULL;
    memset(skyline_windows, 0, sizeof skyline_windows);
    skyline_win_cnt = 0;
    memset(&skyline_beacon, 0, sizeof skyline_beacon);
}
