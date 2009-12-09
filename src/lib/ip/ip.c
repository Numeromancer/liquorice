/*
 * ip.c
 *	Internet Protocol support.
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
#include "ip_datalink.h"
#include "ip.h"
#include "ipcsum.h"

void icmp_issue_netbuf(struct ip_instance *ii, u32_t dest_addr, u8_t type, u8_t code, u8_t *extra, struct netbuf *nb);
void ip_issue_netbuf(struct ip_instance *ii, u32_t dest_addr, u8_t protocol, struct netbuf *nb);

/*
 * icmp_recv_netbuf_echo_request()
 */
static void icmp_recv_netbuf_echo_request(struct ip_instance *ii, struct netbuf *nb)
{
        struct ip_header *iph;
        struct icmp_header *ich;
	struct netbuf *nbrep;
	
	iph = nb->nb_network;
	ich = nb->nb_transport;

	/*
	 * Issue an ICMP echo reply.
	 */	
	nbrep = netbuf_alloc();
	nbrep->nb_application_membuf = membuf_alloc(nb->nb_application_size, NULL);
        nbrep->nb_application = nbrep->nb_application_membuf;
        nbrep->nb_application_size = nb->nb_application_size;

	memcpy(nbrep->nb_application, nb->nb_application, nb->nb_application_size);

	icmp_issue_netbuf(ii, iph->ih_src_addr, 0x00, 0x00, (u8_t *)(ich + 1), nbrep);

	netbuf_deref(nbrep);
}

/*
 * icmp_recv_netbuf_notify_transport()
 *
 * We've received an ICMP message that needs to be forwarded to the appropriate
 * transport layer for further processing!
 */
static void icmp_recv_netbuf_notify_transport(struct ip_instance *ii, struct netbuf *nb)
{
	struct ip_header *iph;
	struct ip_client *ic;

	iph = (struct ip_header *)nb->nb_application;

	spinlock_lock(&ii->ii_lock);

	ic = ii->ii_client_list;
	while (ic && (ic->ic_protocol != iph->ih_protocol)) {
		ic = ic->ic_next;
	}

	if (ic && (ic->ic_recv_icmp)) {
		ip_client_ref(ic);
		spinlock_unlock(&ii->ii_lock);
		ic->ic_recv_icmp(ic, nb);
		ip_client_deref(ic);
	} else {
		spinlock_unlock(&ii->ii_lock);
	}
}

/*
 * icmp_recv_netbuf_default()
 *	Handle an ICMP message that we don't handle automatically.
 */
void icmp_recv_netbuf_default(struct ip_instance *ii, struct netbuf *nb)
{
	struct icmp_client *ic;
	struct icmp_header *ich = (struct icmp_header *)nb->nb_transport;
	
	spinlock_lock(&ii->ii_lock);

	ic = ii->ii_icmp_client_list;
	while (ic && (ic->ic_type != ich->ich_type)) {
		ic = ic->ic_next;
	}

	if (ic) {
		icmp_client_ref(ic);
		spinlock_unlock(&ii->ii_lock);
		ic->ic_recv(ic, nb);
		icmp_client_deref(ic);
	} else {
		spinlock_unlock(&ii->ii_lock);
	}
}

/*
 * icmp_recv_netbuf()
 */
void icmp_recv_netbuf(struct ip_instance *ii, struct netbuf *nb)
{
	struct icmp_header *ich;
	
	ich = (struct icmp_header *)nb->nb_transport;

	/*
	 * If we have a checksum then check it.
	 */
	if (ich->ich_csum) {
		if (ipcsum(0, nb->nb_transport, nb->nb_transport_size) != 0x0000) {
			debug_print_pstr("\n\ricmp rx cs fail");
			return;
		}
	}
	
	/*
	 * OK - well ICMP doesn't strictly have an application layer, but we
	 * split out all of the data after the 8 bytes of the ICMP header to
	 * make life easier later on!
	 */
	nb->nb_application = ((u8_t *)ich) + 8;
	nb->nb_application_size = nb->nb_transport_size - 8;
	nb->nb_transport_size = 8;
	
	switch (ich->ich_type) {
	case 0x03:
	case 0x04:
	case 0x0b:
	case 0x0c:
		/*
		 * In accordance with RFC1122 we notify our transport layer
		 * when we get: "destination unreachable", "source quench",
		 * "time exceeded" and "parameter problem".
		 */
		icmp_recv_netbuf_notify_transport(ii, nb);
		break;
	
	case 0x08:
		icmp_recv_netbuf_echo_request(ii, nb);
		break;
	
	default:
		icmp_recv_netbuf_default(ii, nb);
		break;
	}
}

/*
 * icmp_issue_netbuf()
 *	Send an ICMP netbuf.
 *
 * Note that we assume that the destination address provided here is in
 * network byte order.
 */
void icmp_issue_netbuf(struct ip_instance *ii, u32_t dest_addr, u8_t type, u8_t code, u8_t *extra, struct netbuf *nb)
{
	struct icmp_header *ich;
	u16_t csum;

	nb->nb_transport_membuf = membuf_alloc(8, NULL);
	nb->nb_transport = nb->nb_transport_membuf;
	nb->nb_transport_size = 8;

	ich = (struct icmp_header *)nb->nb_transport;
	ich->ich_type = type;
	ich->ich_code = code;
	ich->ich_csum = 0x0000;
	memcpy(ich + 1, extra, 4);

	csum = ipcsum_partial(0, nb->nb_transport, nb->nb_transport_size);
	ich->ich_csum = ipcsum(csum, nb->nb_application, nb->nb_application_size);
	
	ip_issue_netbuf(ii, dest_addr, 0x01, nb);
}

/*
 * icmp_send_netbuf()
 *	Send an ICMP netbuf.
 *
 * Note that we assume that the destination address provided here is in
 * network byte order.
 */
void icmp_send_netbuf(void *srv, u32_t dest_addr, u8_t type, u8_t code, u8_t *extra, struct netbuf *nb)
{
	struct ip_server *is;
	struct ip_instance *ii;

	is = (struct ip_server *)srv;
	ii = (struct ip_instance *)is->is_instance;

	icmp_issue_netbuf(ii, dest_addr, type, code, extra, nb);
}

/*
 * icmp_server_alloc()
 *	Allocate an ICMP server structure.
 */
struct icmp_server *icmp_server_alloc(void)
{
	struct icmp_server *is;
	
	is = (struct icmp_server *)membuf_alloc(sizeof(struct icmp_server), NULL);
	is->is_send = icmp_send_netbuf;

	return is;
}

/*
 * icmp_client_alloc()
 *	Allocate an ICMP client structure.
 */
struct icmp_client *icmp_client_alloc(void)
{
	struct icmp_client *ic;
	
	ic = (struct icmp_client *)membuf_alloc(sizeof(struct icmp_client), NULL);

	return ic;
}

/*
 * icmp_client_attach()
 *	Attach an ICMP client.
 */
struct icmp_server *icmp_client_attach(struct ip_instance *ii, struct icmp_client *ic)
{
	struct icmp_server *is = NULL;
        struct icmp_client *p;

	/*
	 * Before we do anything, check if we allow our client to hook the
	 * IMCP message type that is being requested.  In accordance with
	 * RFC1122 we allow "echo", "timestamp" and "address mask".
	 */
	if ((ic->ic_type != 0x00) && (ic->ic_type != 0x0d) && (ic->ic_type != 0x11)) {
		return NULL;
	}
        	
	spinlock_lock(&ii->ii_lock);

	/*
	 * Walk the list of clients and check that we can do this request.
	 */
	p = ii->ii_icmp_client_list;
	while (p && (p->ic_type != ic->ic_type)) {
		p = p->ic_next;
	}

	if (!p) {
		is = icmp_server_alloc();
		is->is_instance = ii;
                ip_instance_ref(ii);

		ic->ic_server = is;
		ic->ic_next = ii->ii_icmp_client_list;
		ii->ii_icmp_client_list = ic;
		icmp_client_ref(ic);
	}
				
	spinlock_unlock(&ii->ii_lock);

	return is;
}

/*
 * icmp_client_detach()
 *	Detach an ICMP client.
 */
void icmp_client_detach(struct ip_instance *ii, struct icmp_server *is)
{
        struct icmp_client *ic;
        struct icmp_client **icprev;
	
	spinlock_lock(&ii->ii_lock);

	/*
	 * Walk the list of clients and find the one we're to remove.
	 */
	ic = ii->ii_icmp_client_list;
	icprev = &ii->ii_icmp_client_list;
	while (ic) {
		if (ic->ic_server == is) {
			*icprev = ic->ic_next;
			icmp_server_deref(is);
			ip_instance_deref(ii);
			icmp_client_deref(ic);
			break;
		}
	
		icprev = &ic->ic_next;
		ic = ic->ic_next;
	}
			
	spinlock_unlock(&ii->ii_lock);
}

/*
 * ip_recv_netbuf()
 */
void ip_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ip_datalink_client *idc;
	struct ip_instance *ii;
	struct ip_header *iph;
	struct ip_client *ic;
	u32_t empty = 0;
	
	idc = (struct ip_datalink_client *)clnt;
	ii = (struct ip_instance *)idc->idc_instance;
		
	iph = nb->nb_network;
		
	/*
	 * We're only dealing with v4 IP here.
	 */
	if (iph->ih_version != 4) {
		return;
	}

	/*
	 * If we have a header checksum then check it.
	 */
	if (iph->ih_header_csum) {
		if (ipcsum(0, iph, iph->ih_header_len * 4) != 0x0000) {
			return;
		}
	}

	/*
	 * Check that we have a match on the IP address.
	 */
	if (hton32(iph->ih_dest_addr) != ii->ii_addr) {
		return;
	}

	/*
	 * Now that we know things are sane then we should refine our
	 * netbuf structure.
	 */
	nb->nb_network_size = 4 * iph->ih_header_len;
	nb->nb_transport = ((u8_t *)iph) + (4 * iph->ih_header_len);
	nb->nb_transport_size = hton16(iph->ih_total_len) - (4 * iph->ih_header_len);
		
	/*
	 * Determine the IP protocol and handle as appropriate.
	 */
	if (iph->ih_protocol == 0x01) {
		icmp_recv_netbuf(ii, nb);
		return;
	}
	
	spinlock_lock(&ii->ii_lock);

	ic = ii->ii_client_list;
	while (ic && (ic->ic_protocol != iph->ih_protocol)) {
		ic = ic->ic_next;
	}

	if (ic) {
		ip_client_ref(ic);
		spinlock_unlock(&ii->ii_lock);
		ic->ic_recv(ic, nb);
		ip_client_deref(ic);
	} else {
		struct ip_header *iph, *iphc;
		struct netbuf *nbrep;
			
		spinlock_unlock(&ii->ii_lock);
		
		iph = (struct ip_header *)nb->nb_network;
                              		
		/*
		 * Issue an ICMP destination unreachable message (protocol unreachable).
		 */	
		nbrep = netbuf_alloc();
		nbrep->nb_application_membuf = membuf_alloc(nb->nb_network_size + 8, NULL);
		nbrep->nb_application = nbrep->nb_application_membuf;
	        nbrep->nb_application_size = nb->nb_network_size + 8;
	
		iphc = (struct ip_header *)nbrep->nb_application;
		memcpy(iphc, iph, nb->nb_network_size);
		memcpy((iphc + 1), nb->nb_transport, 8);
	
		icmp_issue_netbuf(ii, iph->ih_src_addr, 0x03, 0x02, (u8_t *)(&empty), nbrep);

		netbuf_deref(nbrep);
	}
}

/*
 * ip_issue_netbuf()
 *	Send an IP netbuf.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 */
void ip_issue_netbuf(struct ip_instance *ii, u32_t dest_addr, u8_t protocol, struct netbuf *nb)
{
	struct ip_header *iph;
	struct ip_datalink_server *ids;

	nb->nb_network_membuf = membuf_alloc(sizeof(struct ip_header), NULL);
	nb->nb_network = nb->nb_network_membuf;
	nb->nb_network_size = sizeof(struct ip_header);
		
	iph = (struct ip_header *)nb->nb_network;
	iph->ih_version = 4;
	iph->ih_header_len = sizeof(struct ip_header) / 4;
	iph->ih_src_addr = hton32(ii->ii_addr);
	iph->ih_dest_addr = dest_addr;
	iph->ih_protocol = protocol;
	iph->ih_time_to_live = 0x40;
	iph->ih_type_of_service = 0x00;
	iph->ih_ident = hton16(ii->ii_pkt_ident++);
	iph->ih_flags = 0;
	iph->ih_fragment_offs = 0;
	iph->ih_total_len = hton16(nb->nb_network_size + nb->nb_transport_size + nb->nb_application_size);
	iph->ih_header_csum = 0x0000;

	spinlock_lock(&ii->ii_lock);
	ids = ii->ii_server;
	ip_datalink_server_ref(ids);
	spinlock_unlock(&ii->ii_lock);
		
	iph->ih_header_csum = ipcsum(0, iph, iph->ih_header_len * 4);
	
	ids->ids_send(ids, nb);

	ip_datalink_server_deref(ids);
}

/*
 * ip_send_netbuf()
 *	Send an IP netbuf.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 */
void ip_send_netbuf(void *srv, u32_t dest_addr, u8_t protocol, struct netbuf *nb)
{
	struct ip_server *is;
	struct ip_instance *ii;

	is = (struct ip_server *)srv;
	ii = (struct ip_instance *)is->is_instance;
        		
	ip_issue_netbuf(ii, dest_addr, protocol, nb);
}

/*
 * ip_server_alloc()
 *	Allocate an IP server structure.
 */
struct ip_server *ip_server_alloc(void)
{
	struct ip_server *is;
	
	is = (struct ip_server *)membuf_alloc(sizeof(struct ip_server), NULL);
	is->is_send = ip_send_netbuf;
	is->is_send_icmp = icmp_send_netbuf;
		                	
	return is;
}

/*
 * ip_client_alloc()
 *	Allocate an IP client structure.
 */
struct ip_client *ip_client_alloc(void)
{
	struct ip_client *ic;
	
	ic = (struct ip_client *)membuf_alloc(sizeof(struct ip_client), NULL);
	ic->ic_instance = NULL;
	
	return ic;
}

/*
 * ip_client_attach()
 *	Attach an IP protocol handler.
 */
struct ip_server *ip_client_attach(struct ip_instance *ii, struct ip_client *ic)
{
        struct ip_client *p;
        struct ip_server *is = NULL;
        	
	spinlock_lock(&ii->ii_lock);

	/*
	 * Walk the client list and check that we can do this request.
	 */
	p = ii->ii_client_list;
	while (p && (p->ic_protocol != ic->ic_protocol)) {
		p = p->ic_next;
	}

	if (!p) {
		is = ip_server_alloc();
		is->is_instance = ii;
                ip_instance_ref(ii);

		ic->ic_server = is;
		ic->ic_next = ii->ii_client_list;
		ii->ii_client_list = ic;
		ip_client_ref(ic);
	}
				
	spinlock_unlock(&ii->ii_lock);

	return is;
}

/*
 * ip_client_detach()
 *	Detach an IP protocol handler.
 */
void ip_client_detach(struct ip_instance *ii, struct ip_server *is)
{
        struct ip_client *ic;
        struct ip_client **icprev;
	
	spinlock_lock(&ii->ii_lock);

	/*
	 * Walk the client list and find the one we're to remove.
	 */
	ic = ii->ii_client_list;
	icprev = &ii->ii_client_list;
	while (ic) {
		if (ic->ic_server == is) {
			*icprev = ic->ic_next;
			ip_server_deref(is);
			ip_instance_deref(ii);
			ip_client_deref(ic);
			break;
		}
	
		icprev = &ic->ic_next;
		ic = ic->ic_next;
	}
			
	spinlock_unlock(&ii->ii_lock);
}

/*
 * ip_instance_alloc()
 */
struct ip_instance *ip_instance_alloc(struct ip_datalink_instance *idi, u32_t addr)
{
	struct ip_instance *ii;
        struct ip_datalink_client *idc;
        		
	ii = (struct ip_instance *)membuf_alloc(sizeof(struct ip_instance), NULL);
	spinlock_init(&ii->ii_lock, 0x18);
	ii->ii_addr = addr;
	ii->ii_pkt_ident = 0;
	ii->ii_server = NULL;
	ii->ii_client_list = NULL;
	ii->ii_icmp_client_list = NULL;
	ii->ii_ip_client_attach = ip_client_attach;
	ii->ii_ip_client_detach = ip_client_detach;
	ii->ii_icmp_client_attach = icmp_client_attach;
	ii->ii_icmp_client_detach = icmp_client_detach;
	
	/*
	 * Attach this IP handler to our datalink handler.
	 */
	idc = ip_datalink_client_alloc();
	idc->idc_addr = addr;
	idc->idc_recv = ip_recv_netbuf;
	idc->idc_instance = ii;
	ip_instance_ref(ii);
	ii->ii_server = idi->idi_client_attach(idi, idc);
	ip_datalink_client_deref(idc);

	return ii;
}
