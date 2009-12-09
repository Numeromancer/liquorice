/*
 * sem.c
 *	Counting semaphore routines.
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
#include "sem.h"

/*
 * sem_wait()
 *	Wait until someone wakes us - either when the count reaches zero
 *	or if a timeout occurs.
 *
 * If we get woken we return CONTEXT_READY, otherwise we were interrupted
 * and we return CONTEXT_INTERRUPTED.
 */
u8_t sem_wait(struct sem *s)
{
	u8_t res;

	spinlock_lock(&s->s_lock);

	s->s_count--;
	if (s->s_count >= 0) {
		res = CONTEXT_READY;
	} else {
		res = context_wait_pri_queue(&s->s_lock, &s->s_sleep_queue);
	}

	spinlock_unlock(&s->s_lock);
	
	return res;
}

/*
 * sem_post()
 */
void sem_post(struct sem *s)
{
	spinlock_lock(&s->s_lock);

	s->s_count++;
	if (s->s_count <= 0) {
		context_signal_queue(&s->s_sleep_queue);
	}

	spinlock_unlock(&s->s_lock);
}

/*
 * sem_interrupt()
 *	Interrupt a thread that's waiting on a semaphore.
 */
void sem_interrupt(struct sem *s, struct context *c)
{
	spinlock_lock(&s->s_lock);

	context_interrupt_queue(c, &s->s_sleep_queue);

	spinlock_unlock(&s->s_lock);
}

/*
 * sem_get_count()
 *	Get the current semaphore count.
 */
s8_t sem_get_count(struct sem *s)
{
	s8_t res;
	
	spinlock_lock(&s->s_lock);

	res = s->s_count;
	
	spinlock_unlock(&s->s_lock);

	return res;
}

/*
 * sem_init()
 */
void sem_init(struct sem *s, priority_t pri, s8_t count)
{
	s->s_sleep_queue = NULL;
	spinlock_init(&s->s_lock, pri);
	s->s_count = count;
}
