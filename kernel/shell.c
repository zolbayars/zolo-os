#include "shell.h"
#include "vga.h"
#include "kprintf.h"
#include "keyboard.h"
#include "string.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "task.h"
#include "fb.h"
#include "wm.h"
#include "types.h"

/* =============================================================================
 * shell.c — Interactive Kernel Shell
 * =============================================================================
 *
 * The shell is the user-facing interface of the OS. It runs in an infinite
 * loop: print prompt → read line → parse → execute → repeat.
 *
 * COMMAND REGISTRY
 *
 * Each built-in command is a struct with a name, description, and function
 * pointer. To add a new command, just add an entry to the `commands` array
 * and write the handler function. The shell_execute() function does a linear
 * search through the table to find a match.
 *
 * ANALOGY: The command registry is like a restaurant menu. Each item has a
 * name ("help"), a description ("Show available commands"), and a recipe
 * (the handler function that actually does the work).
 */

/* Maximum line length and argument count */
#define MAX_LINE    256
#define MAX_ARGS    16

/* ---------------------------------------------------------------------------
 * Command handler type and registry entry
 * --------------------------------------------------------------------------- */
typedef void (*cmd_handler_t)(int argc, char* argv[]);

typedef struct {
    const char*   name;
    const char*   description;
    cmd_handler_t handler;
} shell_command_t;

/* Forward declarations of built-in command handlers */
static void cmd_help(int argc, char* argv[]);
static void cmd_clear(int argc, char* argv[]);
static void cmd_echo(int argc, char* argv[]);
static void cmd_meminfo(int argc, char* argv[]);
static void cmd_uptime(int argc, char* argv[]);
static void cmd_tasks(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);

/* The command table — add new commands here */
static shell_command_t commands[] = {
    { "help",    "Show available commands",            cmd_help    },
    { "clear",   "Clear the screen",                   cmd_clear   },
    { "echo",    "Print arguments to screen",          cmd_echo    },
    { "meminfo", "Show memory usage (physical + heap)", cmd_meminfo },
    { "uptime",  "Show time since boot",               cmd_uptime  },
    { "tasks",   "List running tasks",                 cmd_tasks   },
    { "reboot",  "Reboot the system",                  cmd_reboot  },
};

/* The shell's window (set by shell_set_window, NULL if in VGA text mode) */
static window_t* shell_window = NULL;

/* ---------------------------------------------------------------------------
 * shell_set_window — Assign a GUI window for the shell to render into.
 * When set, the shell writes to the window's text console instead of VGA.
 * Pass NULL to revert to VGA text mode.
 * --------------------------------------------------------------------------- */
void shell_set_window(void* win) {
    shell_window = (window_t*)win;
}

/* ---------------------------------------------------------------------------
 * Shell output helpers — abstract over VGA text mode vs framebuffer
 *
 * In VGA text mode, we call vga_putchar/vga_print directly.
 * In framebuffer mode, we write to the shell's window and re-render.
 * --------------------------------------------------------------------------- */
static void shell_putchar(char c) {
    if (shell_window) {
        wm_putchar(shell_window, c);
    } else {
        vga_putchar(c);
    }
}

static void shell_print(const char* s) {
    if (shell_window) {
        wm_print(shell_window, s);
    } else {
        vga_print(s);
    }
}

/* Called from kprintf via the putchar hook when shell is active in GUI mode */
static void shell_kprintf_putchar(char c) {
    shell_putchar(c);
}

/* Refresh the screen after output — only needed in GUI mode */
static void shell_refresh(void) {
    if (shell_window) {
        wm_render_all();
    }
}

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* ---------------------------------------------------------------------------
 * shell_read_line — Read a line from the keyboard, handling backspace + enter
 *
 * Characters are echoed to the screen as they're typed. Backspace removes
 * the last character. Enter terminates the line.
 * --------------------------------------------------------------------------- */
static int shell_read_line(char* buf, int max_len) {
    int pos = 0;

    while (pos < max_len - 1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            /* Enter pressed — finish the line */
            shell_putchar('\n');
            shell_refresh();
            buf[pos] = '\0';
            return pos;
        } else if (c == '\b') {
            /* Backspace — erase one character */
            if (pos > 0) {
                pos--;
                shell_putchar('\b');
                shell_refresh();
            }
        } else if (c >= ' ' && c <= '~') {
            /* Printable character — add to buffer and echo */
            buf[pos++] = c;
            shell_putchar(c);
            shell_refresh();
        }
        /* Ignore non-printable characters (arrow keys, etc.) */
    }

    buf[pos] = '\0';
    return pos;
}

/* ---------------------------------------------------------------------------
 * shell_tokenize — Split a line into argc/argv by spaces
 *
 * Works by replacing spaces with null terminators and recording the start
 * of each token. This is the same basic idea as strtok(), but simpler.
 *
 * Example: "echo hello world"
 *   → argv[0]="echo", argv[1]="hello", argv[2]="world", argc=3
 * --------------------------------------------------------------------------- */
static int shell_tokenize(char* line, char* argv[], int max_args) {
    int argc = 0;
    bool in_token = false;

    for (int i = 0; line[i] != '\0' && argc < max_args; i++) {
        if (line[i] == ' ') {
            line[i] = '\0';  /* Terminate the current token */
            in_token = false;
        } else if (!in_token) {
            argv[argc++] = &line[i];  /* Start of a new token */
            in_token = true;
        }
    }

    return argc;
}

/* ---------------------------------------------------------------------------
 * shell_execute — Find and run the command
 * --------------------------------------------------------------------------- */
static void shell_execute(int argc, char* argv[]) {
    if (argc == 0) return;  /* Empty line */

    for (uint32_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    /* Command not found */
    kprintf("Unknown command: '%s'. Type 'help' for a list.\n", argv[0]);
}

/* ---------------------------------------------------------------------------
 * shell_run — Main shell loop (runs as a task)
 * --------------------------------------------------------------------------- */
void shell_run(void) {
    char line[MAX_LINE];
    char* argv[MAX_ARGS];

    /* Ensure interrupts are on even if this task was first scheduled from
     * an interrupt handler (where IF is cleared). Think of it as flipping
     * the house breaker back on so the keyboard "doorbell" can ring. */
    __asm__ __volatile__("sti");

    /* If in GUI mode, redirect kprintf output through our window */
    if (shell_window) {
        kprintf_set_putchar(shell_kprintf_putchar);
    }

    shell_print("ZoloOS Shell\nType 'help' for commands\n\n");
    shell_refresh();

    for (;;) {
        /* Print prompt */
        shell_print("zolo> ");
        shell_refresh();

        /* Read a line */
        shell_read_line(line, MAX_LINE);

        /* Parse and execute */
        int argc = shell_tokenize(line, argv, MAX_ARGS);
        shell_execute(argc, argv);
        shell_refresh();
    }
}

/* =============================================================================
 * Built-in Command Handlers
 * ============================================================================= */

/* help — list all available commands */
static void cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;

    shell_print("Available commands:\n");
    for (uint32_t i = 0; i < NUM_COMMANDS; i++) {
        kprintf("  %-10s %s\n", commands[i].name, commands[i].description);
    }
}

/* clear — wipe the screen */
static void cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    if (shell_window) {
        wm_clear(shell_window);
        wm_render_all();
    } else {
        vga_clear();
    }
}

/* echo — print arguments */
static void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) shell_putchar(' ');
        shell_print(argv[i]);
    }
    shell_putchar('\n');
    shell_refresh();
}

/* meminfo — show physical + heap memory status */
static void cmd_meminfo(int argc, char* argv[]) {
    (void)argc; (void)argv;

    uint32_t total_frames = pmm_get_total_count();
    uint32_t free_frames  = pmm_get_free_count();
    uint32_t used_frames  = total_frames - free_frames;

    shell_print("Physical Memory:\n");
    kprintf("  Total:  %u frames (%u MiB)\n",
            total_frames,
            (uint32_t)((uint64_t)total_frames * FRAME_SIZE / 1024 / 1024));
    kprintf("  Used:   %u frames (%u KiB)\n",
            used_frames,
            (uint32_t)((uint64_t)used_frames * FRAME_SIZE / 1024));
    kprintf("  Free:   %u frames (%u MiB)\n",
            free_frames,
            (uint32_t)((uint64_t)free_frames * FRAME_SIZE / 1024 / 1024));

    shell_print("Kernel Heap:\n");
    kprintf("  Size:   %u KiB\n", heap_get_size() / 1024);
    kprintf("  Used:   %u bytes\n", heap_get_used());
    kprintf("  Free:   %u bytes\n", heap_get_size() - heap_get_used());
    shell_refresh();
}

/* uptime — show seconds since boot */
static void cmd_uptime(int argc, char* argv[]) {
    (void)argc; (void)argv;

    uint32_t seconds = timer_get_uptime();
    uint32_t minutes = seconds / 60;
    uint32_t hours   = minutes / 60;

    kprintf("Up %u:%02u:%02u (%u seconds, %u ticks)\n",
            hours, minutes % 60, seconds % 60,
            seconds, timer_get_ticks());
}

/* tasks — list all tasks and their states */
static void cmd_tasks(int argc, char* argv[]) {
    (void)argc; (void)argv;

    const char* state_names[] = { "READY", "RUNNING", "BLOCKED", "DEAD" };
    task_t* table = task_get_table();

    kprintf("  %-4s %-16s %s\n", "ID", "Name", "State");

    for (int i = 0; i < MAX_TASKS; i++) {
        if (table[i].state == TASK_DEAD && table[i].id == 0 && i != 0) continue;
        if (table[i].name[0] == '\0') continue;  /* unused slot */

        const char* state = (table[i].state <= TASK_DEAD)
                            ? state_names[table[i].state] : "???";
        kprintf("  %-4u %-16s %s\n", table[i].id, table[i].name, state);
    }
    shell_refresh();
}

/* reboot — triple fault to restart the CPU.
 * Loading a zero-length IDT and triggering an interrupt causes a triple fault,
 * which the CPU responds to by resetting — effectively a reboot.
 *
 * ANALOGY: It's like pulling the power cord and plugging it back in. The CPU
 * has no choice but to restart from scratch. */
static void cmd_reboot(int argc, char* argv[]) {
    (void)argc; (void)argv;

    shell_print("Rebooting...\n");
    shell_refresh();

    /* Load a zero-length IDT — any interrupt will now triple-fault */
    uint8_t null_idt[6] = {0};
    __asm__ __volatile__ ("lidt (%0)" : : "r"(null_idt));

    /* Trigger an interrupt — this causes the triple fault */
    __asm__ __volatile__ ("int $0x03");

    /* Should never reach here */
    for (;;) __asm__ __volatile__ ("hlt");
}
