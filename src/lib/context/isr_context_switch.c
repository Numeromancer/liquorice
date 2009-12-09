/*
 * context.c
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

#if defined(ATMEGA103)
#include "avr/context_switch.h"
#elif defined(AT90S8515)
#include "avr/context_switch.h"
#elif defined(I386)
#include "i386/context_switch.h"
#else
#error "no valid architecture found"
#endif

/*
 * isr_context_switch()
 *
 * Returns the run-state of the context before we started running again.
 *
 * WARNING - do NOT change any part of this function unless you really know
 * what you're doing - if the context stack frame is changed then any new
 * contexts will not start correctly!
 */
u8_t isr_context_switch(void)
{
	u8_t ret;
	
	current_context->c_context_switches++;
	
	context_switch_prologue(current_context);
	
	startup_mark();

	current_context = (struct context *)run_queue;

	context_switch_epilogue(current_context);
	
	ret = current_context->c_state;
	current_context->c_state = CONTEXT_RUNNING;

	return ret;
}
