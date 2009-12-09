/*
 * ppp.c
 *	Point-to-point protocol support.
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
 * We attempt to manage the PPP connection negotiation by running the state
 * machine described int RFC1661.  There are other ways that this can be done,
 * but they are almost certainly harder to code if they still provide a
 * completely general implementation.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "membuf.h"
#include "netbuf.h"
#include "timer.h"
#include "oneshot.h"
#include "ppp_ahdlc.h"
#include "ppp.h"

/*
 * Link operation phases.  See page 6 of RFC1661.
 */
#define PHASE_DEAD 0
#define PHASE_ESTABLISH 1
#define PHASE_AUTHENTICATE 2
#define PHASE_NETWORK 3
#define PHASE_TERMINATE 4

/*
 * Control protocol states.  See pages 12 and 13 of RFC1661.
 */
#define STATE_INITIAL 0
#define STATE_STARTING 1
#define STATE_CLOSED 2
#define STATE_STOPPED 3
#define STATE_CLOSING 4
#define STATE_STOPPING 5
#define STATE_REQ_SENT 6
#define STATE_ACK_RCVD 7
#define STATE_ACK_SENT 8
#define STATE_OPENED 9

/*
 * State transition events.  See page 11 of RFC1661
 */
#define EVENT_UP 0
#define EVENT_DOWN 1
#define EVENT_OPEN 2
#define EVENT_CLOSE 3
#define EVENT_TO_P 4
#define EVENT_TO_M 5
#define EVENT_RCR_P 6
#define EVENT_RCR_M 7
#define EVENT_RCA 8
#define EVENT_RCN 9
#define EVENT_RTR 10
#define EVENT_RTA 11
#define EVENT_RUC 12
#define EVENT_RXJ_P 13
#define EVENT_RXJ_M 14
#define EVENT_RXR 15

/*
 * Actions.  See page 11 of RFC1661.  Note that some of the actions listed below
 * are in fact compound actions built from combinations of simpler ones.
 */
#define ACTION_NONE 0
#define ACTION_TLU 1
#define ACTION_TLD 2
#define ACTION_TLS 3
#define ACTION_TLF 4
#define ACTION_IRC 5
#define ACTION_ZRC 6
#define ACTION_SCR 7
#define ACTION_SCA 8
#define ACTION_SCN 9
#define ACTION_STR 10
#define ACTION_STA 11
#define ACTION_SCJ 12
#define ACTION_SER 13
#define ACTION_TLD_SCR 14
#define ACTION_IRC_TLU 15
#define ACTION_IRC_SCR 16
#define ACTION_IRC_STR 17
#define ACTION_SCA_TLU 18
#define ACTION_TLD_IRC_STR 19
#define ACTION_TLD_SCR_SCA 20
#define ACTION_TLD_SCR_SCN 21
#define ACTION_TLD_ZRC_STA 22
#define ACTION_IRC_SCR_SCA 23
#define ACTION_IRC_SCR_SCN 24

/*
 * Codes used for request/response messages.  See page 27 of RFC1661.
 */
#define CODE_CONF_REQ 1
#define CODE_CONF_ACK 2
#define CODE_CONF_NAK 3
#define CODE_CONF_REJ 4
#define CODE_TERM_REQ 5
#define CODE_TERM_ACK 6
#define CODE_CODE_REJ 7
#define CODE_PROTOCOL_REJ 8
#define CODE_ECHO_REQ 9
#define CODE_ECHO_REP 10
#define CODE_DISCARD_REQ 11

/*
 * Transmission retry defaults.
 */
#define RESTART_MAX_CONFIGURE 10
#define RESTART_MAX_TERMINATE 2
#define RESTART_MAX_FAILURE 5

/*
 * Timeout value.
 */
#define TIMEOUT_TICKS (3 * TICK_RATE)

/*
 * State transition table.
 */
struct transition {
	u8_t t_action;			/* Action required to make the transition */
	u8_t t_next_state;		/* State we move to */
};

/*
 * State transition table.
 */
static struct transition ppp_trans[16][10] = {
	{
		/*
		 * Up event.
		 */
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_IRC_SCR, STATE_REQ_SENT},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_ACK_RCVD},
		{ACTION_NONE, STATE_ACK_SENT},
		{ACTION_NONE, STATE_OPENED}
	},
	{
		/*
		 * Down event.
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_TLS, STATE_STARTING},
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_TLD, STATE_STARTING}
	},
	{
		/*
		 * Open event.
		 */
		{ACTION_TLS, STATE_STARTING},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_IRC_SCR, STATE_REQ_SENT},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_ACK_RCVD},
		{ACTION_NONE, STATE_ACK_SENT},
		{ACTION_NONE, STATE_OPENED}
	},
	{
		/*
		 * Close event.
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_TLF, STATE_INITIAL},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_IRC_STR, STATE_CLOSING},
		{ACTION_IRC_STR, STATE_CLOSING},
		{ACTION_IRC_STR, STATE_CLOSING},
		{ACTION_TLD_IRC_STR, STATE_CLOSING}
	},
	{
		/*
		 * Timeout event (TO+).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_STR, STATE_CLOSING},
		{ACTION_STR, STATE_STOPPING},
		{ACTION_SCR, STATE_REQ_SENT},
		{ACTION_SCR, STATE_REQ_SENT},
		{ACTION_SCR, STATE_ACK_SENT},
		{ACTION_NONE, STATE_OPENED}
	},
	{
		/*
		 * Timeout event (TO-).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_TLF, STATE_CLOSED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_NONE, STATE_OPENED}
	},
	{
		/*
		 * Receive-configure-request event (RCR+).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_STA, STATE_CLOSED},
		{ACTION_IRC_SCR_SCA, STATE_ACK_SENT},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_SCA, STATE_ACK_SENT},
		{ACTION_SCA_TLU, STATE_OPENED},
		{ACTION_SCA, STATE_ACK_SENT},
		{ACTION_TLD_SCR_SCA, STATE_ACK_SENT}
	},
	{
		/*
		 * Receive-configure-request event (RCR-).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_STA, STATE_CLOSED},
		{ACTION_IRC_SCR_SCN, STATE_REQ_SENT},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_SCN, STATE_REQ_SENT},
		{ACTION_SCN, STATE_ACK_RCVD},
		{ACTION_SCN, STATE_REQ_SENT},
		{ACTION_TLD_SCR_SCN, STATE_REQ_SENT}
	},
	{
		/*
		 * Receive-configure-ack event (RCA).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_STA, STATE_CLOSED},
		{ACTION_STA, STATE_STOPPED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_IRC, STATE_ACK_RCVD},
		{ACTION_SCR, STATE_REQ_SENT},
		{ACTION_IRC_TLU, STATE_OPENED},
		{ACTION_TLD_SCR, STATE_REQ_SENT}
	},
	{
		/*
		 * Receive-configure-nak/rej event (RCN).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_STA, STATE_CLOSED},
		{ACTION_STA, STATE_STOPPED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_IRC_SCR, STATE_REQ_SENT},
		{ACTION_SCR, STATE_REQ_SENT},
		{ACTION_IRC_SCR, STATE_ACK_SENT},
		{ACTION_TLD_SCR, STATE_REQ_SENT}
	},
	{
		/*
		 * Receive-terminate-request event (RTR).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_STA, STATE_CLOSED},
		{ACTION_STA, STATE_STOPPED},
		{ACTION_STA, STATE_CLOSING},
		{ACTION_STA, STATE_STOPPING},
		{ACTION_STA, STATE_REQ_SENT},
		{ACTION_STA, STATE_REQ_SENT},
		{ACTION_STA, STATE_REQ_SENT},
		{ACTION_TLD_ZRC_STA, STATE_STOPPING}
	},
	{
		/*
		 * Receive-terminate-ack event (RTA).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_TLF, STATE_CLOSED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_ACK_SENT},
		{ACTION_TLD_SCR, STATE_REQ_SENT}
	},
	{
		/*
		 * Receive-unknown-code event (RUC).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_SCJ, STATE_CLOSED},
		{ACTION_SCJ, STATE_STOPPED},
		{ACTION_SCJ, STATE_CLOSING},
		{ACTION_SCJ, STATE_STOPPING},
		{ACTION_SCJ, STATE_REQ_SENT},
		{ACTION_SCJ, STATE_ACK_RCVD},
		{ACTION_SCJ, STATE_ACK_SENT},
		{ACTION_SCJ, STATE_OPENED}
	},
	{
		/*
		 * Receive-code-reject or receive-protocol-reject event (RXJ+).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_ACK_SENT},
		{ACTION_NONE, STATE_OPENED}
	},
	{
		/*
		 * Receive-code-reject or receive-protocol-reject event (RXJ-).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_TLF, STATE_CLOSED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_CLOSED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLF, STATE_STOPPED},
		{ACTION_TLD_IRC_STR, STATE_STOPPING}
	},
	{
		/*
		 * Receive-echo-request, receive-echo-reply or receive-discard-request event (RXR).
		 */
		{ACTION_NONE, STATE_INITIAL},
		{ACTION_NONE, STATE_STARTING},
		{ACTION_NONE, STATE_CLOSED},
		{ACTION_NONE, STATE_STOPPED},
		{ACTION_NONE, STATE_CLOSING},
		{ACTION_NONE, STATE_STOPPING},
		{ACTION_NONE, STATE_REQ_SENT},
		{ACTION_NONE, STATE_ACK_RCVD},
		{ACTION_NONE, STATE_ACK_SENT},
		{ACTION_SER, STATE_OPENED}
	}
};

static void handle_event(struct ppp_instance *pi, u8_t event);

/*
 * lcp_send_netbuf()
 */
static void lcp_send_netbuf(struct ppp_instance *pi, struct netbuf *nb)
{
	struct ppp_ahdlc_server *pas;
	u16_t *protocol;
	
	pas = pi->pi_server;
	ppp_ahdlc_server_ref(pas);
	spinlock_unlock(&pi->pi_lock);

	nb->nb_datalink_membuf = membuf_alloc(sizeof(u16_t), NULL);
	nb->nb_datalink = nb->nb_datalink_membuf;
	nb->nb_datalink_size = sizeof(u16_t);

	protocol = (u16_t *)nb->nb_datalink;
	*protocol = hton16(0xc021);

	pas->pas_send(pas, nb);

	ppp_ahdlc_server_deref(pas);
	
	spinlock_lock(&pi->pi_lock);
}

/*
 * lcp_tick_timeout()
 *	Callback function for the timeout timer.
 */
void lcp_tick_timeout(void *inst)
{
	struct ppp_instance *pi;
	
	pi = (struct ppp_instance *)inst;

	spinlock_lock(&pi->pi_lock);
		
	pi->pi_ident--;
	handle_event(pi, (pi->pi_restart_counter != 0) ? EVENT_TO_P : EVENT_TO_M);
		
	spinlock_unlock(&pi->pi_lock);
}

/*
 * lcp_build_req()
 */
static void lcp_build_req(struct ppp_instance *pi, struct netbuf *nb)
{
	u8_t *p, *reply_start;

	pi->pi_ident++;
	
	p = (u8_t *)nb->nb_network;
	*p++ = CODE_CONF_REQ;
	*p++ = pi->pi_ident;
	*p++ = 0;
	*p++ = 4;
	reply_start = p;

	if (pi->pi_mru_usage == PPP_USAGE_ACK) {
		u16_t mru;
		mru = hton16(pi->pi_mru);

		*p++ = 0x01;
		*p++ = 4;
		memcpy(p, &mru, sizeof(u16_t));
		p += sizeof(u16_t);
	}
	
	if (pi->pi_accm_usage == PPP_USAGE_ACK) {
		u32_t accm;
		accm = hton32(pi->pi_accm);

		*p++ = 0x02;
		*p++ = 6;
		memcpy(p, &accm, sizeof(u32_t));
		p += sizeof(u32_t);
	}
	
	nb->nb_network_size = 4 + (u16_t)(p - reply_start);
	*((u16_t *)(reply_start - 2)) = hton16(nb->nb_network_size);
}

/*
 * lcp_implement_options()
 *	Propagate any ack'd options so that they start to be used.
 */
static void lcp_implement_options(struct ppp_instance *pi)
{
	if (pi->pi_accm_usage == PPP_USAGE_ACK) {
		pi->pi_server->pas_set_accm(pi->pi_server, pi->pi_accm);
	}
}

/*
 * lcp_default_options()
 *	Set all options to a default state prior to negotiation.
 */
static void lcp_default_options(struct ppp_instance *pi)
{
	pi->pi_mru = 1500;
	pi->pi_mru_usage = PPP_USAGE_ACK;
	pi->pi_accm = 0xffffffff;
	pi->pi_accm_usage = PPP_USAGE_ACK;
	pi->pi_local_magic_number = 0;
	pi->pi_local_magic_number_usage = PPP_USAGE_REJ;
	pi->pi_remote_magic_number = 0;
	pi->pi_pfc = FALSE;
	pi->pi_pfc_usage = PPP_USAGE_REJ;
	pi->pi_acfc = FALSE;
	pi->pi_acfc_usage = PPP_USAGE_REJ;

	lcp_implement_options(pi);
}

/*
 * handle_action()
 *	Handle a specified action.
 *
 * Our instance lock is expected to be held on entry to this function.
 */
static void handle_action(struct ppp_instance *pi, u8_t action)
{
	struct netbuf *nb;
	u8_t *p;

	switch (action) {
	case ACTION_TLU:
		{
			struct ppp_client *pc;

			lcp_implement_options(pi);

			/*
			 * Strictly we should go to an authentication phase here,
			 * but we don't support that yet!
			 */
			pi->pi_phase = PHASE_NETWORK;
			
			/*
			 * Find all of the network layers that are registered and
			 * notify them that the link is up.
			 */
			pc = pi->pi_client_list;
			while (pc) {
				if (((pc->pc_protocol & 0xff00) == 0x8000) && (pc->pc_link_up)) {
					ppp_client_ref(pc);	
					spinlock_unlock(&pi->pi_lock);
					pc->pc_link_up(pc);
					spinlock_lock(&pi->pi_lock);
					ppp_client_deref(pc);
				}
				pc = pc->pc_next;
			}
		}
		break;

	case ACTION_TLD:
		lcp_default_options(pi);
		pi->pi_phase = PHASE_TERMINATE;
		break;

	case ACTION_TLS:
		pi->pi_phase = PHASE_ESTABLISH;
		break;

	case ACTION_TLF:
		pi->pi_phase = PHASE_DEAD;
		break;

	case ACTION_IRC:
		pi->pi_restart_counter = (pi->pi_phase == PHASE_TERMINATE) ? RESTART_MAX_TERMINATE : RESTART_MAX_CONFIGURE;
		break;

	case ACTION_ZRC:
		pi->pi_restart_counter = 0;
		oneshot_detach(pi->pi_timeout_timer);
		oneshot_attach(pi->pi_timeout_timer);
		break;

	case ACTION_SCR:
		if (pi->pi_restart_counter > 0) {
			pi->pi_restart_counter--;
			
			nb = netbuf_alloc();
			nb->nb_network_membuf = membuf_alloc(1500, NULL);
			nb->nb_network = nb->nb_network_membuf;
			nb->nb_network_size = 0;
			lcp_build_req(pi, nb);
			pi->pi_timeout_timer->os_ticks_left = TIMEOUT_TICKS;
			oneshot_detach(pi->pi_timeout_timer);
			oneshot_attach(pi->pi_timeout_timer);
			lcp_send_netbuf(pi, nb);
			netbuf_deref(nb);
		}
		break;

	case ACTION_SCA:
		lcp_send_netbuf(pi, pi->pi_send);
		netbuf_deref(pi->pi_send);
		break;

	case ACTION_SCN:
		lcp_send_netbuf(pi, pi->pi_send);
		netbuf_deref(pi->pi_send);
		break;

	case ACTION_STR:
		if (pi->pi_restart_counter > 0) {
			pi->pi_restart_counter--;
			
			nb = netbuf_alloc();
			nb->nb_network_membuf = membuf_alloc(4, NULL);
			nb->nb_network = nb->nb_network_membuf;
			nb->nb_network_size = 4;
			p = (u8_t *)nb->nb_network;
			*p++ = CODE_TERM_REQ;
			*p++ = pi->pi_ident++;
			*p++ = 0;
			*p++ = 4;
			pi->pi_timeout_timer->os_ticks_left = TIMEOUT_TICKS;
			oneshot_detach(pi->pi_timeout_timer);
			oneshot_attach(pi->pi_timeout_timer);
			lcp_send_netbuf(pi, nb);
			netbuf_deref(nb);
		}
		break;

	case ACTION_STA:
		lcp_send_netbuf(pi, pi->pi_send);
		netbuf_deref(pi->pi_send);
		break;

	case ACTION_SCJ:
// Send a code reject.
		break;

	case ACTION_SER:
		lcp_send_netbuf(pi, pi->pi_send);
		netbuf_deref(pi->pi_send);
		break;

	case ACTION_TLD_SCR:
		handle_action(pi, ACTION_TLD);
		handle_action(pi, ACTION_SCR);
		break;

	case ACTION_IRC_TLU:
		handle_action(pi, ACTION_IRC);
		handle_action(pi, ACTION_TLU);
		break;

	case ACTION_IRC_SCR:
		handle_action(pi, ACTION_IRC);
		handle_action(pi, ACTION_SCR);
		break;

	case ACTION_IRC_STR:
		handle_action(pi, ACTION_IRC);
		handle_action(pi, ACTION_STR);
		break;
	
	case ACTION_SCA_TLU:
		handle_action(pi, ACTION_SCA);
		handle_action(pi, ACTION_TLU);
		break;
	
	case ACTION_TLD_IRC_STR:
		handle_action(pi, ACTION_TLD);
		handle_action(pi, ACTION_IRC_STR);
		break;
	
	case ACTION_TLD_SCR_SCA:
		handle_action(pi, ACTION_TLD_SCR);
		handle_action(pi, ACTION_SCA);
		break;
	
	case ACTION_TLD_SCR_SCN:
		handle_action(pi, ACTION_TLD_SCR);
		handle_action(pi, ACTION_SCN);
		break;

	case ACTION_TLD_ZRC_STA:
		handle_action(pi, ACTION_TLD);
		handle_action(pi, ACTION_ZRC);
		handle_action(pi, ACTION_STA);
		break;

	case ACTION_IRC_SCR_SCA:
		handle_action(pi, ACTION_IRC_SCR);
		handle_action(pi, ACTION_SCA);
		break;

	case ACTION_IRC_SCR_SCN:
		handle_action(pi, ACTION_IRC_SCR);
		handle_action(pi, ACTION_SCN);
		break;
	}
}

/*
 * handle_event()
 *	Handle a specified event.
 */
static void handle_event(struct ppp_instance *pi, u8_t event)
{
	struct transition *t;

	/*
	 * Take the action required to do the state transition.
	 */
	t = &ppp_trans[event][pi->pi_state];
	handle_action(pi, t->t_action);
	
	/*
	 * If we've just moved to a state that doesn't have the restart timer
	 * running then we need to stop it.  RFC1661 page 13.
	 */
	if ((t->t_next_state <= STATE_STOPPED) || (t->t_next_state == STATE_OPENED)) {
		oneshot_detach(pi->pi_timeout_timer);
	}
	
	/*
	 * Look up the transition actions and the next state to which we need
	 * to move, given the event that has been notified.
	 */
	pi->pi_state = t->t_next_state;
}

/*
 * frame_rej()
 */
static u8_t *frame_rej(struct ppp_instance *pi, u8_t *p, u8_t *reply_start, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pi->pi_parse_state < PPP_USAGE_REJ) {
		*(reply_start - 4) = CODE_CONF_REJ;
		p = reply_start;
		pi->pi_parse_state = PPP_USAGE_REJ;
	}
	*p++ = opt;
	*p++ = optsz;
	memcpy(p, optbuf, (optsz - 2));
	p += (optsz - 2);

	return p;
}

/*
 * frame_nak()
 */
static u8_t *frame_nak(struct ppp_instance *pi, u8_t *p, u8_t *reply_start, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pi->pi_parse_state < PPP_USAGE_NAK) {
		*(reply_start - 4) = CODE_CONF_NAK;
		p = reply_start;
		pi->pi_parse_state = PPP_USAGE_NAK;
	}

	if (pi->pi_parse_state <= PPP_USAGE_NAK) {
		*p++ = opt;
		*p++ = optsz;
		memcpy(p, optbuf, (optsz - 2));
		p += (optsz - 2);
	}
	
	return p;
}

/*
 * frame_ack()
 */
static u8_t *frame_ack(struct ppp_instance *pi, u8_t *p, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pi->pi_parse_state <= PPP_USAGE_ACK) {
		*p++ = opt;
		*p++ = optsz;
		memcpy(p, optbuf, (optsz - 2));
		p += (optsz - 2);
	}
	
	return p;
}

/*
 * lcp_check_request()
 *	Check a request's options and decide how to proceed.
 */
static void lcp_check_request(struct ppp_instance *pi, u8_t *buf, u16_t sz, u8_t id)
{
	u8_t opt;
	u8_t optsz;
	u8_t *p, *reply_start;

	pi->pi_parse_state = PPP_USAGE_ACK;
	p = (u8_t *)pi->pi_send->nb_network;
	*p++ = CODE_CONF_ACK;
	*p++ = id;
	*p++ = 0;
	*p++ = 4;
	reply_start = p;

	while (sz >= 2) {
		opt = *buf++;
		optsz = *buf++;
		if ((optsz > sz) || (optsz < 2)) {
			p = frame_rej(pi, p, reply_start, opt, 2, buf);
			goto finish;
		}
	
		switch (opt) {
		case 0x01:
			{
				u16_t mru, nak_mru;
				nak_mru = hton16(pi->pi_mru);
				
				if (optsz == 4) {
					mru = hton16(*((u16_t *)buf));
					if ((mru >= 64) || (mru <= 1500)) {
						p = frame_ack(pi, p, opt, optsz, buf);
						pi->pi_mru = mru;
						break;
					}
				}
			
				p = frame_nak(pi, p, reply_start, opt, optsz, &nak_mru);
			}
			break;
				
		case 0x02:
			{
				u32_t accm, nak_accm;
				nak_accm = hton32(0xff0f0f0f);

				if (optsz == 6) {
					accm = hton32(*((u32_t *)buf));
					if ((accm & 0xff0f0f0f) == 0xff0f0f0f) {
						p = frame_ack(pi, p, opt, optsz, buf);
						pi->pi_accm = accm;
						break;
					}
				}
			
				p = frame_nak(pi, p, reply_start, opt, optsz, &nak_accm);
			}
			break;
				
		default:
			p = frame_rej(pi, p, reply_start, opt, optsz, buf);
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}

finish:
	pi->pi_send->nb_network_size = 4 + (u16_t)(p - reply_start);
	*((u16_t *)(reply_start - 2)) = hton16(pi->pi_send->nb_network_size);
}

/*
 * lcp_check_ack()
 *	Check a configure-ack's options and decide how to proceed.
 */
static void lcp_check_ack(struct ppp_instance *pi, u8_t *buf, u16_t sz)
{
	u8_t opt;
	u8_t optsz;

	while (sz >= 2) {
		opt = *buf++;
		optsz = *buf++;
		if ((optsz > sz) || (optsz < 2)) {
			return;
		}
	
		switch (opt) {
		case 0x01:
			{
				u16_t mru;
				
				if (optsz == 4) {
					mru = hton16(*((u16_t *)buf));
					if ((mru >= 64) || (mru <= 1500)) {
						pi->pi_mru = mru;
						pi->pi_mru_usage = PPP_USAGE_ACK;
						break;
					} else {
						pi->pi_mru_usage = PPP_USAGE_NAK;
					}
				}
			}
			break;
				
		case 0x02:
			{
				u32_t accm;

				if (optsz == 6) {
					accm = hton32(*((u32_t *)buf));
					if ((accm & 0xff0f0f0f) == 0xff0f0f0f) {
						pi->pi_accm = accm;
						pi->pi_accm_usage = PPP_USAGE_ACK;
						break;
					} else {
						pi->pi_accm_usage = PPP_USAGE_NAK;
					}
				}
			}
			break;
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}
}

/*
 * lcp_check_reject()
 *	Check a configure-reject's options and decide how to proceed.
 */
static void lcp_check_reject(struct ppp_instance *pi, u8_t *buf, u16_t sz)
{
	u8_t opt;
	u8_t optsz;

	while (sz >= 2) {
		opt = *buf++;
		optsz = *buf++;
		if ((optsz > sz) || (optsz < 2)) {
			return;
		}
	
		switch (opt) {
		case 0x01:
			pi->pi_mru_usage = PPP_USAGE_REJ;
			break;
				
		case 0x02:
			pi->pi_accm_usage = PPP_USAGE_REJ;
			break;
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}
}

/*
 * lcp_recv_netbuf()
 */
static void lcp_recv_netbuf(struct ppp_instance *pi, struct netbuf *nb)
{
	u8_t rcode;
	u8_t rid;
	u16_t rlen;
	u8_t *p;

	p = (u8_t *)nb->nb_network;
	rcode = *p++;
	rid = *p++;
	rlen = ((u16_t)(*p++) << 8);
	rlen |= ((u16_t)(*p++));

	if (rlen > nb->nb_network_size) {
		debug_print_pstr("lcp_recv_nb: rlen > sz");
		return;
	}

	spinlock_lock(&pi->pi_lock);
	
	switch (rcode) {
	case CODE_CONF_REQ:
		pi->pi_send = netbuf_alloc();
		pi->pi_send->nb_network_membuf = membuf_alloc(1500, NULL);
		pi->pi_send->nb_network = pi->pi_send->nb_network_membuf;
		pi->pi_send->nb_network_size = 0;
		lcp_check_request(pi, p, (rlen - 4), rid);
		if (pi->pi_parse_state == PPP_USAGE_ACK) {
			handle_event(pi, EVENT_RCR_P);
		} else {
			handle_event(pi, EVENT_RCR_M);
		}
		break;

	case CODE_CONF_ACK:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pi->pi_ident) {
			lcp_check_ack(pi, p, (rlen - 4));
			handle_event(pi, EVENT_RCA);
		}
		break;

	case CODE_CONF_NAK:
	case CODE_CONF_REJ:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pi->pi_ident) {
			pi->pi_send = netbuf_alloc();
			pi->pi_send->nb_network_membuf = membuf_alloc(1500, NULL);
			pi->pi_send->nb_network = pi->pi_send->nb_network_membuf;
			pi->pi_send->nb_network_size = 0;
			lcp_check_reject(pi, p, (rlen - 4));
			handle_event(pi, EVENT_RCN);
		}
		break;

	case CODE_TERM_REQ:
		pi->pi_send = netbuf_alloc();
		pi->pi_send->nb_network_membuf = membuf_alloc(4, NULL);
		pi->pi_send->nb_network = pi->pi_send->nb_network_membuf;
		pi->pi_send->nb_network_size = 4;
		p = (u8_t *)pi->pi_send->nb_network;
		*p++ = CODE_TERM_ACK;
		*p++ = *(((u8_t *)nb->nb_network) + 1);
		*p++ = 0;
		*p++ = 4;
		handle_event(pi, EVENT_RTR);
		break;

	case CODE_TERM_ACK:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pi->pi_ident) {
			handle_event(pi, EVENT_RTA);
		}
		break;

	case CODE_CODE_REJ:
		/*
		 * Sanity check that it makes sense to allow rejection of the
		 * code that has been bounced.  If it doesn't then we have a
		 * pretty fatal error condition.
		 */
		{
			u8_t badcode = *p++;
			
			if (badcode <= CODE_PROTOCOL_REJ) {
				handle_event(pi, EVENT_RXJ_M);
			} else {
				handle_event(pi, EVENT_RXJ_P);
			}
		}
		break;

	case CODE_PROTOCOL_REJ:
		/*
		 * Sanity check that it makes sense to allow rejection of the
		 * protocol that has been bounced.  We can allow almost
		 * anything except rejection of LCP.
		 */
		{
			u16_t badprot = hton16(*(u16_t *)p);
			
			if (badprot == 0xc021) {
				handle_event(pi, EVENT_RXJ_M);
			} else {
				handle_event(pi, EVENT_RXJ_P);
			}
		}
		break;

	case CODE_ECHO_REQ:
		/*
		 * If we're in the opened state then ready a response to the request.
		 */
		pi->pi_send = netbuf_alloc();
		pi->pi_send->nb_network_membuf = membuf_alloc(8, NULL);
		pi->pi_send->nb_network = pi->pi_send->nb_network_membuf;
		pi->pi_send->nb_network_size = 8;
		p = (u8_t *)pi->pi_send->nb_network;
		*p++ = CODE_ECHO_REP;
		*p++ = pi->pi_ident++;
		*p++ = 0;
		*p++ = 8;
		*(u32_t *)p = hton32(pi->pi_local_magic_number);
		handle_event(pi, EVENT_RXR);

	case CODE_DISCARD_REQ:
	case CODE_ECHO_REP:
		/*
		 * We don't do anything with these at the moment - simply
		 * silently discard them.  RFC1661 says that we should handle
		 * an RXR event, but that doesn't actually do anything for
		 * these codes!
		 */
		break;

	default:
		handle_event(pi, EVENT_RUC);
	}

	spinlock_unlock(&pi->pi_lock);
}

/*
 * ppp_recv_netbuf()
 */
void ppp_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ppp_ahdlc_client *pac;
	struct ppp_instance *pi;
	struct ppp_client *pc;
	u16_t *protocol;

	pac = (struct ppp_ahdlc_client *)clnt;
	pi = (struct ppp_instance *)pac->pac_instance;
		
	protocol = (u16_t *)nb->nb_datalink;
	
	nb->nb_network = protocol + 1;
	nb->nb_network_size = nb->nb_datalink_size - sizeof(u16_t);
	nb->nb_datalink_size = sizeof(u16_t);
		
	/*
	 * Check if this is an LCP packet.  If it is then handle it as a
	 * special case.
	 */
	if (hton16(*protocol) == 0xc021) {
		lcp_recv_netbuf(pi, nb);
		return;
	}
	
	/*
	 * Route the netbuf to the appropriate network layer.
	 */
	spinlock_lock(&pi->pi_lock);
	
	pc = pi->pi_client_list;
	while (pc && (pc->pc_protocol != hton16(*protocol))) {
		pc = pc->pc_next;
	}

	if (pc) {
		ppp_client_ref(pc);	
		spinlock_unlock(&pi->pi_lock);
		pc->pc_recv(pc, nb);
		ppp_client_deref(pc);
	} else {
		/*
		 * If we don't know how to handle this protocol then issue
		 * a protocol reject message.
		 */
		u8_t *p;
		u16_t len;

		len = nb->nb_network_size;
		if (len + 16 > pi->pi_mru) {
			len = pi->pi_mru - 16;
		}

		pi->pi_send = netbuf_alloc();
		pi->pi_send->nb_network_membuf = membuf_alloc(1500, NULL);
		pi->pi_send->nb_network = pi->pi_send->nb_network_membuf;
		pi->pi_send->nb_network_size = 0;
		p = (u8_t *)pi->pi_send->nb_network;
		*p++ = CODE_PROTOCOL_REJ;
		*p++ = *(((u8_t *)nb->nb_network) + 1);
		*p++ = (u8_t)(((len + 6) >> 8) & 0xff);
		*p++ = (u8_t)((len + 6) & 0xff);
		*p++ = *((u8_t *)protocol);
		*p++ = *(((u8_t *)protocol) + 1);
		memcpy(p, nb->nb_network, len);
		pi->pi_send->nb_network_size = len + 6;
		
		lcp_send_netbuf(pi, pi->pi_send);
		netbuf_deref(pi->pi_send);
		
		spinlock_unlock(&pi->pi_lock);
	}
	
}

/*
 * ppp_send_netbuf()
 */
void ppp_send_netbuf(void *srv, struct netbuf *nb)
{
	struct ppp_server *ps;
	struct ppp_instance *pi;
	u16_t *protocol;
	struct ppp_ahdlc_server *pas;
		
	ps = (struct ppp_server *)srv;
	pi = ps->ps_instance;
	
	spinlock_lock(&pi->pi_lock);
	pas = pi->pi_server;
	ppp_ahdlc_server_ref(pas);
	spinlock_unlock(&pi->pi_lock);

	nb->nb_datalink_membuf = membuf_alloc(sizeof(u16_t), NULL);
	nb->nb_datalink = nb->nb_datalink_membuf;
	nb->nb_datalink_size = sizeof(u16_t);

	protocol = (u16_t *)nb->nb_datalink;
	*protocol = hton16(ps->ps_protocol);

	pas->pas_send(pas, nb);

	ppp_ahdlc_server_deref(pas);
}

/*
 * ppp_server_alloc()
 *	Allocate an PPP server structure.
 */
struct ppp_server *ppp_server_alloc(void)
{
	struct ppp_server *ps;
	
	ps = (struct ppp_server *)membuf_alloc(sizeof(struct ppp_server), NULL);
        ps->ps_send = ppp_send_netbuf;

	return ps;
}

/*
 * ppp_client_alloc()
 *	Allocate an PPP client structure.
 */
struct ppp_client *ppp_client_alloc(void)
{
	struct ppp_client *pc;
	
	pc = (struct ppp_client *)membuf_alloc(sizeof(struct ppp_client), NULL);
        pc->pc_recv = NULL;
	pc->pc_link_up = NULL;
	pc->pc_link_down = NULL;
	pc->pc_protocol = 0;
	pc->pc_instance = NULL;
		
	return pc;
}

/*
 * ppp_client_attach()
 *	Attach a client to the PPP driver.
 */
struct ppp_server *ppp_client_attach(struct ppp_instance *pi, struct ppp_client *pc)
{
        struct ppp_client *p;
        struct ppp_server *ps = NULL;
        	
	spinlock_lock(&pi->pi_lock);

	/*
	 * Walk our list of clients and check that we can do this request.
	 */
	p = pi->pi_client_list;
	while (p && (p->pc_protocol != pc->pc_protocol)) {
		p = p->pc_next;
	}

	if (!p) {
		ps = ppp_server_alloc();		
		ps->ps_protocol = pc->pc_protocol;
		ps->ps_instance = pi;
		ppp_instance_ref(pi);

		pc->pc_server = ps;
		pc->pc_next = pi->pi_client_list;
		pi->pi_client_list = pc;
		ppp_client_ref(pc);
	}
				
	spinlock_unlock(&pi->pi_lock);

	return ps;
}

/*
 * ppp_client_detach()
 *	Detach a client from the PPP driver.
 */
void ppp_client_detach(struct ppp_instance *pi, struct ppp_server *ps)
{
        struct ppp_client *pc;
        struct ppp_client **pcprev;
	
	spinlock_lock(&pi->pi_lock);

	/*
	 * Walk our list of clients and find the one we're to remove.
	 */
	pc = pi->pi_client_list;
	pcprev = &pi->pi_client_list;
	while (pc) {
		if (pc->pc_server == ps) {
			*pcprev = pc->pc_next;
			ppp_server_deref(ps);
			ppp_instance_deref(pi);
			ppp_client_deref(pc);
			break;
		}
	
		pcprev = &pc->pc_next;
		pc = pc->pc_next;
	}
		
	spinlock_unlock(&pi->pi_lock);
}

/*
 * ppp_instance_alloc()
 */
struct ppp_instance *ppp_instance_alloc(struct ppp_ahdlc_instance *pai)
{
	struct ppp_instance *pi;
	struct ppp_ahdlc_client *pac;

	pi = (struct ppp_instance *)membuf_alloc(sizeof(struct ppp_instance), NULL);
	pi->pi_state = STATE_INITIAL;
	pi->pi_phase = PHASE_DEAD;
	pi->pi_ident = 0;
	pi->pi_timeout_timer = oneshot_alloc();
	pi->pi_timeout_timer->os_callback = lcp_tick_timeout;
	pi->pi_timeout_timer->os_arg = pi;
	pi->pi_send = NULL;
	pi->pi_client_attach = ppp_client_attach;
	pi->pi_client_detach = ppp_client_detach;
	pi->pi_client_list = NULL;
	pi->pi_server = NULL;
	spinlock_init(&pi->pi_lock, 0x20);

	/*
	 * Attach this handler to a PPP framing service.
	 */
	pac = ppp_ahdlc_client_alloc();
	pac->pac_recv = ppp_recv_netbuf;
	pac->pac_instance = pi;
	ppp_instance_ref(pi);
	
	pi->pi_server = pai->pai_client_attach(pai, pac);
	ppp_ahdlc_client_deref(pac);

// XXX - this is not correct, however it works for testing purposes!
	spinlock_lock(&pi->pi_lock);
	lcp_default_options(pi);
	handle_event(pi, EVENT_UP);
	handle_event(pi, EVENT_OPEN);
	spinlock_unlock(&pi->pi_lock);
	
	return pi;
}
