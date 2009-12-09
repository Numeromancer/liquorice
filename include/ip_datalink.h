/*
 * ip_datalink.h
 *	IP datalink object support.
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
 * Structure used to bind the datalink layer to an IP protocol handler.
 */
struct ip_datalink_server {
	struct ip_datalink_instance *ids_instance;
					/* The instance of this service */
	void (*ids_send)(void *srv, struct netbuf *nb);
					/* Callback to the server when a packet is to be sent */
};

/*
 * Structure used to bind the IP protocol to the datalink layer.
 */
struct ip_datalink_client {
	u32_t idc_addr;			/* IP address of our client */
	struct ip_datalink_server *idc_server;
					/* Reference to the server structure given to our client */
	void *idc_instance;		/* The instance of our client */
	void (*idc_recv)(void *clnt, struct netbuf *nb);
					/* The callback function for when we have received packet data */
};

/*
 * Instance of an IP datalink service.
 */
struct ip_datalink_instance {
	struct lock idi_lock;
	u16_t idi_mru;			/* Maximum receive unit size */
	u16_t idi_mtu;			/* Maximum transmit unit size */
	struct ip_datalink_client *idi_client;
	struct ip_datalink_server *(*idi_client_attach)(struct ip_datalink_instance *idi, struct ip_datalink_client *idc);
	void (*idi_client_detach)(struct ip_datalink_instance *idi, struct ip_datalink_server *ids);
};

/*
 * Function prototypes.
 */
extern struct ip_datalink_client *ip_datalink_client_alloc(void);
extern void ip_datalink_client_detach(struct ip_datalink_instance *idi, struct ip_datalink_server *ids);

/*
 * ip_datalink_client_ref()
 */
extern inline void ip_datalink_client_ref(struct ip_datalink_client *idc)
{
	membuf_ref(idc);
}

/*
 * ip_datalink_client_deref()
 */
extern inline ref_t ip_datalink_client_deref(struct ip_datalink_client *idc)
{
	return membuf_deref(idc);
}

/*
 * ip_datalink_server_ref()
 */
extern inline void ip_datalink_server_ref(struct ip_datalink_server *ids)
{
	membuf_ref(ids);
}

/*
 * ip_datalink_server_deref()
 */
extern inline ref_t ip_datalink_server_deref(struct ip_datalink_server *ids)
{
	return membuf_deref(ids);
}

/*
 * ip_datalink_instance_ref()
 */
extern inline void ip_datalink_instance_ref(struct ip_datalink_instance *idi)
{
	membuf_ref(idi);
}

/*
 * ip_datalink_instance_deref()
 */
extern inline ref_t ip_datalink_instance_deref(struct ip_datalink_instance *idi)
{
	return membuf_deref(idi);
}
