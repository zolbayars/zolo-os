; =============================================================================
; gdt.asm — GDT Flush Routine
; =============================================================================
;
; After kernel/gdt.c builds the GDT table, we need to install it in the CPU
; by writing its address and size into the GDTR register (via `lgdt`), then
; reload all segment registers so they point into our new GDT.
;
; The tricky part: CS (code segment) cannot be reloaded with a plain `mov`.
; The only instruction that loads CS is a far jump — `jmp segment:offset`.
; That's not expressible in standard C, so we do it here in assembly.
;
; Called from C as:
;   extern void gdt_flush(uint32_t gdtr_addr);

bits 32
section .text
global gdt_flush

gdt_flush:
    mov eax, [esp+4]    ; argument: pointer to our packed gdtr_t {limit, base}
    lgdt [eax]          ; load the GDT register

    ; Far jump to reload CS. 0x08 = GDT index 1, ring 0 = kernel code segment.
    jmp 0x08:.reload_cs

.reload_cs:
    ; Reload data segment registers. 0x10 = GDT index 2, ring 0 = kernel data.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
