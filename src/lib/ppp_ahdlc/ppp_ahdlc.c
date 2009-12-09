/*
 * ppp_ahdlc.c
 *	Point-to-point protocol async framing support.
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
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "heap.h"
#include "thread.h"
#include "membuf.h"
#include "netbuf.h"
#include "uart.h"
#include "ppp_ahdlc.h"

/*
 * Maximum packet size.
 */
#define MRU 1500

/*
 * PPP async framing information.
 */
#define PPP_FLAG 0x7e
#define PPP_ESC 0x7d

/*
 * Structure used for serialization hints.
 */
struct ppp_ahdlc_hint {
	u32_t pah_accm;			/* ACCM bitmask */
};

/*
 * Table space for the PPP CRC table and a lock to protect it during initialization.
 */
static struct lock ppp_crc_lock = {0, 0x78, 0xff};
static u16_t *ppp_crc = NULL;

/*
 * update_crc()
 */
static u16_t update_crc(u16_t orig_crc, u8_t val)
{
	return ((orig_crc >> 8) ^ ppp_crc[val ^ (orig_crc & 0xff)]);
}

/*
 * ppp_ahdlc_recv_u8()
 */
void ppp_ahdlc_recv_u8(void *clnt, u8_t ch)
{
	struct uart_client *uc;
	struct ppp_ahdlc_instance *pai;
		
	uc = (struct uart_client *)clnt;
	pai = (struct ppp_ahdlc_instance *)uc->uc_instance;

	spinlock_lock(&pai->pai_lock);
			
	if (ch == PPP_FLAG) {
		if (!pai->pai_recv_ignore && pai->pai_recv_octets) {
			/*
			 * Sanity check the received frame - look at standard
			 * bytes within it and the calculated CRC.
			 */
			if ((pai->pai_recv_crc == 0xf0b8)
					|| (pai->pai_recv_packet[0] == 0xff)
					|| (pai->pai_recv_packet[1] == 0x03)) {
				struct netbuf *nb;
				struct ppp_ahdlc_client *pac;

				/*
				 * When we build the netbuf to be passed upwards, we get
				 * a case of convenient memory syndrome and forget all
				 * about the framing.
				 */
				nb = netbuf_alloc();
				nb->nb_datalink_membuf = pai->pai_recv_packet;
				nb->nb_datalink = pai->pai_recv_packet + 2;
				nb->nb_datalink_size = pai->pai_recv_octets - 4;
	                			
				pac = pai->pai_client;
				if (pac) {
					ppp_ahdlc_client_ref(pac);
					spinlock_unlock(&pai->pai_lock);
					pai->pai_client->pac_recv(pai->pai_client, nb);
					spinlock_lock(&pai->pai_lock);
					ppp_ahdlc_client_deref(pac);
				}
						
				netbuf_deref(nb);

				pai->pai_recv_packet = membuf_alloc(MRU, NULL);
			}
		}

 		pai->pai_recv_octets = 0;
		pai->pai_recv_esc = FALSE;
		pai->pai_recv_ignore = FALSE;
		pai->pai_recv_crc = 0xffff;
	} else {
		if (!pai->pai_recv_ignore) {
                	if (ch == PPP_ESC) {
               			if (pai->pai_recv_esc) {
					pai->pai_recv_ignore = TRUE;
               			} else {
					pai->pai_recv_esc = TRUE;
				}
			} else {
				if (pai->pai_recv_esc) {
					ch ^= 0x20;
					pai->pai_recv_esc = FALSE;
				}
                		
				pai->pai_recv_packet[pai->pai_recv_octets++] = ch;
				if (pai->pai_recv_octets >= MRU) {
					pai->pai_recv_ignore = TRUE;
				}
			
				pai->pai_recv_crc = update_crc(pai->pai_recv_crc, ch);
			}
		}
	}

	spinlock_unlock(&pai->pai_lock);
}

/*
 * send_u8()
 */
static u16_t send_u8(struct uart_server *us, u8_t ch, u32_t accm, u16_t crc)
{
	crc = update_crc(crc, ch);
		
	if ((ch == PPP_FLAG) || (ch == PPP_ESC) || ((ch < 0x20) && (accm & ((u32_t)1 << ch)))) {
		us->us_send(us, PPP_ESC);
		ch ^= 0x20;
	}
		
	us->us_send(us, ch);

	return crc;
}

/*
 * ppp_ahdlc_send_sequence()
 */
static u16_t ppp_ahdlc_send_sequence(struct uart_server *us, u8_t *buf, u16_t cnt, u32_t accm, u16_t crc)
{
	while (cnt) {
		crc = send_u8(us, *buf++, accm, crc);
		cnt--;
	}

	return crc;
}

/*
 * ppp_ahdlc_send_thread()
 */
void ppp_ahdlc_send_thread(void *arg) __attribute__ ((noreturn));
void ppp_ahdlc_send_thread(void *arg)
{
	struct ppp_ahdlc_instance *pai;
	struct ppp_ahdlc_client *pac;
	struct uart_server *us;
		
	pai = (struct ppp_ahdlc_instance *)arg;
	us = pai->pai_server;
		
	pai->pai_send_ctx = current_context;
	us->us_set_send_isr();
	
	/*
	 * Look to see if we need to issue an "up" notice to our client.
	 */
	spinlock_lock(&pai->pai_lock);
	pac = pai->pai_client;
	if (pac && pac->pac_link_up) {
		ppp_ahdlc_client_ref(pac);	
		spinlock_unlock(&pai->pai_lock);
		pac->pac_link_up(pac);
		ppp_ahdlc_client_deref(pac);
	} else {
		spinlock_unlock(&pai->pai_lock);
	}
	
	while (1) {
                struct netbuf *nb;
		struct ppp_ahdlc_hint *hint;
		u16_t crc = 0xffff;

		/*
		 * Wait until we've got a packet to send.  If there's nothing
		 * to do then we go to sleep.
		 */
		spinlock_lock(&pai->pai_lock);
		
                while (!pai->pai_send_queue) {
                	context_wait(&pai->pai_lock);
                }
		nb = pai->pai_send_queue;
		pai->pai_send_queue = nb->nb_next;
		
		spinlock_unlock(&pai->pai_lock);

		/*
		 * We've got work to do now, so get on with it.
		 */
		hint = (struct ppp_ahdlc_hint *)nb->nb_hint_membuf;
		us->us_send(us, PPP_FLAG);

		crc = send_u8(us, 0xff, hint->pah_accm, crc);
		crc = send_u8(us, 0x03, hint->pah_accm, crc);
			
		crc = ppp_ahdlc_send_sequence(us, nb->nb_datalink, nb->nb_datalink_size, hint->pah_accm, crc);
		crc = ppp_ahdlc_send_sequence(us, nb->nb_network, nb->nb_network_size, hint->pah_accm, crc);
		if (nb->nb_transport_size) {
			crc = ppp_ahdlc_send_sequence(us, nb->nb_transport, nb->nb_transport_size, hint->pah_accm, crc);
		}
		if (nb->nb_application_size) {
			crc = ppp_ahdlc_send_sequence(us, nb->nb_application, nb->nb_application_size, hint->pah_accm, crc);
		}

		crc = ~crc;
		send_u8(us, (u8_t)(crc & 0xff), hint->pah_accm, 0);
		send_u8(us, (u8_t)((crc >> 8) & 0xff), hint->pah_accm, 0);
		                		
		us->us_send(us, PPP_FLAG);
	
		netbuf_deref(nb);
	}
}

/*
 * ppp_ahdlc_send_netbuf()
 */
void ppp_ahdlc_send_netbuf(void *srv, struct netbuf *nb)
{
	struct ppp_ahdlc_server *pas;
	struct ppp_ahdlc_instance *pai;
	struct netbuf *p, **pprev;
	struct ppp_ahdlc_hint *hint;

	pas = (struct ppp_ahdlc_server *)srv;
	pai = (struct ppp_ahdlc_instance *)pas->pas_instance;
				
	netbuf_ref(nb);
	
	spinlock_lock(&pai->pai_lock);
		
	pprev = &pai->pai_send_queue;
	p = pai->pai_send_queue;
	
	while (p) {
		pprev = &p->nb_next;
		p = p->nb_next;
	}

	*pprev = nb;
	nb->nb_next = NULL;

	hint = membuf_alloc(sizeof(struct ppp_ahdlc_hint), NULL);
	hint->pah_accm = pai->pai_accm;
	nb->nb_hint_membuf = hint;

	if (pai->pai_send_ctx) {
		context_signal(pai->pai_send_ctx);
	}
	
	spinlock_unlock(&pai->pai_lock);
}

/*
 * ppp_ahdlc_set_accm()
 */
void ppp_ahdlc_set_accm(void *srv, u32_t accm)
{
	struct ppp_ahdlc_server *pas;
	struct ppp_ahdlc_instance *pai;
	
	pas = (struct ppp_ahdlc_server *)srv;
	pai = (struct ppp_ahdlc_instance *)pas->pas_instance;
	
	spinlock_lock(&pai->pai_lock);
	pai->pai_accm = accm;	
	spinlock_unlock(&pai->pai_lock);
}

/*
 * ppp_ahdlc_get_accm()
 */
u32_t ppp_ahdlc_get_accm(void *srv)
{
	u32_t accm;
	struct ppp_ahdlc_server *pas;
	struct ppp_ahdlc_instance *pai;
	
	pas = (struct ppp_ahdlc_server *)srv;
	pai = (struct ppp_ahdlc_instance *)pas->pas_instance;
	
	spinlock_lock(&pai->pai_lock);
	accm = pai->pai_accm;	
	spinlock_unlock(&pai->pai_lock);

	return accm;
}

/*
 * ppp_ahdlc_server_alloc()
 *	Allocate a PPP async framing server structure.
 */
struct ppp_ahdlc_server *ppp_ahdlc_server_alloc(void)
{
	struct ppp_ahdlc_server *pas;
	
	pas = (struct ppp_ahdlc_server *)membuf_alloc(sizeof(struct ppp_ahdlc_server), NULL);
        pas->pas_send = ppp_ahdlc_send_netbuf;
	pas->pas_set_accm = ppp_ahdlc_set_accm;
	pas->pas_get_accm = ppp_ahdlc_get_accm;

	return pas;
}

/*
 * ppp_ahdlc_client_attach()
 *	Attach a client to the PPP async framing driver.
 */
struct ppp_ahdlc_server *ppp_ahdlc_client_attach(struct ppp_ahdlc_instance *pai, struct ppp_ahdlc_client *pac)
{
	struct ppp_ahdlc_server *pas = NULL;

	spinlock_lock(&pai->pai_lock);

	if (pai->pai_client == NULL) {
		pas = ppp_ahdlc_server_alloc();
                pas->pas_instance = pai;
                ppp_ahdlc_instance_ref(pai);

		pac->pac_server = pas;
		pai->pai_client = pac;
		ppp_ahdlc_client_ref(pac);
	}
		
	spinlock_unlock(&pai->pai_lock);

	return pas;
}

/*
 * ppp_ahdlc_client_alloc()
 *	Allocate a PPP async framing client structure.
 */
struct ppp_ahdlc_client *ppp_ahdlc_client_alloc(void)
{
	struct ppp_ahdlc_client *pac;
	
	pac = (struct ppp_ahdlc_client *)membuf_alloc(sizeof(struct ppp_ahdlc_client), NULL);
	pac->pac_instance = NULL;
	pac->pac_link_up = NULL;
	pac->pac_link_down = NULL;
	
	return pac;
}

/*
 * ppp_ahdlc_client_detach()
 *	Detach a client from the PPP async framing driver.
 */
void ppp_ahdlc_client_detach(struct ppp_ahdlc_instance *pai, struct ppp_ahdlc_server *pas)
{
	spinlock_lock(&pai->pai_lock);

	if (pai->pai_client->pac_server == pas) {
		ppp_ahdlc_server_deref(pas);
		ppp_ahdlc_instance_deref(pai);
		ppp_ahdlc_client_deref(pai->pai_client);
		pai->pai_client = NULL;
	}
		
	spinlock_unlock(&pai->pai_lock);
}

/*
 * ppp_ahdlc_instance_alloc()
 */
struct ppp_ahdlc_instance *ppp_ahdlc_instance_alloc(struct uart_instance *ui)
{
	struct ppp_ahdlc_instance *pai;
        struct uart_client *uc;

        /*
         * Is this the first instance of the PPP async service?  If it is then
         * we need to create the CRC table.
         */
	spinlock_lock(&ppp_crc_lock);
	if (!ppp_crc) {
                int i;
				
		ppp_crc = heap_alloc(sizeof(u16_t) * 256);
		for (i = 0; i < 256; i++) {
			u16_t zx;
			
			zx = (i ^ (i << 4)) & 0xff;
			ppp_crc[i] = (zx << 8) ^ (zx << 3) ^ (zx >> 4);
		}
	}
	spinlock_unlock(&ppp_crc_lock);

	pai = (struct ppp_ahdlc_instance *)membuf_alloc(sizeof(struct ppp_ahdlc_instance), NULL);
	spinlock_init(&pai->pai_lock, 0x10);
	pai->pai_client = NULL;
	pai->pai_client_attach = ppp_ahdlc_client_attach;
	pai->pai_client_detach = ppp_ahdlc_client_detach;
	pai->pai_server = NULL;
	pai->pai_recv_esc = FALSE;
	pai->pai_recv_octets = 0;
	pai->pai_recv_ignore = TRUE;
	pai->pai_recv_packet = membuf_alloc(MRU, NULL);
	pai->pai_accm = 0xffffffff;
	pai->pai_send_queue = NULL;

	thread_create(ppp_ahdlc_send_thread, pai, 0x1000, 0x89);

	/*
	 * Attach this protocol handler to the UART.
	 */
	uc = uart_client_alloc();
	uc->uc_recv = ppp_ahdlc_recv_u8;
	uc->uc_instance = pai;
	ppp_ahdlc_instance_ref(pai);
	
	pai->pai_server = ui->ui_client_attach(ui, uc);
	uart_client_deref(uc);
	
	return pai;
}
