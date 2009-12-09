/*
 * tcp.h
 *	Transmission Control Protocol (TCP/IP) header.
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
 * TCP header layout.
 */
struct tcp_header {
	u16_t th_src_port;
	u16_t th_dest_port;
	u32_t th_sequence;
	u32_t th_ack;
	u8_t th_res1 : 4,
		th_data_offs : 4;
	u8_t th_ctrl_flags;
	u16_t th_window;
	u16_t th_csum;
	u16_t th_urgent_ptr;
};

/*
 * TCP control flags.
 */
#define TCF_FIN 0x01
#define TCF_SYN 0x02
#define TCF_RST 0x04
#define TCF_PSH 0x08
#define TCF_ACK 0x10
#define TCF_URG 0x20

/*
 * TCP connection states.
 */
#define TCS_CLOSED 0
#define TCS_LISTEN 1
#define TCS_SYN_SENT 2
#define TCS_SYN_RECEIVED 3
#define TCS_ESTABLISHED 4
#define TCS_FIN_WAIT1 5
#define TCS_FIN_WAIT2 6
#define TCS_CLOSE_WAIT 7
#define TCS_CLOSING 8
#define TCS_LAST_ACK 9
#define TCS_TIME_WAIT 10

/*
 * TCP socket structure.
 *
 * This is used to allow a client thread to chain packets for a specific TCP
 * socket.
 */
struct tcp_socket {
	u8_t ts_state;			/* Socket state (machine) number */
	u16_t ts_local_port;		/* Local port number */
	u16_t ts_remote_port;		/* Remote port number */
	u32_t ts_remote_addr;		/* Remote IP address */
	u32_t ts_snd_nxt;		/* SND-NXT from RFC793 */
	u32_t ts_snd_una;		/* SND-UNA from RFC793 */
	u32_t ts_rcv_nxt;		/* RCV-NXT from RFC793 */
	u32_t ts_remote_mss;		/* MSS for remote socket */
	u32_t ts_send_jiffies;		/* Jiffy count at which we sent the last segment */
	struct netbuf *ts_awaiting_send;
	struct netbuf *ts_awaiting_ack;
	struct oneshot *ts_wait_timer;  /* Timer used for handling misc waits */
	u16_t ts_retransmits;
	struct netbuf *ts_deferred_ack;
	struct oneshot *ts_deferred_ack_timer;
					/* Timer used for handling deferred ACKs */
	u8_t ts_send_flags;		/* Flags used during transmissions - see below */
	u32_t ts_rto_next;		/* Next round-trip timeout */
	u32_t ts_srtt_avg;		/* Scaled round-trip timer average */
	u32_t ts_srtt_var;		/* Scaled round-trip timer variance */
	struct tcp_socket *ts_awaiting_accept;
	struct tcp_socket *ts_next;
	struct condvar ts_wait_cv;
	struct tcp_server *ts_server;
	struct tcp_client *ts_client;
	struct tcp_instance *ts_instance;
};

/*
 * TCP send flags.
 */
#define TCP_SND_DEFERRED_FIN 0x01
#define TCP_SND_RETRANS_TIMING 0x02
#define TCP_SND_ACKED 0x04
#define TCP_SND_DEFERRED_ACK 0x08

/*
 * TCP server structure.
 */
struct tcp_server {
	struct tcp_instance *ts_instance;
					/* The instance of this service */
	struct tcp_client *ts_client;
	struct tcp_socket *ts_socket;
	void (*ts_listen)(void *srv, u16_t port);
	s8_t (*ts_accept)(void *srv, void *listensrv);
	void (*ts_reject)(void *srv, void *listensrv);
	s8_t (*ts_connect)(void *srv, u32_t addr, u16_t port);
	void (*ts_send)(void *srv, struct netbuf *nb);
					/* Function used when we send packet data */
	void (*ts_close)(void *srv);	/* Function to close the socket */
};

/*
 * TCP client structure.
 */
struct tcp_client {
	void *tc_instance;		/* The instance of our client */
	struct tcp_client *tc_next;
	struct tcp_server *tc_server;	/* Reference to the server structure given to our client */
	struct tcp_socket *tc_socket;
	void (*tc_recv)(void *clnt, struct netbuf *);
					/* Function called when data is received */
	void (*tc_connect)(void *clnt, void *srv);
					/* Function called when a connection is requested on a listening socket */
	void (*tc_close)(void *clnt);	/* Function called when our socket has been closed */
};

/*
 * Instance of the TCP service.
 */
struct tcp_instance {
	struct lock ti_lock;
	struct tcp_client *ti_client_list;
	struct tcp_socket *ti_socks;
	u16_t ti_last_local_port;
	struct ip_server *ti_server;
	struct tcp_server *(*ti_client_attach)(struct tcp_instance *ti, struct tcp_client *tc);
	void (*ti_client_detach)(struct tcp_instance *ti, struct tcp_server *ts);
};

/*
 * Function prototypes.
 */
extern int tcp_dump_stats(struct tcp_instance *ti, struct tcp_socket *sbuf, int max);
extern struct tcp_client *tcp_client_alloc(void);
extern struct tcp_instance *tcp_instance_alloc(struct ip_instance *ii);

/*
 * tcp_socket_ref()
 */
extern inline void tcp_socket_ref(struct tcp_socket *sock)
{
	membuf_ref(sock);
}

/*
 * tcp_socket_deref()
 */
extern inline ref_t tcp_socket_deref(struct tcp_socket *sock)
{
	return membuf_deref(sock);
}

/*
 * tcp_socket_get_refs()
 */
extern inline ref_t tcp_socket_get_refs(struct tcp_socket *sock)
{
	return membuf_get_refs(sock);
}

/*
 * tcp_client_ref()
 */
extern inline void tcp_client_ref(struct tcp_client *tc)
{
	membuf_ref(tc);
}

/*
 * tcp_client_deref()
 */
extern inline ref_t tcp_client_deref(struct tcp_client *tc)
{
	return membuf_deref(tc);
}

/*
 * tcp_server_ref()
 */
extern inline void tcp_server_ref(struct tcp_server *ts)
{
	membuf_ref(ts);
}

/*
 * tcp_server_deref()
 */
extern inline ref_t tcp_server_deref(struct tcp_server *ts)
{
	return membuf_deref(ts);
}

/*
 * tcp_instance_ref()
 */
extern inline void tcp_instance_ref(struct tcp_instance *ti)
{
	membuf_ref(ti);
}

/*
 * tcp_instance_deref()
 */
extern inline ref_t tcp_instance_deref(struct tcp_instance *ti)
{
	return membuf_deref(ti);
}
