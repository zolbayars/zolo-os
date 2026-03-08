#ifndef TASK_H
#define TASK_H

/* =============================================================================
 * task.h — Cooperative/Preemptive Multitasking
 * =============================================================================
 *
 * WHAT IS MULTITASKING?
 *
 * A single CPU can only run one piece of code at a time. Multitasking creates
 * the illusion that multiple programs are running simultaneously by rapidly
 * switching between them — like a chef who stirs pot A, then chops for dish B,
 * then checks on dish C, cycling so fast that all three seem to cook at once.
 *
 * Each "program" (or task/thread) gets its own stack and saved register state.
 * When the OS wants to switch from task A to task B, it:
 *   1. Saves A's current registers (where it was, what it was doing)
 *   2. Restores B's saved registers (where B left off last time)
 *   3. Jumps to B's instruction pointer
 *
 * Task A is now "frozen" and B resumes exactly where it paused. This save/
 * restore is called a "context switch."
 *
 * PREEMPTIVE vs COOPERATIVE
 *
 * Cooperative: tasks voluntarily yield the CPU ("I'm done for now, you go").
 * Preemptive: the timer interrupt forcibly switches tasks, even if the running
 *             task didn't ask to stop. This prevents one task from hogging the
 *             CPU forever.
 *
 * We implement preemptive scheduling: the PIT timer fires IRQ0 at 100 Hz,
 * and every N ticks our scheduler picks the next task to run.
 *
 * ANALOGY: Cooperative multitasking is like a polite roundtable discussion
 * where each person voluntarily passes the microphone. Preemptive multitasking
 * is like a moderator with a buzzer who cuts you off after your time is up,
 * no matter what you're saying.
 *
 * TASK CONTROL BLOCK (TCB)
 *
 * Each task has a Task Control Block — a struct that holds everything the
 * scheduler needs to manage it:
 *   - Saved stack pointer (ESP): where the task's frozen state lives
 *   - Task ID and name: for identification
 *   - State: RUNNING, READY, BLOCKED, or DEAD
 *   - Stack: each task gets its own 4 KiB stack (separate from the kernel stack)
 */

#include "types.h"

/* Maximum number of concurrent tasks */
#define MAX_TASKS 16

/* Size of each task's stack in bytes */
#define TASK_STACK_SIZE 4096

/* Task states — like traffic light colors for tasks */
typedef enum {
    TASK_READY,     /* Waiting in line, ready to run (green light queued) */
    TASK_RUNNING,   /* Currently executing on the CPU (green light active) */
    TASK_BLOCKED,   /* Waiting for something (I/O, sleep) — can't run yet */
    TASK_DEAD       /* Finished execution, slot can be reclaimed */
} task_state_t;

/* Task Control Block — everything we need to manage one task */
typedef struct {
    uint32_t      esp;                    /* Saved stack pointer (where to resume) */
    uint32_t      id;                     /* Unique task identifier */
    task_state_t  state;                  /* Current state (READY/RUNNING/etc.) */
    char          name[32];               /* Human-readable name for debugging */
    uint8_t       stack[TASK_STACK_SIZE]; /* The task's private stack */
} task_t;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/* Initialize the task system. Registers the current execution context
 * (kernel_main) as task 0 — the "idle" / "kernel" task. Must be called
 * after heap_init() and before creating any other tasks. */
void task_init(void);

/* Create a new task that will execute the given function.
 * @name: human-readable name (e.g. "shell", "idle")
 * @entry_point: function pointer — the task starts executing here
 * Returns the task ID, or -1 if the task table is full. */
int task_create(const char* name, void (*entry_point)(void));

/* Voluntarily give up the CPU and let another task run.
 * This is the cooperative path — tasks can call this when they're waiting. */
void task_yield(void);

/* Called from the timer interrupt to perform preemptive scheduling.
 * Should be called from timer_callback, NOT from regular kernel code. */
void task_schedule(void);

/* Terminate the currently running task. If a task's entry_point function
 * returns, this is called automatically to clean up. */
void task_exit(void);

/* Get the number of currently alive tasks (READY + RUNNING). */
uint32_t task_get_count(void);

/* Get the task table (for commands like `tasks` to display). */
task_t* task_get_table(void);

/* Get the currently running task. */
task_t* task_get_current(void);

#endif /* TASK_H */
