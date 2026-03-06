#include "idt.h"
#include "types.h"

/* =============================================================================
 * idt.c — Interrupt Descriptor Table implementation
 * =============================================================================
 *
 * See idt.h for a full explanation of what the IDT is.
 *
 * STRUCTURE OF AN IDT ENTRY (8 bytes)
 *
 * Each entry tells the CPU: "when interrupt N fires, jump to this address,
 * using this code segment, with these permissions."
 *
 *  Bytes 0-1:  handler address bits 0-15  (low half)
 *  Bytes 2-3:  segment selector           (0x08 = kernel code segment in our GDT)
 *  Byte  4:    reserved, always 0
 *  Byte  5:    type and attributes        (0x8E = interrupt gate, ring 0, present)
 *  Bytes 6-7:  handler address bits 16-31 (high half)
 *
 * TYPE BYTE 0x8E breakdown:
 *   Bit 7:    Present = 1       (this entry is active)
 *   Bits 6-5: DPL = 00          (ring 0 — only kernel can trigger this via INT)
 *   Bit 4:    Storage = 0       (interrupt/trap gate, not a task gate)
 *   Bits 3-0: Type = 1110       (0xE = 32-bit interrupt gate)
 *
 * The difference between an interrupt gate and a trap gate:
 *   Interrupt gate (0x8E): CPU clears IF (disables further interrupts) on entry.
 *     Used for hardware IRQs — we don't want a timer IRQ firing while we're
 *     handling a timer IRQ.
 *   Trap gate (0x8F): CPU leaves IF alone.
 *     Used for exceptions where re-entrancy is acceptable.
 * We use interrupt gates for everything for simplicity.
 */

typedef struct {
    uint16_t base_low;   /* handler address bits 0-15                     */
    uint16_t selector;   /* GDT selector: 0x08 = kernel code segment      */
    uint8_t  reserved;   /* always 0                                       */
    uint8_t  flags;      /* type + attributes: 0x8E = 32-bit int gate, r0 */
    uint16_t base_high;  /* handler address bits 16-31                    */
} __attribute__((packed)) idt_entry_t;

/* The IDTR — 6 bytes loaded into the CPU's IDT register via lidt.
 * Same concept as the GDTR for the GDT. */
typedef struct {
    uint16_t limit;  /* size of the IDT in bytes, minus 1                 */
    uint32_t base;   /* physical address of the IDT array                 */
} __attribute__((packed)) idtr_t;

static idt_entry_t idt[256];
static idtr_t      idtr;

/* Declare all 32 ISR stubs from boot/idt.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* Declare all 16 IRQ stubs from boot/idt.asm */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* ---------------------------------------------------------------------------
 * idt_set_gate — Write one entry into the IDT
 * --------------------------------------------------------------------------- */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].reserved  = 0;
    idt[num].flags     = flags;
}

/* ---------------------------------------------------------------------------
 * idt_init — Fill the IDT and load it into the CPU
 * --------------------------------------------------------------------------- */
void idt_init(void) {
    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint32_t)&idt;

    /* Register the 32 CPU exception handlers (ISRs 0-31).
     * 0x08 = kernel code segment selector, 0x8E = interrupt gate ring 0. */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* Register the 16 hardware IRQ handlers (IRQs 0-15 → IDT slots 32-47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    /* Load the IDT into the CPU. After this instruction, any interrupt that
     * fires will be dispatched through our handlers above. */
    __asm__ __volatile__ ("lidt %0" : : "m"(idtr));
}
