/*
 * slip.c
 *	Serial Line IP support.
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
#include "thread.h"
#include "membuf.h"
#include "netbuf.h"
#include "uart.h"
#include "ip_datalink.h"
#include "slip.h"

/*
 * Maximum receive packet size.
 */
#define MRU 576

/*
 * Magic characters in the serial data stream used for special purposes by the
 * SLIP encoding.
 */
#define SLIP_END 0xc0
#define SLIP_ESC 0xdb

/*
 * slip_recv_u8()
 */
void slip_recv_u8(void *clnt, u8_t ch)
{
	struct uart_client *uc;
	struct ip_datalink_instance *idi;
	struct slip_instance *si;
		
	uc = (struct uart_client *)clnt;
	idi = (struct ip_datalink_instance *)uc->uc_instance;
	si = (struct slip_instance *)idi;

	spinlock_lock(&idi->idi_lock);
			
	if (ch == SLIP_END) {
		if (!si->si_recv_ignore && si->si_recv_octets) {
			struct netbuf *nb;
			struct ip_datalink_client *idc;

			nb = netbuf_alloc();
			nb->nb_network_membuf = si->si_recv_packet;
			nb->nb_network = si->si_recv_packet;
			nb->nb_network_size = si->si_recv_octets;
	                			
			idc = idi->idi_client;
			if (idc) {
				ip_datalink_client_ref(idc);
				spinlock_unlock(&idi->idi_lock);
				idi->idi_client->idc_recv(idi->idi_client, nb);
				spinlock_lock(&idi->idi_lock);
				ip_datalink_client_deref(idc);
			}
						
			netbuf_deref(nb);
						
			si->si_recv_packet = membuf_alloc(MRU, NULL);
		}

		si->si_recv_octets = 0;
		si->si_recv_esc = FALSE;
		si->si_recv_ignore = FALSE;
	} else {
		if (!si->si_recv_ignore) {
			if (ch == SLIP_ESC) {
				if (si->si_recv_esc) {
					si->si_recv_ignore = TRUE;
				} else {
					si->si_recv_esc = TRUE;
				}
			} else {
				if (si->si_recv_esc) {
					if (ch == 0xdc) {
						ch = SLIP_END;
					} else if (ch == 0xdd) {
						ch = SLIP_ESC;
					} else {
						si->si_recv_ignore = TRUE;
						spinlock_unlock(&idi->idi_lock);
						return;
					}

					si->si_recv_esc = FALSE;
				}

				si->si_recv_packet[si->si_recv_octets++] = ch;
				if (si->si_recv_octets >= MRU) {
					si->si_recv_ignore = TRUE;
				}
			}
		}
	}

	spinlock_unlock(&idi->idi_lock);
}

/*
 * slip_send_sequence()
 */
static void slip_send_sequence(struct uart_server *us, u8_t *buf, u16_t cnt)
{
	while (cnt) {
		u8_t ch;

		ch = *buf++;
		if (ch == SLIP_END) {
			us->us_send(us, SLIP_ESC);
			ch = 0xdc;
		} else if (ch == SLIP_ESC) {
			us->us_send(us, SLIP_ESC);
			ch = 0xdd;
		}

		us->us_send(us, ch);
		
		cnt--;
	}
}

/*
 * slip_send_thread()
 */
void slip_send_thread(void *arg) __attribute__ ((noreturn));
void slip_send_thread(void *arg)
{
	struct ip_datalink_instance *idi;
	struct slip_instance *si;
	struct uart_server *us;
		
	idi = (struct ip_datalink_instance *)arg;
	si = (struct slip_instance *)idi;
	us = si->si_server;
		
	si->si_send_ctx = current_context;
	us->us_set_send_isr();
	
	while (1) {
                struct netbuf *nb;
	        		
		/*
		 * Wait until we've got a packet to send.  If there's nothing
		 * to do then we go to sleep.
		 */
		spinlock_lock(&idi->idi_lock);
		
                while (!si->si_send_queue) {
                	context_wait(&idi->idi_lock);
                }
		nb = si->si_send_queue;
		si->si_send_queue = nb->nb_next;
		
		spinlock_unlock(&idi->idi_lock);

		/*
		 * We've got work to do now, so get on with it.
		 */
		us->us_send(us, SLIP_END);
	
		slip_send_sequence(us, nb->nb_network, nb->nb_network_size);
		if (nb->nb_transport_size) {
			slip_send_sequence(us, nb->nb_transport, nb->nb_transport_size);
		}
		if (nb->nb_application_size) {
			slip_send_sequence(us, nb->nb_application, nb->nb_application_size);
		}
                		
		us->us_send(us, SLIP_END);
	
		netbuf_deref(nb);
	}
}

/*
 * slip_send_netbuf()
 */
void slip_send_netbuf(void *srv, struct netbuf *nb)
{
	struct ip_datalink_server *ids;
	struct ip_datalink_instance *idi;
	struct slip_instance *si;	
	struct netbuf *p, **pprev;

	ids = (struct ip_datalink_server *)srv;
	idi = (struct ip_datalink_instance *)ids->ids_instance;
	si = (struct slip_instance *)idi;
				
	netbuf_ref(nb);
	
	spinlock_lock(&idi->idi_lock);
		
	pprev = &si->si_send_queue;
	p = si->si_send_queue;
	
	while (p) {
		pprev = &p->nb_next;
		p = p->nb_next;
	}

	*pprev = nb;
	nb->nb_next = NULL;

	context_signal(si->si_send_ctx);
	
	spinlock_unlock(&idi->idi_lock);
}

/*
 * slip_server_alloc()
 *	Allocate an SLIP server structure.
 */
struct ip_datalink_server *slip_server_alloc(void)
{
	struct ip_datalink_server *ids;
	
	ids = (struct ip_datalink_server *)membuf_alloc(sizeof(struct ip_datalink_server), NULL);
        ids->ids_send = slip_send_netbuf;
        	
	return ids;
}

/*
 * slip_client_attach()
 *	Attach a client to the SLIP driver.
 */
struct ip_datalink_server *slip_client_attach(struct ip_datalink_instance *idi, struct ip_datalink_client *idc)
{
	struct ip_datalink_server *ids = NULL;

	spinlock_lock(&idi->idi_lock);

	if (idi->idi_client == NULL) {
		ids = slip_server_alloc();
                ids->ids_instance = idi;
                ip_datalink_instance_ref(idi);

		idc->idc_server = ids;
		idi->idi_client = idc;
		ip_datalink_client_ref(idc);
	}
		
	spinlock_unlock(&idi->idi_lock);

	return ids;
}

/*
 * slip_instance_alloc()
 */
struct ip_datalink_instance *slip_instance_alloc(struct uart_instance *ui)
{
	struct slip_instance *si;
	struct ip_datalink_instance *idi;
        struct uart_client *uc;

	idi = (struct ip_datalink_instance *)membuf_alloc(sizeof(struct slip_instance), NULL);
	si = (struct slip_instance *)idi;
	spinlock_init(&idi->idi_lock, 0x10);
	idi->idi_mru = MRU;
	idi->idi_mtu = MRU;
	idi->idi_client = NULL;
	idi->idi_client_attach = slip_client_attach;
	idi->idi_client_detach = ip_datalink_client_detach;
	
	si->si_server = NULL;
	si->si_recv_esc = FALSE;
	si->si_recv_octets = 0;
	si->si_recv_ignore = TRUE;
	si->si_recv_packet = membuf_alloc(MRU, NULL);
	si->si_send_queue = NULL;
        	
	thread_create(slip_send_thread, idi, 0x1000, 0x89);

	/*
	 * Attach this protocol handler to the UART.
	 */
	uc = uart_client_alloc();
	uc->uc_recv = slip_recv_u8;
	uc->uc_instance = idi;
	ip_datalink_instance_ref(idi);
	
	si->si_server = ui->ui_client_attach(ui, uc);
	uart_client_deref(uc);
	
	return idi;
}
