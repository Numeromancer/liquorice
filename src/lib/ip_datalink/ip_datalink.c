/*
 * ip_datalink.c
 *	IP datalink service support.
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
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "membuf.h"
#include "netbuf.h"
#include "uart.h"
#include "ip_datalink.h"

/*
 * ip_datalink_client_alloc()
 *	Allocate an ip_datalink client structure.
 */
struct ip_datalink_client *ip_datalink_client_alloc(void)
{
	struct ip_datalink_client *idc;
	
	idc = (struct ip_datalink_client *)membuf_alloc(sizeof(struct ip_datalink_client), NULL);
	idc->idc_instance = NULL;
	
	return idc;
}

/*
 * ip_datalink_client_detach()
 *	Detach a client from the ip_datalink driver.
 */
void ip_datalink_client_detach(struct ip_datalink_instance *idi, struct ip_datalink_server *ids)
{
	spinlock_lock(&idi->idi_lock);

	if (idi->idi_client->idc_server == ids) {
		ip_datalink_server_deref(ids);
		ip_datalink_instance_deref(idi);
		ip_datalink_client_deref(idi->idi_client);
		idi->idi_client = NULL;
	}
		
	spinlock_unlock(&idi->idi_lock);
}
