/*
 * uart.c
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
#include "cpu.h"
#include "io.h"
#include "memory.h"
#include "isr.h"
#include "debug.h"
#include "context.h"
#include "thread.h"
#include "membuf.h"
#include "uart.h"

/*
 * Speed.
 */
#define UART_BAUD_RATE 9600

/*
 * Baud rate calculation.
 */
#define UART_BAUD_SELECT (CPU_SPEED / (UART_BAUD_RATE * 16L) - 1)

/*
 * Depth of the software receive FIFO.
 */
#define UART_RX_FIFO_DEPTH 16

/*
 * Globals.
 */
static struct context *recv_isr_ctx;
static struct lock recv_isr_lock;
static struct context *data_isr_ctx;
static struct lock data_isr_lock;
static struct uart_client *client = NULL;
static struct lock uart_lock;
static u8_t recv_fifo[UART_RX_FIFO_DEPTH];
static u8_t recv_head = 0;
static u8_t recv_tail = 0;

/*
 * uart_recv_isr()
 */
void uart_recv_isr(void)
{
	u8_t next;
	
	debug_set_lights(0x01);

	isr_spinlock_lock(&recv_isr_lock);

	debug_check_stack(0x30);

	recv_fifo[recv_head] = in8(UDR);
	next = (recv_head + 1) & (UART_RX_FIFO_DEPTH - 1);
	if (next != recv_tail) {
		recv_head = next;
	}
	
	isr_context_signal(recv_isr_ctx);

	isr_spinlock_unlock(&recv_isr_lock);
}

/*
 * _uart_recv_()
 *	Interrupt service routine.
 *
 * As a rule of thumb we try and avoid doing any processing in hardware ISRs
 * and prefer to redirect the interrupt to a thread for handling at a user
 * priority.  Unfortunately, in the case of the AVR UART there is no FIFO, so,
 * if we can't schedule the thread and run it fast enough, we can lose data.
 * As UART receive operations are fast then we make an exception and provide
 * a very small receive FIFO.
 */
void _uart_recv_(void) __attribute__ ((naked));
void _uart_recv_(void)
{
	asm volatile ("push r0\n\t"
			"push r1\n\t"
			"in r0, 0x3f\n\t"
			"push r0\n\t"
#ifdef ATMEGA103
			"in r0, 0x3b\n\t"
			"push r0\n\t"
#endif
			"push r18\n\t"
			"push r19\n\t"
			"push r20\n\t"
			"push r21\n\t"
			"push r22\n\t"
			"push r23\n\t"
			"push r24\n\t"
			"push r25\n\t"
			"push r26\n\t"
			"push r27\n\t"
			"push r30\n\t"
			"push r31\n\t"
			::);

	uart_recv_isr();
				
	asm volatile ("jmp startup_continue\n\t" ::);
}

/*
 * uart_recv_intr_thread()
 */
void uart_recv_intr_thread(void *arg) __attribute__ ((noreturn));
void uart_recv_intr_thread(void *arg)
{
	recv_isr_ctx = current_context;
	
	/*
	 * Release the interrupt to the processor.
	 */
	out8(UCR, in8(UCR) | BV(RXCIE));
	
	isr_disable();
	debug_set_lights(0x02);
	isr_spinlock_lock(&recv_isr_lock);
	
	while (1) {
		while (recv_tail == recv_head) {
			isr_context_wait(&recv_isr_lock);
		}

		while (recv_tail != recv_head) {
			u8_t ch;

			ch = recv_fifo[recv_tail++];
			recv_tail &= (UART_RX_FIFO_DEPTH - 1);
		
			isr_spinlock_unlock(&recv_isr_lock);
			isr_enable();
			
			spinlock_lock(&uart_lock);
			if (client) {
				uart_client_ref(client);
				spinlock_unlock(&uart_lock);
				client->uc_recv(client, ch);
				spinlock_lock(&uart_lock);
				uart_client_deref(client);
			}
			spinlock_unlock(&uart_lock);
			
			isr_disable();
			debug_set_lights(0x02);
			isr_spinlock_lock(&recv_isr_lock);
		}
	}
}

/*
 * uart_data_isr()
 */
void uart_data_isr(void)
{
	debug_set_lights(0x41);
	
	isr_spinlock_lock(&data_isr_lock);

	out8(UCR, in8(UCR) & (~BV(UDRIE)));

	debug_check_stack(0x30);
	
	isr_context_signal(data_isr_ctx);

	isr_spinlock_unlock(&data_isr_lock);
}

/*
 * _uart_data_()
 */
void _uart_data_(void) __attribute__ ((naked));
void _uart_data_(void)
{
	asm volatile ("push r0\n\t"
			"push r1\n\t"
			"in r0, 0x3f\n\t"
			"push r0\n\t"
#ifdef ATMEGA103
			"in r0, 0x3b\n\t"
			"push r0\n\t"
#endif
			"push r18\n\t"
			"push r19\n\t"
			"push r20\n\t"
			"push r21\n\t"
			"push r22\n\t"
			"push r23\n\t"
			"push r24\n\t"
			"push r25\n\t"
			"push r26\n\t"
			"push r27\n\t"
			"push r30\n\t"
			"push r31\n\t"
			::);

	uart_data_isr();

	asm volatile ("jmp startup_continue\n\t" ::);
}

/*
 * uart_data_send_u8()
 */
void uart_data_send_u8(void *srv, u8_t ch)
{
	isr_disable();
	debug_set_lights(0x05);
	isr_spinlock_lock(&data_isr_lock);
	
	while (!(in8(USR) & BV(UDRE))) {
		out8(UCR, in8(UCR) | BV(UDRIE));
		isr_context_wait(&data_isr_lock);
	}
	
	out8(UDR, ch);

	isr_spinlock_unlock(&data_isr_lock);
	isr_enable();
}

/*
 * uart_data_set_isr()
 */
void uart_data_set_isr(void)
{
	isr_disable();
	debug_set_lights(0x06);
	isr_spinlock_lock(&data_isr_lock);

	data_isr_ctx = current_context;
	
	out8(UCR, in8(UCR) | BV(UDRIE));
				
	isr_spinlock_unlock(&data_isr_lock);
	isr_enable();
}

/*
 * uart_server_alloc()
 *	Allocate an UART server structure.
 */
struct uart_server *uart_server_alloc(void)
{
	struct uart_server *us;
	
	us = (struct uart_server *)membuf_alloc(sizeof(struct uart_server), NULL);
	us->us_send = uart_data_send_u8;
	us->us_set_send_isr = uart_data_set_isr;
        	
	return us;
}

/*
 * uart_client_alloc()
 *	Allocate an UART client structure.
 */
struct uart_client *uart_client_alloc(void)
{
	struct uart_client *uc;
	
	uc = (struct uart_client *)membuf_alloc(sizeof(struct uart_client), NULL);
	uc->uc_instance = NULL;

	return uc;
}

/*
 * uart_client_attach()
 *	Attach a protocol handler to the UART.
 */
struct uart_server *uart_client_attach(struct uart_instance *ui, struct uart_client *uc)
{
	struct uart_server *us = NULL;
		
	spinlock_lock(&uart_lock);

	if (client == NULL) {
		us = uart_server_alloc();
                us->us_instance = ui;
                uart_instance_ref(ui);

                uc->uc_server = us;
		client = uc;
		uart_client_ref(uc);
	}
		
	spinlock_unlock(&uart_lock);

	return us;
}

/*
 * uart_client_detach()
 *	Detach a protocol handler to the UART.
 */
void uart_client_detach(struct uart_instance *ui, struct uart_server *us)
{
	spinlock_lock(&uart_lock);

	if (client->uc_server == us) {
		uart_server_deref(us);
		uart_instance_deref(ui);
		uart_client_deref(client);
		client = NULL;
	}
		
	spinlock_unlock(&uart_lock);
}

/*
 * uart_instance_alloc()
 */
struct uart_instance *uart_instance_alloc(void)
{
        struct uart_instance *ui;

	ui = (struct uart_instance *)membuf_alloc(sizeof(struct uart_instance), NULL);
	ui->ui_client_attach = uart_client_attach;
	ui->ui_client_detach = uart_client_detach;

	spinlock_init(&uart_lock, 0x0c);
	spinlock_init(&recv_isr_lock, 0x00);
	spinlock_init(&data_isr_lock, 0x00);
	
	out8(UCR, BV(RXEN) | BV(TXEN));
	
	out8(UBRR, (u8_t)UART_BAUD_SELECT);
			
	thread_create(uart_recv_intr_thread, NULL, 0x180, 0x40);

	return ui;
}
