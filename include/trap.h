/*
 * trap.h
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

#if defined(ATMEGA103)
#include "avr/atmega103/trap.h"
#elif defined(AT90S8515)
#include "avr/at90s8515/trap.h"
#elif defined(ATMEGA644P)
#include "avr/atmega644p/trap.h"
#elif defined(I386)
#include "i386/trap.h"
#else
#error "no valid architecture found"
#endif
