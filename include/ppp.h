/*
 * ppp.h
 *	Point-to-point protocol implementation.
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
 * Structure used to bind the PPP driver to a protocol.
 */
struct ppp_server {
	struct ppp_instance *ps_instance;
					/* The instance of this service */
	u16_t ps_protocol;		/* PPP protocol field associated with the service */
	void (*ps_send)(void *srv, struct netbuf *nb);
					/* Function used when we send packet data */
};

/*
 * Structure used to bind a protocol to the PPP driver.
 */
struct ppp_client {
	void *pc_instance;		/* The instance of our client */
	struct ppp_server *pc_server;
					/* Reference to the server structure given to our client */
	u16_t pc_protocol;		/* PPP protocol field associated with the client */
	struct ppp_client *pc_next;
					/* Next client in the list */
	void (*pc_recv)(void *clnt, struct netbuf *nb);
					/* The callback function for when we have received packet data */
	void (*pc_link_up)(void *clnt);	/* Callback used to notify that the PPP layer is now up */
	void (*pc_link_down)(void *clnt);
					/* Callback used to notify that the PPP layer is now down */
};

/*
 * Instance of the PPP service.
 */
struct ppp_instance {
	struct lock pi_lock;
	u8_t pi_phase;			/* Link operation phase */
	u8_t pi_state;			/* Negotiation state */
	u8_t pi_ident;			/* Command ident number */
	struct oneshot *pi_timeout_timer;
					/* Command timeout one-shot timer */
	u8_t pi_restart_counter;	/* Command retry counter */
	struct netbuf *pi_send;
	u8_t pi_parse_state;
	u16_t pi_mru;			/* MRU */
	u8_t pi_mru_usage;
	u32_t pi_accm;			/* Asynchronous control character map */
	u8_t pi_accm_usage;
	u32_t pi_local_magic_number;	/* Local host's magic number */
	u8_t pi_local_magic_number_usage;
	u32_t pi_remote_magic_number;	/* Remote host's magic number */
	u8_t pi_pfc;			/* Protocol field compression */
	u8_t pi_pfc_usage;
	u8_t pi_acfc;			/* Address and control field compression */
	u8_t pi_acfc_usage;
	struct ppp_client *pi_client_list;
	struct ppp_ahdlc_server *pi_server;
	struct ppp_server *(*pi_client_attach)(struct ppp_instance *pi, struct ppp_client *pc);
	void (*pi_client_detach)(struct ppp_instance *pi, struct ppp_server *ps);
};

/*
 * Usage states.
 */
#define PPP_USAGE_ACK 0
#define PPP_USAGE_NAK 1
#define PPP_USAGE_REJ 2

/*
 * Function prototypes for the PPP.
 */
extern struct ppp_client *ppp_client_alloc(void);
extern struct ppp_instance *ppp_instance_alloc(struct ppp_ahdlc_instance *pai);

/*
 * ppp_client_ref()
 */
extern inline void ppp_client_ref(struct ppp_client *pc)
{
	membuf_ref(pc);
}

/*
 * ppp_client_deref()
 */
extern inline ref_t ppp_client_deref(struct ppp_client *pc)
{
	return membuf_deref(pc);
}

/*
 * ppp_server_ref()
 */
extern inline void ppp_server_ref(struct ppp_server *ps)
{
	membuf_ref(ps);
}

/*
 * ppp_server_deref()
 */
extern inline ref_t ppp_server_deref(struct ppp_server *ps)
{
	return membuf_deref(ps);
}

/*
 * ppp_instance_ref()
 */
extern inline void ppp_instance_ref(struct ppp_instance *pi)
{
	membuf_ref(pi);
}

/*
 * ppp_instance_deref()
 */
extern inline ref_t ppp_instance_deref(struct ppp_instance *pi)
{
	return membuf_deref(pi);
}
