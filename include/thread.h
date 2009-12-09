/*
 * thread.h
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
 * Operating thread information.
 *
 * Note - do *NOT* change the position of t_context in this structure!
 */
struct thread {
	struct context t_context;
	unsigned int t_ticks_scheduled;
	unsigned int t_ticks_left;
	struct thread *t_next;
};

/*
 * Function declarations
 */
extern void thread_create(void (*fn)(void *), void *arg, addr_t stack_sz, priority_t priority);
extern void thread_exit(void) __attribute__ ((noreturn));
extern void thread_yield(void);
extern void thread_wait(struct lock *l);
extern void thread_interrupt(struct thread *t);
extern int thread_dump_stats(struct thread *tbuf, int max);
extern void thread_init(void *idle, addr_t idle_size) __attribute__ ((noreturn));

/*
 * isr_thread_get_run_queue_len()
 *	Get the number of threads on the run queue.
 */
extern inline u8_t isr_thread_get_run_queue_len(void)
{
	return isr_context_get_run_queue_len();
}

/*
 * isr_thread_schedule()
 *	Look to see if our scheduling policy requires the current thread to be
 *	rescheduled.
 */
extern inline void isr_thread_schedule(void)
{
        /*
	 * We do round-robin timeslice scheduling for any thread with a
	 * priority lower than or equal to 0x80.
	 */
	if (current_context->c_priority >= 0x80) {
		struct thread *t = (struct thread *)current_context;
		
		t->t_ticks_left--;
		if (t->t_ticks_left == 0) {
			t->t_ticks_left = t->t_ticks_scheduled;
			isr_context_yield();
		}
	}
}
