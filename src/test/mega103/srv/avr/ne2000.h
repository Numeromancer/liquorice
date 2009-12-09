/*
 * avr/ne2000.h
 *	NE2000 Ethernet controller definitions.
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
 * ns8390_read()
 *	Perform an I/O read on the NS8390.
 */
#define ns8390_read(reg) ({ \
	u8_t d; \
	asm volatile ("ldd %0, %a1+%2\n\t" \
			: "=r" (d) \
			: "b" (0x8300), "I" (reg)); \
	d; \
})

/*
 * ns8390_write()
 *	Perform and I/O write on the LCD.
 */
#define ns8390_write(reg, data) \
	asm volatile ("std %a0+%1, %2\n\t" \
			: /* No outputs */ \
			: "b" (0x8300), "I" (reg), "r" (data));
