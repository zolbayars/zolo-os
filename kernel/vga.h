#ifndef VGA_H
#define VGA_H

/* =============================================================================
 * vga.h — VGA Text Mode Driver
 * =============================================================================
 *
 * WHAT IS VGA TEXT MODE?
 *
 * VGA (Video Graphics Array) text mode is the simplest way to put characters
 * on screen in an x86 system. No GPU drivers, no framebuffers, no fonts to
 * render — just a block of memory that the display hardware reads and shows.
 *
 * ANALOGY: Think of it as a shared whiteboard between the CPU and the monitor.
 * The CPU writes characters into specific cells, and the monitor continuously
 * reads that memory and draws whatever it finds there. Write an 'A' to cell 0,
 * and the letter A appears in the top-left corner of the screen — instantly.
 *
 * HOW IT WORKS
 *
 * The VGA controller maps a fixed region of physical memory — starting at
 * address 0xB8000 — directly to the screen. The screen is an 80x25 grid of
 * character cells (80 columns, 25 rows). Each cell is 2 bytes (16 bits):
 *
 *   Byte 0 (bits 0–7):  The ASCII character to display (e.g. 0x41 = 'A')
 *   Byte 1 (bits 8–15): The color attribute:
 *                          bits 8–11:  foreground color (0–15)
 *                          bits 12–14: background color (0–7)
 *                          bit  15:    blink enable (we leave this off)
 *
 * Example — print 'A' in white on black at the top-left corner:
 *   uint16_t* vga = (uint16_t*)0xB8000;
 *   vga[0] = 'A' | (0x0F << 8);   // 0x0F = white foreground, black background
 *
 * That's it. No system calls. No drivers to load. Just a memory write.
 */

#include "types.h"

/* Screen dimensions */
#define VGA_COLS 80
#define VGA_ROWS 25

/* Physical address of the VGA text buffer */
#define VGA_BUFFER ((uint16_t*)0xB8000)

/* ---------------------------------------------------------------------------
 * VGA color palette
 *
 * The 4-bit color codes used by VGA text mode. These map directly to the
 * bits in the attribute byte — no lookup needed.
 * --------------------------------------------------------------------------- */
typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color_t;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the VGA driver: clear screen, set default color (white on black) */
void vga_init(void);

/* Change the current foreground and background color for subsequent writes */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/* Write a single character at the current cursor position, advance cursor */
void vga_putchar(char c);

/* Write a null-terminated string at the current cursor position */
void vga_print(const char* str);

/* Write a null-terminated string at a specific (row, col) without moving cursor */
void vga_print_at(const char* str, uint32_t row, uint32_t col);

/* Clear the entire screen and reset cursor to (0, 0) */
void vga_clear(void);

/* Get current cursor row and column */
uint32_t vga_get_row(void);
uint32_t vga_get_col(void);

#endif /* VGA_H */
