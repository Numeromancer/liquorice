/*
 * condvar.h
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

/*
 * condvar structure.
 */
struct condvar {
	struct context *c_sleep_queue;
	struct lock *c_lock;
};

/*
 * Function prototypes.
 */
extern u8_t condvar_wait(struct condvar *cv);
extern void condvar_signal(struct condvar *cv);
extern void isr_condvar_signal(struct condvar *cv);
extern void condvar_broadcast(struct condvar *cv);
extern void condvar_interrupt(struct condvar *cv, struct context *c);
extern void condvar_init(struct condvar *cv, struct lock *l);
