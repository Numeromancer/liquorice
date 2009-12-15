/*
 * avr/bbi2c.c
 *	Bit-banged I2C support.
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
#include "cpu.h"
#include "io.h"

/*
 * bbi2c_short_delay()
 */
void bbi2c_short_delay(void)
{
	asm volatile ("nop\n\t"
			"nop\n\t"
			::);
}

/*
 * bbi2c_long_delay()
 *	Delay for 5 microseconds in order to keep to the I2C timings.
 */
void bbi2c_long_delay(void)
{
	bbi2c_short_delay();
	bbi2c_short_delay();
}

/*
 * bbi2c_init()
 *	Initialise the I2C bus.
 */
void bbi2c_init(void)
{
	/*
	 * Initialize our SDA and SCL pins.  Set them to be inputs and tri-stated
	 * which in effect means pull them high!
	 */
        out8(PORTD, in8(PORTD) & 0x3f);
        out8(DDRD, in8(DDRD) & 0x3f);
}
