#ifndef KPRINTF_H
#define KPRINTF_H

/* =============================================================================
 * kprintf.h — Formatted kernel printf
 * =============================================================================
 *
 * WHAT IS THIS?
 *
 * In user-space you have printf(). It formats a string, then asks the OS to
 * write it to a file descriptor, which eventually ends up on screen through
 * layers of buffering, system calls, and terminal emulation.
 *
 * We can't use any of that — we ARE the OS and none of those layers exist yet.
 * kprintf is our equivalent: it formats a string and writes it directly to the
 * VGA text buffer in memory. No buffering, no system calls, zero indirection.
 *
 * ANALOGY: Regular printf sends a letter through the postal system. kprintf
 * walks across the room and tapes the letter directly to the wall.
 *
 * Supported format specifiers (same syntax as printf):
 *   %s   — null-terminated string  (prints "(null)" if pointer is NULL)
 *   %d   — signed decimal integer
 *   %u   — unsigned decimal integer
 *   %x   — unsigned hex, lowercase  (e.g. 255 → "ff", no "0x" prefix)
 *   %c   — single character
 *   %%   — a literal percent sign
 *
 * HOW VARIADIC ARGUMENTS WORK WITHOUT LIBC
 *
 * A variadic function like kprintf(fmt, ...) needs a way to walk through its
 * extra arguments at runtime. Normally you'd use <stdarg.h> (va_list, va_arg).
 * That header requires libc, which we don't have.
 *
 * Instead, we use compiler built-ins: __builtin_va_start, __builtin_va_arg,
 * __builtin_va_end. These are provided directly by Clang/GCC and work in
 * freestanding mode. They compile down to the same machine code as <stdarg.h>
 * — just without needing the library wrapper.
 */

void kprintf(const char* fmt, ...);

#endif /* KPRINTF_H */
