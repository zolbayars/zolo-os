#ifndef KPRINTF_H
#define KPRINTF_H

/* =============================================================================
 * kprintf.h — Formatted kernel printf
 * =============================================================================
 *
 * kprintf is the kernel's formatted print function. It writes directly to the
 * VGA text buffer via vga_putchar / vga_print — no buffering, no syscalls.
 *
 * Supported format specifiers:
 *   %s   — null-terminated string (prints "(null)" if pointer is NULL)
 *   %d   — signed 32-bit decimal integer
 *   %u   — unsigned 32-bit decimal integer
 *   %x   — unsigned 32-bit hex (lowercase, no "0x" prefix)
 *   %c   — single character
 *   %%   — literal percent sign
 *
 * Variadic arguments use __builtin_va_start / __builtin_va_arg / __builtin_va_end.
 * These are compiler built-ins that work in freestanding mode — no <stdarg.h> needed.
 */

void kprintf(const char* fmt, ...);

#endif /* KPRINTF_H */
