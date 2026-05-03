// Copyright (c) 2024-2026 Yoonkyu Lee
// SPDX-License-Identifier: MIT
//
// memory.c -- minimal malloc/free for the skyline demo and test bench.
//
// The only call site (mp1.S::add_star) requests 16 bytes per star and
// frees them in remove_star, so the allocator keeps a free list of
// 16-byte blocks and falls back to a bump pointer for any larger
// request the test harness throws at it. Out-of-arena returns NULL,
// which add_star treats as "drop this star silently".

#include <stddef.h>
#include <stdint.h>

// 256 KB arena -- comfortably covers SKYLINE_WIN_MAX (4000 windows) and
// any reasonable star/window churn the bench runs.
#define ARENA_SIZE  (256u * 1024u)

static char         arena[ARENA_SIZE] __attribute__((aligned(16)));
static char *       arena_top = arena;
static char * const arena_end = arena + ARENA_SIZE;

// Singly-linked free list for 16-byte blocks.
struct fnode { struct fnode * next; };
static struct fnode * free16 = (struct fnode *)0;

// Reset the allocator. The test bench calls this between cases so
// allocations leaked by one test don't poison the next one's heap.
void memory_init(void) {
    arena_top = arena;
    free16 = (struct fnode *)0;
}

void * malloc(size_t n) {
    if (n == 0) return (void *)0;
    // Round up to 16-byte alignment so every allocation is suitable for
    // the smallest free-list bucket.
    n = (n + 15u) & ~(size_t)15u;

    if (n == 16u && free16 != (struct fnode *)0) {
        struct fnode * node = free16;
        free16 = node->next;
        return node;
    }
    if ((size_t)(arena_end - arena_top) < n)
        return (void *)0;

    void * p = arena_top;
    arena_top += n;
    return p;
}

void free(void * p) {
    if (p == (void *)0) return;
    // Treat every free as a 16-byte recycle. mp1's only allocator user
    // hands back exactly the 16 bytes it asked for. Larger bump-arena
    // allocations from the test bench are intentionally leaked: the
    // bench finishes in well under one arena.
    struct fnode * node = (struct fnode *)p;
    node->next = free16;
    free16 = node;
}
