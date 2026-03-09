#ifndef WM_H
#define WM_H

/* =============================================================================
 * wm.h — Window Manager
 * =============================================================================
 *
 * WHAT IS A WINDOW MANAGER?
 *
 * A window manager handles the creation, positioning, and rendering of
 * rectangular windows on a graphical display. Each window has a title bar,
 * a border, and a content area where an application can draw.
 *
 * ANALOGY: The window manager is like a bulletin board coordinator. Different
 * people (tasks) want to pin their notes (windows) on the board (screen).
 * The coordinator decides where each note goes, draws the border and label
 * for each, and makes sure they look neat.
 *
 * DESIGN
 *
 * We keep it simple: windows are stored in a fixed-size array, rendered
 * back-to-front (painter's algorithm). No overlapping window management,
 * no drag-and-drop — just static positioned rectangles with titlebars.
 *
 * Each window has a text console built in: a grid of characters that can
 * be written to with wm_print(). This makes it easy for the shell or
 * system info display to output text into their respective windows.
 */

#include "types.h"

/* Maximum number of windows */
#define WM_MAX_WINDOWS 8

/* Titlebar height in pixels */
#define WM_TITLEBAR_HEIGHT 24

/* Window border thickness */
#define WM_BORDER 2

/* Maximum characters in the window's text console.
 * We store a flat text buffer and track cursor position. */
#define WM_TEXT_COLS 80
#define WM_TEXT_ROWS 40

/* Window struct — everything the WM needs to manage one window */
typedef struct {
    uint32_t x, y;                          /* Top-left corner on screen */
    uint32_t w, h;                          /* Total size (including border + titlebar) */
    char     title[32];                     /* Window title (shown in titlebar) */
    bool     visible;                       /* Whether to render this window */

    /* Built-in text console for the window's content area */
    char     text[WM_TEXT_ROWS][WM_TEXT_COLS]; /* Character grid */
    uint32_t text_row;                      /* Current cursor row */
    uint32_t text_col;                      /* Current cursor column */
    uint32_t text_fg;                       /* Text foreground color */
    uint32_t text_bg;                       /* Text background (window bg) */
    uint32_t max_cols;                      /* Actual columns that fit */
    uint32_t max_rows;                      /* Actual rows that fit */
} window_t;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the window manager — clears the window table. */
void wm_init(void);

/* Create a new window at (x, y) with dimensions w x h and a title.
 * Returns a pointer to the window, or NULL if the table is full. */
window_t* wm_create_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           const char* title);

/* Render all visible windows to the framebuffer.
 * Draws desktop background, then each window (border, titlebar, content). */
void wm_render_all(void);

/* Write a character to a window's text console (handles '\n', '\b', scroll). */
void wm_putchar(window_t* win, char c);

/* Write a string to a window's text console. */
void wm_print(window_t* win, const char* str);

/* Clear a window's text console. */
void wm_clear(window_t* win);

#endif /* WM_H */
