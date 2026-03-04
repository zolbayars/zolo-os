#include "gdt.h"
#include "types.h"

/* =============================================================================
 * gdt.c — Global Descriptor Table implementation
 * =============================================================================
 *
 * This file builds the GDT in memory and installs it in the CPU. See gdt.h
 * for a full explanation of what the GDT is and why we need it.
 *
 * STRUCTURE OF A GDT ENTRY (8 bytes)
 *
 * Each entry is a tightly packed 8-byte descriptor. The fields are split and
 * scattered across those 8 bytes in a way that looks bizarre — it's a legacy
 * of the 80286/80386 era where the format grew in stages and backwards
 * compatibility was preserved. Modern OS code just fills it in and moves on.
 *
 *  Byte: [ 0  1 ][ 2  3  4 ][ 5 ][ 6 ][ 7 ]
 *               ^limit_low  ^base_low ^base_mid
 *                                  ^access
 *                                       ^flags+limit_high
 *                                            ^base_high
 *
 * ACCESS BYTE (byte 5) — who can use this segment and how:
 *
 *   Bit 7:   Present    — must be 1 for the CPU to use this entry
 *   Bits 6-5: DPL       — privilege level: 0 = kernel (ring 0), 3 = user (ring 3)
 *   Bit 4:   Type       — 1 = code/data segment (what we want)
 *   Bit 3:   Executable — 1 = code (can be jumped to), 0 = data (read/write)
 *   Bit 2:   Direction  — 0 = segment grows up (normal)
 *   Bit 1:   RW         — for code: readable; for data: writable
 *   Bit 0:   Accessed   — CPU sets this when the segment is used; start at 0
 *
 * GRANULARITY BYTE (byte 6) — scale and size of the segment:
 *
 *   Bit 7: G  — Granularity: 0 = limit in bytes, 1 = limit in 4 KiB pages
 *   Bit 6: DB — 1 = 32-bit segment (use 32-bit instructions), 0 = 16-bit
 *   Bit 5: L  — 0 = not 64-bit (we're 32-bit)
 *   Bit 4: —  — available for OS use; we set to 0
 *   Bits 3-0: limit[16:19] — the top 4 bits of the 20-bit limit field
 */

/* A single GDT entry — 8 bytes, layout must match exactly what the CPU expects */
typedef struct {
    uint16_t limit_low;   /* bits  0-15 of the segment size limit       */
    uint16_t base_low;    /* bits  0-15 of the segment start address     */
    uint8_t  base_mid;    /* bits 16-23 of the segment start address     */
    uint8_t  access;      /* access byte: present, privilege, type flags */
    uint8_t  granularity; /* upper limit bits + G/DB/L flags             */
    uint8_t  base_high;   /* bits 24-31 of the segment start address     */
} __attribute__((packed)) gdt_entry_t;
/* __attribute__((packed)) tells the compiler: do NOT add padding bytes
 * between fields for alignment. The CPU expects exactly 8 bytes with no gaps. */

/* The GDTR — the 6-byte value we load into the CPU's GDT register via lgdt.
 * ANALOGY: this is like a library card that says "the catalogue is at shelf 42,
 * and it has 3 entries." The CPU reads this to find and size the GDT table. */
typedef struct {
    uint16_t limit;  /* size of the GDT in bytes, minus 1               */
    uint32_t base;   /* physical address of the first GDT entry in RAM  */
} __attribute__((packed)) gdtr_t;

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];  /* the actual table, lives in .bss  */
static gdtr_t      gdtr;              /* the pointer struct we give to lgdt */

/* Declared in boot/gdt.asm — loads GDTR and reloads all segment registers */
extern void gdt_flush(uint32_t gdtr_addr);

/* ---------------------------------------------------------------------------
 * gdt_set_entry — Fill in one row of the GDT table
 *
 * @i:      index (0 = null, 1 = kernel code, 2 = kernel data)
 * @base:   start address of the segment (0 for flat model)
 * @limit:  size of the segment - 1 (0xFFFFF for 4 GB with G=1)
 * @access: access byte (see bit field description above)
 * @gran:   granularity byte (see bit field description above)
 * --------------------------------------------------------------------------- */
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[i].base_low   = (uint16_t)(base & 0xFFFF);
    gdt[i].base_mid   = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].base_high  = (uint8_t)((base >> 24) & 0xFF);
    gdt[i].limit_low  = (uint16_t)(limit & 0xFFFF);
    /* Pack the top 4 bits of limit into the low nibble of granularity */
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[i].access     = access;
}

/* ---------------------------------------------------------------------------
 * gdt_init — Build the GDT and install it
 *
 * Access byte values used below:
 *   0x9A = 1001 1010 = present | ring 0 | code/data | executable | readable
 *   0x92 = 1001 0010 = present | ring 0 | code/data | data       | writable
 *
 * Granularity byte 0xCF = 1100 1111:
 *   G=1  (limit counts 4 KiB pages, so limit 0xFFFFF = 4 GiB total)
 *   DB=1 (32-bit segment)
 *   L=0  (not 64-bit)
 *   AVL=0
 *   limit[16:19] = 0xF
 * --------------------------------------------------------------------------- */
void gdt_init(void) {
    gdtr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtr.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0,       0x00, 0x00); /* null descriptor — required blank slot */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF); /* kernel code: ring 0, execute/read     */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF); /* kernel data: ring 0, read/write       */

    /* Hand the GDTR to the assembly routine which runs lgdt and reloads
     * all segment registers. After this returns, our GDT is active. */
    gdt_flush((uint32_t)&gdtr);
}
