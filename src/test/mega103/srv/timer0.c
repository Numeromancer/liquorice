/*
 * timer0.c
 *	Timer 0 support routines.
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
#include "isr.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "thread.h"
#include "heap.h"
#include "membuf.h"
#include "timer.h"
#include "oneshot.h"

/*
 * Timer state values.
 */
volatile u32_t jiffies = 0;
static struct lock timer0_lock;
static struct lock timer0_isr_lock;
static struct context *timer0_isr_ctx;
static volatile u16_t isr_ticks = 0;

/*
 * Load average values.
 */
u8_t ldavg_runnable = 0;
u32_t avenrun[LDAV_VALS] = {0, 0, 0};
u16_t ldavg_ticks = LDAV_TICKS;

/*
 * timer0_overflow_isr()
 */
void timer0_overflow_isr(void)
{
	debug_set_lights(0x03);
	
	isr_spinlock_lock(&timer0_isr_lock);
			
	/*
	 * Ensure that we run again.
	 */
	out8(TCNT0, 100);

	isr_ticks++;

	debug_check_stack(0x30);

	ldavg_runnable = isr_thread_get_run_queue_len();
			
	isr_context_signal(timer0_isr_ctx);

	isr_spinlock_unlock(&timer0_isr_lock);

	/*
	 * Check if we need to re-schedule.
	 */
	isr_thread_schedule();
}

/*
 * _timer0_overflow_()
 */
void _timer0_overflow_(void) __attribute__ ((naked));
void _timer0_overflow_(void)
{
	asm volatile ("push r0\n\t"
			"push r1\n\t"
			"in r0, 0x3f\n\t"
			"push r0\n\t"
#ifdef ATMEGA103
			"in r0, 0x3b\n\t"
			"push r0\n\t"
#endif
			"push r18\n\t"
			"push r19\n\t"
			"push r20\n\t"
			"push r21\n\t"
			"push r22\n\t"
			"push r23\n\t"
			"push r24\n\t"
			"push r25\n\t"
			"push r26\n\t"
			"push r27\n\t"
			"push r30\n\t"
			"push r31\n\t"
			::);

	timer0_overflow_isr();

	asm volatile ("jmp startup_continue\n\t" ::);
}

/*
 * timer0_overflow_thread()
 */
void timer0_overflow_thread(void *arg) __attribute__ ((noreturn));
void timer0_overflow_thread(void *arg)
{
        /*
         * We will now redirect ISR activity here.
         */
	timer0_isr_ctx = current_context;

	isr_disable();
	isr_spinlock_lock(&timer0_isr_lock);
	
	/*
	 * Allow timer 0 to tick (well enable interrupts from it anyway).
	 */
	out8(TIMSK, BV(TOIE0));

	while (1) {
                u8_t runnable;
                u16_t ticks;
                                                						
		debug_set_lights(0x0c);

		while (!isr_ticks) {
			isr_context_wait(&timer0_isr_lock);
		}
		
		ticks = isr_ticks;
		isr_ticks = 0;	

		runnable = ldavg_runnable;
				
		isr_spinlock_unlock(&timer0_isr_lock);
		isr_enable();

		spinlock_lock(&timer0_lock);

		/*
		 * Update the wall-clock!
		 */
		jiffies += (u32_t)ticks;
		
		/*
		 * Check if we need to run a load average calculation.
		 */
		if (ldavg_ticks <= ticks) {
		        u32_t run_fixp;
        		
			run_fixp = (runnable - 1) * LDAV_1;

			avenrun[0] *= LDAV_EXP_1;
			avenrun[0] += run_fixp * (LDAV_1 - LDAV_EXP_1);
			avenrun[0] >>= LDAV_FSHIFT;
			
//			avenrun[1] *= LDAV_EXP_5;
//			avenrun[1] += run_fixp * (LDAV_1 - LDAV_EXP_5);
//			avenrun[1] >>= LDAV_FSHIFT;
			
//			avenrun[2] *= LDAV_EXP_15;
//			avenrun[2] += run_fixp * (LDAV_1 - LDAV_EXP_15);
//			avenrun[2] >>= LDAV_FSHIFT;
			
			ldavg_ticks = LDAV_TICKS;
		} else {
			ldavg_ticks -= ticks;
		}
		
		spinlock_unlock(&timer0_lock);
		
		oneshot_tick(ticks);
		
		isr_disable();
		isr_spinlock_lock(&timer0_isr_lock);
	}
}

/*
 * timer_init()
 */
void timer_init(void)
{
	out8(TCCR0, 6);
	out8(TCNT0, 100);

	spinlock_init(&timer0_lock, 0x12);
	spinlock_init(&timer0_isr_lock, 0x00);
	
	thread_create(timer0_overflow_thread, NULL, 0x100, 0x7f);
}

/*
 * timer_get_jiffies()
 *	Get the elapsed jiffy count.
 */
u32_t timer_get_jiffies(void)
{
	u32_t res;
	
	spinlock_lock(&timer0_lock);
	res = jiffies;
	spinlock_unlock(&timer0_lock);
	
	return res;
}

/*
 * timer_dump_stats()
 *	Get the system load averages.
 */
void timer_dump_stats(u32_t *ldavgs, int max)
{
	spinlock_lock(&timer0_lock);
	memcpy(ldavgs, avenrun, sizeof(u32_t) * ((LDAV_VALS > max) ? max : LDAV_VALS));
	spinlock_unlock(&timer0_lock);
}
