#ifndef VGA_H
#define VGA_H

/* =============================================================================
 * vga.h — VGA Text Mode Driver
 * =============================================================================
 *
 * VGA text mode gives us an 80x25 character grid. Each cell in the grid is
 * a 16-bit value stored at physical address 0xB8000:
 *
 *   Bits  0–7:  ASCII character code
 *   Bits  8–11: Foreground color (0–15)
 *   Bits 12–14: Background color (0–7, bit 15 enables blinking if set)
 *
 * To print the letter 'A' in white on black at row 0, column 0:
 *   uint16_t* vga = (uint16_t*)0xB8000;
 *   vga[0] = 'A' | (0x0F << 8);   // 0x0F = white fg, black bg
 *
 * That's it — no syscalls, no GPU, just a memory write.
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
