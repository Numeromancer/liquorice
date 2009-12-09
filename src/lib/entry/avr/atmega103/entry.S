/*
 * entry/avr/atmega103/entry.S
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

 	.arch	atmega103
	.section	.init
	.func	.start_of_init

.start_of_init:
	jmp	_start
	jmp	_interrupt0_
	jmp	_interrupt1_
	jmp	_interrupt2_
	jmp	_interrupt3_
	jmp	_interrupt4_
	jmp	_interrupt5_
	jmp	_interrupt6_
	jmp	_interrupt7_
	jmp	_output_compare2_
	jmp	_timer2_overflow_
	jmp	_input_capture1_
	jmp	_output_compare1a_
	jmp	_output_compare1b_
	jmp	_timer1_overflow_
	jmp	_output_compare0_
	jmp	_timer0_overflow_
	jmp	_spi_
	jmp	_uart_recv_
	jmp	_uart_data_
	jmp	_uart_trans_
	jmp	_adc_
	jmp	_eeprom_ready_
	jmp	_comparator_

/*
 * _start
 *	Initial code entry point from a CPU reset.
 */
_start:
	clr	r1			; Reset the flags
	out	0x3f, r1
	
	ldi	r16, 0xc0		; Set for external RAM enabled and
	out	0x35, r16		; a wait state
	
	ldi	r30, lo8(_etext)	; Load from the end of .text segment
	ldi	r31, hi8(_etext)	; at it is the start of the .data image
	ldi	r16, hh8(_etext)
	ldi	r26, lo8(0x0060)	; Store from the start of RAM
	ldi	r27, hi8(0x0060)
	ldi	r28, lo8(_edata - 0x0060)
					; Size of .data image - we assume that
	ldi	r29, hi8(_edata - 0x0060)
					; we have more than zero bytes to copy!

1:
	out	0x3b, r16
	elpm
	adiw	r30, 1
	adc	r16, r1
	st	X+, r0
	sbiw	r28, 1
	brne	1b

	ldi	r26, lo8(__bss_start)
	ldi	r27, hi8(__bss_start)
	ldi	r16, hi8(__bss_end)
	rjmp	2f

.clr_bss:
	st	X+, r1

2:
	cpi	r26, lo8(__bss_end)
	cpc	r27, r16
	brne	.clr_bss

	jmp	main
	
	.endfunc
	
.section .text
	.func	.weaks			; Keep avr-as happy with -gstabs
.weaks:
	.weak	_interrupt0_
	.weak	_interrupt1_
	.weak	_interrupt2_
	.weak	_interrupt3_
	.weak	_interrupt4_
	.weak	_interrupt5_
	.weak	_interrupt6_
	.weak	_interrupt7_
	.weak	_output_compare2_
	.weak	_timer2_overflow_
	.weak	_input_capture1_
	.weak	_output_compare1a_
	.weak	_output_compare1b_
	.weak	_timer1_overflow_
	.weak	_output_compare0_
	.weak	_timer0_overflow_
	.weak	_spi_
	.weak	_uart_recv_
	.weak	_uart_data_
	.weak	_uart_trans_
	.weak	_adc_
	.weak	_eeprom_ready_
	.weak	_comparator_

_interrupt0_:
_interrupt1_:
_interrupt2_:
_interrupt3_:
_interrupt4_:
_interrupt5_:
_interrupt6_:
_interrupt7_:
_output_compare2_:
_timer2_overflow_:
_input_capture1_:
_output_compare1a_:
_output_compare1b_:
_timer1_overflow_:
_output_compare0_:
_timer0_overflow_:
_spi_:
_uart_recv_:
_uart_data_:
_uart_trans_:
_adc_:
_eeprom_ready_:
_comparator_:
_unexpected_:
	ldi	r20, 0x55
	out	24, r20
1:
	jmp	1b

	.global	startup_continue

startup_continue:
	pop	r31
	pop	r30
	pop	r27
	pop	r26
	pop	r25
	pop	r24
	pop	r23
	pop	r22
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r0
	out	0x3b, r0
	pop	r0
	out	0x3f, r0
	pop	r1
	pop	r0
	reti
