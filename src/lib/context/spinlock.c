/*
 * spinlock.c
 *	Data structure locking routines.
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
#include "context.h"

/*
 * isr_spinlock_lock()
 *	Lock a lock used to protect information within an ISR.
 */
void isr_spinlock_lock(struct lock *l)
{
	if (DEBUG) {
		if (l->l_pri_locked != 0) {
			do {
				debug_print_pstr("\fispllk pri !0:");
				debug_print_addr((addr_t)current_context);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
		
        l->l_pri_unlocked = current_context->c_priority;
	current_context->c_priority = 0;
	current_context->c_spinlocks_held++;
		
	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 1) {
				debug_stop();
				do {
					debug_print_pstr("\fispllk already held:");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
			
		l->l_lock = 1;
	}
}

/*
 * spinlock_lock()
 *	Lock a lock.
 */
void spinlock_lock(struct lock *l)
{
	if (DEBUG) {
		debug_assert_isr(FALSE);
	}
	
	isr_disable();
	debug_set_lights(0x16);
	
	if (DEBUG) {
		if (l->l_pri_locked > current_context->c_priority) {
			debug_stop();
			do {
				debug_print_pstr("\fspllk pri: ");
				debug_print8(current_context->c_priority);
				debug_print_pstr(":can't to ");
				debug_print8(l->l_pri_locked);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
		
        l->l_pri_unlocked = current_context->c_priority;
	current_context->c_priority = l->l_pri_locked;
	current_context->c_spinlocks_held++;
		
	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 1) {
				debug_stop();
				do {
					debug_print_pstr("\fspllk already held:");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
			
		l->l_lock = 1;
	}
	
	isr_enable();
}

/*
 * isr_spinlock_unlock()
 *	Unlock a lock used to protect data within an ISR.
 */
void isr_spinlock_unlock(struct lock *l)
{
	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 0) {
				debug_stop();
				do {
					debug_print_pstr("\fisplun not held: ");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
	
		l->l_lock = 0;
	}

	if (DEBUG) {
		if (l->l_pri_unlocked < current_context->c_priority) {
			debug_stop();
			do {
				debug_print_pstr("\fisplun pri: ");
				debug_print8(current_context->c_priority);
				debug_print_pstr(" - can't to ");
				debug_print8(l->l_pri_unlocked);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
		
	current_context->c_priority = l->l_pri_unlocked;
	current_context->c_spinlocks_held--;

	/*
	 * If we ever do SMP then this logic will need to be changed!  It
	 * would need to check all CPUs to determine whether this (current)
	 * context needs to be pre-empted out.
	 */
        if (current_context->c_run_queue) {
        	if (current_context->c_run_queue->c_priority < l->l_pri_unlocked) {
			struct context *q, **qprev;

			run_queue = current_context->c_run_queue;
		
			q = (struct context *)run_queue;
			qprev = (struct context **)&run_queue;

			while (q && (q->c_priority < current_context->c_priority)) {
				qprev = &q->c_run_queue;
				q = q->c_run_queue;
			}
		
			current_context->c_run_queue = q;
			*qprev = current_context;
			current_context->c_state = CONTEXT_READY;
			isr_context_switch();
        	}
	}
}

/*
 * spinlock_unlock()
 *	Unlock a spinlock.
 */
void spinlock_unlock(struct lock *l)
{
	if (DEBUG) {
		debug_assert_isr(FALSE);
	}

	isr_disable();
	debug_set_lights(0x17);

	if (LOCK) {
		if (DEBUG) {
			if (l->l_lock == 0) {
				debug_stop();
				do {
					debug_print_pstr("\fsplun not held: ");
					debug_print_addr((addr_t)l);
					debug_wait_button();
					debug_stack_trace();
				} while (debug_cycle());
			}
		}
	
		l->l_lock = 0;
	}

	if (DEBUG) {
		if (l->l_pri_unlocked < current_context->c_priority) {
			debug_stop();
			do {
				debug_print_pstr("\fsplun pri: ");
				debug_print8(current_context->c_priority);
				debug_print_pstr(" - can't to ");
				debug_print8(l->l_pri_unlocked);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
		
	current_context->c_priority = l->l_pri_unlocked;
	current_context->c_spinlocks_held--;

	/*
	 * If we ever do SMP then this logic will need to be changed!  It
	 * would need to check all CPUs to determine whether this (current)
	 * context needs to be pre-empted out.
	 */
        if (current_context->c_run_queue) {
        	if (current_context->c_run_queue->c_priority < l->l_pri_unlocked) {
			struct context *q, **qprev;

			run_queue = current_context->c_run_queue;
		
			q = (struct context *)run_queue;
			qprev = (struct context **)&run_queue;

			while (q && (q->c_priority < current_context->c_priority)) {
				qprev = &q->c_run_queue;
				q = q->c_run_queue;
			}
		
			current_context->c_run_queue = q;
			*qprev = current_context;
			current_context->c_state = CONTEXT_READY;
			isr_context_switch();
        	}
	}
	
	isr_enable();
}
