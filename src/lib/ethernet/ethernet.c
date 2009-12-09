/*
 * ethernet.c
 *	Ethernet device support.
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
#include "ethdev.h"
#include "ethernet.h"

/*
 * ethernet_recv_netbuf()
 */
void ethernet_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ethdev_client *edc;
	struct ethernet_instance *ei;
	struct ethernet_client *ec;
	struct eth_phys_header *eph;

	edc = (struct ethdev_client *)clnt;
	ei = (struct ethernet_instance *)edc->edc_instance;
		
	eph = (struct eth_phys_header *)nb->nb_datalink;
	
	/*
	 * Route the netbuf to the appropriate network layer.
	 */
	spinlock_lock(&ei->ei_lock);
	
	ec = ei->ei_client_list;
	while (ec && (ec->ec_type != hton16(eph->eph_type))) {
		ec = ec->ec_next;
	}

	if (ec) {
		/*
		 * Once we know that we're going to do something more with this
		 * packet we update its information.
		 */
		nb->nb_network = eph + 1;
		nb->nb_network_size = nb->nb_datalink_size - sizeof(struct eth_phys_header);
		nb->nb_datalink_size = sizeof(struct eth_phys_header);
		
		ethernet_client_ref(ec);	
		spinlock_unlock(&ei->ei_lock);
		ec->ec_recv(ec, nb);
		ethernet_client_deref(ec);
	} else {
		spinlock_unlock(&ei->ei_lock);
	}
	
}

/*
 * ethernet_send_netbuf()
 */
void ethernet_send_netbuf(void *srv, u8_t *dest_mac, struct netbuf *nb)
{
	struct ethernet_server *es;
	struct ethernet_instance *ei;
	struct eth_phys_header *eph;
	struct ethdev_server *eds;
		
	es = (struct ethernet_server *)srv;
	ei = es->es_instance;
	
	spinlock_lock(&ei->ei_lock);
	eds = ei->ei_server;
	ethdev_server_ref(eds);
	spinlock_unlock(&ei->ei_lock);

	nb->nb_datalink_membuf = membuf_alloc(sizeof(struct eth_phys_header), NULL);
	nb->nb_datalink = nb->nb_datalink_membuf;
	nb->nb_datalink_size = sizeof(struct eth_phys_header);

	eph = (struct eth_phys_header *)nb->nb_datalink;
	memcpy(eph->eph_src_mac, eds->eds_get_mac(eds), 6);
	memcpy(eph->eph_dest_mac, dest_mac, 6);
	eph->eph_type = hton16(es->es_type);

	eds->eds_send(nb);

	ethdev_server_deref(eds);
}

/*
 * ethernet_get_mac()
 */
u8_t *ethernet_get_mac(struct ethernet_server *es)
{
	return es->es_instance->ei_server->eds_get_mac(es->es_instance->ei_server);
}

/*
 * ethernet_server_alloc()
 *	Allocate an Ethernet server structure.
 */
struct ethernet_server *ethernet_server_alloc(void)
{
	struct ethernet_server *es;
	
	es = (struct ethernet_server *)membuf_alloc(sizeof(struct ethernet_server), NULL);
        es->es_send = ethernet_send_netbuf;
	es->es_get_mac = ethernet_get_mac;

	return es;
}

/*
 * ethernet_client_alloc()
 *	Allocate an Ethernet client structure.
 */
struct ethernet_client *ethernet_client_alloc(void)
{
	struct ethernet_client *ec;
	
	ec = (struct ethernet_client *)membuf_alloc(sizeof(struct ethernet_client), NULL);
        ec->ec_recv = NULL;
	ec->ec_type = 0;
	ec->ec_instance = NULL;
		
	return ec;
}

/*
 * ethernet_client_attach()
 *	Attach a client to the Ethernet driver.
 */
struct ethernet_server *ethernet_client_attach(struct ethernet_instance *ei, struct ethernet_client *ec)
{
        struct ethernet_client *p;
        struct ethernet_server *es = NULL;
        	
	spinlock_lock(&ei->ei_lock);

	/*
	 * Walk our list of clients and check that we can do this request.
	 */
	p = ei->ei_client_list;
	while (p && (p->ec_type != ec->ec_type)) {
		p = p->ec_next;
	}

	if (!p) {
		es = ethernet_server_alloc();		
		es->es_type = ec->ec_type;
		es->es_instance = ei;
		ethernet_instance_ref(ei);

		ec->ec_server = es;
		ec->ec_next = ei->ei_client_list;
		ei->ei_client_list = ec;
		ethernet_client_ref(ec);
	}
				
	spinlock_unlock(&ei->ei_lock);

	return es;
}

/*
 * ethernet_client_detach()
 *	Detach a client from the Ethernet driver.
 */
void ethernet_client_detach(struct ethernet_instance *ei, struct ethernet_server *es)
{
        struct ethernet_client *ec;
        struct ethernet_client **ecprev;
	
	spinlock_lock(&ei->ei_lock);

	/*
	 * Walk our list of clients and find the one we're to remove.
	 */
	ec = ei->ei_client_list;
	ecprev = &ei->ei_client_list;
	while (ec) {
		if (ec->ec_server == es) {
			*ecprev = ec->ec_next;
			ethernet_server_deref(es);
			ethernet_instance_deref(ei);
			ethernet_client_deref(ec);
			break;
		}
	
		ecprev = &ec->ec_next;
		ec = ec->ec_next;
	}
		
	spinlock_unlock(&ei->ei_lock);
}

/*
 * ethernet_instance_alloc()
 */
struct ethernet_instance *ethernet_instance_alloc(struct ethdev_instance *edi)
{
	struct ethernet_instance *ei;
	struct ethdev_client *edc;

	ei = (struct ethernet_instance *)membuf_alloc(sizeof(struct ethernet_instance), NULL);
	ei->ei_client_attach = ethernet_client_attach;
	ei->ei_client_detach = ethernet_client_detach;
	ei->ei_client_list = NULL;
	ei->ei_server = NULL;
	spinlock_init(&ei->ei_lock, 0x10);

	/*
	 * Attach this handler to an Ethernet device.
	 */
	edc = ethdev_client_alloc();
	edc->edc_recv = ethernet_recv_netbuf;
	edc->edc_instance = ei;
	ethernet_instance_ref(ei);
	
	ei->ei_server = edi->edi_client_attach(edi, edc);
	ethdev_client_deref(edc);

	return ei;
}
