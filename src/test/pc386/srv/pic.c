/*
 * pic.c
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
#include "pic.h"

/*
 * PIC interrupt mask - start with all masked
 */
static u16_t imask = 0xffff;

/*
 * pic_enable()
 *	Permit a hardware interrupt to be delivered
 */
void pic_enable(int irq)
{
	imask &= ~(1 << irq);
	if (irq >= IRQ_MASTER) {
		imask &= ~(1 << IRQ_DAISY);
	}
	out8(PIC0_OCW1, (u8_t)(imask & 0xff));
	out8(PIC1_OCW1, (u8_t)((imask >> 8) & 0xff));
}

/*
 * pic_disable()
 *	Inhibit a hardware interrupt from being delivered
 */
void pic_disable(int irq)
{
	imask |= (1 << irq);
	
	/*
	 * If the IRQ being disabled is from the slave PIC then we need to
	 * look at whether we should disable the slave PIC
	 */
	if (irq >= IRQ_MASTER) {
		if ((imask & 0xff00) == 0xff00) {
			imask |= (1 << IRQ_DAISY);
		}
	}
	out8(PIC0_OCW1, (u8_t)(imask & 0xff));
	out8(PIC1_OCW1, (u8_t)((imask >> 8) & 0xff));
}

/*
 * pic_init()
 *	Initialise the 8259 interrupt controllers
 */
void pic_init(void)
{
	/*
	 * initialise the 8259's
	 */
	out8(PIC0_ICW1, 0x11);		/* Reset */
	out8(PIC0_ICW2, IRQ_BASE);	/* Vectors served */
	out8(PIC0_ICW3, 1 << 2);	/* Chain to PIC1 */
	out8(PIC0_ICW4, 1);		/* 8086 mode */
	out8(PIC0_OCW1, 0xff);		/* No interrupts for now */
	out8(PIC0_OCW2, 2);		/* ISR mode */

	out8(PIC1_ICW1, 0x11);		/* Reset */
	out8(PIC1_ICW2, IRQ_BASE + 8);  /* Vectors served */
	out8(PIC1_ICW3, 2);		/* We are a slave unit */
	out8(PIC1_ICW4, 1);		/* 8086 mode */
	out8(PIC1_OCW1, 0xff);		/* No interrupts for now */
	out8(PIC1_OCW2, 2);		/* ISR mode */
}
