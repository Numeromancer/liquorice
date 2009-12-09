/*
 * 3c509.c
 *	3Com 3C509 Ethernet card support.
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
 *
 * I would like to suggest that anyone designing Ethernet interfaces should
 * look at the Etherlink III Adapter Drivers Technical Reference - it's one
 * of the best I've seen to date (27th May 2000) and has made writing this
 * code very straightforward!
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
#include "3c509.h"

#if defined(I386)
#include "i386/3c509.h"
#else
#error "no valid architecture found"
#endif

#include "ethernet.h"

/*
 * Command/status register is available on all pages.
 */
#define C509_CMD 0x0e
#define C509_STATUS 0x0e

/*
 * Window 0 registers.
 */
#define C509_W0_EEPROM_DATA 0x0c
#define C509_W0_EEPROM_CMD 0x0a
#define C509_W0_RESOURCE_CFG 0x08
#define C509_W0_CONFIG_CTRL 0x04

/*
 * Window 1 registers.
 */
#define C509_W1_TX_STATUS 0x0b
#define C509_W1_RX_STATUS 0x08
#define C509_W1_TX_DATA 0x00
#define C509_W1_RX_DATA 0x00

/*
 * Window 2 registers.
 */
#define C509_W2_ADDR5 0x05
#define C509_W2_ADDR4 0x04
#define C509_W2_ADDR3 0x03
#define C509_W2_ADDR2 0x02
#define C509_W2_ADDR1 0x01
#define C509_W2_ADDR0 0x00

/*
 * Window 4 registers.
 */
#define C509_W4_MEDIA_TYPE 0x0a

/*
 * Commands.
 */
#define CMD_GLOBAL_RESET (0 << 11)
#define CMD_SELECT_WINDOW (1 << 11)
#define CMD_RX_ENABLE (4 << 11)
#define CMD_RX_RESET (5 << 11)
#define CMD_RX_DISCARD (8 << 11)
#define CMD_TX_ENABLE (9 << 11)
#define CMD_TX_RESET (11 << 11)
#define CMD_ACK_INT (13 << 11)
#define CMD_SET_INT_MASK (14 << 11)
#define CMD_SET_READ_ZERO_MASK (15 << 11)
#define CMD_SET_RX_FILTER (16 << 11)
#define CMD_STATS_ENABLE (21 << 11)
#define CMD_STATS_DISABLE (22 << 11)

/*
 * Status.
 */
#define STAT_INT_LATCH 0x0001
#define STAT_ADAPTER_FAILURE 0x0002
#define STAT_TX_COMPLETE 0x0004
#define STAT_TX_AVAILABLE 0x0008
#define STAT_RX_COMPLETE 0x0010
#define STAT_RX_EARLY 0x0020
#define STAT_INT_REQUESTED 0x0040
#define STAT_UPDATE_STATS 0x0080
#define STAT_CMD_IN_PROGRESS 0x1000

/*
 * Send state information.
 */
static struct context *dev_isr_ctx;
static struct lock dev_isr_lock;
static struct lock dev_lock;
static struct netbuf volatile *send_queue = NULL;
static volatile u8_t tx_available = 1;

/*
 * trap_irq5()
 */
#if defined(DJHPC)
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
 * c509_recv_get_packet()
 *	Fetch the next packet out of the receive ring buffer.
 */
struct netbuf *c509_recv_get_packet(void)
{
        u16_t stat;
        struct netbuf *nb = NULL;

        /*
         * Did we have a successful receive?
         */
        stat = c509_read16(C509_W1_RX_STATUS);
        if (!stat) {
        	return NULL;
        }

        if (!(stat & 0x4000)) {
	        u8_t *pkt;
	        u16_t *buf;
	        u16_t words;
        	u16_t pktsz = ((stat & 0x07ff) + 1) & 0xfffe;
		
		pkt = (u8_t *)membuf_alloc(pktsz, NULL);
		buf = (u16_t *)pkt;
	
		/*
		 * Read the packet.
		 */
		words = pktsz >> 1;
		while (words) {
			*buf++ = c509_read16(C509_W1_RX_DATA);
			words--;
		}

        	nb = netbuf_alloc();
		nb->nb_datalink_membuf = pkt;
		nb->nb_datalink = pkt;
		nb->nb_datalink_size = pktsz;
	}

        /*
         * Discard the packet and wait for the discard operation to complete.
         */
        c509_write16(C509_CMD, CMD_RX_DISCARD);
	while (c509_read16(C509_STATUS) & 0x1000);

	return nb;
}

/*
 * c509_send_set_packet()
 */
void c509_send_set_packet(struct netbuf *nb)
{
	u16_t words;
	u16_t *buf;
        u16_t sz;

        /*
         * Work out how many bytes we're actually going to be sending!
         */
	sz = nb->nb_datalink_size + nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size;
	if (sz >= 0x0600) {
		debug_print_pstr("\fc509 sd: ");
		debug_print16(sz);
        	return;
        }
	
	/*
	 * Write out the 32 bit header at the start of the packet buffer.
	 */
	c509_write16(C509_W1_TX_DATA, 0x8000 | sz);
	c509_write16(C509_W1_TX_DATA, 0x0000);

	/*
	 * Write out the different sections of the network buffer.  Note that
	 * we make the rather sweeping assumption that only the application
	 * layer has any odd-sized segments.
	 */
	words = nb->nb_datalink_size >> 1;
	buf = nb->nb_datalink;
	while (words) {
		c509_write16(C509_W1_TX_DATA, *buf++);
		words--;
	}
			
	words = nb->nb_network_size >> 1;
	buf = nb->nb_network;
	while (words) {
		c509_write16(C509_W1_TX_DATA, *buf++);
		words--;
	}
		
	words = nb->nb_transport_size >> 1;
	buf = nb->nb_transport;
	while (words) {
		c509_write16(C509_W1_TX_DATA, *buf++);
		words--;
	}
		
	words = nb->nb_application_size >> 1;
	buf = nb->nb_application;
	while (words) {
		c509_write16(C509_W1_TX_DATA, *buf++);
		words--;
	}
        if (nb->nb_application_size & 0x1) {
        	c509_write8(C509_W1_TX_DATA, *(u8_t *)buf);
        	c509_write8(C509_W1_TX_DATA, 0x00);
        }
			
	if (((sz + 1) >> 1) & 0x1) {
		c509_write16(C509_W1_TX_DATA, NULL);
	}
	
	tx_available = 0;
}

/*
 * c509_intr_thread()
 */
void c509_intr_thread(void *arg) __attribute__ ((noreturn));
void c509_intr_thread(void *arg)
{
	struct ethdev_instance *edi;
	
	edi = (struct ethdev_instance *)arg;

	dev_isr_ctx = current_context;
	
	pic_enable(5);
	
	while (1) {
		u16_t stat;
                				
		isr_disable();
		debug_set_lights(0x09);
		isr_spinlock_lock(&dev_isr_lock);

		stat = c509_read16(C509_STATUS);
		while ((stat & (STAT_RX_COMPLETE | STAT_TX_AVAILABLE | STAT_TX_COMPLETE | STAT_UPDATE_STATS)) == 0x0000) {
			isr_context_wait(&dev_isr_lock);
			stat = c509_read16(C509_STATUS);
		}

		isr_spinlock_unlock(&dev_isr_lock);
		isr_enable();
		
		spinlock_lock(&dev_lock);
		
		if (stat & STAT_RX_COMPLETE) {
			struct netbuf *nb;
        		struct ethdev_client *edc;
			
        		edc = edi->edi_client;	        		
			nb = c509_recv_get_packet();
			if (nb) {
				if (edc) {
					ethdev_client_ref(edc);
					spinlock_unlock(&dev_lock);
					edc->edc_recv(edc, nb);
				        spinlock_lock(&dev_lock);
	 				ethdev_client_deref(edc);
				}
				netbuf_deref(nb);
			}
		}
		
		if (stat & STAT_TX_COMPLETE) {
			u8_t st;
					
			/*
			 * Look at the transmit status.  If there's a problem then
			 * deal with it.
			 */
			st = c509_read8(C509_W1_TX_STATUS);
			
			/*
			 * If we've had a jabber or underrun error we need a reset.
			 */
			if (st & 0x30) {
				c509_write16(C509_CMD, CMD_TX_RESET);
			}
			
			/*
			 * If we've had a jabber, underrun, maximum collisions
			 * or tx status overflow we need to re-enable.
			 */
			if (st & 0x3c) {
				c509_write16(C509_CMD, CMD_TX_ENABLE);
			}

			/*
			 * If we've completed sending then look to see if there's
			 * any more to be done.
			 */		
			if (st & 0x80) {
				/*
				 * See if there's any more transmits pending.
				 */
				tx_available = 1;
			
		                if (send_queue) {
		                        struct netbuf *nb = NULL;
       	        			
       	        			nb = (struct netbuf *)send_queue;
					send_queue = nb->nb_next;
					c509_send_set_packet(nb);
				
					netbuf_deref(nb);
				}
			}

			/*
			 * Acknowledge the TX interrupt.
			 */
			c509_write8(C509_W1_TX_STATUS, 0x00);
		}
					
		if (stat & STAT_UPDATE_STATS) {
			/*
			 * We need to read the stats to clear the interrupt flag.
			 */
			c509_write16(C509_CMD, CMD_STATS_DISABLE);
			c509_write16(C509_CMD, CMD_SELECT_WINDOW | 6);
			c509_read8(0x00);
			c509_read8(0x01);
			c509_read8(0x02);
			c509_read8(0x03);
			c509_read8(0x04);
			c509_read8(0x05);
			c509_read8(0x06);
			c509_read8(0x07);
			c509_read8(0x08);
			c509_read8(0x09);
			c509_read8(0x0a);
			c509_read8(0x0b);
			c509_read8(0x0c);
			c509_read8(0x0d);
			c509_write16(C509_CMD, CMD_STATS_ENABLE);
			c509_write16(C509_CMD, CMD_SELECT_WINDOW | 1);
		}

		if (stat & STAT_ADAPTER_FAILURE) {
			c509_write16(C509_CMD, CMD_RX_RESET);
		        c509_write16(C509_CMD, CMD_SET_RX_FILTER | 0x0005);
		        c509_write16(C509_CMD, CMD_RX_ENABLE);
			c509_write16(C509_CMD, CMD_ACK_INT | STAT_ADAPTER_FAILURE);
		}
			
		/*
		 * Clear down the interrupt request and interrupt latch flags.
		 */
		c509_write16(C509_CMD, CMD_ACK_INT | STAT_INT_REQUESTED | STAT_INT_LATCH);
	
		spinlock_unlock(&dev_lock);
	}
}

/*
 * c509_send_netbuf()
 */
void c509_send_netbuf(struct netbuf *nb)
{
	spinlock_lock(&dev_lock);
		
	if (tx_available) {
		c509_send_set_packet(nb);
	} else {
		struct netbuf *p, **pprev;
		
		netbuf_ref(nb);
		        		
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
 * eeprom_read()
 *
 * We assume that we're in register window 0 on entry.
 */
static u16_t eeprom_read(u8_t addr)
{
	u32_t i;
	
	c509_write16(C509_W0_EEPROM_CMD, (u16_t)addr + 0x80);

	/*
	 * Pause for 162 us.
	 */
	for (i = 0; i < 500000; i++);

	return c509_read16(C509_W0_EEPROM_DATA);
}

/*
 * c509_init()
 *	Initialize the 3C509 card.
 */
void c509_init(struct ethdev_instance *edi)
{
        int i;
        u16_t ids = 0x00ff;
        u8_t *p;

        /*
         * Jump through hoops in the style described in the 3Com
         * documentation - don't ask ... it's not worth it :-/
         */
        c509_write8id(0x00);
        c509_write8id(0x00);
        for (i = 0; i < 256; i++) {
		c509_write8id((u8_t)ids);
		ids <<= 1;
		ids = (ids & 0x0100) ? (ids ^ 0x00cf) : ids;
        }
        c509_write8id(0xd1);

        /*
         * Enable the board at address 0x0300 (as far as the ISA bus is
         * concerned that is!).
         */
        c509_write8id(0xe0 | (0x100 >> 4));

	/*
	 * Get our MAC address.
	 */
	c509_write16(C509_CMD, CMD_SELECT_WINDOW | 0);
	for (i = 0; i < 3; i++) {
        	*((u16_t *)&edi->edi_mac[i << 1]) = hton16(eeprom_read(i));
	}

	debug_print_pstr("\f3c509 mac:\n\r");
	for (i = 0; i < 6; i++) {
		debug_print8(edi->edi_mac[i]);
	}
	
	/*
	 * Enable the adaptor.
	 */
	c509_write16(C509_W0_CONFIG_CTRL, 0x0001);

	/*
	 * Reset the transmitter and receiver and temporarily suspend all
	 * status flags.
	 */
	c509_write16(C509_CMD, CMD_TX_RESET);
	c509_write16(C509_CMD, CMD_RX_RESET);
	c509_write16(C509_CMD, CMD_SET_READ_ZERO_MASK | 0x0000);
		
	/*
	 * Set IRQ5.
	 */
	c509_write16(C509_W0_RESOURCE_CFG, 0x5f00);
	
	/*
	 * Set our MAC address.
	 */
	c509_write16(C509_CMD, CMD_SELECT_WINDOW | 2);
        p = (u8_t *)edi->edi_mac;
        c509_write8(C509_W2_ADDR0, *p++);
        c509_write8(C509_W2_ADDR1, *p++);
        c509_write8(C509_W2_ADDR2, *p++);
        c509_write8(C509_W2_ADDR3, *p++);
        c509_write8(C509_W2_ADDR4, *p++);
        c509_write8(C509_W2_ADDR5, *p++);
				
	/*
	 * Set media type to twisted pair (link beat and jabber enable)
	 */
	c509_write16(C509_CMD, CMD_SELECT_WINDOW | 4);
	c509_write16(C509_W4_MEDIA_TYPE, c509_read16(C509_W4_MEDIA_TYPE) | 0x00c0);
	
	/*
	 * Clear down all of the statistics (by reading them).
	 */
	c509_write16(C509_CMD, CMD_STATS_DISABLE);
	c509_write16(C509_CMD, CMD_SELECT_WINDOW | 6);
        c509_read8(0x00);
        c509_read8(0x01);
        c509_read8(0x02);
        c509_read8(0x03);
        c509_read8(0x04);
        c509_read8(0x05);
        c509_read8(0x06);
        c509_read8(0x07);
        c509_read8(0x08);
        c509_read8(0x09);
        c509_read8(0x0a);
        c509_read8(0x0b);
        c509_read8(0x0c);
        c509_read8(0x0d);
	c509_write16(C509_CMD, CMD_STATS_ENABLE);

        /*
         * Set to receive host address and broadcasts.
         */
	c509_write16(C509_CMD, CMD_SELECT_WINDOW | 1);
        c509_write16(C509_CMD, CMD_SET_RX_FILTER | 0x0005);

        /*
         * Enable the receiver and transmitter.
         */
        c509_write16(C509_CMD, CMD_RX_ENABLE);
        c509_write16(C509_CMD, CMD_TX_ENABLE);

        /*
         * Allow status bits through.
         */
        c509_write16(C509_CMD, CMD_SET_READ_ZERO_MASK | 0x00ff);

        /*
         * Acknowledge any pending interrupts and enable those that we want to
         * see during operations.
         */
        c509_write16(C509_CMD, CMD_ACK_INT | STAT_INT_LATCH | STAT_TX_AVAILABLE
        		| STAT_RX_EARLY | STAT_INT_REQUESTED);
        c509_write16(C509_CMD, CMD_SET_INT_MASK | STAT_INT_LATCH | STAT_ADAPTER_FAILURE
			| STAT_TX_COMPLETE | STAT_RX_COMPLETE | STAT_UPDATE_STATS);

	spinlock_init(&dev_isr_lock, 0x00);
	spinlock_init(&dev_lock, 0x47);
}

/*
 * c509_server_alloc()
 *	Allocate an 3C509 server structure.
 */
struct ethdev_server *c509_server_alloc(void)
{
	struct ethdev_server *eds;
	
	eds = (struct ethdev_server *)membuf_alloc(sizeof(struct ethdev_server), NULL);
	eds->eds_send = c509_send_netbuf;
	eds->eds_get_mac = ethdev_get_mac;

	return eds;
}
/*
 * c509_client_attach()
 *	Attach a client to an Ethernet device.
 */
struct ethdev_server *c509_client_attach(struct ethdev_instance *edi, struct ethdev_client *edc)
{
	struct ethdev_server *eds = NULL;
		
	spinlock_lock(&edi->edi_lock);

	if (edi->edi_client == NULL) {
		eds = c509_server_alloc();
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
 * c509_instance_alloc()
 */
struct ethdev_instance *c509_instance_alloc(void)
{
        struct ethdev_instance *edi;

	edi = (struct ethdev_instance *)membuf_alloc(sizeof(struct ethdev_instance), NULL);
	edi->edi_client = NULL;
	edi->edi_client_attach = c509_client_attach;
	edi->edi_client_detach = ethdev_client_detach;

	spinlock_init(&edi->edi_lock, 0x47);

	c509_init(edi);
			
	thread_create(c509_intr_thread, edi, 0x1800, 0x90);

	return edi;
}
