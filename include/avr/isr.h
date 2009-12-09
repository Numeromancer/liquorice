/*
 * avr/isr.h
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
 * isr_disable()
 */
extern inline void isr_disable(void)
{
	asm volatile ("cli\n\t" ::);
}

/*
 * isr_enable()
 */
extern inline void isr_enable(void)
{
	asm volatile ("sei\n\t" ::);
}

/*
 * isr_check_disabled()
 */
extern inline fast_u8_t isr_check_disabled(void)
{
	u8_t sreg;

	asm volatile ("in %0, %1\n\t"
			: "=r" (sreg)
			: "I" (SREG));

	return (fast_u8_t)((sreg & 0x80) ? FALSE : TRUE);
}
