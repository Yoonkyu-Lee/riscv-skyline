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

// Composite scene: stars across the night sky, a row of lit building
// windows along the city silhouette, and a single blinking beacon
// perched on top of the tallest building.
static void compose_scene(void) {
    // Stars: 220 random pinpricks across the upper 60% of the screen.
    // White and faint warm tints only -- avoids the "Christmas lights"
    // look that bright primaries would give an actual night sky.
    for (int i = 0; i < 220; i++) {
        uint16_t x = xorshift32() % SKYLINE_WIDTH;
        uint16_t y = xorshift32() % (SKYLINE_HEIGHT * 3 / 5);
        uint16_t color;
        switch (xorshift32() & 0x7) {
        case 0:  color = 0xFFE0; break; // tiny touch of yellow
        case 1:  color = 0xFFDE; break; // pale cream
        default: color = 0xFFFF; break; // plain white
        }
        add_star(x, y, color);
    }

    // Building windows: lit warm-yellow squares across the lower city.
    // Real lit windows are warm; mixing in greens / cyans looked
    // alien, so this palette only spans yellow -> amber -> dim amber.
    static const uint16_t window_palette[] = {
        0xFFE0,  // bright yellow
        0xFE60,  // soft yellow
        0xFD20,  // amber
        0xFC00,  // dim amber
    };

    // Generate a deterministic city: 7 buildings, each given a base x,
    // a width, and a top-y. The tallest of them gets the beacon perched
    // on its roof.
    int      n_buildings   = 7;
    uint16_t city_left     = 30;
    uint16_t spacing       = 88;          // 7 * 88 = 616 < 640
    uint16_t tallest_top   = SKYLINE_HEIGHT;  // smaller y = taller
    uint16_t tallest_x     = 0;
    uint16_t tallest_w     = 60;
    int      p             = 0;

    for (int b = 0; b < n_buildings; b++) {
        uint16_t bx  = city_left + (uint16_t)(b * spacing);
        uint16_t bw  = 56 + (uint16_t)(xorshift32() % 24);   // 56..79
        uint16_t top = 250 + (uint16_t)(xorshift32() % 130); // 250..379

        for (uint16_t wy = top + 10; wy + 14 < SKYLINE_HEIGHT - 8; wy += 22) {
            for (uint16_t wx = bx + 8; wx + 12 < bx + bw - 6; wx += 16) {
                // Skip ~1/4 windows to give a "some lights off" texture.
                if ((xorshift32() & 0x3) == 0) continue;
                uint16_t color = window_palette[(uint32_t)p++ & 0x3];
                add_window(wx, wy, 8, 12, color);
            }
        }

        if (top < tallest_top) {
            tallest_top = top;
            tallest_x   = bx;
            tallest_w   = bw;
        }
    }

    // Beacon sits centered on top of the tallest building, slightly
    // overhanging its roof line. period = 60 frames, on for 30 frames
    // (~50% duty so the blink is unmissable).
    uint16_t beacon_x = tallest_x + (tallest_w / 2) - 4;
    uint16_t beacon_y = tallest_top - 12;   // 12 px above the roof
    start_beacon(beacon_img, beacon_x, beacon_y, 8, 60, 30);
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

    // Animation loop. Each frame: clear, redraw all scene elements,
    // bump the wall clock for the beacon's blink phase.
    uint64_t t = 0;
    const long pixels = (long)SKYLINE_WIDTH * SKYLINE_HEIGHT;
    for (;;) {
        // Clear back buffer to deep-night blue (RGB565: low blue + tiny green).
        for (long i = 0; i < pixels; i++)
            fbuf[i] = 0x0001;

        for (struct skyline_star * s = skyline_star_list; s != 0; s = s->next)
            draw_star(fbuf, s);

        for (uint16_t i = 0; i < skyline_win_cnt; i++)
            draw_window(fbuf, &skyline_windows[i]);

        draw_beacon(fbuf, t, &skyline_beacon);

        t += 1;
        delay_frame();
    }
}
