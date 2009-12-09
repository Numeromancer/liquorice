/*
 * context/avr/at90s8515/context_enter.h
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
	u8_t cef_r25;
	u8_t cef_r24;
	u8_t cef_sreg;
	u8_t cef_r1;
	u8_t cef_pchi;
	u8_t cef_pclo;
};

/*
 * context_enter_frame_build()
 *	Build a new trap frame for a new context.
 *
 * Note that we *know* that r24/r25 are used to pass our "arg" parameter to
 * our new context!
 */
extern inline void context_enter_frame_build(struct context_enter_frame *cef, void (*fn)(void *), void *arg)
{
	cef->cef_pchi = (unsigned char)((((addr_t)fn) >> 8) & 0xff);
	cef->cef_pclo = (unsigned char)(((addr_t)fn) & 0xff);
	cef->cef_sreg = 0x80;
	cef->cef_r1 = 0x00;
	cef->cef_r24 = (u8_t)(((u16_t)arg) & 0xff);
	cef->cef_r25 = (u8_t)(((u16_t)arg) >> 8);
}
