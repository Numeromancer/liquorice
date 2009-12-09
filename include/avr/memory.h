/*
 * avr/memory.h
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
 *
 * Some of the code in this file is derived from the avr-libc library written
 * by Marek Michalkiewicz <marekm@linux.org.pl>
 */

/*
 * PSTR()
 *	Accessor to get at strings within the ROM space.
 */
#define PSTR(s) ({static char c[] __attribute__ ((progmem)) = s;c;})

/*
 * __lpm_macro()
 */
#define __lpm_macro(addr) ({ \
	u8_t res; \
	asm volatile ("lpm\n\t" \
			"mov %0, r0\n\t" \
			: "=r" (res) \
			: "z" (addr)); \
	res; \
})

/*
 * memcpy()
 *	Copy a block of memory from one location to another.
 */
extern inline void *memcpy(void *dest, const void *src, addr_t n)
{
	void *a1, *a2;
	addr_t a3;	

	if (n) {
		asm volatile ("\n"
				"L_hi%=:\n\t"
				"ld __tmp_reg__, %a0+\n\t"
				"st %a1+, __tmp_reg__\n\t"
				"sbiw %2, 1\n\t"
				"brne L_hi%=\n\t"
				: "=e" (a1), "=e" (a2), "=w" (a3)
				: "0" (src), "1" (dest), "2" (n)
				: "memory");
	}
	
	return (dest);
}
