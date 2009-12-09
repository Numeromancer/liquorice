/*
 * ipcsum.c
 *	Internet Protocol checksum and related support.
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
#include "ipcsum.h"

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/ipcsum.h"
#elif defined(I386)
#include "i386/ipcsum.h"
#else
#error "no valid architecture found"
#endif

/*
 * ipcsum()
 *	Calculates an the final IP checksum over a block of data.
 *
 * Unlike the partial checksum in "ipcsum_partial()", this function takes
 * the one's complement of the final result, thus making it the full checksum.
 */
u16_t ipcsum(u16_t partial_csum, void *buf, u16_t count)
{
	return ipcsum_partial(partial_csum, buf, count) ^ 0xffff;
}

/*
 * Details of the pseudo header used as part of the calculation of UDP and TCP
 * header checksums.
 */
struct pseudo_hdr {
	u32_t ph_src_addr;
	u32_t ph_dest_addr;
	u8_t ph_zero;
	u8_t ph_protocol;
	u16_t ph_len;
};

/*
 * ipcsum_pseudo_partial()
 *	Calculates the partial IP pseudo checksum.
 */
u16_t ipcsum_pseudo_partial(u32_t src_addr, u32_t dest_addr, u8_t protocol, u16_t len)
{
	struct pseudo_hdr ph;
	
	ph.ph_src_addr = src_addr;
	ph.ph_dest_addr = dest_addr;
	ph.ph_zero = 0;
	ph.ph_protocol = protocol;
	ph.ph_len = len;

	return ipcsum_partial(0, &ph, sizeof(struct pseudo_hdr));
}

/*
 * ipcsum_pseudo()
 *	Calculates the IP pseudo checksum.
 */
u16_t ipcsum_pseudo(u32_t src_addr, u32_t dest_addr, u8_t protocol, u16_t len)
{
	return ipcsum_pseudo_partial(src_addr, dest_addr, protocol, len) ^ 0xffff;
}
