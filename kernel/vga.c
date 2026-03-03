#include "vga.h"
#include "string.h"

/* =============================================================================
 * vga.c — VGA Text Mode Driver Implementation
 * =============================================================================
 *
 * State we track:
 *   - cursor_row / cursor_col: where the next character will be written
 *   - current_color: the attribute byte applied to every character we write
 *   - vga_buffer: pointer to the VGA memory at 0xB8000
 */

/* Current cursor position */
static uint32_t cursor_row = 0;
static uint32_t cursor_col = 0;

/* Current color attribute byte: (bg << 4) | fg */
static uint8_t current_color = 0;

/* Pointer to the VGA text buffer in memory */
static uint16_t* vga_buffer = VGA_BUFFER;

/* ---------------------------------------------------------------------------
 * make_entry — Pack a character and color into a 16-bit VGA cell value
 *
 * The VGA cell format:
 *   bits  0-7:  ASCII character
 *   bits  8-15: color attribute byte
 *
 * Example: make_entry('A', 0x0F) → 0x0F41
 *   0x41 = ASCII 'A', 0x0F = white on black
 * --------------------------------------------------------------------------- */
static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* ---------------------------------------------------------------------------
 * make_color — Build the attribute byte from fg and bg color values
 *
 * Background occupies bits 4-6 (3 bits = 8 possible bg colors)
 * Foreground occupies bits 0-3 (4 bits = 16 possible fg colors)
 *
 * Example: make_color(VGA_WHITE, VGA_BLUE) → 0x1F
 *   0x1_ = blue background, 0x_F = white foreground
 * --------------------------------------------------------------------------- */
static inline uint8_t make_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)((bg << 4) | fg);
}

/* ---------------------------------------------------------------------------
 * vga_update_cursor — Move the hardware blinking cursor to (row, col)
 *
 * The VGA cursor position is controlled through two I/O port registers:
 *   Port 0x3D4: index register (tells the VGA chip which register to access)
 *   Port 0x3D5: data register (reads/writes the selected register)
 *
 * Register 0x0F: cursor location low byte  (bits 0-7 of linear position)
 * Register 0x0E: cursor location high byte (bits 8-15 of linear position)
 *
 * Linear position = row * VGA_COLS + col
 *
 * We use outb() from io.h to write to these ports.
 * --------------------------------------------------------------------------- */
static void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);

    /* Select register 0x0F (cursor low byte) and write low 8 bits of pos */
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)0x0F),       "Nd"((uint16_t)0x3D4));
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));

    /* Select register 0x0E (cursor high byte) and write high 8 bits of pos */
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)0x0E),        "Nd"((uint16_t)0x3D4));
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)(pos >> 8)),   "Nd"((uint16_t)0x3D5));
}

/* ---------------------------------------------------------------------------
 * vga_scroll — Scroll the screen up by one row
 *
 * When the cursor moves past row 24 (the last row), we need to scroll:
 *   1. Move rows 1-24 up to rows 0-23 (memmove handles the overlap)
 *   2. Clear row 24 (the new blank line at the bottom)
 *   3. Move the cursor back to the start of row 24
 *
 * Each row is VGA_COLS cells. Each cell is a uint16_t (2 bytes).
 * --------------------------------------------------------------------------- */
static void vga_scroll(void) {
    /* Shift all rows up by one: row[i] = row[i+1] */
    memmove(
        vga_buffer,                       /* dest: row 0 */
        vga_buffer + VGA_COLS,            /* src:  row 1 */
        (VGA_ROWS - 1) * VGA_COLS * sizeof(uint16_t)
    );

    /* Clear the last row by filling it with spaces in the current color */
    uint16_t blank = make_entry(' ', current_color);
    for (uint32_t col = 0; col < VGA_COLS; col++) {
        vga_buffer[(VGA_ROWS - 1) * VGA_COLS + col] = blank;
    }

    /* Keep the cursor on the last row */
    cursor_row = VGA_ROWS - 1;
}

/* ---------------------------------------------------------------------------
 * vga_init — Set up the VGA driver
 * --------------------------------------------------------------------------- */
void vga_init(void) {
    current_color = make_color(VGA_WHITE, VGA_BLACK);
    cursor_row    = 0;
    cursor_col    = 0;
    vga_buffer    = VGA_BUFFER;
    vga_clear();
}

/* ---------------------------------------------------------------------------
 * vga_set_color — Change the color used for subsequent character writes
 * --------------------------------------------------------------------------- */
void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = make_color(fg, bg);
}

/* ---------------------------------------------------------------------------
 * vga_clear — Fill the entire screen with spaces and reset the cursor
 * --------------------------------------------------------------------------- */
void vga_clear(void) {
    uint16_t blank = make_entry(' ', current_color);
    for (uint32_t i = 0; i < VGA_ROWS * VGA_COLS; i++) {
        vga_buffer[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

/* ---------------------------------------------------------------------------
 * vga_putchar — Write one character at the cursor, then advance it
 *
 * Handles:
 *   '\n'  — move to the start of the next row
 *   '\r'  — move to the start of the current row (carriage return)
 *   '\t'  — advance to the next tab stop (every 8 columns)
 *   '\b'  — backspace: move cursor left one column
 *   other — write the character and advance cursor right
 *
 * After every write, if cursor_col reaches VGA_COLS, wrap to the next row.
 * If cursor_row reaches VGA_ROWS, scroll the screen up.
 * --------------------------------------------------------------------------- */
void vga_putchar(char c) {
    switch (c) {
        case '\n':
            cursor_col = 0;
            cursor_row++;
            break;

        case '\r':
            cursor_col = 0;
            break;

        case '\t':
            /* Advance to next multiple of 8 */
            cursor_col = (cursor_col + 8) & ~7u;
            if (cursor_col >= VGA_COLS) {
                cursor_col = 0;
                cursor_row++;
            }
            break;

        case '\b':
            if (cursor_col > 0) {
                cursor_col--;
            }
            /* Overwrite with a space to visually erase the character */
            vga_buffer[cursor_row * VGA_COLS + cursor_col] =
                make_entry(' ', current_color);
            break;

        default:
            /* Write the character into the VGA buffer at the cursor position */
            vga_buffer[cursor_row * VGA_COLS + cursor_col] =
                make_entry(c, current_color);
            cursor_col++;

            /* Wrap to next row if we've gone past the last column */
            if (cursor_col >= VGA_COLS) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
    }

    /* Scroll up if the cursor has moved past the last row */
    if (cursor_row >= VGA_ROWS) {
        vga_scroll();
    }

    /* Move the hardware blinking cursor to match our tracked position */
    vga_update_cursor();
}

/* ---------------------------------------------------------------------------
 * vga_print — Write a null-terminated string at the current cursor position
 * --------------------------------------------------------------------------- */
void vga_print(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/* ---------------------------------------------------------------------------
 * vga_print_at — Write a string at a specific position without moving cursor
 *
 * Useful for updating fixed "widgets" on screen (e.g. a task counter on
 * row 24) without disrupting whatever the shell is doing at the cursor.
 * --------------------------------------------------------------------------- */
void vga_print_at(const char* str, uint32_t row, uint32_t col) {
    /* Save current cursor state */
    uint32_t saved_row = cursor_row;
    uint32_t saved_col = cursor_col;

    /* Move cursor to target position and print */
    cursor_row = row;
    cursor_col = col;
    vga_print(str);

    /* Restore cursor state */
    cursor_row = saved_row;
    cursor_col = saved_col;
    vga_update_cursor();
}

/* ---------------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------------- */
uint32_t vga_get_row(void) { return cursor_row; }
uint32_t vga_get_col(void) { return cursor_col; }
