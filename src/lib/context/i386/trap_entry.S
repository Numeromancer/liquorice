/*
 * context/i386/trap_entry.S
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

/*
 * Templates for entry handling.  IDT_ERROR() is for entries which
 * already have an error code supplied by the i386.  IDT_DUMMY() is for
 * those which don't and we push a dummy 0.
 */
#define IDT_DUMMY(n, f) \
	.globl	__##f ; \
	.align	4 ; \
; \
__##f##: ; \
	cld ; \
	pushl	$0 ; \
	push	$(n) ; \
	pushl	%eax ; \
	pushl	%ecx ; \
	pushl	%edx ; \
	call	f ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	addl	$8, %esp ; \
	iret

#define IDT_ERROR(n, f) \
	.globl	__##f ; \
	.align	4 ; \
; \
__##f##: ; \
	cld ; \
	pushl	$(n) ; \
	pushl	%eax ; \
	pushl	%ecx ; \
	pushl	%edx ; \
	call	f ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	addl	$8, %esp ; \
	iret

/*
 * The vectors we handle
 */
IDT_DUMMY(0x00, trap_divide_by_zero)
IDT_DUMMY(0x01, trap_debug)
IDT_DUMMY(0x02, trap_nmi)
IDT_DUMMY(0x03, trap_breakpoint)
IDT_DUMMY(0x04, trap_overflow)
IDT_DUMMY(0x05, trap_bound)
IDT_DUMMY(0x06, trap_invalid_opcode)
IDT_DUMMY(0x07, trap_device_not_available)
IDT_ERROR(0x08, trap_double_fault)
IDT_ERROR(0x09, trap_coprocessor_overrun)
IDT_ERROR(0x0a, trap_invalid_tss)
IDT_ERROR(0x0b, trap_segment_not_present)
IDT_ERROR(0x0c, trap_stack_fault)
IDT_ERROR(0x0d, trap_general_protection)
IDT_ERROR(0x0e, trap_page_fault)
IDT_ERROR(0x10, trap_floating_point_error)
IDT_ERROR(0x11, trap_alignment_check)
IDT_DUMMY(0xff, trap_unused)

/*
 * IRQ_VECTOR()
 *	Macro to set up trap frame for a hardware interrupt
 *
 * We use an extra pushl to make it look much like a trap frame.
 * This radically simplifies things and makes it easier to debug
 * things.  We push the IRQ number, not the interrupt vector number
 * as this saves some time in the interrupt handler
 */
#define IRQ_VECTOR(n, f) \
	.globl	__##f ; \
	.align	4 ; \
; \
__##f##: ; \
	cld ; \
	pushl	$0 ; \
	push	$(n) ; \
	pushl	%eax ; \
	pushl	%ecx ; \
	pushl	%edx ; \
	call	f ; \
	popl	%edx ; \
	popl	%ecx ; \
	popl	%eax ; \
	addl	$8, %esp ; \
	iret

IRQ_VECTOR(0x20, trap_irq0)
IRQ_VECTOR(0x21, trap_irq1)
IRQ_VECTOR(0x22, trap_irq2)
IRQ_VECTOR(0x23, trap_irq3)
IRQ_VECTOR(0x24, trap_irq4)
IRQ_VECTOR(0x25, trap_irq5)
IRQ_VECTOR(0x26, trap_irq6)
IRQ_VECTOR(0x27, trap_irq7)
IRQ_VECTOR(0x28, trap_irq8)
IRQ_VECTOR(0x29, trap_irq9)
IRQ_VECTOR(0x2a, trap_irq10)
IRQ_VECTOR(0x2b, trap_irq11)
IRQ_VECTOR(0x2c, trap_irq12)
IRQ_VECTOR(0x2d, trap_irq13)
IRQ_VECTOR(0x2e, trap_irq14)
IRQ_VECTOR(0x2f, trap_irq15)

/*
 * Keep things neat
 */
	.align 4
