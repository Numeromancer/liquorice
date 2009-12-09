/*
 * ethdev.c
 *	Ethernet device support.
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
 * The ethdev code is designed to provide a support framework for an Ethernet
 * device driver.  It provides a standard interface to other objects that need
 * to access a Ethernet driver's services.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "membuf.h"
#include "netbuf.h"
#include "ethdev.h"

/*
 * ethdev_get_mac()
 */
u8_t *ethdev_get_mac(struct ethdev_server *eds)
{
	return eds->eds_instance->edi_mac;
}

/*
 * ethdev_client_alloc()
 *	Allocate an Ethernet device client structure.
 */
struct ethdev_client *ethdev_client_alloc(void)
{
	struct ethdev_client *edc;
	
	edc = (struct ethdev_client *)membuf_alloc(sizeof(struct ethdev_client), NULL);
	edc->edc_instance = NULL;

	return edc;
}

/*
 * ethdev_client_detach()
 *	Detach a client from an Ethernet device.
 */
void ethdev_client_detach(struct ethdev_instance *edi, struct ethdev_server *eds)
{
	spinlock_lock(&edi->edi_lock);

	if (edi->edi_client->edc_server == eds) {
		ethdev_server_deref(eds);
		ethdev_instance_deref(edi);
		ethdev_client_deref(edi->edi_client);
		edi->edi_client = NULL;
	}
		
	spinlock_unlock(&edi->edi_lock);
}
