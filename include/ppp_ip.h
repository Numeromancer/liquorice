/*
 * ppp_ip.h
 *	IP over PPP support.
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
 * IP to PPP instance.
 *
 * This is subclassed from the "ip_datalink_instance" class.  Note that the
 * ei_idi field must be the first in the structure in order for the code to
 * work.
 */
struct ppp_ip_instance {
	struct ip_datalink_instance pii_idi;
	struct ppp_server *pii_ip_server;
	struct ppp_server *pii_ipcp_server;
	u8_t pii_phase;			/* Link operation phase */
	u8_t pii_state;			/* Negotiation state */
	u8_t pii_ident;			/* Command ident number */
	struct oneshot *pii_timeout_timer;
					/* Command timeout one-shot timer */
	u8_t pii_restart_counter;	/* Command retry counter */
	struct netbuf *pii_send;
	u8_t pii_parse_state;
	u32_t pii_remote_ip_addr;	/* Remote IP address */
	u32_t pii_local_ip_addr;	/* Local IP address */
	u8_t pii_local_ip_addr_usage;
};

struct ppp_instance;

/*
 * Function prototypes.
 */
extern struct ip_datalink_instance *ppp_ip_instance_alloc(struct ppp_instance *pi);
