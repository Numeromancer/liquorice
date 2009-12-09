/*
 * avr/io.h
 *	I/O management functions.
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
 * in8()
 */
#define in8(port) ({ \
	u8_t res; \
	asm volatile ("in %0, %1\n\t" \
		: "=r" (res) \
		: "I" ((u8_t)(port))); \
	res; \
})

/*
 * out8()
 */
#define out8(port, val) \
	asm volatile ("out %0, %1\n\t" \
		: /* No outputs */ \
		: "I" ((u8_t)(port)), "r" ((u8_t)(val)))
