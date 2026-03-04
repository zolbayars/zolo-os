#ifndef GDT_H
#define GDT_H

/* =============================================================================
 * gdt.h — Global Descriptor Table (GDT)
 * =============================================================================
 *
 * WHAT IS THE GDT?
 *
 * In user-space programming, when you write `int x = 42`, the CPU just reads
 * and writes memory freely. But at the hardware level, every memory access
 * goes through a layer of checking: the CPU asks "is this code allowed to
 * touch that memory?"
 *
 * The GDT is the table that answers that question. It's a small array of
 * entries — called segment descriptors — where each entry is like a security
 * badge that says: "code using this badge can access memory from address X
 * to address Y, and it has privilege level Z."
 *
 * ANALOGY: think of a hotel keycard system. The front desk programs keycards
 * (GDT entries). When a guest swipes their keycard (the CPU checks the segment
 * register), the door controller looks up what that card is allowed to open.
 * A master keycard (ring 0, kernel) opens every door. A guest keycard (ring 3,
 * user process) only opens specific rooms.
 *
 * WHY DO WE NEED TO SET IT UP OURSELVES?
 *
 * When QEMU boots our kernel, it already sets up a minimal GDT and flips the
 * CPU into 32-bit protected mode (which requires a GDT). But that GDT lives
 * somewhere in low memory that we don't control — once we start managing
 * memory ourselves, we could accidentally overwrite it, which would make
 * the CPU crash immediately.
 *
 * We install our own GDT in kernel memory so we own it forever.
 *
 * OUR GDT HAS 3 ENTRIES:
 *
 *   Entry 0 — Null descriptor
 *     The x86 spec requires the first entry to be completely zeroed out.
 *     It's like a dummy row at the top of the table that nothing ever uses.
 *     If code accidentally uses selector 0x00, the CPU faults immediately —
 *     which is actually useful for catching bugs.
 *
 *   Entry 1 — Kernel Code segment (selector 0x08)
 *     "Code fetched via CS can execute anywhere in the 4 GB address space,
 *      and it runs at privilege level 0 (full hardware access)."
 *
 *   Entry 2 — Kernel Data segment (selector 0x10)
 *     "Data accessed via DS/SS/ES can be read and written anywhere in the
 *      4 GB address space, at privilege level 0."
 *
 * This is a "flat" memory model — both segments span the entire 4 GB with no
 * restrictions. The GDT here isn't about limiting memory access; it's about
 * telling the CPU "we're running kernel code with full privileges." Memory
 * protection comes later via paging.
 */

void gdt_init(void);

#endif /* GDT_H */
