/*
 * debug.h
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
 * Are we debugging or not?
 */
#define DEBUG 1

/*
 * Global functions
 */
extern void debug_send_byte(unsigned char data);
extern void debug_print_prog_str(unsigned char *buf);
extern void debug_print8(u8_t data);
extern void debug_print16(u16_t data);
extern void debug_print32(u32_t data);
extern void debug_print_addr(addr_t data);
extern void debug_wait_button(void);
extern void debug_set_lights(u8_t pattern);
extern void debug_stop(void);
extern void debug_print_stack(void);
extern void debug_init(void);

/*
 * debug_print_pstr()
 */
#if DEBUG
#define debug_print_pstr(data) \
	debug_print_prog_str(PSTR(data))
#else
#define debug_print_pstr(data)
#endif

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/debug.h"
#elif defined(I386)
#include "i386/debug.h"
#else
#error "no valid architecture found"
#endif
