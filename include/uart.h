/*
 * uart.h
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
 * Forward reference
 */
struct uart_instance;

/*
 * Structure used by a bound client to access the UART.
 */
struct uart_server {
	void *us_instance;		/* The instance of this service */
	void (*us_send)(void *srv, u8_t data);
	void (*us_set_send_isr)(void);
};

/*
 * Structure used to bind a client protocol handler to the UART.
 */
struct uart_client {
	void *uc_instance;		/* The instance of our client */
	struct uart_server *uc_server;	/* Reference to the server structure given to our client */
	void (*uc_recv)(void *clnt, u8_t data);
};

/*
 * Instance of the UART service.
 */
struct uart_instance {
	struct uart_client *ui_client;
	struct uart_server *(*ui_client_attach)(struct uart_instance *ui, struct uart_client *uc);
	void (*ui_client_detach)(struct uart_instance *ui, struct uart_server *us);
};

/*
 * Global functions
 */
extern struct uart_client *uart_client_alloc(void);
extern struct uart_instance *uart_instance_alloc(void);

/*
 * uart_client_ref()
 */
extern inline void uart_client_ref(struct uart_client *uc)
{
	membuf_ref(uc);
}

/*
 * uart_client_deref()
 */
extern inline ref_t uart_client_deref(struct uart_client *uc)
{
	return membuf_deref(uc);
}

/*
 * uart_server_ref()
 */
extern inline void uart_server_ref(struct uart_server *us)
{
	membuf_ref(us);
}

/*
 * uart_server_deref()
 */
extern inline ref_t uart_server_deref(struct uart_server *us)
{
	return membuf_deref(us);
}

/*
 * uart_instance_ref()
 */
extern inline void uart_instance_ref(struct uart_instance *ui)
{
	membuf_ref(ui);
}

/*
 * uart_instance_deref()
 */
extern inline ref_t uart_instance_deref(struct uart_instance *ui)
{
	return membuf_deref(ui);
}
