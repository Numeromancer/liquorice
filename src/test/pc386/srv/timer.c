/*
 * timer.c
 *	Timer support routines.
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
#include "pic.h"
#include "timer.h"
#include "oneshot.h"

/*
 * Port addresses of the control port and timer channels
 */
#define PIT_CTRL 0x43
#define PIT_CH0 0x40
#define PIT_CH1 0x41
#define PIT_CH2 0x42

/*
 * Command to set rate generator mode
 */
#define CMD_SQR_WAVE 0x34

/*
 * Command to latch the timer registers
 */
#define CMD_LATCH 0x00

/*
 * Number of PIT ticks per second.
 */
#define PIT_TICKS 1193180

/*
 * The latch count value for the current TICK_RATE setting (actually one more
 * than the value, but this is used to indicate the number of ticks per time
 * interval).
 */
#define PIT_LATCH ((PIT_TICKS + (TICK_RATE / 2)) / TICK_RATE)

/*
 * Timer state values.
 */
volatile u32_t jiffies = 0;
static struct lock timer_lock;
static struct lock timer_isr_lock;
static struct context *timer_isr_ctx;
static volatile u16_t isr_ticks = 0;

/*
 * Load average values.
 */
u8_t ldavg_runnable = 0;
u32_t avenrun[LDAV_VALS] = {0, 0, 0};
u16_t ldavg_ticks = LDAV_TICKS;

/*
 * trap_irq0()
 */
void trap_irq0(void)
{
	debug_set_lights(0x03);
(*((u8_t *)0xb8000 + 140))++;
	
	isr_spinlock_lock(&timer_isr_lock);

	pic_acknowledge(IRQ_TIMER);
				
	isr_ticks++;

	debug_check_stack(0x100);

	ldavg_runnable = isr_thread_get_run_queue_len();
			
	isr_context_signal(timer_isr_ctx);

	isr_spinlock_unlock(&timer_isr_lock);

	/*
	 * Check if we need to re-schedule.
	 */
	isr_thread_schedule();
}

/*
 * timer_overflow_thread()
 */
void timer_overflow_thread(void *arg) __attribute__ ((noreturn));
void timer_overflow_thread(void *arg)
{
        /*
         * We will now redirect ISR activity here.
         */
	timer_isr_ctx = current_context;

	isr_disable();
	isr_spinlock_lock(&timer_isr_lock);
	
	/*
	 * initialise 8254 (or 8253) channel 0.  We set the timer to
	 * generate an interrupt every time we have done enough ticks.
	 * The output format is command, LSByte, MSByte.  Note that the
	 * lowest the value of HZ can be is 19 - otherwise the
	 * calculations get screwed up,
	 */
	out8(PIT_CTRL, CMD_SQR_WAVE);
	out8(PIT_CH0, (PIT_LATCH - 1) & 0x00ff);
	out8(PIT_CH0, ((PIT_LATCH - 1) & 0xff00) >> 8);

	/*
	 * Unmask timer interrupts - we don't need to worry yet though as
	 * the CPU interrupts are masked.
	 */
	pic_enable(IRQ_TIMER);

	while (1) {
                u8_t runnable;
                u16_t ticks;
                                                						
(*((u8_t *)0xb8000 + 142))++;
		debug_set_lights(0x0c);

		while (!isr_ticks) {
			isr_context_wait(&timer_isr_lock);
		}
		
		ticks = isr_ticks;
		isr_ticks = 0;	

		runnable = ldavg_runnable;
				
		isr_spinlock_unlock(&timer_isr_lock);
		isr_enable();

		spinlock_lock(&timer_lock);

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
			
			avenrun[1] *= LDAV_EXP_5;
			avenrun[1] += run_fixp * (LDAV_1 - LDAV_EXP_5);
			avenrun[1] >>= LDAV_FSHIFT;
			
			avenrun[2] *= LDAV_EXP_15;
			avenrun[2] += run_fixp * (LDAV_1 - LDAV_EXP_15);
			avenrun[2] >>= LDAV_FSHIFT;
			
			ldavg_ticks = LDAV_TICKS;
		} else {
			ldavg_ticks -= ticks;
		}
		
		spinlock_unlock(&timer_lock);
		
		oneshot_tick(ticks);
		
		isr_disable();
		isr_spinlock_lock(&timer_isr_lock);
	}
}

/*
 * timer_init()
 */
void timer_init(void)
{
	spinlock_init(&timer_lock, 0x12);
	spinlock_init(&timer_isr_lock, 0x00);
	
	thread_create(timer_overflow_thread, NULL, 0x2000, 0x7f);
}

/*
 * timer_get_jiffies()
 *	Get the elapsed jiffy count.
 */
u32_t timer_get_jiffies(void)
{
	u32_t res;
	
	spinlock_lock(&timer_lock);
	res = jiffies;
	spinlock_unlock(&timer_lock);
	
	return res;
}

/*
 * timer_dump_stats()
 *	Get the system load averages.
 */
void timer_dump_stats(u32_t *ldavgs, int max)
{
	spinlock_lock(&timer_lock);
	memcpy(ldavgs, avenrun, sizeof(u32_t) * ((LDAV_VALS > max) ? max : LDAV_VALS));
	spinlock_unlock(&timer_lock);
}
