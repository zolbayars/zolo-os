/*
 * ZoloOS Linker Script
 *
 * This controls how the linker arranges our kernel binary in memory.
 *
 * Key concept: When the BIOS/bootloader loads our kernel, it places it at
 * a physical memory address. This script says WHERE in memory each section
 * goes. We start at 1 MiB (0x100000) because everything below that is
 * reserved by the BIOS for legacy reasons (interrupt vector table, VGA
 * memory at 0xB8000, BIOS ROM, etc.).
 *
 * The .multiboot section MUST appear within the first 8 KiB of the binary,
 * or the bootloader won't find the multiboot header and will refuse to boot.
 *
 * Memory layout after linking:
 *   0x00100000  .multiboot  (multiboot header - must be first!)
 *               .text       (kernel code)
 *               .rodata     (read-only data: string literals, const arrays)
 *               .data       (initialized global/static variables)
 *               .bss        (uninitialized globals - zeroed at boot)
 *   _kernel_end             (symbol marking end of kernel - used by PMM later)
 */

ENTRY(_start)

SECTIONS {
    /* Start at 1 MiB - conventional load address for multiboot kernels */
    . = 1M;

    /* Code sections - .multiboot first so the header is in the first 8 KiB */
    .text : ALIGN(4K) {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data (string literals, lookup tables, etc.) */
    .rodata : ALIGN(4K) {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized read-write data */
    .data : ALIGN(4K) {
        *(.data)
    }

    /* Uninitialized data (zeroed by the bootloader per multiboot spec) */
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    /* This symbol lets our C code know where the kernel ends in memory.
     * Used later by the physical memory manager to find free RAM. */
    _kernel_end = .;
}
