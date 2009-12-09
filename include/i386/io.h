/*
 * i386/io.h
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
 */

/*
 * in8()
 */
#define in8(port) ({ \
	u8_t res; \
	asm volatile ("inb %%dx, %%al\n\t" \
			: "=a" (res) \
			: "d" (port)); \
	res; \
})

/*
 * in16()
 */
#define in16(port) ({ \
	u16_t res; \
	asm volatile ("inw %%dx, %%ax\n\t" \
			: "=a" (res) \
			: "d" (port)); \
	res; \
})

/*
 * out8()
 */
#define out8(port, val) \
	asm volatile ("outb %%al, %%dx\n\t" \
			: /* No output */ \
			: "a" (val), "d" (port)) \

/*
 * out16()
 */
#define out16(port, val) \
	asm volatile ("outw %%ax, %%dx\n\t" \
			: /* No output */ \
			: "a" (val), "d" (port)) \
