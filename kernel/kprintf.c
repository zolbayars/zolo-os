#include "kprintf.h"
#include "vga.h"
#include "types.h"

/* =============================================================================
 * kprintf.c — Formatted kernel printf implementation
 * =============================================================================
 *
 * HOW VARIADIC ARGUMENTS WORK
 *
 * When you call kprintf("x=%d y=%d", 10, 20), the compiler pushes 10 and 20
 * onto the stack after the format string. The __builtin_va_* family gives us
 * a way to walk through those extra arguments one by one at runtime:
 *
 *   __builtin_va_list   — a "cursor" that tracks which argument we're up to
 *   __builtin_va_start  — places the cursor just after the last named argument
 *   __builtin_va_arg    — reads the next argument (you tell it the type) and
 *                         advances the cursor
 *   __builtin_va_end    — marks the cursor as done (required for correctness
 *                         on some CPU architectures where cleanup is needed)
 *
 * ANALOGY: Think of the extra arguments as a row of numbered envelopes.
 * va_start hands you an envelope opener pointing at envelope #1. va_arg
 * opens the current envelope, reads it, and moves to the next one.
 *
 * HOW INTEGER-TO-STRING CONVERSION WORKS (print_uint)
 *
 * You can't print an integer directly — you need to convert it to ASCII digits.
 * The trick: repeatedly divide by the base (10 for decimal, 16 for hex) and
 * collect remainders. The remainders come out in reverse order (least significant
 * digit first), so we store them in a small buffer and print the buffer backward.
 *
 * Example: converting 123 to "123"
 *   123 % 10 = 3  → buf[0]
 *    12 % 10 = 2  → buf[1]
 *     1 % 10 = 1  → buf[2]
 *   Print buf in reverse: "123" ✓
 *
 * SIGNED INTEGERS (%d) AND INT_MIN SAFETY
 *
 * For negative numbers we print '-' then negate the value. But negating
 * INT_MIN (-2,147,483,648) in signed arithmetic overflows — there's no +2B
 * that fits in 32 bits. We avoid this by working in unsigned arithmetic:
 *   (uint32_t)0 - (uint32_t)val
 * Unsigned subtraction wraps around by the C standard, giving the correct
 * absolute value even for INT_MIN.
 */

/* ---------------------------------------------------------------------------
 * uint_to_str — convert an unsigned integer to a string in the given base.
 * Returns the number of characters written to buf (not including terminator).
 * --------------------------------------------------------------------------- */
static int uint_to_str(uint32_t val, uint32_t base, char* buf, int buf_size) {
    static const char digits[] = "0123456789abcdef";
    char tmp[12]; /* longest uint32 in base 10 is 10 digits + null */
    int i = 0;

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0 && i < 11) {
            tmp[i++] = digits[val % base];
            val /= base;
        }
    }

    /* Reverse into output buffer */
    int len = 0;
    while (i > 0 && len < buf_size - 1) {
        buf[len++] = tmp[--i];
    }
    buf[len] = '\0';
    return len;
}

/* ---------------------------------------------------------------------------
 * pad_and_print — print a string with optional width and alignment.
 * If width > len, pads with pad_char. If left_align, padding goes on the right.
 * --------------------------------------------------------------------------- */
static void pad_and_print(const char* s, int len, int width, bool left_align, char pad_char) {
    if (!left_align) {
        for (int i = len; i < width; i++) vga_putchar(pad_char);
    }
    for (int i = 0; i < len; i++) vga_putchar(s[i]);
    if (left_align) {
        for (int i = len; i < width; i++) vga_putchar(' ');
    }
}

#include "string.h"

/* ---------------------------------------------------------------------------
 * kprintf — formatted kernel print
 * --------------------------------------------------------------------------- */
void kprintf(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            vga_putchar(*p);
            continue;
        }

        p++; /* consume '%' */

        /* --- Parse optional flags, width, and zero-fill ---
         * Supports: %-10s (left align, width 10), %02u (zero-pad, width 2), etc.
         */
        bool left_align = false;
        char pad_char = ' ';
        int width = 0;

        /* Flag: '-' = left-align */
        if (*p == '-') {
            left_align = true;
            p++;
        }

        /* Flag: '0' = zero-pad (only for numbers, ignored with '-') */
        if (*p == '0' && !left_align) {
            pad_char = '0';
            p++;
        }

        /* Width: one or more digits */
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p) {
            case 's': {
                const char* s = __builtin_va_arg(args, const char*);
                if (!s) s = "(null)";
                int len = (int)strlen(s);
                pad_and_print(s, len, width, left_align, ' ');
                break;
            }
            case 'd': {
                int32_t val = __builtin_va_arg(args, int32_t);
                char buf[12];
                int len;
                if (val < 0) {
                    vga_putchar('-');
                    len = uint_to_str((uint32_t)0 - (uint32_t)val, 10, buf, sizeof(buf));
                } else {
                    len = uint_to_str((uint32_t)val, 10, buf, sizeof(buf));
                }
                pad_and_print(buf, len, (val < 0 && width > 0) ? width - 1 : width,
                              left_align, pad_char);
                break;
            }
            case 'u': {
                char buf[12];
                int len = uint_to_str(__builtin_va_arg(args, uint32_t), 10, buf, sizeof(buf));
                pad_and_print(buf, len, width, left_align, pad_char);
                break;
            }
            case 'x': {
                char buf[12];
                int len = uint_to_str(__builtin_va_arg(args, uint32_t), 16, buf, sizeof(buf));
                pad_and_print(buf, len, width, left_align, pad_char);
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                vga_putchar(c);
                break;
            }
            case '%': {
                vga_putchar('%');
                break;
            }
            default: {
                vga_putchar('%');
                vga_putchar(*p);
                break;
            }
        }
    }

    __builtin_va_end(args);
}
