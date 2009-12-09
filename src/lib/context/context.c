/*
 * context.c
 *	Thread (and scheduling) support routines.
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
#include "types.h"
#include "cpu.h"
#include "memory.h"
#include "isr.h"
#include "debug.h"
#include "trap.h"
#include "context.h"

#if defined(ATMEGA103)
#include "avr/context_switch.h"
#include "avr/atmega103/context_enter.h"
#elif defined(AT90S8515)
#include "avr/context_switch.h"
#include "avr/at90s8515/context_enter.h"
#elif defined(I386)
#include "i386/context_switch.h"
#include "i386/context_enter.h"
#else
#error "no valid architecture found"
#endif

/*
 * Global declarations associated with scheduling contexts.
 *
 * Note that the volatile declaration of run_queue is very important and it
 * definitely should *NOT* be:
 *
 * 	volatile struct context *run_queue
 */
struct context *current_context = NULL;
struct context *volatile run_queue = NULL;
struct lock run_queue_lock = {0x00, 0x00, 0xff};
u8_t run_queue_len = 0;
struct context *context_list = NULL;

/*
 * isr_context_ready()
 */
void isr_context_ready(struct context *c)
{
	struct context *q, **qprev;

	if (DEBUG) {
		debug_assert_isr(TRUE);
	}
		
	q = (struct context *)run_queue;
	qprev = (struct context **)&run_queue;

	while (q && (q->c_priority <= c->c_priority)) {
		qprev = &q->c_run_queue;
		q = q->c_run_queue;
	}
		
	c->c_run_queue = q;
	*qprev = c;
}

/*
 * context_exit()
 *	Cause the current context to cease executing.
 *
 * We remove ourself from the run queue, clear down some of the context table
 * details and then jump into the middle of the task switching routine.
 * This switches our current state out without trying to save it first.
 */
void context_exit(void)
{
	isr_disable();
	debug_set_lights(0x11);
	
	run_queue = current_context->c_run_queue;
        run_queue_len--;
		
	current_context->c_state = CONTEXT_NULL;

	jump_startup();

	while (1);
}

/*
 * isr_context_yield()
 *	Release the current timeslice to another context that may be ready to run.
 */
void isr_context_yield(void)
{
	run_queue = current_context->c_run_queue;
	isr_context_ready(current_context);

	if (current_context != (struct context *)run_queue) {
		current_context->c_state = CONTEXT_READY;
		isr_context_switch();
	}
}

/*
 * context_yield()
 *	Release the current timeslice to another context that may be ready to run.
 */
void context_yield(void)
{
	isr_disable();
	debug_set_lights(0x12);
	
	isr_context_yield();

	isr_enable();
}

/*
 * __isr_context_wait()
 *	Wait (sleep) indefinitely.
 *
 * If we get woken (signalled) we return CONTEXT_READY, otherwise we must
 * have been interrupted and we return CONTEXT_INTERRUPTED.
 */
u8_t __isr_context_wait(struct lock *l, u8_t sleep_state)
{
	u8_t ret;

	/*
	 * We're going to lose our lock until we wakeup.
	 */
	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 0) {
				do {
					debug_stop();
					debug_print_pstr("\fcw lock not held: ");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
	
		l->l_lock = 0;
	}

	current_context->c_priority = l->l_pri_unlocked;
	
	current_context->c_spinlocks_held--;
	if (DEBUG) {
		if (current_context->c_spinlocks_held) {
			debug_stop();
			do {
				debug_print_pstr("\fcontext_wait: locks held!");
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}

	/*
	 * Get off the run queue and sleep until someone wakes us up.
	 */
	run_queue = current_context->c_run_queue;
	run_queue_len--;
	current_context->c_state = sleep_state;
	ret = isr_context_switch();

	/*
	 * We're back and running, so retake the lock!
	 */
        l->l_pri_unlocked = current_context->c_priority;
	current_context->c_priority = l->l_pri_locked;
	current_context->c_spinlocks_held++;

	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 1) {
				do {
					debug_stop();
					debug_print_pstr("\fcw lock already held:");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
			
		l->l_lock = 1;
	}

	return ret;
}


/*
 * isr_context_wait()
 *	Wait (sleep) indefinitely.
 *
 * If we get woken (signalled) we return CONTEXT_READY, otherwise we must
 * have been interrupted and we return CONTEXT_INTERRUPTED.
 */
u8_t isr_context_wait(struct lock *l)
{
	return __isr_context_wait(l, CONTEXT_SLEEP);
}

/*
 * context_wait()
 *	Wait (sleep) indefinitely.
 */
u8_t context_wait(struct lock *l)
{
	u8_t res;
	
	isr_disable();
	debug_set_lights(0x18);

	res = __isr_context_wait(l, CONTEXT_SLEEP);

	isr_enable();

	return res;
}

/*
 * context_wait_queue()
 *	Add ourself to a sleep queue and then wait (sleep) indefinitely.
 */
u8_t context_wait_queue(struct lock *l, struct context **queue)
{
	u8_t res;
	
	isr_disable();
	debug_set_lights(0x38);

	current_context->c_sleep_queue = *queue;
	*queue = current_context;
	
	res = __isr_context_wait(l, CONTEXT_SLEEP_QUEUE);

	isr_enable();

	return res;
}

/*
 * context_wait_pri_queue()
 *	Add ourself to a priority-ordered sleep queue and then wait (sleep) indefinitely.
 */
u8_t context_wait_pri_queue(struct lock *l, struct context **queue)
{
	u8_t res;
	priority_t cur_unlocked_pri;
	struct context *q;
	
	isr_disable();
	debug_set_lights(0x48);

	cur_unlocked_pri = l->l_pri_unlocked;
	q = *queue;

	while (q && (q->c_priority <= cur_unlocked_pri)) {
		queue = &q->c_sleep_queue;
		q = q->c_sleep_queue;
	}
	
	current_context->c_sleep_queue = q;
	*queue = current_context;
	
	res = __isr_context_wait(l, CONTEXT_SLEEP_QUEUE);

	isr_enable();

	return res;
}

/*
 * isr_context_signal()
 *	Signal a context that it's time to wake up.
 */
void isr_context_signal(struct context *c)
{
	if (c->c_state == CONTEXT_SLEEP) {
		c->c_state = CONTEXT_READY;
		run_queue_len++;
		isr_context_ready(c);
	}
}

/*
 * context_signal()
 *	Signal a context that it's time to wake up.
 */
void context_signal(struct context *c)
{
	isr_disable();
	debug_set_lights(0x33);

	isr_context_signal(c);

	isr_enable();
}

/*
 * context_signal_queue()
 *	Signal the first context on a sleep queue that it's time to wake up.
 *
 * Returns the number of contexts (0 or 1) woken up by the operation.
 */
u8_t context_signal_queue(struct context **queue)
{
	u8_t woken = 0;
	struct context *c;
	
	isr_disable();
	debug_set_lights(0x33);

	c = *queue;
	if (c) {
		*queue = c->c_sleep_queue;
		c->c_state = CONTEXT_READY;
		run_queue_len++;
		isr_context_ready(c);
		woken = 1;
	}
	
	isr_enable();

	return woken;
}

/*
 * context_broadcast_queue()
 *	Signal all of the contexts on a sleep queue that it's time to wake up.
 *
 * Returns the number of contexts woken up by the operation.
 */
u8_t context_broadcast_queue(struct context **queue)
{
	u8_t woken = 0;
	struct context *c;
	
	isr_disable();
	debug_set_lights(0x33);

	c = *queue;
	while (c) {
		c->c_state = CONTEXT_READY;
		run_queue_len++;
		isr_context_ready(c);
		
		c = c->c_sleep_queue;
		woken++;
	}
	*queue = NULL;

	isr_enable();

	return woken;
}

/*
 * isr_context_interrupt()
 *	Interrupt a context that's currently sleeping.
 */
void isr_context_interrupt(struct context *c)
{
	if (c->c_state == CONTEXT_SLEEP) {
		c->c_state = CONTEXT_INTERRUPTED;
		run_queue_len++;
		isr_context_ready(c);
	}
}

/*
 * context_interrupt()
 *	Interrupt a context that's currently sleeping.
 */
void context_interrupt(struct context *c)
{
	isr_disable();
	debug_set_lights(0x31);

	isr_context_interrupt(c);

	isr_enable();
}

/*
 * context_interrupt_queue()
 *	Interrupt a context that's queued on a sleep queue and is currently
 *	sleeping.
 *
 * Returns the number of contexts (0 or 1) interrupted (and woken) by the
 * operation.
 */
u8_t context_interrupt_queue(struct context *c, struct context **queue)
{
	u8_t woken = 0;
	struct context *q;
	
	isr_disable();
	debug_set_lights(0x39);

	q = *queue;
	while (q && (q != c)) {
		queue = &q->c_sleep_queue;
		q = q->c_sleep_queue;
	}

	if (q) {
		*queue = c->c_sleep_queue;
		c->c_state = CONTEXT_INTERRUPTED;
		run_queue_len++;
		isr_context_ready(c);
		woken = 1;
	}

	isr_enable();

	return woken;
}

/*
 * context_init()
 *	Create a new operating context and ready it for running.
 *
 * Our code sequencing here has to be a bit cute as this function may be
 * overwriting our call stack (this is safe for the first context that's
 * initialized since we never return in this case).
 */
void context_init(struct context *c, void (*fn)(void *), void *arg, void *stack_memory, addr_t stack_addr, priority_t pri)
{
	struct context_enter_frame *cef;
	struct context_switch_frame *csf;

	/*
	 * Initialize our context structure.
	 */
	c->c_stack_memory = stack_memory;
	c->c_priority = pri;
	c->c_state = CONTEXT_READY;
	c->c_context_switches = 0;
	c->c_spinlocks_held = 0;

	/*
	 * We need to create an entry frame to start this context.
	 */
	cef = (struct context_enter_frame *)(stack_addr - sizeof(struct context_enter_frame));
	context_enter_frame_build(cef, fn, arg);

	/*
	 * We also create a dummy context switch frame below the trap frame as we
	 * need to make it look like this new context simply got restored after
	 * being switched-out.
	 */
	csf = (struct context_switch_frame *)(stack_addr - sizeof(struct context_enter_frame) - sizeof(struct context_switch_frame));
	context_switch_frame_build(csf, c);
	
	run_queue_len++;
	isr_context_ready(c);
	
	if (!current_context) {
		jump_startup();
	}
	
	if (current_context != (struct context *)run_queue) {
		current_context->c_state = CONTEXT_READY;
		isr_context_switch();
	}
}
