/*
 * i386/types.h
 *	Standard type declarations.
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
 * Start with standard bit-sized declarations.
 */
typedef char s8_t;
typedef unsigned char u8_t;
typedef short int s16_t;
typedef unsigned short int u16_t;
typedef int s32_t;
typedef unsigned int u32_t;
typedef long long int s64_t;
typedef unsigned long long int u64_t;

/*
 * Fast types - these are guaranteed to be of a particular minimum size, but
 * may well be larger if that would be faster on a particular architecture.
 * In this instance, the i386 architecture can handle 8 bits quite nicely, but
 * 16 bit accesses incur a one cycle penalty (so we try to avoid them).
 */
typedef char fast_s8_t;
typedef unsigned char fast_u8_t;
typedef int fast_s16_t;
typedef unsigned int fast_u16_t;
typedef int fast_s32_t;
typedef unsigned int fast_u32_t;
typedef long long int fast_s64_t;
typedef unsigned long long int fast_u64_t;

/*
 * Address type.
 */
typedef unsigned int addr_t;

/*
 * Thread priority type.
 */
typedef unsigned int priority_t;

/*
 * Reference count type.
 */
typedef unsigned int ref_t;

/*
 * hton16()
 *	Conversion of 16 bit value to network order (big endian)
 */
extern inline u16_t hton16(u16_t hval)
{
	asm volatile ("xchgb %b0, %h0\n\t"
			: "=q" (hval)
			: "0" (hval));

	return hval;
}

/*
 * hton32()
 *	Conversion of 32 bit value to network order (big endian)
 */
extern inline u32_t hton32(u32_t hval)
{
	asm volatile ("xchgb %b0, %h0\n\t"
			"rorl $16, %0\n\t"
			"xchgb %b0, %h0\n\t"
			: "=q" (hval)
			: "0" (hval));

	return hval;
}
