/*
 * ethernet_ip.c
 *	IP over Ethernet support
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
#include "membuf.h"
#include "netbuf.h"
#include "ethdev.h"
#include "ethernet.h"
#include "ip_datalink.h"
#include "ethernet_ip.h"
#include "ip.h"

/*
 * __ethernet_ip_instance_free()
 */
static void __ethernet_ip_instance_free(void *inst)
{
	struct ethernet_ip_instance *eii;
	struct ip_route *r;
	
	eii = (struct ethernet_ip_instance *)inst;
	
	/*
	 * Clean up the route table - don't worry about locking as this is a
	 * dead reference!
	 */
	r = eii->eii_route_info;
	while (r) {
		struct ip_route *p;
		
		p = r;
		r = r->ir_next;
	
		heap_free(p);
	}
}

/*
 * ip_arp_cache_add()
 *
 * I know this is wrong, but it's only for a short time (the age bit's screwed up).
 */
static void ip_arp_cache_add(struct ip_datalink_instance *idi, u8_t *mac, u32_t ip)
{
	u8_t i;
	u8_t oldest = 0;
        u32_t old_age = 0xffffffff;
	struct ethernet_ip_instance *eii;
	
	eii = (struct ethernet_ip_instance *)idi;
        		
	spinlock_lock(&idi->idi_lock);
		
	for (i = 0; i < ARP_CACHE_SLOTS; i++) {
		if (eii->eii_arp_cache[i].iacs_ip == ip) {
			oldest = i;
			break;
		} else if (eii->eii_arp_cache[i].iacs_age < old_age) {
			oldest = i;
			old_age = eii->eii_arp_cache[i].iacs_age;
		}
	}

	memcpy(eii->eii_arp_cache[oldest].iacs_mac, mac, 6);
	eii->eii_arp_cache[oldest].iacs_ip = ip;
	eii->eii_arp_cache[oldest].iacs_age = eii->eii_ip_arp_age++;

	spinlock_unlock(&idi->idi_lock);
}

/*
 * ip_arp_cache_find()
 *
 * Returns 0 if no match is found, or 1 if one is found.  If one is found
 * we also write the MAC address into the area pointed to by mac.
 */
static u8_t ip_arp_cache_find(struct ip_datalink_instance *idi, u8_t *mac, u32_t ip)
{
	u8_t i;
        u8_t retval = 0;
	struct ethernet_ip_instance *eii;
	
	eii = (struct ethernet_ip_instance *)idi;
        		
	spinlock_lock(&idi->idi_lock);
		
	/*
	 * First look in our cache slots.
	 */
	for (i = 0; i < ARP_CACHE_SLOTS; i++) {
		if (eii->eii_arp_cache[i].iacs_ip == ip) {
			/*
			 * If we get a match then flag that this entry has
			 * been newly refreshed.
			 */
			eii->eii_arp_cache[i].iacs_age = eii->eii_ip_arp_age++;
			memcpy(mac, eii->eii_arp_cache[i].iacs_mac, 6);
			retval = 1;
			break;
		}
	}

	spinlock_unlock(&idi->idi_lock);
	
	return retval;
}

/*
 * ip_arp_recv_request()
 */
static void ip_arp_recv_request(struct ip_datalink_instance *idi, struct netbuf *nb)
{
        u8_t *send_packet;
	struct eth_phys_header *reph;
	struct arp_header *rah, *sah;
        struct ip_arp_header *rp, *sp;
        struct netbuf *nbrep;
	struct ethernet_ip_instance *eii;
	struct ethernet_server *es;
	
	eii = (struct ethernet_ip_instance *)idi;
        	
	reph = (struct eth_phys_header *)nb->nb_datalink;
	rah = (struct arp_header *)nb->nb_network;
	rp = (struct ip_arp_header *)(rah + 1);
	
	/*
	 * Issue an ARP reply.
	 */	
	send_packet = membuf_alloc(sizeof(struct arp_header) + sizeof(struct ip_arp_header), NULL);
                				
       	sah = (struct arp_header *)send_packet;	
	sp = (struct ip_arp_header *)(sah + 1);
       	
	memcpy(sp->iah_dest_mac, rp->iah_src_mac, 6);
	
	spinlock_lock(&idi->idi_lock);
	es = eii->eii_arp_server;
	ethernet_server_ref(es);
	spinlock_unlock(&idi->idi_lock);
	
	memcpy(sp->iah_src_mac, es->es_get_mac(es), 6);

        sah->ah_hardware = rah->ah_hardware;
        sah->ah_protocol = rah->ah_protocol;
        sah->ah_haddr_len = rah->ah_haddr_len;
        sah->ah_paddr_len = rah->ah_paddr_len;
        sah->ah_operation = hton16(2);
       	
	sp->iah_src_ip = hton32(idi->idi_client->idc_addr);
	sp->iah_dest_ip = hton32(rp->iah_src_ip);

	nbrep = netbuf_alloc();
	nbrep->nb_network_membuf = sah;
	nbrep->nb_network = sah;
	nbrep->nb_network_size = sizeof(struct arp_header) + sizeof(struct ip_arp_header);
			
	es->es_send(es, sp->iah_dest_mac, nbrep);
	
	netbuf_deref(nbrep);
	ethernet_server_deref(es);
}

/*
 * ip_arp_recv_netbuf()
 *	Handle reception of an ARP packet for IP.
 */
void ip_arp_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ethernet_client *ec;
	struct ip_datalink_instance *idi;
	struct eth_phys_header *eph;
	struct arp_header *ah;
        struct ip_arp_header *iah;
	struct ethernet_ip_instance *eii;

	ec = (struct ethernet_client *)clnt;
	idi = (struct ip_datalink_instance *)ec->ec_instance;
	eii = (struct ethernet_ip_instance *)idi;
                	
	eph = (struct eth_phys_header *)nb->nb_datalink;
	ah = (struct arp_header *)nb->nb_network;
	iah = (struct ip_arp_header *)(ah + 1);
	
	if (ah->ah_haddr_len != 6) {
		debug_print_pstr("\farp haddr_len !6");
		return;
	}
	
	if (ah->ah_paddr_len != 4) {
		debug_print_pstr("\farp paddr_len !4");
		return;
	}
	
	if (hton32(iah->iah_dest_ip) != idi->idi_client->idc_addr) {
		return;
	}
	
	/*
	 * Update our ARP cache with the sender's details.
	 */
	ip_arp_cache_add(idi, eph->eph_src_mac, hton32(iah->iah_src_ip));
	
	switch (hton16(ah->ah_operation)) {
	case 1:
		/*
		 * ARP request.
		 */
	        ip_arp_recv_request(idi, nb);
                break;
	        	
	case 2:
		/*
		 * ARP response.
		 */
		break;
	
	default:
		debug_print_pstr("\fip_arp_recv_netbuf: op");
		debug_print16(hton16(ah->ah_operation));
	}
}

/*
 * ip_arp_request()
 *	Issue an ARP request for a given IP address.
 */
u8_t ip_arp_request(struct ip_datalink_instance *idi, u32_t ip)
{
        u8_t *pkt;
	struct netbuf *nb;
        struct arp_header *ah;
        struct ip_arp_header *iah;
        static u8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct ethernet_ip_instance *eii;
	struct ethernet_server *es;
	
	eii = (struct ethernet_ip_instance *)idi;
                        	
	pkt = membuf_alloc(sizeof(struct arp_header) + sizeof(struct ip_arp_header), NULL);
	
	ah = (struct arp_header *)pkt;
	ah->ah_hardware = hton16(1);
	ah->ah_protocol = hton16(0x0800);
	ah->ah_haddr_len = 6;
	ah->ah_paddr_len = 4;
        ah->ah_operation = hton16(1);
		
	iah = (struct ip_arp_header *)(ah + 1);
	iah->iah_src_ip = hton32(idi->idi_client->idc_addr);
	iah->iah_dest_ip = hton32(ip);
	
	spinlock_lock(&idi->idi_lock);
	es = eii->eii_arp_server;
	ethernet_server_ref(es);
	spinlock_unlock(&idi->idi_lock);
	
	memcpy(iah->iah_src_mac, es->es_get_mac(es), 6);
	memcpy(iah->iah_dest_mac, broadcast_mac, 6);
	
	nb = netbuf_alloc();
	nb->nb_network_membuf = ah;
	nb->nb_network = ah;
	nb->nb_network_size = sizeof(struct arp_header) + sizeof(struct ip_arp_header);

	es->es_send(es, broadcast_mac, nb);
	
	netbuf_deref(nb);
	ethernet_server_deref(es);
	
	return 0;
}

/*
 * ip_route_add()
 *	Add a route into the routing list.
 *
 * We jump through quite a few hoops here to ensure that the routes are added
 * in such a way that we can scan from the head of the list and use the first
 * path that we find.
 */
void ip_route_add(struct ip_datalink_instance *idi, struct ip_route *radd)
{
	struct ip_route *r, **rprev;
	struct ethernet_ip_instance *eii;
	
	eii = (struct ethernet_ip_instance *)idi;
	
	spinlock_lock(&idi->idi_lock);
		
	r = eii->eii_route_info;
	rprev = &eii->eii_route_info;

	/*
	 * Are we adding a host or a network?  If we're adding a host then we can
	 * drop through and place it at the head of the list.
	 */
	if (radd->ir_network) {
		/*
		 * We're adding a network - skip past the host entries.
		 */
		while (r && (r->ir_network == FALSE)) {
			rprev = &r->ir_next;
			r = r->ir_next;
		}

		/*
		 * Add the network reference, placing the ones with the fewest
		 * possible hosts at the head of the list.  This means a simple
		 * search will check smaller specific routes before more general
		 * ones.
		 */	
		while (r && (r->ir_addr.ir_network.irn_netmask > radd->ir_addr.ir_network.irn_netmask)) {
			rprev = &r->ir_next;
			r = r->ir_next;
		}
	}

	/*
	 * Insert the entry into the list.
	 */
	radd->ir_next = r;
	*rprev = radd;

	spinlock_unlock(&idi->idi_lock);
}

/*
 * ip_route_find()
 *	Find the device associated with a particular IP route.
 */
struct ip_route *ip_route_find(struct ip_datalink_instance *idi, u32_t ip)
{
	struct ip_route *ir;
	struct ethernet_ip_instance *eii;
	
	eii = (struct ethernet_ip_instance *)idi;
	
	spinlock_lock(&idi->idi_lock);
		
	ir = eii->eii_route_info;
	while (ir) {
		if (ir->ir_network) {
			if ((ip & ir->ir_addr.ir_network.irn_netmask) == ir->ir_addr.ir_network.irn_addr) {
				break;
			}
		} else {
			if (ip == ir->ir_addr.ir_host.irh_addr) {
				break;
			}
		}
		
		ir = ir->ir_next;
	}

	spinlock_unlock(&idi->idi_lock);

	return ir;
}

/*
 * ethernet_ip_recv_netbuf()
 */
void ethernet_ip_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ethernet_client *ec;
	struct ip_datalink_instance *idi;
	struct ip_datalink_client *idc;

	ec = (struct ethernet_client *)clnt;
	idi = (struct ip_datalink_instance *)ec->ec_instance;
	
	spinlock_lock(&idi->idi_lock);
	idc = idi->idi_client;
	ip_datalink_client_ref(idc);
	spinlock_unlock(&idi->idi_lock);

	idc->idc_recv(idc, nb);
	
	ip_datalink_client_deref(idc);
}

/*
 * ethernet_ip_send_netbuf()
 *	Send a netbuf.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 */
void ethernet_ip_send_netbuf(void *srv, struct netbuf *nb)
{
	struct ip_datalink_server *ids;
	struct ip_datalink_instance *idi;
	struct ethernet_ip_instance *eii;
	struct ethernet_server *es;
	struct ip_header *iph;
        struct ip_route *rt;
	u32_t addr;
	u8_t mac[6];

	ids = (struct ip_datalink_server *)srv;
	idi = (struct ip_datalink_instance *)ids->ids_instance;
	eii = (struct ethernet_ip_instance *)idi;
	iph = (struct ip_header *)nb->nb_network;

	rt = ip_route_find(idi, hton32(iph->ih_dest_addr));
	if (!rt) {
		debug_print_pstr("\fethernet_ip_send_netbuf: no rt:");
		debug_print32(hton32(iph->ih_dest_addr));
		return;
	}
	
	if (rt->ir_gateway) {
		addr = rt->ir_gateway;
	} else {
		addr = hton32(iph->ih_dest_addr);
	}

	if (ip_arp_cache_find(idi, mac, addr)) {
		spinlock_lock(&idi->idi_lock);
		es = eii->eii_ip_server;
		ethernet_server_ref(es);
		spinlock_unlock(&idi->idi_lock);
	
		es->es_send(es, mac, nb);
	
		ethernet_server_deref(es);
	} else {
		/*
		 * If we don't know where this packet should go to then
		 * it's time to find out.  For now we simply discard the
		 * packet we were supposed to be sending.  Sometime in
		 * the future, perhaps this should be changed for queueing
		 * the request until the address is known.
		 */
		ip_arp_request(idi, addr);
		debug_print_pstr("\fMAC not found:");
		debug_print32(addr);
		debug_print_pstr("rt:");
		debug_print32(rt->ir_addr.ir_network.irn_addr);
	}
}

/*
 * ethernet_ip_server_alloc()
 *	Allocate an IP/Ethernet server structure.
 */
struct ip_datalink_server *ethernet_ip_server_alloc(void)
{
	struct ip_datalink_server *ids;
	
	ids = (struct ip_datalink_server *)membuf_alloc(sizeof(struct ip_datalink_server), NULL);
        ids->ids_send = ethernet_ip_send_netbuf;
        	
	return ids;
}

/*
 * ethernet_ip_client_attach()
 *	Attach a client to the IP/Ethernet layer.
 */
struct ip_datalink_server *ethernet_ip_client_attach(struct ip_datalink_instance *idi, struct ip_datalink_client *idc)
{
	struct ip_datalink_server *ids = NULL;

	spinlock_lock(&idi->idi_lock);

	if (idi->idi_client == NULL) {
		ids = ethernet_ip_server_alloc();
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
 * ethernet_ip_instance_alloc()
 */
struct ip_datalink_instance *ethernet_ip_instance_alloc(struct ethernet_instance *ethi)
{
	struct ip_route *ir;
	struct ip_datalink_instance *idi;
	struct ethernet_ip_instance *eii;
	u8_t i;
        struct ethernet_client *ipec, *arpec;
        		
	idi = (struct ip_datalink_instance *)membuf_alloc(sizeof(struct ethernet_ip_instance), __ethernet_ip_instance_free);
	eii = (struct ethernet_ip_instance *)idi;
	spinlock_init(&idi->idi_lock, 0x10);
	idi->idi_mru = 1500;
	idi->idi_mtu = 1500;
	idi->idi_client = NULL;
	idi->idi_client_attach = ethernet_ip_client_attach;
	idi->idi_client_detach = ip_datalink_client_detach;

	eii->eii_ip_arp_age = 1;
	eii->eii_route_info = NULL;
	eii->eii_ip_server = NULL;
	eii->eii_arp_server = NULL;
	
	/*
	 * Clear down the ARP cache.
	 */
	for (i = 0; i < ARP_CACHE_SLOTS; i++) {
		eii->eii_arp_cache[i].iacs_age = 0;
		eii->eii_arp_cache[i].iacs_ip = 0;
	}

	/*
	 * Attach this IP handler to our Ethernet channel.
	 */
	ipec = ethernet_client_alloc();
	ipec->ec_recv = ethernet_ip_recv_netbuf;
	ipec->ec_type = 0x0800;
	ipec->ec_instance = idi;
	ip_datalink_instance_ref(idi);
	eii->eii_ip_server = ethi->ei_client_attach(ethi, ipec);
	ethernet_client_deref(ipec);
	
	arpec = ethernet_client_alloc();
	arpec->ec_recv = ip_arp_recv_netbuf;
	arpec->ec_type = 0x0806;
	arpec->ec_instance = idi;
	ip_datalink_instance_ref(idi);
	eii->eii_arp_server = ethi->ei_client_attach(ethi, arpec);
	ethernet_client_deref(arpec);
	
	/*
	 * Route to the network.
	 */
	ir = (struct ip_route *)heap_alloc(sizeof(struct ip_route));
	ir->ir_network = TRUE;
	ir->ir_addr.ir_network.irn_addr = 0xc0a80000;
	ir->ir_addr.ir_network.irn_netmask = 0xffffff00;
	ir->ir_gateway = 0x00000000;
	ir->ir_next = NULL;
	ip_route_add(idi, ir);
			
	/*
	 * Default route to anywhere else!
	 */
	ir = (struct ip_route *)heap_alloc(sizeof(struct ip_route));
	ir->ir_network = TRUE;
	ir->ir_addr.ir_network.irn_addr = 0x00000000;
	ir->ir_addr.ir_network.irn_netmask = 0x00000000;
	ir->ir_gateway = 0xc0a80001;
	ir->ir_next = NULL;
	ip_route_add(idi, ir);

	return idi;
}
