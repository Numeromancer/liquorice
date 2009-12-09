/*
 * udp.c
 *	User Datagram Protocol (UDP/IP) support.
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
#include "membuf.h"
#include "netbuf.h"
#include "ipcsum.h"
#include "ip.h"
#include "udp.h"

/*
 * udp_recv_netbuf()
 */
static void udp_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ip_client *ic;
	struct udp_instance *ui;
	struct udp_client *uc;
	u16_t csum;
	struct ip_header *iph = (struct ip_header *)nb->nb_network;
	struct udp_header *udh = (struct udp_header *)nb->nb_transport;

	ic = (struct ip_client *)clnt;
	ui = (struct udp_instance *)ic->ic_instance;
        	
	/*
	 * When we get here our application data is still hooked under the
	 * transport layer, so the first thing to do is untangle it.
	 */
	nb->nb_application = udh + 1;
	nb->nb_application_size = nb->nb_transport_size - sizeof(struct udp_header);
	nb->nb_transport_size = sizeof(struct udp_header);
	
	/*
	 * If we've been given a checksum then check it!
	 */
	if (udh->uh_csum != 0) {
		csum = ipcsum_pseudo_partial(hton32(((struct ip_instance *)ic->ic_server->is_instance)->ii_addr),
						iph->ih_src_addr, 0x11, udh->uh_len);
		csum = ipcsum_partial(csum, nb->nb_transport, nb->nb_transport_size);
		csum = ipcsum(csum, nb->nb_application, nb->nb_application_size);
		if (csum != 0) {
			debug_print_pstr("\fudp recv: csum fail: ");
			debug_print16(csum);
			return;
		}
	}

	/*
	 * Look up the socket and see if we know anyone who will try and deal
	 * with it!
	 */
	spinlock_lock(&ui->ui_lock);

	uc = ui->ui_client_list;
	while (uc && (uc->uc_port != hton16(udh->uh_dest_port))) {
		uc = uc->uc_next;
	}

	if (uc) {
		udp_client_ref(uc);
		spinlock_unlock(&ui->ui_lock);
		uc->uc_recv(uc, nb);
		udp_client_deref(uc);
	} else {
	        struct ip_header *iphc;
		struct netbuf *nbrep;
		struct ip_server *is;
                u32_t empty = 0;

		is = ui->ui_server;
		ip_server_ref(is);
		spinlock_unlock(&ui->ui_lock);
		
		/*
		 * Issue an ICMP destination unreachable message (port unreachable).
		 */	
		nbrep = netbuf_alloc();
		nbrep->nb_application_membuf = membuf_alloc(nb->nb_network_size + 8, NULL);
		nbrep->nb_application = nbrep->nb_application_membuf;
	        nbrep->nb_application_size = nb->nb_network_size + 8;

		iphc = (struct ip_header *)nbrep->nb_application;
                memcpy(iphc, iph, nb->nb_network_size);
		memcpy((iphc + 1), nb->nb_transport, 8);
		
		is->is_send_icmp(is, iph->ih_src_addr, 0x03, 0x03, (u8_t *)(&empty), nbrep);
	
		ip_server_deref(is);
		netbuf_deref(nbrep);
	}
}

/*
 * udp_send_netbuf()
 *	Send a UDP netbuf.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 */
static void udp_send_netbuf(void *srv, u32_t dest_addr, u16_t dest_port, struct netbuf *nb)
{
	struct udp_server *us;
	struct udp_instance *ui;
	u16_t csum;
        struct udp_header *udh;
	struct ip_server *is;

	us = (struct udp_server *)srv;
	ui = (struct udp_instance *)us->us_instance;
        	
	nb->nb_transport_membuf = membuf_alloc(sizeof(struct udp_header), NULL);
	nb->nb_transport = nb->nb_transport_membuf;
	nb->nb_transport_size = sizeof(struct udp_header);

	udh = (struct udp_header *)nb->nb_transport;
	udh->uh_dest_port = dest_port;
	udh->uh_src_port = hton16(us->us_port);
	udh->uh_len = hton16(nb->nb_transport_size + nb->nb_application_size);
	
	udh->uh_csum = 0;
	
	spinlock_lock(&ui->ui_lock);
	is = ui->ui_server;
	ip_server_ref(is);
	spinlock_unlock(&ui->ui_lock);
	
	csum = ipcsum_pseudo_partial(hton32(((struct ip_instance *)is->is_instance)->ii_addr),
					dest_addr, 0x11, udh->uh_len);
	csum = ipcsum_partial(csum, udh, sizeof(struct udp_header));
	udh->uh_csum = ipcsum(csum, nb->nb_application, nb->nb_application_size);

	/*
	 * Pass the netbuf down to the next layer.
	 */
	is->is_send(is, dest_addr, 0x11, nb);

	ip_server_deref(is);
}

/*
 * udp_dump_stats()
 */
int udp_dump_stats(struct udp_instance *ui, struct udp_client *ubuf, int max)
{
	struct udp_client *uc;
        int ct = 0;

	spinlock_lock(&ui->ui_lock);

	uc = ui->ui_client_list;
	while (uc && (ct < max)) {
		memcpy(ubuf, uc, sizeof(struct udp_client));
		ubuf->uc_next = uc;
		ubuf++;
		ct++;
		uc = uc->uc_next;
	}
	
	spinlock_unlock(&ui->ui_lock);

	return ct;
}

/*
 * udp_server_alloc()
 *	Allocate a UDP server structure.
 */
struct udp_server *udp_server_alloc(void)
{
	struct udp_server *us;
	
	us = (struct udp_server *)membuf_alloc(sizeof(struct udp_server), NULL);
        us->us_send = udp_send_netbuf;
	
	return us;
}

/*
 * udp_client_alloc()
 *	Allocate an UDP socket structure.
 */
struct udp_client *udp_client_alloc(void)
{
	struct udp_client *uc;
	
	uc = (struct udp_client *)membuf_alloc(sizeof(struct udp_client), NULL);

	return uc;
}

/*
 * udp_client_attach()
 *	Attach an UDP client.
 */
struct udp_server *udp_client_attach(struct udp_instance *ui, struct udp_client *uc)
{
        struct udp_server *us = NULL;
        struct udp_client *p;
	
	spinlock_lock(&ui->ui_lock);

	/*
	 * Walk the socket list and check that we can do this request.
	 */
	p = ui->ui_client_list;
	while (p && (p->uc_port != uc->uc_port)) {
		p = p->uc_next;
	}

	if (!p) {
		us = udp_server_alloc();		
		us->us_port = uc->uc_port;
		us->us_instance = ui;
		udp_instance_ref(ui);

		uc->uc_server = us;
		uc->uc_next = ui->ui_client_list;
		ui->ui_client_list = uc;
		udp_client_ref(uc);
	}
				
	spinlock_unlock(&ui->ui_lock);

	return us;
}

/*
 * udp_client_detach()
 *	Detach an UDP client.
 */
void udp_client_detach(struct udp_instance *ui, struct udp_server *us)
{
        struct udp_client *uc;
        struct udp_client **ucprev;
	
	spinlock_lock(&ui->ui_lock);

	/*
	 * Walk the socket list and find the one we're to remove.
	 */
	uc = ui->ui_client_list;
	ucprev = &ui->ui_client_list;
	while (uc) {
		if (uc->uc_server == us) {
			*ucprev = uc->uc_next;
			udp_client_deref(uc);
			udp_instance_deref(ui);
			udp_server_deref(us);
			break;
		}
	
		ucprev = &uc->uc_next;
		uc = uc->uc_next;
	}
			
	spinlock_unlock(&ui->ui_lock);
}

/*
 * udp_instance_alloc()
 */
struct udp_instance *udp_instance_alloc(struct ip_instance *ii)
{
	struct udp_instance *ui;
	struct ip_client *ic;

	ui = (struct udp_instance *)membuf_alloc(sizeof(struct udp_instance), NULL);
	ui->ui_client_attach = udp_client_attach;
	ui->ui_client_detach = udp_client_detach;
	ui->ui_client_list = NULL;
	spinlock_init(&ui->ui_lock, 0x24);

	/*
	 * Attach this protocol handler to the IP stack.
	 */
	ic = ip_client_alloc();
	ic->ic_protocol = 0x11;
	ic->ic_recv = udp_recv_netbuf;
	ic->ic_recv_icmp = NULL;
	ic->ic_instance = ui;
	udp_instance_ref(ui);
	
	ui->ui_server = ii->ii_ip_client_attach(ii, ic);
	ip_client_deref(ic);
	
	return ui;
}
