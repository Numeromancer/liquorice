/*
 * avr/3c509.h
 *	I/O functions used with the 3C509 Ethernet card.
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
 * c509_read8()
 *	Perform an I/O read on the Ethernet chip.
 */
#define c509_read8(reg) ({ \
	u8_t d; \
	asm volatile ("ldd %0, %a1+%2\n\t" \
			: "=r" (d) \
			: "b" (0x8300), "I" (reg)); \
	d; \
})

/*
 * c509_read16()
 *	Perform an I/O read on the Ethernet chip.
 */
#define c509_read16(reg) ({ \
	u16_t d; \
	asm volatile ("ldd %A0, %a1+%2\n\t" \
			"ldd %B0, %a1+%3\n\t" \
			: "=w" (d) \
			: "b" (0x8300), "I" (reg), "I" (reg + 1)); \
	d; \
})

/*
 * c509_read8id()
 *	Perform an I/O read on the ID port of the Ethernet chip.
 */
#define c509_read8id() ({ \
	u8_t d; \
	asm volatile ("ld %0, %a1\n\t" \
			: "=r" (d) \
			: "b" (0x8110)); \
	d; \
})

/*
 * c509_write8()
 *	Perform and I/O write on the Ethernet chip.
 */
#define c509_write8(reg, data) \
	asm volatile ("std %a0+%1, %2\n\t" \
			: /* No outputs */ \
			: "b" (0x8300), "I" (reg), "r" (data));

/*
 * c509_write16()
 *	Perform and I/O write on the Ethernet chip.
 */
#define c509_write16(reg, data) \
	asm volatile ("std %a0+%1, %A3\n\t" \
			"std %a0+%2, %B3\n\t" \
			: /* No outputs */ \
			: "b" (0x8300), "I" (reg), "I" (reg + 1), "w" (data));

/*
 * c509_write8id()
 *	Perform and I/O write on the ID port of the Ethernet chip.
 */
#define c509_write8id(data) \
	asm volatile ("st %a0, %1\n\t" \
			: /* No outputs */ \
			: "b" (0x8110), "r" (data));
