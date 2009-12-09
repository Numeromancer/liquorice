/*
 * oneshot.c
 *	One-shot timer support routines.
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
#include "io.h"
#include "memory.h"
#include "isr.h"
#include "debug.h"
#include "context.h"
#include "heap.h"
#include "membuf.h"
#include "oneshot.h"

/*
 * One-shot timer list.
 */
static struct oneshot *volatile oneshot_list = NULL;
static struct lock oneshot_lock;

/*
 * oneshot_tick()
 */
void oneshot_tick(u16_t ticks)
{
	struct oneshot *os, **osprev;
	struct oneshot *callq = NULL;
	
	spinlock_lock(&oneshot_lock);
		
	/*
	 * Look at our oneshot list.  Reduce the ticks down on each one, and if
	 * we find that any have expired, remove them from this list and put
	 * them onto a callback queue.
	 */
	os = oneshot_list;
	osprev = (struct oneshot **)&oneshot_list;
	while (os) {
		if (os->os_ticks_left <= (u32_t)ticks) {
			os->os_call_queue = callq;
			callq = os;
			*osprev = os->os_next;
			os->os_next = NULL;
		} else {
			os->os_ticks_left -= (u32_t)ticks;
			osprev = &os->os_next;
		}
			
		os = os->os_next;
	}
				
	/*
	 * Run down the callback queue making calls to the different callees.
	 * Before we call each one we dereference it.  If we find that we
	 * held the last reference then it means our callee is no longer
	 * interested and we shouldn't be either!
	 */
	while (callq) {
               	os = callq;
               	callq = callq->os_call_queue;

		if (oneshot_deref(os)) {
			void *arg;
			void (*callback)(void *);
			
			callback = os->os_callback;
			arg = os->os_arg;
                				
			if (callback) {
				spinlock_unlock(&oneshot_lock);
				callback(arg);
				spinlock_lock(&oneshot_lock);
			}
		}
	}
		
	spinlock_unlock(&oneshot_lock);
}

/*
 * oneshot_alloc()
 *	Allocate a oneshot timer.
 */
struct oneshot *oneshot_alloc(void)
{
	struct oneshot *os;
	
	os = (struct oneshot *)membuf_alloc(sizeof(struct oneshot), NULL);
        os->os_ticks_left = 0;
        os->os_callback = NULL;
        os->os_arg = NULL;
        os->os_next = NULL;
                        	
	return os;
}

/*
 * oneshot_attach()
 *	Attach a timer to the list.
 */
void oneshot_attach(struct oneshot *timer)
{
	spinlock_lock(&oneshot_lock);

	if (DEBUG) {
		if (timer->os_next) {
			debug_stop();
			while (1) {
				debug_print_pstr("\fAttach timer: ");
				debug_print16((addr_t)timer);
				debug_print_pstr(" - is already");
				debug_wait_button();
				debug_stack_trace();
			}
		}
	}
	
	timer->os_next = oneshot_list;
	oneshot_list = timer;
        oneshot_ref(timer);
					
	spinlock_unlock(&oneshot_lock);
}

/*
 * oneshot_detach()
 *	Detach a timer from the list.
 */
void oneshot_detach(struct oneshot *timer)
{
        struct oneshot *os;
        struct oneshot **osprev;
        	
	spinlock_lock(&oneshot_lock);

	/*
	 * Walk the oneshot list and find the one we're to remove.
	 */
	os = oneshot_list;
	osprev = (struct oneshot **)&oneshot_list;
	while (os) {
		if (os == timer) {
			*osprev = os->os_next;
		        os->os_next = NULL;
			oneshot_deref(timer);
			break;
		}
	
		osprev = &os->os_next;
		os = os->os_next;
	}

	spinlock_unlock(&oneshot_lock);
}

/*
 * oneshot_dump_stats()
 */
int oneshot_dump_stats(struct oneshot *obuf, int max)
{
	struct oneshot *os;
        int ct = 0;
		
	spinlock_lock(&oneshot_lock);

	os = oneshot_list;
	while (os && (ct < max)) {
		memcpy(obuf, os, sizeof(struct oneshot));
		obuf->os_next = os;
		obuf++;
		ct++;
		os = os->os_next;
	}
	
	spinlock_unlock(&oneshot_lock);

	return ct;
}

/*
 * oneshot_init()
 */
void oneshot_init(void)
{
	spinlock_init(&oneshot_lock, 0x12);
}
