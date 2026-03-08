#ifndef PAGING_H
#define PAGING_H

/* =============================================================================
 * paging.h — x86 Virtual Memory (Paging)
 * =============================================================================
 *
 * WHAT IS PAGING?
 *
 * Without paging, every memory address the CPU uses is a physical address —
 * it goes straight to the RAM chip. This works, but it has problems:
 *   - Programs can stomp on each other's memory
 *   - Programs can stomp on the kernel
 *   - You can't give a program the illusion of having more memory than exists
 *
 * Paging adds a translation layer: the CPU uses "virtual addresses," and the
 * hardware automatically translates them to physical addresses before the
 * memory access reaches RAM. It's like a mail forwarding service — you send
 * a letter to a virtual P.O. box, and the post office routes it to the real
 * street address behind the scenes.
 *
 * HOW x86 TWO-LEVEL PAGING WORKS
 *
 * The 4 GiB virtual address space is split into pages (4 KiB each).
 * Translation uses two levels of lookup tables:
 *
 *   Page Directory (PD):  1024 entries, each pointing to a Page Table
 *   Page Table (PT):      1024 entries, each pointing to a 4 KiB physical frame
 *
 * A virtual address is split into three parts:
 *
 *   [bits 31-22]  Directory index  (10 bits → selects 1 of 1024 PD entries)
 *   [bits 21-12]  Table index      (10 bits → selects 1 of 1024 PT entries)
 *   [bits 11-0]   Offset           (12 bits → byte offset within the 4 KiB page)
 *
 * ANALOGY: Think of it like finding a book in a library.
 *   - The page directory is the floor map: "Fiction is on floor 3."
 *   - The page table is the shelf index on that floor: "Shelf 7."
 *   - The offset is the position on the shelf: "5th book from the left."
 *
 * Total: 1024 PD entries × 1024 PT entries × 4096 bytes = 4 GiB. Exactly
 * covers the full 32-bit address space.
 *
 * IDENTITY MAPPING
 *
 * For now, we set up "identity mapping" — virtual address X maps to physical
 * address X. This means the CPU's view of memory doesn't change when we turn
 * paging on. If we didn't do this, the instruction pointer (EIP) would
 * suddenly point to an unmapped address the instant paging is enabled, causing
 * an immediate triple fault (crash).
 *
 * We identity-map the first 16 MiB, which covers:
 *   - The kernel at 1 MiB
 *   - The VGA buffer at 0xB8000
 *   - The PMM bitmap (right after the kernel)
 *   - Plenty of room for the page tables themselves
 *
 * ENABLING PAGING
 *
 * Two CPU control registers are involved:
 *   CR3 — holds the physical address of the page directory
 *   CR0 — bit 31 (PG) enables/disables paging
 *
 * We load CR3 with our page directory address, then set CR0.PG = 1.
 * From that moment on, every memory access goes through the translation tables.
 */

#include "types.h"

/* Page/frame size: 4 KiB */
#define PAGE_SIZE 4096

/* Page table/directory entry flags (bits 0-11 of each entry).
 * The upper 20 bits hold the physical frame address (aligned to 4 KiB,
 * so the low 12 bits are always zero and can be reused as flags). */
#define PTE_PRESENT   0x01  /* Page is present in physical memory */
#define PTE_WRITABLE  0x02  /* Page is writable (otherwise read-only) */
#define PTE_USER      0x04  /* Page is accessible from user mode (ring 3) */

/* Number of entries in a page directory or page table */
#define ENTRIES_PER_TABLE 1024

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize paging: create page directory and tables, identity-map the first
 * 16 MiB, load CR3, and enable paging by setting CR0.PG.
 *
 * MUST be called after pmm_init() (we need the PMM to allocate frames for
 * page tables) and before any code that depends on virtual memory. */
void paging_init(void);

#endif /* PAGING_H */
