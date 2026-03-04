#include "vga.h"
#include "kprintf.h"
#include "types.h"

/* =============================================================================
 * kernel.c — Kernel Entry Point
 * =============================================================================
 *
 * kernel_main is called from boot.asm after the stack is set up.
 * It receives two arguments the bootloader left in registers:
 *
 *   magic      — should be 0x2BADB002, confirms we were loaded by a
 *                multiboot-compliant bootloader (QEMU's or GRUB)
 *
 *   mboot_addr — physical address of the multiboot info structure,
 *                which contains the memory map, boot device info, etc.
 *                Used later when setting up memory management.
 *
 * Initialization order matters: each subsystem may depend on the ones
 * before it. VGA has no dependencies so it goes first — we want to be
 * able to print status messages as early as possible.
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
