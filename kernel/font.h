#ifndef FONT_H
#define FONT_H

/* =============================================================================
 * font.h — 8x16 Bitmap Font for Framebuffer Text Rendering
 * =============================================================================
 *
 * HOW BITMAP FONTS WORK
 *
 * In VGA text mode, the hardware draws characters for us — we just write an
 * ASCII code to memory and the VGA chip looks up the glyph shape internally.
 * In framebuffer mode, there's no such luxury. We have to draw each letter
 * pixel by pixel.
 *
 * A bitmap font stores each character as a grid of bits. Our font is 8 pixels
 * wide and 16 pixels tall. Each character is 16 bytes — one byte per row,
 * where each bit is one pixel (1 = foreground, 0 = background).
 *
 *   Example: the letter 'A' (8x16):
 *
 *     Row 0:  00000000  = 0x00  (blank top rows)
 *     Row 1:  00000000  = 0x00
 *     Row 2:  00011000  = 0x18  (top of the A)
 *     Row 3:  00111100  = 0x3C
 *     Row 4:  01100110  = 0x66  (sides spreading)
 *     Row 5:  01100110  = 0x66
 *     Row 6:  01111110  = 0x7E  (crossbar)
 *     Row 7:  01100110  = 0x66
 *     Row 8:  01100110  = 0x66
 *     Row 9:  01100110  = 0x66
 *     ...
 *
 * To draw character 'A' at pixel (x, y): for each row, check each bit.
 * If the bit is 1, draw the foreground color; if 0, draw the background.
 *
 * The MSB (bit 7) is the leftmost pixel. So to check pixel column `col`
 * in a row byte: `(row_byte >> (7 - col)) & 1`.
 *
 * FONT DIMENSIONS
 *
 * 8x16 is the classic VGA BIOS font size. At 1024x768 resolution:
 *   - 128 characters per row (1024 / 8)
 *   - 48 rows (768 / 16)
 *   - Total: 6144 characters on screen
 */

#include "types.h"

/* Font dimensions in pixels */
#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* The font covers ASCII 32 (space) through 126 (~) = 95 printable characters.
 * To look up character c: font_data[(c - 32) * FONT_HEIGHT] gives the first row. */
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR  126
#define FONT_NUM_CHARS  (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

/* The font bitmap data — defined in font.c */
extern const uint8_t font_data[FONT_NUM_CHARS * FONT_HEIGHT];

#endif /* FONT_H */
