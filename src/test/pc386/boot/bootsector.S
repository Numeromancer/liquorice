#
# bootsector.S
#	Bootstrap loaders for a floppy diskette drive
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
# Some of the code written here was originally based on that written (and
# GPL'd) by Linus Torvalds <torvalds@transmeta.com> as part of the Linux
# operating system.  It is also placed under a GPL, but as it is not linked
# with any of the Liquorice libraries this does not cause any confict.
#
# This code represents a boot sector.  It is normally loaded by the system
# ROM (PC BIOS) at 0x07c0:0x0000.  If there's enough base memory (below 1
# MByte) the code is relocated to 0x90000 and executed from there.
#
# This code is designed to be pretty simple - it's prime function is to
# load the setup routines that will set the system into a state where it
# can enter the CPU's protected mode, and to load the boot time kernel code.
#
# There's one little bit of this file that I'm especially fond of - it's
# the code used to create a hex printable character.  I first came across
# it in 1980 on an 8080 based system and it's been my all-time favourite
# coding trick ever since :-)
#

#include "bootconst.h"

	.code16
	
	.text

#
# ld86 requires an entry symbol. This may as well be "_main" - in keeping
# with convention :-)
#
	.globl	_main

_main:
	movw	$0x07C0, %ax		# Move the boot code from where it is
	movw	%ax, %ds		# to where it wants to be!  As the Linux
	movw	$0x9000, %ax		# LILO source code says - beam me up :-)
	movw	%ax, %es
	movw	$256, %cx
	xorw	%si, %si
	xorw	%di, %di
	cld
	rep
	movsw
	ljmp	$0x9000, $go		# Switch execution to the new location

go:
	movw	%cs, %ax		# Establish addressing
	movw	$0x4000, %dx		# 0x4000 is arbitrary value >= length
					# of bootsect + length of setup + room
					# for stack
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss		# Put stack at INITSEG:0x4000
	movw	%dx, %sp

#
# Many BIOS's default disk parameter tables will not recognize multi-sector
# reads beyond the maximum sector number specified in the default diskette
# parameter tables - this may mean 7 sectors in some cases.  Since single
# sector reads are slow and out of the question, we must take care of this by
# creating new parameter tables (for the first disk) in RAM.  We will set the
# maximum sector count to 36 - the most we will encounter on an 2.88 MByte
# Extra Density floppy (were much less concerned about booting from anything
# faster like a hard drive.
#
# High values don't hurt.  Low ones do!
#
	pushw	$0			# Establish fs:bx = 0x0000:0x0078
					# - the parameter table
	popw	%fs
	movw	$0x0078, %bx
	push	%ds
	ldsw	%fs:(%bx), %si		# ds:si is the source address
	movw	%dx, %di		# es:di is the destination
	movw	$6, %cx			# Copy 6 words (12 bytes)
	rep
	movsw

	pop	%ds
	movw	%dx, %di
	movb	$36, 4(%di)		# Patch the sector count

	movw	%di, %fs:(%bx)		# Point the BIOS at the new param table
	movw	%es, %fs:2(%bx)

	movw	%cs, %ax		# ds = es = fs = gs = cs
	movw	%ax, %fs
	movw	%ax, %gs
	
disk_reset:
	xorb	%ah, %ah		# Reset the disk controller
	xorb	%dl, %dl
	int 	$0x13	

#
# load the setup-sectors directly after the bootblock.
#
load_setup:
	movb	drive, %dl		# Select the boot drive
	xorb	%dh, %dh		# Select head 0
	movw	$0x0002, %cx		# Select sector 2, track 0
	movw	$0x8000, %bx		# Address = 0x8000, in INITSEG
	movw	$0x0200 + 4, %ax	# Service 2, nr of sectors (assume all
					# on head 0, track 0)
	int	$0x13			# Read it
	jnc	ok_load_setup		# If all's well, continue

	jmp	disk_reset		# Re-reset the drive and try again!

#
# Get disk drive parameters, specifically nr of sectors/track.  It seems that
# there is no reliable BIOS call to get the number of sectors.  Guess that 18
# sectors if sector 18 can be read, 15 if sector 15 can be read.  Otherwise
# guess 9.
#
ok_load_setup:
	movb	drive, %dl		# Select the boot drive
	xorb	%dh, %dh		# Select head 0
	movw	$0x0012, %cx		# Try sector 18, track 0
	movw	$0x8000 + 0x800, %bx	# address after setup (es = cs)
	movw	$0x0201, %ax		# Read 1 sector
	int	$0x13

	jnc	got_sectors		# If there's no fail we have 18 sectors
					# per track!

	movb	$0x0f, %cl		# Try sector 15
	movw	$0x0201, %ax		# Read 1 sector
	int	$0x13

	jnc	got_sectors		# If there's no fail we have 15 sectors
					# per track

	movb	$0x09, %cl		# Assume 9 otherwise

got_sectors:
	movw	%cx, %cs:sectors
	movw	$0x9000, %ax
	movw	%ax, %es

#
# Print some inane message - "Loading" or something like it :-)
#
	movb	$0x03, %ah		# Read cursor pos
	xorb	%bh, %bh
	int	$0x10
	
	movw	$31, %cx
	movw	$0x0007, %bx		# Page 0, attribute 7 (normal)
	movw	$msg1, %bp
	movw	$0x1301, %ax		# Write string, move cursor
	int	$0x10

#
# OK, we've written the message, now we want to load the system.  We need to
# ensure that no 64k boundaries are crossed, as the PC's FDC is only designed
# to handle 16 bit DMA really!  In order to make things as fast as possible we
# try to load whole tracks in one go.
#
	xorw	%ax, %ax		# This is where we want to read from
	movw	%ax, %es		# es must be at a 64 kByte boundary
	movw	$0x1000, %bx		# bx is starting address within segment

rp_read:
	movw	%es, %ax		# Where's the next effective segment to be
					# written?
	subw	$0x0100, %ax
	movw	%bx, %dx
	shrw	$4, %dx
	addw	%dx, %ax

	pusha				# Preseve status

	movw	$0x0e08, %ax		# Print 8 backspaces

	movw	$8, %cx

bs_loop:
	int	$0x10
	loop	bs_loop

	popa
	pusha

	movw	%ax, %dx		# Print the next sector's physical address
	addw	$0x0100, %dx
	movzwl	%dx, %edx
	shll	$4, %edx
	call	print_hex

	popa

	shrw	$5, %ax			# How many sectors have we loaded so far?
	
	cmpw	sys_size, %ax		# have we loaded all yet?
	jbe	ok1_read

	jmp	stop_motor		# If yes, stop looping and stop the drive
					# motor

ok1_read:
	movw	sectors, %ax		# How man sectors left on this track?
	subw	sread, %ax
	movw	%ax, %cx		# Can we handle that many in this 64k block?
	shlw	$9, %cx
	addw	%bx, %cx
	jnc	ok2_read		# If yes, go on

	je	ok2_read

	xorw	%ax, %ax		# Otherwise work out how man we can do
	subw	%bx, %ax		# before we hit the end of the 64k block
	shrw	$9, %ax

ok2_read:
	call	read_track

	movw	%ax, %cx		# How many sectors have we just got?
	addw	sread, %ax		# And find out how many we've done from
					# this track
	cmpw	%cs:sectors, %ax		# Have we done all of them?
	jne	ok3_read		# If no, jump onwards

	movw	$1, %ax			# Switch head
	subw	head, %ax
	jnz	ok4_read

	incw	track			# If necessary, also step one track on

ok4_read:
	movw	%ax, head		# Save the new "head in use" value
	xorw	%ax, %ax

ok3_read:
	movw	%ax, sread		# Save the count of sectors already read
					# from this track

	shlw	$9, %cx			# Update the destination pointer
	addw	%cx, %bx		# If we've not hit the end of 64k loop round
	jnc	rp_read

	movw	%es, %ax		# Otherwise reset the base segment,
					# and point to 64k futher
	addb	$0x10, %ah		# along the memory map!
	movw	%ax, %es
	xorw	%bx, %bx
	ljmp	$0x9000, $rp_read

stop_motor:
	movw	$0x3f2, %dx		# Turn the floppy drive motor off
	xorb	%al, %al
	outb	%al, %dx

#
# Print another inane message - "completed" - to say that everything's wonderful
#
	movw	$0x9000, %ax
	movw	%ax, %es

	movb	$0x03, %ah		# Read cursor pos
	xorb	%bh, %bh
	int	$0x10
	
	movw	$10, %cx
	movw	$0x0007, %bx		# Page 0, attribute 7 (normal)
	movw	$msg2, %bp
	movw	$0x1301, %ax		# Write string, move cursor
	int	$0x10

#
# After everyting's loaded we jump to the setup-routine loaded directly after
# the bootblock
#
	ljmp	$0x9800, $0


#
# read_track
#	Read the next block of data from the current track.
#
# On entry to the routine, ax holds the number of sectors to be read in
# and es:bx points to where they need to go
#
read_track:
	pusha				# Preserve state

	movw	track, %dx		# Get the current track
	movw	sread, %cx		# Get the number of sectors already
					# read
	incw	%cx			# Plus 1 sets the starting sector
	movb	%dl, %ch		# Set the track number
	movb	head, %dh		# Get the current head
	andb	$0x01, %dh		# Mask the head number
	movb	drive, %dl		# Select the boot drive
	movb	$2, %ah			# Set for a read operation
	
	int	$0x13			# Invoke

	jc	bad_rt			# Any nasty error thingies?  Jump if yes

	popa
	ret				# All done, so quit

bad_rt:
	xorb	%ah, %ah		# Reset the disk drive
	movb	drive, %dl
	int	$0x13
	
	addw	$10, %sp		# Tidy up the error dump
	popa	

	jmp	read_track		# And retry the operation


#
# print_hex
#	Print the hex word in edx
#
print_hex:
	movw	$8, %cx			# 8 hex digits

print_digit:
	roll	$4, %edx		# Rotate so that lowest 4 bits are used
	movb	$0xe, %ah	
	movb	%dl, %al		# Mask off so we have only next nibble
	andb	$0xf, %al
	addb	$0x90, %al		# Make like an 8080 - the old decimal
					# adjust trick :-)
	daa
	adcb	$0x40, %al
	daa
	int	$0x10
	
	loop	print_digit

	ret


#
# A couple of messages
#
msg1:	.byte	13, 10			# A message from our sponsor eh?
	.ascii	"Loading Liquorice: 0x00000000"

msg2:	.ascii	" completed"


#
# Dynamic location data area
#
	.org 0x01f2

sectors:
	.word	0			# Number of diskette sectors per track
sread:	.word	1 + 4			# Sectors read so far (used after boot
					# and setup code loaded)
head:	.word	0			# Head in use
track:	.word	0			# Track in use
drive:	.word	0			# Drive in use
sys_size:
	.word	0x0010			# Size of the image to be loaded
boot_flag:
	.word	0xaa55			# Needed so the BIOS will boot this
					# sector


#
# That's All Folks!
#
