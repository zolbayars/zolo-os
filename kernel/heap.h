#ifndef HEAP_H
#define HEAP_H

/* =============================================================================
 * heap.h — Kernel Heap Allocator
 * =============================================================================
 *
 * WHAT IS A HEAP?
 *
 * The stack gives you automatic, short-lived memory — local variables that
 * disappear when a function returns. But sometimes you need memory that:
 *   - Outlives the function that created it
 *   - Has a size determined at runtime (not compile time)
 *   - Can be passed around between different parts of the kernel
 *
 * That's what the heap is for. In user-space you'd call malloc(). In the
 * kernel, we call kmalloc() — our own allocator that manages a region of
 * memory we've carved out for dynamic allocations.
 *
 * ANALOGY: The stack is like a stack of plates at a buffet — you can only
 * add/remove from the top, and when you walk away the plates go back. The
 * heap is like a warehouse — you can rent any shelf you want, keep it as
 * long as you need, and return it whenever. The warehouse manager (this
 * allocator) tracks which shelves are in use.
 *
 * BUMP ALLOCATOR (WATERMARK ALLOCATOR)
 *
 * The simplest possible heap. We maintain a pointer ("watermark") that starts
 * at the beginning of the heap. Each kmalloc() advances the pointer forward
 * by the requested size and returns the old position.
 *
 * Pros: Dead simple, very fast (O(1) allocation), no fragmentation worries
 * Cons: kfree() is a no-op — memory is never reclaimed until we implement
 *        a real allocator later
 *
 * ANALOGY: Imagine a roll of paper towels. Each allocation tears off a piece.
 * You can't put pieces back — you just keep tearing until the roll runs out.
 * Simple and fast, but wasteful. Good enough for early kernel bringup where
 * we mostly allocate things once and keep them forever.
 *
 * HEAP PLACEMENT
 *
 * The heap starts right after the PMM bitmap in memory:
 *
 *   [kernel code + data] [PMM bitmap] [heap →→→→→→→→→→...]
 *   ^                    ^            ^
 *   0x100000         _kernel_end    heap_start
 *
 * The PMM tells us where the bitmap ends, and that's where we start the heap.
 */

#include "types.h"

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the kernel heap.
 * @start: the physical address where the heap begins (after PMM bitmap)
 * @size:  total size of the heap in bytes
 */
void heap_init(uint32_t start, uint32_t size);

/* Allocate `size` bytes from the kernel heap.
 * Returns a pointer to the allocated memory, or NULL if the heap is full.
 * Memory is 4-byte aligned (suitable for any 32-bit data type). */
void* kmalloc(size_t size);

/* Free a previously allocated block. Currently a no-op in the bump allocator.
 * Exists so calling code can be written correctly from the start — when we
 * later replace the bump allocator with a real one, kfree() will work. */
void kfree(void* ptr);

/* Return the number of bytes currently allocated from the heap. */
uint32_t heap_get_used(void);

/* Return the total heap size in bytes. */
uint32_t heap_get_size(void);

#endif /* HEAP_H */
