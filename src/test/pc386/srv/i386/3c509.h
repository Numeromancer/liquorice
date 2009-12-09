/*
 * i386/3c509.h
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
#define c509_read8(reg) in8(0x300 + reg)

/*
 * c509_read16()
 *	Perform an I/O read on the Ethernet chip.
 */
#define c509_read16(reg) in16(0x300 + reg)

/*
 * c509_read8id()
 *	Perform an I/O read on the ID port of the Ethernet chip.
 */
#define c509_read8id() in8(0x110)

/*
 * c509_write8()
 *	Perform and I/O write on the Ethernet chip.
 */
#define c509_write8(reg, data) out8(0x300 + reg, data)

/*
 * c509_write16()
 *	Perform and I/O write on the Ethernet chip.
 */
#define c509_write16(reg, data) out16(0x300 + reg, data)

/*
 * c509_write8id()
 *	Perform and I/O write on the ID port of the Ethernet chip.
 */
#define c509_write8id(data) out8(0x110, data)
