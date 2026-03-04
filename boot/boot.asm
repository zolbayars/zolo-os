; =============================================================================
; boot.asm — Multiboot Header and Kernel Entry Point
; =============================================================================
;
; This is the VERY FIRST code that runs when ZoloOS boots. Not kernel_main —
; this file. The CPU jumps here before any C code has had a chance to run.
;
; ANALOGY: Think of this file as the "handoff zone" in a relay race. The
; bootloader (QEMU's built-in loader) runs the first leg: it powers on the
; hardware, sets up a safe CPU mode, and finds our kernel binary in memory.
; Then it hands the baton to _start (defined below), which does a few small
; setup steps before passing control to our C kernel.
;
; BOOT SEQUENCE — what happens before _start runs:
;   1. QEMU starts and simulates a PC powering on
;   2. The virtual BIOS runs POST (hardware self-check)
;   3. QEMU's built-in bootloader scans the first 8 KiB of our binary,
;      looking for a specific 4-byte magic number (0x1BADB002) — this is
;      the Multiboot standard: a way for bootloaders to recognize OS kernels
;   4. Once found, it loads our binary into RAM at 1 MiB and jumps to _start
;
; WHAT THE BOOTLOADER GUARANTEES when _start begins:
;
;   32-bit protected mode — The CPU has been switched out of the ancient
;     "real mode" (which was limited to 1 MiB of memory) into a modern mode
;     where we can address the full 4 GiB address space. Think of it as
;     switching from a 16-bit app to a 32-bit app.
;
;   A20 line enabled — A hardware quirk from the 1980s: an address line
;     called A20 was historically gated off to maintain compatibility with
;     old software. The bootloader enables it so memory above 1 MiB is
;     accessible. (If it wasn't enabled, addresses would "wrap around".)
;
;   Paging DISABLED — At this point, every memory address the CPU uses is
;     a real physical address. There's no virtual memory yet. This simplifies
;     things early on — what we write is literally what lands in RAM.
;
;   Interrupts DISABLED — The CPU won't respond to keyboard, timer, or any
;     other hardware signals yet. We have to set up our own interrupt table
;     (IDT) before we dare turn interrupts on.
;
;   EAX = 0x2BADB002 — A magic number left by the bootloader confirming it
;     was multiboot-compliant. We check this in kernel_main.
;
;   EBX = multiboot info address — A pointer to a struct describing the
;     machine: how much RAM it has, what disk was booted from, etc.
;
;   NO STACK EXISTS — This is critical. C functions absolutely need a stack
;     to work: local variables live there, return addresses are pushed there.
;     Without one, the first function call would scribble over random memory.
;     Our first job in _start is to carve out a stack and point ESP at it.
;

bits 32         ; Tell NASM we're writing 32-bit instructions

; =============================================================================
; Multiboot Header
; =============================================================================
;
; ANALOGY: This header is like the barcode on a retail product. When a store
; scanner sees it, it immediately knows what the product is. When the bootloader
; scans our binary, it's looking for this exact byte pattern — if found within
; the first 8 KiB, it recognizes our file as a valid OS kernel and boots it.
;
; The Multiboot spec defines exactly three consecutive 32-bit values:
;
;   [magic]    0x1BADB002  — "I'm a multiboot-compatible OS kernel"
;   [flags]    0x00000003  — requests to the bootloader:
;                              bit 0: align any loaded modules to 4 KB boundaries
;                              bit 1: pass us a memory map (how much RAM exists)
;   [checksum]             — must make (magic + flags + checksum) equal zero,
;                            so the bootloader can verify the header isn't corrupted
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
;
; WHAT IS THE STACK? Every function call in C needs scratch space: somewhere
; to store local variables, track where to return when the function is done,
; and pass arguments to other functions. That scratch space is the stack.
;
; ANALOGY: Imagine a physical stack of sticky notes. Every time you call a
; function, you add a note on top ("return to line 42, local var x=5...").
; When the function returns, you tear off the top note and go back to where
; it says. Without any paper, you can't keep track of anything.
;
; ESP (Extended Stack Pointer) is the CPU register that tracks where the top
; of the stack currently is. It's just a number — a memory address. When you
; PUSH something, the CPU subtracts 4 from ESP and writes there. When you POP,
; it reads from ESP and adds 4 back.
;
; The stack grows DOWNWARD in memory (toward lower addresses). So we reserve
; a 16 KiB block, and set ESP to stack_top (the HIGH end). The first push
; lands at stack_top-4, the next at stack_top-8, and so on downward.
;
;   stack_bottom  [lower address]  — the floor; overflowing past here = crash
;   |             ...              — stack grows downward into this space
;   stack_top     [higher address] — ESP starts here, moves down with each PUSH
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
    ; Our kernel_main C signature is:
    ;   void kernel_main(uint32_t magic, uint32_t mboot_addr)
    ;
    ; In the cdecl calling convention (what our C compiler uses), function
    ; arguments are passed by pushing them onto the stack in REVERSE order
    ; (last argument first), then calling the function. The called function
    ; knows to read its first argument at [esp+4], second at [esp+8], etc.
    ;
    ; At this point:
    ;   EAX = 0x2BADB002       (magic, set by the bootloader)
    ;   EBX = multiboot info   (pointer to boot info struct, set by bootloader)
    ;
    ; EAX and EBX are general-purpose CPU registers — buckets that hold
    ; 32-bit values. The bootloader left our two values there by convention.
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
