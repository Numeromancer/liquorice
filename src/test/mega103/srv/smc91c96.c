/*
 * smc91c96.c
 *	SMC 91C96 Ethernet controller support.
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
#include "ethdev.h"
#include "smc91c96.h"

#if defined(ATMEGA103) || defined(AT90S8515)
#include "avr/smc91c96.h"
#else
#error "no valid architecture found"
#endif

#include "ethernet.h"

/*
 * Bank select register is available on all pages.
 */
#define SMC_BANK_SELECT 0x0e

/*
 * Bank 0 registers.
 */
#define SMC_B0_TX_CTRL 0x0
#define SMC_B0_EPH_STATUS 0x2
#define SMC_B0_RX_CTRL 0x4
#define SMC_B0_COUNTER 0x6
#define SMC_B0_MEM_INFO 0x8
#define SMC_B0_MEM_CONFIG 0xa

/*
 * Bank 1 registers.
 */
#define SMC_B1_CONFIG 0x0
#define SMC_B1_BASE_ADDR 0x2
#define SMC_B1_ADDR0 0x4
#define SMC_B1_ADDR1 0x5
#define SMC_B1_ADDR2 0x6
#define SMC_B1_ADDR3 0x7
#define SMC_B1_ADDR4 0x8
#define SMC_B1_ADDR5 0x9
#define SMC_B1_GENERAL_ADDR 0xa
#define SMC_B1_CTRL 0xc

/*
 * Bank 2 registers.
 */
#define SMC_B2_MMU_CMD 0x0
#define SMC_B2_AUTO_TX_START 0x1
#define SMC_B2_PKT_NUM 0x2
#define SMC_B2_ALLOC_RES 0x3
#define SMC_B2_FIFO_PORTS 0x4
#define SMC_B2_POINTER 0x6
#define SMC_B2_DATA 0x8
#define SMC_B2_INT_STAT 0xc
#define SMC_B2_INT_ACK 0xc
#define SMC_B2_INT_MASK 0xd

/*
 * MMU command register info.
 */
#define MMU_CMD_NO_OP 0x00
#define MMU_CMD_ALLOC_MEM 0x20
#define MMU_CMD_RESET 0x40
#define MMU_CMD_REMOVE_RX_FRM 0x60
#define MMU_CMD_REMOVE_TX_FRM 0x70
#define MMU_CMD_REMOVE_RELEASE_RX_FRM 0x80
#define MMU_CMD_RELEASE_PKT 0xa0
#define MMU_CMD_QUEUE_TX 0xc0
#define MMU_CMD_RESET_TX 0xe0

/*
 * Interrupt status flags.
 */
#define INT_TX_IDLE 0x80
#define INT_ERCV 0x40
#define INT_EPH 0x20
#define INT_RX_OVRN 0x10
#define INT_ALLOC 0x08
#define INT_TX_EMPTY 0x04
#define INT_TX 0x02
#define INT_RCV 0x01

/*
 * Pointer register flags.
 */
#define PTR_RCV 0x8000
#define PTR_AUTO_INCR 0x4000
#define PTR_READ 0x2000
#define PTR_ETEN 0x1000
#define PTR_AUTO_TX 0x0800
#define PTR_MASK 0x7ff

/*
 * Receive status flags.
 */
#define RSTAT_ALIGN_ERR 0x8000
#define RSTAT_BROADCAST 0x4000
#define RSTAT_BAD_CRC 0x2000
#define RSTAT_OFF_FRM 0x1000
#define RSTAT_TOO_LONG 0x0800
#define RSTAT_TOO_SHORT 0x0400

/*
 * Transmit status flags.
 */
#define TSTAT_TX_UNRN 0x8000
#define TSTAT_LINK_OK 0x4000
#define TSTAT_CTR_ROL 0x1000
#define TSTAT_EXC_DEF 0x0800
#define TSTAT_LOST_CARR 0x0400
#define TSTAT_LATCOL 0x0200
#define TSTAT_WAKEUP 0x0100
#define TSTAT_TX_DEFR 0x0080
#define TSTAT_LTX_BRD 0x0040
#define TSTAT_SQET 0X0020
#define TSTAT_16COL 0x0010
#define TSTAT_LTX_MULT 0X0008
#define TSTAT_MUL_COL 0x0004
#define TSTAT_SNGL_COL 0x0002
#define TSTAT_TX_SUC 0x0001

/*
 * Send state information.
 */
static struct context *dev_isr_ctx;
static struct lock dev_isr_lock;
static struct lock dev_lock;
static struct netbuf volatile *send_queue = NULL;
static struct netbuf *tx_waiting = NULL;

/*
 * smc91c96_isr()
 */
void smc91c96_isr(void)
{
	debug_set_lights(0x0a);

	isr_spinlock_lock(&dev_isr_lock);
	
	out8(EIMSK, in8(EIMSK) & (~BV(INT7)));
	
	debug_check_stack(0x30);
	
	isr_context_signal(dev_isr_ctx);

	isr_spinlock_unlock(&dev_isr_lock);
}

/*
 * _interrupt7_()
 */
#if defined(PCB520)
void _interrupt7_(void) __attribute__ ((naked));
void _interrupt7_(void)
{
	asm volatile ("push r0\n\t"
			"push r1\n\t"
			"in r0, 0x3f\n\t"
			"push r0\n\t"
#ifdef ATMEGA103
			"in r0, 0x3b\n\t"
			"push r0\n\t"
#endif
			"push r18\n\t"
			"push r19\n\t"
			"push r20\n\t"
			"push r21\n\t"
			"push r22\n\t"
			"push r23\n\t"
			"push r24\n\t"
			"push r25\n\t"
			"push r26\n\t"
			"push r27\n\t"
			"push r30\n\t"
			"push r31\n\t"
			::);

	smc91c96_isr();
	
	asm volatile ("jmp startup_continue\n\t" ::);
}
#endif

/*
 * smc91c96_recv_get_packet()
 *	Fetch the next packet out of the receive ring buffer.
 */
struct netbuf *smc91c96_recv_get_packet(void)
{
        struct netbuf *nb = NULL;
        u16_t stat;

        /*
         * Find out if we have a received packet.  Assume that we're in bank 2 already.
         */
        if (smc91c96_read16(SMC_B2_FIFO_PORTS) & 0x8000) {
        	return NULL;
        }

        /*
         * Prepare to read the packet.
         */
        smc91c96_write16(SMC_B2_POINTER, PTR_RCV | PTR_READ | PTR_AUTO_INCR);

        /*
         * First word is the status.  If we've had no errors then start to read the data.
         */
        stat = smc91c96_read16(SMC_B2_DATA);
        if ((stat & (RSTAT_ALIGN_ERR | RSTAT_BAD_CRC | RSTAT_TOO_LONG | RSTAT_TOO_SHORT)) == 0x0000) {
	        u8_t *pkt;
	        u16_t *buf;
	        u16_t words;
        	u16_t pktsz;
        	        	
        	/*
        	 * Our packet size includes the status and packet size so we
        	 * need to ignore them.
        	 */
        	pktsz = smc91c96_read16(SMC_B2_DATA) - 4;

       		pkt = membuf_alloc(pktsz, NULL);
	        buf = (u16_t *)pkt;
	
		/*
		 * Read the packet.
		 */
		words = pktsz >> 1;
		while (words) {
			*buf++ = smc91c96_read16(SMC_B2_DATA);
			words--;
		}

        	nb = netbuf_alloc();
		nb->nb_datalink_membuf = pkt;
		nb->nb_datalink = pkt;
		nb->nb_datalink_size = pktsz;
	}

        /*
         * All done - release the packet.
         */
        smc91c96_write8(SMC_B2_MMU_CMD, MMU_CMD_REMOVE_RELEASE_RX_FRM);

	return nb;
}

/*
 * smc91c96_send_after_alloc()
 *	Send the packet, knowing that we've been allocated the buffer space.
 */
void smc91c96_send_after_alloc(struct netbuf *nb)
{
	u16_t words;
        u16_t *buf;
        u16_t sz;
        u8_t pkt;

	/*
	 * Work out how many bytes we're actually going to be sending!
	 */
	sz = nb->nb_datalink_size + nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size;
	
	/*
	 * Get the allocated page number and use it!
	 */
	pkt = smc91c96_read8(SMC_B2_ALLOC_RES);
	smc91c96_write8(SMC_B2_PKT_NUM, pkt);
	smc91c96_write16(SMC_B2_POINTER, PTR_AUTO_INCR);

	/*
	 * Write out the 32 bit header at the start of the packet buffer.
	 */
	smc91c96_write16(SMC_B2_DATA, 0x0000);
	smc91c96_write16(SMC_B2_DATA, (sz & 0xfffe) + 6);

	/*
	 * Write out the different sections of the network buffer.
	 */
	words = nb->nb_datalink_size >> 1;
	buf = nb->nb_datalink;
	while (words) {
		smc91c96_write16(SMC_B2_DATA, *buf++);
		words--;
	}
		
	words = nb->nb_network_size >> 1;
	buf = nb->nb_network;
	while (words) {
		smc91c96_write16(SMC_B2_DATA, *buf++);
		words--;
	}
		
	words = nb->nb_transport_size >> 1;
	buf = nb->nb_transport;
	while (words) {
		smc91c96_write16(SMC_B2_DATA, *buf++);
		words--;
	}
		
	words = nb->nb_application_size >> 1;
	buf = nb->nb_application;
	while (words) {
		smc91c96_write16(SMC_B2_DATA, *buf++);
		words--;
	}
		
	/*
	 * We assume that any odd length comes from the application segment.
	 */
	if (nb->nb_application_size & 0x1) {
		smc91c96_write8(SMC_B2_DATA, *(u8_t *)buf);
		smc91c96_write8(SMC_B2_DATA, 0x20);
	} else {
		smc91c96_write16(SMC_B2_DATA, 0x0000);
	}

	/*
	 * Enable the tranmsit interrupt support.
	 */
	smc91c96_write8(SMC_B2_INT_MASK, smc91c96_read8(SMC_B2_INT_MASK) | INT_TX);

	/*
	 * Let the transmitter run.
	 */
        smc91c96_write8(SMC_B2_MMU_CMD, MMU_CMD_QUEUE_TX);
}

/*
 * smc91c96_send_set_packet()
 */
void smc91c96_send_set_packet(struct netbuf *nb)
{
        u8_t pgs;
        u16_t sz;

        /*
         * Work out how many bytes we're actually going to be sending!
         */
	sz = nb->nb_datalink_size + nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size;
        		
	/*
	 * How many pages does this require?
	 */
	pgs = (sz + 6) >> 8;
	
	/*
	 * Reject any packets that are really too large.
	 */
	if (pgs > 6) {
		debug_print_pstr("\f91c96 sd: ");
		debug_print16(sz);
		return;
	}

	/*
	 * Allocate the space.
	 */
        smc91c96_write8(SMC_B2_MMU_CMD, MMU_CMD_ALLOC_MEM | pgs);
	
	/*
	 * Look to see if this worked immediately.  If it didn't then we enable
	 * the alloc interrupt and return until the memory is available.
	 */
	if (!(smc91c96_read8(SMC_B2_INT_STAT) & INT_ALLOC)) {
		smc91c96_write8(SMC_B2_INT_MASK, smc91c96_read8(SMC_B2_INT_MASK) | INT_ALLOC);
		tx_waiting = nb;

		netbuf_ref(nb);
		return;
	}

	/*
	 * Excellent - we have our allocation, so on with the transmission!
	 * First though up we have to ack the alloc interrupt!
	 */
	smc91c96_write8(SMC_B2_INT_ACK, INT_ALLOC);
	smc91c96_send_after_alloc(nb);
}

/*
 * smc91c96_intr_thread()
 */
void smc91c96_intr_thread(void *arg) __attribute__ ((noreturn));
void smc91c96_intr_thread(void *arg)
{
	struct ethdev_instance *edi;
	
	edi = (struct ethdev_instance *)arg;

	dev_isr_ctx = current_context;

	while (1) {
		u8_t stat;

		isr_disable();
		debug_set_lights(0x0b);
		isr_spinlock_lock(&dev_isr_lock);

		smc91c96_write16(SMC_BANK_SELECT, 0x3302);

		stat = smc91c96_read8(SMC_B2_INT_STAT) & smc91c96_read8(SMC_B2_INT_MASK);
		while ((stat & (INT_ALLOC | INT_TX | INT_RCV)) == 0x0000) {
			out8(EIMSK, in8(EIMSK) | BV(INT7));
			isr_context_wait(&dev_isr_lock);
			stat = smc91c96_read8(SMC_B2_INT_STAT) & smc91c96_read8(SMC_B2_INT_MASK);
		}

		isr_spinlock_unlock(&dev_isr_lock);
		isr_enable();
		
		spinlock_lock(&dev_lock);
		
		if (stat & INT_RCV) {
			struct netbuf *nb;
        		struct ethdev_client *edc;
			
        		edc = edi->edi_client;	        		
			nb = smc91c96_recv_get_packet();
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
	
		if (stat & INT_TX) {
			u16_t st;
			u8_t pktsave;
                        u8_t pkt;
							
			/*
			 * Save the pointer register.
			 */
			pktsave = smc91c96_read8(SMC_B2_PKT_NUM);
		 		
			/*
			 * Find out which packet was just being transmitted and
			 * take a look at its status.
			 */
			pkt = (u8_t)(smc91c96_read16(SMC_B2_FIFO_PORTS)) & 0x7f;
		        smc91c96_write16(SMC_B2_POINTER, PTR_READ | PTR_AUTO_INCR);
                        st = smc91c96_read16(SMC_B2_DATA);
		        			
			/*
			 * Re-enable the transmitter.
			 */
			smc91c96_write16(SMC_BANK_SELECT, 0x3300);
		        smc91c96_write16(SMC_B0_TX_CTRL, 0x0081);

			/*
			 * Issue a release to free up the transmit buffer.
			 */
			smc91c96_write16(SMC_BANK_SELECT, 0x3302);
			smc91c96_write16(SMC_B2_MMU_CMD, MMU_CMD_RELEASE_PKT);
			
			/*
			 * Acknowledge the TX interrupt.
			 */
			smc91c96_write8(SMC_B2_INT_ACK, INT_TX);
					
			/*
			 * Restore the pointer register.
			 */
			smc91c96_write8(SMC_B2_PKT_NUM, pktsave);
		}
	
		if (stat & INT_ALLOC) {
                        struct netbuf *nb = NULL;
				
			/*
			 * Mask the interrupt, acknowledge it and continue with
			 * our delayed transmission.
			 */
			smc91c96_write8(SMC_B2_INT_MASK, smc91c96_read8(SMC_B2_INT_MASK) & (~INT_ALLOC));
			smc91c96_write8(SMC_B2_INT_ACK, INT_ALLOC);
			smc91c96_send_after_alloc(tx_waiting);
			
                        netbuf_deref(tx_waiting);
						
			tx_waiting = NULL;
				
			/*
			 * See if there's any more transmits pending.
			 */
	                if (send_queue) {
				nb = (struct netbuf *)send_queue;
				send_queue = nb->nb_next;
				smc91c96_send_set_packet(nb);
			
				netbuf_deref(nb);
			}
		}

		spinlock_unlock(&dev_lock);
	}
}

/*
 * smc91c96_send_netbuf()
 */
void smc91c96_send_netbuf(struct netbuf *nb)
{
	spinlock_lock(&dev_lock);
		
	if (!tx_waiting) {
		smc91c96_send_set_packet(nb);
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
 * smc91c96_init()
 */
void smc91c96_init(struct ethdev_instance *edi)
{
	/*
	 * An Ethernet MAC address that I know I won't encounter on any network since
	 * this address was assigned to a card that I now have on one side as it
	 * doesn't work properly.
	 */
	static u8_t mac_addr[6] = {0x00, 0x40, 0xc7, 0x20, 0xfb, 0x33};
	u8_t *p;

	/*
	 * Establish our default base I/O address at 0x0300.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3301);
	smc91c96_write16(SMC_B1_BASE_ADDR, 0x1800);

	/*
	 * Give ourselves a MAC address.  For now let's use one that I know
	 * we'll never hit on a real network (because I've got the card on one
	 * side as it doesn't work properly!)
	 */
	p = (u8_t *)mac_addr;
	smc91c96_write8(SMC_B1_ADDR0, *p++);
	smc91c96_write8(SMC_B1_ADDR1, *p++);
	smc91c96_write8(SMC_B1_ADDR2, *p++);
	smc91c96_write8(SMC_B1_ADDR3, *p++);
	smc91c96_write8(SMC_B1_ADDR4, *p++);
	smc91c96_write8(SMC_B1_ADDR5, *p++);
	memcpy(edi->edi_mac, mac_addr, 6);

	/*
	 * Issue a soft reset.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3300);
	smc91c96_write16(SMC_B0_RX_CTRL, 0x8000);
	smc91c96_write16(SMC_B0_RX_CTRL, 0x0000);

	/*
	 * Configure to auto-release completed transmit pages.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3301);
	smc91c96_write16(SMC_B1_CTRL, 0x0800);

	/*
	 * Reset the MMU to it original state.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3302);
	smc91c96_write8(SMC_B2_MMU_CMD, MMU_CMD_RESET);

	/*
	 * Enable the device.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3300);
	smc91c96_write16(SMC_B0_TX_CTRL, 0x0081);
	smc91c96_write16(SMC_B0_RX_CTRL, 0x0300);

	/*
	 * Set the interrupt mask.
	 */
	smc91c96_write16(SMC_BANK_SELECT, 0x3302);
	smc91c96_write8(SMC_B2_INT_MASK, INT_EPH | INT_RX_OVRN | INT_RCV);

	spinlock_init(&dev_lock, 0x47);
	spinlock_init(&dev_isr_lock, 0x00);
}

/*
 * smc91c96_server_alloc()
 *	Allocate an SMC91C96 server structure.
 */
struct ethdev_server *smc91c96_server_alloc(void)
{
	struct ethdev_server *eds;
	
	eds = (struct ethdev_server *)membuf_alloc(sizeof(struct ethdev_server), NULL);
	eds->eds_send = smc91c96_send_netbuf;
	eds->eds_get_mac = ethdev_get_mac;
        	
	return eds;
}
/*
 * smc91c96_client_attach()
 *	Attach a client to an Ethernet device.
 */
struct ethdev_server *smc91c96_client_attach(struct ethdev_instance *edi, struct ethdev_client *edc)
{
	struct ethdev_server *eds = NULL;
		
	spinlock_lock(&edi->edi_lock);

	if (edi->edi_client == NULL) {
		eds = smc91c96_server_alloc();
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
 * smc91c96_instance_alloc()
 */
struct ethdev_instance *smc91c96_instance_alloc(void)
{
        struct ethdev_instance *edi;

	edi = (struct ethdev_instance *)membuf_alloc(sizeof(struct ethdev_instance), NULL);
	edi->edi_client = NULL;
	edi->edi_client_attach = smc91c96_client_attach;
	edi->edi_client_detach = ethdev_client_detach;

	spinlock_init(&edi->edi_lock, 0x47);

	smc91c96_init(edi);
	
	thread_create(smc91c96_intr_thread, edi, 0x180, 0x90);

	return edi;
}
