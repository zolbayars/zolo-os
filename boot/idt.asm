; =============================================================================
; idt.asm — Interrupt Service Routine and IRQ Assembly Stubs
; =============================================================================
;
; WHAT THIS FILE DOES
;
; When an interrupt fires, the CPU jumps to a handler address stored in the
; IDT. But it doesn't jump straight to C code — the CPU only saves a few
; registers automatically, and C functions expect ALL registers to be in a
; known state. We need a thin assembly wrapper ("stub") that:
;
;   1. Saves the CPU state completely (all registers)
;   2. Calls our C handler with a pointer to that saved state
;   3. Restores the CPU state completely
;   4. Returns from the interrupt (iret, not a normal ret)
;
; ANALOGY: Imagine a surgeon being interrupted mid-operation by a fire alarm.
; Before leaving, they must: note exactly what they were doing, hand off the
; patient safely, handle the alarm, then come back and pick up exactly where
; they left off. The stub is that careful hand-off procedure.
;
; WHY TWO KINDS OF STUBS?
;
; Some CPU exceptions automatically push an "error code" onto the stack before
; jumping to the handler. Others don't. Our C handler (isr_handler) expects a
; consistent stack layout, so for exceptions that DON'T push an error code, we
; push a dummy zero first. This makes the stack layout identical in both cases.
;
; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21
; Everything else: we push a dummy 0.
;
; MACRO APPROACH
;
; Instead of writing 48 near-identical stubs by hand, we define two macros:
;   ISR_NOERRCODE %1  — for exceptions that don't push an error code
;   ISR_ERRCODE   %1  — for exceptions that do push an error code
;   IRQ %1, %2        — for hardware IRQs (maps IRQ line to interrupt number)
;

bits 32

; =============================================================================
; Macros
; =============================================================================

; ISR stub for CPU exceptions that do NOT push an error code automatically.
; We push 0 as a placeholder so the stack layout matches ISR_ERRCODE.
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0        ; dummy error code (so all ISRs look the same to C)
    push dword %1       ; interrupt number
    jmp  common_isr_stub
%endmacro

; ISR stub for CPU exceptions that DO push an error code automatically.
; The CPU already pushed the error code, so we just push the interrupt number.
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1       ; interrupt number (error code already on stack from CPU)
    jmp  common_isr_stub
%endmacro

; IRQ stub for hardware interrupts (IRQ 0-15).
; @irq_line:     the IRQ number (0-15)
; @int_number:   the IDT slot (32-47 after PIC remapping)
%macro IRQ 2
global irq%1
irq%1:
    push dword 0        ; no error code for hardware IRQs
    push dword %2       ; interrupt number (e.g. IRQ0 → 32)
    jmp  common_irq_stub
%endmacro

; =============================================================================
; CPU Exception Stubs (ISRs 0-31)
; =============================================================================
;
; These are defined by the x86 spec. They fire when the CPU encounters a
; problem with the currently executing code.
;
; See: https://wiki.osdev.org/Exceptions
;
ISR_NOERRCODE 0   ; #DE  Divide-by-zero (int x = 1/0)
ISR_NOERRCODE 1   ; #DB  Debug exception
ISR_NOERRCODE 2   ;      Non-maskable interrupt (hardware failure signal)
ISR_NOERRCODE 3   ; #BP  Breakpoint (INT3 instruction, used by debuggers)
ISR_NOERRCODE 4   ; #OF  Overflow (INTO instruction)
ISR_NOERRCODE 5   ; #BR  Bound range exceeded
ISR_NOERRCODE 6   ; #UD  Invalid opcode (unknown instruction)
ISR_NOERRCODE 7   ; #NM  Device not available (FPU instruction without FPU)
ISR_ERRCODE   8   ; #DF  Double fault (exception while handling an exception)
ISR_NOERRCODE 9   ;      Coprocessor segment overrun (legacy, never fires now)
ISR_ERRCODE   10  ; #TS  Invalid TSS
ISR_ERRCODE   11  ; #NP  Segment not present
ISR_ERRCODE   12  ; #SS  Stack-segment fault
ISR_ERRCODE   13  ; #GP  General protection fault (most common: bad memory access)
ISR_ERRCODE   14  ; #PF  Page fault (accessing unmapped virtual memory)
ISR_NOERRCODE 15  ;      Reserved
ISR_NOERRCODE 16  ; #MF  x87 floating-point exception
ISR_ERRCODE   17  ; #AC  Alignment check
ISR_NOERRCODE 18  ; #MC  Machine check (hardware error)
ISR_NOERRCODE 19  ; #XM  SIMD floating-point exception
ISR_NOERRCODE 20  ; #VE  Virtualization exception
ISR_ERRCODE   21  ; #CP  Control protection exception
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Reserved
ISR_NOERRCODE 29  ; Reserved
ISR_NOERRCODE 30  ; Reserved
ISR_NOERRCODE 31  ; Reserved

; =============================================================================
; Hardware IRQ Stubs (IRQs 0-15, mapped to INT 32-47 after PIC remap)
; =============================================================================
;
; These fire when physical hardware devices signal the CPU.
;
IRQ 0,  32  ; PIT timer  (fires 100 times/sec after we configure it)
IRQ 1,  33  ; PS/2 keyboard
IRQ 2,  34  ; Cascade (used internally by PIC to chain master→slave)
IRQ 3,  35  ; COM2 serial port
IRQ 4,  36  ; COM1 serial port
IRQ 5,  37  ; LPT2 parallel port
IRQ 6,  38  ; Floppy disk controller
IRQ 7,  39  ; LPT1 / spurious interrupt
IRQ 8,  40  ; RTC (real-time clock)
IRQ 9,  41  ; ACPI / legacy SCSI
IRQ 10, 42  ; Available / legacy SCSI
IRQ 11, 43  ; Available
IRQ 12, 44  ; PS/2 mouse
IRQ 13, 45  ; FPU / coprocessor
IRQ 14, 46  ; Primary ATA (IDE) disk
IRQ 15, 47  ; Secondary ATA (IDE) disk

; =============================================================================
; Common ISR Stub — called by all ISR_NOERRCODE and ISR_ERRCODE stubs above
; =============================================================================
;
; At this point the stack looks like (top = lowest address):
;
;   [esp+0]  interrupt number    (pushed by our stub)
;   [esp+4]  error code          (pushed by CPU or dummy 0 by our stub)
;   [esp+8]  EIP                 (pushed automatically by CPU on interrupt)
;   [esp+12] CS                  (pushed automatically by CPU)
;   [esp+16] EFLAGS              (pushed automatically by CPU)
;
; We push the remaining registers and segment registers, then call isr_handler
; with a pointer to the whole lot as interrupt_frame_t*.
;
section .text

extern isr_handler      ; defined in kernel/isr.c

common_isr_stub:
    pusha               ; push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    ; Push segment registers so the handler can inspect them
    mov  ax, ds
    push eax
    mov  ax, es
    push eax
    mov  ax, fs
    push eax
    mov  ax, gs
    push eax

    ; Load kernel data segment into all data segment registers.
    ; The interrupt may have fired while user-mode code was running (later),
    ; so DS might not point to the kernel's data segment.
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Call isr_handler(interrupt_frame_t* frame)
    ; ESP now points to the bottom of the interrupt_frame_t struct on the stack.
    push esp
    call isr_handler
    add  esp, 4         ; clean up the pushed argument (cdecl caller cleanup)

    ; Restore segment registers
    pop  eax
    mov  gs, ax
    pop  eax
    mov  fs, ax
    pop  eax
    mov  es, ax
    pop  eax
    mov  ds, ax

    popa                ; restore EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    add  esp, 8         ; discard the interrupt number and error code we pushed
    iret                ; return from interrupt: pops EIP, CS, EFLAGS (and
                        ; SS+ESP if privilege level changed — not yet relevant)

; =============================================================================
; Common IRQ Stub — called by all IRQ stubs above
; =============================================================================
;
; Identical to common_isr_stub but calls irq_handler instead of isr_handler.
; The separation keeps exception handling and device I/O cleanly distinct.
;
extern irq_handler      ; defined in kernel/irq.c

common_irq_stub:
    pusha

    mov  ax, ds
    push eax
    mov  ax, es
    push eax
    mov  ax, fs
    push eax
    mov  ax, gs
    push eax

    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp
    call irq_handler
    add  esp, 4

    pop  eax
    mov  gs, ax
    pop  eax
    mov  fs, ax
    pop  eax
    mov  es, ax
    pop  eax
    mov  ds, ax

    popa
    add  esp, 8
    iret
