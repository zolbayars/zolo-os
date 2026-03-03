#ifndef IO_H
#define IO_H

/* =============================================================================
 * io.h — x86 Port I/O
 * =============================================================================
 *
 * On x86, hardware devices are controlled through two mechanisms:
 *
 *   1. Memory-mapped I/O (MMIO): The device exposes registers as memory
 *      addresses. You read/write them with normal mov instructions.
 *      Example: VGA video memory at 0xB8000.
 *
 *   2. Port I/O: The device has a separate 16-bit "I/O address space"
 *      (distinct from RAM). You access it with special `in` and `out`
 *      CPU instructions. Example: keyboard controller at port 0x60.
 *
 * This file provides outb() and inb() — the fundamental primitives for
 * port I/O. Nearly every piece of hardware we'll talk to uses them:
 *
 *   Port 0x60        PS/2 keyboard data
 *   Port 0x20/0xA0   PIC (interrupt controller)
 *   Port 0x40-0x43   PIT (timer)
 *   Port 0x3D4/0x3D5 VGA cursor position
 *   Port 0x64        PS/2 controller command/status
 */

#include "types.h"

/* ---------------------------------------------------------------------------
 * outb — Write a byte to an I/O port
 *
 * @port: The 16-bit port address to write to
 * @val:  The 8-bit value to send
 *
 * The `out` x86 instruction has two forms:
 *   out dx, al   — port number in DX register, value in AL register
 *
 * The inline assembly constraints tell the compiler:
 *   "a"(val)  → put `val` into AL (the low byte of the EAX register)
 *   "Nd"(port) → put `port` as an immediate (N) or in DX (d)
 *
 * `volatile` tells the compiler: don't optimize this away or reorder it.
 * Hardware I/O has real side effects — writing to port 0x20 signals the
 * interrupt controller. The compiler must not skip or reorder that.
 * --------------------------------------------------------------------------- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ---------------------------------------------------------------------------
 * inb — Read a byte from an I/O port
 *
 * @port: The 16-bit port address to read from
 * Returns: The 8-bit value the device sent back
 *
 * The `in` instruction reads from the port into AL:
 *   in al, dx
 *
 * The "=a"(ret) constraint tells the compiler:
 *   → the return value comes from AL (output constraint, hence "=")
 * --------------------------------------------------------------------------- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------------------------------------------------------------------------
 * io_wait — Insert a tiny delay after an I/O operation
 *
 * Some old hardware (like the 8259 PIC) needs a brief pause after being
 * written to. Writing to port 0x80 is a harmless operation that takes
 * ~1 microsecond — commonly used as a delay mechanism in x86 OS code.
 * --------------------------------------------------------------------------- */
static inline void io_wait(void) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)0), "Nd"((uint16_t)0x80));
}

#endif /* IO_H */
