/*
 * rwlock.h
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
 * reader-writer lock structure.
 */
struct rwlock {
	struct lock rwl_lock;		/* Lock used to protect the structure */
	struct context *rwl_read_sleep_queue;
					/* List of readers that are waiting */
	struct context *rwl_write_sleep_queue;
					/* List of writers that are waiting */
	u8_t rwl_readers;		/* Number of threads currently holding read access */
	u8_t rwl_writers;		/* Number of threads currently holding write access */
};

/*
 * Function prototypes.
 */
extern u8_t rwlock_lock_read(struct rwlock *rwl);
extern u8_t rwlock_lock_write(struct rwlock *rwl);
extern void rwlock_unlock_read(struct rwlock *rwl);
extern void rwlock_unlock_write(struct rwlock *rwl);
extern void rwlock_interrupt(struct rwlock *rwl, struct context *c);
extern void rwlock_init(struct rwlock *rwl, priority_t pri);
