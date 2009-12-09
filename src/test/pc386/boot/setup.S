#
# setup.S
#	Get the system ready for running in 32 bit protected mode.
#
# Copyright (C) 2000 David J. Hudson <dave@humbug.demon.co.uk>
#
# This file is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
#
# You can redistribute this file and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software Foundation;
# either version 2 of the License, or (at your discretion) any later version.
# See the accompanying file "copying-gpl.txt" for more details.
#
# We're loaded by a bootstrap loader, find any relevant system parameters
# from the system ROM and put the system into protected mode (actually into
# paged protected mode).  We then initiate a call into the 32 bit system
# proper.  None of the code within this module is required after this
# transition, and so may be safely eliminated immediately afterwards!
#

#include "bootconst.h"
	
	.code16
	
	.text

#
# As with the bootsector we need an entry point - also "main" - in line with
# "C" thinking :-)
#
	.globl	_main

_main:
	cli				# We don't want interrupts now!
	movw	$0x9800, %ax		# Ensure that we have our segment
	movw	%ax, %ds		# referencing straight

#
# OK, time for a stack eh - put it at 0x9c00:1000 (0x0009d000 once we're in
# flat mode).  Remember of course that the stack grows downwards!
#
	movw	$0x9c00, %ax
	movw	%ax, %ss
	movw	$0x1000, %sp

#
# We need two page tables and a page directory.  Page directory is going to be
# at 0x0009f000, and our first page table at 0x0009e000.  First we blank both
# pages and then we start filling in the parts we need.
#
	movw	$0x9e00, %ax
	movw	%ax, %es
	xorl	%eax, %eax
	xorw	%si, %si
	movw	$0x0800, %cx
	
blank:
	movl	%eax, %es:(%si)
	addw	$4, %si
	loop	blank
	
#
# Now fill the page directory.
#
	movw	$0x1000, %si
	movl	$0x0009e003, %eax	# Page is read/writeable and present
	movl	%eax, %es:(%si)		# Our startup mapping is from 0x00001000 to 0x000fffff

#
# Next we want the pages mapping into our page table - we start at page 1 and
# not page 0 so that any NULL pointer dereferencing causes a trap.
#
	movw	$0x0004, %si		# Remember that we don't map page 0
	movl	$0x00001003, %eax	# Pages are readable, writeable and present
	movw	$0xff, %cx

fill_pt:
	movl	%eax, %es:(%si)
	addw	$4, %si
	addl	$0x00001000, %eax
	loop	fill_pt

#
# Establish the base table registers
#
	movl	$0x0009f000, %eax
	movl	%eax, %cr3		# Set up the base page dir reference
	lgdt	setup_gdt_48		# Set up the global descriptor table

#
# Now switch to paged, protected mode
#
	movl	%cr0, %eax
	orl	$0x80000005, %eax
	movl	%eax, %cr0		# Set the PG, EM and PE bits
	
	jmp	protect

#
# 0x00001000 is the entry point for our kernel code - we can't directly
# assemble the code as as86 won't let us perform a 32 bit jump, so we
# insert the byte code instead.
#
protect:
	.byte	0x66, 0x67, 0xea
	.long	0x00001000
	.word	0x0010

#
# Data definitions
#
gdt:	.word	0x0000			# Dummy entry
	.word	0x0000
	.word	0x0000
	.word	0x0000

#
# Kernel data descriptor
#
	.word	0xffff			# 4Gb - limit
	.word	0x0000			# Base address = 0
	.word	0x9200			# Data read/write
	.word	0x00cf			# Granularity = 4096, 386

#
# Kernel text (code) descriptor
#
	.word	0xffff			# 4Gb - limit
	.word	0x0000			# Base address= 0
	.word	0x9a00			# Code read/exec
	.word	0x00cf			# Granularity= 4096, 386

#
# Data used to load the GDT and IDT registers
#
setup_gdt_48:
	.word	0x0030			# Gdt limit = 48, 6 GDT entries
	.word	gdt + 0x8000, 0x9	# Gdt base = 0X9xxxx

setup_idt_48:
	.word	0			# Idt limit = 0
	.word	0, 0			# Idt base = 0

#
# That's All Folks!
#
