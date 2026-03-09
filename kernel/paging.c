#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "kprintf.h"

/* =============================================================================
 * paging.c — x86 Two-Level Paging Setup
 * =============================================================================
 *
 * This file sets up identity mapping for the first 16 MiB of physical memory.
 * After this runs, virtual address 0x00100000 still maps to physical 0x00100000
 * — the CPU's view of memory is unchanged, but now the paging hardware is
 * active and we can remap addresses later (for user processes, for example).
 *
 * MEMORY LAYOUT OF THE PAGE STRUCTURES
 *
 * We need:
 *   1 Page Directory    = 1024 entries × 4 bytes = 4 KiB  (1 frame)
 *   4 Page Tables       = 1024 entries × 4 bytes = 4 KiB each  (4 frames)
 *     (4 tables × 4 MiB each = 16 MiB coverage)
 *
 * Total overhead: 5 frames = 20 KiB to map 16 MiB. Very efficient.
 *
 * We allocate these frames from the PMM. They must be page-aligned (4 KiB),
 * which pmm_alloc_frame() guarantees since it hands out whole frames.
 */

/* ---------------------------------------------------------------------------
 * The page directory — the "root" of the translation tree.
 *
 * We store it as a file-scope variable so we can reference it later when we
 * need to modify mappings. It's aligned to 4 KiB because CR3 requires the
 * page directory to be page-aligned (the low 12 bits of CR3 are flags/zero).
 *
 * Using a static array rather than PMM allocation keeps things simple and
 * guarantees the alignment. 4 KiB for the directory + 16 KiB for 4 page
 * tables = 20 KiB total — trivial cost.
 * --------------------------------------------------------------------------- */

/* Page directory: 1024 entries, each 4 bytes (uint32_t).
 * __attribute__((aligned(4096))) ensures this starts on a 4 KiB boundary. */
static uint32_t page_directory[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

/* Page tables for the first 16 MiB.
 * Each table maps 4 MiB (1024 pages × 4 KiB), so 4 tables cover 16 MiB. */
static uint32_t page_tables[4][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

/* ---------------------------------------------------------------------------
 * paging_init — Build the identity map and turn on paging
 *
 * Steps:
 *   1. Zero out the page directory (all entries "not present")
 *   2. Fill in 4 page tables, each identity-mapping 4 MiB
 *   3. Point the first 4 page directory entries at those tables
 *   4. Load CR3 with the page directory's physical address
 *   5. Set the PG bit in CR0 to enable paging
 *
 * After step 5, every memory access goes through the page directory → page
 * table → physical frame lookup. Since we identity-mapped everything the
 * kernel uses, nothing changes from the kernel's perspective — it just works.
 * --------------------------------------------------------------------------- */
void paging_init(void) {
    /* --- Step 1: Clear the page directory ---
     * All entries start as 0 = "not present." Any access to a virtual address
     * in an unmapped region will trigger a page fault (interrupt 14). */
    memset(page_directory, 0, sizeof(page_directory));

    /* --- Step 2 & 3: Fill page tables and wire them into the directory ---
     *
     * For each of the 4 page tables (covering 0–16 MiB):
     *   - Fill every entry with the identity-mapped physical address + flags
     *   - Tell the page directory about this table
     *
     * Each page table entry looks like:
     *   [31:12] Physical frame address (top 20 bits, since frames are 4 KiB-aligned)
     *   [11:0]  Flags (present, writable, etc.)
     *
     * For identity mapping: virtual page N → physical frame N.
     * Frame N's address = N × 4096 = N << 12.
     */
    for (int t = 0; t < 4; t++) {
        for (int p = 0; p < ENTRIES_PER_TABLE; p++) {
            /* Calculate the physical address this entry maps to.
             * Table t covers addresses starting at t * 4 MiB.
             * Entry p within that table covers page (t * 1024 + p).
             * Physical address = page_number * 4096. */
            uint32_t phys_addr = (t * ENTRIES_PER_TABLE + p) * PAGE_SIZE;

            /* Entry = physical address (top 20 bits) | flags (low 12 bits).
             * Since phys_addr is already 4 KiB-aligned, its low 12 bits are
             * all zero, so we can just OR in the flags. */
            page_tables[t][p] = phys_addr | PTE_PRESENT | PTE_WRITABLE;
        }

        /* Point page directory entry t at page table t.
         * The PD entry format is the same as a PT entry: top 20 bits = address
         * of the page table, low bits = flags. */
        page_directory[t] = (uint32_t)&page_tables[t] | PTE_PRESENT | PTE_WRITABLE;
    }

    /* --- Step 4: Load CR3 with the page directory's physical address ---
     *
     * CR3 (Control Register 3) tells the CPU where to find the page directory.
     * When the CPU needs to translate a virtual address, it reads CR3 to find
     * the page directory, then walks the two-level table from there.
     *
     * ANALOGY: CR3 is like setting your GPS's "home address." Every time
     * the CPU needs directions (address translation), it starts from CR3. */
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(page_directory));

    /* --- Step 5: Enable paging by setting bit 31 (PG) in CR0 ---
     *
     * CR0 is the main CPU control register. Bit 31 is the paging enable switch.
     * Once we set it, EVERY memory access — including fetching the very next
     * instruction — goes through the page tables. This is why identity mapping
     * is critical: if the instruction pointer (EIP) isn't mapped, the CPU
     * faults immediately.
     *
     * We read CR0, set bit 31, and write it back. We don't touch other bits
     * (like PE = protected mode enable in bit 0) because they're already set
     * correctly by the bootloader. */
    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;   /* Set bit 31 = PG (Paging Enable) */
    __asm__ __volatile__ ("mov %0, %%cr0" : : "r"(cr0));

    /* If we reach here, paging is active and the identity mapping works!
     * The CPU is now translating every virtual address through our page tables,
     * but since virtual = physical for the first 16 MiB, nothing has visibly
     * changed. The magic happens later when we create non-identity mappings
     * for user-space processes. */
    kprintf("Paging: identity-mapped first 16 MiB (%u page tables)\n", 4);
}

/* ---------------------------------------------------------------------------
 * paging_identity_map_region — Map a physical region into virtual memory
 *
 * This is used after paging is already enabled to add new mappings — for
 * example, to make the VESA framebuffer (at a high physical address like
 * 0xFD000000) accessible to the CPU.
 *
 * For each page in the region, we:
 *   1. Check if a page table exists for that address range
 *   2. If not, allocate one from the PMM and wire it into the page directory
 *   3. Set the page table entry to map virtual = physical (identity map)
 *
 * ANALOGY: Our initial setup built roads for the first 16 MiB of the city.
 * This function extends the road network to reach a new neighborhood
 * (the framebuffer) that's far outside the original city limits.
 * --------------------------------------------------------------------------- */
void paging_identity_map_region(uint32_t phys_start, uint32_t size) {
    /* Align start down and end up to page boundaries */
    uint32_t start = phys_start & ~(PAGE_SIZE - 1);
    uint32_t end = (phys_start + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint32_t pages_mapped = 0;

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        /* Which page directory entry covers this address?
         * Top 10 bits of the address = PD index (each PDE covers 4 MiB). */
        uint32_t pd_index = addr >> 22;

        /* Which page table entry within that table?
         * Middle 10 bits = PT index (each PTE covers 4 KiB). */
        uint32_t pt_index = (addr >> 12) & 0x3FF;

        /* If no page table exists for this PD entry, allocate one */
        if (!(page_directory[pd_index] & PTE_PRESENT)) {
            uint32_t pt_frame = pmm_alloc_frame();
            if (!pt_frame) {
                kprintf("paging: out of frames for page table!\n");
                return;
            }
            /* Zero the new page table (all entries "not present") */
            memset((void*)pt_frame, 0, PAGE_SIZE);
            /* Wire it into the page directory */
            page_directory[pd_index] = pt_frame | PTE_PRESENT | PTE_WRITABLE;
        }

        /* Get the page table's address (strip the flag bits from the PDE) */
        uint32_t* pt = (uint32_t*)(page_directory[pd_index] & ~0xFFF);

        /* Set the PTE: identity map (virtual addr = physical addr) */
        pt[pt_index] = addr | PTE_PRESENT | PTE_WRITABLE;
        pages_mapped++;
    }

    /* Flush the TLB by reloading CR3 — tells the CPU to forget any cached
     * translations that might be stale after we added new mappings.
     *
     * ANALOGY: After adding new roads to the map, you tell the GPS to
     * re-download the map so it knows about the new routes. */
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(page_directory));

    kprintf("Paging: mapped %u pages at 0x%x-0x%x\n",
            pages_mapped, start, end);
}
