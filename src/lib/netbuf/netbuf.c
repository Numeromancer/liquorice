/*
 * netbuf.c
 *	Network buffer support.
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
 *
 * The network buffer handling is designed to make life easy (ish) when it
 * comes to handling ISO-layered communications.  By this we're thinking about
 * communications structures that are hierarchically organized.
 *
 * The idea is that "netbuf" structures are allocated every time a new "packet"
 * is either received or readied for sending.  The various fields of the
 * netbuf are then handled by matching layers within the appropriate protocol
 * stack.  The names of each layer may be different for different protocols,
 * but we're using the ones from ISO here.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "membuf.h"
#include "netbuf.h"

/*
 * Lock used to ensure that we don't run into any memory corruption problems,
 * but without causing any trouble with interrupt latencies either.
 */
struct lock netbuf_lock;

/*
 * __netbuf_free()
 *	Callback to handle cleaning up a netbuf.
 */
void __netbuf_free(void *buf)
{
	struct netbuf *nb = (struct netbuf *)buf;
	
	if (nb->nb_datalink_membuf) {
		membuf_deref(nb->nb_datalink_membuf);
	}
	if (nb->nb_network_membuf) {
		membuf_deref(nb->nb_network_membuf);
	}
	if (nb->nb_transport_membuf) {
		membuf_deref(nb->nb_transport_membuf);
	}
	if (nb->nb_application_membuf) {
		membuf_deref(nb->nb_application_membuf);
	}
	if (nb->nb_hint_membuf) {
		membuf_deref(nb->nb_hint_membuf);
	}
}

/*
 * netbuf_alloc()
 */
struct netbuf *netbuf_alloc(void)
{
	struct netbuf *nb;
	
	nb = (struct netbuf *)membuf_alloc(sizeof(struct netbuf), __netbuf_free);
	
	nb->nb_datalink_membuf = NULL;
	nb->nb_datalink = NULL;
	nb->nb_datalink_size = 0;
	nb->nb_network_membuf = NULL;
	nb->nb_network = NULL;
	nb->nb_network_size = 0;
	nb->nb_transport_membuf = NULL;
	nb->nb_transport = NULL;
	nb->nb_transport_size = 0;
	nb->nb_application_membuf = NULL;
	nb->nb_application = NULL;
	nb->nb_application_size = 0;
	nb->nb_next = NULL;
	nb->nb_hint_membuf = NULL;

	return nb;
}

/*
 * netbuf_clone()
 *	Produce a clone (copy) of a netbuf.
 */
struct netbuf *netbuf_clone(struct netbuf *orig)
{
	struct netbuf *nb;
	
	spinlock_lock(&netbuf_lock);
	
	nb = (struct netbuf *)membuf_alloc(sizeof(struct netbuf), __netbuf_free);
		
	memcpy(nb, orig, sizeof(struct netbuf));
        if (nb->nb_datalink_membuf) {
		membuf_ref(nb->nb_datalink_membuf);
        }
        if (nb->nb_network_membuf) {
		membuf_ref(nb->nb_network_membuf);
        }
        if (nb->nb_transport_membuf) {
		membuf_ref(nb->nb_transport_membuf);
        }
        if (nb->nb_application_membuf) {
		membuf_ref(nb->nb_application_membuf);
        }
	nb->nb_next = NULL;
	if (nb->nb_hint_membuf) {
		membuf_ref(nb->nb_hint_membuf);
	}
		
	spinlock_unlock(&netbuf_lock);

	return nb;
}

/*
 * netbuf_init()
 *	Initialize the netbuf handling.
 */
void netbuf_init(void)
{
	spinlock_init(&netbuf_lock, 0x08);
}
