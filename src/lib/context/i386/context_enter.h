/*
 * context/i386/context_enter.h
 *	Context startup support.
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
 * Context entry frame layout.
 *	This is the layout we build for a new context for use in
 *	"context_enter".
 *
 * Remember of course that the bottom of the structure is the first thing to
 * be popped off the stack.
 */
struct context_enter_frame {
	u32_t cef_eip;			/* Address to return back to via "iret" */
	u16_t cef_cs;
	u16_t cef_cs_unused;
	u32_t cef_eflags;		/* Flags are the last thing popped by the "iret" */
	u32_t cef_fn_ret_addr;		/* Aparent return address when seen by the new context */
	u32_t cef_arg;			/* Argument to the new context function */
};

/*
 * context_enter_frame_build()
 *	Build a context entry frame for a new context.
 */
extern inline void context_enter_frame_build(struct context_enter_frame *cef, void (*fn)(void *), void *arg)
{
	cef->cef_cs = 0x0010;
	cef->cef_eip = (addr_t)fn;
	cef->cef_eflags = F_IF;
	cef->cef_arg = (u32_t)arg;
}
