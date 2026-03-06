#ifndef TIMER_H
#define TIMER_H

/* =============================================================================
 * timer.h — PIT (Programmable Interval Timer) driver
 * =============================================================================
 *
 * WHAT IS THE PIT?
 *
 * The PIT is a hardware chip (Intel 8253/8254) that counts down from a
 * configured value at a fixed frequency of 1,193,180 Hz, and fires IRQ0
 * every time it reaches zero. Think of it as a kitchen timer that resets
 * and rings again automatically, forever.
 *
 * ANALOGY: The PIT is the drummer in the band. Everything else in the OS
 * — scheduling, uptime, timeouts — keeps rhythm by listening to it.
 *
 * WHY 100 Hz?
 *
 * We configure the PIT to fire 100 times per second (every 10ms). This is
 * called the "tick rate" or HZ. Each tick is an opportunity for the OS to:
 *   - Update the uptime counter
 *   - Check if a sleeping process should wake up
 *   - Preempt the running task and switch to another (scheduling)
 *
 * Higher HZ = more responsive but more CPU time spent in the timer handler.
 * 100 Hz is a classic, conservative choice. Linux defaults to 250 or 1000.
 */

#include "types.h"

#define TIMER_HZ 100  /* timer fires this many times per second */

/* Initialize the PIT to fire IRQ0 at TIMER_HZ and register the handler */
void timer_init(void);

/* Returns the number of ticks since timer_init() was called */
uint32_t timer_get_ticks(void);

/* Returns the number of whole seconds since timer_init() was called */
uint32_t timer_get_uptime(void);

#endif /* TIMER_H */
