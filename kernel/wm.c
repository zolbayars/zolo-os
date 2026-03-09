#include "wm.h"
#include "fb.h"
#include "font.h"
#include "string.h"

/* =============================================================================
 * wm.c — Window Manager Implementation
 * =============================================================================
 *
 * The window manager maintains an array of windows and renders them to the
 * framebuffer. Each window has:
 *   - A colored titlebar with the window's name
 *   - A border
 *   - A content area with a built-in text console
 *
 * Rendering uses the "painter's algorithm" — we draw the background first,
 * then each window on top. Later windows cover earlier ones. Since our windows
 * don't overlap in this simple setup, the order doesn't matter much.
 *
 * TEXT CONSOLE
 *
 * Each window has a built-in character grid (like a mini VGA text buffer).
 * wm_putchar() writes characters into this grid, handling newlines, cursor
 * advancement, and scrolling. wm_render_all() draws the grid to the
 * framebuffer using fb_draw_char().
 *
 * This means you can call wm_print(win, "Hello!\n") and the text appears
 * in the window — the WM handles font rendering and positioning.
 */

/* Window table */
static window_t windows[WM_MAX_WINDOWS];
static int window_count = 0;

/* ---------------------------------------------------------------------------
 * wm_init — Reset the window table
 * --------------------------------------------------------------------------- */
void wm_init(void) {
    memset(windows, 0, sizeof(windows));
    window_count = 0;
}

/* ---------------------------------------------------------------------------
 * wm_create_window — Allocate and configure a new window
 * --------------------------------------------------------------------------- */
window_t* wm_create_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           const char* title) {
    if (window_count >= WM_MAX_WINDOWS) return NULL;

    window_t* win = &windows[window_count++];
    memset(win, 0, sizeof(window_t));

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->visible = true;
    win->text_fg = FB_LIGHT_GREY;
    win->text_bg = FB_BG_WINDOW;

    strncpy(win->title, title, 31);

    /* Calculate how many text columns and rows fit in the content area.
     * Content area = window size minus borders and titlebar. */
    uint32_t content_w = w - 2 * WM_BORDER;
    uint32_t content_h = h - WM_TITLEBAR_HEIGHT - WM_BORDER;
    win->max_cols = content_w / FONT_WIDTH;
    win->max_rows = content_h / FONT_HEIGHT;

    /* Clamp to our buffer limits */
    if (win->max_cols > WM_TEXT_COLS) win->max_cols = WM_TEXT_COLS;
    if (win->max_rows > WM_TEXT_ROWS) win->max_rows = WM_TEXT_ROWS;

    /* Fill text buffer with spaces */
    memset(win->text, ' ', sizeof(win->text));

    return win;
}

/* ---------------------------------------------------------------------------
 * draw_window — Render a single window (border, titlebar, text content)
 * --------------------------------------------------------------------------- */
static void draw_window(window_t* win) {
    if (!win->visible) return;

    /* Border */
    fb_fill_rect(win->x, win->y, win->w, win->h, FB_BORDER);

    /* Titlebar background */
    fb_fill_rect(win->x + WM_BORDER, win->y + WM_BORDER,
                 win->w - 2 * WM_BORDER, WM_TITLEBAR_HEIGHT - WM_BORDER,
                 FB_TITLEBAR);

    /* Title text (centered vertically in titlebar) */
    uint32_t title_x = win->x + WM_BORDER + 8;
    uint32_t title_y = win->y + WM_BORDER + (WM_TITLEBAR_HEIGHT - WM_BORDER - FONT_HEIGHT) / 2;
    fb_draw_string(title_x, title_y, win->title, FB_WHITE, FB_TITLEBAR);

    /* Content area background */
    uint32_t content_x = win->x + WM_BORDER;
    uint32_t content_y = win->y + WM_TITLEBAR_HEIGHT;
    uint32_t content_w = win->w - 2 * WM_BORDER;
    uint32_t content_h = win->h - WM_TITLEBAR_HEIGHT - WM_BORDER;
    fb_fill_rect(content_x, content_y, content_w, content_h, win->text_bg);

    /* Draw text content from the character grid */
    for (uint32_t row = 0; row < win->max_rows; row++) {
        for (uint32_t col = 0; col < win->max_cols; col++) {
            char c = win->text[row][col];
            if (c < ' ' || c > '~') c = ' ';

            uint32_t px = content_x + col * FONT_WIDTH;
            uint32_t py = content_y + row * FONT_HEIGHT;
            fb_draw_char(px, py, c, win->text_fg, win->text_bg);
        }
    }
}

/* ---------------------------------------------------------------------------
 * wm_render_all — Draw the desktop background and all windows
 * --------------------------------------------------------------------------- */
void wm_render_all(void) {
    /* Desktop background */
    fb_clear(FB_BG_DESKTOP);

    /* Draw each window */
    for (int i = 0; i < window_count; i++) {
        draw_window(&windows[i]);
    }
}

/* ---------------------------------------------------------------------------
 * scroll_up — Scroll the text buffer up by one row
 *
 * Move every row up by one (row 1 → row 0, row 2 → row 1, etc.)
 * and clear the bottom row. This is the same thing that happens when you
 * fill a terminal — old text scrolls off the top.
 * --------------------------------------------------------------------------- */
static void scroll_up(window_t* win) {
    for (uint32_t row = 1; row < win->max_rows; row++) {
        memcpy(win->text[row - 1], win->text[row], win->max_cols);
    }
    memset(win->text[win->max_rows - 1], ' ', win->max_cols);
}

/* ---------------------------------------------------------------------------
 * wm_putchar — Write a character to a window's text console
 * --------------------------------------------------------------------------- */
void wm_putchar(window_t* win, char c) {
    if (c == '\n') {
        win->text_col = 0;
        win->text_row++;
    } else if (c == '\b') {
        if (win->text_col > 0) {
            win->text_col--;
            win->text[win->text_row][win->text_col] = ' ';
        }
    } else if (c == '\r') {
        win->text_col = 0;
    } else if (c >= ' ' && c <= '~') {
        if (win->text_col < win->max_cols) {
            win->text[win->text_row][win->text_col] = c;
            win->text_col++;
        }
        if (win->text_col >= win->max_cols) {
            win->text_col = 0;
            win->text_row++;
        }
    }

    /* Scroll if cursor went past the bottom */
    if (win->text_row >= win->max_rows) {
        scroll_up(win);
        win->text_row = win->max_rows - 1;
    }
}

/* ---------------------------------------------------------------------------
 * wm_print — Write a string to a window's text console
 * --------------------------------------------------------------------------- */
void wm_print(window_t* win, const char* str) {
    while (*str) {
        wm_putchar(win, *str);
        str++;
    }
}

/* ---------------------------------------------------------------------------
 * wm_clear — Clear a window's text buffer and reset cursor
 * --------------------------------------------------------------------------- */
void wm_clear(window_t* win) {
    memset(win->text, ' ', sizeof(win->text));
    win->text_row = 0;
    win->text_col = 0;
}
