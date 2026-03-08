#ifndef SHELL_H
#define SHELL_H

/* =============================================================================
 * shell.h — Interactive Kernel Shell
 * =============================================================================
 *
 * WHAT IS THIS?
 *
 * A basic command-line interface running inside the kernel. It reads lines
 * from the keyboard, parses them into commands and arguments, and dispatches
 * to registered command handlers. Think of it as a tiny bash that lives in
 * the kernel instead of userspace.
 *
 * ANALOGY: The shell is the kernel's receptionist. You walk up (type a
 * command), the receptionist looks up the right department (finds the handler),
 * and connects you. "help" → here's what we can do. "clear" → wipes the board.
 * "meminfo" → asks the accounting department how memory is doing.
 *
 * DESIGN
 *
 * Commands are registered in a static table as {name, description, handler}.
 * The shell tokenizes input by spaces into argc/argv (just like C's main()),
 * then finds the matching handler by name and calls it.
 *
 * The shell runs as its own task (via task_create), so it's preemptively
 * scheduled alongside any other tasks.
 */

/* Start the shell — called as a task entry point (via task_create).
 * This function never returns (it loops reading and executing commands). */
void shell_run(void);

#endif /* SHELL_H */
