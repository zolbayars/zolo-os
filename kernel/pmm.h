#ifndef PMM_H
#define PMM_H

/* =============================================================================
 * pmm.h — Physical Memory Manager
 * =============================================================================
 *
 * WHAT IS PHYSICAL MEMORY MANAGEMENT?
 *
 * Your computer has RAM — a big slab of bytes. When the OS needs memory (to
 * load a program, allocate a buffer, create a page table), it needs to hand
 * out chunks of that RAM and track which chunks are already in use.
 *
 * ANALOGY: Imagine a parking garage with thousands of numbered spaces. The PMM
 * is the parking attendant who keeps a chart of which spaces are taken (1) and
 * which are free (0). When a car arrives, the attendant finds an empty space
 * and marks it taken. When a car leaves, the attendant marks the space free.
 *
 * HOW IT WORKS — BITMAP ALLOCATOR
 *
 * We divide all physical RAM into fixed-size blocks called "frames." Each frame
 * is 4 KiB (4096 bytes) — the same size as a page in x86 virtual memory. This
 * is no coincidence: the paging hardware maps virtual pages to physical frames,
 * so they must be the same size.
 *
 * To track which frames are used, we use a bitmap: a giant array of bits where
 * each bit represents one frame. Bit 0 = frame 0 (address 0x0000–0x0FFF),
 * bit 1 = frame 1 (address 0x1000–0x1FFF), and so on.
 *
 *   Bit value 0 = frame is FREE   (parking space is empty)
 *   Bit value 1 = frame is USED   (parking space is taken)
 *
 * For 128 MiB of RAM: 128 MiB / 4 KiB = 32768 frames = 32768 bits = 4 KiB of
 * bitmap. So tracking 128 MiB of RAM costs us only 4 KiB of memory. Very cheap.
 *
 * WHERE THE BITMAP LIVES
 *
 * We place the bitmap immediately after the kernel's code and data in memory.
 * The linker script exports a symbol called `_kernel_end` that marks the byte
 * right after the last kernel section. We start the bitmap there.
 *
 * HOW WE KNOW WHAT MEMORY EXISTS
 *
 * We can't just assume 128 MiB of RAM is available. The bootloader (via the
 * Multiboot standard) passes us a memory map — a list of regions describing
 * which ranges of physical memory are usable and which are reserved (BIOS,
 * hardware devices, etc.). We walk that list and mark usable regions as free
 * in our bitmap, then mark the kernel + bitmap area as used.
 */

#include "types.h"

/* Size of a physical memory frame (and a virtual page) in bytes */
#define FRAME_SIZE 4096

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the physical memory manager.
 * Parses the multiboot memory map to discover available RAM, sets up the
 * bitmap, and marks the kernel + bitmap region as used.
 *
 * @mboot_addr: physical address of the multiboot info structure (passed to
 *              kernel_main by the bootloader via EBX register)
 */
void pmm_init(uint32_t mboot_addr);

/* Allocate a single 4 KiB physical frame.
 * Scans the bitmap for the first free frame, marks it used, and returns its
 * physical address. Returns 0 if no free frames are available (out of memory).
 *
 * ANALOGY: "Give me any empty parking space."
 */
uint32_t pmm_alloc_frame(void);

/* Free a previously allocated frame, making it available for reuse.
 * @frame_addr: the physical address returned by pmm_alloc_frame().
 *              Must be 4 KiB-aligned. Freeing an already-free frame is a no-op.
 *
 * ANALOGY: "I'm leaving this parking space, mark it empty."
 */
void pmm_free_frame(uint32_t frame_addr);

/* Return the number of free 4 KiB frames currently available. */
uint32_t pmm_get_free_count(void);

/* Return the total number of frames the PMM is tracking. */
uint32_t pmm_get_total_count(void);

/* Return the physical address right after the PMM bitmap.
 * Used by the heap to know where it can safely start allocating. */
uint32_t pmm_get_bitmap_end(void);

#endif /* PMM_H */
