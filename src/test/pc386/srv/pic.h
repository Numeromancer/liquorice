/*
 * pic.h
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
 * I/O port addresses for the two pics
 */
#define PIC0_ICW1 0x20
#define PIC0_ICW2 0x21
#define PIC0_ICW3 0x21
#define PIC0_ICW4 0x21
#define PIC0_OCW1 0x21
#define PIC0_OCW2 0x20
#define PIC0_OCW3 0x20
#define PIC0_IRR 0x20
#define PIC1_ICW1 0xa0
#define PIC1_ICW2 0xa1
#define PIC1_ICW3 0xa1
#define PIC1_ICW4 0xa1
#define PIC1_OCW1 0xa1
#define PIC1_OCW2 0xa0
#define PIC1_OCW3 0xa0
#define PIC1_IRR 0xa0

/*
 * Flag an "End Of Interrupt"
 */
#define FLAG_EOI 0x20

/*
 * Various IRQ line interrupt parameters
 */
#define IRQ_MAX 16		/* Number of ISA interrupt lines */
#define IRQ_MASTER 8		/* Number of master IRQ lines */

/*
 * Known hardware and virtual hardware interrupts
 */
#define IRQ_TIMER 0		/* Timer tick */
#define IRQ_DAISY 2		/* Daisy chain IRQ */

/*
 * Prototypes.
 */
extern void pic_enable(int irq);
extern void pic_disable(int irq);
extern void pic_init(void);

/*
 * pic_acknowledge()
 *	Acknowledge an interrupt.
 */
extern inline void pic_acknowledge(int irq)
{
	out8(PIC0_OCW2, FLAG_EOI);
	if (irq >= IRQ_MASTER) {
		out8(PIC1_OCW2, FLAG_EOI);
	}
}