/*
 * types.h
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

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/types.h"
#elif defined(I386)
#include "i386/types.h"
#else
#error "no valid architecture found"
#endif

/*
 * Null value declaration.
 */
#define NULL 0

/*
 * Boolean states.
 */
#define FALSE 0
#define TRUE 1

/*
 * Bit value conversion macro
 */
#define BV(x) (1 << x)

/*
 * Argument handling for variable parameter functions
 */
typedef char *va_list;

/*
 * Amount of space required in an arg list for an arg of type TYPE.
 * TYPE may alternatively be an expression whose type is used.
 */
#define __va_rnd_size(TYPE) \
	(((sizeof(TYPE) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#define va_start(AP, LASTARG) \
	(AP = ((char *)&(LASTARG) + __va_rnd_size(LASTARG)))

#define va_end(AP)			/* Nothing at all! */

#define va_arg(AP, TYPE) (AP += __va_rnd_size(TYPE), \
	*((TYPE *)(AP - __va_rnd_size(TYPE))))
