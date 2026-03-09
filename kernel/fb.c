#include "fb.h"
#include "font.h"
#include "paging.h"
#include "string.h"
#include "kprintf.h"

/* =============================================================================
 * fb.c — VESA Linear Framebuffer Driver
 * =============================================================================
 *
 * This driver provides pixel-level drawing on a VESA linear framebuffer.
 * The bootloader (via Multiboot) sets up the video mode before our kernel
 * starts, and tells us the framebuffer's physical address, dimensions, and
 * pixel format through the multiboot info struct.
 *
 * MEMORY LAYOUT
 *
 * The framebuffer is a contiguous block of video memory at a physical address
 * chosen by the hardware (typically in the PCI memory space, e.g. 0xFD000000).
 * For a 1024x768x32 display:
 *   Total size = 1024 * 768 * 4 bytes = 3 MiB
 *
 * Since this address is far above our initial 16 MiB identity map, we must
 * add page table entries for it before we can write pixels. That's why
 * fb_init() calls paging_identity_map_region().
 *
 * DOUBLE BUFFERING NOTE
 *
 * We write directly to the framebuffer (no back buffer). This means drawing
 * operations are visible immediately, which can cause flicker during complex
 * redraws. A real GUI would use a back buffer and flip, but for our learning
 * OS, direct writes are simpler and work fine.
 */

/* ---------------------------------------------------------------------------
 * Framebuffer state (set once in fb_init)
 * --------------------------------------------------------------------------- */
static uint8_t* fb_addr;         /* Pointer to framebuffer memory */
static uint32_t fb_width;        /* Screen width in pixels */
static uint32_t fb_height;       /* Screen height in pixels */
static uint32_t fb_pitch;        /* Bytes per row (may differ from width*4) */
static uint32_t fb_bpp;          /* Bits per pixel (should be 32) */
static int      fb_active = 0;   /* 1 if framebuffer is initialized */

/* ---------------------------------------------------------------------------
 * fb_init — Parse multiboot framebuffer info and set up video memory
 * --------------------------------------------------------------------------- */
int fb_init(multiboot_info_t* mboot) {
    /* Check if the bootloader provided framebuffer info (bit 12 of flags) */
    if (!(mboot->flags & MULTIBOOT_INFO_FRAMEBUF)) {
        return 0;
    }

    /* Reject non-RGB framebuffers (type 0 = indexed, type 2 = EGA text) */
    if (mboot->framebuffer_type != 1) {
        return 0;
    }

    /* Store framebuffer parameters */
    fb_width  = mboot->framebuffer_width;
    fb_height = mboot->framebuffer_height;
    fb_pitch  = mboot->framebuffer_pitch;
    fb_bpp    = mboot->framebuffer_bpp;

    /* We only support 32-bit color for simplicity */
    if (fb_bpp != 32) {
        return 0;
    }

    /* The framebuffer address is 64-bit in the multiboot struct, but we're
     * a 32-bit kernel so we only use the low 32 bits. For QEMU this is fine
     * — the framebuffer is always below 4 GiB. */
    uint32_t fb_phys = (uint32_t)mboot->framebuffer_addr;

    /* Identity-map the framebuffer region so we can write to it.
     * Total size = pitch * height (pitch accounts for row padding). */
    uint32_t fb_size = fb_pitch * fb_height;
    paging_identity_map_region(fb_phys, fb_size);

    fb_addr = (uint8_t*)fb_phys;
    fb_active = 1;

    return 1;
}

/* ---------------------------------------------------------------------------
 * fb_plot — Set a single pixel
 *
 * Each pixel is 4 bytes in BGRA order at address:
 *   fb_addr + y * pitch + x * 4
 * --------------------------------------------------------------------------- */
void fb_plot(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    uint32_t* pixel = (uint32_t*)(fb_addr + y * fb_pitch + x * 4);
    *pixel = color;
}

/* ---------------------------------------------------------------------------
 * fb_fill_rect — Fill a solid rectangle
 * --------------------------------------------------------------------------- */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = y; row < y + h && row < fb_height; row++) {
        uint32_t* pixel = (uint32_t*)(fb_addr + row * fb_pitch + x * 4);
        for (uint32_t col = 0; col < w && (x + col) < fb_width; col++) {
            pixel[col] = color;
        }
    }
}

/* ---------------------------------------------------------------------------
 * fb_draw_rect — Draw a 1-pixel outline rectangle
 * --------------------------------------------------------------------------- */
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    /* Top and bottom edges */
    for (uint32_t i = x; i < x + w; i++) {
        fb_plot(i, y, color);
        fb_plot(i, y + h - 1, color);
    }
    /* Left and right edges */
    for (uint32_t i = y; i < y + h; i++) {
        fb_plot(x, i, color);
        fb_plot(x + w - 1, i, color);
    }
}

/* ---------------------------------------------------------------------------
 * fb_draw_line — Bresenham's line algorithm
 *
 * Draws a line between any two points using only integer arithmetic.
 * Bresenham's algorithm is the classic method — it works by tracking an
 * error term and deciding at each step whether to move diagonally or
 * straight, keeping the line as close to the ideal as possible.
 *
 * ANALOGY: Imagine walking on a grid of tiles from one corner to another.
 * At each step you can go right, or right-and-up. You keep a running tally
 * of how far off the ideal line you are, and step up whenever the error
 * gets too large. This traces a smooth-looking line using only whole tiles.
 * --------------------------------------------------------------------------- */
void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t sx = (dx > 0) ? 1 : -1;
    int32_t sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int32_t err = dx - dy;

    for (;;) {
        fb_plot((uint32_t)x0, (uint32_t)y0, color);
        if (x0 == x1 && y0 == y1) break;

        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* ---------------------------------------------------------------------------
 * fb_clear — Fill the entire screen
 * --------------------------------------------------------------------------- */
void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, fb_width, fb_height, color);
}

/* ---------------------------------------------------------------------------
 * fb_draw_char — Render one character using the 8x16 bitmap font
 *
 * For each of the 16 rows in the glyph, we check each of the 8 bits.
 * Bit 7 (MSB) = leftmost pixel, bit 0 = rightmost pixel.
 * If the bit is 1, we draw the foreground color; if 0, the background.
 * --------------------------------------------------------------------------- */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    /* Only render printable characters; everything else becomes a space */
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
        c = ' ';
    }

    /* Index into the font data: each character is FONT_HEIGHT bytes */
    const uint8_t* glyph = &font_data[(c - FONT_FIRST_CHAR) * FONT_HEIGHT];

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        uint32_t py = y + row;
        if (py >= fb_height) break;

        /* Fast path: write a whole row of pixels directly */
        uint32_t* pixel = (uint32_t*)(fb_addr + py * fb_pitch + x * 4);
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if (x + col >= fb_width) break;
            /* Check bit (7 - col): MSB is leftmost pixel */
            pixel[col] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

/* ---------------------------------------------------------------------------
 * fb_draw_string — Draw a null-terminated string
 * --------------------------------------------------------------------------- */
void fb_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg) {
    uint32_t cx = x;
    uint32_t cy = y;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += FONT_HEIGHT;
        } else {
            fb_draw_char(cx, cy, *str, fg, bg);
            cx += FONT_WIDTH;
            if (cx + FONT_WIDTH > fb_width) {
                cx = x;
                cy += FONT_HEIGHT;
            }
        }
        str++;
    }
}

/* ---------------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------------- */
uint32_t fb_get_width(void)  { return fb_width; }
uint32_t fb_get_height(void) { return fb_height; }
uint32_t fb_get_pitch(void)  { return fb_pitch; }
int fb_is_active(void)       { return fb_active; }
