/*
 * tcp.c
 *	Transmission Control Protocol (TCP/IP) support.
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
 * This implementation has been written from scratch based upon the information
 * in numerous RFCs, but particularly RFC793 and RFC1122.  As this code is
 * intended to be supported on small (8 bit) targets some elements of the
 * design have been done for space or simplicity rather than for best network
 * latency or throughput.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "condvar.h"
#include "membuf.h"
#include "netbuf.h"
#include "ipcsum.h"
#include "timer.h"
#include "oneshot.h"
#include "ip.h"
#include "tcp.h"

#include "ip_datalink.h"
#include "ethernet_ip.h"

/*
 * Prototypes.
 */
void tcp_tick_deferred_ack(void *sock);
void tcp_tick_retransmit(void *sock);
void tcp_tick_time_wait(void *sock);
void tcp_send_sequence(struct tcp_instance *ti, struct tcp_socket *sock, struct netbuf *nb);
void tcp_build_and_send_sequence(struct tcp_instance *ti, struct tcp_socket *sock, u8_t ctrl_flags);
void tcp_send_netbuf_now(struct tcp_instance *ti, struct tcp_socket *sock, struct netbuf *nb);
struct tcp_client *tcp_client_alloc(void);
struct tcp_server *tcp_client_attach(struct tcp_instance *ti, struct tcp_client *tc);

/*
 * __tcp_socket_free()
 */
void __tcp_socket_free(void *sock)
{
	struct tcp_socket *ts = (struct tcp_socket *)sock;

	oneshot_deref(ts->ts_wait_timer);
	oneshot_deref(ts->ts_deferred_ack_timer);
}

/*
 * tcp_socket_alloc()
 */
struct tcp_socket *tcp_socket_alloc(struct tcp_instance *ti)
{
	struct tcp_socket *ts;
	
	ts = (struct tcp_socket *)membuf_alloc(sizeof(struct tcp_socket), __tcp_socket_free);
	ts->ts_snd_nxt = 0x00a8393a;
	ts->ts_snd_una = ts->ts_snd_nxt;
	ts->ts_next = NULL;
	ts->ts_state = TCS_CLOSED;
	ts->ts_awaiting_send = NULL;
	ts->ts_awaiting_accept = NULL;
	ts->ts_rto_next = 3 * TICK_RATE;
	ts->ts_srtt_avg = 0;
	ts->ts_srtt_var = 3 * TICK_RATE;
	ts->ts_remote_mss = 536;
	ts->ts_send_flags = 0;

	ts->ts_deferred_ack_timer = oneshot_alloc();
	ts->ts_deferred_ack_timer->os_callback = tcp_tick_deferred_ack;
	ts->ts_deferred_ack_timer->os_arg = ts;

	ts->ts_wait_timer = oneshot_alloc();
	ts->ts_wait_timer->os_callback = tcp_tick_retransmit;
	ts->ts_wait_timer->os_arg = ts;

	ts->ts_instance = ti;
	ts->ts_client = NULL;
	ts->ts_server = NULL;

	condvar_init(&ts->ts_wait_cv, &ti->ti_lock);
        	
	return ts;
}

/*
 * tcp_socket_attach()
 *
 * It is assumed that the tcp socket lock is held on entry to this function.
 */
void tcp_socket_attach(struct tcp_instance *ti, struct tcp_socket *sock)
{
        struct tcp_socket *ts;
	
	ts = ti->ti_socks;
	
	/*
	 * We loop until we've either run out of sockets to check, or, until we
	 * find a socket that clashes.  A clash is either trying to attach a
	 * passive socket where one already exists, or a connected socket where
	 * the same connection already exists.
	 */
	while (ts) {
		if (ts->ts_local_port == sock->ts_local_port) {
			if (sock->ts_state == TCS_LISTEN) {
				if (ts->ts_state == TCS_LISTEN) {
					break;
				}
			} else {
				if ((ts->ts_state != TCS_LISTEN)
						&& (ts->ts_remote_addr == sock->ts_remote_addr)
						&& (ts->ts_remote_port == sock->ts_remote_port)) {
					break;
				}
			}
		}
		ts = ts->ts_next;
	}

	if (!ts) {
		sock->ts_next = ti->ti_socks;
		ti->ti_socks = sock;
		tcp_socket_ref(sock);
	}
}

/*
 * tcp_socket_detach()
 *
 * It is assumed that the tcp socket lock is held on entry to this function.
 */
void tcp_socket_detach(struct tcp_instance *ti, struct tcp_socket *sock)
{
        struct tcp_socket *ts;
        struct tcp_socket **tsprev;

	ts = ti->ti_socks;
	tsprev = &ti->ti_socks;
	while (ts) {
		if (ts == sock) {
			*tsprev = ts->ts_next;
			tcp_socket_deref(sock);
			break;
		}
	
		tsprev = &ts->ts_next;
		ts = ts->ts_next;
	}
}

/*
 * build_netbuf()
 *	Build a TCP netbuf.
 *
 * The assumption here is that the netbuf structure has already been allocated
 * pointers to the network and transport layers.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 * We also assume that the instance is locked when we're called.
 */
static void build_netbuf(struct tcp_instance *ti, struct tcp_socket *sock, u8_t ctrl_flags, struct netbuf *nb)
{
	struct tcp_header *tch;
        u8_t sz;
        	
	/*
	 * Are we building a SYN packet?  If we are then we extend the TCP
	 * header with space for an MSS-size option.
	 */
	sz = sizeof(struct tcp_header);
	if (ctrl_flags & TCF_SYN) {
		sz += 4;
	}

	nb->nb_transport_membuf = membuf_alloc(sz, NULL);
	nb->nb_transport = nb->nb_transport_membuf;
	nb->nb_transport_size = sz;
	
	tch = (struct tcp_header *)nb->nb_transport;
	tch->th_dest_port = hton16(sock->ts_remote_port);
	tch->th_src_port = hton16(sock->ts_local_port);
	tch->th_sequence = hton32(sock->ts_snd_nxt);
	tch->th_data_offs = sz / 4;
	tch->th_ctrl_flags = ctrl_flags;
	tch->th_window = hton16(2048);
	tch->th_urgent_ptr = 0;

	if (ctrl_flags & TCF_SYN) {
	        u8_t *p;
		
		/*
		 * MSS size option.  Look up the MRU for our datalink layer and
		 * advertise our MSS as being the largest that is supportable.
		 */
		p = (u8_t *)(tch + 1);
		*p++ = 0x02;
		*p++ = 4;
		*((u16_t *)p) = hton16(ti->ti_server->is_instance->ii_server->ids_instance->idi_mru
					- sizeof(struct tcp_header) - sizeof(struct ip_header));
	}
}

/*
 * tcp_send_close()
 */
void tcp_send_close(struct tcp_instance *ti, struct tcp_socket *sock)
{
	struct netbuf *nb;

	/*
	 * Build our close request's packet's buffers.
	 */	
	nb = netbuf_alloc();
        		
	spinlock_lock(&ti->ti_lock);

	/*
	 * If we had a previously deferred ACK then we can eliminate it now
	 * because we're going to send the FIN, ACK instead.
	 */
	if (sock->ts_send_flags & TCP_SND_DEFERRED_ACK) {
		sock->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
		oneshot_detach(sock->ts_deferred_ack_timer);
		tcp_socket_deref(sock);
		netbuf_deref(sock->ts_deferred_ack);
	}
	
	/*
	 * Build and send our FIN, ACK packet.
	 */
	build_netbuf(ti, sock, TCF_ACK | TCF_FIN, nb);

	sock->ts_snd_nxt++;
	sock->ts_awaiting_ack = netbuf_clone(nb);
	sock->ts_wait_timer->os_ticks_left = sock->ts_rto_next;
	sock->ts_retransmits = 0;
        sock->ts_send_jiffies = timer_get_jiffies();
        sock->ts_send_flags = TCP_SND_RETRANS_TIMING;
	tcp_socket_ref(sock);
	oneshot_attach(sock->ts_wait_timer);
		
	spinlock_unlock(&ti->ti_lock);
	
	tcp_send_sequence(ti, sock, nb);

	netbuf_deref(nb);
}

/*
 * tcp_tick_deferred_ack()
 *	Callback function for the deferred ACK timer.
 */
void tcp_tick_deferred_ack(void *sock)
{
        struct tcp_socket *ts = (struct tcp_socket *)sock;
	struct tcp_instance *ti;
	
	ti = ts->ts_instance;

	spinlock_lock(&ti->ti_lock);

	/*
	 * There's a possibility that we can race between our timer callback
	 * and our TCP's other code.  This means that in theory we might not
	 * need to do deferred ACK now - check first!
	 */
	if ((tcp_socket_deref(ts) > 0) && (ts->ts_send_flags & TCP_SND_DEFERRED_ACK)) {
	        struct netbuf *nb;
		
		if (DEBUG) {
			if (ts->ts_state == TCS_CLOSED) {
				debug_stop();
				debug_print_pstr("\ftcp tick_d_a in closed state");
				while (1);
			}
		}
		
		ts->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
		nb = ts->ts_deferred_ack;
		spinlock_unlock(&ti->ti_lock);
	
		tcp_send_sequence(ti, ts, nb);
	
		spinlock_lock(&ti->ti_lock);
		netbuf_deref(nb);
	}
	
	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_tick_retransmit()
 *	Callback function for the retransmission timer.
 */
void tcp_tick_retransmit(void *sock)
{
        struct tcp_socket *ts = (struct tcp_socket *)sock;
	struct tcp_instance *ti;
	
	ti = ts->ts_instance;

	spinlock_lock(&ti->ti_lock);
		
        /*
         * There's a possibility that we can race between our timer callback
         * and our TCP's receive packet code.  This means that in theory we
         * might not need to do this retransmit now - check first!
         */
	if ((tcp_socket_deref(ts) > 0) && (ts->ts_send_flags & TCP_SND_RETRANS_TIMING)) {
		if (DEBUG) {
			if (ts->ts_state == TCS_CLOSED) {
				debug_stop();
				debug_print_pstr("\ftcp tick_retx in closed state");
				while (1);
			}
		}
		
		/*
		 * When we retransmit this segment we double the timeout period that
		 * broughts us here (exponential backoff).  Note that we don't
		 * retransmit if the buffer is already waiting to be transmitted
		 * (as indicated by a ref count > 1).
		 */
		ts->ts_wait_timer->os_ticks_left = ts->ts_rto_next << ts->ts_retransmits;
		if (ts->ts_wait_timer->os_ticks_left > (240 * TICK_RATE)) {
			ts->ts_wait_timer->os_ticks_left = 240 * TICK_RATE;
		}
		tcp_socket_ref(ts);
		oneshot_attach(ts->ts_wait_timer);
						
		if (netbuf_get_refs(ts->ts_awaiting_ack) == 1) {
			struct netbuf *nb;
			
			/*
			 * If we had a previously deferred ACK then we can eliminate it now
			 * because we're sending a segment and an ACK now.
			 */
			if (ts->ts_send_flags & TCP_SND_DEFERRED_ACK) {
				ts->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
				oneshot_detach(ts->ts_deferred_ack_timer);
				tcp_socket_deref(ts);
				netbuf_deref(ts->ts_deferred_ack);
			}
	
			nb = netbuf_clone(ts->ts_awaiting_ack);
			ts->ts_retransmits++;
			spinlock_unlock(&ti->ti_lock);
			tcp_send_sequence(ti, ts, nb);
			netbuf_deref(nb);
			return;
		}
        }
	
	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_tick_time_wait()
 *	Callback function for the TIME-WAIT timer.
 */
void tcp_tick_time_wait(void *sock)
{
	struct tcp_socket *ts = (struct tcp_socket *)sock;
	struct tcp_instance *ti;
	
	ti = ts->ts_instance;

	spinlock_lock(&ti->ti_lock);
	
	ts->ts_state = TCS_CLOSED;
	tcp_socket_detach(ti, ts);
	
	spinlock_unlock(&ti->ti_lock);
}

/*
 * close_socket()
 *	Move a TCP socket to the closed state.
 */
static void close_socket(struct tcp_instance *ti, struct tcp_socket *ts)
{
	if (ts->ts_send_flags & TCP_SND_RETRANS_TIMING) {
		struct netbuf *nb;
		
		ts->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
		oneshot_detach(ts->ts_wait_timer);
		tcp_socket_deref(ts);
		netbuf_deref(ts->ts_awaiting_ack);

		while (ts->ts_awaiting_send) {
			nb = ts->ts_awaiting_send;
			ts->ts_awaiting_send = nb->nb_next;
			
			netbuf_deref(nb);
		}
	}
	ts->ts_state = TCS_CLOSED;

	tcp_socket_detach(ti, ts);
				
	condvar_signal(&ts->ts_wait_cv);
}

/*
 * send_reset_close_socket()
 *	Send a reset to a TCP socket and then move it to the closed state.
 */
static void send_reset_close_socket(struct tcp_instance *ti, struct tcp_socket *ts)
{
	if (ts->ts_send_flags & TCP_SND_RETRANS_TIMING) {
		struct netbuf *nb;
		
		ts->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
		oneshot_detach(ts->ts_wait_timer);
		tcp_socket_deref(ts);
		netbuf_deref(ts->ts_awaiting_ack);

		while (ts->ts_awaiting_send) {
			nb = ts->ts_awaiting_send;
			ts->ts_awaiting_send = nb->nb_next;
			
			netbuf_deref(nb);
		}
	}
	ts->ts_state = TCS_CLOSED;

	tcp_build_and_send_sequence(ti, ts, TCF_RST);

	tcp_socket_detach(ti, ts);
				
	condvar_signal(&ts->ts_wait_cv);
}

/*
 * calc_round_trip()
 *	Work out the round trip time for the last packet we sent.
 *
 * This is *VERY* important and much cited in various RFCs and conference
 * papers.  We implement Van Jacobson's round-trip variance estimator
 * (originally published in "Congestion Avoidance and Control" - November 1988)
 * and use Karn's algorithm to avoid problems caused by retransmissions
 * (originally published in "Improving Round-Trip Time Estimates in Reliable
 * Transport Protocols - August 1987).
 *
 * Note RFC1122 mandates that these algorithms MUST be used.
 */
static void calc_round_trip(struct tcp_socket *ts)
{
	s32_t ticks;
	
	if (ts->ts_send_flags & TCP_SND_ACKED) {
		return;
	}
	
	ticks = (s32_t)(timer_get_jiffies() - ts->ts_send_jiffies);

	if (ts->ts_retransmits) {
		/*
		 * This is Karn's algorithm - don't use retransmitted packets
		 * to calculate new RTTs,  instead keep the last backed-off
		 * RTO value instead.
		 */
		ts->ts_rto_next <<= ts->ts_retransmits;
	} else {
		if (ticks == 0) {
			ticks = 1;
		}

		/*
		 * Here's the Van Jacobson code - almost directly extracted
		 * from appendix A of his paper.
		 */
		if (ts->ts_srtt_avg == 0) {
			ts->ts_srtt_var = ticks << 2;
			ts->ts_srtt_avg = ts->ts_srtt_var << 1;
		} else {
			ticks -= (ts->ts_srtt_avg >> 3);
			ts->ts_srtt_avg += ticks;
			if (ticks < 0) {
				ticks = -ticks;
			}
			ticks -= (ts->ts_srtt_var >> 2);
			ts->ts_srtt_var += ticks;
		}
				
		ts->ts_rto_next = (ts->ts_srtt_avg >> 3) + ts->ts_srtt_var;
	}

	/*
	 * Don't allow a timeout that's greater than 2xMSL!
	 */	
	if (ts->ts_rto_next > (240L * TICK_RATE)) {
		ts->ts_rto_next = 240L * TICK_RATE;
	}

	ts->ts_send_flags |= TCP_SND_ACKED;
}

/*
 * tcp_recv_options()
 *	Handle any options fields in the TCP header.
 *
 * When processing options we take RFC1122 at its word (para 4.2.2.5) and
 * assume that any new options have length fields.
 */
static void tcp_recv_options(struct tcp_instance *ti, struct tcp_socket *ts, struct netbuf *nb)
{
	struct tcp_header *th;
	u8_t length;
	u8_t *p;
	
        th = (struct tcp_header *)nb->nb_transport;
        p = (u8_t *)(th + 1);
        			
	length = (th->th_data_offs << 2) - sizeof(struct tcp_header);

	while (length) {
		u8_t opt;
		u8_t optsz;
		
		opt = *p++;
				
		switch (opt) {
		case 0x00:
			/*
			 * End of options.
			 */
			return;
			
		case 0x01:
			/*
			 * NOP (do nothing).
			 */
			length--;
			break;
			
		default:
			if (length == 1) {
				return;
			}
			
			optsz = *p++;
			if ((optsz < 2) || (optsz > length)) {
				return;
			}
		
			if (opt == 0x02) {
				/*
				 * MSS (maximum segment size).
				 */
				if (optsz == 0x02) {
					ts->ts_remote_mss = hton16(*((u16_t *)p));
				}
			}
		
			length -= optsz;
			p += (optsz - 2);
		}
	}
}

/*
 * tcp_recv_state_machine()
 *	Main function that drives us around the TCP state diagram!
 *
 * If you are of a nervous disposition then you probably don't want to look at
 * the implementation of this function.  The normal rules about keeping a
 * function body short have been thrown in the bin and we seem to almost go on
 * forever.  In fact it's not really that bad because this function is actually
 * just a large switch() statement and the program flow is therefore trivially
 * simple.
 */
static void tcp_recv_state_machine(struct tcp_instance *ti, struct tcp_socket *sock, struct netbuf *nb)
{
	struct ip_header *iph = (struct ip_header *)nb->nb_network;
	struct tcp_header *tch = (struct tcp_header *)nb->nb_transport;

	switch (sock->ts_state) {
	case TCS_LISTEN:
		if (tch->th_ctrl_flags & TCF_RST) {
			break;
		}

		if (tch->th_ctrl_flags & TCF_ACK) {
			/*
			 * We need to send out a reset because it looks like
			 * we have a half-open connection.  As this is a listening
			 * socket however we need to get our sender's socket
			 * details before we can reply!
			 */
			sock->ts_remote_port = hton16(tch->th_src_port);
			sock->ts_remote_addr = hton32(iph->ih_src_addr);
			sock->ts_snd_nxt = hton32(tch->th_ack);
			tcp_build_and_send_sequence(ti, sock, TCF_RST);
			break;
		}
					
		if (tch->th_ctrl_flags & TCF_SYN) {
			if (!sock->ts_awaiting_accept) {
				struct tcp_socket *consock;
				struct tcp_client *tc;

				consock = tcp_socket_alloc(ti);
				consock->ts_local_port = sock->ts_local_port;
				consock->ts_remote_port = hton16(tch->th_src_port);
				consock->ts_remote_addr = hton32(iph->ih_src_addr);
				consock->ts_snd_nxt = sock->ts_snd_nxt;
				consock->ts_snd_una = sock->ts_snd_una;
				consock->ts_rcv_nxt = hton32(tch->th_sequence) + 1;
				sock->ts_awaiting_accept = consock;
				        				
				/*
				 * Issue a callback to our client to let them
				 * know that a new connection has been requested.
				 */
				tc = sock->ts_client;
				if (tc) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_connect(tc, sock->ts_server);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
		}
		break;

	case TCS_SYN_SENT:
		if (tch->th_ctrl_flags & TCF_ACK) {
			if (hton32(tch->th_ack) != sock->ts_snd_nxt) {
        			if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_RST);
        			}
       				break;
        		} else {
        			calc_round_trip(sock);
				if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
					sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
					oneshot_detach(sock->ts_wait_timer);
					tcp_socket_deref(sock);
					netbuf_deref(sock->ts_awaiting_ack);
				}
        		}
        	}
        	
        	if (tch->th_ctrl_flags & TCF_RST) {
			if (tch->th_ctrl_flags & TCF_ACK) {
				sock->ts_state = TCS_CLOSED;
				tcp_socket_detach(ti, sock);
				condvar_signal(&sock->ts_wait_cv);
        		}
        	}
        	
        	if (tch->th_ctrl_flags & TCF_SYN) {
			sock->ts_rcv_nxt = hton32(tch->th_sequence) + 1;
        		
        		if (tch->th_ctrl_flags & TCF_ACK) {
				sock->ts_state = TCS_ESTABLISHED;
				sock->ts_snd_una++;
				
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
        		
				condvar_signal(&sock->ts_wait_cv);
			} else {
				if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
					sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
					oneshot_detach(sock->ts_wait_timer);
					tcp_socket_deref(sock);
					netbuf_deref(sock->ts_awaiting_ack);
				}
				
				sock->ts_state = TCS_SYN_RECEIVED;
				
				tcp_build_and_send_sequence(ti, sock, TCF_SYN | TCF_ACK);
			}
		}
        	break;
	
	case TCS_SYN_RECEIVED:
		if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
			if ((tch->th_ctrl_flags & TCF_RST) == 0) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
		}
		
		if (tch->th_ctrl_flags & TCF_RST) {
			struct tcp_client *tc;

			close_socket(ti, sock);

			tc = sock->ts_client;
			if (tc && tc->tc_close) {
				tcp_client_ref(tc);
				spinlock_unlock(&ti->ti_lock);
				tc->tc_close(tc);
				spinlock_lock(&ti->ti_lock);
				tcp_client_deref(tc);
			}
			break;
		}
		
		if (tch->th_ctrl_flags & TCF_SYN) {
			struct tcp_client *tc;

			send_reset_close_socket(ti, sock);

			tc = sock->ts_client;
			if (tc && tc->tc_close) {
				tcp_client_ref(tc);
				spinlock_unlock(&ti->ti_lock);
				tc->tc_close(tc);
				spinlock_lock(&ti->ti_lock);
				tcp_client_deref(tc);
			}
			break;
		}

		if (!(tch->th_ctrl_flags & TCF_ACK)) {
			break;
		}
			
		if (hton32(tch->th_ack) != sock->ts_snd_nxt) {
			tcp_build_and_send_sequence(ti, sock, TCF_RST);
			break;
		}
						
		calc_round_trip(sock);
		if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
			struct netbuf *nbnext;

			sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
			oneshot_detach(sock->ts_wait_timer);
			tcp_socket_deref(sock);
			netbuf_deref(sock->ts_awaiting_ack);

			nbnext = sock->ts_awaiting_send;
			if (nbnext) {
				sock->ts_awaiting_send = nbnext->nb_next;
				tcp_send_netbuf_now(ti, sock, nbnext);
				netbuf_deref(nbnext);
			}
		}
		sock->ts_state = TCS_ESTABLISHED;
		sock->ts_snd_una++;
			
		condvar_signal(&sock->ts_wait_cv);
		
		if (tch->th_ctrl_flags & TCF_FIN) {
			struct tcp_client *tc;

			/*
			 * Whilst RFC793 shows going to a TCP state of CLOSE-WAIT,
			 * in practice we don't use it.  We always jump straight
			 * to LAST-ACK because we send the "FIN" at the same time
			 * as the "ACK of FIN".
			 */
			sock->ts_rcv_nxt++;
			sock->ts_state = TCS_LAST_ACK;
			
			tcp_build_and_send_sequence(ti, sock, TCF_ACK | TCF_FIN);
		
			tc = sock->ts_client;
			if (tc && tc->tc_close) {
				tcp_client_ref(tc);
				spinlock_unlock(&ti->ti_lock);
				tc->tc_close(tc);
				spinlock_lock(&ti->ti_lock);
				tcp_client_deref(tc);
			}
		}
		break;
	
	case TCS_ESTABLISHED:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				struct tcp_client *tc;

				close_socket(ti, sock);

				tc = sock->ts_client;
				if (tc && tc->tc_close) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_close(tc);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				struct tcp_client *tc;

				send_reset_close_socket(ti, sock);

				tc = sock->ts_client;
				if (tc && tc->tc_close) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_close(tc);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				/*
				 * If we've just had an ACK of all of the data
				 * that we've sent we need to check if this is the
				 * first time we've seen such an ACK.  If we still
				 * had a segment retransmit timer running then it's
				 * the first time and we clear the retransmit and
				 * signal the socket's owner.
				 */
				if (netack == sock->ts_snd_nxt) {
					if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
						struct netbuf *nbnext;

						sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
						oneshot_detach(sock->ts_wait_timer);
						tcp_socket_deref(sock);
						netbuf_deref(sock->ts_awaiting_ack);

						nbnext = sock->ts_awaiting_send;				
						if (nbnext) {
							sock->ts_awaiting_send = nbnext->nb_next;
							tcp_send_netbuf_now(ti, sock, nbnext);
							netbuf_deref(nbnext);
						}
					}
				}
			}

			/*
			 * Deal with the data.
	                 */
	                if (nb->nb_application_size) {
				struct tcp_client *tc;

				sock->ts_rcv_nxt += nb->nb_application_size;

				tcp_build_and_send_sequence(ti, sock, TCF_ACK);

				tc = sock->ts_client;
				if (tc) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_recv(tc, nb);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
						
			if (tch->th_ctrl_flags & TCF_FIN) {
				struct tcp_client *tc;

				/*
				 * If all of our segments are ACK'd then we can skip
				 * the CLOSE-WAIT state, send FIN, ACK and go to
				 * LAST-ACK.  If we've not had all of our transmissions
				 * ACK'd then send ACK and go to CLOSE-WAIT, from where
				 * we'll send our FIN!
				 */
				sock->ts_rcv_nxt++;
			
				if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
			        	sock->ts_state = TCS_CLOSE_WAIT;
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
			        } else {
					sock->ts_state = TCS_LAST_ACK;
					tcp_build_and_send_sequence(ti, sock, TCF_ACK | TCF_FIN);
			        }
			
				condvar_signal(&sock->ts_wait_cv);
			
				tc = sock->ts_client;
				if (tc && tc->tc_close) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_close(tc);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
		}
		break;
				
	case TCS_FIN_WAIT1:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				close_socket(ti, sock);
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				send_reset_close_socket(ti, sock);
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				if (netack == sock->ts_snd_nxt) {
					if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
						struct netbuf *nbnext;

						sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
						oneshot_detach(sock->ts_wait_timer);
						tcp_socket_deref(sock);
						netbuf_deref(sock->ts_awaiting_ack);

						nbnext = sock->ts_awaiting_send;				
						if (nbnext) {
							sock->ts_awaiting_send = nbnext->nb_next;
							tcp_send_netbuf_now(ti, sock, nbnext);
							netbuf_deref(nbnext);
						} else if (sock->ts_send_flags & TCP_SND_DEFERRED_FIN) {
							sock->ts_send_flags &= (~TCP_SND_DEFERRED_FIN);
							spinlock_unlock(&ti->ti_lock);
							tcp_send_close(ti, sock);
							spinlock_lock(&ti->ti_lock);
						} else {
							sock->ts_state = TCS_FIN_WAIT2;
						}
					} else {
						sock->ts_state = TCS_FIN_WAIT2;
					}
				}
			}

			/*
			 * Deal with the data.
			 */
	                if (nb->nb_application_size) {
				struct tcp_client *tc;

				sock->ts_rcv_nxt += nb->nb_application_size;

				tcp_build_and_send_sequence(ti, sock, TCF_ACK);

				tc = sock->ts_client;
				if (tc) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_recv(tc, nb);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
						
			if (tch->th_ctrl_flags & TCF_FIN) {
				sock->ts_rcv_nxt++;
			
				/*
				 * Have we had an ack as well?  If we have then we move
				 * to TIME-WAIT, otherwise we go to LAST-ACK.  We know
				 * that we've had the ack because our earlier code will
				 * have moved us to FIN-WAIT2.
				 */
				if (sock->ts_state == TCS_FIN_WAIT2) {
					sock->ts_state = TCS_TIME_WAIT;
				        sock->ts_wait_timer->os_callback = tcp_tick_time_wait;
					sock->ts_wait_timer->os_ticks_left = 60 * TICK_RATE;
					oneshot_attach(sock->ts_wait_timer);
				} else {
					sock->ts_state = TCS_CLOSING;
				}
			
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
			}
		}
		break;
				
	case TCS_FIN_WAIT2:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				close_socket(ti, sock);
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				send_reset_close_socket(ti, sock);
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				if (netack == sock->ts_snd_nxt) {
					if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
						struct netbuf *nbnext;

						sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
						oneshot_detach(sock->ts_wait_timer);
						tcp_socket_deref(sock);
						netbuf_deref(sock->ts_awaiting_ack);

						nbnext = sock->ts_awaiting_send;				
						if (nbnext) {
							sock->ts_awaiting_send = nbnext->nb_next;
							tcp_send_netbuf_now(ti, sock, nbnext);
							netbuf_deref(nbnext);
						}
					}
				}
			}

			/*
			 * Deal with the data.
	                 */
	                if (nb->nb_application_size) {
				struct tcp_client *tc;

				sock->ts_rcv_nxt += nb->nb_application_size;

				tcp_build_and_send_sequence(ti, sock, TCF_ACK);

				tc = sock->ts_client;
				if (tc) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_recv(tc, nb);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
						
			if (tch->th_ctrl_flags & TCF_FIN) {
				sock->ts_rcv_nxt++;
				sock->ts_state = TCS_TIME_WAIT;
			        sock->ts_wait_timer->os_callback = tcp_tick_time_wait;
				sock->ts_wait_timer->os_ticks_left = 60 * TICK_RATE;
				oneshot_attach(sock->ts_wait_timer);

				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
			}
		}
		break;

	case TCS_CLOSE_WAIT:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				close_socket(ti, sock);
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				send_reset_close_socket(ti, sock);
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				if (netack == sock->ts_snd_nxt) {
					if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
						struct netbuf *nbnext;

						sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
						oneshot_detach(sock->ts_wait_timer);
						tcp_socket_deref(sock);
						netbuf_deref(sock->ts_awaiting_ack);

						nbnext = sock->ts_awaiting_send;				
						if (nbnext) {
							sock->ts_awaiting_send = nbnext->nb_next;
							tcp_send_netbuf_now(ti, sock, nbnext);
							netbuf_deref(nbnext);
							break;
						}
						
						/*
						 * We implicitly have a deferred FIN
						 * if we're in CLOSE-WAIT, so send it!
						 */
						spinlock_unlock(&ti->ti_lock);
						tcp_send_close(ti, sock);
						spinlock_lock(&ti->ti_lock);
						sock->ts_state = TCS_LAST_ACK;
						break;
					}
				}
			}

			/*
			 * Deal with the data.
			 */
	                if (nb->nb_application_size) {
				struct tcp_client *tc;

				sock->ts_rcv_nxt += nb->nb_application_size;

				tcp_build_and_send_sequence(ti, sock, TCF_ACK);

				tc = sock->ts_client;
				if (tc) {
					tcp_client_ref(tc);
					spinlock_unlock(&ti->ti_lock);
					tc->tc_recv(tc, nb);
					spinlock_lock(&ti->ti_lock);
					tcp_client_deref(tc);
				}
			}
						
			if (tch->th_ctrl_flags & TCF_FIN) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
			}
		}
		break;
				
	case TCS_CLOSING:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				close_socket(ti, sock);
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				send_reset_close_socket(ti, sock);
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				/*
				 * If we've just had an ACK of all of the data
				 * that we've sent we're finished
				 */
				if (netack == sock->ts_snd_nxt) {
					if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
						sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
						oneshot_detach(sock->ts_wait_timer);
						tcp_socket_deref(sock);
						netbuf_deref(sock->ts_awaiting_ack);
					}
			
					sock->ts_state = TCS_TIME_WAIT;
				        sock->ts_wait_timer->os_callback = tcp_tick_time_wait;
					sock->ts_wait_timer->os_ticks_left = 60 * TICK_RATE;
					oneshot_attach(sock->ts_wait_timer);
				}
			}
		}
		break;
				
	case TCS_LAST_ACK:
		{
			u32_t netack;
			
			if (hton32(tch->th_sequence) != sock->ts_rcv_nxt) {
				if ((tch->th_ctrl_flags & TCF_RST) == 0) {
					tcp_build_and_send_sequence(ti, sock, TCF_ACK);
					break;
				}
			}
		
			if (tch->th_ctrl_flags & TCF_RST) {
				close_socket(ti, sock);
				break;
			}
		
			if (tch->th_ctrl_flags & TCF_SYN) {
				send_reset_close_socket(ti, sock);
				break;
			}
		
			if (!(tch->th_ctrl_flags & TCF_ACK)) {
				break;
			}
			
			netack = hton32(tch->th_ack);

			if (netack > sock->ts_snd_nxt) {
				tcp_build_and_send_sequence(ti, sock, TCF_ACK);
				break;
			}
						
			if (netack >= sock->ts_snd_una) {
				calc_round_trip(sock);
				
				sock->ts_snd_una = netack;
				
				/*
				 * If we've just had an ACK of all of the data
				 * that we've sent we're finished
				 */
				if (netack == sock->ts_snd_nxt) {
					close_socket(ti, sock);
				}
			}
		}
		break;
				
	case TCS_TIME_WAIT:
	case TCS_CLOSED:
	default:
	}
}

/*
 * tcp_recv_netbuf_icmp()
 *
 * This will be invoked where a packet that we sent got an ICMP error back.
 *
 * For now at least we're only interested in "destination unreachable"
 * messages.
 *
 * We look to see if we can find a socket for which this is relevant and also
 * check that this is either a code 2 (protocol unreachable), code 3 (port
 * unreachable) or code 4 (fragmentation required and DF set).  If we get a
 * match then we abort our connection (RFC1122 para 4.2.3.9)
 */
void tcp_recv_netbuf_icmp(void *clnt, struct netbuf *nb)
{
	struct ip_client *ic;
	struct tcp_instance *ti;
	struct tcp_socket *sock;
	struct ip_header *iph;
	struct icmp_header *ich;
        struct tcp_header *tch;

	ic = (struct ip_client *)clnt;
	ti = (struct tcp_instance *)ic->ic_instance;
			
	ich = (struct icmp_header *)nb->nb_transport;

	/*
	 * Ignore anything other than "destination unreachable" errors.
	 */
	if (ich->ich_type != 0x04) {
		return;
	}
	
	iph = (struct ip_header *)nb->nb_application;
	tch = (struct tcp_header *)(((u8_t *)iph) + (iph->ih_header_len * 4));

	/*
	 * Look up the socket details.  Note that we're only interested in
	 * active (connected) sockets and not passive (listening) ones.
	 */
	spinlock_lock(&ti->ti_lock);

	sock = ti->ti_socks;
	while (sock) {
		if ((sock->ts_local_port == hton16(tch->th_dest_port))
				&& (sock->ts_remote_addr == hton32(iph->ih_src_addr))
				&& (sock->ts_remote_port == hton16(tch->th_src_port))) {
			struct icmp_header *ich;
			
			/*
			 * Is the ICMP type 2, 3 or 4?
			 */
			ich = (struct icmp_header *)nb->nb_transport;
		
			if ((ich->ich_code >= 2) && (ich->ich_code <= 4)) {
				/*
				 * Send reset segments where necessary, and
				 * then close the connection.
				 */
				if ((sock->ts_state == TCS_SYN_RECEIVED)
						|| (sock->ts_state == TCS_ESTABLISHED)
						|| (sock->ts_state == TCS_FIN_WAIT1)
						|| (sock->ts_state == TCS_FIN_WAIT2)
						|| (sock->ts_state == TCS_CLOSE_WAIT)) {
					send_reset_close_socket(ti, sock);
				} else {
					close_socket(ti, sock);
				}
			}
		
			break;
		}
		
		sock = sock->ts_next;
	}

	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_recv_netbuf()
 */
void tcp_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ip_client *ic;
	struct tcp_instance *ti;
	struct tcp_socket *sock, *listener;
	u16_t csum;
	struct ip_header *iph = (struct ip_header *)nb->nb_network;
	struct tcp_header *tch = (struct tcp_header *)nb->nb_transport;

	ic = (struct ip_client *)clnt;
	ti = (struct tcp_instance *)ic->ic_instance;
	
	/*
	 * When we get here our application data is still hooked under the
	 * transport layer, so the first thing to do is untangle it.
	 */
	nb->nb_application = ((u32_t *)tch) + tch->th_data_offs;
	nb->nb_application_size = nb->nb_transport_size - (tch->th_data_offs * 4);
	nb->nb_transport_size = tch->th_data_offs * 4;
	
	/*
	 * If we've been given a checksum then check it!
	 */
	if (tch->th_csum != 0) {
		csum = ipcsum_pseudo_partial(hton32(((struct ip_instance *)ic->ic_server->is_instance)->ii_addr),
						iph->ih_src_addr, 0x06, hton16(nb->nb_transport_size + nb->nb_application_size));
		csum = ipcsum_partial(csum, nb->nb_transport, nb->nb_transport_size);
		csum = ipcsum(csum, nb->nb_application, nb->nb_application_size);
		if (csum != 0) {
			debug_print_pstr("\ftcprcv: csum fl: ");
			debug_print16(csum);
			debug_print_pstr(":");
			debug_print16(nb->nb_transport_size);
			debug_print_pstr(":");
			debug_print16(nb->nb_application_size);
			return;
		}
	}

	/*
	 * Look up the socket and see if we know anyone who will try and deal
	 * with it!  If we find a listener then we need to keep looking for an
	 * exact match before we go with it.
	 */
	spinlock_lock(&ti->ti_lock);

	listener = NULL;
	sock = ti->ti_socks;
	while (sock) {
		if (sock->ts_local_port == hton16(tch->th_dest_port)) {
			if (sock->ts_state == TCS_LISTEN) {
				listener = sock;
			} else if ((sock->ts_remote_addr == hton32(iph->ih_src_addr))
					&& (sock->ts_remote_port == hton16(tch->th_src_port))) {
				break;
			}
		}
		
		sock = sock->ts_next;
	}

	/*
	 * If we didn't get a connection match then we go for any passive match.
	 */
	if (!sock) {
		sock = listener;
	}

	if (!sock) {
	        struct ip_header *iphc;
		struct netbuf *nbrep;
                u32_t empty = 0;
		struct ip_server *is;

		is = ti->ti_server;
		ip_server_ref(is);
		spinlock_unlock(&ti->ti_lock);
		
		/*
		 * Issue an ICMP destination unreachable message (port unreachable).
		 */	
		nbrep = netbuf_alloc();
		nbrep->nb_application_membuf = membuf_alloc(nb->nb_network_size + 8, NULL);
		nbrep->nb_application = nbrep->nb_application_membuf;
	        nbrep->nb_application_size = nb->nb_network_size + 8;

		iphc = (struct ip_header *)nbrep->nb_application;
                memcpy(iphc, nb->nb_network, nb->nb_network_size);
		memcpy((iphc + 1), nb->nb_transport, 8);
		
		is->is_send_icmp(is, iph->ih_src_addr, 0x03, 0x03, (u8_t *)(&empty), nbrep);
	
		ip_server_deref(is);
		netbuf_deref(nbrep);
		return;
	}

	/*
	 * Process any TCP header options and then invoke the receive state
	 * machine.
	 */		
	tcp_recv_options(ti, sock, nb);
	tcp_recv_state_machine(ti, sock, nb);
	
	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_send_sequence()
 *	Send a netbuf.
 *
 * Calculates the header checksum before we send the netbuf.
 */
void tcp_send_sequence(struct tcp_instance *ti, struct tcp_socket *sock, struct netbuf *nb)
{
	u16_t csum;
        struct tcp_header *tch;
	struct ip_server *is;

        /*
         * Complete the TCP header.
         */
        tch = (struct tcp_header *)nb->nb_transport;
	tch->th_ack = hton32(sock->ts_rcv_nxt);
		
	tch->th_csum = 0;
	
	spinlock_lock(&ti->ti_lock);
	is = ti->ti_server;
	ip_server_ref(is);
	spinlock_unlock(&ti->ti_lock);
	
	csum = ipcsum_pseudo_partial(hton32(((struct ip_instance *)is->is_instance)->ii_addr),
					hton32(sock->ts_remote_addr), 0x06, hton16(nb->nb_transport_size + nb->nb_application_size));
	csum = ipcsum_partial(csum, tch, nb->nb_transport_size);
	tch->th_csum = ipcsum(csum, nb->nb_application, nb->nb_application_size);
		
	is->is_send(is, hton32(sock->ts_remote_addr), 0x06, nb);

	ip_server_deref(is);
}

/*
 * tcp_build_and_send_sequence()
 */
void tcp_build_and_send_sequence(struct tcp_instance *ti, struct tcp_socket *sock, u8_t ctrl_flags)
{
	struct netbuf *nb;
	
	nb = netbuf_alloc();
	
	build_netbuf(ti, sock, ctrl_flags, nb);

	if (ctrl_flags & (TCF_SYN | TCF_FIN)) {
		sock->ts_snd_nxt++;
		sock->ts_awaiting_ack = netbuf_clone(nb);
		sock->ts_wait_timer->os_ticks_left = sock->ts_rto_next;
		sock->ts_retransmits = 0;
		sock->ts_send_jiffies = timer_get_jiffies();
	        sock->ts_send_flags = TCP_SND_RETRANS_TIMING;
		tcp_socket_ref(sock);
		oneshot_attach(sock->ts_wait_timer);
	}

        /*
         * If our sequence contains any flags other than ACK then we send it
         * immediately.  If however this is only an ACK packet then we try and
         * defer sending it if we can.
         */
        if ((ctrl_flags & (~TCF_ACK))
        		|| (sock->ts_send_flags & TCP_SND_DEFERRED_ACK)
        		|| (sock->ts_state != TCS_ESTABLISHED)) {
		if (sock->ts_send_flags & TCP_SND_DEFERRED_ACK) {
			sock->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
			oneshot_detach(sock->ts_deferred_ack_timer);
			tcp_socket_deref(sock);
			netbuf_deref(sock->ts_deferred_ack);
		}
				        		
		spinlock_unlock(&ti->ti_lock);
		tcp_send_sequence(ti, sock, nb);
		spinlock_lock(&ti->ti_lock);

		netbuf_deref(nb);
        } else {
		sock->ts_send_flags |= TCP_SND_DEFERRED_ACK;
        	sock->ts_deferred_ack = nb;
        	sock->ts_deferred_ack_timer->os_ticks_left = TICK_RATE / 5;
		tcp_socket_ref(sock);
        	oneshot_attach(sock->ts_deferred_ack_timer);
        }
}	

/*
 * tcp_listen()
 *	Set a socket listening for incoming TCP connection requests.
 */
void tcp_listen(void *srv, u16_t port)
{
	struct tcp_server *ts;
	struct tcp_instance *ti;
	struct tcp_socket *sock;
		
	ts = (struct tcp_server *)srv;
	ti = (struct tcp_instance *)ts->ts_instance;
	
	spinlock_lock(&ti->ti_lock);
	
	if (ts->ts_socket) {
		spinlock_unlock(&ti->ti_lock);
		return;
	}
	
	sock = tcp_socket_alloc(ti);
	sock->ts_state = TCS_LISTEN;
	sock->ts_local_port = port;
	tcp_socket_attach(ti, sock);
	tcp_socket_deref(sock);

	ts->ts_client->tc_socket = sock;
	ts->ts_socket = sock;
	sock->ts_client = ts->ts_client;
	sock->ts_server = ts;

	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_accept()
 *	Accept an incoming TCP connection (from a listening socket).
 *
 * Don't be misled by the name of this function as it doesn't do quite what
 * you might expect if you are used to Berkeley sockets.  Whilst, this is
 * called by a "service provider" to accept an incoming connection request, the
 * TCP does not send SYN, ACK back to the connecting host until we accept the
 * connection here.  In addition the service provider also has the option to
 * reject the connection too.
 *
 * This may all seem a little odd, however it's symmetrical and efficient:
 * the pair of operations means that we can choose how to handle connection
 * requests instead of having a policy mandated by the TCP layer, and we don't
 * waste any bandwidth or CPU-time on completing the three-way handshake and
 * then immediately closing the link.
 */
s8_t tcp_accept(void *srv, void *listensrv)
{
	struct tcp_server *ts, *listents;
	struct tcp_instance *ti;
	struct tcp_socket *sock, *listensock;
	
	ts = (struct tcp_server *)srv;
	listents = (struct tcp_server *)listensrv;
	ti = (struct tcp_instance *)ts->ts_instance;
	
	spinlock_lock(&ti->ti_lock);
	
	listensock = listents->ts_socket;
	if (ts->ts_socket || (!listensock) || (listensock->ts_state != TCS_LISTEN)) {
		spinlock_unlock(&ti->ti_lock);
		return -1;
	}

	if (DEBUG) {
		if (listensock->ts_awaiting_accept == NULL) {
			debug_stop();
			debug_print_pstr("tcp_accept: no waiting socket");
			while (1);
		}
	}
	
	/*
	 * We have a connection awaiting acceptance.  Attach the new socket and
	 * send the SYN, ACK in response.  We don't do this earlier because we
	 * don't want to go any faster than our service is capable of supporting!
	 */
	sock = listensock->ts_awaiting_accept;
	listensock->ts_awaiting_accept = NULL;
	sock->ts_state = TCS_SYN_RECEIVED;
	tcp_socket_attach(ti, sock);
	tcp_socket_deref(sock);

	ts->ts_client->tc_socket = sock;
	ts->ts_socket = sock;
	sock->ts_client = ts->ts_client;
	sock->ts_server = ts;

	tcp_build_and_send_sequence(ti, sock, TCF_SYN | TCF_ACK);

	spinlock_unlock(&ti->ti_lock);

	return 0;
}

/*
 * tcp_reject()
 *	Reject an incoming TCP connection (from a listening socket).
 *
 * This is a slightly odd function as there's no direct mention of anything
 * like it in the various TCP RFCs.  The nearest equivalent is the ability for
 * the TCP to reset any connection that fails to meet the correct security
 * requirements.  We take this as a design guide that we can implement this
 * feature by sending a reset if we don't want to talk to a host that is trying
 * to connect.
 *
 * Please see tcp_accept() for more notes about how it and this function
 * interact.
 */
void tcp_reject(void *srv, void *listensrv)
{
	struct tcp_server *ts, *listents;
	struct tcp_instance *ti;
	struct tcp_socket *sock, *listensock;
	
	ts = (struct tcp_server *)srv;
	listents = (struct tcp_server *)listensrv;
	ti = (struct tcp_instance *)ts->ts_instance;
	
	spinlock_lock(&ti->ti_lock);
	
	listensock = listents->ts_socket;
	if (ts->ts_socket || (!listensock) || (listensock->ts_state != TCS_LISTEN)) {
		spinlock_unlock(&ti->ti_lock);
		return;
	}

	if (DEBUG) {
		if (listensock->ts_awaiting_accept == NULL) {
			debug_stop();
			debug_print_pstr("tcp_reject: no waiting socket");
			while (1);
		}
	}
	
	sock = listensock->ts_awaiting_accept;
	listensock->ts_awaiting_accept = NULL;
	tcp_build_and_send_sequence(ti, sock, TCF_RST);
	tcp_socket_deref(sock);

	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_connect()
 *	Connect to a remote socket.
 */
s8_t tcp_connect(void *srv, u32_t addr, u16_t port)
{
	struct tcp_server *ts;
	struct tcp_instance *ti;
	struct tcp_socket *sock;
	
	ts = (struct tcp_server *)srv;
	ti = (struct tcp_instance *)ts->ts_instance;
	
	spinlock_lock(&ti->ti_lock);
	
	if (ts->ts_socket) {
		spinlock_unlock(&ti->ti_lock);
		return -1;
	}

	/*
	 * Find a new local port with which to make our connection.
	 */
	do {
		ti->ti_last_local_port++;
		if (ti->ti_last_local_port == 0x0000) {
			ti->ti_last_local_port = 0x1000;
		}
		sock = ti->ti_socks;
		while (sock) {
			if (sock->ts_local_port == ti->ti_last_local_port) {
				break;
			}
			sock = sock->ts_next;
		}
	} while (sock != NULL);

	/*
	 * OK - we've got a new port.  Now create a new socket structure and
	 * add it into the list.
	 */	
	sock = tcp_socket_alloc(ti);
	sock->ts_local_port = ti->ti_last_local_port;
	sock->ts_remote_port = port;
	sock->ts_remote_addr = addr;
	sock->ts_state = TCS_SYN_SENT;
	tcp_socket_attach(ti, sock);
	tcp_socket_deref(sock);

	ts->ts_client->tc_socket = sock;
	ts->ts_socket = sock;
	sock->ts_client = ts->ts_client;
	sock->ts_server = ts;

	tcp_build_and_send_sequence(ti, sock, TCF_SYN);
	
	/*
	 * Now go to sleep until we get out of the synchronization states (one
	 * way or another).
	 */
	while ((sock->ts_state == TCS_SYN_SENT) || (sock->ts_state == TCS_SYN_RECEIVED)) {
		if (condvar_wait(&sock->ts_wait_cv) == CONTEXT_INTERRUPTED) {
			/*
			 * We've been interrupted so we purge and close
			 * this socket.
			 */
			if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
				sock->ts_send_flags &= (~TCP_SND_RETRANS_TIMING);
				oneshot_detach(sock->ts_wait_timer);
				tcp_socket_deref(sock);
				netbuf_deref(sock->ts_awaiting_ack);
			}
			sock->ts_state = TCS_CLOSED;
			tcp_socket_detach(ti, sock);
		}
	}
	spinlock_unlock(&ti->ti_lock);

	return 0;
}

/*
 * client_close_socket()
 *
 * It is assumed that on entry to this function the instance lock is held.
 */
static void client_close_socket(struct tcp_instance *ti, struct tcp_socket *sock)
{
	/*
	 * If we're detaching a listening (passive) socket then just detach it
	 * from our list of sockets.  Otherwise, if we're in an ESTABLISHED
	 * state then we need to close the socket.
	 */
	if (sock->ts_state == TCS_LISTEN) {
		tcp_socket_detach(ti, sock);
	} else if (sock->ts_state == TCS_ESTABLISHED) {
		/*
		 * We go to FIN-WAIT1 immediately, however we only send the FIN if
		 * we're not waiting for an ACK to another segment.  If we are waiting
		 * then the code in the ACK receive path will issue the FIN.
		 */
		sock->ts_state = TCS_FIN_WAIT1;
		if (!(sock->ts_send_flags & TCP_SND_RETRANS_TIMING)) {
			spinlock_unlock(&ti->ti_lock);
			tcp_send_close(ti, sock);
			spinlock_lock(&ti->ti_lock);
	        } else {
			sock->ts_send_flags |= TCP_SND_DEFERRED_FIN;
	        }
	}
}

/*
 * tcp_close()
 */
void tcp_close(void *srv)
{
	struct tcp_server *ts;
	struct tcp_instance *ti;
	struct tcp_socket *sock;
	
	ts = (struct tcp_server *)srv;
	ti = (struct tcp_instance *)ts->ts_instance;
		
	spinlock_lock(&ti->ti_lock);
	
	sock = ts->ts_socket;
	client_close_socket(ti, sock);
	ts->ts_socket = NULL;
        ts->ts_client->tc_socket = NULL;
	sock->ts_server = NULL;
	sock->ts_client = NULL;
		
	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_send_netbuf_now()
 */
void tcp_send_netbuf_now(struct tcp_instance *ti, struct tcp_socket *sock, struct netbuf *nb)
{
	/*
	 * If we had a previously deferred ACK then we can eliminate it now
	 * because we're going to send our segment and an ACK now.
	 */
	if (sock->ts_send_flags & TCP_SND_DEFERRED_ACK) {
		sock->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
		oneshot_detach(sock->ts_deferred_ack_timer);
		tcp_socket_deref(sock);
		netbuf_deref(sock->ts_deferred_ack);
	}
	
        build_netbuf(ti, sock, TCF_PSH | TCF_ACK, nb);
	
	sock->ts_snd_nxt += nb->nb_application_size;
	sock->ts_awaiting_ack = netbuf_clone(nb);
	sock->ts_wait_timer->os_ticks_left = sock->ts_rto_next;
	sock->ts_retransmits = 0;
	sock->ts_send_jiffies = timer_get_jiffies();
        sock->ts_send_flags = TCP_SND_RETRANS_TIMING;
	tcp_socket_ref(sock);
	oneshot_attach(sock->ts_wait_timer);

	spinlock_unlock(&ti->ti_lock);

	tcp_send_sequence(ti, sock, nb);

	spinlock_lock(&ti->ti_lock);
}

/*
 * tcp_send_netbuf()
 */
void tcp_send_netbuf(void *srv, struct netbuf *nb)
{
	struct tcp_server *ts;
	struct tcp_instance *ti;
	struct tcp_socket *sock;
	
	ts = (struct tcp_server *)srv;
	ti = (struct tcp_instance *)ts->ts_instance;
	
	spinlock_lock(&ti->ti_lock);
	
	sock = ts->ts_socket;
        if ((!sock) || (sock->ts_state != TCS_ESTABLISHED)) {
		spinlock_unlock(&ti->ti_lock);
        	return;
        }

	if (sock->ts_send_flags & TCP_SND_RETRANS_TIMING) {
		struct netbuf *p;
		struct netbuf **pprev;

		netbuf_ref(nb);
		pprev = &sock->ts_awaiting_send;
		p = sock->ts_awaiting_send;
		while (p) {
			pprev = &p->nb_next;
			p = p->nb_next;
		}
	
		nb->nb_next = NULL;
		*pprev = nb;

		spinlock_unlock(&ti->ti_lock);
        	return;
	}
	
	/*
	 * If we had a previously deferred ACK then we can eliminate it now
	 * because we're going to send our segment and an ACK now.
	 */
	if (sock->ts_send_flags & TCP_SND_DEFERRED_ACK) {
		sock->ts_send_flags &= (~TCP_SND_DEFERRED_ACK);
		oneshot_detach(sock->ts_deferred_ack_timer);
		tcp_socket_deref(sock);
		netbuf_deref(sock->ts_deferred_ack);
	}
	
        build_netbuf(ti, sock, TCF_PSH | TCF_ACK, nb);
	
	sock->ts_snd_nxt += nb->nb_application_size;
	sock->ts_awaiting_ack = netbuf_clone(nb);
	sock->ts_wait_timer->os_ticks_left = sock->ts_rto_next;
	sock->ts_retransmits = 0;
	sock->ts_send_jiffies = timer_get_jiffies();
        sock->ts_send_flags = TCP_SND_RETRANS_TIMING;
	tcp_socket_ref(sock);
	oneshot_attach(sock->ts_wait_timer);

	spinlock_unlock(&ti->ti_lock);

	tcp_send_sequence(ti, sock, nb);
}

/*
 * tcp_server_alloc()
 *	Allocate a TCP server structure.
 */
struct tcp_server *tcp_server_alloc(void)
{
	struct tcp_server *ts;
	
	ts = (struct tcp_server *)membuf_alloc(sizeof(struct tcp_server), NULL);
	ts->ts_socket = NULL;
	ts->ts_listen = tcp_listen;
	ts->ts_accept = tcp_accept;
	ts->ts_reject = tcp_reject;
	ts->ts_connect = tcp_connect;
	ts->ts_send = tcp_send_netbuf;
	ts->ts_close = tcp_close;
		        	
	return ts;
}

/*
 * tcp_client_alloc()
 *	Allocate an TCP socket structure.
 */
struct tcp_client *tcp_client_alloc(void)
{
	struct tcp_client *tc;
	
	tc = (struct tcp_client *)membuf_alloc(sizeof(struct tcp_client), NULL);
	tc->tc_socket = NULL;
	tc->tc_connect = NULL;
	tc->tc_recv = NULL;
	tc->tc_close = NULL;

	return tc;
}

/*
 * tcp_client_attach()
 *	Attach an TCP client.
 */
struct tcp_server *tcp_client_attach(struct tcp_instance *ti, struct tcp_client *tc)
{
        struct tcp_server *ts = NULL;
	
	spinlock_lock(&ti->ti_lock);

	ts = tcp_server_alloc();		
	ts->ts_instance = ti;
	ts->ts_client = tc;
	tcp_instance_ref(ti);

	tc->tc_server = ts;
	tc->tc_next = ti->ti_client_list;
	ti->ti_client_list = tc;
	tcp_client_ref(tc);
				
	spinlock_unlock(&ti->ti_lock);

	return ts;
}

/*
 * tcp_client_detach()
 *	Detach an TCP client.
 */
void tcp_client_detach(struct tcp_instance *ti, struct tcp_server *ts)
{
	struct tcp_client *tc;
	struct tcp_client **tcprev;
	struct tcp_socket *sock;

	spinlock_lock(&ti->ti_lock);
	
	/*
	 * If we have a socket attached to this connection then go and
	 * close it.
	 */
	sock = ts->ts_socket;
	if (sock) {
		client_close_socket(ti, sock);
	}
	
	/*
	 * Walk the client list and find the one we're to remove.
	 */
	tc = ti->ti_client_list;
	tcprev = &ti->ti_client_list;
	while (tc) {
		if (tc->tc_server == ts) {
			*tcprev = tc->tc_next;
			tcp_client_deref(tc);
			tcp_instance_deref(ti);
			tcp_server_deref(ts);
			break;
		}
	
		tcprev = &tc->tc_next;
		tc = tc->tc_next;
	}
			
	spinlock_unlock(&ti->ti_lock);
}

/*
 * tcp_dump_stats()
 */
int tcp_dump_stats(struct tcp_instance *ti, struct tcp_socket *sbuf, int max)
{
	struct tcp_socket *sock;
        int ct = 0;
		
	spinlock_lock(&ti->ti_lock);

	sock = ti->ti_socks;
	while (sock && (ct < max)) {
		memcpy(sbuf, sock, sizeof(struct tcp_socket));
		sbuf->ts_next = sock;
		sbuf++;
		ct++;
		sock = sock->ts_next;
	}
	
	spinlock_unlock(&ti->ti_lock);

	return ct;
}

/*
 * tcp_instance_alloc()
 */
struct tcp_instance *tcp_instance_alloc(struct ip_instance *ii)
{
	struct tcp_instance *ti;
	struct ip_client *ic;

	ti = (struct tcp_instance *)membuf_alloc(sizeof(struct tcp_instance), NULL);
	ti->ti_client_attach = tcp_client_attach;
	ti->ti_client_detach = tcp_client_detach;
	ti->ti_client_list = NULL;
	spinlock_init(&ti->ti_lock, 0x20);
	ti->ti_last_local_port = 0x1008;
	ti->ti_socks = NULL;
	
	/*
	 * Attach this protocol handler to the IP stack.
	 */
	ic = ip_client_alloc();
	ic->ic_protocol = 0x06;
	ic->ic_recv = tcp_recv_netbuf;
	ic->ic_recv_icmp = tcp_recv_netbuf_icmp;
	ic->ic_instance = ti;
	tcp_instance_ref(ti);
	
	ti->ti_server = ii->ii_ip_client_attach(ii, ic);
	ip_client_deref(ic);
		
	return ti;
}
