/*
 * avr/ipcsum.c
 *	Internet Protocol checksum support.
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

/*
 * ipcsum_partial()
 *	Calculates a partial IP checksum over a block of data.
 *
 * Note that this returns the checksum in network byte order, and thus does
 * not need to be converted via hton16(), etc.  Of course this means that
 * we mustn't use this value for normal arithmetic!
 *
 * This is a partial checksum because it doesn't take the 1's complement
 * of the overall sum.
 */
u16_t ipcsum_partial(u16_t partial_csum, void *buf, u16_t count)
{
        u16_t words;
	void *d1;
	u16_t d2;
	
        words = count >> 1;
        d1 = buf;
                	
	if (words != 0) {
		/*
		 * This doesn't look particularly intuitive at first sight - in fact
		 * it probably looks plain wrong.  It does work however (take my word
		 * for it), but for some explanation the reader is referred to
		 * RFC1071 where the maths is explained in detail.
		 */
		asm volatile ("\n"
				"L_redo%=:\n\t"
				"ld __tmp_reg__, %a1+\n\t"
				"add %A0, __tmp_reg__\n\t"
				"ld __tmp_reg__, %a1+\n\t"
				"adc %B0, __tmp_reg__\n\t"
				"adc %A0, r1\n\t"
				"adc %B0, r1\n\t"
				"dec %A2\n\t"
				"brne L_redo%=\n\t"
				"subi %B2, 1\n\t"
				"brsh L_redo%=\n\t"
				"\n\t"
				: "=r" (partial_csum), "=e" (d1), "=w" (d2)
				: "0" (partial_csum), "1" (d1), "2" (words));
	}
	
	/*
	 * Did we have an odd number of bytes to do?
	 */
	if (count & 0x01) {
		asm volatile ("ld __tmp_reg__, %a1+\n\t"
				"add %A0, __tmp_reg__\n\t"
				"adc %B0, r1\n\t"
				"adc %A0, r1\n\t"
				: "=r" (partial_csum), "=e" (d1)
				: "0" (partial_csum), "1" (d1));
	}
	
	return partial_csum;
}
