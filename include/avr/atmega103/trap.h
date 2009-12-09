/*
 * avr/atmega103/trap.h
 *	Standard pre-emption trap handler.
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
 * Trap frame layout.
 *	This is the layout of the stack after a trap/interrupt handler has
 *	been invoked.
 *
 * The stack frame shown here shows the stack after registers that can be
 * wantonly destroyed by GCC have been saved, but before the C code ISR has
 * been called.
 *
 * Remember of course that the bottom of the structure was the last thing to
 * have been pushed onto the stack and the top is the first!
 */
struct trap_frame {
	u8_t tf_r31;
	u8_t tf_r30;
	u8_t tf_r27;
	u8_t tf_r26;
	u8_t tf_r25;
	u8_t tf_r24;
	u8_t tf_r23;
	u8_t tf_r22;
	u8_t tf_r21;
	u8_t tf_r20;
	u8_t tf_r19;
	u8_t tf_r18;
	u8_t tf_rampz;
	u8_t tf_sreg;
	u8_t tf_r1;
	u8_t tf_r0;
	u8_t tf_pchi;
	u8_t tf_pclo;
};
