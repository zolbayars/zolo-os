#ifndef IO_H
#define IO_H

/* =============================================================================
 * io.h — x86 Port I/O
 * =============================================================================
 *
 * HOW THE CPU TALKS TO HARDWARE
 *
 * When your app calls printf(), eventually some code down the stack has to
 * physically send bytes to the screen, keyboard, or disk. How? There are two
 * ways a CPU can communicate with hardware devices:
 *
 *   1. Memory-Mapped I/O (MMIO):
 *      The device "pretends" to be a region of RAM at a fixed address.
 *      Writing to that address doesn't go to RAM — it goes to the device.
 *      Example: the VGA text buffer lives at physical address 0xB8000.
 *      We write a character there with a normal memory write, and it appears
 *      on screen. No special instruction needed.
 *
 *   2. Port I/O:
 *      The x86 CPU has a completely separate 16-bit address space just for
 *      hardware devices, distinct from RAM. You can't reach it with normal
 *      memory reads/writes — you need special `in` and `out` CPU instructions.
 *
 *      ANALOGY: think of RAM as the city's street addresses, and ports as a
 *      private internal phone extension system in a building. Same building,
 *      but a completely different numbering scheme accessible only with the
 *      right "phone" (the in/out instructions).
 *
 * Every hardware device that uses port I/O has a fixed port number assigned
 * to it by the PC hardware standard:
 *
 *   Port 0x60        PS/2 keyboard — read the last key pressed
 *   Port 0x20/0xA0   PIC (Programmable Interrupt Controller) — controls which
 *                    hardware events trigger CPU interrupts
 *   Port 0x40–0x43   PIT (Programmable Interval Timer) — the system clock tick
 *   Port 0x3D4/0x3D5 VGA cursor — move the blinking cursor on screen
 *   Port 0x64        PS/2 controller status and commands
 *
 * outb() and inb() are the two primitives that cover everything above.
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
