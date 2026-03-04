#ifndef ISR_H
#define ISR_H

/* =============================================================================
 * isr.h — Interrupt Service Routines (CPU Exception Handlers)
 * =============================================================================
 *
 * WHAT ARE INTERRUPTS?
 *
 * An interrupt is the CPU's way of saying "stop what you're doing — something
 * needs attention right now." There are two kinds:
 *
 *   Hardware interrupts (IRQs) — triggered by physical devices. The keyboard
 *     fires one when a key is pressed. The timer fires one 100 times a second.
 *     These are external events the CPU didn't cause.
 *
 *   CPU exceptions (ISRs) — triggered by the CPU itself when something goes
 *     wrong with the currently running code. Examples:
 *       - Dividing by zero (#0 — the CPU can't do it, so it raises an exception)
 *       - Accessing an unmapped memory address (#14 — page fault)
 *       - Running an unknown instruction (#6 — invalid opcode)
 *
 * ANALOGY: Think of exceptions as the OS's smoke alarms. When something goes
 * wrong (fire = bad instruction, divide by zero, bad memory access), the alarm
 * fires and an exception handler runs to deal with the situation — in our case
 * by printing what went wrong and halting the CPU.
 *
 * THE INTERRUPT FRAME
 *
 * When an interrupt fires, the CPU automatically saves the state of whatever
 * code was running (its registers, flags, instruction pointer) onto the stack,
 * then jumps to our handler. Our assembly stubs (boot/idt.asm) save the rest
 * of the registers and pass a pointer to this saved state as interrupt_frame_t.
 *
 * This lets our C handler inspect exactly what was happening at the moment
 * the exception occurred — like a crash dump.
 */

#include "types.h"

/* ---------------------------------------------------------------------------
 * interrupt_frame_t — snapshot of the CPU state at the moment of an interrupt
 *
 * Layout matches the exact order registers are pushed onto the stack by our
 * assembly stub in boot/idt.asm (lowest address = first field).
 *
 * The CPU pushes EIP/CS/EFLAGS automatically. Our stub pushes the rest.
 * --------------------------------------------------------------------------- */
typedef struct {
    /* Segment registers — pushed last by stub, so they're at the top */
    uint32_t gs, fs, es, ds;

    /* General-purpose registers — pushed by pusha instruction.
     * pusha pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
     * They appear reversed on the stack (EDI at lowest address). */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    /* Pushed by our ISR stub: which interrupt fired, and the error code
     * (for exceptions that produce one; our stub pushes 0 for those that don't) */
    uint32_t int_no, err_code;

    /* Pushed automatically by the CPU when the interrupt fired:
     *   EIP    — address of the instruction that caused the fault
     *   CS     — code segment selector at the time of the fault
     *   EFLAGS — CPU status flags (interrupt enable, overflow, etc.) */
    uint32_t eip, cs, eflags;
} __attribute__((packed)) interrupt_frame_t;

/* Called from common_isr_stub in boot/idt.asm — do not call directly */
void isr_handler(interrupt_frame_t* frame);

#endif /* ISR_H */
