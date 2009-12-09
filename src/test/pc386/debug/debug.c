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

static u8_t cx = 0;
static u8_t cy = 0;

/*
 * debug_send_byte()
 *	Send a character to the debug port.
 */
void debug_send_byte(unsigned char data)
{   
	u8_t *p = (u8_t *)0x000b8000;
	fast_u8_t sreg;
	int x;
        		
        sreg = isr_save_disable();

	if (data == '\r') {
		cx = 0;
	} else if (data == '\n') {
		cy++;
	} else if (data == '\f') {
		cx = 0;
	        cy = 0;
			
		for (x = 0; x < (80 * 24); x++) {
			*p = ' ';
			p += 2;
		}
	} else {
		if (cx >= 80) {
			cx = 0;
			cy++;
		}

		if (cy >= 24) {
			u8_t *src = (u8_t *)(0x000b8000 + 160);
				
			for (x = 160; x < (160 * 24); x++) {
				*p++ = *src++;
			}
			for (x = 0; x < 160; x += 2) {
				*p = ' ';
				p += 2;
			}
			p = (u8_t *)0x000b8000;
			cy = 23;
		}
	
		*(p + (((cy * 80) + cx) * 2)) = data;
		cx++;
	}

	isr_restore(sreg);
}

/*
 * debug_print_prog_str()
 *	Send a string to the debug port.
 */
void debug_print_prog_str(unsigned char *buf)
{
	unsigned char x;

	while ((x = *buf) != 0) {
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
	debug_print_pstr("\n\r");
}

/*
 * debug_set_lights()
 *	Display a pattern on 8 LEDs.
 *
 * OK - with a PC we don't have any LEDs so we emulate them on the bottom of
 * the screen instead.
 */
void debug_set_lights(u8_t pattern)
{
	s8_t i;
	u8_t *p = (u8_t *)(0x000b8000 + (24 * 80 * 2));
	
	for (i = 7; i >= 0; i--) {
		*p = (pattern & (1 << i)) ? '1' : '0';
		p += 2;
	}
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
	u8_t *sp;
        u8_t i;
				
	asm volatile ("movl %%esp, %0\n\t"
			: "=g" (sp)
			: /* No input */);
	
	/*
	 * Remove this function's references from the stack.
	 */
	sp += 16;
		
	debug_print_pstr("\n\rstack from:");
	debug_print_addr((addr_t)sp);

	debug_print_pstr("\n\rst1:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
	debug_print_pstr("\n\rst2:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
	debug_print_pstr("\n\rst3:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
	debug_print_pstr("\n\rst4:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
	debug_print_pstr("\n\rst5:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
	debug_print_pstr("\n\rst6:");
	for (i = 0; i < 32; i++) {
		debug_print8(*sp++);
		if ((i & 0x03) == 3) {
			debug_print_pstr(" ");
		}
	}
}

/*
 * debug_init()
 *	Initialize the debug output.
 */
void debug_init(void)
{
	int x;
	u8_t *p = (u8_t *)0x000b8000;
		
	for (x = 0; x < (80 * 25); x++) {
		*p = ' ';
		p += 2;
	}
		
	debug_print_pstr("Liquorice 001018\r\n");
}
