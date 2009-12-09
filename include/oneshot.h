/*
 * oneshot.h
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
 * One-shot timer structure.
 */
struct oneshot {
	u32_t os_ticks_left;		/* Number of ticks before the timer triggers */
	void (*os_callback)(void *);	/* Callback function */
	void *os_arg;			/* Callback function argument */
	struct oneshot *os_next;	/* Next timer in the list */
	struct oneshot *os_call_queue;	/* Next timer on the callback queue */
};

/*
 * Function declarations
 */
extern void oneshot_tick(u16_t ticks);
extern struct oneshot *oneshot_alloc(void);
extern void oneshot_attach(struct oneshot *timer);
extern void oneshot_detach(struct oneshot *timer);
extern int oneshot_dump_stats(struct oneshot *obuf, int max);
extern void oneshot_init(void);

/*
 * oneshot_ref()
 */
extern inline void oneshot_ref(struct oneshot *os)
{
	membuf_ref(os);
}

/*
 * oneshot_deref()
 */
extern inline ref_t oneshot_deref(struct oneshot *os)
{
	return membuf_deref(os);
}

/*
 * oneshot_get_refs()
 *	Get the number of references to a timer.
 */
extern inline ref_t oneshot_get_refs(struct oneshot *os)
{
	return membuf_get_refs(os);
}
