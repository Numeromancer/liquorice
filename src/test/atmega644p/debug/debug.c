/*
 * debug.c
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
#include "memory.h"
#include "isr.h"
#include "debug.h"



/*
 * debug_send_byte()
 *	Send a character to the debug port.
 */
void debug_send_byte(unsigned char data)
{   
}

/*
 * debug_print_prog_str()
 *	Send a string to the debug port.
 */
void debug_print_prog_str(unsigned char *buf)
{
	unsigned char x;

	while ((x = __lpm_macro(buf)) != 0) {
		debug_send_byte(x);
		buf++;
	}
}

/*
 * debug_print4()
 *	Send the hex represenation of a 4 bit value (nibble) to the debug port.
 */
void debug_print4(unsigned char data)
{
	unsigned char character = data & 0x0f;
	if (character > 9) {
		character += 'A' - 10;
	} else {
		character += '0';
	}

	debug_send_byte(character);
}

/*
 * debug_print8()
 *	Send the hex represenation of an 8 bit value (byte) to the debug port.
 */
void debug_print8(u8_t data)
{
	debug_print4(data >> 4);
	debug_print4(data);
}

/*
 * debug_print16()
 *	Send the hex represenation of a 16 bit value (word) to the debug port.
 */
void debug_print16(u16_t data)
{
	debug_print8((u8_t)(data >> 8));
	debug_print8((u8_t)data);
}

/*
 * debug_print32()
 *	Send the hex represenation of a 32 bit value (dword) to the debug port.
 */
void debug_print32(u32_t data)
{
	debug_print16((u16_t)(data >> 16));
	debug_print16((u16_t)data);
}

/*
 * debug_wait_button()
 *	Wait for a push button to be released and then pressed.
 */
void debug_wait_button(void)
{
}

/*
 * debug_set_lights()
 *	Display a pattern on 8 LEDs.
 */
void debug_set_lights(u8_t pattern)
{
}

/*
 * debug_stop()
 *	Stop interrupts.
 */
void debug_stop(void)
{
	isr_disable();
}

/*
 * debug_print_stack()
 *	Print a debug stack trace.
 */
void debug_print_stack(void)
{
	u8_t *sp = (u8_t *)SP;
    u8_t i;

    /*
	 * The AVR stack is one byte lower than the first "stacked" item, plus
	 * we want to ignore the call to this function and it's 3 pushes!
	 */
	sp += 6;
	
	debug_print_pstr("st1:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
	
	debug_print_pstr("st2:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
	
	debug_print_pstr("st3:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
	
	debug_print_pstr("st4:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
	
	debug_print_pstr("st5:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
	
	debug_print_pstr("st6:");
	for (i = 0; i < 14; i++) {
		debug_print8(*sp++);
	}
	debug_wait_button();
}

/*
 * debug_init()
 *	Initialize the debug output.
 */
void debug_init(void)
{
    debug_print_pstr("\fLiquorice 001018\r\n");
}
