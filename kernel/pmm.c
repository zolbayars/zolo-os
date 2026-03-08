#include "pmm.h"
#include "string.h"
#include "kprintf.h"

/* =============================================================================
 * pmm.c — Physical Memory Manager (Bitmap Allocator)
 * =============================================================================
 *
 * This file implements the "parking attendant" described in pmm.h. The core
 * data structure is a bitmap — an array of bytes where each BIT represents one
 * 4 KiB frame of physical memory.
 *
 * BITMAP LAYOUT EXAMPLE (first 2 bytes = 16 frames = 64 KiB of RAM):
 *
 *   Byte 0:  [bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0]
 *   Byte 1:  [bit15 bit14 bit13 bit12 bit11 bit10 bit9 bit8]
 *
 *   bit0 = frame at 0x00000000, bit1 = frame at 0x00001000, etc.
 *   1 = used, 0 = free.
 *
 * To find which byte and bit correspond to a given frame number:
 *   byte index = frame_number / 8
 *   bit offset = frame_number % 8
 *
 * MULTIBOOT MEMORY MAP
 *
 * The bootloader gives us a linked list of memory regions. Each entry says:
 *   "from address X to address X+length, the memory is [available / reserved]."
 *
 * We only care about type 1 (available). Everything else (BIOS, ACPI, device
 * memory) we leave marked as used in the bitmap. Think of it like a city map
 * where some blocks are parks (available) and some are government buildings
 * (reserved) — we can only build on the parks.
 */

/* ---------------------------------------------------------------------------
 * Multiboot Info Structures
 *
 * These match the Multiboot specification (v1). The bootloader fills these in
 * and passes us a pointer to the top-level struct in the EBX register.
 *
 * We only need a few fields from the main struct:
 *   - flags:    tells us which fields are valid (bit 6 = memory map present)
 *   - mmap_length / mmap_addr: location and size of the memory map array
 * --------------------------------------------------------------------------- */

/* The main multiboot info structure. We only define fields we actually use.
 * The full struct has many more fields, but C lets us define a partial struct
 * as long as we only access the fields we've defined and they're at the right
 * offsets. We pad with dummy fields to keep offsets correct. */
typedef struct {
    uint32_t flags;             /* Offset 0:  which info fields are valid */
    uint32_t mem_lower;         /* Offset 4:  KiB of low memory (below 1 MiB) */
    uint32_t mem_upper;         /* Offset 8:  KiB of upper memory (above 1 MiB) */
    uint32_t boot_device;       /* Offset 12 */
    uint32_t cmdline;           /* Offset 16 */
    uint32_t mods_count;        /* Offset 20 */
    uint32_t mods_addr;         /* Offset 24 */
    uint32_t syms[4];           /* Offset 28-44: ELF section header info */
    uint32_t mmap_length;       /* Offset 44: total size of memory map in bytes */
    uint32_t mmap_addr;         /* Offset 48: physical address of memory map */
} multiboot_info_t;

/* Each entry in the memory map. The bootloader creates an array of these.
 *
 * IMPORTANT QUIRK: each entry starts with a `size` field that tells you how
 * many bytes follow (NOT including the size field itself). The typical value
 * is 20, meaning the entry is 24 bytes total (4 for size + 20 for the rest).
 * To advance to the next entry: next = (void*)entry + entry->size + 4.
 */
typedef struct {
    uint32_t size;              /* Size of this entry (not counting this field) */
    uint32_t base_addr_low;     /* Low 32 bits of region start address */
    uint32_t base_addr_high;    /* High 32 bits (always 0 for <4 GiB systems) */
    uint32_t length_low;        /* Low 32 bits of region length */
    uint32_t length_high;       /* High 32 bits of length */
    uint32_t type;              /* 1 = available RAM, anything else = reserved */
} multiboot_mmap_entry_t;

/* Memory map entry types */
#define MULTIBOOT_MEMORY_AVAILABLE 1

/* Multiboot flags bit that indicates the memory map is valid */
#define MULTIBOOT_INFO_MEM_MAP 0x40   /* bit 6 */

/* ---------------------------------------------------------------------------
 * Bitmap state
 * --------------------------------------------------------------------------- */

/* Pointer to the bitmap (placed right after _kernel_end) */
static uint8_t* bitmap;

/* Total number of frames we're tracking */
static uint32_t total_frames;

/* Number of currently free frames */
static uint32_t free_frames;

/* The linker script provides this symbol at the end of the kernel image.
 * &_kernel_end gives us the first usable address after the kernel. */
extern uint32_t _kernel_end;

/* ---------------------------------------------------------------------------
 * Bitmap helpers — manipulate individual bits
 *
 * Each byte in the bitmap holds 8 frame states. To work with a specific
 * frame, we need to find the right byte and the right bit within that byte.
 *
 * Division by 8 gives the byte index, modulo 8 gives the bit position.
 * We use shifts and masks instead of / and % because the compiler generates
 * the same code and it makes the bit-level intent clearer.
 * --------------------------------------------------------------------------- */

/* Mark a frame as USED (set its bit to 1) */
static void bitmap_set(uint32_t frame) {
    bitmap[frame / 8] |= (1 << (frame % 8));
}

/* Mark a frame as FREE (clear its bit to 0) */
static void bitmap_clear(uint32_t frame) {
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

/* Check if a frame is USED (returns nonzero if used, 0 if free) */
static uint32_t bitmap_test(uint32_t frame) {
    return bitmap[frame / 8] & (1 << (frame % 8));
}

/* ---------------------------------------------------------------------------
 * pmm_init — Discover RAM and build the frame bitmap
 *
 * Steps:
 *   1. Read the multiboot memory map to find the highest usable address.
 *      This tells us how many frames we need to track.
 *   2. Place the bitmap right after _kernel_end.
 *   3. Mark ALL frames as used initially (safe default — assume everything
 *      is off-limits until proven otherwise).
 *   4. Walk the memory map again: for each "available" region, mark those
 *      frames as free.
 *   5. Re-mark the kernel + bitmap area as used (so we don't accidentally
 *      allocate over our own code or bitmap).
 *   6. Also mark frame 0 as used — the first 4 KiB of memory is used by
 *      the BIOS interrupt vector table and other legacy structures. We never
 *      want to hand out address 0x0 as an allocation anyway (it looks like
 *      NULL, which would confuse error checks).
 * --------------------------------------------------------------------------- */
void pmm_init(uint32_t mboot_addr) {
    multiboot_info_t* mboot = (multiboot_info_t*)mboot_addr;

    /* Verify the memory map is present */
    if (!(mboot->flags & MULTIBOOT_INFO_MEM_MAP)) {
        kprintf("[FAIL] No multiboot memory map available!\n");
        return;
    }

    /* --- Step 1: Find the highest usable physical address --- */
    uint32_t highest_addr = 0;

    /* Walk the memory map entries.
     * The map is a packed array of variable-size entries starting at mmap_addr.
     * Each entry's `size` field tells us how far to jump to the next one.
     * We stop when offset reaches mmap_length. */
    uint32_t offset = 0;
    while (offset < mboot->mmap_length) {
        multiboot_mmap_entry_t* entry =
            (multiboot_mmap_entry_t*)(mboot->mmap_addr + offset);

        /* We only handle the low 32 bits — our kernel is 32-bit, so we can't
         * address anything above 4 GiB anyway. */
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->base_addr_high == 0) {
            uint32_t region_end = entry->base_addr_low + entry->length_low;
            if (region_end > highest_addr) {
                highest_addr = region_end;
            }
        }

        /* Advance to the next entry: skip `size` bytes + the 4-byte size field */
        offset += entry->size + sizeof(entry->size);
    }

    /* --- Step 2: Calculate bitmap size and place it after the kernel --- */

    /* Total frames = highest address / frame size.
     * Example: 128 MiB = 0x08000000 → 32768 frames */
    total_frames = highest_addr / FRAME_SIZE;

    /* Bitmap size in bytes: 1 bit per frame, 8 bits per byte, round up */
    uint32_t bitmap_size = (total_frames + 7) / 8;

    /* Place bitmap right after the kernel image.
     * _kernel_end is a symbol, not a variable — take its address to get the
     * first byte after the kernel. */
    bitmap = (uint8_t*)&_kernel_end;

    /* --- Step 3: Mark ALL frames as used (conservative default) ---
     * 0xFF means all bits set = all frames used. We'll free the available
     * ones in the next step. This "deny by default" approach prevents us
     * from accidentally handing out reserved memory. */
    memset(bitmap, 0xFF, bitmap_size);
    free_frames = 0;

    /* --- Step 4: Free the regions the bootloader told us are available --- */
    offset = 0;
    while (offset < mboot->mmap_length) {
        multiboot_mmap_entry_t* entry =
            (multiboot_mmap_entry_t*)(mboot->mmap_addr + offset);

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->base_addr_high == 0) {
            /* Convert byte addresses to frame numbers.
             * Align start UP to the next frame boundary (don't give out partial frames).
             * Align end DOWN (same reason). */
            uint32_t region_start = entry->base_addr_low;
            uint32_t region_end   = entry->base_addr_low + entry->length_low;

            /* Round start up to next frame boundary */
            uint32_t frame_start = (region_start + FRAME_SIZE - 1) / FRAME_SIZE;
            /* Round end down to frame boundary */
            uint32_t frame_end = region_end / FRAME_SIZE;

            for (uint32_t f = frame_start; f < frame_end && f < total_frames; f++) {
                bitmap_clear(f);
                free_frames++;
            }
        }

        offset += entry->size + sizeof(entry->size);
    }

    /* --- Step 5: Mark the kernel + bitmap area as USED ---
     * The kernel lives from 0x100000 (1 MiB, set in linker.ld) to _kernel_end.
     * The bitmap lives from _kernel_end to _kernel_end + bitmap_size.
     * We must protect this entire range from being allocated. */
    uint32_t protected_start = 0x100000;  /* kernel load address */
    uint32_t protected_end   = (uint32_t)bitmap + bitmap_size;

    uint32_t prot_frame_start = protected_start / FRAME_SIZE;
    uint32_t prot_frame_end   = (protected_end + FRAME_SIZE - 1) / FRAME_SIZE;

    for (uint32_t f = prot_frame_start; f < prot_frame_end; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            free_frames--;
        }
    }

    /* --- Step 6: Always mark frame 0 as used ---
     * Frame 0 (0x0000–0x0FFF) contains the real-mode interrupt vector table
     * and BIOS data area. Also, returning address 0 from pmm_alloc_frame()
     * would look like NULL (allocation failure), so we never hand it out. */
    if (!bitmap_test(0)) {
        bitmap_set(0);
        free_frames--;
    }

    /* Print summary */
    uint32_t total_mb = (uint32_t)((uint64_t)total_frames * FRAME_SIZE / 1024 / 1024);
    uint32_t free_mb  = (uint32_t)((uint64_t)free_frames * FRAME_SIZE / 1024 / 1024);
    kprintf("PMM: %u MiB total, %u MiB free (%u/%u frames)\n",
            total_mb, free_mb, free_frames, total_frames);
}

/* ---------------------------------------------------------------------------
 * pmm_alloc_frame — Find and return any free 4 KiB frame
 *
 * Strategy: linear scan through the bitmap looking for the first 0 bit.
 * We first check byte-level (skip bytes that are 0xFF = all 8 frames used),
 * then bit-level within the first non-full byte.
 *
 * Returns the physical address of the allocated frame, or 0 on failure.
 * --------------------------------------------------------------------------- */
uint32_t pmm_alloc_frame(void) {
    uint32_t bitmap_bytes = (total_frames + 7) / 8;

    for (uint32_t i = 0; i < bitmap_bytes; i++) {
        /* Skip fully-used bytes (all 8 bits set = no free frames here).
         * This is the fast path — most of the bitmap will be full in a
         * busy system, so skipping whole bytes saves a lot of time. */
        if (bitmap[i] == 0xFF) continue;

        /* Found a byte with at least one free bit — find which one */
        for (uint32_t bit = 0; bit < 8; bit++) {
            uint32_t frame = i * 8 + bit;
            if (frame >= total_frames) return 0;  /* past end of RAM */

            if (!bitmap_test(frame)) {
                bitmap_set(frame);
                free_frames--;
                /* Convert frame number to physical address:
                 * frame 0 = 0x0, frame 1 = 0x1000, frame 256 = 0x100000, etc. */
                return frame * FRAME_SIZE;
            }
        }
    }

    /* No free frames — we're out of physical memory */
    return 0;
}

/* ---------------------------------------------------------------------------
 * pmm_free_frame — Return a frame to the free pool
 * --------------------------------------------------------------------------- */
void pmm_free_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / FRAME_SIZE;

    /* Sanity checks */
    if (frame >= total_frames) return;  /* address out of range */
    if (!bitmap_test(frame)) return;    /* already free — nothing to do */

    bitmap_clear(frame);
    free_frames++;
}

/* ---------------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------------- */
uint32_t pmm_get_free_count(void) {
    return free_frames;
}

uint32_t pmm_get_total_count(void) {
    return total_frames;
}

uint32_t pmm_get_bitmap_end(void) {
    uint32_t bitmap_size = (total_frames + 7) / 8;
    return (uint32_t)bitmap + bitmap_size;
}
