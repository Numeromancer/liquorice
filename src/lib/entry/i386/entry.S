/*
 * entry/i386/entry.S
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

	.text

/*
 * _start
 *	Initial code entry point from the real-mode code.
 *
 * This function *MUST* be the first code in the kernel image!
 */
	.globl	_start
	.align	4

_start:
	/*
	 * Initialize data segments and the stack.  We use 0x0009d000 for the
	 * stack as the page below it is specially allocated for it by the
	 * bootstraping code.  Once contexts are established this page can be
	 * freed back to the general pool.
	 */
	movl	$0x08,%eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	movl	$0x0009d000, %esp

	/*
	 * Ensure that all flags are clean - in particular the direction flag,
	 * which is not set correctly by some systems.
	 */
	pushl	$0
	popfl

	jmp	main

/*
 * Keep things neat
 */
	.align 4
