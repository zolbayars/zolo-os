; =============================================================================
; boot.asm — Multiboot Header and Kernel Entry Point
; =============================================================================
;
; This is the VERY FIRST code that executes when ZoloOS boots.
;
; Boot sequence (what happens before _start runs):
;   1. QEMU starts and simulates power-on
;   2. The virtual BIOS initializes hardware
;   3. QEMU's built-in bootloader scans the first 8 KiB of our binary
;      looking for the multiboot magic number (0x1BADB002)
;   4. Once found, it loads the binary into memory at 1 MiB (as our
;      linker script specifies) and jumps to _start
;
; What the bootloader guarantees when _start runs:
;   - CPU is in 32-bit protected mode  (we can address 4 GiB of RAM)
;   - A20 line is enabled              (memory above 1 MiB is accessible)
;   - Paging is DISABLED               (virtual addr == physical addr for now)
;   - Interrupts are DISABLED          (no keyboard/timer until WE set them up)
;   - EAX = 0x2BADB002                 (magic value confirming multiboot boot)
;   - EBX = pointer to multiboot info  (has memory map, boot device info, etc.)
;   - No stack exists yet              (we MUST create one before calling C)
;

bits 32         ; Tell NASM we're writing 32-bit instructions

; =============================================================================
; Multiboot Header
; =============================================================================
; The bootloader finds our OS by scanning the first 8 KiB of the binary for
; this exact sequence of bytes. The linker script puts this section first.
;
; The header is three 4-byte (32-bit) values placed back to back:
;
;   [magic]    0x1BADB002  — the "knock knock, I'm an OS" signature
;   [flags]    0x00000003  — requests: (bit 0) align modules to 4K pages,
;                                       (bit 1) give us a memory map
;   [checksum] ???         — must make (magic + flags + checksum) == 0
;                            This is just a sanity check for the bootloader
;
section .multiboot
align 4

MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x03
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

dd MULTIBOOT_MAGIC      ; dd = "define doubleword" = write a 32-bit value
dd MULTIBOOT_FLAGS
dd MULTIBOOT_CHECKSUM

; =============================================================================
; Stack
; =============================================================================
; x86 C code needs a stack. Local variables, function arguments, and return
; addresses all live on the stack. Without one, the very first C function call
; would corrupt random memory.
;
; We carve out 16 KiB in the .bss section (uninitialized data). The bootloader
; zeroes .bss for us before jumping to _start. The stack grows DOWNWARD, so
; we'll set ESP to stack_top (the highest address).
;
;   stack_bottom  [lower address]
;   |             <-- stack grows downward
;   |             <-- each PUSH decrements ESP then writes
;   |             <-- each POP reads then increments ESP
;   stack_top     [higher address]  <-- ESP starts here
;
section .bss
align 16                ; 16-byte alignment (required for SSE instructions)

stack_bottom:
    resb 16384          ; Reserve 16384 bytes = 16 KiB
stack_top:              ; This label = address just past the reserved area

; =============================================================================
; Entry Point
; =============================================================================
section .text
global _start           ; Export _start so the linker can find it (it's in linker.ld ENTRY(_start))

_start:
    ; ------------------------------------------------------------------
    ; 1. Point the stack pointer at the TOP of our reserved stack area
    ; ------------------------------------------------------------------
    ; ESP (Extended Stack Pointer) is the register x86 uses for the stack.
    ; We set it to stack_top because the stack grows downward — the first
    ; PUSH instruction will decrement ESP by 4, then write to that address.
    ;
    mov esp, stack_top

    ; ------------------------------------------------------------------
    ; 2. Pass multiboot info to kernel_main as C function arguments
    ; ------------------------------------------------------------------
    ; Our kernel_main C signature will be:
    ;   void kernel_main(uint32_t magic, uint32_t mboot_addr)
    ;
    ; The C calling convention (cdecl) passes arguments on the stack,
    ; pushed right-to-left (last argument first). So we push EBX (the
    ; multiboot info address = 2nd arg) first, then EAX (magic = 1st arg).
    ;
    ; When kernel_main does:  uint32_t magic = first_arg;
    ; the compiler reads it from [esp+4] — which is where EAX landed.
    ;
    push ebx            ; 2nd argument: multiboot info struct address
    push eax            ; 1st argument: magic number (should be 0x2BADB002)

    ; ------------------------------------------------------------------
    ; 3. Call the C kernel
    ; ------------------------------------------------------------------
    extern kernel_main  ; Tell NASM this symbol is defined in another file
    call kernel_main    ; Push return address, jump to kernel_main

    ; ------------------------------------------------------------------
    ; 4. Hang if kernel_main ever returns (it shouldn't)
    ; ------------------------------------------------------------------
    ; In a real OS, kernel_main never returns — it runs forever.
    ; But if something goes wrong and it does return, we should NOT
    ; just fall off into random memory. cli+hlt is the safe landing pad.
    ;
    cli                 ; Disable interrupts (so nothing can wake us)
.hang:
    hlt                 ; Halt the CPU until the next interrupt
    jmp .hang           ; If a non-maskable interrupt fires, halt again
