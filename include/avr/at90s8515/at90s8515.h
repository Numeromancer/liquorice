/*
 * avr/at90s8515/at90s8515.h
 *	AT90S8515 I/O definitions.
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
 * Microcontroller registers addresses.
 *
 * Wherever possible we use the Atmel datasheet's naming.
 */
#define ACSR 0x08			/* Analog comparator control and status register */
#define UBRR 0x09			/* UART baud rate register */
#define UCR 0x0a			/* UART control register */
#define USR 0x0b			/* UART status register */
#define UDR 0x0c			/* UART I/O data register */
#define SPCR 0x0d			/* SPI control register */
#define SPSR 0x0e			/* SPI status register */
#define SPDR 0x0f			/* SPI data register */
#define PIND 0x10			/* Port D input pins */
#define DDRD 0x11			/* Port D data direction register */
#define PORTD 0x12			/* Port D data register */
#define PINC 0x13			/* Port C input pins */
#define DDRC 0x14			/* Port C data direction register */
#define PORTC 0x15			/* Port C data register */
#define PINB 0x16			/* Port B input pins */
#define DDRB 0x17			/* Port B data direction register */
#define PORTB 0x18			/* Port B data register */
#define PINA 0x19			/* Port A input pins */
#define DDRA 0x1a			/* Port A data direction register */
#define PORTA 0x1b			/* Port A data register */
#define EECR 0x1c			/* EEPROM control register */
#define EEDR 0x1d			/* EEPROM data register */
#define EEARL 0x1e			/* EEPROM address register (low) */
#define EEARH 0x1f			/* EEPROM address register (high) */
#define WDTCR 0x21			/* Watchdog timer control register */
#define ICR1L 0x24			/* Timer/counter1 input capture register (low) */
#define ICR1H 0x25			/* Timer/counter1 input capture register (high) */
#define OCR1BL 0x28			/* Timer/counter1 output compare register B (low) */
#define OCR1BH 0x29			/* Timer/counter1 output compare register B (high) */
#define OCR1AL 0x2a			/* Timer/counter1 output compare register A (low) */
#define OCR1AH 0x2b			/* Timer/counter1 output compare register A (high) */
#define TCNT1L 0x2c			/* Timer/counter1 counter register (low) */
#define TCNT1H 0x2d			/* Timer/counter1 counter register (high) */
#define TCCR1B 0x2e			/* Timer/counter1 control register B */
#define TCCR1A 0x2f			/* Timer/counter1 control register A */
#define TCNT0 0x32			/* Timer/counter0 counter register */
#define TCCR0 0x33			/* Timer/counter0 control register */
#define MCUCR 0x35			/* MCU control register */
#define TIFR 0x38			/* Timer/counter interrupt flag register */
#define TIMSK 0x39			/* Timer/counter interrupt mask register */
#define GIFR 0x3a			/* General interrupt flag register */
#define GIMSK 0x3b			/* General interrupt mask register */
#define SPL 0x3d			/* Stack pointer (low) */
#define SPH 0x3e			/* Stack pointer (high) */
#define SREG 0x3f			/* Status register */

/*
 * Analog comparator control and status register.
 */
#define ACIS0 0
#define ACIS1 1
#define ACIC 2
#define ACIE 3
#define ACI 4
#define ACO 5
#define ACD 7

/*
 * UART control register.
 */
#define TXB8 0
#define RXB8 1
#define CHR9 2
#define TXEN 3
#define RXEN 4
#define UDRIE 5
#define TXCIE 6
#define RXCIE 7

/*
 * UART status register.
 */
#define OVR 3
#define FE 4
#define UDRE 5
#define TXC 6
#define RXC 7

/*
 * SPI control register.
 */
#define SPR0 0
#define SPR1 1
#define CPHA 2
#define CPOL 3
#define MSTR 4
#define DORD 5
#define SPE 6
#define SPIE 7

/*
 * SPI Status Register.
 */
#define WCOL 6
#define SPIF 7

/*
 * Port D input pins.
 */
#define PIND7 7
#define PIND6 6
#define PIND5 5
#define PIND4 4
#define PIND3 3
#define PIND2 2
#define PIND1 1
#define PIND0 0

/*
 * Port D data direction register.
 */
#define DDD0 0
#define DDD1 1
#define DDD2 2
#define DDD3 3
#define DDD4 4
#define DDD5 5
#define DDD6 6
#define DDD7 7

/*
 * Port D data register.
 */
#define PD0 0
#define PD1 1
#define PD2 2
#define PD3 3
#define PD4 4
#define PD5 5
#define PD6 6
#define PD7 7

/*
 * Port C input pins.
 */
#define PINC7 7
#define PINC6 6
#define PINC5 5
#define PINC4 4
#define PINC3 3
#define PINC2 2
#define PINC1 1
#define PINC0 0

/*
 * Port C data direction register.
 */
#define DDC0 0
#define DDC1 1
#define DDC2 2
#define DDC3 3
#define DDC4 4
#define DDC5 5
#define DDC6 6
#define DDC7 7

/*
 * Port C data register.
 */
#define PC0 0
#define PC1 1
#define PC2 2
#define PC3 3
#define PC4 4
#define PC5 5
#define PC6 6
#define PC7 7

/*
 * Port B input pins.
 */
#define PINB0 0
#define PINB1 1
#define PINB2 2
#define PINB3 3
#define PINB4 4
#define PINB5 5
#define PINB6 6
#define PINB7 7

/*
 * Port B data direction register.
 */
#define DDB0 0
#define DDB1 1
#define DDB2 2
#define DDB3 3
#define DDB4 4
#define DDB5 5
#define DDB6 6
#define DDB7 7

/*
 * Port B data register.
 */
#define PB0 0
#define PB1 1
#define PB2 2
#define PB3 3
#define PB4 4
#define PB5 5
#define PB6 6
#define PB7 7

/*
 * Port A input pins.
 */
#define PINA0 0
#define PINA1 1
#define PINA2 2
#define PINA3 3
#define PINA4 4
#define PINA5 5
#define PINA6 6
#define PINA7 7

/*
 * Port A data direction register.
 */
#define DDA0 0
#define DDA1 1
#define DDA2 2
#define DDA3 3
#define DDA4 4
#define DDA5 5
#define DDA6 6
#define DDA7 7

/*
 * Port A data register.
 */
#define PA0 0
#define PA1 1
#define PA2 2
#define PA3 3
#define PA4 4
#define PA5 5
#define PA6 6
#define PA7 7

/*
 * EEPROM control register.
 */
#define EERE 0
#define EEWE 1
#define EEMWE 2

/*
 * Watchdog timer control register.
 */
#define WDP0 0
#define WDP1 1
#define WDP2 2
#define WDE 3
#define WDTOE 4

/*
 * Timer/counter 1 control and status register.
 */
#define CS10 0
#define CS11 1
#define CS12 2
#define CTC1 3
#define ICES1 6
#define ICNC1 7

/*
 * Timer/counter1 control register.
 */
#define PWM10 0
#define PWM11 1
#define COM1B0 4
#define COM1B1 5
#define COM1A0 6
#define COM1A1 7

/*
 * Timer/counter0 control register.
 */
#define CS00 0
#define CS01 1
#define CS02 2

/*
 * MCU control register.
 */
#define ISC00 0
#define ISC01 1
#define ISC10 2
#define ISC11 3
#define SM 4
#define SE 5
#define SRW 6
#define SRE 7

/*
 * Timer/Counter interrupt flag register.
 */
#define TOV0 1
#define ICF1 3
#define OCF1B 5
#define OCF1A 6
#define TOV1 7

/*
 * Timer/counter interrupt mask register.
 */
#define TOIE0 1
#define TICIE1 3
#define OCIE1B 5
#define OCIE1A 6
#define TOIE1 7

/*
 * General interrupt flag register.
 */
#define INTF0 6
#define INTF1 7

/*
 * General interrupt mask register.
 */
#define INT0 6
#define INT1 7

/*
 * Pointer definition.
 */
#define XL r26
#define XH r27
#define YL r28
#define YH r29
#define ZL r30
#define ZH r31

/*
 * Constants.
 */
#define RAMSTART 0x0060			/* First on-chip SRAM address */
#define RAMEND 0x01FF			/* Last on-chip SRAM address */
#define XRAMSTART 0x0200		/* First off-chip SRAM address */
#define XRAMEND 0xFFFF			/* Last off-chip SRAM address */
#define E2END 0x01FF			/* Last EEPROM address */
#define FLASHEND 0x0FFFF		/* Last code-space address */
 
/*
 * get_stack_pointer()
 */
extern inline addr_t get_stack_pointer(void)
{
	addr_t sp;
	
	asm volatile ("in %A0, %1\n\t"
			"in %B0, %2\n\t"
			: "=r" (sp)
			: "I" (SPL), "I" (SPH));

	return sp;
}

/*
 * Speed of the CPU in Hz.
 */
#define CPU_SPEED 4000000
