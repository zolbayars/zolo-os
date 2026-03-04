#ifndef IRQ_H
#define IRQ_H

/* =============================================================================
 * irq.h — Hardware Interrupt (IRQ) Infrastructure
 * =============================================================================
 *
 * WHAT ARE IRQs?
 *
 * IRQ stands for Interrupt ReQuest. Hardware devices use them to get the CPU's
 * attention. When the keyboard detects a keypress, it fires IRQ1. When the
 * timer ticks, it fires IRQ0. The CPU stops what it's doing, runs our handler,
 * and then returns to whatever it was doing before.
 *
 * ANALOGY: IRQs are like a waiter tapping on your shoulder while you're working.
 * You pause, handle what they need ("your food is ready"), and resume your work.
 * The key thing: you don't poll the kitchen every second asking "is it ready?"
 * — the kitchen tells YOU when it's time.
 *
 * THE 8259 PIC (PROGRAMMABLE INTERRUPT CONTROLLER)
 *
 * On a classic x86 PC, a chip called the 8259 PIC sits between the hardware
 * devices and the CPU. Its job is to collect IRQ signals from up to 15 devices,
 * prioritize them, and forward them one at a time to the CPU as interrupts.
 *
 * There are two PIC chips chained together:
 *   Master PIC — handles IRQ 0–7  (timer, keyboard, COM ports...)
 *   Slave PIC  — handles IRQ 8–15 (RTC, PS/2 mouse, IDE drives...)
 *
 * WHY WE NEED TO REMAP THE PIC
 *
 * By default the BIOS programs the PIC to fire interrupts 0–15 for IRQs 0–15.
 * But the CPU already uses interrupts 0–31 for its own exceptions (divide by
 * zero, page fault, etc.). This collision means a timer tick (IRQ0 → INT 0)
 * looks like a divide-by-zero — chaos.
 *
 * We reprogram the PIC to fire interrupts 32–47 instead:
 *   IRQ  0–7  → INT 32–39  (master PIC, offset 32)
 *   IRQ  8–15 → INT 40–47  (slave PIC,  offset 40)
 *
 * HOW TO USE THIS MODULE
 *
 * 1. Call irq_init() to remap the PIC and initialize the handler table.
 * 2. Call irq_register_handler(irq_number, my_function) for each device.
 *    The function will be called automatically when that IRQ fires.
 *    Registering also unmasks the IRQ so the PIC will forward it to the CPU.
 * 3. Enable interrupts globally with __asm__ __volatile__("sti").
 */

#include "isr.h"  /* for interrupt_frame_t */

/* Function pointer type for IRQ handlers.
 * A handler receives the CPU state at the time the IRQ fired. */
typedef void (*irq_handler_t)(interrupt_frame_t* frame);

/* Initialize the PIC: remap IRQs 0-15 to interrupts 32-47, mask all IRQs */
void irq_init(void);

/* Register a handler for a specific IRQ line (0–15).
 * Automatically unmasks the IRQ so the PIC will forward it to the CPU. */
void irq_register_handler(uint8_t irq, irq_handler_t handler);

/* Mask/unmask individual IRQ lines.
 * Masked = the PIC ignores that device's signals.
 * Unmasked = the PIC forwards the signal to the CPU as an interrupt. */
void irq_enable(uint8_t irq);
void irq_disable(uint8_t irq);

/* Called from common_irq_stub in boot/idt.asm — do not call directly */
void irq_handler(interrupt_frame_t* frame);

#endif /* IRQ_H */
