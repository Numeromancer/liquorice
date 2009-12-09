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

#define LCD_INSTR 0
#define LCD_DATA 1

/*
 * lcd_read()
 *	Perform an I/O read on the LCD.
 */
static inline u8_t lcd_read(u8_t reg)
{
	u8_t volatile *rp;
	
#if defined(STK300)
	/*
	 * Why is A14 used for C/#D?  This is bizarre - A0 would seem a more
	 * logical choice, but never mind this is how it is...
	 */
	rp = (u8_t *)((reg == LCD_INSTR) ? 0x8000 : 0xc000);
#elif defined(DJH300) || defined(DJHNE)
	rp = (u8_t *)((reg == LCD_INSTR) ? 0xc000 : 0xc001);
#elif defined(PCB520)
	rp = (u8_t *)((reg == LCD_INSTR) ? 0xa000 : 0xa001);
#else
#error "No valid LCD configuration defined."
#endif
		
	return *rp;
}

/*
 * lcd_write()
 *	Perform and I/O write on the LCD.
 */
static inline void lcd_write(u8_t reg, u8_t data)
{
	u8_t volatile *rp;
	
#if defined(STK300)
	rp = (u8_t *)((reg == LCD_INSTR) ? 0x8000 : 0xc000);
#elif defined(DJH300) || defined(DJHNE)
	rp = (u8_t *)((reg == LCD_INSTR) ? 0xc000 : 0xc001);
#elif defined(PCB520)
	rp = (u8_t *)((reg == LCD_INSTR) ? 0xa000 : 0xa001);
#else
#error "No valid LCD configuration defined."
#endif
	
	*rp = data;
}

/*
 * wait_ready()
 */
static void wait_ready(void)
{
	u16_t i;

	for (i = 0; i < 100; i++) {
		if ((lcd_read(LCD_INSTR) & 0x80) == 0x00) {
			return;
		}
	}
}

/*
 * debug_send_byte()
 *	Send a character to the debug port.
 */
void debug_send_byte(unsigned char data)
{   
	unsigned char sreg;
	int x;
        		
        sreg = isr_save_disable();

	for (x = 0; x < 10; x++);
		
	if (data == '\r') {
		cx = 0;
	} else if (data == '\n') {
		cy++;
	} else if (data == '\f') {
		cx = 0;
	        cy = 0;

		wait_ready();
		lcd_write(LCD_INSTR, 0x01);
		for (x = 0; x < 1000; x++);
	} else {
		if (cx >= 16) {
			cx = 0;
			cy++;
		}
			
		if (cy <= 1) {
			wait_ready();
			lcd_write(LCD_INSTR, (u8_t)(0x80 | ((cy == 1) ? 0x40 : 0x00) | cx));
			for (x = 0; x < 100; x++);
		
			wait_ready();
			lcd_write(LCD_DATA, data);
			for (x = 0; x < 100; x++);
		}
	
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
	u16_t debounce;

	/*
	 * The debounce timing is a little arbitrary to say the least.  Really
	 * should be based on something a little more scientific :-)
	 */
	debounce = 0;
	while (debounce < 10000) {
		if ((in8(PIND) & 0x01) == 0x00) {
			debounce++;
		} else {
			debounce = 0;
		}
	}
		
	debounce = 0;
	while (debounce < 10000) {
		if ((in8(PIND) & 0x01) == 0x01) {
			debounce++;
		} else {
			debounce = 0;
		}
	}

	debug_print_pstr("\f");
}

/*
 * debug_set_lights()
 *	Display a pattern on 8 LEDs.
 */
void debug_set_lights(u8_t pattern)
{
	out8(PORTB, pattern ^ 0xff);
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
				
	asm volatile ("in %A0, %1\n\t"
			"in %B0, %2\n\t"
			: "=r" (sp)
			: "I" (SPL), "I" (SPH));

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
	int i;
	
	for (i = 0; i < 30000; i++);
	
	wait_ready();
	lcd_write(LCD_INSTR, 0x30);
	wait_ready();
	lcd_write(LCD_INSTR, 0x30);
	wait_ready();
	lcd_write(LCD_INSTR, 0x30);
	wait_ready();
	lcd_write(LCD_INSTR, 0x3f);
	wait_ready();
	lcd_write(LCD_INSTR, 0x0c);
	wait_ready();
	lcd_write(LCD_INSTR, 0x06);
	wait_ready();
	lcd_write(LCD_INSTR, 0x01);
		
	debug_print_pstr("\fLiquorice 001018\r\n");
		
#if defined(PCB520)
	/*
	 * This looks a little odd, but on a PCB520 system this is used
	 * to ensure the correct operation of the debug input switch.
	 * D0 is used as the input pin, whilst D1 is held low and D2
	 * is held high.  A 1k resistor normally pulls D0 to D2, whilst
	 * a push button pulls D0 to D1!
	 */
        out8(PORTD, 0xfb);
#endif
}
