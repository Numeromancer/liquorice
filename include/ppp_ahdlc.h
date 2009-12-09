/*
 * ppp_ahdlc.h
 *	Point-to-point protocol async framing definitions.
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
 * Structure used to bind the PPP async service to a the PPP layer.
 */
struct ppp_ahdlc_server {
	struct ppp_ahdlc_instance *pas_instance;
					/* The instance of this service */
	void (*pas_send)(void *srv, struct netbuf *nb);
					/* Function used when we send packet data */
	void (*pas_set_accm)(void *srv, u32_t accm);
					/* Function used to set the ACCM bitmask */
	u32_t (*pas_get_accm)(void *srv);
					/* Function used to get the ACCM bitmask */
};

/*
 * Structure used to bind the PPP layer to the PPP async service.
 */
struct ppp_ahdlc_client {
	void *pac_instance;		/* The instance of our client */
	struct ppp_ahdlc_server *pac_server;
					/* Reference to the server structure given to our client */
	void (*pac_recv)(void *clnt, struct netbuf *nb);
					/* The callback function for when we have received packet data */
	void (*pac_link_up)(void *clnt);
					/* Callback used to notify that the physical layer is now up */
	void (*pac_link_down)(void *clnt);
					/* Callback used to notify that the physical layer is now down */
};

/*
 * Instance of the PPP async framing service.
 */
struct ppp_ahdlc_instance {
	struct lock pai_lock;
	struct ppp_ahdlc_client *pai_client;
	struct uart_server *pai_server;
	u8_t pai_recv_esc;
	u16_t pai_recv_octets;
	u8_t pai_recv_ignore;
	u8_t *pai_recv_packet;
	u16_t pai_recv_crc;
	u32_t pai_accm;
	struct context *pai_send_ctx;
	struct netbuf *pai_send_queue;
	struct ppp_ahdlc_server *(*pai_client_attach)(struct ppp_ahdlc_instance *pai, struct ppp_ahdlc_client *pac);
	void (*pai_client_detach)(struct ppp_ahdlc_instance *pai, struct ppp_ahdlc_server *pas);
};

/*
 * Structure type referenced in parameter list.
 */
struct uart_instance;

/*
 * Function prototypes.
 */
extern struct ppp_ahdlc_client *ppp_ahdlc_client_alloc(void);
extern struct ppp_ahdlc_instance *ppp_ahdlc_instance_alloc(struct uart_instance *ui);

/*
 * ppp_ahdlc_client_ref()
 */
extern inline void ppp_ahdlc_client_ref(struct ppp_ahdlc_client *pac)
{
	membuf_ref(pac);
}

/*
 * ppp_ahdlc_client_deref()
 */
extern inline ref_t ppp_ahdlc_client_deref(struct ppp_ahdlc_client *pac)
{
	return membuf_deref(pac);
}

/*
 * ppp_ahdlc_server_ref()
 */
extern inline void ppp_ahdlc_server_ref(struct ppp_ahdlc_server *pas)
{
	membuf_ref(pas);
}

/*
 * ppp_ahdlc_server_deref()
 */
extern inline ref_t ppp_ahdlc_server_deref(struct ppp_ahdlc_server *pas)
{
	return membuf_deref(pas);
}

/*
 * ppp_ahdlc_instance_ref()
 */
extern inline void ppp_ahdlc_instance_ref(struct ppp_ahdlc_instance *pai)
{
	membuf_ref(pai);
}

/*
 * ppp_ahdlc_instance_deref()
 */
extern inline ref_t ppp_ahdlc_instance_deref(struct ppp_ahdlc_instance *pai)
{
	return membuf_deref(pai);
}
