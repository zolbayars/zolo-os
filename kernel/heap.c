#include "heap.h"
#include "kprintf.h"

/* =============================================================================
 * heap.c — Bump (Watermark) Allocator
 * =============================================================================
 *
 * This is the simplest possible dynamic memory allocator. It works like a
 * stack of sticky notes — you keep peeling off the next one, and there's no
 * way to put one back in the middle.
 *
 * State:
 *   heap_start — fixed base address of the heap
 *   heap_end   — fixed upper limit (heap_start + heap_size)
 *   next_free  — "watermark" that advances with each allocation
 *
 *   [already allocated]  [next_free →→→→→→→→→→ free space]  [heap_end]
 *
 * Each kmalloc(N) does:
 *   1. Align next_free up to 4 bytes (so pointers and ints are aligned)
 *   2. Check that next_free + N doesn't exceed heap_end
 *   3. Save next_free as the return value
 *   4. Advance next_free by N
 *
 * kfree() is intentionally a no-op. In the bump allocator model, memory is
 * never reclaimed. This is fine for early kernel bringup where allocations
 * are mostly permanent (task structs, buffers that live for the kernel's
 * entire lifetime). Later we'll replace this with a free-list allocator.
 */

/* Heap boundaries (set once in heap_init, never changed) */
static uint32_t heap_start;
static uint32_t heap_end;

/* The watermark — points to the next free byte */
static uint32_t next_free;

/* ---------------------------------------------------------------------------
 * heap_init — Set up the heap region
 * --------------------------------------------------------------------------- */
void heap_init(uint32_t start, uint32_t size) {
    heap_start = start;
    heap_end   = start + size;
    next_free  = start;

    kprintf("Heap: %u KiB at 0x%x\n", size / 1024, start);
}

/* ---------------------------------------------------------------------------
 * kmalloc — Allocate memory from the heap
 *
 * We align every allocation to 4 bytes. On x86, unaligned access works but
 * is slower. More importantly, some data structures (like page directories)
 * require alignment. 4-byte alignment covers all 32-bit types safely.
 *
 * ALIGNMENT TRICK: To round up to the next multiple of 4:
 *   aligned = (value + 3) & ~3
 *
 * The `& ~3` clears the bottom 2 bits, rounding down. Adding 3 first ensures
 * we round UP instead of down. Example:
 *   value = 5  → (5+3)=8  → 8 & ~3 = 8   (aligned up from 5)
 *   value = 8  → (8+3)=11 → 11 & ~3 = 8  (already aligned, stays at 8)
 * --------------------------------------------------------------------------- */
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align the watermark up to a 4-byte boundary */
    next_free = (next_free + 3) & ~((uint32_t)3);

    /* Check for overflow */
    if (next_free + size > heap_end || next_free + size < next_free) {
        kprintf("kmalloc: out of heap memory! (requested %u bytes)\n", size);
        return NULL;
    }

    /* Hand out the current position and advance the watermark */
    void* ptr = (void*)next_free;
    next_free += size;

    return ptr;
}

/* ---------------------------------------------------------------------------
 * kfree — No-op in the bump allocator
 *
 * We accept the pointer parameter to match the standard free() signature,
 * so callers can write correct code now. When we upgrade to a free-list
 * allocator, this function will actually reclaim memory.
 * --------------------------------------------------------------------------- */
void kfree(void* ptr) {
    /* Suppress unused parameter warning */
    (void)ptr;
    /* Nothing to do — bump allocator can't free individual blocks. */
}

/* ---------------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------------- */
uint32_t heap_get_used(void) {
    return next_free - heap_start;
}

uint32_t heap_get_size(void) {
    return heap_end - heap_start;
}
