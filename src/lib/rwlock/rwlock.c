/*
 * rwlock.c
 *	Reader/writer lock routines.
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
#include "rwlock.h"

/*
 * rwlock_lock_read()
 *	Attempt to gain read access.  If we can't get it immediately then sleep
 *	until we can.
 *
 * If we get woken we return CONTEXT_READY, otherwise we were interrupted
 * and we return CONTEXT_INTERRUPTED.
 */
u8_t rwlock_lock_read(struct rwlock *rwl)
{
	u8_t res;

	spinlock_lock(&rwl->rwl_lock);

	/*
	 * Are there any writers waiting or holding the lock?  If there aren't
	 * any then we can just go straight through, otherwise we have to wait.
	 */
	if ((rwl->rwl_writers == 0) && (rwl->rwl_write_sleep_queue == NULL)) {
		rwl->rwl_readers++;
		res = CONTEXT_READY;
	} else {
		/*
		 * Note that we don't need to priority-queue readers as they
		 * will all be released at the same time (so there's nothing to
		 * be gained by sorting them).
		 */
		res = context_wait_queue(&rwl->rwl_lock, &rwl->rwl_read_sleep_queue);
	}
	
	spinlock_unlock(&rwl->rwl_lock);
	
	return res;
}

/*
 * rwlock_lock_write()
 *	Attempt to gain write access.  If we can't get it immediately then
 *	sleep until we can.
 *
 * If we get woken we return CONTEXT_READY, otherwise we were interrupted
 * and we return CONTEXT_INTERRUPTED.
 */
u8_t rwlock_lock_write(struct rwlock *rwl)
{
	u8_t res;

	spinlock_lock(&rwl->rwl_lock);

	/*
	 * Are there any readers or writers already holding the lock?  If the
	 * answer is no then we can just go straight through, otherwise we have
	 * to wait.
	 */
	if ((rwl->rwl_writers == 0) && (rwl->rwl_readers == 0)) {
		rwl->rwl_writers++;
		res = CONTEXT_READY;
	} else {
		res = context_wait_pri_queue(&rwl->rwl_lock, &rwl->rwl_write_sleep_queue);
	}
	
	spinlock_unlock(&rwl->rwl_lock);
	
	return res;
}

/*
 * rwlock_unlock_read()
 */
void rwlock_unlock_read(struct rwlock *rwl)
{
	spinlock_lock(&rwl->rwl_lock);

	if (DEBUG) {
		if (rwl->rwl_readers == 0) {
			debug_stop();
			do {
				debug_print_pstr("\frwl_unl_rd: not holding: ");
				debug_print_addr((addr_t)rwl);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
	
	if (rwl->rwl_readers) {
		rwl->rwl_readers--;
		if (rwl->rwl_readers == 0) {
			rwl->rwl_writers = context_signal_queue(&rwl->rwl_write_sleep_queue);
		}
	}
	
	spinlock_unlock(&rwl->rwl_lock);
}

/*
 * rwlock_unlock_write()
 */
void rwlock_unlock_write(struct rwlock *rwl)
{
	spinlock_lock(&rwl->rwl_lock);

	if (DEBUG) {
		if (rwl->rwl_writers == 0) {
			debug_stop();
			do {
				debug_print_pstr("\frwl_unl_wr: not holding: ");
				debug_print_addr((addr_t)rwl);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
	
	/*
	 * If we have another writer waiting for access then give it control.
	 * If we only have readers waiting then release them all!
	 */
	if (context_signal_queue(&rwl->rwl_write_sleep_queue) == 0) {
		rwl->rwl_readers = context_broadcast_queue(&rwl->rwl_read_sleep_queue);
	}
	
	spinlock_unlock(&rwl->rwl_lock);
}

/*
 * rwlock_interrupt()
 *	Interrupt a thread that's waiting on a reader/writer lock.
 */
void rwlock_interrupt(struct rwlock *rwl, struct context *c)
{
	spinlock_lock(&rwl->rwl_lock);

	/*
	 * First try the writer queue.  If we don't wake anyone that way, then
	 * try the reader queue.  Note - our context can't be queued on both!
	 */
	if (context_interrupt_queue(c, &rwl->rwl_write_sleep_queue) == 0) {
		context_interrupt_queue(c, &rwl->rwl_read_sleep_queue);
	}

	spinlock_unlock(&rwl->rwl_lock);
}

/*
 * rwlock_init()
 */
void rwlock_init(struct rwlock *rwl, priority_t pri)
{
	rwl->rwl_read_sleep_queue = NULL;
	rwl->rwl_write_sleep_queue = NULL;
	spinlock_init(&rwl->rwl_lock, pri);
	rwl->rwl_readers = 0;
}
