#ifndef IDT_H
#define IDT_H

/* =============================================================================
 * idt.h — Interrupt Descriptor Table (IDT)
 * =============================================================================
 *
 * WHAT IS THE IDT?
 *
 * The IDT is the CPU's lookup table for interrupt handlers — a list of "if
 * interrupt N fires, jump to this function." It has 256 slots, one for each
 * possible interrupt number (0–255).
 *
 * ANALOGY: The IDT is like an emergency contact list on your phone. Each entry
 * says "if emergency type 14 (page fault) happens, call Dr. Smith at 555-0014."
 * When the CPU gets interrupt 14, it looks up slot 14 in the IDT and calls
 * whatever handler address is stored there.
 *
 * Slots 0–31:  CPU exceptions  (divide-by-zero, page fault, etc.)
 * Slots 32–47: Hardware IRQs   (timer, keyboard, etc. — after PIC remapping)
 * Slots 48–255: available for software interrupts (system calls, etc.)
 *
 * The IDT works together with the GDT: each IDT entry contains a segment
 * selector (pointing into the GDT) plus a handler address. When an interrupt
 * fires, the CPU loads CS from that selector and jumps to the handler address.
 * That's why we need the GDT set up first.
 */

#include "types.h"

/* Initialize the IDT: fill all 256 entries for exceptions and IRQs, load it */
void idt_init(void);

/* Set a single IDT gate entry.
 * @num:     interrupt number (0–255)
 * @handler: address of the assembly stub that handles this interrupt
 * @sel:     GDT segment selector for the handler (0x08 = kernel code)
 * @flags:   gate type and privilege — 0x8E = 32-bit interrupt gate, ring 0 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags);

#endif /* IDT_H */
