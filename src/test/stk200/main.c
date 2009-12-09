/*
 * main.c
 *	Main entry point into the code.
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
#include "memory.h"
#include "isr.h"
#include "string.h"
#include "debug.h"
#include "context.h"
#include "condvar.h"
#include "heap.h"
#include "thread.h"

/*
 * Stack size for our idle thread
 */
#define IDLE_SIZE 0x90

/*
 * A gcc-defined symbol that signifies the end of the bss section.
 */
extern void *__bss_end;

static struct lock timer0_isr_lock = {0, 0x00, 0xff};

/*
 * _interrupt0_()
 */
#if DEBUG
void _interrupt0_(void) __attribute__ ((naked));
void _interrupt0_(void)
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

	while (1) {
		debug_print_pstr("\fint ctrl:");
		debug_print8(in8(GIFR));
		debug_print8(in8(GIMSK));
		debug_wait_button();
		debug_stack_trace();
	}

	asm volatile ("rjmp startup_continue\n\t" ::);
}
#endif

/*
 * timer0_overflow_isr()
 */
void timer0_overflow_isr(void)
{
	debug_set_lights(0xf3);

	isr_spinlock_lock(&timer0_isr_lock);
			
	/*
	 * Ensure that we run again.
	 */
	out8(TCNT0, 100);

	debug_check_stack(0);
			
	isr_spinlock_unlock(&timer0_isr_lock);

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

	asm volatile ("rjmp startup_continue\n\t" ::);
}

/*
 * test1_thread()
 */
void test1_thread(void *arg) __attribute__ ((noreturn));
void test1_thread(void *arg)
{
	u32_t i;

	while (1) {
		debug_print_pstr("\f1:mem:");
		debug_print_addr(heap_get_free());
	
		for (i = 0; i < 100000; i++);
	}
}

/*
 * test2_thread()
 */
void test2_thread(void *arg) __attribute__ ((noreturn));
void test2_thread(void *arg)
{
	u32_t i;

	while (1) {
		debug_print_pstr("\f2:thread");
	
		for (i = 0; i < 70000; i++);
	}
}

/*
 * init()
 */
void init(void)
{
	heap_add((addr_t)(&__bss_end), (addr_t)(RAMEND - (IDLE_SIZE - 1)) - (addr_t)(&__bss_end));
	
	/*
	 * Initialize timer0.
	 */
	out8(TCCR0, 4);
	out8(TCNT0, 100);
	out8(TIMSK, BV(TOIE0));

	/*
	 * Create the test threads
	 */
	thread_create(test1_thread, NULL, 0x40, 0xf8);
	thread_create(test2_thread, NULL, 0x40, 0xf8);
}

/*
 * main()
 */
int main(void)
{
	out8(DDRA, 0xff);
	out8(DDRB, 0xff);
	out8(DDRD, 0xff);
	
	/*
	 * We run the external bus with a wait state.  This isn't ideal, but an
	 * awful lot of peripherals really don't like the AVR memory bus
	 * timings :-(
	 */
	out8(MCUCR, BV(SRW) | BV(SRE));

	/*
	 * Provide debugging capabilities before we attempt anything else.
	 */		
	debug_init();
	
	/*
	 * Debug setup.
	 */
        out8(DDRD, 0xf3);
	out8(GIMSK, in8(GIMSK) | BV(INT0));

	/*
	 * Start the threading system.  Note that we don't exit here, we never
	 * return from the thread initialization.
	 */
	thread_init((void *)(RAMEND - (IDLE_SIZE - 1)), 0x90);
}
