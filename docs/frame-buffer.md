# Frame buffer model

## Geometry

- 640 × 480 pixels
- 16 bits per pixel, RGB565 packing
- Row-major; pixel `(x, y)` is at `fbuf[y * 640 + x]`
- VRAM is `640 × 480 × 2 = 614 400` bytes (~600 KB)

The full bochs BAR0 region is 16 MB (the device's standard size),
but only the first ~600 KB are actually scanned out for the configured
640×480×16 mode.

## RGB565 color encoding

```
bit  15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
      R  R  R  R  R  G  G  G  G  G  G  B  B  B  B  B
```

| Channel | Bits | Range | Notes                                  |
| ------- | ---- | ----- | -------------------------------------- |
| Red     | 5    | 0..31 | upper 5 bits of an 8-bit channel       |
| Green   | 6    | 0..63 | upper 6 bits — extra precision because the eye is greenest-sensitive |
| Blue    | 5    | 0..31 | upper 5 bits                           |

A few colors used by the demo:

| Hex      | Color                | What it draws            |
| -------- | -------------------- | ------------------------ |
| `0x0001` | near-black (1B blue) | night sky background     |
| `0x0841` | very dark gray       | building silhouette      |
| `0xFFFF` | white                | most stars               |
| `0xFFE0` | bright yellow        | brightest lit window     |
| `0xFD20` | amber                | warmer lit window        |
| `0xF800` | red                  | beacon sprite            |

## Pixel addressing

```c
uint16_t * fbuf = ...; // from vga_attach
fbuf[(y * 640) + x] = color;
```

In assembly (from `mp1.S::draw_star`):

```
li      t2, 640
mul     t1, t1, t2          # t1 = y * 640
add     t1, t1, t0          # t1 = x + y * 640 (pixel index)
slli    t1, t1, 1           # times 2 bytes per pixel
add     t1, a0, t1          # &fbuf[index]
sh      color, 0(t1)
```

## Clipping rules

Every drawing primitive enforces these rules:

- A pixel is drawn only if `0 <= x < 640` and `0 <= y < 480`.
- Coordinates are unsigned 16-bit, so "negative" is impossible at the
  primitive boundary, but the test bench passes very large values
  (e.g. `0xFFFF`) to test the upper-edge guard.
- For `draw_window`: each candidate pixel inside the `w × h` rectangle
  is checked individually. Windows can hang off any edge of the
  screen; only the in-bounds portion gets written.
- For `draw_beacon`: same per-pixel check, against the `dia × dia`
  sprite.
