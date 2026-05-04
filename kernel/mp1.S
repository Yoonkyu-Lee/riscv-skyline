# Copyright (c) 2024-2026 Yoonkyu Lee
# SPDX-License-Identifier: MIT
#
        # mp1.S -- riscv-skyline drawing primitives in RISC-V assembly.
        #
        # The 4 skyline globals are defined in demo.c (or test_globals.c
        # for the test bench). We reference them by name here.

        .extern skyline_star_list       # struct skyline_star *
        .extern skyline_windows         # struct skyline_window[SKYLINE_WIN_MAX]
        .extern skyline_win_cnt         # uint16_t
        .extern skyline_beacon          # struct skyline_beacon

        # ---- struct field offsets (must match skyline.h) ----------------
        #
        # struct skyline_star { struct skyline_star *next; uint16_t x,y,color; }
        # next at 0 (8 bytes), x at 8, y at 10, color at 12.  sizeof = 16
        # (8-byte aligned; 2 bytes of trailing pad).

        #define STAR_NEXT     0
        #define STAR_X        8
        #define STAR_Y       10
        #define STAR_COLOR   12
        #define STAR_SIZE    16

        # struct skyline_window { uint16_t x,y; uint8_t w,h; uint16_t color; }
        # x:0, y:2, w:4, h:5, color:6.  sizeof = 8 (naturally packed).

        #define WIN_X         0
        #define WIN_Y         2
        #define WIN_W         4
        #define WIN_H         5
        #define WIN_COLOR     6
        #define WIN_SIZE      8

        # struct skyline_beacon { const uint16_t *img; uint16_t x,y;
        #                         uint8_t dia; uint16_t period, ontime; }
        # img:0 (8B), x:8, y:10, dia:12, [pad 13], period:14, ontime:16.

        #define BCN_IMG       0
        #define BCN_X         8
        #define BCN_Y        10
        #define BCN_DIA      12
        #define BCN_PERIOD   14
        #define BCN_ONTIME   16

        # ---- screen / array constants -----------------------------------

        #define SCREEN_W    640
        #define SCREEN_H    480
        #define WIN_MAX    4000

        .text
        .global skyline_init
        .type   skyline_init, @function
skyline_init:
        ret

        # ===============================================================
        # add_star -- insert a new star at the head of skyline_star_list.
        #
        # Inputs:
        #   a0 = uint16_t x
        #   a1 = uint16_t y
        #   a2 = uint16_t color
        # Output: none.
        # Side effects:
        #   - calls malloc(16); on NULL return, no-op.
        #   - mutates global skyline_star_list to point at the new node.
        # Clobbers (callee-saved): saves and restores ra, s0-s2.
        # ===============================================================
        .global add_star
        .type   add_star, @function
add_star:
        addi    sp, sp, -32
        sd      ra, 24(sp)
        sd      s0, 16(sp)
        sd      s1,  8(sp)
        sd      s2,  0(sp)

        # Stash args in callee-saved (malloc clobbers caller-saved).
        mv      s0, a0          # x
        mv      s1, a1          # y
        mv      s2, a2          # color

        li      a0, STAR_SIZE
        call    malloc
        beqz    a0, add_star_done   # malloc failed -- return silently

        # Populate the new node.
        sh      s0, STAR_X(a0)
        sh      s1, STAR_Y(a0)
        sh      s2, STAR_COLOR(a0)

        # Link at the head: new->next = head;  head = new.
        la      t0, skyline_star_list
        ld      t1, 0(t0)
        sd      t1, STAR_NEXT(a0)
        sd      a0, 0(t0)

add_star_done:
        ld      ra, 24(sp)
        ld      s0, 16(sp)
        ld      s1,  8(sp)
        ld      s2,  0(sp)
        addi    sp, sp, 32
        ret

        # ===============================================================
        # remove_star -- remove a node with matching (x,y) from
        #                skyline_star_list and free() it.  No-op if not
        #                found.  Spec says at most one node per (x,y).
        #
        # Inputs:  a0 = x, a1 = y.
        # Output:  none.
        # Side effects: calls free() on the matching node.
        # Clobbers (callee-saved): saves and restores ra.
        # ===============================================================
        .global remove_star
        .type   remove_star, @function
remove_star:
        addi    sp, sp, -16
        sd      ra,  8(sp)

        la      t0, skyline_star_list
        ld      t1, 0(t0)               # cur = head
        li      t2, 0                   # prev = NULL

remove_star_loop:
        beqz    t1, remove_star_done    # walked off the end -- not found

        lhu     t3, STAR_X(t1)
        bne     t3, a0, remove_star_advance
        lhu     t3, STAR_Y(t1)
        bne     t3, a1, remove_star_advance

        # Match.  Unlink cur.  next_of_cur = cur->next.
        ld      t3, STAR_NEXT(t1)
        beqz    t2, remove_star_unlink_head
        sd      t3, STAR_NEXT(t2)       # prev->next = cur->next
        j       remove_star_free
remove_star_unlink_head:
        sd      t3, 0(t0)               # head    = cur->next
remove_star_free:
        mv      a0, t1                  # free(cur)
        call    free
        j       remove_star_done

remove_star_advance:
        mv      t2, t1                  # prev = cur
        ld      t1, STAR_NEXT(t1)       # cur  = cur->next
        j       remove_star_loop

remove_star_done:
        ld      ra,  8(sp)
        addi    sp, sp, 16
        ret

        # ===============================================================
        # draw_star -- write one pixel into fbuf at (star->x, star->y).
        #
        # Inputs:  a0 = uint16_t *fbuf
        #          a1 = const struct skyline_star *star
        # Output:  none.
        # Behavior:
        #   - star == NULL  -> no-op
        #   - off-screen    -> no-op (clip)
        #   - in-screen     -> fbuf[x + y*640] = star->color
        # Leaf function -- no prologue/epilogue needed.
        # ===============================================================
        .global draw_star
        .type   draw_star, @function
draw_star:
        beqz    a1, draw_star_done          # NULL star -> skip

        lhu     t0, STAR_X(a1)              # x
        li      t1, SCREEN_W
        bgeu    t0, t1, draw_star_done      # x >= 640 -> clip

        lhu     t1, STAR_Y(a1)              # y
        li      t2, SCREEN_H
        bgeu    t1, t2, draw_star_done      # y >= 480 -> clip

        # offset_bytes = (x + y*SCREEN_W) * 2
        li      t2, SCREEN_W
        mul     t1, t1, t2                  # t1 = y * 640
        add     t1, t1, t0                  # t1 = x + y*640 (pixel index)
        slli    t1, t1, 1                   # bytes (each pixel is 2B)
        add     t1, a0, t1                  # &fbuf[offset]

        lhu     t0, STAR_COLOR(a1)
        sh      t0, 0(t1)

draw_star_done:
        ret

        # ===============================================================
        # add_window -- append a window to skyline_windows[].
        #
        # Inputs:  a0 = x, a1 = y, a2 = w, a3 = h, a4 = color
        # Output:  none.
        # Behavior:
        #   - if skyline_win_cnt >= WIN_MAX (4000), silently ignore.
        #   - otherwise write fields into windows[cnt], cnt++.
        # Leaf function.
        # ===============================================================
        .global add_window
        .type   add_window, @function
add_window:
        la      t0, skyline_win_cnt
        lhu     t1, 0(t0)                   # cnt
        li      t2, WIN_MAX
        bgeu    t1, t2, add_window_done     # array full -> ignore

        # &windows[cnt] = base + cnt * sizeof(window) (= cnt << 3)
        la      t2, skyline_windows
        slli    t3, t1, 3
        add     t2, t2, t3

        sh      a0, WIN_X(t2)
        sh      a1, WIN_Y(t2)
        sb      a2, WIN_W(t2)
        sb      a3, WIN_H(t2)
        sh      a4, WIN_COLOR(t2)

        addi    t1, t1, 1
        sh      t1, 0(t0)                   # win_cnt++

add_window_done:
        ret

        # ===============================================================
        # remove_window -- find a window with matching upper-left (x,y),
        #                  replace it with the last element, decrement
        #                  cnt.  No-op if not found.
        #
        # Inputs:  a0 = x, a1 = y.
        # Output:  none.
        # Leaf function.
        # ===============================================================
        .global remove_window
        .type   remove_window, @function
remove_window:
        la      t0, skyline_win_cnt
        lhu     t1, 0(t0)                   # cnt
        la      t2, skyline_windows         # base
        li      t3, 0                       # i = 0

remove_window_loop:
        bgeu    t3, t1, remove_window_done  # i >= cnt -> not found

        slli    t4, t3, 3
        add     t4, t2, t4                  # &windows[i]

        lhu     t5, WIN_X(t4)
        bne     t5, a0, remove_window_advance
        lhu     t5, WIN_Y(t4)
        bne     t5, a1, remove_window_advance

        # Match.  Swap-with-last: copy windows[cnt-1] over windows[i].
        # When i == cnt-1, this is a self-copy (no-op).  Then cnt--.
        addi    t1, t1, -1
        slli    t5, t1, 3
        add     t5, t2, t5                  # &windows[cnt-1]
        ld      t6, 0(t5)                   # 8B copy = whole struct
        sd      t6, 0(t4)
        sh      t1, 0(t0)                   # write back new cnt
        j       remove_window_done

remove_window_advance:
        addi    t3, t3, 1
        j       remove_window_loop

remove_window_done:
        ret

        # ===============================================================
        # draw_window -- fill a rectangle of color into fbuf with per-pixel
        #                screen clipping.  Window may extend off-screen
        #                in any direction (or be entirely off-screen).
        #
        # Inputs:  a0 = uint16_t *fbuf
        #          a1 = const struct skyline_window *win
        # Output:  none.
        # Behavior:
        #   - win == NULL -> no-op
        #   - for dy in [0,h): py = y+dy.  If py >= H break (rest also
        #     off-screen below).
        #   - for dx in [0,w): px = x+dx.  If px >= W break (rest also
        #     off-screen right).  Else fbuf[py*W + px] = color.
        # Leaf function.
        # ===============================================================
        .global draw_window
        .type   draw_window, @function
draw_window:
        beqz    a1, draw_window_done

        # Load all fields once -- they don't change during the draw.
        lhu     t0, WIN_X(a1)
        lhu     t1, WIN_Y(a1)
        lbu     t2, WIN_W(a1)
        lbu     t3, WIN_H(a1)
        lhu     t4, WIN_COLOR(a1)

        li      t5, 0                       # dy = 0

draw_window_outer:
        bgeu    t5, t3, draw_window_done    # dy >= h -> done
        add     t6, t1, t5                  # py = y + dy
        li      a2, SCREEN_H
        bgeu    t6, a2, draw_window_done    # py >= H -> rest are below screen

        # Hoist row offset out of inner loop:  row_off = py * W (pixels).
        li      a2, SCREEN_W
        mul     a4, t6, a2

        li      a3, 0                       # dx = 0

draw_window_inner:
        bgeu    a3, t2, draw_window_next    # dx >= w -> next row
        add     a5, t0, a3                  # px = x + dx
        bgeu    a5, a2, draw_window_next    # px >= W -> rest off-screen right

        add     a5, a4, a5                  # row_off + px (pixel index)
        slli    a5, a5, 1                   # *2 -> byte offset
        add     a5, a0, a5                  # &fbuf[...]
        sh      t4, 0(a5)

        addi    a3, a3, 1                   # dx++
        j       draw_window_inner

draw_window_next:
        addi    t5, t5, 1                   # dy++
        j       draw_window_outer

draw_window_done:
        ret

        # ===============================================================
        # start_beacon -- copy 6 args into the global skyline_beacon
        #                 struct.
        #
        # Inputs:  a0 = const uint16_t *img
        #          a1 = uint16_t x
        #          a2 = uint16_t y
        #          a3 = uint8_t  dia
        #          a4 = uint16_t period
        #          a5 = uint16_t ontime
        # Output:  none.
        # Leaf function.
        # ===============================================================
        .global start_beacon
        .type   start_beacon, @function
start_beacon:
        la      t0, skyline_beacon
        sd      a0, BCN_IMG(t0)
        sh      a1, BCN_X(t0)
        sh      a2, BCN_Y(t0)
        sb      a3, BCN_DIA(t0)
        sh      a4, BCN_PERIOD(t0)
        sh      a5, BCN_ONTIME(t0)
        ret

        # ===============================================================
        # draw_beacon -- conditionally render a dia x dia block from
        #                bcn->img into fbuf at (bcn->x, bcn->y).
        #
        # Inputs:  a0 = uint16_t *fbuf
        #          a1 = uint64_t t (current time, in ticks)
        #          a2 = const struct skyline_beacon *bcn
        # Output:  none.
        # Behavior:
        #   - bcn == NULL -> no-op
        #   - bcn->period == 0 -> defensive no-op (avoid divide-by-zero)
        #   - (t % period) >= ontime -> no-op (off phase; do NOT clear)
        #   - else: for each (dy,dx) in [0,dia)x[0,dia) on-screen,
        #           fbuf[(y+dy)*W + (x+dx)] = img[dy*dia + dx]
        # Leaf function.
        # ===============================================================
        .global draw_beacon
        .type   draw_beacon, @function
draw_beacon:
        beqz    a2, draw_beacon_done

        # period == 0 -> skip (defensive)
        lhu     t0, BCN_PERIOD(a2)
        beqz    t0, draw_beacon_done

        # (t % period) >= ontime -> off-phase, skip
        remu    t1, a1, t0
        lhu     t0, BCN_ONTIME(a2)
        bgeu    t1, t0, draw_beacon_done

        # Load the rest of the fields once.
        ld      t0, BCN_IMG(a2)             # img
        lhu     t1, BCN_X(a2)               # x
        lhu     t2, BCN_Y(a2)               # y
        lbu     t3, BCN_DIA(a2)             # dia

        li      t6, SCREEN_W                # constant for both axes
        li      t4, 0                       # dy = 0

draw_beacon_outer:
        bgeu    t4, t3, draw_beacon_done    # dy >= dia -> done
        add     t5, t2, t4                  # py = y + dy
        li      a1, SCREEN_H
        bgeu    t5, a1, draw_beacon_done    # py >= H -> rest off-screen

        # Hoist row offsets out of inner loop.
        mul     a3, t5, t6                  # row_off_fb  = py * SCREEN_W
        mul     a4, t4, t3                  # row_off_img = dy * dia

        li      a5, 0                       # dx = 0

draw_beacon_inner:
        bgeu    a5, t3, draw_beacon_next    # dx >= dia
        add     a6, t1, a5                  # px = x + dx
        bgeu    a6, t6, draw_beacon_next    # px >= W -> rest off-screen right

        # color = img[row_off_img + dx]
        add     a7, a4, a5
        slli    a7, a7, 1
        add     a7, t0, a7
        lhu     a1, 0(a7)

        # fbuf[row_off_fb + px] = color
        add     a7, a3, a6
        slli    a7, a7, 1
        add     a7, a0, a7
        sh      a1, 0(a7)

        addi    a5, a5, 1
        j       draw_beacon_inner

draw_beacon_next:
        addi    t4, t4, 1
        j       draw_beacon_outer

draw_beacon_done:
        ret

        .end
