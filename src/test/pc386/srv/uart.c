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
#include "pic.h"
#include "uart.h"

/*
 * 16 bit baud rate divisor
 */
#define	COMBRD(x) (1843200 / (16 * (x)))

/*
 * Receive/transmit data here, also baud low
 */
#define DATA 0				/* Rx/tx data */
#define BAUDLO 0			/* Baud rate low byte */
#define IER 1				/* Interrupt enable register */
#define BAUDHI 1			/* Baud rate high byte */
#define IIR 2				/* Interrupt identification register */
#define FIFO 2				/* FIFO control register */
#define CFCR 3				/* Character format control register */
#define MCR 4				/* Modem control register */
#define LSR 5				/* Line status register */
#define MSR 6				/* Modem status register */
#define SCRATCH 7			/* Scratchpad register */

/*
 * Interrupt enable register, also baud high
 */
#define IER_ERXRDY 0x1
#define IER_ETXRDY 0x2
#define IER_ERLS 0x4
#define IER_EMSC 0x8

/*
 * Interrupt identification register
 */
#define IIR_FIFO_MASK 0xc0
#define IIR_MLS 0x0
#define IIR_NOPEND 0x1
#define IIR_TXRDY 0x2
#define IIR_RXRDY 0x4
#define IIR_RLS 0x6
#define IIR_RXTOUT 0xc
#define IIR_IMASK 0xf

/*
 * FIFO control register
 */
#define FIFO_ENABLE 0x01
#define FIFO_RCV_RST 0x02
#define FIFO_XMT_RST 0x04
#define FIFO_DMA_MODE 0x08
#define FIFO_TRIGGER_1 0x00
#define FIFO_TRIGGER_4 0x40
#define FIFO_TRIGGER_8 0x80
#define FIFO_TRIGGER_14 0xc0

/*
 * Character format control register
 */
#define CFCR_5BITS 0x00
#define CFCR_6BITS 0x01
#define CFCR_7BITS 0x02
#define CFCR_8BITS 0x03
#define CFCR_STOPB 0x04
#define CFCR_PODD 0x00
#define CFCR_PENAB 0x08
#define CFCR_PEVEN 0x10
#define CFCR_PMARK 0x20
#define CFCR_PSPACE 0x30
#define CFCR_SBREAK 0x40
#define CFCR_DLAB 0x80

/*
 * Modem control register
 */
#define MCR_DTR 0x01
#define MCR_RTS 0x02
#define MCR_DRS 0x04
#define MCR_IENABLE 0x08
#define MCR_LOOPBACK 0x10

/*
 * Line status register
 */
#define LSR_RXRDY 0x01
#define LSR_OE 0x02
#define LSR_PE 0x04
#define LSR_FE 0x08
#define LSR_BI 0x10
#define LSR_RCV_MASK 0x1f
#define LSR_TXRDY 0x20
#define LSR_TSRE 0x40
#define LSR_RCV_FIFO 0x80

/*
 * Modem status register
 */
#define MSR_DCTS 0x01
#define MSR_DDSR 0x02
#define MSR_TERI 0x04
#define MSR_DDCD 0x08
#define MSR_CTS 0x10
#define MSR_DSR 0x20
#define MSR_RI 0x40
#define MSR_DCD 0x80

/*
 * Depth of the software receive FIFO.
 */
#define UART_RX_FIFO_DEPTH 16

/*
 * Speed
 */
#define UART_BAUD_RATE 9600

/*
 * Globals.
 */
static struct lock uart_isr_lock;
static struct context *recv_isr_ctx;
static struct context *data_isr_ctx;
static struct uart_client *client = NULL;
static struct lock uart_lock;
static u8_t recv_fifo[UART_RX_FIFO_DEPTH];
static u8_t recv_head = 0;
static u8_t recv_tail = 0;

/*
 * uart_read8()
 *	Perform an I/O read on the UART.
 */
#define uart_read8(reg) \
	in8(0x3f8 + reg)

/*
 * uart_write8()
 *	Perform an I/O write on the UART.
 */
#define uart_write8(reg, data) \
	out8(0x3f8 + reg, data)

/*
 * trap_irq4()
 */
void trap_irq4(void)
{
	u8_t iir;
	
	debug_set_lights(0x01);

	isr_spinlock_lock(&uart_isr_lock);

	pic_acknowledge(4);
				
	debug_check_stack(0x30);
	
	/*
	 * Loop round until we've handled all of the reasons for the interrupt.
	 */
	do {
		iir = uart_read8(IIR) & IIR_IMASK;
		
		switch (iir) {
		case IIR_TXRDY:
			uart_write8(IER, uart_read8(IER) & (~IER_ETXRDY));
			isr_context_signal(data_isr_ctx);
			break;
		
		case IIR_RXRDY:
		case IIR_RXTOUT:
			{
				u8_t lsr;
				u8_t next;
				
				lsr = uart_read8(LSR);
				while (lsr & LSR_RXRDY) {
					recv_fifo[recv_head] = uart_read8(DATA);
					next = (recv_head + 1) & (UART_RX_FIFO_DEPTH - 1);
					if (next != recv_tail) {
						recv_head = next;
					}
					lsr = uart_read8(LSR);
				}

				isr_context_signal(recv_isr_ctx);
			}
			break;
		
		default:
			iir = IIR_NOPEND;
		}
	} while (iir != IIR_NOPEND);

	isr_spinlock_unlock(&uart_isr_lock);
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
	pic_enable(4);
	
	isr_disable();
	debug_set_lights(0x02);
	isr_spinlock_lock(&uart_isr_lock);
	
	while (1) {
		while (recv_tail == recv_head) {
			isr_context_wait(&uart_isr_lock);
		}

		while (recv_tail != recv_head) {
			u8_t ch;
                				
			ch = recv_fifo[recv_tail++];
			recv_tail &= (UART_RX_FIFO_DEPTH - 1);
		
			isr_spinlock_unlock(&uart_isr_lock);
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
			isr_spinlock_lock(&uart_isr_lock);
		}
	}
}

/*
 * uart_data_send_u8()
 */
void uart_data_send_u8(void *srv, u8_t ch)
{
	isr_disable();
	debug_set_lights(0x05);
	isr_spinlock_lock(&uart_isr_lock);
	
	while (!(uart_read8(LSR) & LSR_TXRDY)) {
		uart_write8(IER, uart_read8(IER) | IER_ETXRDY);
		isr_context_wait(&uart_isr_lock);
	}
	
	uart_write8(DATA, ch);

	isr_spinlock_unlock(&uart_isr_lock);
	isr_enable();
}

/*
 * uart_data_set_isr()
 */
void uart_data_set_isr(void)
{
	isr_disable();
	debug_set_lights(0x06);
	isr_spinlock_lock(&uart_isr_lock);

	data_isr_ctx = current_context;
	
	uart_write8(IER, uart_read8(IER) | IER_ERXRDY);
				
	isr_spinlock_unlock(&uart_isr_lock);
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
        u16_t bits;
        struct uart_instance *ui;

	ui = (struct uart_instance *)membuf_alloc(sizeof(struct uart_instance), NULL);
	ui->ui_client_attach = uart_client_attach;
	ui->ui_client_detach = uart_client_detach;

	spinlock_init(&uart_lock, 0x0c);
	spinlock_init(&uart_isr_lock, 0x00);

	/*
	 * Default us to 9600, 8, N, 1
	 */
	bits = COMBRD(UART_BAUD_RATE);
	uart_write8(CFCR, CFCR_DLAB);
	uart_write8(BAUDHI, (bits >> 8) & 0xff);
	uart_write8(BAUDLO, bits & 0xff);
	uart_write8(CFCR, CFCR_8BITS);
	uart_write8(FIFO, FIFO_TRIGGER_1 | FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST);
	uart_write8(MCR, MCR_IENABLE);
		
	thread_create(uart_recv_intr_thread, NULL, 0x1800, 0x40);

	return ui;
}
