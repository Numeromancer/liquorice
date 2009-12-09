/*
 * i82595.c
 *	Intel 82595 Ethernet controller support.
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
#include "isr.h"
#include "debug.h"
#include "context.h"
#include "thread.h"
#include "membuf.h"
#include "netbuf.h"
#include "pic.h"
#include "ethdev.h"
#include "i82595.h"


#if defined(I386)
#include "i386/i82595.h"
#else
#error "no valid architecture found"
#endif

#include "ethernet.h"

/*
 * Command register is visible on all pages.
 */
#define I82595_CMD 0x00

/*
 * Page 0 registers.
 */
#define I82595_PG0_STATUS 0x01
#define I82595_PG0_ID 0x02
#define I82595_PG0_INT_MASK 0x03
#define I82595_PG0_RCV_BARL 0x04
#define I82595_PG0_RCV_BARH 0x05
#define I82595_PG0_RCV_STOPL 0x06
#define I82595_PG0_RCV_STOPH 0x07
#define I82595_PG0_XMT_BARL 0x0a
#define I82595_PG0_XMT_BARH 0x0b
#define I82595_PG0_HOST_ADDRL 0x0c
#define I82595_PG0_HOST_ADDRH 0x0d
#define I82595_PG0_IOPORT 0x0e

/*
 * Page 1 registers.
 */
#define I82595_PG1_REG1 0x01
#define I82595_PG1_REG2 0x02
#define I82595_PG1_RCV_LOWER_LIM 0x08
#define I82595_PG1_RCV_UPPER_LIM 0x09
#define I82595_PG1_XMT_LOWER_LIM 0x0a
#define I82595_PG1_XMT_UPPER_LIM 0x0b

/*
 * Page 2 registers.
 */
#define I82595_PG2_REG1 0x01
#define I82595_PG2_REG2 0x02
#define I82595_PG2_REG3 0x03
#define I82595_PG2_IADDR0 0x04
#define I82595_PG2_IADDR1 0x05
#define I82595_PG2_IADDR2 0x06
#define I82595_PG2_IADDR3 0x07
#define I82595_PG2_IADDR4 0x08
#define I82595_PG2_IADDR5 0x09
#define I82595_PG2_EEPROM 0x0a

/*
 * Bit fields for page 2's EEPROM register.
 */
#define EEPROM_EESK 0
#define EEPROM_EECS 1
#define EEPROM_EEDI 2
#define EEPROM_EEDO 3

/*
 * Send state information.
 */
static struct context *dev_isr_ctx;
static struct lock dev_isr_lock;
static struct lock dev_lock;
static struct netbuf volatile *send_queue = NULL;
static volatile u8_t tx_available = 1;
static u8_t next_rxl = 0;
static u8_t next_rxh = 0;

/*
 * trap_irq5()
 */
#if defined(DJHEEP)
void trap_irq5(void)
{
	debug_set_lights(0x07);

	isr_spinlock_lock(&dev_isr_lock);

	pic_acknowledge(5);
	
	debug_check_stack(0x100);
	
	isr_context_signal(dev_isr_ctx);

	isr_spinlock_unlock(&dev_isr_lock);
}
#endif

/*
 * i82595_recv_get_packet()
 *	Fetch the next packet out of the receive ring buffer.
*/
static struct netbuf *i82595_recv_get_packet(void)
{
	u8_t d;
	u8_t event;
	u16_t status;
	u8_t nextpl, nextph;
        struct netbuf *nb = NULL;
        		
	/*
	 * Point at the next received packet slot.
	 */
        i82595_write8(I82595_PG0_HOST_ADDRL, next_rxl);
        i82595_write8(I82595_PG0_HOST_ADDRH, next_rxh);
	 	
	/*
	 * Get the receive event.
	 */
	event = i82595_read8(I82595_PG0_IOPORT);
	if (event != 0x08) {
		return NULL;
	}
	
	/*
	 * Read an empty slot.
	 */
	d = i82595_read8(I82595_PG0_IOPORT + 1);

	/*
	 * Get the status word.
	 */
	status = i82595_read16(I82595_PG0_IOPORT);

	/*
	 * Get the pointer to the next frame.
	 */
	nextpl = i82595_read8(I82595_PG0_IOPORT);
        nextph = i82595_read8(I82595_PG0_IOPORT + 1);
			
        if ((status & 0x2d81) == 0x2000) {
	        u8_t *pkt;
	        u16_t *buf;
	        u16_t words;
        	u16_t pktsz;
		
		/*
		 * Get the frame size.
		 */
		pktsz = i82595_read16(I82595_PG0_IOPORT);

		pkt = (u8_t *)membuf_alloc(pktsz, NULL);
	        buf = (u16_t *)pkt;
	
		/*
		 * Read the packet.
		 */
		words = (pktsz + 1) >> 1;
		while (words) {
			*buf++ = i82595_read16(I82595_PG0_IOPORT);
			words--;
		}
		
        	nb = netbuf_alloc();
		nb->nb_datalink_membuf = pkt;
		nb->nb_datalink = pkt;
		nb->nb_datalink_size = pktsz;
        }

	next_rxl = nextpl;
	next_rxh = nextph;

	/*
	 * Update the stop pointer.
	 */
	if (nextpl <= 0x02) {
		if (nextph == 0x00) {
			nextph = 0x60;
		}
		nextph--;
	}
        nextpl -= 2;
				 	
	i82595_write8(I82595_PG0_RCV_STOPL, nextpl);
	i82595_write8(I82595_PG0_RCV_STOPH, nextph);

        return nb;
}

/*
 * i82595_send_set_packet()
 */
void i82595_send_set_packet(struct netbuf *nb)
{
	u16_t words;
        u16_t *buf;
        u16_t sz;
	u8_t padding = 0;
        u16_t chptr;

        /*
         * Work out how many bytes we're actually going to be sending!
         */
	sz = nb->nb_datalink_size + nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size;
        		
	if (sz < 60) {
		padding = (u8_t)(60 - sz);
		sz = 60;
	}

	i82595_write16(I82595_PG0_HOST_ADDRL, 0x6000);

	i82595_write8(I82595_PG0_IOPORT, 0x04);
	i82595_write8(I82595_PG0_IOPORT + 1, 0x00);

	i82595_write16(I82595_PG0_IOPORT, 0x0000);

	chptr = 0x6000 + ((sz + 1) & 0xfffe) + 8;
	i82595_write16(I82595_PG0_IOPORT, chptr);
		
	i82595_write16(I82595_PG0_IOPORT, sz);
			
	/*
	 * Write out the different sections of the network buffer.
	 */
	words = nb->nb_datalink_size >> 1;
	buf = nb->nb_datalink;
	while (words) {
		i82595_write16(I82595_PG0_IOPORT, *buf++);
		words--;
	}
		
	words = nb->nb_network_size >> 1;
	buf = nb->nb_network;
	while (words) {
		i82595_write16(I82595_PG0_IOPORT, *buf++);
		words--;
	}
		
	words = nb->nb_transport_size >> 1;
	buf = nb->nb_transport;
	while (words) {
		i82595_write16(I82595_PG0_IOPORT, *buf++);
		words--;
	}
		
	words = nb->nb_application_size >> 1;
	buf = nb->nb_application;
	while (words) {
		i82595_write16(I82595_PG0_IOPORT, *buf++);
		words--;
	}
        if (nb->nb_application_size & 0x1) {
		i82595_write16(I82595_PG0_IOPORT, (u16_t)(*(u8_t *)buf));
        }

	/*
	 * Pad the transmission if it's not big enough.  Note that we're
	 * assuming that if the packet size wasn't a multiple of 16 bits we
	 * will have made up the odd byte when writing the application buffer.
	 */
	words = padding >> 1;
	while (words) {
		i82595_write16(I82595_PG0_IOPORT, 0x00);
		words--;
	}
	
	i82595_write16(I82595_PG0_IOPORT, 0x0000);

	i82595_write16(I82595_PG0_XMT_BARL, 0x6000);

	i82595_write8(I82595_CMD, 0x04);
	
	tx_available = 0;
}

/*
 * i82595_intr_thread()
 */
void i82595_intr_thread(void *arg) __attribute__ ((noreturn));
void i82595_intr_thread(void *arg)
{
	struct ethdev_instance *edi;
	
	edi = (struct ethdev_instance *)arg;

	dev_isr_ctx = current_context;

	pic_enable(5);

	while (1) {
		u8_t stat;
                				
		isr_disable();
		debug_set_lights(0x09);
		isr_spinlock_lock(&dev_isr_lock);

		stat = i82595_read8(I82595_PG0_STATUS);
		while ((stat & 0x06) == 0x00) {
			isr_context_wait(&dev_isr_lock);
			stat = i82595_read8(I82595_PG0_STATUS);
		}
		
		isr_spinlock_unlock(&dev_isr_lock);
		isr_enable();
		
		spinlock_lock(&dev_lock);
		
		if (stat & 0x02) {
			struct netbuf *nb;
        		struct ethdev_client *edc;
			
        		edc = edi->edi_client;	        		
			nb = i82595_recv_get_packet();
			if (nb) {
				if (edc) {
					ethdev_client_ref(edc);
					spinlock_unlock(&dev_lock);
					edc->edc_recv(edc, nb);
				        spinlock_lock(&dev_lock);
	 				ethdev_client_deref(edc);
				}
				netbuf_deref(nb);
			} else {
				/*
				 * We reset the ISR flag when we've finished
				 * all of the pending receives.  This saves us
				 * being hit with new interrupts while we're
				 * still actively processing them.
				 */
				i82595_write8(I82595_PG0_STATUS, 0x02);
			}
		}
		
		if (stat & 0x04) {
			/*
			 * See if there's any more transmits pending.
			 */
			tx_available = 1;
			
	                if (send_queue) {
				struct netbuf *nb;
			
				nb = (struct netbuf *)send_queue;
				send_queue = nb->nb_next;
				i82595_send_set_packet(nb);
				
				netbuf_deref(nb);
			}

			/*
			 * Acknowledge the TX interrupt.
			 */
			i82595_write8(I82595_PG0_STATUS, 0x04);
		}
					
		if (stat & 0x09) {
			i82595_write8(I82595_PG0_STATUS, 0x09);
		}
	
		spinlock_unlock(&dev_lock);
	}
}

/*
 * i82595_send_netbuf()
 */
void i82595_send_netbuf(struct netbuf *nb)
{
	struct netbuf *p, **pprev;
		
	spinlock_lock(&dev_lock);
		
	if (tx_available) {
		i82595_send_set_packet(nb);
	} else {
		pprev = (struct netbuf **)&send_queue;
		p = (struct netbuf *)send_queue;
	
		while (p) {
			pprev = &p->nb_next;
			p = p->nb_next;
		}

		*pprev = nb;
		nb->nb_next = NULL;
	}
	
	spinlock_unlock(&dev_lock);
}

/*
 * short_pause()
 */
static void short_pause(void)
{
	int x;
	
	for (x = 0; x < 300000; x++);
}

/*
 * eeprom_read()
 */
static u16_t eeprom_read(u8_t addr)
{
	u16_t ret = 0;
	s8_t i;
	u16_t rd = (6 << 6) | addr;
	
	/*
	 * Select register page 2.
	 */
	i82595_write8(I82595_CMD, 0x80);
	i82595_write8(I82595_PG2_EEPROM, BV(EEPROM_EECS));

	/*
	 * Sequence out the read command.
	 */
	for (i = 8; i >= 0; i--) {
		u8_t ov = (rd & (1 << i)) ? (BV(EEPROM_EECS) | BV(EEPROM_EEDI)) : (BV(EEPROM_EECS));
	
		i82595_write8(I82595_PG2_EEPROM, ov);
		i82595_write8(I82595_PG2_EEPROM, ov | BV(EEPROM_EESK));
		short_pause();
		i82595_write8(I82595_PG2_EEPROM, ov);
		short_pause();
	}
	i82595_write8(I82595_PG2_EEPROM, BV(EEPROM_EECS));

	/*
	 * Sequence in the word of data.
	 */
	for (i = 16; i > 0; i--) {
		i82595_write8(I82595_PG2_EEPROM, BV(EEPROM_EECS) | BV(EEPROM_EESK));
		short_pause();
		ret = (ret << 1) | (i82595_read8(I82595_PG2_EEPROM) & BV(EEPROM_EEDO) ? 1 : 0);
		i82595_write8(I82595_PG2_EEPROM, BV(EEPROM_EECS));
		short_pause();
	}

	/*
	 * Terminate and restore page 0.
	 */
	i82595_write8(I82595_PG2_EEPROM, BV(EEPROM_EESK));
	short_pause();
	i82595_write8(I82595_PG2_EEPROM, 0x00);
	short_pause();
	i82595_write8(I82595_CMD, 0x00);
	
	return ret;
}

/*
 * i82595_init()
 */
void i82595_init(struct ethdev_instance *edi)
{
	u8_t id;
        u8_t haddr[6];
        u16_t irq_map;
        u16_t *p;
	u8_t i;

	/*
	 * Read the ID register.
	 */
	id = i82595_read8(I82595_PG0_ID) & 0x2c;

	/*
	 * Read the host address in reverse order!
	 */
	p = (u16_t *)&haddr[0];
	*p++ = eeprom_read(2);
	*p++ = eeprom_read(3);
	*p++ = eeprom_read(4);
        irq_map = eeprom_read(7);
	
	/*
	 * Send a reset command.
	 */
	i82595_write8(I82595_CMD, 0x0e);
	short_pause();
	
	/*
	 * Ensure that TurnOff Enable bit is disabled.
	 */
	i82595_write8(I82595_CMD, 0x80);
	i82595_write8(I82595_PG2_EEPROM, i82595_read8(I82595_PG2_EEPROM) & 0xef);
	
	/*
	 * Write it out to the IADDR registers.
	 */
	i82595_write8(I82595_PG2_IADDR0, haddr[5]);
	i82595_write8(I82595_PG2_IADDR1, haddr[4]);
	i82595_write8(I82595_PG2_IADDR2, haddr[3]);
	i82595_write8(I82595_PG2_IADDR3, haddr[2]);
	i82595_write8(I82595_PG2_IADDR4, haddr[1]);
	i82595_write8(I82595_PG2_IADDR5, haddr[0]);
	for (i = 0; i < 6; i++) {
		edi->edi_mac[i] = haddr[5 - i];
	}

	/*
	 * Establish transmit chaining and discards for bad received data.
	 */
	i82595_write8(I82595_PG2_REG1, 0xe0);

	/*
	 * Match broadcasts.
	 */
	i82595_write8(I82595_PG2_REG2, i82595_read8(I82595_PG2_REG2) | 0x14);

	/*
	 * Clear test mode.
	 */
	i82595_write8(I82595_PG2_REG3, i82595_read8(I82595_PG2_REG3) & 0x3f);
	
	/*
	 * Set the interrupt vector to IRQ3.
	 */
	i82595_write8(I82595_CMD, 0x40);
        i82595_write8(I82595_PG1_REG2, (i82595_read8(I82595_PG1_REG2) & 0xf0) | /* 0x08 */ 0x0a);
	
	/*
	 * Initialize the receive and transmit upper and lower limits.
	 */
	i82595_write8(I82595_PG1_RCV_LOWER_LIM, 0x00);
	i82595_write8(I82595_PG1_RCV_UPPER_LIM, 0x5f);
	i82595_write8(I82595_PG1_XMT_LOWER_LIM, 0x60);
	i82595_write8(I82595_PG1_XMT_UPPER_LIM, 0x7f);

	/*
	 * Enable the interrupt line.
	 */
	i82595_write8(I82595_PG1_REG1, i82595_read8(I82595_PG1_REG1) | 0x80);
		
	/*
	 * Allow receive and transmit interrupts through.
	 */
	i82595_write8(I82595_CMD, 0x00);
	i82595_write8(I82595_PG0_INT_MASK, 0x09);
	
	/*
	 * Clear any pending interrupts.
	 */
	i82595_write8(I82595_PG0_STATUS, 0x0f);

	/*
	 * Initialize receive ring buffer.
	 */
	i82595_write16(I82595_PG0_RCV_BARL, 0x0000);
	i82595_write16(I82595_PG0_RCV_STOPL, 0x5ffe);

	/*
	 * Initialize transmit details.
	 */
	i82595_write16(I82595_PG0_XMT_BARL, 0x0000);

	/*
	 * Finally send a short reset command.
	 */
	i82595_write8(I82595_CMD, 0x1e);
        short_pause();
					
	/*
	 * Issue a receive enable command.
	 */
        i82595_write8(I82595_CMD, 0x08);

	spinlock_init(&dev_isr_lock, 0x00);
	spinlock_init(&dev_lock, 0x47);
}

/*
 * i82595_server_alloc()
 *	Allocate an i82595 server structure.
 */
struct ethdev_server *i82595_server_alloc(void)
{
	struct ethdev_server *eds;
	
	eds = (struct ethdev_server *)membuf_alloc(sizeof(struct ethdev_server), NULL);
	eds->eds_send = i82595_send_netbuf;
	eds->eds_get_mac = ethdev_get_mac;

	return eds;
}
/*
 * i82595_client_attach()
 *	Attach a client to an Ethernet device.
 */
struct ethdev_server *i82595_client_attach(struct ethdev_instance *edi, struct ethdev_client *edc)
{
	struct ethdev_server *eds = NULL;
		
	spinlock_lock(&edi->edi_lock);

	if (edi->edi_client == NULL) {
		eds = i82595_server_alloc();
                eds->eds_instance = edi;
                ethdev_instance_ref(edi);

                edc->edc_server = eds;
		edi->edi_client = edc;
		ethdev_client_ref(edc);
	}
		
	spinlock_unlock(&edi->edi_lock);

	return eds;
}

/*
 * i82595_instance_alloc()
 */
struct ethdev_instance *i82595_instance_alloc(void)
{
        struct ethdev_instance *edi;

	edi = (struct ethdev_instance *)membuf_alloc(sizeof(struct ethdev_instance), NULL);
	edi->edi_client = NULL;
	edi->edi_client_attach = i82595_client_attach;
	edi->edi_client_detach = ethdev_client_detach;

	spinlock_init(&edi->edi_lock, 0x47);

	i82595_init(edi);
			
	thread_create(i82595_intr_thread, edi, 0x1800, 0x90);

	return edi;
}
