#ifndef FB_H
#define FB_H

/* =============================================================================
 * fb.h — VESA Linear Framebuffer Driver
 * =============================================================================
 *
 * WHAT IS A FRAMEBUFFER?
 *
 * A framebuffer is a block of memory where each byte (or group of bytes)
 * represents one pixel on screen. Unlike VGA text mode (which stores
 * characters), a framebuffer stores raw pixel colors. This gives us full
 * control over every dot on the display — we can draw shapes, text with
 * custom fonts, images, and build a graphical user interface.
 *
 * ANALOGY: VGA text mode is like a typewriter — you can place characters in
 * a grid but nothing else. A framebuffer is like a canvas — you can paint
 * any pixel any color. More powerful, but you have to draw everything yourself,
 * including text (by painting individual pixels that form letter shapes).
 *
 * HOW IT WORKS (32-bit color)
 *
 * Our framebuffer uses 32 bits per pixel (bpp). Each pixel is 4 bytes:
 *
 *   [byte 0: Blue] [byte 1: Green] [byte 2: Red] [byte 3: unused/alpha]
 *
 * This is "BGRA" format (common on x86). To set pixel (x, y) to a color:
 *
 *   address = framebuffer_base + y * pitch + x * 4
 *
 * Where `pitch` is the number of BYTES per row (not pixels — it may include
 * padding for alignment). Always use pitch, never width*4.
 *
 * COLOR FORMAT
 *
 * We use 32-bit packed colors: 0x00RRGGBB (alpha/unused in top byte).
 * Examples:
 *   0x00FF0000 = pure red
 *   0x0000FF00 = pure green
 *   0x000000FF = pure blue
 *   0x00FFFFFF = white
 *   0x00000000 = black
 *   0x00333333 = dark grey
 */

#include "types.h"
#include "multiboot.h"

/* Convenience color constants (0x00RRGGBB format) */
#define FB_BLACK       0x00000000
#define FB_WHITE       0x00FFFFFF
#define FB_RED         0x00FF0000
#define FB_GREEN       0x0000FF00
#define FB_BLUE        0x000000FF
#define FB_CYAN        0x0000FFFF
#define FB_MAGENTA     0x00FF00FF
#define FB_YELLOW      0x00FFFF00
#define FB_DARK_GREY   0x00333333
#define FB_MED_GREY    0x00808080
#define FB_LIGHT_GREY  0x00C0C0C0

/* Titlebar and UI colors */
#define FB_TITLEBAR    0x004488CC
#define FB_TITLEBAR_HI 0x005599DD
#define FB_BORDER      0x00666666
#define FB_BG_DESKTOP  0x002B2B3B
#define FB_BG_WINDOW   0x001E1E2E

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the framebuffer from multiboot info.
 * Parses the framebuffer fields, identity-maps the framebuffer memory,
 * and stores the configuration for use by drawing functions.
 * Returns 1 on success, 0 if no framebuffer is available. */
int fb_init(multiboot_info_t* mboot);

/* Set a single pixel at (x, y) to the given color. */
void fb_plot(uint32_t x, uint32_t y, uint32_t color);

/* Fill a solid rectangle. */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Draw a 1-pixel outline rectangle. */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Draw a line from (x0,y0) to (x1,y1) using Bresenham's algorithm. */
void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/* Fill the entire screen with a color. */
void fb_clear(uint32_t color);

/* Draw a single character at pixel position (x, y) using the 8x16 bitmap font.
 * fg = foreground (text) color, bg = background color. */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

/* Draw a null-terminated string starting at (x, y). Characters wrap at screen edge. */
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);

/* Get framebuffer dimensions */
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);

/* Returns 1 if framebuffer mode is active, 0 if still in VGA text mode */
int fb_is_active(void);

#endif /* FB_H */
