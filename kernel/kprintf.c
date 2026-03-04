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
 * print_uint — helper that prints val in the given base (10 or 16)
 * --------------------------------------------------------------------------- */
static void print_uint(uint32_t val, uint32_t base) {
    static const char digits[] = "0123456789abcdef";
    char buf[10]; /* 10 digits covers the full uint32 range in base 10 */
    int i = 0;

    if (val == 0) {
        vga_putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = digits[val % base];
        val /= base;
    }

    /* digits accumulated in reverse — print them back to front */
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

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

        switch (*p) {
            case 's': {
                const char* s = __builtin_va_arg(args, const char*);
                vga_print(s ? s : "(null)");
                break;
            }
            case 'd': {
                int32_t val = __builtin_va_arg(args, int32_t);
                if (val < 0) {
                    vga_putchar('-');
                    /* Safe negation via unsigned arithmetic — handles INT_MIN */
                    print_uint((uint32_t)0 - (uint32_t)val, 10);
                } else {
                    print_uint((uint32_t)val, 10);
                }
                break;
            }
            case 'u': {
                print_uint(__builtin_va_arg(args, uint32_t), 10);
                break;
            }
            case 'x': {
                print_uint(__builtin_va_arg(args, uint32_t), 16);
                break;
            }
            case 'c': {
                /* char is promoted to int through default argument promotions */
                vga_putchar((char)__builtin_va_arg(args, int));
                break;
            }
            case '%': {
                vga_putchar('%');
                break;
            }
            default: {
                /* Unknown specifier: echo it literally so nothing is silently lost */
                vga_putchar('%');
                vga_putchar(*p);
                break;
            }
        }
    }

    __builtin_va_end(args);
}
