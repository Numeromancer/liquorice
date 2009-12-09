/*
 * ipcsum.h
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

/*
 * Function prototypes.
 */
extern u16_t ipcsum_partial(u16_t partial_csum, void *buf, u16_t count);
extern u16_t ipcsum(u16_t partial_csum, void *buf, u16_t count);
extern u16_t ipcsum_pseudo_partial(u32_t src_addr, u32_t dest_addr, u8_t protocol, u16_t len);
extern u16_t ipcsum_pseudo(u32_t src_addr, u32_t dest_addr, u8_t protocol, u16_t len);
