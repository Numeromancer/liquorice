/*
 * condvar.c
 *	Condition variable routines.
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
#include "condvar.h"

/*
 * condvar_wait()
 *	Wait until someone wakes us - either after the condition becomes true
 *	or if a timeout occurs.
 *
 * If we get woken we return CONTEXT_READY, otherwise we were interrupted
 * and we return CONTEXT_INTERRUPTED.
 */
u8_t condvar_wait(struct condvar *cv)
{
	return context_wait_pri_queue(cv->c_lock, &cv->c_sleep_queue);
}

/*
 * condvar_signal()
 */
void condvar_signal(struct condvar *cv)
{
	context_signal_queue(&cv->c_sleep_queue);
}

/*
 * condvar_broadcast()
 */
void condvar_broadcast(struct condvar *cv)
{
	context_broadcast_queue(&cv->c_sleep_queue);
}

/*
 * condvar_interrupt()
 *	Interrupt a thread that's waiting on a condvar.
 */
void condvar_interrupt(struct condvar *cv, struct context *c)
{
	context_interrupt_queue(c, &cv->c_sleep_queue);
}

/*
 * condvar_init()
 */
void condvar_init(struct condvar *cv, struct lock *l)
{
	cv->c_sleep_queue = NULL;
	cv->c_lock = l;
}
