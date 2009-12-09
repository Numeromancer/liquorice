/*
 * ethernet_ip.h
 *	IP over Ethernet support.
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

/*
 * ARP header layout.
 */
struct arp_header {
	u16_t ah_hardware;
	u16_t ah_protocol;
	u8_t ah_haddr_len;
	u8_t ah_paddr_len;
	u16_t ah_operation;
};

/*
 * ARP details for IPv4 (after the arp_header).
 */
struct ip_arp_header {
	u8_t iah_src_mac[6];
	u32_t iah_src_ip;
	u8_t iah_dest_mac[6];
	u32_t iah_dest_ip;
} __attribute__ ((packed));

/*
 * ip_arp_cache_slot
 */
struct ip_arp_cache_slot {
	u32_t iacs_ip;
	u8_t iacs_mac[6];
	u32_t iacs_age;
};

/*
 * Number of ARP cache slots allocated per IP instance.
 */
#define ARP_CACHE_SLOTS 8

/*
 * Route information specific to network routes.
 */
struct ip_route_network {
	u32_t irn_addr;			/* Network number */
	u32_t irn_netmask;		/* Netmask for the network */
};

/*
 * Route information specific to host routes.
 */
struct ip_route_host {
	u32_t irh_addr;
};

/*
 * Route details.
 */
struct ip_route {
	u8_t ir_network;		/* Is this a route to a network or a host? */
	union {
		struct ip_route_network ir_network;
		struct ip_route_host ir_host;
	} ir_addr;
	u32_t ir_gateway;		/* Address of gateway to access this route */
	struct ip_route *ir_next;
};

/*
 * IP to Ethernet instance.
 *
 * This is subclassed from the "ip_datalink_instance" class.  Note that the
 * ei_idi field must be the first in the structure in order for the code to
 * work.
 */
struct ethernet_ip_instance {
	struct ip_datalink_instance eii_idi;
	struct ip_arp_cache_slot eii_arp_cache[ARP_CACHE_SLOTS];
	u32_t eii_ip_arp_age;
	struct ip_route *eii_route_info;
	u16_t eii_pkt_ident;
	struct ethernet_server *eii_ip_server;
	struct ethernet_server *eii_arp_server;
};

struct ethernet_instance;

/*
 * Function prototypes.
 */
extern struct ip_datalink_instance *ethernet_ip_instance_alloc(struct ethernet_instance *ei);
