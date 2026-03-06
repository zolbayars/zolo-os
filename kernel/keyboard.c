#include "keyboard.h"
#include "irq.h"
#include "io.h"

/* =============================================================================
 * keyboard.c — PS/2 Keyboard Driver Implementation
 * =============================================================================
 *
 * SCAN CODE SET 1 — US QWERTY layout
 *
 * This table maps scan codes (the index) to ASCII characters. A value of 0
 * means the key has no printable ASCII representation (Shift, Ctrl, F-keys,
 * arrows, etc.) — we handle those separately or ignore them for now.
 *
 * The table covers scan codes 0x00–0x57 (standard AT keyboard range).
 * Index = scan code, value = ASCII character (lowercase, no shift).
 */
static const char scancode_table[88] = {
/*00*/  0,    0,   '1', '2', '3', '4', '5', '6',
/*08*/ '7',  '8', '9', '0', '-', '=', '\b', '\t',
/*10*/ 'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',
/*18*/ 'o',  'p', '[', ']', '\n', 0,  'a', 's',
/*20*/ 'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',
/*28*/ '\'', '`', 0,  '\\','z', 'x', 'c', 'v',
/*30*/ 'b',  'n', 'm', ',', '.', '/', 0,   '*',
/*38*/  0,   ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,    0,   0,   0,   0,   0,   0,  '7',
/*48*/ '8',  '9', '-', '4', '5', '6', '+', '1',
/*50*/ '2',  '3', '0', '.'
};

/* Shifted versions of the same scan codes (when Shift is held) */
static const char scancode_table_shift[88] = {
/*00*/  0,    0,   '!', '@', '#', '$', '%', '^',
/*08*/ '&',  '*', '(', ')', '_', '+', '\b', '\t',
/*10*/ 'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',
/*18*/ 'O',  'P', '{', '}', '\n', 0,  'A', 'S',
/*20*/ 'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',
/*28*/ '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',
/*30*/ 'B',  'N', 'M', '<', '>', '?', 0,   '*',
/*38*/  0,   ' ',  0,   0,   0,   0,   0,   0,
/*40*/  0,    0,   0,   0,   0,   0,   0,  '7',
/*48*/ '8',  '9', '-', '4', '5', '6', '+', '1',
/*50*/ '2',  '3', '0', '.'
};

/* Scan codes for modifier keys we track */
#define SC_LEFT_SHIFT  0x2A
#define SC_RIGHT_SHIFT 0x36
#define SC_RELEASE     0x80  /* bit 7 set = key released (break code) */

/* ---------------------------------------------------------------------------
 * Circular input buffer
 *
 * The IRQ handler writes here; keyboard_getchar() reads from here.
 * Using a power-of-two size lets us use bitwise AND instead of modulo
 * for wrapping: (x + 1) & (SIZE - 1) is faster than (x + 1) % SIZE.
 *
 * volatile: both fields are modified in the interrupt handler and read
 * in regular kernel code — must not be cached in a register.
 * --------------------------------------------------------------------------- */
#define BUFFER_SIZE 256

static volatile char     buf[BUFFER_SIZE];
static volatile uint32_t buf_read  = 0;
static volatile uint32_t buf_write = 0;

static volatile int shift_held = 0;

/* ---------------------------------------------------------------------------
 * keyboard_callback — IRQ1 handler, runs on every keypress/release
 * --------------------------------------------------------------------------- */
static void keyboard_callback(interrupt_frame_t* frame) {
    (void)frame;

    uint8_t scancode = inb(0x60);  /* read the scan code from the keyboard */

    /* Check if this is a key release (break code = make code | 0x80) */
    if (scancode & SC_RELEASE) {
        uint8_t released = scancode & ~SC_RELEASE;
        if (released == SC_LEFT_SHIFT || released == SC_RIGHT_SHIFT) {
            shift_held = 0;
        }
        return;  /* nothing to print on key release */
    }

    /* Track shift state */
    if (scancode == SC_LEFT_SHIFT || scancode == SC_RIGHT_SHIFT) {
        shift_held = 1;
        return;
    }

    /* Translate scan code to ASCII */
    if (scancode >= 88) return;  /* out of our table range */

    char c = shift_held ? scancode_table_shift[scancode]
                        : scancode_table[scancode];

    if (c == 0) return;  /* non-printable key (Ctrl, Alt, F-keys, etc.) */

    /* Write into the circular buffer, dropping the character if full */
    uint32_t next_write = (buf_write + 1) & (BUFFER_SIZE - 1);
    if (next_write != buf_read) {  /* buffer not full */
        buf[buf_write] = c;
        buf_write = next_write;
    }
}

/* ---------------------------------------------------------------------------
 * keyboard_init — register the IRQ1 handler
 * --------------------------------------------------------------------------- */
void keyboard_init(void) {
    irq_register_handler(1, keyboard_callback);
}

/* ---------------------------------------------------------------------------
 * keyboard_haschar — returns 1 if a character is waiting in the buffer
 * --------------------------------------------------------------------------- */
int keyboard_haschar(void) {
    return buf_read != buf_write;
}

/* ---------------------------------------------------------------------------
 * keyboard_getchar — blocking read: waits until a key is available
 *
 * hlt pauses the CPU until the next interrupt. Since the timer fires 100
 * times/sec and the keyboard fires on each keypress, hlt won't wait long.
 * This is far more efficient than a busy spin (while (!haschar) {}).
 * --------------------------------------------------------------------------- */
char keyboard_getchar(void) {
    while (!keyboard_haschar()) {
        __asm__ __volatile__ ("hlt");
    }
    char c = buf[buf_read];
    buf_read = (buf_read + 1) & (BUFFER_SIZE - 1);
    return c;
}
