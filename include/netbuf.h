/*
 * netbuf.h
 *	Network buffer information.
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
 * Network buffer layout.
 *
 * The structure of the network buffer is based on the ISO 7 layer protocol
 * stack.  We don't have all of the 7 here, but the others aren't relevant at
 * the moment (IP uses 5 for example and the lowest level, physical, isn't
 * likely to be relevant to software under almost all instances).
 */
struct netbuf {
	void *nb_datalink_membuf;	/* Pointer to allocated membuf - if one has been allocated */
	void *nb_datalink;		/* The datalink layer's data */
	addr_t nb_datalink_size;	/* Size of the datalink layer's data */
	void *nb_network_membuf;
	void *nb_network;
	addr_t nb_network_size;
	void *nb_transport_membuf;
	void *nb_transport;
	addr_t nb_transport_size;
	void *nb_application_membuf;
	void *nb_application;
	addr_t nb_application_size;
	struct netbuf *nb_next;
	void *nb_hint_membuf;
};

/*
 * Prototypes.
 */
extern struct netbuf *netbuf_alloc(void);
extern struct netbuf *netbuf_clone(struct netbuf *orig);
extern void netbuf_init(void);

/*
 * netbuf_ref()
 *	Reference a netbuf.
 */
extern inline void netbuf_ref(struct netbuf *nb)
{
	membuf_ref(nb);
}

/*
 * netbuf_deref()
 *	Dereference a netbuf.
 */
extern inline ref_t netbuf_deref(struct netbuf *nb)
{
	return membuf_deref(nb);
}

/*
 * netbuf_get_refs()
 *	Get the number of references to a netbuf.
 */
extern inline ref_t netbuf_get_refs(struct netbuf *nb)
{
	return membuf_get_refs(nb);
}
