/*
 * i386/i82595.h
 *	i82595 Ethernet controller support.
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
 * i82595_read8()
 *	Perform an I/O read on the Ethernet chip.
 */
#define i82595_read8(reg) in8(0x300 + reg)

/*
 * i82595_read16()
 *	Perform an I/O read on the Ethernet chip.
 */
#define i82595_read16(reg) in16(0x300 + reg)

/*
 * i82595_write8()
 *	Perform and I/O write on the Ethernet chip.
 */
#define i82595_write8(reg, data) out8(0x300 + reg, data)

/*
 * i82595_write16()
 *	Perform and I/O write on the Ethernet chip.
 */
#define i82595_write16(reg, data) out16(0x300 + reg, data)
