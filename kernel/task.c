#include "task.h"
#include "string.h"
#include "kprintf.h"

/* =============================================================================
 * task.c — Round-Robin Preemptive Multitasking
 * =============================================================================
 *
 * HOW ROUND-ROBIN SCHEDULING WORKS
 *
 * We have an array of tasks and a "current" index. Every time the timer fires,
 * we move to the next READY task in the array, wrapping around like a circular
 * queue. Every task gets an equal share of CPU time.
 *
 * ANALOGY: It's like a merry-go-round at a fair. Each horse (task) gets the
 * same amount of time in the spotlight (running). When the music pauses (timer
 * tick), the ride rotates to the next horse. Nobody gets to stay on longer
 * than anyone else.
 *
 * FAKE INITIAL STACK FRAME
 *
 * When we create a new task, it hasn't run yet — there's no "saved state" to
 * restore. So we fake one. We set up the task's stack to look exactly like
 * context_switch() had just saved registers and was about to return to the
 * task's entry point.
 *
 * The fake stack frame looks like:
 *
 *   [top of stack — higher addresses]
 *     task_exit   ← return address for when entry_point() returns
 *     entry_point ← context_switch's `ret` will jump here
 *     0 (EBP)     ← fake saved callee-saved registers
 *     0 (EBX)
 *     0 (ESI)
 *     0 (EDI)
 *   [esp points here — lower addresses]
 *
 * When context_switch restores this task for the first time, it pops the four
 * zeros (restoring "empty" registers), then `ret` pops entry_point and jumps
 * there. The task starts running! When entry_point() returns, it returns to
 * task_exit(), which cleans up the task.
 */

/* The context_switch function defined in boot/switch.asm */
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);

/* Task table — fixed array of task slots */
static task_t tasks[MAX_TASKS];

/* Index of the currently running task */
static int current_task = -1;

/* Number of tasks that have been created (used to assign IDs) */
static int task_count = 0;

/* Whether the scheduler is active (set after task_init completes) */
static bool scheduler_active = false;

/* Tick counter for preemptive scheduling — switch every N ticks.
 * At 100 Hz, 10 ticks = 100ms per time slice. */
#define SCHEDULE_INTERVAL 10
static uint32_t schedule_ticks = 0;

/* ---------------------------------------------------------------------------
 * task_init — Register the current execution as task 0
 *
 * The kernel has been running since boot with its own stack (set up in
 * boot.asm). We register this existing execution as task 0 so the scheduler
 * can switch away from it and back to it like any other task.
 *
 * We don't need to set up a fake stack frame — task 0 is already running.
 * Its ESP will be saved naturally the first time context_switch is called.
 * --------------------------------------------------------------------------- */
void task_init(void) {
    /* Zero out all task slots */
    memset(tasks, 0, sizeof(tasks));

    /* Register the current execution as task 0 */
    tasks[0].id = 0;
    tasks[0].state = TASK_RUNNING;
    strncpy(tasks[0].name, "kernel", 31);
    /* ESP will be saved by context_switch when we first switch away */

    current_task = 0;
    task_count = 1;
    scheduler_active = true;

    kprintf("Tasks: scheduler active (round-robin, %d ms slice)\n",
            SCHEDULE_INTERVAL * 10);
}

/* ---------------------------------------------------------------------------
 * task_create — Create a new task with a fake initial stack frame
 * --------------------------------------------------------------------------- */
int task_create(const char* name, void (*entry_point)(void)) {
    /* Find an empty slot */
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD && i != 0) {
            slot = i;
            break;
        }
    }

    /* If no dead slots, find one that was never used (state=0 which is TASK_READY
     * but id=0 and no stack set up). Use task_count to find the next fresh slot. */
    if (slot == -1) {
        if (task_count >= MAX_TASKS) {
            kprintf("task_create: no free task slots!\n");
            return -1;
        }
        slot = task_count;
    }

    task_t* t = &tasks[slot];
    memset(t, 0, sizeof(task_t));
    t->id = slot;
    t->state = TASK_READY;
    strncpy(t->name, name, 31);

    /* --- Build the fake stack frame ---
     *
     * The stack grows downward, so "top" of the stack array = highest address.
     * We place our fake values at the top and set ESP to point below them.
     *
     *   stack[TASK_STACK_SIZE - 4]  = task_exit (entry_point's return address)
     *   stack[TASK_STACK_SIZE - 8]  = entry_point (context_switch's `ret` target)
     *   stack[TASK_STACK_SIZE - 12] = 0 (fake EBP)
     *   stack[TASK_STACK_SIZE - 16] = 0 (fake EBX)
     *   stack[TASK_STACK_SIZE - 20] = 0 (fake ESI)
     *   stack[TASK_STACK_SIZE - 24] = 0 (fake EDI)
     *   ESP = &stack[TASK_STACK_SIZE - 24]
     */
    uint32_t* stack_top = (uint32_t*)((uint32_t)t->stack + TASK_STACK_SIZE);

    /* Return address for when entry_point() returns — go to task_exit */
    *(--stack_top) = (uint32_t)task_exit;

    /* context_switch's `ret` will pop this and jump to entry_point */
    *(--stack_top) = (uint32_t)entry_point;

    /* Fake callee-saved registers (EBP, EBX, ESI, EDI — all zero) */
    *(--stack_top) = 0;  /* EBP */
    *(--stack_top) = 0;  /* EBX */
    *(--stack_top) = 0;  /* ESI */
    *(--stack_top) = 0;  /* EDI */

    t->esp = (uint32_t)stack_top;

    if (slot >= task_count) {
        task_count = slot + 1;
    }

    return slot;
}

/* ---------------------------------------------------------------------------
 * find_next_task — Find the next READY task in round-robin order
 *
 * Scans from (current + 1) wrapping around. If nothing else is ready,
 * returns the current task (it keeps running).
 * --------------------------------------------------------------------------- */
static int find_next_task(void) {
    for (int i = 1; i <= task_count; i++) {
        int idx = (current_task + i) % task_count;
        if (tasks[idx].state == TASK_READY || tasks[idx].state == TASK_RUNNING) {
            return idx;
        }
    }
    /* Shouldn't happen — task 0 (kernel) is always alive */
    return current_task;
}

/* ---------------------------------------------------------------------------
 * switch_to — Perform the actual context switch from current to `next`
 * --------------------------------------------------------------------------- */
static void switch_to(int next) {
    if (next == current_task) return;  /* Already running this task */

    int prev = current_task;

    /* Mark states */
    if (tasks[prev].state == TASK_RUNNING) {
        tasks[prev].state = TASK_READY;
    }
    tasks[next].state = TASK_RUNNING;
    current_task = next;

    /* The actual register save/restore + stack switch happens in assembly.
     * When this returns, we're running in the context of `next`. */
    context_switch(&tasks[prev].esp, tasks[next].esp);
}

/* ---------------------------------------------------------------------------
 * task_yield — Voluntarily give up the CPU
 * --------------------------------------------------------------------------- */
void task_yield(void) {
    if (!scheduler_active) return;
    int next = find_next_task();
    switch_to(next);
}

/* ---------------------------------------------------------------------------
 * task_schedule — Called from the timer IRQ handler for preemptive switching
 *
 * We don't switch every tick — that would waste too much time on context
 * switches. Instead, we switch every SCHEDULE_INTERVAL ticks (e.g. every
 * 100ms at 100 Hz).
 * --------------------------------------------------------------------------- */
void task_schedule(void) {
    if (!scheduler_active) return;

    schedule_ticks++;
    if (schedule_ticks < SCHEDULE_INTERVAL) return;
    schedule_ticks = 0;

    int next = find_next_task();
    switch_to(next);
}

/* ---------------------------------------------------------------------------
 * task_exit — Clean up when a task's entry_point() function returns
 * --------------------------------------------------------------------------- */
void task_exit(void) {
    tasks[current_task].state = TASK_DEAD;
    kprintf("[task %u '%s' exited]\n", tasks[current_task].id, tasks[current_task].name);

    /* We can't just return — there's nowhere to return TO (the task is done).
     * Yield to let another task run. Since our state is DEAD, the scheduler
     * will never pick us again. */
    task_yield();

    /* Safety net — if yield somehow returns (e.g. we're the last task),
     * just spin. This shouldn't happen because task 0 (kernel) never exits. */
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}

/* ---------------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------------- */
uint32_t task_get_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) {
            count++;
        }
    }
    return count;
}

task_t* task_get_table(void) {
    return tasks;
}

task_t* task_get_current(void) {
    if (current_task < 0) return NULL;
    return &tasks[current_task];
}
