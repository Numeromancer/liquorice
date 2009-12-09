/*
 * i386/ipcsum.c
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
        u32_t words;
	void *d1;
	u32_t d2;
	
        words = count >> 1;
        d1 = buf;
                	
	if (words != 0) {
		/*
		 * This doesn't look particularly intuitive at first sight - in fact
		 * it probably looks plain wrong.  It does work however (take my word
		 * for it), but for some explanation the reader is referred to
		 * RFC1071 where the maths is explained in detail.
		 *
		 * Note that for a 32 bit CPU this is "really bad" (TM) code.  It
		 * needs reworking into something a lot faster!
		 */
		asm volatile ("\n"
				"L_redo%=:\n\t"
				"lodsw\n\t"
				"addw %%ax, %0\n\t"
				"adcw $0, %0\n\t"
				"loop L_redo%=\n\t"
				"\n\t"
				: "=g" (partial_csum), "=S" (d1), "=c" (d2)
				: "0" (partial_csum), "1" (d1), "2" (words)
				: "ax");
	}
	
	/*
	 * Did we have an odd number of bytes to do?
	 */
	if (count & 0x01) {
		asm volatile ("movb (%%esi), %%al\n\t"
				"xorb %%ah, %%ah\n\t"
				"addw %%ax, %0\n\t"
				"adcw $0, %0\n\t"
				: "=g" (partial_csum), "=S" (d1)
				: "0" (partial_csum), "1" (d1)
				: "ax");
	}
	
	return partial_csum;
}
