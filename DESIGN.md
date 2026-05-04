# DESIGN — riscv-skyline

How the kernel comes up, how it gets pixels onto the QEMU virt
machine's bochs-display PCI VGA, what the eight assembly drawing
primitives do, and how the test bench exercises them. Reference
material (full layouts, primitive contracts, color encoding) lives
under [`docs/`](docs/).

## Boot path

QEMU `-machine virt -bios none` loads `demo.elf` and jumps to its
entry symbol at `0x80000000`. From there:

1. **`start.s`** sets the stack pointer, installs the trap vector
   (points at `_trap_entry` in `trap.s`), and tail-jumps into
   `main`.
2. **`main()`** (in `kernel/vendor/demo.o`, the original starter
   driver kept as a checked-in vendor binary) wires up the boot
   plumbing — console, interrupt enable, PCI scan for the bochs card,
   `vga_attach`, `skyline_init` — then seeds the scene (stars, lit
   windows, the lighthouse beacon position) and enters its frame
   loop.
3. The frame loop calls into the eight assembly drawing primitives
   in `mp1.S` every iteration: `draw_star` for each linked-list
   star, `draw_window` for each entry in `skyline_windows[]`, and
   `draw_beacon` for the single beacon.

## PCI / bochs framebuffer wire-up

The QEMU virt machine exposes its PCIe ECAM at `0x30000000` and a
32-bit MMIO window at `0x40000000..0x80000000`. The bochs card has to
have its VRAM BAR mapped *inside* that MMIO window — putting it in
DRAM produces the silent-drop "Unexpected Bochs VBE ID" symptom that
took two debug iterations to track down.

```
[0x30000000] PCIe ECAM    -> per-device config space
[0x40000000] PCI MMIO     -> bochs-display BAR0 (VRAM, 16 MB aligned at 0x41000000)
                              bochs-display BAR2 (VBE control regs, immediately after BAR0)
[0x80000000] DRAM         -> kernel image, stack, BSS arena
```

`vga.c` runs the standard "write -1, read back to learn size" PCI BAR
probe, places BAR0 at `FBUF_PMA = 0x41000000` and BAR2 at `BAR0 +
fbsize` *bytes* (the previous version of this code did
`fbuf + fbsize` on a `uint16_t *`, advancing by 2× and missing the
control window). It then programs the bochs VBE indexes — XRES, YRES,
BPP, ENABLE — and the framebuffer is live.

## Scene composition + frame loop

The starter driver (`kernel/vendor/demo.o`) handles both. It seeds a
dense field of stars across the whole screen, lays out clusters of
small lit windows along the bottom (which form the city silhouette
implicitly against the black sky), and pins a beacon above the
tallest cluster. Every frame it clears the framebuffer to black and
re-issues `draw_star` / `draw_window` / `draw_beacon` calls into
`mp1.S`.

The clean-room replacement of this driver was prototyped and is
trivial against the `skyline.h` API; the vendor binary was kept
because the original scene reads more like a real city skyline than
the rewrite did. See [`NOTICE`](NOTICE) for the attribution
boundary.

## Drawing primitives (mp1.S)

Eight functions, all RISC-V LP64 calling convention:

- `add_star(x, y, color)` — `malloc(16)`, fill, push at head of
  `skyline_star_list`. malloc-failure → silent no-op.
- `remove_star(x, y)` — walk the list, unlink first match, `free()`.
- `draw_star(fbuf, *star)` — null-guard, screen-clip, write one
  RGB565 pixel.
- `add_window(x, y, w, h, color)` — bounds-check `skyline_win_cnt`,
  write a `struct skyline_window` into the array, increment counter.
- `remove_window(x, y)` — find by upper-left match, shift later
  entries down (windows are kept contiguous), decrement counter.
- `draw_window(fbuf, *win)` — fill an `w × h` solid rect with
  per-pixel screen clipping (the window can hang off any edge).
- `start_beacon(img, x, y, dia, period, ontime)` — populate the
  single `skyline_beacon` global.
- `draw_beacon(fbuf, t, *bcn)` — if `(t % period) < ontime`, blit
  the `dia × dia` sprite from `bcn->img` at `(bcn->x, bcn->y)`,
  again with per-pixel screen clipping.

Calling-convention details (which registers each function preserves)
are in [`docs/drawing-api.md`](docs/drawing-api.md).

## Allocator (memory.c)

Backing store is a 256 KB BSS arena, 16-byte aligned. `malloc`
rounds the request up to 16 bytes and pulls from a singly-linked
free list of 16-byte blocks if it has any; otherwise it bumps the
arena pointer. `free` always recycles into that one bucket — this is
fine because mp1.S only ever asks for 16 bytes (one
`struct skyline_star`); larger allocations from the test harness are
bump-only and are leaked on purpose. `memory_init()` resets the
arena between test cases so leaks from one case don't poison the
next.

## Test bench

`make run-test` rebuilds `test.elf` with `tests/test_main.o` swapping
in for the demo's main, and `tests/halt_replace.o` swapping in for
the panic shim so a fault returns control to the runner instead.

The runner calls each test function inside a `setjmp` / `longjmp`
trap-recovery shim (`tests/setjmp.S` + the trap path in `trap.s`),
so a faulting test gets a `FAILED` row with `cause`/`sepc`/`stval`
captured instead of crashing the kernel. A separate wrapper
(`tests/clobber.S`) detects callee-saved register clobbers — any
function that doesn't restore `s0..s11` reliably gets flagged.

The framebuffer module that the tests draw into is a software-only
mock (`tests/fb.{c,h}`); it implements the same screen-clip rules as
a real bochs display so tests can validate `draw_*` behaviour without
needing a real display device.

## Component index

| File                | Responsibility                                      |
| ------------------- | --------------------------------------------------- |
| `kernel/start.s`    | M-mode entry, stack + trap vector setup, `j main`   |
| `kernel/trap.s`     | trap save/restore, dispatch                         |
| `kernel/vendor/demo.o`      | starter binary: bring-up, PCI scan, scene seed, frame loop (see NOTICE) |
| `kernel/vga.c`      | bochs PCI VGA driver (BAR programming + VBE)        |
| `kernel/mp1.S`      | 8 drawing primitives                                |
| `kernel/memory.c`   | malloc/free + memory_init                           |
| `kernel/console.c`  | UART0 console (kprintf path)                        |
| `kernel/serial.c`   | low-level UART register I/O                         |
| `kernel/intr.c`     | S-mode interrupt enable + handler dispatch glue     |
| `tests/`            | suite registry, runner, fault-recovery shim, fb mock |
