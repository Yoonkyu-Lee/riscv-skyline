# Drawing API

The eight functions in [`kernel/mp1.S`](../kernel/mp1.S). Inputs and
outputs use the standard RISC-V LP64 calling convention (`a0..a7` for
arguments, `a0` for return); each function preserves all callee-saved
registers (`ra` if it makes a call, `s0..s11` if it touches them).

## Stars

### `add_star(uint16_t x, uint16_t y, uint16_t color)` -> void

Allocates a fresh `struct skyline_star` (`malloc(16)`), populates it,
and pushes it at the head of `skyline_star_list`. `malloc` returning
NULL is treated as a silent no-op.

### `remove_star(uint16_t x, uint16_t y)` -> void

Walks the list and unlinks + `free()`s the first node whose
coordinates match. No-op if no node matches. The spec guarantees at
most one star per `(x, y)`.

### `draw_star(uint16_t * fbuf, const struct skyline_star * star)` -> void

Writes one RGB565 pixel: `fbuf[star->y * 640 + star->x] = star->color`.
Returns immediately if `star == NULL` or if the pixel is off-screen.
Leaf function — no prologue / epilogue.

## Windows

### `add_window(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t color)` -> void

Appends to `skyline_windows[]` and bumps `skyline_win_cnt`. Silently
drops the request if the array is full (`>= SKYLINE_WIN_MAX`).

### `remove_window(uint16_t x, uint16_t y)` -> void

Finds the window whose upper-left corner matches `(x, y)`, shifts all
later entries down by one (the array is kept contiguous), and
decrements `skyline_win_cnt`.

### `draw_window(uint16_t * fbuf, const struct skyline_window * win)` -> void

Fills the `win->w × win->h` rectangle with `win->color`. Each pixel is
clipped individually so the window can hang off any edge of the
screen. Returns immediately on `NULL`.

## Beacon

### `start_beacon(const uint16_t * img, uint16_t x, uint16_t y, uint8_t dia, uint16_t period, uint16_t ontime)` -> void

Populates the single `skyline_beacon` global. Does not allocate.

### `draw_beacon(uint16_t * fbuf, uint64_t t, const struct skyline_beacon * bcn)` -> void

If `(t % bcn->period) < bcn->ontime`, blits the `bcn->dia × bcn->dia`
sprite at `bcn->img` to `fbuf` at position `(bcn->x, bcn->y)`. Otherwise
it's the "off" half of the blink cycle and the function returns
without writing. Per-pixel clip identical to `draw_window`.

## Globals consumed

The drawing primitives read and write these globals (declared in
`skyline.h`, defined in `kernel/demo.c` for the production binary and
in `tests/test_globals.c` for the test bench):

```c
struct skyline_star *      skyline_star_list;
struct skyline_window      skyline_windows[SKYLINE_WIN_MAX]; // 4000 slots
uint16_t                   skyline_win_cnt;
struct skyline_beacon      skyline_beacon;
```

## Calling-convention notes

- Every function preserves `s0..s11` and `ra`.
- `add_star` saves `s0..s2` because it has to stash `(x, y, color)`
  across the `malloc(16)` call (`malloc` clobbers caller-saved).
- `remove_star` saves only `ra` because it makes a `free()` call.
- The other five functions are leaf functions and don't touch the
  callee-saved set at all.
- The test bench wraps every primitive in a clobber-detector shim
  that fills `s0..s11` with sentinel values before the call and
  inspects them after; mismatches are reported as a register-clobber
  penalty.
