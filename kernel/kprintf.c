#include "kprintf.h"
#include "vga.h"
#include "types.h"

/* =============================================================================
 * kprintf.c — Formatted kernel printf implementation
 * =============================================================================
 *
 * Design notes:
 *
 * Variadic arguments — the __builtin_va_* family:
 *   In a hosted environment you'd use <stdarg.h>. In freestanding mode, clang
 *   exposes the same mechanism as built-in compiler intrinsics:
 *     __builtin_va_list   — opaque type that tracks the argument position
 *     __builtin_va_start  — initializes the list, points it past the last
 *                           named parameter
 *     __builtin_va_arg    — reads the next argument with the given type and
 *                           advances the position
 *     __builtin_va_end    — clean-up (required by the ABI on some platforms)
 *
 * Integer formatting — print_uint:
 *   Converting an integer to decimal or hex is done by extracting digits from
 *   the least-significant end (val % base) and storing them in a small buffer,
 *   then printing the buffer in reverse. The buffer needs at most 10 chars for
 *   base-10 (max uint32 = 4,294,967,295) or 8 chars for base-16.
 *
 * Signed integers (%d):
 *   We print a '-' sign and then convert to unsigned using two's complement
 *   arithmetic: `(uint32_t)0 - (uint32_t)val`. This handles INT_MIN correctly
 *   because it stays in well-defined unsigned arithmetic — no signed overflow.
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
