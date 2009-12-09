/*
 * ip.h
 *	Internet Protocol header.
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
 * IP header layout.
 */
struct ip_header {
	u8_t ih_header_len : 4,
		ih_version : 4;
	u8_t ih_type_of_service;
	u16_t ih_total_len;
	u16_t ih_ident;
	u16_t ih_flags : 3,
		ih_fragment_offs : 13;
	u8_t ih_time_to_live;
	u8_t ih_protocol;
	u16_t ih_header_csum;
	u32_t ih_src_addr;
	u32_t ih_dest_addr;
};

/*
 * ICMP header layout.
 *	well the first few bytes that are common to all ICMP messages anyway.
 */
struct icmp_header {
	u8_t ich_type;
	u8_t ich_code;
	u16_t ich_csum;
};

/*
 * Forward declaration
 */
struct ip_instance;

/*
 * IP server structure.
 */
struct ip_server {
	struct ip_instance *is_instance;
					/* The instance of this service */
	void (*is_send)(void *srv, u32_t dest_addr, u8_t protocol, struct netbuf *nb);
					/* Callback to the server when data is to be sent */
	void (*is_send_icmp)(void *srv, u32_t dest_addr, u8_t type, u8_t code, u8_t *extra, struct netbuf *nb);
};

/*
 * IP client structure.
 */
struct ip_client {
	void *ic_instance;		/* The instance of our client */
	u8_t ic_protocol;		/* Transport protocol implemented by the client */
	struct ip_server *ic_server;	/* Reference to the server structure given to our client */
	struct ip_client *ic_next;	/* Next client in the list */
	void (*ic_recv)(void *clnt, struct netbuf *nb);
					/* Callback to the client when data is received */
	void (*ic_recv_icmp)(void *clnt, struct netbuf *nb);
					/* Callback to the client when ICMP desintation unreachable errors are received */
};

/*
 * ICMP server structure.
 */
struct icmp_server {
	void *is_instance;		/* The instance of this service */
	void (*is_send)(void *srv, u32_t dest_addr, u8_t type, u8_t code, u8_t *extra, struct netbuf *nb);
};

/*
 * ICMP client daisy-chaining structure.
 */
struct icmp_client {
	u8_t ic_type;			/* ICMP type handled by client */
	struct icmp_server *ic_server;	/* Reference to the server structure given to our client */
	struct icmp_client *ic_next;	/* Next client in the list of clients */
	void (*ic_recv)(void *clnt, struct netbuf *);
					/* Function callled when ICMP data is received */
};

/*
 * Number of ARP cache slots allocated per TCP instance.
 */
#define ARP_CACHE_SLOTS 8

/*
 * IP service instance.
 */
struct ip_instance {
	struct lock ii_lock;
	u32_t ii_addr;
	u16_t ii_pkt_ident;
	struct ip_datalink_server *ii_server;
	struct ip_client *ii_client_list;
	struct icmp_client *ii_icmp_client_list;
	struct ip_server *(*ii_ip_client_attach)(struct ip_instance *ii, struct ip_client *ic);
	void (*ii_ip_client_detach)(struct ip_instance *ii, struct ip_server *is);
	struct icmp_server *(*ii_icmp_client_attach)(struct ip_instance *ii, struct icmp_client *ic);
	void (*ii_icmp_client_detach)(struct ip_instance *ii, struct icmp_server *is);
};

/*
 * Advance declarations.
 */
struct ip_datalink_instance;

/*
 * Function prototypes.
 */
extern struct ip_client *ip_client_alloc(void);
extern struct icmp_client *icmp_client_alloc(void);
extern struct ip_instance *ip_instance_alloc(struct ip_datalink_instance *idi, u32_t addr);

/*
 * ip_client_ref()
 */
extern inline void ip_client_ref(struct ip_client *ic)
{
	membuf_ref(ic);
}

/*
 * ip_client_deref()
 */
extern inline ref_t ip_client_deref(struct ip_client *ic)
{
	return membuf_deref(ic);
}

/*
 * ip_server_ref()
 */
extern inline void ip_server_ref(struct ip_server *is)
{
	membuf_ref(is);
}

/*
 * ip_server_deref()
 */
extern inline ref_t ip_server_deref(struct ip_server *is)
{
	return membuf_deref(is);
}

/*
 * ip_instance_ref()
 */
extern inline void ip_instance_ref(struct ip_instance *ii)
{
	membuf_ref(ii);
}

/*
 * ip_instance_deref()
 */
extern inline ref_t ip_instance_deref(struct ip_instance *ii)
{
	return membuf_deref(ii);
}

/*
 * icmp_client_ref()
 */
extern inline void icmp_client_ref(struct icmp_client *ic)
{
	membuf_ref(ic);
}

/*
 * icmp_client_deref()
 */
extern inline ref_t icmp_client_deref(struct icmp_client *ic)
{
	return membuf_deref(ic);
}

/*
 * icmp_server_ref()
 */
extern inline void icmp_server_ref(struct icmp_server *is)
{
	membuf_ref(is);
}

/*
 * icmp_server_deref()
 */
extern inline ref_t icmp_server_deref(struct icmp_server *is)
{
	return membuf_deref(is);
}
