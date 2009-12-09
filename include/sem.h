/*
 * sem.h
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
 * semaphore structure.
 */
struct sem {
	struct context *s_sleep_queue;
	struct lock s_lock;
	s8_t s_count;
};

/*
 * Function prototypes.
 */
extern u8_t sem_wait(struct sem *s);
extern void sem_post(struct sem *s);
extern void sem_interrupt(struct sem *s, struct context *c);
extern s8_t sem_get_count(struct sem *s);
extern void sem_init(struct sem *s, priority_t pri, s8_t count);
