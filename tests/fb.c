// fb.c -- framebuffer module for the MP1 test bench
//
// Provides the two 640x480 framebuffers (actual + expected), simple
// fill / diff helpers, and C reference implementations of the three
// drawer functions that mp1.S must produce equivalent output to.

#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "string.h"
#include "skyline.h"
#include "fb.h"

uint16_t fb_actual  [FB_PIXELS];
uint16_t fb_expected[FB_PIXELS];

// ---- clear ---------------------------------------------------------------

void fb_clear(uint16_t *fb, uint16_t bg) {
    if (bg == 0) {
        memset(fb, 0, FB_PIXELS * sizeof(uint16_t));
        return;
    }
    for (size_t i = 0; i < FB_PIXELS; i++)
        fb[i] = bg;
}

void fb_clear_both(uint16_t bg) {
    fb_clear(fb_actual,   bg);
    fb_clear(fb_expected, bg);
}

// ---- diff ----------------------------------------------------------------

int fb_diff_count(void) {
    int diff = 0;
    for (size_t i = 0; i < FB_PIXELS; i++) {
        if (fb_actual[i] != fb_expected[i])
            diff++;
    }
    return diff;
}

void fb_print_diff_summary(int max_lines) {
    int printed = 0;
    int total   = 0;
    for (size_t i = 0; i < FB_PIXELS; i++) {
        if (fb_actual[i] != fb_expected[i]) {
            total++;
            if (printed < max_lines) {
                int x = (int)(i % SKYLINE_WIDTH);
                int y = (int)(i / SKYLINE_WIDTH);
                kprintf("  (%d,%d) got=0x%x want=0x%x\n",
                        x, y,
                        (unsigned)fb_actual[i],
                        (unsigned)fb_expected[i]);
                printed++;
            }
        }
    }
    if (total > printed)
        kprintf("  ... and %d more\n", total - printed);
}

int fb_in_screen(int x, int y) {
    return (x >= 0 && x < SKYLINE_WIDTH && y >= 0 && y < SKYLINE_HEIGHT);
}

// ---- reference drawers ---------------------------------------------------
//
// These implement the exact behavior described in the MP1 spec.  They
// serve as the oracle: tests render once with the reference into
// fb_expected, then call into mp1.S to render into fb_actual, then
// compare with fb_diff_count.

void ref_draw_star(uint16_t *fb, const struct skyline_star *s) {
    if (s == NULL) return;
    int x = s->x;
    int y = s->y;
    if (!fb_in_screen(x, y)) return;
    fb[x + y * SKYLINE_WIDTH] = s->color;
}

void ref_draw_window(uint16_t *fb, const struct skyline_window *w) {
    if (w == NULL) return;
    int x0 = w->x;
    int y0 = w->y;
    int wd = w->w;     // uint8_t -> int (zero-extended)
    int ht = w->h;
    for (int dy = 0; dy < ht; dy++) {
        int py = y0 + dy;
        if (py < 0 || py >= SKYLINE_HEIGHT) continue;
        for (int dx = 0; dx < wd; dx++) {
            int px = x0 + dx;
            if (px < 0 || px >= SKYLINE_WIDTH) continue;
            fb[px + py * SKYLINE_WIDTH] = w->color;
        }
    }
}

void ref_draw_beacon(uint16_t *fb, uint64_t t,
                     const struct skyline_beacon *b) {
    if (b == NULL) return;
    if (b->period == 0) return;          // defensive: avoid div by 0
    if (b->img == NULL) return;
    if ((t % b->period) >= b->ontime) return;

    int x0  = b->x;
    int y0  = b->y;
    int dia = b->dia;                    // uint8_t

    for (int dy = 0; dy < dia; dy++) {
        int py = y0 + dy;
        if (py < 0 || py >= SKYLINE_HEIGHT) continue;
        for (int dx = 0; dx < dia; dx++) {
            int px = x0 + dx;
            if (px < 0 || px >= SKYLINE_WIDTH) continue;
            fb[px + py * SKYLINE_WIDTH] = b->img[dx + dy * dia];
        }
    }
}
