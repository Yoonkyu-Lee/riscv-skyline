// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// fb.h -- framebuffer scaffolding for the test bench
//
// The test bench owns two 640x480 16bpp framebuffers in BSS:
//
//   fb_actual    -- passed to mp1.S's draw_* functions; the runner
//                   compares this to fb_expected to score correctness.
//   fb_expected  -- populated by the C reference drawers below; this
//                   is the authoritative answer for any given test
//                   setup.
//
// Both arrays are 614400 bytes, total 1.2 MB.  They live in BSS, well
// within the unbounded BSS segment in kernel.ld.

#ifndef _FB_H_
#define _FB_H_

#include <stdint.h>
#include "skyline.h"

#define FB_PIXELS  ((size_t)SKYLINE_WIDTH * (size_t)SKYLINE_HEIGHT)

extern uint16_t fb_actual[FB_PIXELS];
extern uint16_t fb_expected[FB_PIXELS];

// Clear helpers --------------------------------------------------------

void fb_clear(uint16_t *fb, uint16_t bg);
void fb_clear_both(uint16_t bg);

// Diff helpers ---------------------------------------------------------
//
// fb_diff_count returns the number of pixels where fb_actual and
// fb_expected disagree.  fb_print_diff_summary emits up to /max_lines/
// of the form "  (x,y) got=0xRRRR want=0xRRRR" so failing tests give
// an immediate hint at where mp1.S went wrong.

int  fb_diff_count(void);
void fb_print_diff_summary(int max_lines);

// In-screen predicate -- handy for tests that need to express
// "this coordinate is on-screen" cleanly.
int  fb_in_screen(int x, int y);

// Reference drawers ----------------------------------------------------
//
// Plain-C implementations of draw_star / draw_window / draw_beacon
// that exactly follow the drawing primitive contract (row-major 640x480, per-pixel clip
// to [0,640)x[0,480), beacon time gate `t % period < ontime`, NULL
// pointer no-op).  Tests use these to populate fb_expected before
// invoking mp1.S to populate fb_actual.

void ref_draw_star  (uint16_t *fb, const struct skyline_star  *s);
void ref_draw_window(uint16_t *fb, const struct skyline_window *w);
void ref_draw_beacon(uint16_t *fb, uint64_t t,
                     const struct skyline_beacon *b);

#endif // _FB_H_
