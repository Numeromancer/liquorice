/*
 * ne2000.c
 *	NE2000 Ethernet controller support.
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
 * Quite a lot of the information about how to talk to NE2000's comes from
 * the Linux source code written by Donald Becker et al.  This code has been
 * written from scratch to avoid any possible license conflicts (and also
 * to suit smaller systems).
 *
 * Another good place to find information is National Semiconductor's apps
 * notes related to the NS8390 upon which the NE2000 is based.
 *
 * BTW if the sources look a little disjoint, sometimes referencing NE2000's
 * and other times talking about NS8390's, this is because the original NE2000
 * included an '8390, but also added things like the ID prom.  My thinking has
 * been that this driver might be modified to support other '8390-based designs
 * at some stage in the future.
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
#include "ne2000.h"

#if defined(I386)
#include "i386/ne2000.h"
#else
#error "no valid architecture found"
#endif

#include "ethernet.h"

/*
 * Register offset applicable to all register pages.
 */
#define NS8390_CR 0x00			/* Command register */
#define NS8390_IOPORT 0x10		/* I/O data port */
#define NS8390_RESET 0x1f		/* Reset port */

/*
 * Page 0 register offsets.
 */
#define NS8390_PG0_CLDA0 0x01		/* Current local DMA address 0 */
#define NS8390_PG0_PSTART 0x01		/* Page start register */
#define NS8390_PG0_CLDA1 0x02		/* Current local DMA address 1 */
#define NS8390_PG0_PSTOP 0x02		/* Page stop register */
#define NS8390_PG0_BNRY 0x03		/* Boundary pointer */
#define NS8390_PG0_TSR 0x04		/* Transmit status register */
#define NS8390_PG0_TPSR 0x04		/* Transmit page start address */
#define NS8390_PG0_NCR 0x05		/* Number of collisions register */
#define NS8390_PG0_TBCR0 0x05		/* Transmit byte count register 0 */
#define NS8390_PG0_FIFO 0x06		/* FIFO */
#define NS8390_PG0_TBCR1 0x06		/* Transmit byte count register 1 */
#define NS8390_PG0_ISR 0x07		/* Interrupt status register */
#define NS8390_PG0_CRDA0 0x08		/* Current remote DMA address 0 */
#define NS8390_PG0_RSAR0 0x08		/* Remote start address register 0 */
#define NS8390_PG0_CRDA1 0x09		/* Current remote DMA address 1 */
#define NS8390_PG0_RSAR1 0x09		/* Remote start address register 1 */
#define NS8390_PG0_RBCR0 0x0a		/* Remote byte count register 0 */
#define NS8390_PG0_RBCR1 0x0b		/* Remote byte count register 1 */
#define NS8390_PG0_RSR 0x0c		/* Receive status register */
#define NS8390_PG0_RCR 0x0c		/* Receive configuration register */
#define NS8390_PG0_CNTR0 0x0d		/* Tally counter 0 (frame alignment errors) */
#define NS8390_PG0_TCR 0x0d		/* Transmit configuration register */
#define NS8390_PG0_CNTR1 0x0e		/* Tally counter 1 (CRC errors) */
#define NS8390_PG0_DCR 0x0e		/* Data configuration register */
#define NS8390_PG0_CNTR2 0x0f		/* Tally counter 2 (Missed packet errors) */
#define NS8390_PG0_IMR 0x0f		/* Interrupt mask register */

/*
 * Page 1 register offsets.
 */
#define NS8390_PG1_PAR0 0x01		/* Physical address register 0 */
#define NS8390_PG1_PAR1 0x02		/* Physical address register 1 */
#define NS8390_PG1_PAR2 0x03		/* Physical address register 2 */
#define NS8390_PG1_PAR3 0x04		/* Physical address register 3 */
#define NS8390_PG1_PAR4 0x05		/* Physical address register 4 */
#define NS8390_PG1_PAR5 0x06		/* Physical address register 5 */
#define NS8390_PG1_CURR 0x07		/* Current page register */
#define NS8390_PG1_MAR0 0x08		/* Multicast address register 0 */
#define NS8390_PG1_MAR1 0x09		/* Multicast address register 1 */
#define NS8390_PG1_MAR2 0x0a		/* Multicast address register 2 */
#define NS8390_PG1_MAR3 0x0b		/* Multicast address register 3 */
#define NS8390_PG1_MAR4 0x0c		/* Multicast address register 4 */
#define NS8390_PG1_MAR5 0x0d		/* Multicast address register 5 */
#define NS8390_PG1_MAR6 0x0e		/* Multicast address register 6 */
#define NS8390_PG1_MAR7 0x0f		/* Multicast address register 7 */

/*
 * Page 2 register offsets.
 */
#define NS8390_PG2_PSTART 0x01		/* Page start register */
#define NS8390_PG2_CLDA0 0x01		/* Current local DMA address 0 */
#define NS8390_PG2_PSTOP 0x02		/* Page stop register */
#define NS8390_PG2_CLDA1 0x02		/* Current local DMA address 1 */
#define NS8390_PG2_RNP 0x03		/* Remote next packet pointer */
#define NS8390_PG2_TSPR 0x04		/* Transmit page start register */
#define NS8390_PG2_LNP 0x05		/* Local next packet pointer */
#define NS8390_PG2_ACU 0x06		/* Address counter (upper) */
#define NS8390_PG2_ACL 0x07		/* Address counter (lower) */
#define NS8390_PG2_RCR 0x0c		/* Receive configuration register */
#define NS8390_PG2_TCR 0x0d		/* Transmit configuration register */
#define NS8390_PG2_DCR 0x0e		/* Data configuration register */
#define NS8390_PG2_IMR 0x0f		/* Interrupt mask register */

/*
 * Command register bits.
 */
#define NS8390_CR_STP 0x01		/* Stop */
#define NS8390_CR_STA 0x02		/* Start */
#define NS8390_CR_TXP 0x04		/* Transmit packet */
#define NS8390_CR_RD0 0x08		/* Remote DMA command bit 0 */
#define NS8390_CR_RD1 0x10		/* Remote DMA command bit 1 */
#define NS8390_CR_RD2 0x20		/* Remote DMA command bit 2 */
#define NS8390_CR_PS0 0x40		/* Page select bit 0 */
#define NS8390_CR_PS1 0x80		/* Page select bit 1 */

/*
 * Interrupt status register bits.
 */
#define NS8390_ISR_PRX 0x01		/* Packet received */
#define NS8390_ISR_PTX 0x02		/* Packet transmitted */
#define NS8390_ISR_RXE 0x04		/* Receive error */
#define NS8390_ISR_TXE 0x08		/* Transmit error */
#define NS8390_ISR_OVW 0x10		/* Overwrite warning */
#define NS8390_ISR_CNT 0x20		/* Counter overflow */
#define NS8390_ISR_RDC 0x40		/* Remote DMA complete */
#define NS8390_ISR_RST 0x80		/* Reset status */

/*
 * Interrupt mask register bits.
 */
#define NS8390_IMR_PRXE 0x01		/* Packet received interrupt enable */
#define NS8390_IMR_PTXE 0x02		/* Packet transmitted interrupt enable */
#define NS8390_IMR_RXEE 0x04		/* Receive error interrupt enable */
#define NS8390_IMR_TXEE 0x08		/* Transmit error interrupt enable */
#define NS8390_IMR_OVWE 0x10		/* Overwrite warning interrupt enable */
#define NS8390_IMR_CNTE 0x20		/* Counter overflow interrupt enable */
#define NS8390_IMR_RCDE 0x40		/* Remote DMA complete interrupt enable */

/*
 * Data configuration register bits.
 */
#define NS8390_DCR_WTS 0x01		/* Word transfer select */
#define NS8390_DCR_BOS 0x02		/* Byte order select */
#define NS8390_DCR_LAS 0x04		/* Long address select */
#define NS8390_DCR_LS 0x08		/* Loopback select */
#define NS8390_DCR_AR 0x10		/* Auto-initialize remote */
#define NS8390_DCR_FT0 0x20		/* FIFO threshold select bit 0 */
#define NS8390_DCR_FT1 0x40		/* FIFO threshold select bit 1 */

/*
 * Transmit configuration register bits.
 */
#define NS8390_TCR_CRC 0x01		/* Inhibit CRC */
#define NS8390_TCR_LB0 0x02		/* Encoded loopback control bit 0 */
#define NS8390_TCR_LB1 0x04		/* Encoded loopback control bit 1 */
#define NS8390_TCR_ATD 0x08		/* Auto transmit disable */
#define NS8390_TCR_OFST 0x10		/* Collision offset enable */

/*
 * Transmit status register bits.
 */
#define NS8390_TSR_PTX 0x01		/* Packet transmitted */
#define NS8390_TSR_COL 0x04		/* Transmit collided */
#define NS8390_TSR_ABT 0x08		/* Transmit aborted */
#define NS8390_TSR_CRS 0x10		/* Carrier sense lost */
#define NS8390_TSR_FU 0x20		/* FIFO underrun */
#define NS8390_TSR_CDH 0x40		/* CD heartbeat */
#define NS8390_TSR_OWC 0x80		/* Out of window collision */

/*
 * Receive configuration register bits.
 */
#define NS8390_RCR_SEP 0x01		/* Save errored packets */
#define NS8390_RCR_AR 0x02		/* Accept runt packets */
#define NS8390_RCR_AB 0x04		/* Accept broadcast */
#define NS8390_RCR_AM 0x08		/* Accept multicast */
#define NS8390_RCR_PRO 0x10		/* Promiscuous physical */
#define NS8390_RCR_MON 0x20		/* Monitor mode */

/*
 * Receive status register bits.
 */
#define NS8390_RSR_PRX 0x01		/* Packet received intact */
#define NS8390_RSR_CRC 0x02		/* CRC error */
#define NS8390_RSR_FAE 0x04		/* Frame alignment error */
#define NS8390_RSR_FO 0x08		/* FIFO overrun */
#define NS8390_RSR_MPA 0x10		/* Missed packet */
#define NS8390_RSR_PHY 0x20		/* Physical/multicast address */
#define NS8390_RSR_DIS 0x40		/* Receiver disabled */
#define NS8390_RSR_DFR 0x80		/* Deferring */

/*
 * Standard sizing information
 */
#define TX_PAGES 12			/* Allow for 2 back-to-back packets */

/*
 * 8390 per-packet header.
 */
struct ns8390_pkt_header {
	u8_t ph_status;			/* Status */
	u8_t ph_nextpg;			/* Page for next packet */
	u16_t ph_size;			/* Size of header and packet in octets */
};

/*
 * Misc controller device status.
 */
static struct netbuf *send_queue = NULL;
static struct context *dev_isr_ctx;
static struct lock dev_lock;
static struct lock dev_isr_lock;
static u8_t send_start_pg;
static u8_t ring_start_pg;
static u8_t ring_stop_pg;
static volatile u8_t tx_available = 1;

/*
 * trap_irq5()
 */
#if defined (DJHNE)
void trap_irq5(void)
{
	debug_set_lights(0x2f);
			
	isr_spinlock_lock(&dev_isr_lock);

	pic_acknowledge(5);
	
	debug_check_stack(0x30);
	
	isr_context_signal(dev_isr_ctx);

	isr_spinlock_unlock(&dev_isr_lock);
}
#endif

/*
 * ns8390_recv_get_packet()
 *	Fetch the next packet out of the receive ring buffer.
 */
static struct netbuf *ns8390_recv_get_packet(void)
{
	struct netbuf *nb;
	struct ns8390_pkt_header hdr;
	u8_t curr;
	u8_t bnry;
	u16_t count;
	u16_t words;
	u8_t *pkt;
	u16_t *buf;
	u8_t nextpg;
	u16_t i;
        u8_t j;

	ns8390_write8(NS8390_CR, NS8390_CR_RD2 | NS8390_CR_PS0);
	curr = ns8390_read8(NS8390_PG1_CURR);
	ns8390_write8(NS8390_CR, NS8390_CR_RD2);
	bnry = ns8390_read8(NS8390_PG0_BNRY) + 1;
	if (bnry >= ring_stop_pg) {
		bnry = ring_start_pg;
	}

	if (bnry == curr) {
		return 	NULL;
	}
	
	/*
	 * Set up to DMA the packet header back.
	 */
	ns8390_write8(NS8390_PG0_RBCR0, sizeof(struct ns8390_pkt_header));
	ns8390_write8(NS8390_PG0_RBCR1, 0);
	ns8390_write8(NS8390_PG0_RSAR0, 0);
	ns8390_write8(NS8390_PG0_RSAR1, bnry);
	
	/*
	 * Perform the read.
	 */	
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD0);
	
	buf = (u16_t *)&hdr;
	for (j = 0; j < (sizeof(struct ns8390_pkt_header) / 2); j++) {
		*buf++ = ns8390_read16(NS8390_IOPORT);
	}
	
	/*
	 * Tidy up.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD2);
	
	/*
	 * Acknowledge DMA interrupt.
	 */
	ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_RDC);
	
	count = hdr.ph_size - sizeof(struct ns8390_pkt_header);

	/*
	 * Is our next page number correct?  We do a little sanity check!
	 */
	nextpg = bnry + ((hdr.ph_size + 4 + 255) >> 8);
	if (nextpg >= ring_stop_pg) {
		nextpg -= (ring_stop_pg - ring_start_pg);
	}
	
	/*
	 * Sanity check the packet size - if it's not sane then purge the
	 * receive buffers and try to continue.
	 */
	if ((hdr.ph_nextpg != nextpg) || (count < 60) || (count > 1518)) {
		bnry = curr - 1;
		if (bnry < ring_start_pg) {
			bnry = ring_stop_pg - 1;
		}
		ns8390_write8(NS8390_PG0_BNRY, bnry);
		
		return NULL;
	}
	
	pkt = (u8_t *)membuf_alloc(count, NULL);
	buf = (u16_t *)pkt;
		
	/*
	 * Set up to DMA the packet back - we ignore the header (we already
	 * have it if we want it).
	 */
	ns8390_write8(NS8390_PG0_RBCR0, count & 0xff);
	ns8390_write8(NS8390_PG0_RBCR1, (count >> 8) & 0xff);
	ns8390_write8(NS8390_PG0_RSAR0, sizeof(struct ns8390_pkt_header));
	ns8390_write8(NS8390_PG0_RSAR1, bnry);

	/*
	 * Perform the read.
	 */	
	words = count >> 1;
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD0);
	for (i = 0; i < words; i++) {
		*buf++ = ns8390_read16(NS8390_IOPORT);
	}

	if (count & 0x01) {
		*((u8_t *)buf) = ns8390_read8(NS8390_IOPORT);
	}

	/*
	 * Tidy up.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD2);
	
	/*
	 * Acknowledge interrupt.
	 */
	ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_RDC);
	
	/*
	 * Move the boundary pointer along the buffer.
	 */
	nextpg = hdr.ph_nextpg - 1;
	if (nextpg < ring_start_pg) {
		nextpg = ring_stop_pg - 1;
	}
	ns8390_write8(NS8390_PG0_BNRY, nextpg);

	nb = netbuf_alloc();
	nb->nb_datalink_membuf = pkt;
	nb->nb_datalink = pkt;
	nb->nb_datalink_size = count;
		
	return nb;
}

/*
 * ns8390_send_set_packet()
 */
static void ns8390_send_set_packet(struct netbuf *nb)
{
	u16_t sz;
	u16_t i;
	u16_t *p;
	u8_t padding = 0;
	u16_t words;

        /*
         * Work out how many bytes we're actually going to be sending!
         */
	sz = nb->nb_datalink_size + nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size;
        if (sz >= 0x0600) {
		debug_print_pstr("\fne2000 sd: ");
		debug_print16(sz);
		return;
	}

	if (sz < 60) {
		padding = (u8_t)(60 - sz);
		sz = 60;
	}

	/*
	 * Set up to DMA the packet to the card.
	 */
	ns8390_write8(NS8390_PG0_RBCR0, sz & 0xff);
	ns8390_write8(NS8390_PG0_RBCR1, (sz >> 8) & 0xff);
	ns8390_write8(NS8390_PG0_RSAR0, 0x00);
	ns8390_write8(NS8390_PG0_RSAR1, send_start_pg);
	
	/*
	 * Write out the different sections of the network buffer.  Note that
	 * we make the rather sweeping assumption that only the application
	 * layer has any odd-sized segments.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD1);
	
	words = nb->nb_datalink_size >> 1;
	p = nb->nb_datalink;
	while (words) {
		ns8390_write16(NS8390_IOPORT, *p++);
		words--;
	}

	words = nb->nb_network_size >> 1;
	p = nb->nb_network;
	while (words) {
		ns8390_write16(NS8390_IOPORT, *p++);
		words--;
	}

	words = nb->nb_transport_size >> 1;
	p = nb->nb_transport;
	while (words) {
		ns8390_write16(NS8390_IOPORT, *p++);
		words--;
	}

	words = nb->nb_application_size >> 1;
	p = nb->nb_application;
	while (words) {
		ns8390_write16(NS8390_IOPORT, *p++);
		words--;
	}
	if (nb->nb_application_size & 0x1) {
		ns8390_write8(NS8390_IOPORT, *(u8_t *)p);
	}

	/*
	 * Pad the transmission if it's not big enough.
	 */
	words = padding >> 1;
	for (i = 0; i < words; i++) {
		ns8390_write16(NS8390_IOPORT, 0);
	}
	if (padding & 0x1) {
		ns8390_write8(NS8390_IOPORT, 0);
	}
	
	/*
	 * Tidy up.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD2);
	
	/*
	 * Check that we have a DMA complete flag.
	 */
	for (i = 0; i <= 2000; i++) {
		if (ns8390_read8(NS8390_PG0_ISR) & NS8390_ISR_RDC) {
			break;
		}
	}

	if (i >= 2000) {
		debug_print_pstr("\fpkt snd DMA nc:");
		debug_print8(ns8390_read8(NS8390_PG0_ISR));
		while (1);
	}
		
	/*
	 * Acknowledge interrupt.
	 */
	ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_RDC);
		
	/*
	 * Trigger the send.
	 */
	ns8390_write8(NS8390_PG0_TBCR0, (sz & 0xff));
	ns8390_write8(NS8390_PG0_TBCR1, ((sz >> 8) & 0xff));
	ns8390_write8(NS8390_PG0_TPSR, send_start_pg);
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_TXP | NS8390_CR_RD2);

	tx_available = 0;
}

/*
 * ns8390_ring_overflow()
 *
 * The NS8390 does bizarre things when the ring buffer overflows.  Nat-semi's
 * datasheets state:
 *
 * 	"The following procedure is required to recover from a Receiver
 *	Buffer Ring Overflow.  If this routine is not adhered to, the
 *	NIC may act in an unpredictable manner"
 *
 * Unfortunately they forgot to mention that unpredictable can mean virtually
 * completely locking up when working on a flooded network :-(
 *
 * This is so irritating that I've inserted (a shortened version of) their
 * recovery steps as comments.
 */
static void ns8390_ring_overflow(struct ethdev_instance *edi)
{
	u32_t i;
	u8_t tx_in_progress;
        u8_t resend = 1;
	struct netbuf *nb;
	struct ethdev_client *edc;
	u8_t tx_available_save;

	/*
	 * 1. Read and store the value of TXP.
	 */
	tx_in_progress = ns8390_read8(NS8390_CR) & NS8390_CR_TXP;

	/*
	 * 2. Issue the STOP command.
	 */	
	ns8390_write8(NS8390_CR, NS8390_CR_STP | NS8390_CR_RD2);

	/*
	 * 3. Wait for at least 1.6 ms.
	 */
	for (i = 0; i < 100000; i++);

	/*
	 * 4. Clear the NIC's remote byte count registers.
	 */
	ns8390_write8(NS8390_PG0_RBCR0, 0);
	ns8390_write8(NS8390_PG0_RBCR1, 0);

	/*
	 * 5. Read the stored value of TXP from step 1.  If this value is a 1
	 * read the NIC's interrupt status register.  If either the PTX or TXE
	 * bits are set, set the "resend" variable to 0.
	 */
	if (ns8390_read8(NS8390_PG0_ISR) & (NS8390_ISR_PTX | NS8390_ISR_TXE)) {
		resend = 0;
	}

	/*
	 * 6. Place the NIC in either mode 1 or mode 2 loopback.
	 */
	ns8390_write8(NS8390_PG0_TCR, NS8390_TCR_LB0);

	/*
	 * 7. Issue the START command.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD2);

	/*
	 * 8. Remove one or more packets from the receive buffer ring.
	 */
	edc = edi->edi_client;	        		
	if (edc) {
		ethdev_client_ref(edc);
	}

	tx_available_save = tx_available;
	tx_available = FALSE;

	while ((nb = ns8390_recv_get_packet()) != NULL) {
       		spinlock_unlock(&dev_lock);
		if (edc) {
			edc->edc_recv(edc, nb);
		}
	        spinlock_lock(&dev_lock);
		netbuf_deref(nb);
	}
	
	tx_available = tx_available_save;
	
	if (edc) {
		ethdev_client_deref(edc);
	}
	
	/*
	 * 9. Reset the OVW bit in the interrupt status register.
	 */
	ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_OVW);
	
	/*
	 * 10. Take the NIC out of loopback.
	 */
        ns8390_write8(NS8390_PG0_TCR, 0x00);

	/*
	 * 11. If the resend variable is set then reissue the transmit command.
	 */
	if (resend) {
		ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_TXP | NS8390_CR_RD2);
	}
}

/*
 * ne2000_intr_thread()
 */
void ne2000_intr_thread(void *arg) __attribute__ ((noreturn));
void ne2000_intr_thread(void *arg)
{
	struct ethdev_instance *edi;
	
	edi = (struct ethdev_instance *)arg;

	dev_isr_ctx = current_context;
	
	pic_enable(5);
	
	while (1) {
	        u8_t isr;

		isr_disable();
		debug_set_lights(0x30);
		isr_spinlock_lock(&dev_isr_lock);
		
	        isr = ns8390_read8(NS8390_PG0_ISR);
		while ((isr & (NS8390_ISR_PRX | NS8390_ISR_PTX | NS8390_ISR_OVW)) == 0x00) {
			isr_context_wait(&dev_isr_lock);
		        isr = ns8390_read8(NS8390_PG0_ISR);
		}

		isr_spinlock_unlock(&dev_isr_lock);
		isr_enable();
		
		spinlock_lock(&dev_lock);

		if (isr & NS8390_ISR_OVW) {
			ns8390_ring_overflow(edi);
		}
		
		if (isr & NS8390_ISR_PRX) {
			struct netbuf *nb;
			struct ethdev_client *edc;

        		edc = edi->edi_client;
        		nb = ns8390_recv_get_packet();

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
				ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_PRX);
			}
		}

		if (isr & NS8390_ISR_PTX) {
			/*
			 * Reset the ISR flag.
			 */
			ns8390_write8(NS8390_PG0_ISR, NS8390_ISR_PTX);
		
			/*
			 * See if there's any more transmits pending.
			 */
			tx_available = 1;
			
	                if (send_queue) {
	                        struct netbuf *nb;
				
				nb = (struct netbuf *)send_queue;
				send_queue = nb->nb_next;
				ns8390_send_set_packet(nb);
				
				netbuf_deref(nb);
			}
		}

		spinlock_unlock(&dev_lock);
	}
}

/*
 * ne2000_send_netbuf()
 */
void ne2000_send_netbuf(struct netbuf *nb)
{
	spinlock_lock(&dev_lock);
		
	if (tx_available) {
		ns8390_send_set_packet(nb);
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
 * ns8390_init()
 *	Initialize the 8390 in way that Nat Semi intended :-)
 *
 * We assume that on entry our caller has already ensured that any memory
 * bus timings have been adjusted to talk to the 8390 OK.
 */
void ns8390_init(u8_t begin_pg, u8_t end_pg, u8_t *mac_addr)
{
        send_start_pg = begin_pg + 8;
        ring_start_pg = begin_pg + TX_PAGES;
        ring_stop_pg = end_pg;

	/*
	 * Select device stopped, no DMA and register page 0.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STP | NS8390_CR_RD2);

	/*
	 * Now set word-wide access and FIFO threshold to 8 bytes.
	 */
	ns8390_write8(NS8390_PG0_DCR, NS8390_DCR_WTS | NS8390_DCR_LS | NS8390_DCR_FT1);

	/*
	 * Clear down the remote byte count registers.
	 */
	ns8390_write8(NS8390_PG0_RBCR0, 0);
	ns8390_write8(NS8390_PG0_RBCR1, 0);
	
	/*
	 * Set receiver and transmitter modes (temporarily loopback).
	 */
	ns8390_write8(NS8390_PG0_RCR, 0x00);
	ns8390_write8(NS8390_PG0_TCR, NS8390_TCR_LB0);
	
	/*
	 * Setup the transmit pages and receive ring buffer.
	 */
	ns8390_write8(NS8390_PG0_TPSR, begin_pg);
	ns8390_write8(NS8390_PG0_PSTART, begin_pg + TX_PAGES);
	
	ns8390_write8(NS8390_PG0_BNRY, end_pg - 1);
	ns8390_write8(NS8390_PG0_PSTOP, end_pg);

	/*
	 * Switch to page 1 and copy our MAC address into the 8390.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STP | NS8390_CR_RD2 | NS8390_CR_PS0);
	ns8390_write8(NS8390_PG1_PAR0, mac_addr[0]);
	ns8390_write8(NS8390_PG1_PAR1, mac_addr[1]);
	ns8390_write8(NS8390_PG1_PAR2, mac_addr[2]);
	ns8390_write8(NS8390_PG1_PAR3, mac_addr[3]);
	ns8390_write8(NS8390_PG1_PAR4, mac_addr[4]);
	ns8390_write8(NS8390_PG1_PAR5, mac_addr[5]);

	/*
	 * Setup multicast support to allow all packets.
	 */
	ns8390_write8(NS8390_PG1_MAR0, 0xff);
	ns8390_write8(NS8390_PG1_MAR1, 0xff);
	ns8390_write8(NS8390_PG1_MAR2, 0xff);
	ns8390_write8(NS8390_PG1_MAR3, 0xff);
	ns8390_write8(NS8390_PG1_MAR4, 0xff);
	ns8390_write8(NS8390_PG1_MAR5, 0xff);
	ns8390_write8(NS8390_PG1_MAR6, 0xff);
	ns8390_write8(NS8390_PG1_MAR7, 0xff);

	/*
	 * Complete the receive ring buffer setup.
	 */
	ns8390_write8(NS8390_PG1_CURR, begin_pg + TX_PAGES);

	/*
	 * Finally, back to page 0, set the receive mode, start the interrupts
	 * going, clear down any pending interrupts and establish our transmit
	 * mode!
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STP | NS8390_CR_RD2);
	ns8390_write8(NS8390_PG0_RCR, NS8390_RCR_AB /* | NS8390_RCR_PRO */);
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD2);
	ns8390_write8(NS8390_PG0_ISR, 0xff);
	ns8390_write8(NS8390_PG0_IMR, NS8390_IMR_PRXE | NS8390_IMR_PTXE | NS8390_IMR_OVWE);
        ns8390_write8(NS8390_PG0_TCR, 0x00);
	
	spinlock_init(&dev_lock, 0x47);
	spinlock_init(&dev_isr_lock, 0x00);
}

/*
 * ne2000_init()
 */
void ne2000_init(struct ethdev_instance *edi)
{
        u8_t i;
        u16_t j;
	u8_t prom[0x10];

        /*
         * Reset board.
         */
        ns8390_write8(NS8390_RESET, ns8390_read8(NS8390_RESET));
        for (j = 0; j < 10000; j++);
        while ((ns8390_read8(NS8390_PG0_ISR) & NS8390_ISR_RST) == 0);
	ns8390_write8(NS8390_PG0_ISR, 0xff);
                		
	/*
	 * Select device stopped, no DMA and register page 0.
	 */
	ns8390_write8(NS8390_CR, NS8390_CR_STP | NS8390_CR_RD2);

	/*
	 * Now set byte-wide access and FIFO threshold to 8 bytes.
	 */
	ns8390_write8(NS8390_PG0_DCR, NS8390_DCR_LS | NS8390_DCR_FT1);

	/*
	 * Clear down the remote byte count registers.
	 */
	ns8390_write8(NS8390_PG0_RBCR0, 0);
	ns8390_write8(NS8390_PG0_RBCR1, 0);
	
	/*
	 * Mask all interrupts and clear any that may be pending.
	 */
	ns8390_write8(NS8390_PG0_IMR, 0);
	ns8390_write8(NS8390_PG0_ISR, 0xff);
			
	/*
	 * Set receiver to monitor mode and transmitter to loopback.
	 */
	ns8390_write8(NS8390_PG0_RCR, NS8390_RCR_MON);
	ns8390_write8(NS8390_PG0_TCR, NS8390_TCR_LB0);
	
	/*
	 * Set up a 0x20 byte transfer, although we're actually only going
	 * to get 0x10 useful bytes (they're duplicated).
	 */
	ns8390_write8(NS8390_PG0_RBCR0, 0x20);
	ns8390_write8(NS8390_PG0_RBCR1, 0);
	ns8390_write8(NS8390_PG0_RSAR0, 0);
	ns8390_write8(NS8390_PG0_RSAR1, 0);
	ns8390_write8(NS8390_CR, NS8390_CR_STA | NS8390_CR_RD0);

	for (i = 0; i < 0x10; i++) {
		u8_t d;
		
		prom[i] = ns8390_read8(NS8390_IOPORT);
		d = ns8390_read8(NS8390_IOPORT);
	}

	/*
	 * Establish our MAC address.
	 */
	memcpy(edi->edi_mac, prom, 6);

	/*
	 * Initialize with standard NE2000 memory buffer details.
	 */	
	ns8390_init(0x40, 0x80, prom);

	debug_print_pstr("\f");
	for (i = 0x00; i < 0x10; i++) {
		debug_print8(prom[i]);
	}
}

/*
 * ne2000_server_alloc()
 *	Allocate an 3C509 server structure.
 */
struct ethdev_server *ne2000_server_alloc(void)
{
	struct ethdev_server *eds;
	
	eds = (struct ethdev_server *)membuf_alloc(sizeof(struct ethdev_server), NULL);
	eds->eds_send = ne2000_send_netbuf;
	eds->eds_get_mac = ethdev_get_mac;
        	
	return eds;
}
/*
 * ne2000_client_attach()
 *	Attach a client to an Ethernet device.
 */
struct ethdev_server *ne2000_client_attach(struct ethdev_instance *edi, struct ethdev_client *edc)
{
	struct ethdev_server *eds = NULL;
		
	spinlock_lock(&edi->edi_lock);

	if (edi->edi_client == NULL) {
		eds = ne2000_server_alloc();
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
 * ne2000_instance_alloc()
 */
struct ethdev_instance *ne2000_instance_alloc(void)
{
        struct ethdev_instance *edi;

	edi = (struct ethdev_instance *)membuf_alloc(sizeof(struct ethdev_instance), NULL);
	edi->edi_client = NULL;
	edi->edi_client_attach = ne2000_client_attach;
	edi->edi_client_detach = ethdev_client_detach;

	spinlock_init(&edi->edi_lock, 0x47);

	ne2000_init(edi);
	
	thread_create(ne2000_intr_thread, edi, 0x1000, 0x90);

	return edi;
}
