/*
 * udp.h
 *	User Datagram Protocol (UDP/IP) header.
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
 * UDP header layout.
 */
struct udp_header {
	u16_t uh_src_port;
	u16_t uh_dest_port;
	u16_t uh_len;
	u16_t uh_csum;
};

/*
 * UDP server structure.
 */
struct udp_server {
	void *us_instance;		/* The instance of this service */
	u16_t us_port;
	void (*us_send)(void *srv, u32_t dest_addr, u16_t dest_port, struct netbuf *nb);
					/* The callback function for when we send packet data */
};

/*
 * UDP client structure.
 */
struct udp_client {
	void *uc_instance;		/* The instance of our client */
	struct udp_server *uc_server;	/* Reference to the server structure given to our client */
	u16_t uc_port;
	struct udp_client *uc_next;
	void (*uc_recv)(void *clnt, struct netbuf *);
};

/*
 * Instance of the UDP service.
 */
struct udp_instance {
	struct lock ui_lock;
	struct udp_client *ui_client_list;
	struct ip_server *ui_server;
	struct udp_instance *ui_next;
	struct udp_server *(*ui_client_attach)(struct udp_instance *ui, struct udp_client *uc);
	void (*ui_client_detach)(struct udp_instance *ui, struct udp_server *us);
};

/*
 * Function prototypes.
 */
extern int udp_dump_stats(struct udp_instance *ui, struct udp_client *ubuf, int max);
extern struct udp_client *udp_client_alloc(void);
extern struct udp_instance *udp_instance_alloc(struct ip_instance *ii);

/*
 * udp_client_ref()
 */
extern inline void udp_client_ref(struct udp_client *uc)
{
	membuf_ref(uc);
}

/*
 * udp_client_deref()
 */
extern inline ref_t udp_client_deref(struct udp_client *uc)
{
	return membuf_deref(uc);
}

/*
 * udp_server_ref()
 */
extern inline void udp_server_ref(struct udp_server *us)
{
	membuf_ref(us);
}

/*
 * udp_server_deref()
 */
extern inline ref_t udp_server_deref(struct udp_server *us)
{
	return membuf_deref(us);
}

/*
 * udp_instance_ref()
 */
extern inline void udp_instance_ref(struct udp_instance *ui)
{
	membuf_ref(ui);
}

/*
 * udp_instance_deref()
 */
extern inline ref_t udp_instance_deref(struct udp_instance *ui)
{
	return membuf_deref(ui);
}
