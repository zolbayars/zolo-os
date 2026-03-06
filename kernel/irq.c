#include "irq.h"
#include "io.h"
#include "kprintf.h"

/* =============================================================================
 * irq.c — PIC remapping and hardware IRQ dispatch
 * =============================================================================
 *
 * See irq.h for a full explanation of IRQs and why we remap the PIC.
 *
 * PIC I/O PORTS
 *
 * The 8259 PIC has two chips (master + slave), each with two ports:
 *   Master: command = 0x20, data = 0x21
 *   Slave:  command = 0xA0, data = 0xA1
 *
 * INITIALIZATION COMMAND WORDS (ICWs)
 *
 * Reprogramming the PIC is done by sending a sequence of "initialization
 * command words" in a specific order. It's a 1970s-era protocol — rigid
 * and unforgiving. Miss a step or send them out of order and the PIC
 * behaves unpredictably.
 *
 *   ICW1 (to command port): start initialization, tell PIC more ICWs follow
 *   ICW2 (to data port):    set the interrupt vector offset (where in the
 *                           IDT to map IRQ 0 — we use 32 for master, 40 slave)
 *   ICW3 (to data port):    tell master which IRQ line the slave is on (IRQ2),
 *                           tell slave its own cascade identity
 *   ICW4 (to data port):    set 8086 mode (as opposed to older 8080 mode)
 */

#define PIC1_CMD  0x20   /* master PIC command port */
#define PIC1_DATA 0x21   /* master PIC data port    */
#define PIC2_CMD  0xA0   /* slave  PIC command port */
#define PIC2_DATA 0xA1   /* slave  PIC data port    */

#define PIC_EOI   0x20   /* End-Of-Interrupt command — must be sent after
                          * every IRQ handler or the PIC stops firing that
                          * IRQ line. Like acknowledging a doorbell — if you
                          * don't acknowledge it, it won't ring again. */

#define ICW1_INIT 0x11   /* initialization + ICW4 required */
#define ICW4_8086 0x01   /* 8086 mode (vs older 8080 mode) */

#define PIC1_OFFSET 32   /* master IRQs 0-7  → IDT slots 32-39 */
#define PIC2_OFFSET 40   /* slave  IRQs 8-15 → IDT slots 40-47 */

/* Dispatch table: one function pointer per IRQ line (0-15).
 * NULL means no handler registered — the IRQ is acknowledged but ignored. */
static irq_handler_t irq_handlers[16] = { 0 };

/* ---------------------------------------------------------------------------
 * pic_remap — Reprogram both PICs to use our chosen interrupt offsets
 *
 * After this runs, IRQ0 fires interrupt 32 (not 8), so it no longer collides
 * with the CPU's "divide by zero" exception at interrupt 0.
 * --------------------------------------------------------------------------- */
static void pic_remap(void) {
    /* Save the current IRQ masks so we can restore them after remapping.
     * Masks control which IRQ lines the PIC forwards to the CPU. */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: start initialization sequence */
    outb(PIC1_CMD, ICW1_INIT);  io_wait();
    outb(PIC2_CMD, ICW1_INIT);  io_wait();

    /* ICW2: set vector offsets */
    outb(PIC1_DATA, PIC1_OFFSET); io_wait();  /* master: IRQ0 → INT 32 */
    outb(PIC2_DATA, PIC2_OFFSET); io_wait();  /* slave:  IRQ8 → INT 40 */

    /* ICW3: cascade wiring between master and slave */
    outb(PIC1_DATA, 0x04); io_wait(); /* master: slave is on IRQ2 (bit 2 = 0x04) */
    outb(PIC2_DATA, 0x02); io_wait(); /* slave: its cascade identity is 2         */

    /* ICW4: set 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* ---------------------------------------------------------------------------
 * irq_init — Remap the PIC and mask all IRQ lines
 *
 * We mask everything here. Individual drivers unmask their own IRQ line
 * when they register a handler (via irq_register_handler). This way no
 * IRQ fires before we have a handler ready for it.
 * --------------------------------------------------------------------------- */
void irq_init(void) {
    pic_remap();

    /* Mask all IRQ lines on both PICs (0xFF = all bits set = all masked) */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ---------------------------------------------------------------------------
 * irq_register_handler — Register a C function to handle a specific IRQ
 *
 * Also unmasks the IRQ so the PIC will start forwarding it to the CPU.
 * --------------------------------------------------------------------------- */
void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
    irq_enable(irq);
}

/* ---------------------------------------------------------------------------
 * irq_enable / irq_disable — Mask and unmask individual IRQ lines
 *
 * The PIC mask registers work with individual bits: 0 = enabled, 1 = masked.
 * IRQs 0-7  are controlled by the master PIC mask at port 0x21.
 * IRQs 8-15 are controlled by the slave  PIC mask at port 0xA1.
 *
 * ANALOGY: Like a notification setting on your phone — you can silence
 * individual apps (IRQ lines) without turning off the phone entirely.
 * --------------------------------------------------------------------------- */
void irq_enable(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) & (uint8_t)~(1 << bit));  /* clear the mask bit */
}

void irq_disable(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (irq < 8) ? irq : (irq - 8);
    outb(port, inb(port) | (uint8_t)(1 << bit));   /* set the mask bit   */
}

/* ---------------------------------------------------------------------------
 * irq_handler — C-side IRQ dispatcher, called from common_irq_stub
 *
 * Looks up the registered handler for the IRQ that fired, calls it if one
 * exists, then sends End-Of-Interrupt (EOI) to the PIC.
 *
 * The EOI must ALWAYS be sent — even if we have no handler. Without it,
 * the PIC assumes we're still processing the interrupt and will never fire
 * that IRQ line again. Like replying "got it" to a message even if you
 * don't plan to act on it, so the sender knows they can send more.
 * --------------------------------------------------------------------------- */
void irq_handler(interrupt_frame_t* frame) {
    /* The IRQ number is stored at int_no - PIC1_OFFSET
     * (e.g. int_no=32 → IRQ0, int_no=33 → IRQ1) */
    uint8_t irq = (uint8_t)(frame->int_no - PIC1_OFFSET);

    /* Call the registered handler if one exists */
    if (irq_handlers[irq]) {
        irq_handlers[irq](frame);
    }

    /* Send EOI to slave PIC first (if the IRQ came from the slave, IRQ 8-15) */
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }

    /* Always send EOI to the master PIC */
    outb(PIC1_CMD, PIC_EOI);
}
