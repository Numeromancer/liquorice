/*
 * thread.c
 *	Thread (and scheduling) support routines.
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
#include "heap.h"
#include "thread.h"

/*
 * Global declarations associated with scheduling threads.
 */
struct thread *thread_list = NULL;
struct thread *idle_free = NULL;

/*
 * idle_thread()
 */
void idle_thread(void *arg) __attribute__ ((noreturn));
void idle_thread(void *arg)
{
	extern void init(void);
	
	isr_enable();

	init();

	isr_disable();
	debug_set_lights(0x1a);

	current_context->c_priority = 255;
	isr_context_yield();
	isr_enable();
			
	/*
	 * Our idle thread does do a little more than nothing!  It acutally
	 * looks to see if any threads have exited and if they have then it
	 * tidies up after them (they will have left their memory lying around
	 * in a rather careless fashion).
	 */
	while (1) {
		struct thread *t;
	
		isr_disable();
		t = idle_free;
		isr_enable();
	
		while (t) {
			struct thread *tfree;

			tfree = t;
			t = t->t_next;

			/*
			 * Remember that we don't need to free the thread
			 * structure as it's part of the same heap block as
			 * the stack.
			 */
			heap_free(((struct context *)tfree)->c_stack_memory);
		}
	
		idle_free = NULL;
	}
}

/*
 * thread_build()
 */
void thread_build(struct thread *t, void (*fn)(void *), void *arg, void *stack_memory, addr_t stack_addr, priority_t pri)
{
	t->t_ticks_scheduled = 10;
	t->t_ticks_left = 10;

	t->t_next = thread_list;
	thread_list = t;
	
	context_init(&t->t_context, fn, arg, stack_memory, stack_addr, pri);
}

/*
 * thread_create()
 */
void thread_create(void (*fn)(void *), void *arg, addr_t stack_sz, priority_t priority)
{
	struct thread *new_thread;
	void *stack;

	/*
	 * OK - we're moderately cute here and allocate the stack and the
	 * thread structure at the same time.  The thread structure is then
	 * made to occupy the end of the allocated block so that if the stack
	 * grows downwards it doesn't immediately overwrite the thread info.
	 * The double allocation saves both heap overhead and time!
	 */
	stack = heap_alloc(stack_sz + sizeof(struct thread));
	new_thread = (struct thread *)((u8_t *)stack + stack_sz);
		
	isr_disable();
        thread_build(new_thread, fn, arg, stack, (addr_t)new_thread, priority);
	isr_enable();
}

/*
 * thread_exit()
 *	Cause the current thread to cease executing.
 *
 * We remove ourself from the run queue, clear down some of the thread table
 * details and then jump into the middle of the task switching routine.
 * This switches our current state out without trying to save it first.
 */
void thread_exit(void)
{
	struct thread *t, **tprev;
	
	isr_disable();
	
	debug_set_lights(0x11);

	t = thread_list;
	tprev = &thread_list;
	
	while (t != (struct thread *)current_context) {
		tprev = &t->t_next;
		t = t->t_next;
	}

	*tprev = t->t_next;

	/*
	 * We need to make sure that the memory used for this thread gets
	 * restored to the heap.  We can't do that ourself of course as we're
	 * using that memory now!
	 */
	((struct thread *)current_context)->t_next = idle_free;
	idle_free = (struct thread *)current_context;
	
	context_exit();
}

/*
 * thread_yield()
 *	Release the current timeslice to another thread that may be ready to run.
 */
void thread_yield(void)
{
	isr_disable();
	debug_set_lights(0x12);

	((struct thread *)current_context)->t_ticks_left = ((struct thread *)current_context)->t_ticks_scheduled;
	isr_context_yield();

	isr_enable();
}

/*
 * thread_wait()
 *	Wait (sleep) indefinitely.
 *
 * Any thread that calls this can only be woken up by being "interrupted".
 */
void thread_wait(struct lock *l)
{
	context_wait(l);
}

/*
 * thread_interrupt()
 *	Interrupt a thread that's in the THREAD_SLEEP_WAIT state.
 */
void thread_interrupt(struct thread *t)
{
	context_interrupt(&t->t_context);
}

/*
 * thread_dump_stats()
 */
int thread_dump_stats(struct thread *tbuf, int max)
{
	struct thread *t;
        int ct = 0;
		
	/*
	 * OK - this is a thoroughly bad idea - I know.  This should be fixed
	 * when we get a better mechanism for looking at system internals.
	 */
	isr_disable();

	t = thread_list;
	while (t && (ct < max)) {
		memcpy(tbuf, t, sizeof(struct thread));
		tbuf->t_next = t;
		tbuf++;
		ct++;
		t = t->t_next;
	}

	isr_enable();

	return ct;
}

/*
 * thread_init()
 */
void thread_init(void *idle, addr_t idle_size)
{
	addr_t sp;
	
	if (DEBUG) {
		debug_assert_isr(TRUE);
	}
	
	/*
	 * We initialize our idle thread with a different memory layout to any
	 * of our others, but this is safe as it never exits and thus never
	 * confuses the deallocation code.  The reason for this is that we
	 * don't want to have to move our stack now and once the thread starts
	 * running it can continue to use the same area of memory.
	 */
	sp = ((addr_t)idle) + idle_size;
	
	thread_build((struct thread *)idle, idle_thread, NULL, (u8_t *)(idle + sizeof(struct thread)), sp, 0x7f);

	while (1);
}
