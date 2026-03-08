; =============================================================================
; switch.asm — Context Switch (Task Switching)
; =============================================================================
;
; WHAT IS A CONTEXT SWITCH?
;
; When the OS switches from task A to task B, it needs to:
;   1. Save everything task A was doing (its register values)
;   2. Restore everything task B was doing when it was last paused
;
; The "context" is just the set of CPU registers — they hold everything the
; CPU needs to continue executing: the instruction pointer (where am I?),
; the stack pointer (where are my local variables?), and the general-purpose
; registers (what values was I working with?).
;
; ANALOGY: Imagine you're reading two different books. When you switch from
; book A to book B, you place a bookmark in A (save context), then open B to
; your bookmark (restore context). The bookmarks are the saved registers.
;
; HOW THIS WORKS
;
; C signature: void context_switch(uint32_t* old_esp, uint32_t new_esp);
;
; Parameters (cdecl calling convention):
;   [esp+4]  = old_esp — pointer to where we should save task A's stack pointer
;   [esp+8]  = new_esp — task B's stack pointer (where B's saved registers are)
;
; The callee-saved registers in the cdecl convention are: EBP, EBX, ESI, EDI.
; The caller expects these to be unchanged after a function call. So when we
; switch stacks, we need to save/restore exactly these four.
;
; EAX, ECX, EDX are caller-saved — the compiler already saves them before
; calling us, so we don't need to touch them.
;
; The trick: when task B was previously switched away from, it was sitting
; right here in this same function. Its return address is on its stack. So
; when we `ret` after restoring B's stack, we return INTO task B's code,
; right where it left off. Magic.
;

bits 32
section .text
global context_switch

context_switch:
    ; --- Save task A's callee-saved registers ---
    ; Push them onto A's stack so they're preserved when we come back to A later.
    push ebp
    push ebx
    push esi
    push edi

    ; --- Save task A's stack pointer ---
    ; old_esp is a pointer (uint32_t*). We store the current ESP value there
    ; so the scheduler knows where to find A's saved state next time.
    mov eax, [esp + 20]     ; eax = old_esp (parameter 1, offset by 4 pushes + ret addr)
    mov [eax], esp          ; *old_esp = current ESP

    ; --- Switch to task B's stack ---
    ; new_esp is the value we saved last time B was running. It points to
    ; B's stack, which has B's saved EDI/ESI/EBX/EBP and return address.
    mov esp, [esp + 24]     ; esp = new_esp (parameter 2)

    ; --- Restore task B's callee-saved registers ---
    ; These were pushed by this same function when B was last switched away from.
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; --- Return into task B ---
    ; The `ret` instruction pops the return address from B's stack and jumps there.
    ; For a task that was previously running, this returns to wherever B called
    ; context_switch from (or task_yield, or the scheduler).
    ; For a BRAND NEW task, we set up a fake stack frame in task_create() with
    ; the entry point as the return address, so `ret` jumps to the task's function.
    ret
