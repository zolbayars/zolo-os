#ifndef MULTIBOOT_H
#define MULTIBOOT_H

/* =============================================================================
 * multiboot.h — Multiboot Information Structures
 * =============================================================================
 *
 * WHAT IS MULTIBOOT?
 *
 * Multiboot is a standard that defines how a bootloader and an OS kernel talk
 * to each other. The bootloader fills in an info struct with details about the
 * machine (how much RAM, what video mode, etc.) and passes a pointer to it
 * when it jumps to the kernel.
 *
 * ANALOGY: When you check into a hotel, the front desk gives you a welcome
 * packet: room number, WiFi password, breakfast hours. The multiboot info
 * struct is the machine's "welcome packet" — it tells the kernel everything
 * it needs to know about its new home.
 *
 * We define these structs to match the Multiboot v1 specification exactly.
 * Field offsets must be correct or we'll read garbage data.
 */

#include "types.h"

/* ---------------------------------------------------------------------------
 * Multiboot info flags — each bit indicates which fields are valid.
 *
 * The bootloader sets bits in the `flags` field to tell us which parts of
 * the info struct it actually filled in. We must check the relevant bit
 * before reading any optional field.
 * --------------------------------------------------------------------------- */
#define MULTIBOOT_INFO_MEMORY     0x001   /* bit 0:  mem_lower/mem_upper valid */
#define MULTIBOOT_INFO_MEM_MAP    0x040   /* bit 6:  mmap_length/mmap_addr valid */
#define MULTIBOOT_INFO_FRAMEBUF   0x800   /* bit 11: framebuffer info valid */

/* ---------------------------------------------------------------------------
 * multiboot_info_t — The main info struct passed by the bootloader
 *
 * This struct is at the physical address in EBX when _start runs.
 * We define ALL fields up to the framebuffer section so offsets are correct.
 * --------------------------------------------------------------------------- */
typedef struct {
    uint32_t flags;             /* Offset 0:  which fields are valid */
    uint32_t mem_lower;         /* Offset 4:  KiB of memory below 1 MiB */
    uint32_t mem_upper;         /* Offset 8:  KiB of memory above 1 MiB */
    uint32_t boot_device;       /* Offset 12 */
    uint32_t cmdline;           /* Offset 16 */
    uint32_t mods_count;        /* Offset 20 */
    uint32_t mods_addr;         /* Offset 24 */
    uint32_t syms[4];           /* Offset 28-40: ELF section header info */
    uint32_t mmap_length;       /* Offset 44: total size of memory map */
    uint32_t mmap_addr;         /* Offset 48: physical address of memory map */
    uint32_t drives_length;     /* Offset 52 */
    uint32_t drives_addr;       /* Offset 56 */
    uint32_t config_table;      /* Offset 60 */
    uint32_t boot_loader_name;  /* Offset 64 */
    uint32_t apm_table;         /* Offset 68 */
    uint32_t vbe_control_info;  /* Offset 72 */
    uint32_t vbe_mode_info;     /* Offset 76 */
    uint16_t vbe_mode;          /* Offset 80 */
    uint16_t vbe_interface_seg; /* Offset 82 */
    uint16_t vbe_interface_off; /* Offset 84 */
    uint16_t vbe_interface_len; /* Offset 86 */
    uint64_t framebuffer_addr;  /* Offset 88: physical address of framebuffer */
    uint32_t framebuffer_pitch; /* Offset 96: bytes per row */
    uint32_t framebuffer_width; /* Offset 100: pixels wide */
    uint32_t framebuffer_height;/* Offset 104: pixels tall */
    uint8_t  framebuffer_bpp;   /* Offset 108: bits per pixel (e.g. 32) */
    uint8_t  framebuffer_type;  /* Offset 109: 0=indexed, 1=RGB, 2=EGA text */
    /* Color info follows but we don't need it for 32-bit RGB */
} __attribute__((packed)) multiboot_info_t;

/* ---------------------------------------------------------------------------
 * multiboot_mmap_entry_t — One entry in the memory map
 *
 * The memory map is an array of these. Each describes a region of physical
 * memory and whether it's usable or reserved.
 *
 * QUIRK: the `size` field is NOT included in the size it reports. So the
 * total entry size is `size + 4`. To advance to the next entry:
 *   next = (void*)entry + entry->size + 4
 * --------------------------------------------------------------------------- */
typedef struct {
    uint32_t size;              /* Size of remaining fields (usually 20) */
    uint32_t base_addr_low;     /* Low 32 bits of region start */
    uint32_t base_addr_high;    /* High 32 bits (0 for <4 GiB) */
    uint32_t length_low;        /* Low 32 bits of region length */
    uint32_t length_high;       /* High 32 bits of length */
    uint32_t type;              /* 1 = available, anything else = reserved */
} __attribute__((packed)) multiboot_mmap_entry_t;

/* Memory map types */
#define MULTIBOOT_MEMORY_AVAILABLE 1

#endif /* MULTIBOOT_H */
