#ifndef KEYBOARD_H
#define KEYBOARD_H

/* =============================================================================
 * keyboard.h — PS/2 Keyboard Driver
 * =============================================================================
 *
 * WHAT IS A SCAN CODE?
 *
 * When you press a key, the keyboard doesn't send the letter 'A' — it sends
 * a number called a scan code that identifies which physical key moved.
 * There's no ASCII here yet; that translation is our job.
 *
 * Every key generates two scan codes:
 *   Make code  — sent when the key is PRESSED   (e.g. 0x1E for 'A')
 *   Break code — sent when the key is RELEASED  (make code | 0x80, e.g. 0x9E)
 *
 * We only act on make codes (key down). Break codes let us track modifier
 * keys like Shift — we set a flag when Shift is pressed, clear it on release.
 *
 * ANALOGY: Scan codes are like raw button IDs on a game controller. The game
 * (our keyboard driver) decides what "button 30" means — move left, jump,
 * or type the letter 'A' — depending on context (is Shift held?).
 *
 * HOW WE READ IT
 *
 * The PS/2 keyboard controller sits at I/O port 0x60. When IRQ1 fires,
 * we do inb(0x60) to read the scan code the keyboard placed there.
 *
 * INPUT BUFFER
 *
 * The IRQ handler writes characters into a circular buffer. keyboard_getchar()
 * reads from that buffer. This decouples interrupt timing from when the kernel
 * decides to read input — like a queue between producer and consumer.
 */

#include "types.h"

/* Initialize the keyboard driver and register the IRQ1 handler */
void keyboard_init(void);

/* Read one character from the input buffer.
 * Blocks (busy-waits with hlt) until a key is available. */
char keyboard_getchar(void);

/* Returns 1 if there is at least one character waiting in the buffer */
int keyboard_haschar(void);

#endif /* KEYBOARD_H */
