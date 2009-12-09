/*
 * avr/atmega103/atmega103.h
 *	ATMega103 I/O definitions.
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
#define PINF 0x00			/* Port F input pins */
#define PINE 0x01			/* Port E input pins */
#define DDRE 0x02			/* Port E data direction register */
#define PORTE 0x03			/* Port E data register */
#define ADCL 0x04			/* ADC data register (low) */
#define ADCH 0x05			/* ADC data register (high) */
#define ADCSR 0x06			/* ADC control and status register */
#define ADMUX 0x07			/* ADC multiplexer select register */
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
#define OCR2 0x23			/* Timer/counter2 output compare register */
#define TCNT2 0x24			/* Timer/counter2 counter register */
#define TCCR2 0x25			/* Timer/counter2 control register */
#define ICR1L 0x26			/* Timer/counter1 input capture register (low) */
#define ICR1H 0x27			/* Timer/counter1 input capture register (high) */
#define OCR1BL 0x28			/* Timer/counter1 output compare register B (low) */
#define OCR1BH 0x29			/* Timer/counter1 output compare register B (high) */
#define OCR1AL 0x2a			/* Timer/counter1 output compare register A (low) */
#define OCR1AH 0x2b			/* Timer/counter1 output compare register A (high) */
#define TCNT1L 0x2c			/* Timer/counter1 counter register (low) */
#define TCNT1H 0x2d			/* Timer/counter1 counter register (high) */
#define TCCR1B 0x2e			/* Timer/counter1 control register B */
#define TCCR1A 0x2f			/* Timer/counter1 control register A */
#define ASSR 0x30			/* Asynchronous status register */
#define OCR0 0x31			/* Timer/counter0 output compare register */
#define TCNT0 0x32			/* Timer/counter0 counter register */
#define TCCR0 0x33			/* Timer/counter0 control register */
#define MCUSR 0x34			/* MCU status register */
#define MCUCR 0x35			/* MCU control register */
#define TIFR 0x36			/* Timer/counter interrupt flag register */
#define TIMSK 0x37			/* Timer/counter interrupt mask register */
#define EIFR 0x38			/* External interrupt flag register */
#define EIMSK 0x39			/* External interrupt mask register */
#define EICR 0x3a			/* External interrupt control register */
#define RAMPZ 0x3b			/* RAM page Z select register */
#define XDIV 0x3c			/* XTAL divide control register */
#define SPL 0x3d			/* Stack pointer (low) */
#define SPH 0x3e			/* Stack pointer (high) */
#define SREG 0x3f			/* Status register */

/*
 * Port F input pins.
 */
#define PINF0 0
#define PINF1 1
#define PINF2 2
#define PINF3 3
#define PINF4 4
#define PINF5 5
#define PINF6 6
#define PINF7 7

/*
 * Port E inputs pins.
 */
#define PINE0 0
#define PINE1 1
#define PINE2 2
#define PINE3 3
#define PINE4 4
#define PINE5 5
#define PINE6 6
#define PINE7 7

/*
 * Port E data direction register bits.
 */
#define DDE0 0
#define DDE1 1
#define DDE2 2
#define DDE3 3
#define DDE4 4
#define DDE5 5
#define DDE6 6
#define DDE7 7

/*
 * Port E data register bits.
 */
#define PORTE0 0
#define PORTE1 1
#define PORTE2 2
#define PORTE3 3
#define PORTE4 4
#define PORTE5 5
#define PORTE6 6
#define PORTE7 7

/*
 * ADC multiplexer select.
 */
#define MUX0 0
#define MUX1 1
#define MUX2 2

/*
 * ADC control and status register.
 */
#define ADPS0 0
#define ADPS1 1
#define ADPS2 2
#define ADIE 3
#define ADIF 4
#define ADFR 5
#define ADSC 6
#define ADEN 7

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
#define EERIE 3

/*
 * Watchdog timer control register.
 */
#define WDP0 0
#define WDP1 1
#define WDP2 2
#define WDE 3
#define WDTOE 4

/*
 * Timer/counter2 control register.
 */
#define CS20 0
#define CS21 1
#define CS22 2
#define CTC2 3
#define COM20 4
#define COM21 5
#define PWM2 6

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
 * Timer/counter0 asynchronous control & status register.
 */
#define TCR0UB 0
#define OCR0UB 1
#define TCN0UB 2
#define AS0 3

/*
 * Timer/counter0 control register.
 */
#define CS00 0
#define CS01 1
#define CS02 2
#define CTC0 3
#define COM00 4
#define COM01 5
#define PWM0 6

/*
 * MCU status register.
 */
#define PORF 0
#define EXTRF 1

/*
 * MCU control register.
 */
#define SM0 3
#define SM1 4
#define SE 5
#define SRW 6
#define SRE 7

/*
 * Timer/Counter interrupt flag register.
 */
#define TOV0 0
#define OCF0 1
#define TOV1 2
#define OCF1B 3
#define OCF1A 4
#define ICF1 5
#define TOV2 6
#define OCF2 7

/*
 * Timer/counter interrupt mask register.
 */
#define TOIE0 0
#define OCIE0 1
#define TOIE1 2
#define OCIE1B 3
#define OCIE1A 4
#define TICIE1 5
#define TOIE2 6
#define OCIE2 7

/*
 * External interrupt flag register.
 */
#define INTF4 4
#define INTF5 5
#define INTF6 6
#define INTF7 7

/*
 * External interrupt mask register.
 */
#define INT0 0
#define INT1 1
#define INT2 2
#define INT3 3
#define INT4 4
#define INT5 5
#define INT6 6
#define INT7 7

/*
 * External interrupt control register.
 */
#define ISC40 0
#define ISC41 1
#define ISC50 2
#define ISC51 3
#define ISC60 4
#define ISC61 5
#define ISC70 6
#define ISC71 7

/*
 * RAM Page Z select register.
 */
#define RAMPZ0 0

/*
 * XTAL divide control register.
 */
#define XDIV0 0
#define XDIV1 1
#define XDIV2 2
#define XDIV3 3
#define XDIV4 4
#define XDIV5 5
#define XDIV6 6
#define XDIVEN 7

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
#define RAMEND 0x0FFF			/* Last on-chip SRAM address */
#define XRAMSTART 0x1000		/* First off-chip SRAM address */
#define XRAMEND 0xFFFF			/* Last off-chip SRAM address */
#define E2END 0x0FFF			/* Last EEPROM address */
#define FLASHEND 0x1FFFF		/* Last code-space address */
 
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
