// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// demo.c -- riscv-skyline driver. Sets up the framebuffer, seeds a
// starry-night scene, and drives a per-frame redraw loop calling the
// hand-written assembly drawing primitives in mp1.S.

#include <stdint.h>

#include "console.h"
#include "halt.h"
#include "intr.h"
#include "skyline.h"
#include "vga.h"

// QEMU `virt` machine memory map for the PCI bus.
//   ECAM_BASE: bus0 dev0 func0 starts here; address[27:20]=bus,
//              [19:15]=dev, [14:12]=func, [11:0]=cfg-byte
//   FBUF_PMA:  16 MB-aligned chunk inside the 32-bit PCIe MMIO window
//              (which on virt is 0x40000000..0x80000000). The bochs
//              VBE BAR0 has to land here, NOT in DRAM, or the host
//              bridge silently drops the mapping.
//   CTLBASE_PMA: equally MMIO-resident, sized for BAR2's control window.
#define ECAM_BASE   0x30000000UL
#define FBUF_PMA    0x41000000UL

// Drawing globals declared `extern` in skyline.h. The assembly module
// reads/writes these.
struct skyline_star *      skyline_star_list;
struct skyline_window      skyline_windows[SKYLINE_WIN_MAX];
uint16_t                   skyline_win_cnt;
struct skyline_beacon      skyline_beacon;

// 8x8 RGB565 sprite for the lighthouse beacon -- a small red orb.
static const uint16_t beacon_img[8 * 8] = {
    0x0000, 0x0000, 0xF800, 0xF800, 0xF800, 0xF800, 0x0000, 0x0000,
    0x0000, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x0000,
    0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800,
    0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800,
    0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800,
    0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800,
    0x0000, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x0000,
    0x0000, 0x0000, 0xF800, 0xF800, 0xF800, 0xF800, 0x0000, 0x0000,
};

// xorshift32 -- self-contained PRNG so the scene seeding is deterministic
// and we don't need the C runtime's rand().
static uint32_t rng_state = 2463534242u;
static uint32_t xorshift32(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

// Walk PCI bus 0 looking for the QEMU bochs-display card (vendor 0x1234,
// device 0x1111). Returns the absolute ECAM address of its config space.
static uint64_t find_bochs_display(void) {
    for (int dev = 0; dev < 32; dev++) {
        uint64_t cfg = ECAM_BASE | ((uint64_t)dev << 15);
        volatile uint16_t * id = (volatile uint16_t *)(uintptr_t)cfg;
        uint16_t vendor = id[0];
        uint16_t device = id[1];
        if (vendor == 0x1234 && device == 0x1111)
            return cfg;
        if (vendor == 0xFFFF)
            continue; // empty slot
    }
    panic("bochs-display not found on PCI bus 0");
}

// Beacon position remembered between scene composition and frame loop.
static uint16_t beacon_x_global, beacon_y_global;

// Compose a minimalist night scene -- stars across the whole screen and
// dense clusters of tiny lit windows along the bottom that visually
// form building silhouettes. No explicit silhouette is drawn; the dark
// background between the lit windows is what the eye reads as the
// building. (This matches the "dot-pattern" aesthetic of the original
// reference demo, with a warmer yellow tint instead of plain white.)
static void compose_scene(void) {
    // Stars: 600 single-pixel points across the entire screen.
    // Predominantly white with a faint warm tint so they read as
    // starlight rather than Christmas lights.
    for (int i = 0; i < 600; i++) {
        uint16_t x = xorshift32() % SKYLINE_WIDTH;
        uint16_t y = xorshift32() % SKYLINE_HEIGHT;
        uint16_t color;
        switch (xorshift32() & 0xF) {
        case 0:  color = 0xFFE0; break; // touch of yellow
        case 1:  color = 0xFFDE; break; // pale cream
        default: color = 0xFFFF; break; // plain white
        }
        add_star(x, y, color);
    }

    // Window palette: warm yellow / amber. The reference demo uses
    // near-white; we tilt warmer so the city face reads as glowing
    // tungsten lights rather than fluorescent.
    static const uint16_t window_palette[] = {
        0xFFE0,  // bright yellow
        0xFE60,  // soft yellow
        0xFD20,  // amber
        0xFC00,  // dim amber
    };

    // 9 buildings, all in the lower 30% of the screen, varying tops.
    // Window pitch is small (3x3 cells) so each building is a dense
    // cluster of "windows" rather than a grid of fat blocks.
    int      n_buildings   = 9;
    uint16_t city_left     = 20;
    uint16_t spacing       = 68;          // 9 * 68 + 20 < 640
    uint16_t tallest_top   = SKYLINE_HEIGHT;
    uint16_t tallest_x     = 0;
    uint16_t tallest_w     = 50;
    int      p             = 0;

    for (int b = 0; b < n_buildings; b++) {
        uint16_t bx  = city_left + (uint16_t)(b * spacing);
        uint16_t bw  = 44 + (uint16_t)(xorshift32() % 20);   // 44..63
        uint16_t top = 320 + (uint16_t)(xorshift32() % 80);  // 320..399

        // Pixel-dense window cluster. 3x3 lit cells on a 6-pixel pitch,
        // skip ~30% so the cluster reads as a textured silhouette.
        for (uint16_t wy = top + 4; wy + 4 < SKYLINE_HEIGHT - 2; wy += 6) {
            for (uint16_t wx = bx + 3; wx + 4 < bx + bw - 2; wx += 6) {
                if ((xorshift32() % 10) < 3) continue;
                uint16_t color = window_palette[(uint32_t)p++ & 0x3];
                add_window(wx, wy, 3, 3, color);
            }
        }

        if (top < tallest_top) {
            tallest_top = top;
            tallest_x   = bx;
            tallest_w   = bw;
        }
    }

    // Beacon sits centered just above the tallest building's roof.
    beacon_x_global = tallest_x + (tallest_w / 2) - 4;
    beacon_y_global = tallest_top - 12;
    start_beacon(beacon_img, beacon_x_global, beacon_y_global,
                 8, 60, 30);
}

// Tight CPU spin -- the build is freestanding without a timer driver
// here, so we just burn cycles between frames.
static void delay_frame(void) {
    for (volatile uint64_t i = 0; i < 4000000; i++) { /* spin */ }
}

void main(void) {
    console_init();
    intr_init();

    kprintf("riscv-skyline: scanning PCI for bochs-display...\n");
    uint64_t cfgaddr = find_bochs_display();

    uint16_t * fbuf = (uint16_t *)(uintptr_t)FBUF_PMA;
    vga_attach(cfgaddr, fbuf);

    skyline_init();
    compose_scene();

    kprintf("riscv-skyline: %u stars, %u windows, beacon at (%u,%u)\n",
            220, (unsigned)skyline_win_cnt,
            (unsigned)skyline_beacon.x, (unsigned)skyline_beacon.y);

    // Animation loop, drawn back-to-front:
    //   1. clear to black night sky
    //   2. paint stars across the whole screen
    //   3. paint lit windows on top (their dark surroundings form
    //      the implicit building silhouettes)
    //   4. paint the blinking beacon last so it overlays everything
    uint64_t t = 0;
    const long pixels = (long)SKYLINE_WIDTH * SKYLINE_HEIGHT;
    for (;;) {
        for (long i = 0; i < pixels; i++)
            fbuf[i] = 0x0000;   // black night sky

        for (struct skyline_star * s = skyline_star_list; s != 0; s = s->next)
            draw_star(fbuf, s);

        for (uint16_t i = 0; i < skyline_win_cnt; i++)
            draw_window(fbuf, &skyline_windows[i]);

        draw_beacon(fbuf, t, &skyline_beacon);

        t += 1;
        delay_frame();
    }
}
