#include "vga.h"
#include "kprintf.h"
#include "types.h"

/* =============================================================================
 * kernel.c — Kernel Entry Point
 * =============================================================================
 *
 * WHAT IS kernel_main?
 *
 * Every C program has a main() function — the entry point the OS calls after
 * setting up the process. kernel_main is the same idea, but one level lower:
 * it's the entry point the BOOTLOADER calls after setting up the CPU. There
 * is no OS above us. We are the OS.
 *
 * It's called from boot.asm, right after the stack is set up. Two arguments
 * are passed in that the bootloader left in CPU registers:
 *
 *   magic      — a specific number (0x2BADB002) that confirms the bootloader
 *                followed the Multiboot standard. If this is wrong, we know
 *                something went wrong before we even started.
 *
 *   mboot_addr — the physical memory address of a struct the bootloader filled
 *                in for us, containing info about the machine: how much RAM
 *                there is, what device we booted from, etc. We'll use this
 *                later when we add memory management.
 *
 * INITIALIZATION ORDER MATTERS
 *
 * Each subsystem may depend on the ones before it. We initialize in this order:
 *   1. VGA first — so we can print status messages for everything that follows
 *   2. GDT — CPU segment table, required before interrupts
 *   3. IDT — interrupt table, required before enabling any hardware events
 *   4. IRQ — remaps the hardware interrupt controller
 *   5. Timer, Keyboard — register their interrupt handlers
 *   6. STI — only NOW do we tell the CPU to start accepting interrupts
 *
 * Think of it like wiring a house: you run all the cables and install all the
 * switches before you flip the main breaker. Turning on power before the
 * wiring is done causes short circuits.
 */

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mboot_addr) {
    /* Suppress unused warning — mboot_addr will be used for memory management */
    (void)mboot_addr;

    /* -----------------------------------------------------------------------
     * Initialize display first so we can print status messages immediately
     * ----------------------------------------------------------------------- */
    vga_init();

    /* -----------------------------------------------------------------------
     * Print the boot banner
     * ----------------------------------------------------------------------- */
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("  ______     _        ___  ____  \n");
    vga_print(" |__  / ___ | | ___  / _ \\/ ___| \n");
    vga_print("   / / / _ \\| |/ _ \\| | | \\___ \\ \n");
    vga_print("  / /_| (_) | | (_) | |_| |___) |\n");
    vga_print(" /____\\___/|_|\\___/ \\___/|____/ \n");
    vga_print("\n");

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("                                v0.1.0\n");
    vga_print("\n");

    /* -----------------------------------------------------------------------
     * Verify we were booted by a multiboot-compliant loader
     * ----------------------------------------------------------------------- */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("[FAIL] Not loaded by a multiboot bootloader!\n");
        goto halt;
    }

    /* -----------------------------------------------------------------------
     * Print boot status messages
     * ----------------------------------------------------------------------- */
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("Multiboot magic verified\n");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("VGA text mode initialized (80x25)\n");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("Kernel loaded at 0x100000 (1 MiB)\n");

    /* -----------------------------------------------------------------------
     * Test VGA color output
     * ----------------------------------------------------------------------- */
    vga_print("\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("Color test: ");

    vga_color_t colors[] = {
        VGA_RED, VGA_LIGHT_RED, VGA_YELLOW, VGA_LIGHT_GREEN,
        VGA_LIGHT_CYAN, VGA_LIGHT_BLUE, VGA_LIGHT_MAGENTA, VGA_WHITE
    };
    const char* labels[] = {
        "RED ", "LRED ", "YEL ", "LGRN ",
        "LCYN ", "LBLU ", "LMAG ", "WHT "
    };
    for (int i = 0; i < 8; i++) {
        vga_set_color(colors[i], VGA_BLACK);
        vga_print(labels[i]);
    }
    vga_print("\n\n");

    /* -----------------------------------------------------------------------
     * Ready
     * ----------------------------------------------------------------------- */
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("ZoloOS is up.\n");

    /* -----------------------------------------------------------------------
     * kprintf smoke test
     * ----------------------------------------------------------------------- */
    vga_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("\nkprintf: str=%s dec=%d hex=%x char=%c\n", "hello", -42, 0xCAFE, '!');

halt:
    /* Halt the CPU. Interrupts are not yet enabled so nothing will wake us. */
    __asm__ __volatile__ ("cli");
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}
