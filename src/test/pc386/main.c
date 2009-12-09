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
#include "pic.h"
#include "timer.h"
#include "oneshot.h"
#include "uart.h"
#include "ethdev.h"
#include "3c509.h"
#include "ne2000.h"
#include "i82595.h"
#include "ppp_ahdlc.h"
#include "ip_datalink.h"
#include "ppp.h"
#include "ethernet.h"
#include "ppp_ip.h"
#include "ethernet_ip.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"

/*
 * A gcc-defined symbol that signifies the end of the bss section.
 */
extern void *__bss_end;
u32_t ext_ram;

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

/*
 * test1_thread()
 */
void test1_thread(void *arg) __attribute__ ((noreturn));
void test1_thread(void *arg)
{
	u32_t i;

	while (1) {
		debug_print_pstr("\f1:mem:");
		debug_print_addr(heap_get_free());
		debug_print_pstr("\n\rarg:");
		debug_print_addr((addr_t)arg);
	
		for (i = 0; i < 10000000; i++);
	}
}

/*
 * test2_thread()
 */
void test2_thread(void *arg) __attribute__ ((noreturn));
void test2_thread(void *arg)
{
	u32_t i;

	while (1) {
		debug_print_pstr("\f2:thread");
		debug_print_pstr("\n\rarg:");
		debug_print_addr((addr_t)arg);
	
		for (i = 0; i < 7000000; i++);
	}
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
long test5_hits = 0L;

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
char test6_cmd_buf[16];
u8_t test6_cmd_idx = 0;

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
			long avg[3];
                        extern u8_t ldavg_runnable;
                        u8_t r;

                        isr_disable();
                        r = ldavg_runnable - 1;
                        isr_enable();
			
			timer_dump_stats((u32_t *)avg, 3);
                        						
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
						(addr_t)onext->os_next, (long)onext->os_ticks_left);
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
	struct uart_instance *uarti;
	struct ppp_ahdlc_instance *pppai;
	struct ppp_instance *pppi;
	struct ip_datalink_instance *pii;
	struct ip_instance *ipi1;
	struct udp_instance *udpi1;
	struct tcp_instance *tcpi1;
#if defined(DJHPC) || defined(DJHNE) || defined(DJHEEP)
	struct ethdev_instance *edi;
	struct ethernet_instance *ethi;
	struct ip_datalink_instance *eii;
	struct ip_instance *ipi2;
	struct udp_instance *udpi2;
	struct tcp_instance *tcpi2;
#endif
	
	heap_add((addr_t)(&__bss_end), (addr_t)0x00080000 - (addr_t)(&__bss_end));
	heap_add((addr_t)0x10000000, (ext_ram << 10));
	
	membuf_init();
	timer_init();
	oneshot_init();
	netbuf_init();
	
	uarti = uart_instance_alloc();
	pppai = ppp_ahdlc_instance_alloc(uarti);
	pppi = ppp_instance_alloc(pppai);
	pii = ppp_ip_instance_alloc(pppi);
	ipi1 = ip_instance_alloc(pii, 0xbe010102);
	udpi1 = udp_instance_alloc(ipi1);
	tcpi1 = tcp_instance_alloc(ipi1);

#if defined(DJHPC) || defined(DJHNE) || defined(DJHEEP)
#if defined(DJHPC)
	edi = c509_instance_alloc();
#elif defined(DJHNE)
	edi = ne2000_instance_alloc();
#elif defined(DJHEEP)
	edi = i82595_instance_alloc();
#endif
	ethi = ethernet_instance_alloc(edi);
	eii = ethernet_ip_instance_alloc(ethi);
	ipi2 = ip_instance_alloc(eii, 0xc0a800cd);
	udpi2 = udp_instance_alloc(ipi2);
	tcpi2 = tcp_instance_alloc(ipi2);
#endif

	/*
	 * Create the test threads
	 */
//	thread_create(test1_thread, (void *)0x10101010, 0x1000, 0xf8);
//	thread_create(test2_thread, (void *)0x02020202, 0x1000, 0xf8);
	
	/*
	 * Create the worker thread for test 3.
	 */
	test3_init(tcpi1);
	
	/*
	 * Create the worker thread for test 4.
	 */
	thread_create(test4_thread, tcpi1, 0x1000, 0xf8);

	/*
	 * Create the basic setup for test 5.
	 */
#if defined(DJHPC) || defined(DJHNE) || defined(DJHEEP)
	test5_init(tcpi2);
#else
	test5_init(tcpi1);
#endif
	 	
	/*
	 * Create the basic setup for test 6.
	 */
#if defined(DJHPC) || defined(DJHNE) || defined(DJHEEP)
	test6_init(tcpi2);
#else
	test6_init(tcpi1);
#endif

	/*
	 * Now tidy up!
	 */
#if defined(DJHPC) || defined(DJHNE) || defined(DJHEEP)
	tcp_instance_deref(tcpi2);
	udp_instance_deref(udpi2);
	ip_instance_deref(ipi2);
	ip_datalink_instance_deref(eii);
	ethernet_instance_deref(ethi);
	ethdev_instance_deref(edi);
#endif

	tcp_instance_deref(tcpi1);
	udp_instance_deref(udpi1);
	ip_instance_deref(ipi1);
	ip_datalink_instance_deref(pii);
	ppp_instance_deref(pppi);
	ppp_ahdlc_instance_deref(pppai);
	uart_instance_deref(uarti);
}

/*
 * I/O ports used for the keyboard controller
 */
#define KBD_DATA 0x60
#define KBD_CTRL 0x61
#define KBD_STATUS 0x64

/*
 * Bits in KBD_CTRL
 */
#define NMI_RAM_PARITY 0x80
#define NMI_IO_PARITY 0x40
#define NMI_ENABLE_IO_PARITY 0x8
#define NMI_ENABLE_RAM_PARITY 0x4

/*
 * Bits in KBD_STATUS
 */
#define KBD_BUSY 0x2
#define KBD_WRITE 0xd1

/*
 * Command for KBD_DATA to turn on high addresses
 */
#define KBD_ENABLE_A20 0xdf

/*
 * enable_a20()
 *	Enable the A20 gate line
 *
 * The A20 gate allows the CPU to access address above 1 MByte (ie where
 * address line A20 becomes used).  In an inspired piece of design, someone
 * went and placed the functionality for this gate operation within the
 * PC/AT's keyboard controller - nice one :-(
 *
 * Most chipsets since about 1992 implement a fast emulated version of this
 * instead of passing the request to the keyboard controller, but we still
 * need to do things as if we were talking to the original device
 */
static void enable_a20(void)
{
	/*
	 * Wait 'til we're not busy then flag that we want to write
	 */
	while (in8(KBD_STATUS) & KBD_BUSY);
	out8(KBD_STATUS, KBD_WRITE);

	/*
	 * Wait 'til we're not busy and then enable the A20 gate
	 */
	while (in8(KBD_STATUS) & KBD_BUSY);
	out8(KBD_DATA, KBD_ENABLE_A20);
}

/*
 * enable_nmi()
 *	Enable the I/O and RAM parity NMI errors
 */
static void enable_nmi(void)
{
	u8_t d;
	
	d = in8(KBD_CTRL);
	out8(KBD_CTRL, d & ~(NMI_ENABLE_IO_PARITY | NMI_ENABLE_RAM_PARITY));
}

/*
 * main()
 */
int main(void)
{
	u32_t nextpg;
	u32_t *pgdir;
	u32_t *pgtbl;
	int i;

	/*
	 * Provide debugging capabilities before we attempt anything else.
	 */		
	debug_init();

	cpu_init(0x0009a000);

	enable_a20();
	enable_nmi();
		
	/*
	 * The first thing to do is sort out our page mapping.  We need to
	 * allocate page table space for all of the unused pages of memory so
	 * that we can make them available to the heap.
	 *
	 * We're lazy for now and don't bother mapping anything under 1 MByte!
	 * We find out how much RAM is about 1 MByte by reading the CMOS RAM.
	 */
	out8(0x70, 0x17);
	ext_ram = (u32_t)in8(0x71);
	out8(0x70, 0x18);
	ext_ram |= ((u32_t)in8(0x71) << 8);

	/*
	 * Page directory is at a fixed location!  Create entries within it for
	 * up to 64 MBytes of RAM mapped at 0x10000000.  This is lazy and sub
	 * -optimal, but hey, this is a PC not an AVR :-)  Note we use the 64k
	 * from 0x00080000 for the page table entries (above the kernel but
	 * still below 640k).
	 */
	pgdir = (u32_t *)0x0009f100;
	nextpg = 0x00080000;
	for (i = 0; i < 16; i++) {
		*pgdir++ = (nextpg | PTE_P | PTE_RW);
		nextpg += 0x1000;
	}

	/*
	 * Fill in the page table entries for the external RAM.
	 */
	pgtbl = (u32_t *)0x00080000;
	nextpg = 0x00100000;
	for (i = 0; i < (ext_ram >> 2); i++) {
		*pgtbl++ = (nextpg | PTE_P | PTE_RW);
		nextpg += 0x1000;
	}
		
	/*
	 * Setup interrupt vectoring.
	 */
	pic_init();
	
	/*
	 * Start the threading system.  Note that we don't exit here, we never
	 * return from the thread initialization.
	 */
	thread_init((void *)(0x0009c000), 0x1000);
}
