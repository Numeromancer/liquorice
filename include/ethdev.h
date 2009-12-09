/*
 * ethdev.h
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
 * Structure used by a bound client to access the driver.
 */
struct ethdev_server {
	void (*eds_send)(struct netbuf *nb);
	u8_t *(*eds_get_mac)(struct ethdev_server *eds);
	struct ethdev_instance *eds_instance;
					/* The instance of the service to which this structure refers */
};

/*
 * Structure used to bind a client protocol handler to the driver.
 */
struct ethdev_client {
	void (*edc_recv)(void *clnt, struct netbuf *nb);
	struct ethdev_server *edc_server;
					/* Reference to the server structure given to our client */
	void *edc_instance;		/* The instance of our client */
};

/*
 * Instance of an ethdev service.
 */
struct ethdev_instance {
	struct lock edi_lock;
	struct ethdev_client *edi_client;
					/* Our client's reference object */
	u8_t edi_mac[6];		/* MAC address of the device */
	struct ethdev_server *(*edi_client_attach)(struct ethdev_instance *edi, struct ethdev_client *edc);
	void (*edi_client_detach)(struct ethdev_instance *edi, struct ethdev_server *eds);
};

/*
 * Function prototypes.
 */
extern u8_t *ethdev_get_mac(struct ethdev_server *eds);
extern struct ethdev_client *ethdev_client_alloc(void);
extern void ethdev_client_detach(struct ethdev_instance *edi, struct ethdev_server *eds);

/*
 * ethdev_client_ref()
 */
extern inline void ethdev_client_ref(struct ethdev_client *edc)
{
	membuf_ref(edc);
}

/*
 * ethdev_client_deref()
 */
extern inline ref_t ethdev_client_deref(struct ethdev_client *edc)
{
	return membuf_deref(edc);
}

/*
 * ethdev_server_ref()
 */
extern inline void ethdev_server_ref(struct ethdev_server *eds)
{
	membuf_ref(eds);
}

/*
 * ethdev_server_deref()
 */
extern inline ref_t ethdev_server_deref(struct ethdev_server *eds)
{
	return membuf_deref(eds);
}

/*
 * ethdev_instance_ref()
 */
extern inline void ethdev_instance_ref(struct ethdev_instance *edi)
{
	membuf_ref(edi);
}

/*
 * ethdev_instance_deref()
 */
extern inline ref_t ethdev_instance_deref(struct ethdev_instance *edi)
{
	return membuf_deref(edi);
}
