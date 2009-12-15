/*
 * bbi2c.c
 *	Bit-banged I2C support.
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
#include "debug.h"
#include "context.h"
#include "bbi2c.h"

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/bbi2c.h"
#else
#error "no valid architecture found"
#endif

/*
 * Direction control flags for read/write operations
 */
#define I2C_DIR_READ 1
#define I2C_DIR_WRITE 0

/*
 * Acknowledge control flags
 */
#define I2C_ACK_MORE 0
#define I2C_ACK_DONE 1

/*
 * bbi2c_write_byte()
 *	Write a byte onto the I2C bus.
 *
 * On entry to this function SCL should be low and SDA is unknown.
 *
 * Returns 1 if the byte was written and acknowledged or 0 if the
 * acknowledge didn't occur.
 */
static s8_t bbi2c_write_byte(u8_t val)
{
	u8_t mask;
	u8_t r;

	/*
	 * Loop around the 8 bits of the byte we're sending.
	 */
	for (mask = 0x80; mask; mask >>= 1) {
		/*
		 * Assert the SDA bit as required and then pulse SCL.  We
		 * need to leave SCL high for 4 us and low for 4.7 us.
		 */
		bbi2c_write_sda(val & mask);
		bbi2c_short_delay();
		bbi2c_write_scl(1);
		bbi2c_long_delay();
		bbi2c_write_scl(0);
		bbi2c_long_delay();
	}

	/*
	 * Wait for an acknowledge bit and then ensure that the SDA and SCL
	 * lines are set low.
	 */
	bbi2c_write_sda(1);
	bbi2c_short_delay();
	bbi2c_write_scl(1);
	bbi2c_long_delay();
	r = bbi2c_read_sda();
	bbi2c_write_scl(0);
	bbi2c_short_delay();

	return !r;
}

/*
 * bbi2c_read_byte()
 *	Read a byte from the I2C bus
 *
 * On entry to this function SCL should be low and SDA is unknown.
 *
 * When we read from the I2C bus we have to send an acknowledge (low) bit to
 * the device we're talking to unless this is the last transfer before we send
 * a stop sequence.  If it's the last transfer then we leave the SDA line
 * high during the acknowledge clock cycle.
 *
 * Returns the value of the byte read from the I2C bus.
 */
static u8_t bbi2c_read_byte(u8_t ack_bit)
{
	u8_t mask;
	u8_t val = 0;

	/*
	 * Force the data line high so that we can use it as an input.
    	 */
	bbi2c_write_sda(1);
	bbi2c_short_delay();

	/*
	 * We have 8 bits to receive
	 */
	for (mask = 0x80; mask; mask >>= 1) {
		/*
		 * Set SCL high for 4 us and then read the state of SDA to
		 * get the next data bit.  Once we have the data SCL needs
		 * to be low for 4.7 us before we can clock data again.
		 */
		bbi2c_write_scl(1);
		bbi2c_long_delay();
		val |= (bbi2c_read_sda() ? mask : 0);
		bbi2c_write_scl(0);
		bbi2c_long_delay();
	}

	/*
	 * Send the acknowledge bit as specified!
	 */
	bbi2c_write_sda(ack_bit);
	bbi2c_short_delay();
	bbi2c_write_scl(1);
	bbi2c_long_delay();
	bbi2c_write_scl(0);
	bbi2c_short_delay();

	return val;
}

/*
 * bbi2c_start()
 *	Start an I2C operation
 *
 * On entry SCL and SDA are in unknown states.  On exit, both will be low.
 */
static u8_t bbi2c_start(u8_t slave)
{
	bbi2c_write_sda(1);
	bbi2c_short_delay();
	bbi2c_write_scl(1);
	bbi2c_long_delay();

	/*
	 * Drop the SDA bit low whilst maintaining the SCL bit high for
	 * more than 4 us to signal the start sequence (SDA going low whilst
	 * SCL is high).
	 */
	bbi2c_write_sda(0);
	bbi2c_long_delay();
	bbi2c_write_scl(0);
	bbi2c_long_delay();

	/*
	 * Issue the address for the I2C transaction.
	 */
	return bbi2c_write_byte(slave);
}

/*
 * bbi2c_stop()
 *	Stop and I2C operation
 *
 * On entry SCL is low and SDA is unknown.  They will both be high on exit.
 */
static void bbi2c_stop(void)
{	/*
	 * Set SDA low and SCL high (in that order) as a prelude to the
	 * stop sequence (SDA going high whilst SCL is high).  We have to
	 * wait 4 us in this state before raising SDA to complete the stop.
	 */
	bbi2c_write_sda(0);
	bbi2c_short_delay();
	bbi2c_write_scl(1);
	bbi2c_long_delay();
	bbi2c_write_sda(1);
	bbi2c_long_delay();
}

/*
 * bbi2c_read()
 *	Read a block of bytes.
 */
s8_t bbi2c_read(u8_t addr, u8_t offs, u8_t *buf, u8_t count)
{
	u8_t retries;
	
	for (retries = 0; retries < 4; retries++) {
		if (bbi2c_start(addr | I2C_DIR_WRITE)) {
			if (bbi2c_write_byte(offs)) {
				if (bbi2c_start(addr | I2C_DIR_READ)) {
					u8_t i;

					for (i = 0; i < count - 1; i++) {
						*buf++ = bbi2c_read_byte(I2C_ACK_MORE);
					}								
					*buf++ = bbi2c_read_byte(I2C_ACK_DONE);
                                        bbi2c_stop();
										
					return 0;
				}
			}
 		}
		bbi2c_stop();
	}
	
	return -1;
}

/*
 * bbi2c_write()
 *	Write a block of bytes.
 */
s8_t bbi2c_write(u8_t addr, u8_t offs, u8_t *buf, u8_t count)
{
	u8_t retries;
	
	for (retries = 0; retries < 4; retries++) {
		if (bbi2c_start(addr | I2C_DIR_WRITE)) {
			if (bbi2c_write_byte(offs)) {
				u8_t i;
				u8_t ack;
				u8_t *p;
								
				i = 0;
				p = buf;
				do {
					ack = bbi2c_write_byte(*p++);
					i++;
				} while ((i < count) && ack);
				
				if (ack) {
					bbi2c_stop();
					return 0;
				}
			}
 		}
		bbi2c_stop();
	}
	
	return -1;
}
