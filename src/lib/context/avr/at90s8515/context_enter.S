/*
 * context/at90s8515/context_enter.S
 *	Entry point to the system and to any of the functions that might cause
 *	scheduler pre-emption to occur.
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

 	.arch	at90s8515

/*
 * context_enter
 *	Facility for starting a context's normal execution.
 */
	.global	context_enter

context_enter:
	pop	r25
	pop	r24
	pop	r0
	out	0x3f, r0
	pop	r1
	reti
