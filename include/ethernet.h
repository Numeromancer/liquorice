/*
 * ethernet.h
 *	Ethernet-specific information.
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
 * Ethernet header layout.
 */
struct eth_phys_header {
	u8_t eph_dest_mac[6];
	u8_t eph_src_mac[6];
	u16_t eph_type;
};

/*
 * Structure used to bind the Ethernet driver to a protocol.
 */
struct ethernet_server {
	struct ethernet_instance *es_instance;
					/* The instance of this service */
	u16_t es_type;			/* Ethernet type field associated with the service */
	void (*es_send)(void *srv, u8_t *dest_mac, struct netbuf *nb);
					/* Function used when we send packet data */
	u8_t *(*es_get_mac)(struct ethernet_server *es);
					/* Get the Ethernet interface's MAC address */
};

/*
 * Structure used to bind a protocol to the Ethernet driver.
 */
struct ethernet_client {
	void *ec_instance;		/* The instance of our client */
	struct ethernet_server *ec_server;
					/* Reference to the server structure given to our client */
	u16_t ec_type;			/* Ethernet type field associated with the client */
	struct ethernet_client *ec_next;
					/* Next client in the list */
	void (*ec_recv)(void *clnt, struct netbuf *nb);
					/* The callback function for when we have received packet data */
};

/*
 * Instance of the Ethernet service.
 */
struct ethernet_instance {
	struct lock ei_lock;
	struct ethernet_client *ei_client_list;
	struct ethdev_server *ei_server;
	struct ethernet_server *(*ei_client_attach)(struct ethernet_instance *ei, struct ethernet_client *ec);
	void (*ei_client_detach)(struct ethernet_instance *ei, struct ethernet_server *es);
};

/*
 * Function prototypes.
 */
extern u8_t *ethernet_get_mac(struct ethernet_server *es);
extern struct ethernet_client *ethernet_client_alloc(void);
extern struct ethernet_instance *ethernet_instance_alloc(struct ethdev_instance *edi);

/*
 * ethernet_client_ref()
 */
extern inline void ethernet_client_ref(struct ethernet_client *ec)
{
	membuf_ref(ec);
}

/*
 * ethernet_client_deref()
 */
extern inline ref_t ethernet_client_deref(struct ethernet_client *ec)
{
	return membuf_deref(ec);
}

/*
 * ethernet_server_ref()
 */
extern inline void ethernet_server_ref(struct ethernet_server *es)
{
	membuf_ref(es);
}

/*
 * ethernet_server_deref()
 */
extern inline ref_t ethernet_server_deref(struct ethernet_server *es)
{
	return membuf_deref(es);
}

/*
 * ethernet_instance_ref()
 */
extern inline void ethernet_instance_ref(struct ethernet_instance *ei)
{
	membuf_ref(ei);
}

/*
 * ethernet_instance_deref()
 */
extern inline ref_t ethernet_instance_deref(struct ethernet_instance *ei)
{
	return membuf_deref(ei);
}
