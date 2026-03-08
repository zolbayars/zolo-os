#include "vga.h"
#include "kprintf.h"
#include "types.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"

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
 *   6. PMM — discover and track physical memory frames
 *   7. STI — only NOW do we tell the CPU to start accepting interrupts
 *
 * Think of it like wiring a house: you run all the cables and install all the
 * switches before you flip the main breaker. Turning on power before the
 * wiring is done causes short circuits.
 */

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mboot_addr) {
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
     * CPU descriptor tables and interrupt infrastructure
     * ----------------------------------------------------------------------- */
    gdt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("GDT installed (null, kernel code, kernel data)\n");

    idt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("IDT installed (32 exceptions + 16 IRQ slots)\n");

    irq_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("PIC remapped (IRQs 0-15 -> INT 32-47)\n");

    timer_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("PIT timer running at %d Hz\n", TIMER_HZ);

    keyboard_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("PS/2 keyboard ready\n");

    /* -----------------------------------------------------------------------
     * Physical memory manager — discover RAM and build the frame bitmap
     * ----------------------------------------------------------------------- */
    pmm_init(mboot_addr);
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("[OK] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("Physical memory: %u frames free (%u MiB usable)\n",
            pmm_get_free_count(),
            (uint32_t)((uint64_t)pmm_get_free_count() * FRAME_SIZE / 1024 / 1024));

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
    vga_print("ZoloOS is up.\n\n");

    /* -----------------------------------------------------------------------
     * Enable interrupts — the CPU starts responding to hardware events here.
     * Everything above (GDT, IDT, IRQ, timer, keyboard) must be fully set up
     * before this line. Turning on interrupts before the IDT is ready = triple fault.
     * ----------------------------------------------------------------------- */
    __asm__ __volatile__ ("sti");

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print("Type something (Escape to stop):\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);

    /* Echo keypresses to screen until Escape is pressed */
    char c;
    while ((c = keyboard_getchar()) != 0x1B) {  /* 0x1B = Escape */
        vga_putchar(c);
    }

    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_print("\n[Escape pressed — halting]\n");

halt:
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}
