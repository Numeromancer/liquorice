/*
 * i386/memory.h
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
 * PSTR()
 *	Accessor to get at strings within the ROM space.
 *
 * Of course the x86 architecture doesn't have separate code and data spaces
 * so we don't do anything special here!
 */
#define PSTR(s) (s)

/*
 * memcpy()
 *	Copy a block of memory from one location to another.
 *
 * We have a fast version for aligned data and a slower one that can cope with
 * byte copying.
 */
extern inline void *memcpy(void *dest, const void *src, addr_t n)
{
	void *a1, *a2;
	addr_t a3;	

	if (!(n & 3)) {
		/*
		 * Nice quick version :-)
		 */
		asm volatile ("rep\n\t"
				"movsl\n\t"
				: "=S" (a1), "=D" (a2), "=c" (a3)
				: "0" (src), "1" (dest), "2" (n >> 2));
	} else {
		/*
		 * Slower version for byte-aligned data
		 */
		asm volatile ("movl %%ecx,%%eax\n\t"
				"shrl $2,%%ecx\n\t"
				"rep\n\t"
				"movsl\n\t"
				"andl $3,%%eax\n\t"
				"jz 2f\n\t"
				"test $2,%%eax\n\t"
				"jz 1f\n\t"
				"movsw\n"
				"1:\n\t"
				"test $1,%%eax\n\t"
				"jz 2f\n\t"
				"movb (%%esi),%%al\n\t"
				"movb %%al,(%%edi)\n"
				"2:\n\t"
				: "=S" (a1), "=D" (a2), "=c" (a3)
				: "0" (src), "1" (dest), "2" (n)
				: "ax");
	}
	
	return dest;
}
