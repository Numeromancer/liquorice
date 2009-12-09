/*
 * main.c
 *	Main entry point into the code.
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
#include "string.h"
#include "debug.h"
#include "context.h"
#include "condvar.h"
#include "heap.h"
#include "thread.h"
#include "membuf.h"
#include "netbuf.h"
#include "timer.h"
#include "oneshot.h"
#include "uart.h"
#include "ppp_ahdlc.h"
#include "ethdev.h"
#include "3c509.h"
#include "ne2000.h"
#include "smc91c96.h"
#include "ip_datalink.h"
#include "slip.h"
#include "ppp.h"
#include "ethernet.h"
#include "ppp_ip.h"
#include "ethernet_ip.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"
#include "bbi2c.h"

/*
 * Stack size for our idle thread
 */
#define IDLE_SIZE 0xb0

/*
 * I2C address of the 8583.
 */
#define ADDR_8583 0xa0

/*
 * A gcc-defined symbol that signifies the end of the bss section.
 */
extern void *__bss_end;

/*
 * _interrupt2_()
 */
#if DEBUG
#if !defined(PCB520)
void _interrupt2_(void) __attribute__ ((naked));
void _interrupt2_(void)
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

	while (1) {
		debug_print_pstr("\fint ctrl:");
		debug_print8(in8(EIFR));
		debug_print8(in8(EIMSK));
		debug_print8(in8(EICR));
		debug_wait_button();
		debug_stack_trace();
	}

	asm volatile ("jmp startup_continue\n\t" ::);
}
#endif
#endif

/*
 * num()
 *	Convert a number to a string
 */
static char *num(char *obuf, unsigned long x, unsigned int base, u8_t issigned)
{
	char buf[16];
	char *p = buf + 16;
	unsigned int c;
        int sign = 0;
		
	if ((base == 10) && ((long)x < 0) && issigned) {
		*obuf++ = '-';
		x = -(long)x;
		sign++;
	}

	*--p = '\0';
	do {
		c = (x % base);
		if (c < 10) {
			*--p = '0' + c;
		} else {
			*--p = 'a' + (c - 10);
		}
		x /= base;
	} while (x != 0);

	strcpy(obuf, p);

	return obuf + (addr_t)buf + sizeof(buf) - 1 - (addr_t)p;
}

/*
 * sprintf()
 *	Print to a string - really trivial impelementation!
 */
addr_t sprintf(char *buf, const char *fmt, ...)
{
	char *bp = (char *)fmt;
	va_list ap;
	char c;
        char *p;
        u8_t longflag;

        p = buf;
	va_start(ap, fmt);
        	
	while ((c = *bp++)) {
		if (c != '%') {
			*p++ = c;
			continue;
		}

		longflag = FALSE;
		if (*bp == 'l') {
			longflag = TRUE;
			bp++;
		}
		
		switch ((c = *bp++)) {
		case 'd':
			if (longflag) {
				p = num(p, va_arg(ap, long), 10, TRUE);
			} else {
				p = num(p, va_arg(ap, int), 10, TRUE);
			}
			break;

		case 'u':
			if (longflag) {
				p = num(p, va_arg(ap, unsigned long), 10, FALSE);
			} else {
				p = num(p, va_arg(ap, unsigned int), 10, FALSE);
			}
                        break;
						
		case 'x':
			*p++ = '0';
			*p++ = 'x';
			if (longflag) {
				p = num(p, va_arg(ap, unsigned long), 16, FALSE);
			} else {
				p = num(p, va_arg(ap, unsigned int), 16, FALSE);
			}
			break;

		case 's':
			strcpy(p, va_arg(ap, char *));
			p += strlen(p);
			break;

		default:
			*p++ = '?';
			break;
		}
	}

	*p = '\0';

	return (addr_t)(p - buf);
}

struct lock test1_lock;

/*
 * test1_recv()
 *	A target for ICMP messages.
 */
void test1_recv(void *clnt, struct netbuf *nb)
{
	debug_print_pstr("\ftest1 recv ICMP echo response");
}

/*
 * test1_tick()
 *	Timer callback.
 */
void test1_tick(void *arg)
{
	spinlock_lock(&test1_lock);
	thread_interrupt((struct thread *)arg);
	spinlock_unlock(&test1_lock);
}

/*
 * test1_thread()
 *	ICMP echo test (ping!)
 *
 * This thread demonstrates chaining ICMP messages to user-code.  Here we hook
 * the ICMP echo response message and then go into a loop where we hit a
 * target host with an ICMP echo request (approximately 1 every 10 seconds).
 */
void test1_thread(void *arg) __attribute__ ((noreturn));
void test1_thread(void *arg)
{
	struct ip_instance *ii;
	struct icmp_client *ic;
	struct icmp_server *is;
        struct oneshot *os;

	spinlock_init(&test1_lock, 0x50);

	ii = (struct ip_instance *)arg;
        	
	/*
	 * Hook the ICMP echo response message.
	 */
	ic = icmp_client_alloc();
	ic->ic_type = 0;
        ic->ic_recv = test1_recv;
	is = ii->ii_icmp_client_attach(ii, ic);

        os = oneshot_alloc();
        os->os_arg = current_context;
        os->os_callback = test1_tick;
                		
	while (1) {
		struct netbuf *nb;
                u16_t *p;
                u32_t empty = 0;
	        u16_t seq = 0;
                                		
		/*
		 * Issue an ICMP echo request.
		 */	
		nb = netbuf_alloc();
		nb->nb_application_membuf = membuf_alloc(32, NULL);
		nb->nb_application = nb->nb_application_membuf;
	        nb->nb_application_size = 32;

		p = (u16_t *)&empty;
                *p++ = 0;
                *p++ = hton16(seq);
                seq++;
	
	        /*
	         * Ping one of the routers at my ISP :-)  Just checks that the
	         * default route code works too!
	         */
		is->is_send(is, hton32(0xc366ff9b), 0x08, 0x00, (u8_t *)(&empty), nb);

		netbuf_deref(nb);
			
		/*
		 * Set for a wakeup interrupt in 10 seconds - then wait for it!
		 */
		spinlock_lock(&test1_lock);
		os->os_ticks_left = 10 * TICK_RATE;
		oneshot_attach(os);
		thread_wait(&test1_lock);
		spinlock_unlock(&test1_lock);
	}
}

/*
 * test2_recv()
 *	UDP message receiver for test2.
 */
void test2_recv(void *clnt, struct netbuf *nb)
{
	struct udp_client *uc;
	struct udp_server *us;
	struct ip_header *iph;
	struct udp_header *udh;
	struct netbuf *nbrep;

	uc = (struct udp_client *)clnt;
	us = uc->uc_server;
	
	/*
	 * Take the received message, swap some of it round and send it back!
	 */	
	iph = (struct ip_header *)nb->nb_network;
	udh = (struct udp_header *)nb->nb_transport;

	nbrep = netbuf_alloc();
	nbrep->nb_application_membuf = membuf_alloc(nb->nb_application_size, NULL);
	nbrep->nb_application = nbrep->nb_application_membuf;
        nbrep->nb_application_size = nb->nb_application_size;
	        		                				
	memcpy(nbrep->nb_application, nb->nb_application, nb->nb_application_size);
	us->us_send(us, iph->ih_src_addr, udh->uh_src_port, nbrep);
		
	netbuf_deref(nbrep);
}

/*
 * test2_init()
 */
void test2_init(struct udp_instance *ui)
{
	struct udp_client *uc;
	struct udp_server *us;

	/*
	 * Hook UDP packets for port 1234.
	 */
	uc = udp_client_alloc();
        uc->uc_port = 1234;
        uc->uc_recv = test2_recv;
	us = ui->ui_client_attach(ui, uc);
}

struct lock test3_lock;
int test3_count;

/*
 * test3_close()
 */
void test3_close(void *clnt)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
	tcpi = ts->ts_instance;
	
	tcpi->ti_client_detach(tcpi, ts);
}

/*
 * test3_recv()
 *	A target for TCP messages.
 */
void test3_recv(void *clnt, struct netbuf *nb)
{
	struct tcp_client *tc;
	struct tcp_server *ts;
	struct tcp_instance *tcpi;
	struct netbuf *nbr;
	
	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
	tcpi = ts->ts_instance;

	spinlock_lock(&test3_lock);
			
	test3_count++;
	if (test3_count > 10) {
		test3_count = 0;
		spinlock_unlock(&test3_lock);
		
		tcpi->ti_client_detach(tcpi, ts);
		return;
	}
	
	spinlock_unlock(&test3_lock);
        		
	/*
	 * Package up what we've received and send it back.
	 */		
	nbr = netbuf_alloc();
	nbr->nb_application_membuf = membuf_alloc(nb->nb_application_size, NULL);
	nbr->nb_application = nbr->nb_application_membuf;
        nbr->nb_application_size = nb->nb_application_size;
	
	memcpy(nbr->nb_application, nb->nb_application, nb->nb_application_size);
			
	ts->ts_send(ts, nbr);

	netbuf_deref(nbr);
}

/*
 * test3_connect()
 */
void test3_connect(void *clnt, void *srv)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	spinlock_lock(&test3_lock);
	if (test3_count) {
		spinlock_unlock(&test3_lock);
		return;
	}
	
	spinlock_unlock(&test3_lock);

	tcpi = ((struct tcp_server *)srv)->ts_instance;
	
	tc = tcp_client_alloc();
	tc->tc_recv = test3_recv;
	tc->tc_close = test3_close;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
	
	ts->ts_accept(ts, srv);
}

/*
 * test3_init()
 */
void test3_init(struct tcp_instance *tcpi)
{
	struct tcp_client *tc;
	struct tcp_server *ts;

	test3_count = 0;	
	spinlock_init(&test3_lock, 0x30);
		
	/*
	 * Hook TCP requests for our server socket.
	 */
	tc = tcp_client_alloc();
	tc->tc_recv = test3_recv;
	tc->tc_connect = test3_connect;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
		
	ts->ts_listen(ts, 1235);
}

struct lock test4_lock;

/*
 * test4_close()
 */
void test4_close(void *clnt)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
	tcpi = ts->ts_instance;
	
	tcpi->ti_client_detach(tcpi, ts);
}

/*
 * test4_recv()
 *	A target for TCP messages.
 */
void test4_recv(void *clnt, struct netbuf *nb)
{
	u8_t c, i;
        u8_t *p;
		
	debug_print_pstr("\ftest:\n\r");
	c = (nb->nb_application_size > 8) ? 8 : nb->nb_application_size;
	
	p = nb->nb_application;
	for (i = 0; i < c; i++) {
		debug_print8(*p++);
	}
}

/*
 * test4_tick()
 *	Timer callback.
 */
void test4_tick(void *arg)
{
	spinlock_lock(&test4_lock);
	thread_interrupt((struct thread *)arg);
	spinlock_unlock(&test4_lock);
}

/*
 * test4_thread()
 *	Test client thread that tries to connect to a remote "echo" service.
 */
void test4_thread(void *arg) __attribute__ ((noreturn));
void test4_thread(void *arg)
{
	struct tcp_instance *tcpi;
	struct tcp_client *tc;
	struct oneshot *os;
	struct tcp_server *test4_srv;

	tcpi = (struct tcp_instance *)arg;
		
	spinlock_init(&test4_lock, 0x50);
	        		
	tc = tcp_client_alloc();
	tc->tc_recv = test4_recv;
	tc->tc_close = test4_close;
	test4_srv = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
		
	test4_srv->ts_connect(test4_srv, 0xc0a800c9, 7);

        os = oneshot_alloc();
        os->os_arg = current_context;
        os->os_callback = test4_tick;
                		
	if (test4_srv->ts_socket->ts_state == TCS_ESTABLISHED) {
		u8_t x;
		
		for (x = 0; x < 10; x++) {
			struct netbuf *nbr;
      	               	u8_t buf[8];
                        u8_t i;
                	
                	for (i = 0; i < 8; i++) {
                		buf[i] = x * 0x10 + i;
                	}

			nbr = netbuf_alloc();
			nbr->nb_application_membuf = membuf_alloc(8, NULL);
			nbr->nb_application = nbr->nb_application_membuf;
		        nbr->nb_application_size = 8;
	
			memcpy(nbr->nb_application, buf, 8);
			
			test4_srv->ts_send(test4_srv, nbr);

			netbuf_deref(nbr);
                					
			spinlock_lock(&test4_lock);
			os->os_ticks_left = 2 * TICK_RATE;
			oneshot_attach(os);
			thread_wait(&test4_lock);
			spinlock_unlock(&test4_lock);
		}
		
		tcpi->ti_client_detach(tcpi, test4_srv);
	}

	thread_exit();
}

struct lock test5_lock;
u32_t test5_hits = 0L;

/*
 * parse_tokens()
 *	Really dumb parser.
 *
 * Must find two tokens, each followed by a space.
 */
u8_t parse_tokens(char *str, char **tk1, char **tk2)
{
	char *p;
	
	*tk1 = str;
	p = strchr(str, ' ');
	if (p) {
		*p++ = '\0';
		*tk2 = p;
		p = strchr(p, ' ');
		if (p) {
			*p = '\0';
			return TRUE;
		}
	}
	
	return FALSE;
}

/*
 * parse_get()
 */
char *parse_get(char *path)
{
	char *outbuf;
        char *header;
        char *body;
        int hlen;
        int blen;

	test5_hits++;

        body = heap_alloc(1024);
        sprintf(body, "<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\r\n"
        		"<html>\r\n"
        		"<body>\r\n"
        		"&nbsp\r\n"
        		"<h1><center>Welcome To The Liquorice Web Server</center></h1>\r\n"
        		"<p><center>You can find out more about Liquorice from it's web site hosted at Sourceforge</center>\r\n"
        		"<p><center><a href=\"http://liquorice.sourceforge.net/\">liquorice.sourceforge.net</a></center>\r\n"
        		"<p>\r\n"
        		"<p><center>Alternatively, you can email the author:</center>\r\n"
        		"<p><center><a href=\"mailto:davejh@users.sourceforge.net\">davejh@users.sourceforge.net</a></center>\r\n"
        		"<p>\r\n"
        		"<p><center>This page has had %ld hits!</center>\r\n"
        		"</body>\r\n"
        		"</html>\r\n"
        		"\r\n",
        		test5_hits);

        blen = strlen(body);
        		
	header = heap_alloc(256);
	sprintf(header, "HTTP/1.0 200 OK\r\n"
			"Server: Liquorice-AVR\r\n"
                        "Content-Type: text/html\r\n"
                        "Accept-Ranges: bytes\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        blen);
			        		                	
	hlen = strlen(header);
	outbuf = membuf_alloc(hlen + blen + 1, NULL);
	
        memcpy(outbuf, header, hlen);
        memcpy(outbuf + hlen, body, blen + 1);

        heap_free(header);
        heap_free(body);
        		
	return outbuf;
}

/*
 * parse_html()
 */
char *parse_html(char *inbuf)
{
        char *tk1, *tk2;
		
	if (parse_tokens(inbuf, &tk1, &tk2)) {
		if ((strcmp(tk1, "GET") == 0) || (strcmp(tk1, "get") == 0) || (strcmp(tk1, "Get") == 0)) {
			return parse_get(tk2);
		}
	}
	
	return NULL;
}

/*
 * test5_close()
 */
void test5_close(void *clnt)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
	tcpi = ts->ts_instance;
	
	tcpi->ti_client_detach(tcpi, ts);
}

/*
 * test5_recv()
 *	A target for TCP messages.
 *
 * Pretend that we're a web server :-)  This really is a seriously broken
 * implementation and assumes so much about how web browsers work and the TCP
 * datastream that it's almost miraculous that we get anywhere at all :-/
 */
void test5_recv(void *clnt, struct netbuf *nb)
{
	char *outbuf;
	struct tcp_client *tc;
	struct tcp_server *ts;
	struct tcp_instance *tcpi;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;

	outbuf = parse_html(nb->nb_application);
	if (outbuf) {
		struct netbuf *nbr;
				
		nbr = netbuf_alloc();
		nbr->nb_application_membuf = outbuf;
		nbr->nb_application = outbuf;
	        nbr->nb_application_size = strlen(outbuf) + 1;

		ts->ts_send(ts, nbr);

		netbuf_deref(nbr);
	}

	tcpi = (struct tcp_instance *)ts->ts_instance;
	tcpi->ti_client_detach(tcpi, ts);
}

/*
 * test5_connect()
 */
void test5_connect(void *clnt, void *srv)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tcpi = ((struct tcp_server *)srv)->ts_instance;
	
	tc = tcp_client_alloc();
	tc->tc_recv = test5_recv;
	tc->tc_close = test5_close;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
	
	ts->ts_accept(ts, srv);
}

/*
 * test5_init()
 */
void test5_init(struct tcp_instance *tcpi)
{
	struct tcp_client *tc;
	struct tcp_server *ts;
	
	spinlock_init(&test5_lock, 0x50);
	        		
	/*
	 * Hook TCP requests for our server socket.
	 */
	tc = tcp_client_alloc();
	tc->tc_recv = test5_recv;
	tc->tc_connect = test5_connect;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
		
	ts->ts_listen(ts, 80);
}

struct lock test6_lock;

/*
 * test6_body()
 *	Process a command and send back an ASCII stream in response.
 */
void test6_body(struct tcp_server *ts, u8_t cmd)
{
	struct netbuf *nbr;
	char *obuf, *p;
	int ct;
                                                			
	obuf = membuf_alloc(1024, NULL);
	p = obuf;

	switch (cmd) {
	case 'f':
		{
			struct memory_hole *mnext, *mbuf;

			mbuf = heap_alloc(32 * sizeof(struct memory_hole));
			p += sprintf(p, "\r\nFree memory: %d\r\n", heap_get_free());
			
			ct = heap_dump_stats(mbuf, 32);
			if (ct == 32) {
				p += sprintf(p, "Note: List at maximum...\r\n");
			}
			
			mnext = mbuf;
			while (ct) {
				p += sprintf(p, "addr:%x, size:%d\r\n", (addr_t)mnext->mh_next, mnext->mh_size);
				mnext++;
				ct--;
			}
			
			heap_free(mbuf);
		}
		break;

	case 'h':
		strcpy(p, "\r\nMonitor options\r\n"
			"f: Free heap (memory) chains\r\n"
			"h: Help\r\n"
			"l: Load averages\r\n"
			"o: One-shot timers\r\n"
			"q: Quit telnet session\r\n"
			"s: TCP sockets\r\n"
			"t: Threads\r\n"
			"u: UDP sockets\r\n");
		break;
					
	case 'l':
		{
			u32_t avg[3];
                        extern u8_t ldavg_runnable;
                        u8_t r;

                        isr_disable();
                        r = ldavg_runnable - 1;
                        isr_enable();
			
                        timer_dump_stats(avg, 3);
                        						
			p += sprintf(p, "\r\nLoad averages: %ld, %ld, %ld (%d)\r\n\r\n", avg[0], avg[1], avg[2], r);
		}
		break;
	
	case 'o':
		{
			struct oneshot *onext, *obuf;
			
			obuf = heap_alloc(32 * sizeof(struct oneshot));
			p += sprintf(p, "\r\nOne shot timer details:\r\n");
	
			ct = oneshot_dump_stats(obuf, 32);
			if (ct == 32) {
				p += sprintf(p, "Note: List at maximum...\r\n");
			}
			
			onext = obuf;
			while (ct) {
				p += sprintf(p, "addr:%x, ticks left %ld\r\n",
						(addr_t)onext->os_next, onext->os_ticks_left);
				onext++;
				ct--;
			}
		
			heap_free(obuf);
		}
		break;

/*
	case 's':
		{
			struct tcp_socket *snext, *sbuf;
			
			sbuf = heap_alloc(12 * sizeof(struct tcp_socket));
			p += sprintf(p, "\r\nTCP socket details:\r\n");
	
			ct = tcp_dump_stats(NULL, sbuf, 12);
			if (ct == 12) {
				p += sprintf(p, "Note: List at maximum...\r\n");
			}
			
			snext = sbuf;
			while (ct) {
				p += sprintf(p, "addr:%x, local:%lx:%x, remote:%lx:%x, state:%x\r\n",
						(addr_t)snext->ts_next,
						snext->ts_local_addr, snext->ts_local_port,
						snext->ts_remote_addr, snext->ts_remote_port,
						snext->ts_state);
				snext++;
				ct--;
			}
		
			heap_free(sbuf);
		}
		break;
*/

	case 't':
		{
			struct thread *tnext, *tbuf;
						
			tbuf = heap_alloc(16 * sizeof(struct thread));
			p += sprintf(p, "\r\nThread list\r\n");
			
			ct = thread_dump_stats(tbuf, 16);
			if (ct == 16) {
				p += sprintf(p, "Note: List at maximum...\r\n");
			}
			
			tnext = tbuf;
			while (ct) {
				p += sprintf(p, "addr:%x, curpri:%x, state:%x, ctxsw:%u\r\n",
						(addr_t)tnext->t_next, tnext->t_context.c_priority,
						tnext->t_context.c_state, tnext->t_context.c_context_switches);
				tnext++;
				ct--;
			}
		
			heap_free(tbuf);
		}
                break;
					
/*
	case 'u':
		{
			struct udp_client *unext, *ubuf;
			
			ubuf = heap_alloc(16 * sizeof(struct udp_client));
			p += sprintf(p, "\r\nUDP socket details:\r\n");
	
			ct = udp_dump_stats(NULL, ubuf, 16);
			if (ct == 16) {
				p += sprintf(p, "Note: List at maximum...\r\n");
			}
			
			unext = ubuf;
			while (ct) {
				p += sprintf(p, "addr:%x, local:%lx:%x\r\n",
						(addr_t)unext->uc_next, unext->uc_addr, unext->uc_port);
				unext++;
				ct--;
			}
		
			heap_free(ubuf);
		}
		break;
*/
				
	default:
		membuf_deref(obuf);
		return;
	}
	
	/*
	 * Package up what we've received and send it back.
	 */		
	nbr = netbuf_alloc();
	nbr->nb_application_membuf = obuf;
	nbr->nb_application = obuf;
        nbr->nb_application_size = strlen(obuf);
	
	ts->ts_send(ts, nbr);

	netbuf_deref(nbr);
}

/*
 * test6_close()
 */
void test6_close(void *clnt)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
	tcpi = ts->ts_instance;
	
	tcpi->ti_client_detach(tcpi, ts);
}

/*
 * test6_recv()
 *	A target for TCP messages.
 */
void test6_recv(void *clnt, struct netbuf *nb)
{
        struct tcp_client *tc;
        struct tcp_server *ts;
	u16_t ct;
	u8_t *cmd;

	tc = (struct tcp_client *)clnt;
	ts = tc->tc_server;
                	
	/*
	 * Process the commands.
	 */
	cmd = nb->nb_application;
	for (ct = nb->nb_application_size; ct; ct--) {
		if (*cmd == 'q') {
			struct tcp_instance *tcpi;

			tcpi = (struct tcp_instance *)ts->ts_instance;
			tcpi->ti_client_detach(tcpi, ts);
			break;
		}
		test6_body(ts, *cmd++);
	}
}

/*
 * test6_connect()
 */
void test6_connect(void *clnt, void *srv)
{
	struct tcp_client *tc;
	struct tcp_instance *tcpi;
	struct tcp_server *ts;

	tcpi = ((struct tcp_server *)srv)->ts_instance;
	
	tc = tcp_client_alloc();
	tc->tc_recv = test6_recv;
	tc->tc_close = test6_close;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
	
	ts->ts_accept(ts, srv);
}

/*
 * test6_init()
 */
void test6_init(struct tcp_instance *tcpi)
{
	struct tcp_client *tc;
	struct tcp_server *ts;
	
	spinlock_init(&test6_lock, 0x50);
	        		
	/*
	 * Hook TCP requests for our server socket.
	 */
	tc = tcp_client_alloc();
	tc->tc_recv = test6_recv;
	tc->tc_connect = test6_connect;
	ts = tcpi->ti_client_attach(tcpi, tc);
	tcp_client_deref(tc);
		
	ts->ts_listen(ts, 1237);
}

/*
 * init()
 */
void init(void)
{
	u8_t buf[8];
	struct uart_instance *uarti;
	struct ip_datalink_instance *serialipi;
	struct ppp_ahdlc_instance *pppai;
	struct ppp_instance *pppi;
	struct ip_instance *ipi1;
	struct udp_instance *udpi1;
	struct tcp_instance *tcpi1;
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
	struct ethdev_instance *edi;
	struct ethernet_instance *ethi;
	struct ip_datalink_instance *iei;
	struct ip_instance *ipi2;
	struct udp_instance *udpi2;
	struct tcp_instance *tcpi2;
#endif

	heap_add((addr_t)(&__bss_end), (addr_t)(RAMEND - (IDLE_SIZE - 1)) - (addr_t)(&__bss_end));
	heap_add(XRAMSTART, 0x8000 - XRAMSTART);
	
	membuf_init();
	timer_init();
	oneshot_init();
	netbuf_init();

	uarti = uart_instance_alloc();
#if 0
	serialipi = slip_instance_alloc(uarti);
#endif
	pppai = ppp_ahdlc_instance_alloc(uarti);
	pppi = ppp_instance_alloc(pppai);
	serialipi = ppp_ip_instance_alloc(pppi);
	ipi1 = ip_instance_alloc(serialipi, 0xbe010102);
	udpi1 = udp_instance_alloc(ipi1);
	tcpi1 = tcp_instance_alloc(ipi1);

#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
#if defined(DJH300)
	edi = c509_instance_alloc();
#elif defined(DJHNE)
	edi = ne2000_instance_alloc();
#elif defined(PCB520)
	edi = smc91c96_instance_alloc();
#endif
	ethi = ethernet_instance_alloc(edi);
	iei = ethernet_ip_instance_alloc(ethi);
	ipi2 = ip_instance_alloc(iei, 0xc0a800cd);
	udpi2 = udp_instance_alloc(ipi2);
	tcpi2 = tcp_instance_alloc(ipi2);
#endif

#if defined(STK300) || defined(DJHNE) || defined(DJH300)
	bbi2c_init();
#endif

	/*
	 * Quick I2C bus test.
	 */
	if (bbi2c_read(ADDR_8583, 0x00, buf, 8) == 0) {
		u8_t i;
//		debug_print_pstr("\fi2c success\n\r");
		for (i = 0; i < 8; i++) {
			debug_print8(buf[i]);
		}
	} else {
//		debug_print_pstr("\fi2c failure");
	}
		
	/*
	 * Create the worker thread for test 1.
	 */
//	thread_create(test1_thread, ipi, 0x100, 0xf8);
	
	/*
	 * Create the worker thread for test 2.
	 */
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
	test2_init(udpi2);
#endif

	/*
	 * Create the worker thread for test 3.
	 */
	test3_init(tcpi1);
	
	/*
	 * Create the worker thread for test 4.
	 */
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
//	thread_create(test4_thread, tcpi2, 0x100, 0xf8);
#endif

	/*
	 * Create the basic setup and the first worker thread for test 5.
	 */
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
	test5_init(tcpi2);
#else
	test5_init(tcpi1);
#endif
	 	
	/*
	 * Create the basic setup and the first worker thread for test 6.
	 */
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
	test6_init(tcpi2);
#else
	test6_init(tcpi1);
#endif

	/*
	 * Now tidy up!
	 */
#if defined(DJH300) || defined(DJHNE) || defined(PCB520)
	tcp_instance_deref(tcpi2);
	udp_instance_deref(udpi2);
	ip_instance_deref(ipi2);
	ip_datalink_instance_deref(iei);
	ethernet_instance_deref(ethi);
	ethdev_instance_deref(edi);
#endif

	tcp_instance_deref(tcpi1);
	udp_instance_deref(udpi1);
	ip_instance_deref(ipi1);
	ppp_instance_deref(pppi);
	ppp_ahdlc_instance_deref(pppai);
	ip_datalink_instance_deref(serialipi);
	uart_instance_deref(uarti);
}

/*
 * main()
 */
int main(void)
{
	out8(DDRA, 0xff);
	out8(DDRB, 0xff);
	out8(DDRE, 0x20);
	out8(DDRD, 0xff);
	out8(EICR, 0x00);
	
	/*
	 * Provide debugging capabilities before we attempt anything else.
	 */		
	debug_init();
	
#if defined(DJH300) || defined(DJHNE)
	/*
	 * Pin 5 of port E is used to reset the ISA bus - we give it a little
	 * blip for about 1 us and then let it go.  Keeps things sane!
	 */
	out8(PORTE, 0x20);
	out8(PORTE, 0x20);
	out8(PORTE, 0x20);
	out8(PORTE, 0x20);
	out8(PORTE, 0x00);

	/*
	 * Debug initialization specific to this target setup.
	 */
	out8(DDRD, 0xfa);
	out8(EIMSK, in8(EIMSK) | BV(INT2));
#endif
	
	/*
	 * Start the threading system.  Note that we don't exit here, we never
	 * return from the thread initialization.
	 */
	thread_init((void *)(RAMEND - (IDLE_SIZE - 1)), IDLE_SIZE);
}
