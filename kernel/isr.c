#include "isr.h"
#include "kprintf.h"

/* =============================================================================
 * isr.c — CPU Exception Handler
 * =============================================================================
 *
 * Called by common_isr_stub in boot/idt.asm whenever a CPU exception fires.
 * The interrupt_frame_t pointer gives us a full snapshot of the CPU state at
 * the moment the exception occurred — useful for diagnosing what went wrong.
 *
 * For now: print what happened and halt. Later this could be extended to
 * recover from certain faults (e.g. demand-paging on page fault #14).
 */

/* Human-readable names for the 32 CPU exceptions.
 * Index = interrupt number. Source: Intel 64 and IA-32 Architectures SDM. */
static const char* exception_names[] = {
    "Division By Zero",          /*  0 #DE */
    "Debug",                     /*  1 #DB */
    "Non-Maskable Interrupt",    /*  2     */
    "Breakpoint",                /*  3 #BP */
    "Overflow",                  /*  4 #OF */
    "Bound Range Exceeded",      /*  5 #BR */
    "Invalid Opcode",            /*  6 #UD */
    "Device Not Available",      /*  7 #NM */
    "Double Fault",              /*  8 #DF */
    "Coprocessor Segment Overrun", /* 9    */
    "Invalid TSS",               /* 10 #TS */
    "Segment Not Present",       /* 11 #NP */
    "Stack-Segment Fault",       /* 12 #SS */
    "General Protection Fault",  /* 13 #GP */
    "Page Fault",                /* 14 #PF */
    "Reserved",                  /* 15     */
    "x87 Floating-Point",        /* 16 #MF */
    "Alignment Check",           /* 17 #AC */
    "Machine Check",             /* 18 #MC */
    "SIMD Floating-Point",       /* 19 #XM */
    "Virtualization",            /* 20 #VE */
    "Control Protection",        /* 21 #CP */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"       /* 22-31  */
};

/* ---------------------------------------------------------------------------
 * isr_handler — the C-side exception dispatcher
 *
 * Called from common_isr_stub with a pointer to the saved CPU state.
 * We print a crash report and halt — there's no safe way to continue after
 * most CPU exceptions without a full recovery mechanism.
 * --------------------------------------------------------------------------- */
void isr_handler(interrupt_frame_t* frame) {
    kprintf("\n");
    kprintf("*** KERNEL EXCEPTION ***\n");
    kprintf("  Exception : %d - %s\n",
            frame->int_no,
            frame->int_no < 32 ? exception_names[frame->int_no] : "Unknown");
    kprintf("  Error code: 0x%x\n", frame->err_code);
    kprintf("  EIP       : 0x%x\n", frame->eip);
    kprintf("  CS        : 0x%x\n", frame->cs);
    kprintf("  EFLAGS    : 0x%x\n", frame->eflags);
    kprintf("  EAX=0x%x  EBX=0x%x  ECX=0x%x  EDX=0x%x\n",
            frame->eax, frame->ebx, frame->ecx, frame->edx);
    kprintf("  ESI=0x%x  EDI=0x%x  EBP=0x%x\n",
            frame->esi, frame->edi, frame->ebp);
    kprintf("\nSystem halted.\n");

    /* Disable interrupts and halt. The for loop keeps us here even if
     * a non-maskable interrupt fires after cli. */
    __asm__ __volatile__ ("cli");
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}
