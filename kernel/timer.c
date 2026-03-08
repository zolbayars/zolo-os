#include "timer.h"
#include "irq.h"
#include "io.h"
#include "task.h"

/* =============================================================================
 * timer.c — PIT driver implementation
 * =============================================================================
 *
 * HOW THE PIT WORKS
 *
 * The PIT has an internal counter that decrements at 1,193,180 Hz. We load
 * a "divisor" value into it — the counter resets to that value every time
 * it hits zero and fires IRQ0. So:
 *
 *   divisor = PIT_BASE_FREQ / desired_hz
 *           = 1,193,180 / 100
 *           = 11,931  (≈ 10ms per tick)
 *
 * PIT I/O PORTS
 *
 *   0x40  Channel 0 data — where we write the divisor (low byte then high byte)
 *   0x43  Mode/Command  — where we send the configuration command first
 *
 * COMMAND BYTE 0x36 = 0011 0110:
 *   Bits 7-6: 00 = select channel 0
 *   Bits 5-4: 11 = access mode lobyte/hibyte (send low byte first, then high)
 *   Bits 3-1: 011 = mode 3 (square wave generator — counter reloads automatically)
 *   Bit  0:   0 = binary counting (not BCD)
 */

#define PIT_BASE_FREQ  1193180  /* PIT oscillator frequency in Hz */
#define PIT_CHANNEL0   0x40     /* channel 0 data port             */
#define PIT_CMD        0x43     /* mode/command register port      */
#define PIT_CMD_VALUE  0x36     /* channel 0, lobyte/hibyte, mode 3, binary */

/* Tick counter — volatile because it's written in an interrupt handler
 * and read from regular kernel code. Without volatile, the compiler might
 * cache the value in a register and never see the interrupt's update.
 *
 * ANALOGY: Like a shared whiteboard in an office. If you photocopy it once
 * and keep reading your copy, you'll miss updates others write on the board.
 * volatile tells the compiler "always read from the board, not your copy." */
static volatile uint32_t tick_count = 0;

/* ---------------------------------------------------------------------------
 * timer_callback — called by irq_handler on every IRQ0 (timer tick)
 * --------------------------------------------------------------------------- */
static void timer_callback(interrupt_frame_t* frame) {
    (void)frame;
    tick_count++;

    /* Let the scheduler check if it's time to switch tasks.
     * This is what makes multitasking preemptive — tasks get interrupted
     * even if they didn't voluntarily yield. */
    task_schedule();
}

/* ---------------------------------------------------------------------------
 * timer_init — configure the PIT and register the IRQ0 handler
 * --------------------------------------------------------------------------- */
void timer_init(void) {
    uint32_t divisor = PIT_BASE_FREQ / TIMER_HZ;

    /* Send the command byte first to tell the PIT what we're about to configure */
    outb(PIT_CMD, PIT_CMD_VALUE);

    /* Send the divisor as two bytes: low byte first, then high byte.
     * The PIT requires this specific order (lobyte/hibyte access mode). */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register our callback — this also unmasks IRQ0 so the PIC starts
     * forwarding timer ticks to the CPU */
    irq_register_handler(0, timer_callback);
}

/* ---------------------------------------------------------------------------
 * timer_get_ticks — raw tick count since init
 * --------------------------------------------------------------------------- */
uint32_t timer_get_ticks(void) {
    return tick_count;
}

/* ---------------------------------------------------------------------------
 * timer_get_uptime — whole seconds elapsed since init
 * --------------------------------------------------------------------------- */
uint32_t timer_get_uptime(void) {
    return tick_count / TIMER_HZ;
}
