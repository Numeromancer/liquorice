/*
 * ppp_ip.c
 *	IP over PPP support
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
#include "heap.h"
#include "membuf.h"
#include "netbuf.h"
#include "timer.h"
#include "oneshot.h"
#include "ppp_ahdlc.h"
#include "ppp.h"
#include "ip_datalink.h"
#include "ppp_ip.h"
#include "ip.h"

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

static void handle_event(struct ppp_ip_instance *pii, u8_t event);

/*
 * ipcp_send_netbuf()
 */
static void ipcp_send_netbuf(struct ppp_ip_instance *pii, struct netbuf *nb)
{
	struct ppp_server *ps;
	struct ip_datalink_instance *idi;

	idi = (struct ip_datalink_instance *)pii;
	ps = pii->pii_ipcp_server;
	ppp_server_ref(ps);

	spinlock_unlock(&idi->idi_lock);
	
	ps->ps_send(ps, nb);
	
	ppp_server_deref(ps);

	spinlock_lock(&idi->idi_lock);
}

/*
 * ipcp_tick_timeout()
 *	Callback function for the timeout timer.
 */
void ipcp_tick_timeout(void *inst)
{
	struct ppp_ip_instance *pii;
	struct ip_datalink_instance *idi;
	
	pii = (struct ppp_ip_instance *)inst;
	idi = (struct ip_datalink_instance *)pii;

	spinlock_lock(&idi->idi_lock);
		
	pii->pii_ident--;
	handle_event(pii, (pii->pii_restart_counter != 0) ? EVENT_TO_P : EVENT_TO_M);
		
	spinlock_unlock(&idi->idi_lock);
}

/*
 * ipcp_build_req()
 */
static void ipcp_build_req(struct ppp_ip_instance *pii, struct netbuf *nb)
{
	u8_t *p, *reply_start;

	pii->pii_ident++;
	
	p = (u8_t *)nb->nb_network;
	*p++ = CODE_CONF_REQ;
	*p++ = pii->pii_ident;
	*p++ = 0;
	*p++ = 4;
	reply_start = p;

	if (pii->pii_local_ip_addr_usage == PPP_USAGE_ACK) {
		u32_t addr;
		addr = hton32(pii->pii_local_ip_addr);

		*p++ = 0x03;
		*p++ = 6;
		memcpy(p, &addr, sizeof(u32_t));
		p += sizeof(u32_t);
	}
	
	nb->nb_network_size = 4 + (u16_t)(p - reply_start);
	*((u16_t *)(reply_start - 2)) = hton16(nb->nb_network_size);
}

/*
 * ipcp_implement_options()
 *	Propagate any ack'd options so that they start to be used.
 */
static void ipcp_implement_options(struct ppp_ip_instance *pii)
{
}

/*
 * ipcp_default_options()
 *	Set all options to a safe default state prior to (re-)negotiation.
 */
static void ipcp_default_options(struct ppp_ip_instance *pii)
{
}

/*
 * handle_action()
 *	Handle a specified action.
 */
static void handle_action(struct ppp_ip_instance *pii, u8_t action)
{
	struct netbuf *nb;
	u8_t *p;

	switch (action) {
	case ACTION_TLU:
		ipcp_implement_options(pii);
		pii->pii_phase = PHASE_NETWORK;
		break;

	case ACTION_TLD:
		ipcp_default_options(pii);
		pii->pii_phase = PHASE_TERMINATE;
		break;

	case ACTION_TLS:
		pii->pii_phase = PHASE_ESTABLISH;
		break;

	case ACTION_TLF:
		pii->pii_phase = PHASE_DEAD;
		break;

	case ACTION_IRC:
		pii->pii_restart_counter = (pii->pii_phase == PHASE_TERMINATE) ? RESTART_MAX_TERMINATE : RESTART_MAX_CONFIGURE;
		break;

	case ACTION_ZRC:
		pii->pii_restart_counter = 0;
		oneshot_detach(pii->pii_timeout_timer);
		oneshot_attach(pii->pii_timeout_timer);
		break;

	case ACTION_SCR:
		if (pii->pii_restart_counter > 0) {
			pii->pii_restart_counter--;
			
			nb = netbuf_alloc();
			nb->nb_network_membuf = membuf_alloc(1500, NULL);
			nb->nb_network = nb->nb_network_membuf;
			nb->nb_network_size = 0;
			ipcp_build_req(pii, nb);
			pii->pii_timeout_timer->os_ticks_left = TIMEOUT_TICKS;
			oneshot_detach(pii->pii_timeout_timer);
			oneshot_attach(pii->pii_timeout_timer);
			ipcp_send_netbuf(pii, nb);
			netbuf_deref(nb);
		}
		break;

	case ACTION_SCA:
		ipcp_send_netbuf(pii, pii->pii_send);
		netbuf_deref(pii->pii_send);
		break;

	case ACTION_SCN:
		ipcp_send_netbuf(pii, pii->pii_send);
		netbuf_deref(pii->pii_send);
		break;

	case ACTION_STR:
		if (pii->pii_restart_counter > 0) {
			pii->pii_restart_counter--;
			
			nb = netbuf_alloc();
			nb->nb_network_membuf = membuf_alloc(4, NULL);
			nb->nb_network = nb->nb_network_membuf;
			nb->nb_network_size = 4;
			p = (u8_t *)nb->nb_network;
			*p++ = CODE_TERM_REQ;
			*p++ = pii->pii_ident++;
			*p++ = 0;
			*p++ = 4;
			pii->pii_timeout_timer->os_ticks_left = TIMEOUT_TICKS;
			oneshot_detach(pii->pii_timeout_timer);
			oneshot_attach(pii->pii_timeout_timer);
			ipcp_send_netbuf(pii, nb);
			netbuf_deref(nb);
		}
		break;

	case ACTION_STA:
		ipcp_send_netbuf(pii, pii->pii_send);
		netbuf_deref(pii->pii_send);
		break;

	case ACTION_SCJ:
// Send a code reject.
		break;

	case ACTION_SER:
// Send an echo response.
		break;

	case ACTION_TLD_SCR:
		handle_action(pii, ACTION_TLD);
		handle_action(pii, ACTION_SCR);
		break;

	case ACTION_IRC_TLU:
		handle_action(pii, ACTION_IRC);
		handle_action(pii, ACTION_TLU);
		break;

	case ACTION_IRC_SCR:
		handle_action(pii, ACTION_IRC);
		handle_action(pii, ACTION_SCR);
		break;

	case ACTION_IRC_STR:
		handle_action(pii, ACTION_IRC);
		handle_action(pii, ACTION_STR);
		break;
	
	case ACTION_SCA_TLU:
		handle_action(pii, ACTION_SCA);
		handle_action(pii, ACTION_TLU);
		break;
	
	case ACTION_TLD_IRC_STR:
		handle_action(pii, ACTION_TLD);
		handle_action(pii, ACTION_IRC_STR);
		break;
	
	case ACTION_TLD_SCR_SCA:
		handle_action(pii, ACTION_TLD_SCR);
		handle_action(pii, ACTION_SCA);
		break;
	
	case ACTION_TLD_SCR_SCN:
		handle_action(pii, ACTION_TLD_SCR);
		handle_action(pii, ACTION_SCN);
		break;

	case ACTION_TLD_ZRC_STA:
		handle_action(pii, ACTION_TLD);
		handle_action(pii, ACTION_ZRC);
		handle_action(pii, ACTION_STA);
		break;

	case ACTION_IRC_SCR_SCA:
		handle_action(pii, ACTION_IRC_SCR);
		handle_action(pii, ACTION_SCA);
		break;

	case ACTION_IRC_SCR_SCN:
		handle_action(pii, ACTION_IRC_SCR);
		handle_action(pii, ACTION_SCN);
		break;
	}
}

/*
 * handle_event()
 *	Handle a specified event.
 */
static void handle_event(struct ppp_ip_instance *pii, u8_t event)
{
	struct transition *t;

	/*
	 * Take the action required to do the state transition.
	 */
	t = &ppp_trans[event][pii->pii_state];
	handle_action(pii, t->t_action);

	/*
	 * If we've just moved to a state that doesn't have the restart timer
	 * running then we need to stop it.  RFC1661 page 13.
	 */
	if ((t->t_next_state <= STATE_STOPPED) || (t->t_next_state == STATE_OPENED)) {
		oneshot_detach(pii->pii_timeout_timer);
	}
	
	/*
	 * Look up the transition actions and the next state to which we need
	 * to move, given the event that has been notified.
	 */
	pii->pii_state = t->t_next_state;
}

/*
 * frame_rej()
 */
static u8_t *frame_rej(struct ppp_ip_instance *pii, u8_t *p, u8_t *reply_start, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pii->pii_parse_state < PPP_USAGE_REJ) {
		*(reply_start - 4) = CODE_CONF_REJ;
		p = reply_start;
		pii->pii_parse_state = PPP_USAGE_REJ;
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
static u8_t *frame_nak(struct ppp_ip_instance *pii, u8_t *p, u8_t *reply_start, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pii->pii_parse_state < PPP_USAGE_NAK) {
		*(reply_start - 4) = CODE_CONF_NAK;
		p = reply_start;
		pii->pii_parse_state = PPP_USAGE_NAK;
	}

	if (pii->pii_parse_state <= PPP_USAGE_NAK) {
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
static u8_t *frame_ack(struct ppp_ip_instance *pii, u8_t *p, u8_t opt, u8_t optsz, void *optbuf)
{
	if (pii->pii_parse_state <= PPP_USAGE_ACK) {
		*p++ = opt;
		*p++ = optsz;
		memcpy(p, optbuf, (optsz - 2));
		p += (optsz - 2);
	}
	
	return p;
}

/*
 * ipcp_check_request()
 *	Check a request's options and decide how to proceed.
 */
static void ipcp_check_request(struct ppp_ip_instance *pii, u8_t *buf, u16_t sz, u8_t id)
{
	u8_t opt;
	u8_t optsz;
	u8_t *p, *reply_start;

	pii->pii_parse_state = PPP_USAGE_ACK;
	p = (u8_t *)pii->pii_send->nb_network;
	*p++ = CODE_CONF_ACK;
	*p++ = id;
	*p++ = 0;
	*p++ = 4;
	reply_start = p;

	while (sz >= 2) {
		opt = *buf++;
		optsz = *buf++;
		if ((optsz > sz) || (optsz < 2)) {
			p = frame_rej(pii, p, reply_start, opt, 2, buf);
			goto finish;
		}
	
		switch (opt) {
		case 0x03:
			{
				u32_t addr;
				
				if (optsz == 6) {
					addr = hton32(*((u32_t *)buf));
					p = frame_ack(pii, p, opt, optsz, buf);
					pii->pii_remote_ip_addr = addr;
				}
			}
			break;
				
		default:
			p = frame_rej(pii, p, reply_start, opt, optsz, buf);
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}

finish:
	pii->pii_send->nb_network_size = 4 + (u16_t)(p - reply_start);
	*((u16_t *)(reply_start - 2)) = hton16(pii->pii_send->nb_network_size);
}

/*
 * ipcp_check_ack()
 *	Check a configure-ack's options and decide how to proceed.
 */
static void ipcp_check_ack(struct ppp_ip_instance *pii, u8_t *buf, u16_t sz)
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
		case 0x03:
			{
				u32_t addr;
				
				if (optsz == 6) {
					addr = hton32(*((u32_t *)buf));
					pii->pii_local_ip_addr = addr;
					pii->pii_local_ip_addr_usage = PPP_USAGE_ACK;
				}
			}
			break;
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}
}

/*
 * ipcp_check_reject()
 *	Check a configure-reject's options and decide how to proceed.
 */
static void ipcp_check_reject(struct ppp_ip_instance *pii, u8_t *buf, u16_t sz)
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
		case 0x03:
			pii->pii_local_ip_addr_usage = PPP_USAGE_REJ;
			break;
		}
	
		sz -= (u16_t)optsz;
		buf += (optsz - 2);
	}
}

/*
 * ipcp_recv_netbuf()
 */
static void ipcp_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ppp_client *pc;
	struct ip_datalink_instance *idi;
	struct ppp_ip_instance *pii;
	u8_t rcode;
	u8_t rid;
	u16_t rlen;
	u8_t *p;
	
	pc = (struct ppp_client *)clnt;
	idi = (struct ip_datalink_instance *)pc->pc_instance;
	pii = (struct ppp_ip_instance *)idi;

	p = (u8_t *)nb->nb_network;
	rcode = *p++;
	rid = *p++;
	rlen = ((u16_t)(*p++) << 8);
	rlen |= ((u16_t)(*p++));

	if (rlen > nb->nb_network_size) {
		debug_print_pstr("ipcp_recv_nb: rlen > sz");
		return;
	}

	spinlock_lock(&idi->idi_lock);
	
	switch (rcode) {
	case CODE_CONF_REQ:
		pii->pii_send = netbuf_alloc();
		pii->pii_send->nb_network_membuf = membuf_alloc(1500, NULL);
		pii->pii_send->nb_network = pii->pii_send->nb_network_membuf;
		pii->pii_send->nb_network_size = 0;
		ipcp_check_request(pii, p, (rlen - 4), rid);
		if (pii->pii_parse_state == PPP_USAGE_ACK) {
			handle_event(pii, EVENT_RCR_P);
		} else {
			handle_event(pii, EVENT_RCR_M);
		}
		break;

	case CODE_CONF_ACK:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pii->pii_ident) {
			ipcp_check_ack(pii, p, (rlen - 4));
			handle_event(pii, EVENT_RCA);
		}
		break;

	case CODE_CONF_NAK:
	case CODE_CONF_REJ:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pii->pii_ident) {
			pii->pii_send = netbuf_alloc();
			pii->pii_send->nb_network_membuf = membuf_alloc(1500, NULL);
			pii->pii_send->nb_network = pii->pii_send->nb_network_membuf;
			pii->pii_send->nb_network_size = 0;
			ipcp_check_reject(pii, p, (rlen - 4));
			handle_event(pii, EVENT_RCN);
		}
		break;

	case CODE_TERM_REQ:
		pii->pii_send = netbuf_alloc();
		pii->pii_send->nb_network_membuf = membuf_alloc(4, NULL);
		pii->pii_send->nb_network = pii->pii_send->nb_network_membuf;
		pii->pii_send->nb_network_size = 4;
		p = (u8_t *)pii->pii_send->nb_network;
		*p++ = CODE_TERM_ACK;
		*p++ = *(((u8_t *)nb->nb_network) + 1);
		*p++ = 0;
		*p++ = 4;
		handle_event(pii, EVENT_RTR);
		break;

	case CODE_TERM_ACK:
		/*
		 * Check that the ID matches the request that was sent - otherwise
		 * discard silently!
		 */
		if (rid == pii->pii_ident) {
			handle_event(pii, EVENT_RTA);
		}
		break;

	case CODE_CODE_REJ:
		/*
		 * Sanity check that it makes sense to allow rejection of the
		 * code that has been bounced.  If it doesn't then it's pretty
		 * terminal as far as this layer is concerned.
		 */
		{
			u8_t badcode = *p++;
			
			if (badcode <= CODE_CODE_REJ) {
				handle_event(pii, EVENT_RXJ_M);
			} else {
				handle_event(pii, EVENT_RXJ_P);
			}
		}
		break;

	default:
		handle_event(pii, EVENT_RUC);
	}

	spinlock_unlock(&idi->idi_lock);
}

/*
 * ipcp_link_up()
 *	Notification callback used to signal that our LCP layer negotiation has completed.
 */
void ipcp_link_up(void *clnt)
{
	struct ppp_client *pc;
	struct ip_datalink_instance *idi;
	struct ppp_ip_instance *pii;

	pc = (struct ppp_client *)clnt;
	idi = (struct ip_datalink_instance *)pc->pc_instance;
	pii = (struct ppp_ip_instance *)idi;

	spinlock_lock(&idi->idi_lock);
	handle_event(pii, EVENT_UP);
// XXX - this open is not correct, however it works for testing purposes!
	handle_event(pii, EVENT_OPEN);
	spinlock_unlock(&idi->idi_lock);
}

/*
 * ppp_ip_recv_netbuf()
 */
void ppp_ip_recv_netbuf(void *clnt, struct netbuf *nb)
{
	struct ppp_client *pc;
	struct ip_datalink_instance *idi;
	struct ip_datalink_client *idc;

	pc = (struct ppp_client *)clnt;
	idi = (struct ip_datalink_instance *)pc->pc_instance;
	
	spinlock_lock(&idi->idi_lock);
	idc = idi->idi_client;
	ip_datalink_client_ref(idc);
	spinlock_unlock(&idi->idi_lock);

	idc->idc_recv(idc, nb);
	
	ip_datalink_client_deref(idc);
}

/*
 * ppp_ip_send_netbuf()
 *	Send a netbuf.
 *
 * Note that we assume that all parameters are supplied in network byte order.
 */
void ppp_ip_send_netbuf(void *srv, struct netbuf *nb)
{
	struct ip_datalink_server *ids;
	struct ppp_ip_instance *pii;
	struct ppp_server *ps;

	ids = (struct ip_datalink_server *)srv;
	pii = (struct ppp_ip_instance *)ids->ids_instance;
	ps = (struct ppp_server *)pii->pii_ip_server;

	ps->ps_send(ps, nb);
}

/*
 * ppp_ip_server_alloc()
 *	Allocate an IP/PPP server structure.
 */
struct ip_datalink_server *ppp_ip_server_alloc(void)
{
	struct ip_datalink_server *ids;
	
	ids = (struct ip_datalink_server *)membuf_alloc(sizeof(struct ip_datalink_server), NULL);
        ids->ids_send = ppp_ip_send_netbuf;
        	
	return ids;
}

/*
 * ppp_ip_client_attach()
 *	Attach a client to the IP/PPP layer.
 */
struct ip_datalink_server *ppp_ip_client_attach(struct ip_datalink_instance *idi, struct ip_datalink_client *idc)
{
	struct ip_datalink_server *ids = NULL;

	spinlock_lock(&idi->idi_lock);

	if (idi->idi_client == NULL) {
		ids = ppp_ip_server_alloc();
                ids->ids_instance = idi;
                ip_datalink_instance_ref(idi);

		idc->idc_server = ids;
		idi->idi_client = idc;
		ip_datalink_client_ref(idc);
	}
		
	spinlock_unlock(&idi->idi_lock);

	return ids;
}

/*
 * ppp_ip_instance_alloc()
 */
struct ip_datalink_instance *ppp_ip_instance_alloc(struct ppp_instance *pi)
{
	struct ip_datalink_instance *idi;
	struct ppp_ip_instance *pii;
        struct ppp_client *ippc, *ipcppc;
        		
	idi = (struct ip_datalink_instance *)membuf_alloc(sizeof(struct ppp_ip_instance), NULL);
	pii = (struct ppp_ip_instance *)idi;
	spinlock_init(&idi->idi_lock, 0x20);
	idi->idi_mru = 1500;
	idi->idi_mtu = 1500;
	idi->idi_client = NULL;
	idi->idi_client_attach = ppp_ip_client_attach;
	idi->idi_client_detach = ip_datalink_client_detach;

	pii->pii_state = STATE_INITIAL;
	pii->pii_phase = PHASE_DEAD;
	pii->pii_ident = 0;
	pii->pii_timeout_timer = oneshot_alloc();
	pii->pii_timeout_timer->os_callback = ipcp_tick_timeout;
	pii->pii_timeout_timer->os_arg = pi;
	pii->pii_local_ip_addr = 0xbe010102;
	pii->pii_local_ip_addr_usage = PPP_USAGE_ACK;

	/*
	 * Attach this IP handler to our PPP channel.
	 */
	ippc = ppp_client_alloc();
	ippc->pc_recv = ppp_ip_recv_netbuf;
	ippc->pc_protocol = 0x0021;
	ippc->pc_instance = idi;
	ip_datalink_instance_ref(idi);
	pii->pii_ip_server = pi->pi_client_attach(pi, ippc);
	ppp_client_deref(ippc);
	
	ipcppc = ppp_client_alloc();
	ipcppc->pc_recv = ipcp_recv_netbuf;
	ipcppc->pc_link_up = ipcp_link_up;
	ipcppc->pc_protocol = 0x8021;
	ipcppc->pc_instance = idi;
	ip_datalink_instance_ref(idi);
	pii->pii_ipcp_server = pi->pi_client_attach(pi, ipcppc);
	ppp_client_deref(ipcppc);

	return idi;
}
