/* Copyright (c) 2002, Marek Michalkiewicz <marekm@amelek.gda.pl>
   Copyright (c) 2007, 2008 Eric B. Weddington
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE. */

/* $Id$ */

        .arch atmega644p
        
#if (__GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#error "GCC version >= 3.3 required"
#endif

#define AVR_STATUS_ADDR             0x3F
#define AVR_STACK_POINTER_HI_ADDR   0x3E
#define AVR_STACK_POINTER_LO_ADDR   0x3D
#define RAMEND                      0x10FF
        
#define __zero_reg__   r1

        
	.macro	vector name
	.weak	\name
	.set	\name, __bad_interrupt
	jmp	\name
	.endm

	.section .vectors,"ax",@progbits
	.global	__vectors
	.func	__vectors
__vectors:
	jmp	__init
	vector	__vector_1
	vector	__vector_2
	vector	__vector_3
	vector	__vector_4
	vector	__vector_5
	vector	__vector_6
	vector	__vector_7
	vector	__vector_8
	vector	__vector_9
	vector	__vector_10
	vector	__vector_11
	vector	__vector_12
	vector	__vector_13
	vector	__vector_14
	vector	__vector_15
	vector	__vector_16
	vector	__vector_17
	vector	__vector_18
	vector	__vector_19
	vector	__vector_20
	vector	__vector_21
	vector	__vector_22
	vector	__vector_23
	vector	__vector_24
	vector	__vector_25
	vector	__vector_26
	vector	__vector_27
	vector	__vector_28
	vector	__vector_29
	vector	__vector_30
	vector	__vector_31
	.endfunc

	/* Handle unexpected interrupts (enabled and no handler), which
	   usually indicate a bug.  Jump to the __vector_default function
	   if defined by the user, otherwise jump to the reset address.

	   This must be in a different section, otherwise the assembler
	   will resolve "rjmp" offsets and there will be no relocs.  */

	.text
	.global	__bad_interrupt
	.func	__bad_interrupt
__bad_interrupt:
	.weak	__vector_default
	.set	__vector_default, __vectors
	jmp	__vector_default
	.endfunc

	.section .init0,"ax",@progbits
	.weak	__init
;	.func	__init
__init:

	.weak	__stack

	/* By default, malloc() uses the current value of the stack pointer
	   minus __malloc_margin as the highest available address.

	   In some applications with external SRAM, the stack can be below
	   the data section (in the internal SRAM - faster), and __heap_end
	   should be set to the highest address available for malloc().  */
	.weak	__heap_end
	.set	__heap_end, 0

	.section .init2,"ax",@progbits
	clr	__zero_reg__
	out	AVR_STATUS_ADDR, __zero_reg__
	ldi	r28,lo8(__stack)
	ldi	r29,hi8(__stack)
	out	AVR_STACK_POINTER_HI_ADDR, r29
	out	AVR_STACK_POINTER_LO_ADDR, r28

	.set	__stack, RAMEND

	.section .init9,"ax",@progbits
	call	main
	jmp	exit
;	.endfunc

