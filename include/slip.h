/*
 * slip.h
 *	Serial Line IP definitions.
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
 * Instance of the SLIP service.
 *
 * This is subclassed from the "ip_datalink_instance" class.  Note that the
 * si_idi field must be the first in the structure in order for the code to
 * work.
 */
struct slip_instance {
	struct ip_datalink_instance si_idi;
	struct uart_server *si_server;
	u8_t si_recv_esc;
	u16_t si_recv_octets;
	u8_t si_recv_ignore;
	u8_t *si_recv_packet;
	struct context *si_send_ctx;
	struct netbuf *si_send_queue;
};

/*
 * Structure type referenced in parameter list.
 */
struct uart_instance;

/*
 * Function prototypes.
 */
extern struct ip_datalink_instance *slip_instance_alloc(struct uart_instance *ui);
