/*
 * context.h
 *
 * Copyright (C) 2000 David J. Hudson <dave@humbug.demon.co.uk>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "copying-gpl.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "copying-liquorice.txt" for details.
 */

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/context.h"
#elif defined(I386)
#include "i386/context.h"
#else
#error "no valid architecture found"
#endif

/*
 * Are we using locks or not?
 */
#define LOCK 1

/*
 * Spinlock data type.
 */
struct lock {
	fast_u8_t l_lock;
	priority_t l_pri_locked;
	priority_t l_pri_unlocked;
};

/*
 * Prototypes
 */
extern void isr_spinlock_lock(struct lock *l);
extern void spinlock_lock(struct lock *l);
extern void isr_spinlock_unlock(struct lock *l);
extern void spinlock_unlock(struct lock *l);

/*
 * spinlock_init()
 *	Initialize a spinlock.
 */
extern inline void spinlock_init(struct lock *l, u8_t priority)
{
	if (LOCK) {
		l->l_lock = 0;
	}

	l->l_pri_locked = priority;
}

/*
 * Operating context information.
 */
struct context {
	struct cpu_context c_cpu;	/* CPU-specific context information - e.g. saved registers */
	priority_t c_priority;		/* Operating priority */
	u8_t c_state;			/* Operating state - e.g. running, ready, sleeping, etc */
	struct context *c_run_queue;	/* Next context on the system run queue after us */
	struct context *c_sleep_queue;	/* Next context on a queue of sleeping contexts */
	void *c_stack_memory;		/* Base address of this context's stack - used to detect blown stacks */
	u16_t c_context_switches;	/* Number of times that this context been switched out */
	u8_t c_spinlocks_held;		/* Number of spinlocks currently held by this context */
};

/*
 * Thread operating states.
 */
#define CONTEXT_NULL 0
#define CONTEXT_RUNNING 1
#define CONTEXT_READY 2
#define CONTEXT_INTERRUPTED 3
#define CONTEXT_SLEEP 4
#define CONTEXT_SLEEP_QUEUE 5

/*
 * Globals
 */
extern struct context *current_context;
extern struct context *volatile run_queue;
extern struct lock run_queue_lock;
extern u8_t run_queue_len;

/*
 * Function declarations
 */
extern void isr_context_ready(struct context *c);
extern u8_t isr_context_switch(void);
extern void context_exit(void) __attribute__ ((noreturn));
extern void isr_context_yield(void);
extern void context_yield(void);
extern u8_t isr_context_wait(struct lock *l);
extern u8_t context_wait(struct lock *l);
extern u8_t context_wait_queue(struct lock *l, struct context **queue);
extern u8_t context_wait_pri_queue(struct lock *l, struct context **queue);
extern void isr_context_signal(struct context *c);
extern void context_signal(struct context *c);
extern u8_t context_signal_queue(struct context **queue);
extern u8_t context_broadcast_queue(struct context **queue);
extern void context_interrupt(struct context *c);
extern u8_t context_interrupt_queue(struct context *c, struct context **queue);
extern void context_init(struct context *c, void (*fn)(void *), void *arg, void *stack_memory, addr_t stack_addr, priority_t pri);

/*
 * isr_context_get_run_queue_len()
 *	Get the number of contexts on the run queue.
 */
extern inline u8_t isr_context_get_run_queue_len(void)
{
	return run_queue_len;
}

extern void debug_stack_trace(void);
extern void debug_assert_isr(fast_u8_t disabled);
extern void debug_check_stack(addr_t guardband);
