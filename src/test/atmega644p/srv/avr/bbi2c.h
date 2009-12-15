/*
 * avr/bbi2c.h
 *	I/O functions used for the bit-banged I2C port.
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

extern void bbi2c_short_delay(void);
extern void bbi2c_long_delay(void);

/*
 * bbi2c_write_sda()
 *	Set the state of the I2C SDA line.
 *
 * This implementation relies on having a pull-up resistor on the the SDA line,
 * pulling it high when we tri-state the processor pin.  We drive the line low
 * by turning it back into an output.
 */
extern inline void bbi2c_write_sda(u8_t v)
{
	if (v) {
		asm volatile ("cbi %0, 7\n\t"
			: /* No outputs */
			: "I" (DDRD));
	} else {
		asm volatile ("sbi %0, 7\n\t"
			: /* No outputs */
			: "I" (DDRD));
	}
}

/*
 * bbi2c_read_sda()
 *	Read the state of the I2C SDA line.
 *
 * We assume that when this function is called that the pin used for the SDA
 * line is already configured as an input (i.e. we called "bbi2c_write_sda(1)"
 * before we got here!)
 */
extern inline u8_t bbi2c_read_sda(void)
{
	return (in8(PIND) & (1 << 7));
}

/*
 * bbi2c_write_scl()
 *	Set the state of the I2C SCL line.
 *
 * This implementation relies on having a pull-up resistor on the the SCL line,
 * pulling it high when we tri-state the processor pin.  We drive the line low
 * by turning it back into an output.
 */
extern inline void bbi2c_write_scl(u8_t v)
{
	if (v) {
		asm volatile ("cbi %0, 6\n\t"
			: /* No outputs */
			: "I" (DDRD));
	} else {
		asm volatile ("sbi %0, 6\n\t"
			: /* No outputs */
			: "I" (DDRD));
	}
}
