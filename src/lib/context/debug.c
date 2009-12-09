/*
 * debug.c
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
#include "debug.h"
#include "context.h"

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/context_switch.h"
#elif defined(I386)
#include "i386/context_switch.h"
#else
#error "no valid architecture found"
#endif

/*
 * debug_assert_isr()
 *	Stop if we find the interrupt enable state is wrong.
 */
void debug_assert_isr(fast_u8_t disabled)
{
	if (DEBUG) {
		fast_u8_t ckres;
		
		ckres = isr_check_disabled();
		if (disabled != ckres) {
			debug_stop();
			do {
				debug_print_pstr("\fdasi: ints dis: ");
				debug_print8(ckres);
				debug_print_pstr(" should be: ");
				debug_print8(disabled);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
}

/*
 * debug_stack_trace()
 *	Print a debug stack trace.
 */
void debug_stack_trace(void)
{
	u8_t *sp;
				
	sp = (u8_t *)get_stack_pointer();
	
	debug_print_pstr("stp:c:");
	debug_print_addr((addr_t)current_context);
	debug_print_pstr(":p:");
	debug_print8(current_context->c_priority);
	debug_print_pstr(":s:");
	debug_print_addr((addr_t)sp);
	debug_print_pstr(":b:");
	debug_print_addr((addr_t)current_context->c_stack_memory);
	debug_wait_button();
	
	debug_print_stack();
}

/*
 * debug_check_stack()
 */
void debug_check_stack(addr_t guardband)
{
	if (DEBUG) {
		addr_t sp;
				
		sp = get_stack_pointer();
		
		if ((sp - guardband) <= (addr_t)(current_context->c_stack_memory)) {
			debug_stop();
			do {
				debug_print_pstr("\fstack blown:c:");
				debug_print_addr((addr_t)current_context);
				debug_print_pstr(":s:");
				debug_print_addr(sp);
				debug_print_pstr(":b:");
				debug_print_addr((addr_t)current_context->c_stack_memory);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}
}
