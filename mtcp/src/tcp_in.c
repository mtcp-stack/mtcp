#include <assert.h>

#include "mos_api.h"
#include "tcp_util.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "tcp_ring_buffer.h"
#include "eventpoll.h"
#include "debug.h"
#include "timer.h"
#include "ip_in.h"
#include "tcp_rb.h"
#include "config.h"
#include "scalable_event.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define RECOVERY_AFTER_LOSS TRUE
#define SELECTIVE_WRITE_EVENT_NOTIFY TRUE
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_ESTABLISHED (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx);
/*----------------------------------------------------------------------------*/
static inline int 
FilterSYNPacket(mtcp_manager_t mtcp, uint32_t ip, uint16_t port)
{
	struct sockaddr_in *addr;

	/* TODO: This listening logic should be revised */

	/* if not listening, drop */
	if (!mtcp->listener) {
		return FALSE;
	}

	/* if not the address we want, drop */
	addr = &mtcp->listener->socket->saddr;
	if (addr->sin_port == port) {
		if (addr->sin_addr.s_addr != INADDR_ANY) {
			if (ip == addr->sin_addr.s_addr) {
				return TRUE;
			}
			return FALSE;
		} else {
			int i;

			for (i = 0; i < g_config.mos->netdev_table->num; i++) {
				if (ip == g_config.mos->netdev_table->ent[i]->ip_addr) {
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	return FALSE;
}
/*----------------------------------------------------------------------------*/
static inline int
HandleActiveOpen(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	cur_stream->rcvvar->irs = pctx->p.seq;
	cur_stream->snd_nxt = pctx->p.ack_seq;
	cur_stream->sndvar->peer_wnd = pctx->p.window;
	cur_stream->rcvvar->snd_wl1 = cur_stream->rcvvar->irs - 1;
	cur_stream->rcv_nxt = cur_stream->rcvvar->irs + 1;
	cur_stream->rcvvar->last_ack_seq = pctx->p.ack_seq;
	ParseTCPOptions(cur_stream, pctx->p.cur_ts, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);
	cur_stream->sndvar->cwnd = ((cur_stream->sndvar->cwnd == 1)? 
			(cur_stream->sndvar->mss * 2): cur_stream->sndvar->mss);
	cur_stream->sndvar->ssthresh = cur_stream->sndvar->mss * 10;
	if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
		UpdateRetransmissionTimer(mtcp, cur_stream, pctx->p.cur_ts);

	return TRUE;
}
/*----------------------------------------------------------------------------*/
/* ValidateSequence: validates sequence number of the segment                 */
/* Return: TRUE if acceptable, FALSE if not acceptable                        */
/*----------------------------------------------------------------------------*/
static inline int
ValidateSequence(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	/* Protect Against Wrapped Sequence number (PAWS) */
	if (!tcph->rst && cur_stream->saw_timestamp) {
		struct tcp_timestamp ts;
		
		if (!ParseTCPTimestamp(cur_stream, &ts, 
				(uint8_t *)tcph + TCP_HEADER_LEN, 
				(tcph->doff << 2) - TCP_HEADER_LEN)) {
			/* if there is no timestamp */
			/* TODO: implement here */
			TRACE_DBG("No timestamp found.\n");
			return FALSE;
		}

		/* RFC1323: if SEG.TSval < TS.Recent, drop and send ack */
		if (TCP_SEQ_LT(ts.ts_val, cur_stream->rcvvar->ts_recent)) {
			/* TODO: ts_recent should be invalidated 
					 before timestamp wraparound for long idle flow */
			TRACE_DBG("PAWS Detect wrong timestamp. "
					"seq: %u, ts_val: %u, prev: %u\n", 
					pctx->p.seq, ts.ts_val, cur_stream->rcvvar->ts_recent);
			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
			return FALSE;
		} else {
			/* valid timestamp */
			if (TCP_SEQ_GT(ts.ts_val, cur_stream->rcvvar->ts_recent)) {
				TRACE_TSTAMP("Timestamp update. cur: %u, prior: %u "
					"(time diff: %uus)\n", 
					ts.ts_val, cur_stream->rcvvar->ts_recent, 
					TS_TO_USEC(pctx->p.cur_ts - cur_stream->rcvvar->ts_last_ts_upd));
				cur_stream->rcvvar->ts_last_ts_upd = pctx->p.cur_ts;
			}

			cur_stream->rcvvar->ts_recent = ts.ts_val;
			cur_stream->rcvvar->ts_lastack_rcvd = ts.ts_ref;
		}
	}

	/* TCP sequence validation */
	if (!TCP_SEQ_BETWEEN(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt, 
				cur_stream->rcv_nxt + cur_stream->rcvvar->rcv_wnd)) {

		/* if RST bit is set, ignore the segment */
		if (tcph->rst)
			return FALSE;

		if (cur_stream->state == TCP_ST_ESTABLISHED) {
			/* check if it is to get window advertisement */
			if (pctx->p.seq + 1 == cur_stream->rcv_nxt) {
				TRACE_DBG("Window update request. (seq: %u, rcv_wnd: %u)\n", 
						pctx->p.seq, cur_stream->rcvvar->rcv_wnd);
				cur_stream->actions |= MOS_ACT_SEND_ACK_AGG;
				return FALSE;

			}

			if (TCP_SEQ_LEQ(pctx->p.seq, cur_stream->rcv_nxt)) {
				cur_stream->actions |= MOS_ACT_SEND_ACK_AGG;
			} else {
				cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
			}
		} else {
			if (cur_stream->state == TCP_ST_TIME_WAIT) {
				TRACE_DBG("Stream %d: tw expire update to %u\n", 
						cur_stream->id, cur_stream->rcvvar->ts_tw_expire);
				AddtoTimewaitList(mtcp, cur_stream, pctx->p.cur_ts);
			}
			cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		}
		return FALSE;
	}

	return TRUE;
}
/*----------------------------------------------------------------------------*/
static inline void 
NotifyConnectionReset(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	TRACE_DBG("Stream %d: Notifying connection reset.\n", cur_stream->id);
	/* TODO: implement this function */
	/* signal to user "connection reset" */
}
/*----------------------------------------------------------------------------*/
static inline int 
ProcessRST(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	/* TODO: we need reset validation logic */
	/* the sequence number of a RST should be inside window */
	/* (in SYN_SENT state, it should ack the previous SYN */

	TRACE_DBG("Stream %d: TCP RESET (%s)\n", 
			cur_stream->id, TCPStateToString(cur_stream));
#if DUMP_STREAM
	DumpStream(mtcp, cur_stream);
#endif
	
	if (cur_stream->state <= TCP_ST_SYN_SENT) {
		/* not handled here */
		return FALSE;
	}

	if (cur_stream->state == TCP_ST_SYN_RCVD) {
		/* ACK number of last sent ACK packet == rcv_nxt + 1*/
		if (pctx->p.seq == 0 ||
#ifdef BE_RESILIENT_TO_PACKET_DROP
			pctx->p.seq == cur_stream->rcv_nxt + 1 ||
#endif
			pctx->p.ack_seq == cur_stream->snd_nxt)
		{
			cur_stream->state = TCP_ST_CLOSED_RSVD;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			cur_stream->close_reason = TCP_RESET;
			cur_stream->actions |= MOS_ACT_DESTROY;
		} else {
			RAISE_DEBUG_EVENT(mtcp, cur_stream,
				"(SYN_RCVD): Ignore invalid RST. "
				"ack_seq expected: %u, ack_seq rcvd: %u\n",
				cur_stream->rcv_nxt + 1, pctx->p.ack_seq);
		}
		return TRUE;
	}

	/* if the application is already closed the connection, 
	   just destroy the it */
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2 || 
			cur_stream->state == TCP_ST_LAST_ACK || 
			cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_TIME_WAIT) {
		cur_stream->state = TCP_ST_CLOSED_RSVD;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		cur_stream->actions |= MOS_ACT_DESTROY;
		return TRUE;
	}

	if (cur_stream->state >= TCP_ST_ESTABLISHED && 
			cur_stream->state <= TCP_ST_CLOSE_WAIT) {
		/* ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
		/* TODO: flush all the segment queues */
		//NotifyConnectionReset(mtcp, cur_stream);
	}

	if (!(cur_stream->sndvar->on_closeq || cur_stream->sndvar->on_closeq_int || 
		  cur_stream->sndvar->on_resetq || cur_stream->sndvar->on_resetq_int)) {
		//cur_stream->state = TCP_ST_CLOSED_RSVD;
		//cur_stream->actions |= MOS_ACT_DESTROY;
		cur_stream->state = TCP_ST_CLOSED_RSVD;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		cur_stream->close_reason = TCP_RESET;
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			RaiseCloseEvent(mtcp, cur_stream);
	}

	return TRUE;
}
/*----------------------------------------------------------------------------*/
inline void 
EstimateRTT(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t mrtt)
{
	/* This function should be called for not retransmitted packets */
	/* TODO: determine tcp_rto_min */
#define TCP_RTO_MIN 0
	long m = mrtt;
	uint32_t tcp_rto_min = TCP_RTO_MIN;
	struct tcp_recv_vars *rcvvar = cur_stream->rcvvar;

	if (m == 0) {
		m = 1;
	}
	if (rcvvar->srtt != 0) {
		/* rtt = 7/8 rtt + 1/8 new */
		m -= (rcvvar->srtt >> 3);
		rcvvar->srtt += m;
		if (m < 0) {
			m = -m;
			m -= (rcvvar->mdev >> 2);
			if (m > 0) {
				m >>= 3;
			}
		} else {
			m -= (rcvvar->mdev >> 2);
		}
		rcvvar->mdev += m;
		if (rcvvar->mdev > rcvvar->mdev_max) {
			rcvvar->mdev_max = rcvvar->mdev;
			if (rcvvar->mdev_max > rcvvar->rttvar) {
				rcvvar->rttvar = rcvvar->mdev_max;
			}
		}
		if (TCP_SEQ_GT(cur_stream->sndvar->snd_una, rcvvar->rtt_seq)) {
			if (rcvvar->mdev_max < rcvvar->rttvar) {
				rcvvar->rttvar -= (rcvvar->rttvar - rcvvar->mdev_max) >> 2;
			}
			rcvvar->rtt_seq = cur_stream->snd_nxt;
			rcvvar->mdev_max = tcp_rto_min;
		}
	} else {
		/* fresh measurement */
		rcvvar->srtt = m << 3;
		rcvvar->mdev = m << 1;
		rcvvar->mdev_max = rcvvar->rttvar = MAX(rcvvar->mdev, tcp_rto_min);
		rcvvar->rtt_seq = cur_stream->snd_nxt;
	}

	TRACE_RTT("mrtt: %u (%uus), srtt: %u (%ums), mdev: %u, mdev_max: %u, "
			"rttvar: %u, rtt_seq: %u\n", mrtt, mrtt * TIME_TICK, 
			rcvvar->srtt, TS_TO_MSEC((rcvvar->srtt) >> 3), rcvvar->mdev, 
			rcvvar->mdev_max, rcvvar->rttvar, rcvvar->rtt_seq);
}
/*----------------------------------------------------------------------------*/
static inline void
ProcessACK(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;
	uint32_t seq = pctx->p.seq;
	uint32_t ack_seq = pctx->p.ack_seq;
	struct tcp_send_vars *sndvar = cur_stream->sndvar;
	uint32_t cwindow, cwindow_prev;
	uint32_t rmlen;
	uint32_t snd_wnd_prev;
	uint32_t right_wnd_edge;
	uint8_t dup;

	cwindow = pctx->p.window;
	if (!tcph->syn) {
		cwindow = cwindow << sndvar->wscale_peer;
	}
	right_wnd_edge = sndvar->peer_wnd + cur_stream->rcvvar->snd_wl2;

	/* If ack overs the sending buffer, return */
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2 ||
			cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_CLOSE_WAIT || 
			cur_stream->state == TCP_ST_LAST_ACK) {
		if (sndvar->is_fin_sent && ack_seq == sndvar->fss + 1) {
			ack_seq--;
		}
	}
	
	if (TCP_SEQ_GT(ack_seq, sndvar->sndbuf->head_seq + sndvar->sndbuf->len)) {
		TRACE_DBG("Stream %d (%s): invalid acknologement. "
				"ack_seq: %u, possible max_ack_seq: %u\n", cur_stream->id, 
				TCPStateToString(cur_stream), ack_seq, 
				sndvar->sndbuf->head_seq + sndvar->sndbuf->len);
		return;
	}

#ifdef BE_RESILIENT_TO_PACKET_DROP
	if (TCP_SEQ_GT(seq + pctx->p.payloadlen, cur_stream->rcv_nxt))
		cur_stream->rcv_nxt = seq + pctx->p.payloadlen;
#endif

	/* Update window */
	if (TCP_SEQ_LT(cur_stream->rcvvar->snd_wl1, seq) ||
			(cur_stream->rcvvar->snd_wl1 == seq && 
			TCP_SEQ_LT(cur_stream->rcvvar->snd_wl2, ack_seq)) ||
			(cur_stream->rcvvar->snd_wl2 == ack_seq && 
			cwindow > sndvar->peer_wnd)) {
		cwindow_prev = sndvar->peer_wnd;
		sndvar->peer_wnd = cwindow;
		cur_stream->rcvvar->snd_wl1 = seq;
		cur_stream->rcvvar->snd_wl2 = ack_seq;
		TRACE_CLWND("Window update. "
				"ack: %u, peer_wnd: %u, snd_nxt-snd_una: %u\n", 
				ack_seq, cwindow, cur_stream->snd_nxt - sndvar->snd_una);
		if (cwindow_prev < cur_stream->snd_nxt - sndvar->snd_una && 
				sndvar->peer_wnd >= cur_stream->snd_nxt - sndvar->snd_una) {
			TRACE_CLWND("%u Broadcasting client window update! "
					"ack_seq: %u, peer_wnd: %u (before: %u), "
					"(snd_nxt - snd_una: %u)\n", 
					cur_stream->id, ack_seq, sndvar->peer_wnd, cwindow_prev, 
					cur_stream->snd_nxt - sndvar->snd_una);
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RaiseWriteEvent(mtcp, cur_stream);
		}
	}

	/* Check duplicated ack count */
	/* Duplicated ack if 
	   1) ack_seq is old
	   2) payload length is 0.
	   3) advertised window not changed.
	   4) there is outstanding unacknowledged data
	   5) ack_seq == snd_una
	 */

	dup = FALSE;
	if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
		if (ack_seq == cur_stream->rcvvar->last_ack_seq && pctx->p.payloadlen == 0) {
			if (cur_stream->rcvvar->snd_wl2 + sndvar->peer_wnd == right_wnd_edge) {
				if (cur_stream->rcvvar->dup_acks + 1 > cur_stream->rcvvar->dup_acks) {
					cur_stream->rcvvar->dup_acks++;
				}
				dup = TRUE;
			}
		}
	}
	if (!dup) {
		cur_stream->rcvvar->dup_acks = 0;
		cur_stream->rcvvar->last_ack_seq = ack_seq;
	}

	/* Fast retransmission */
	if (dup && cur_stream->rcvvar->dup_acks == 3) {
		TRACE_LOSS("Triple duplicated ACKs!! ack_seq: %u\n", ack_seq);
		if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
			TRACE_LOSS("Reducing snd_nxt from %u to %u\n", 
					cur_stream->snd_nxt, ack_seq);
#if RTM_STAT
			sndvar->rstat.tdp_ack_cnt++;
			sndvar->rstat.tdp_ack_bytes += (cur_stream->snd_nxt - ack_seq);
#endif
			if (ack_seq != sndvar->snd_una) {
				TRACE_DBG("ack_seq and snd_una mismatch on tdp ack. "
						"ack_seq: %u, snd_una: %u\n", 
						ack_seq, sndvar->snd_una);
			}
			cur_stream->snd_nxt = ack_seq;
		}

		/* update congestion control variables */
		/* ssthresh to half of min of cwnd and peer wnd */
		sndvar->ssthresh = MIN(sndvar->cwnd, sndvar->peer_wnd) / 2;
		if (sndvar->ssthresh < 2 * sndvar->mss) {
			sndvar->ssthresh = 2 * sndvar->mss;
		}
		sndvar->cwnd = sndvar->ssthresh + 3 * sndvar->mss;
		TRACE_CONG("Fast retransmission. cwnd: %u, ssthresh: %u\n", 
				sndvar->cwnd, sndvar->ssthresh);

		/* count number of retransmissions */
		if (sndvar->nrtx < TCP_MAX_RTX) {
			sndvar->nrtx++;
		} else {
			TRACE_DBG("Exceed MAX_RTX.\n");
		}

		cur_stream->actions |= MOS_ACT_SEND_DATA;

	} else if (cur_stream->rcvvar->dup_acks > 3) {
		/* Inflate congestion window until before overflow */
		if ((uint32_t)(sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
			sndvar->cwnd += sndvar->mss;
			TRACE_CONG("Dupack cwnd inflate. cwnd: %u, ssthresh: %u\n", 
					sndvar->cwnd, sndvar->ssthresh);
		}
	}

#if TCP_OPT_SACK_ENABLED
	ParseSACKOption(cur_stream, ack_seq, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);
#endif /* TCP_OPT_SACK_ENABLED */

#if RECOVERY_AFTER_LOSS
	/* updating snd_nxt (when recovered from loss) */
	if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
#if RTM_STAT
		sndvar->rstat.ack_upd_cnt++;
		sndvar->rstat.ack_upd_bytes += (ack_seq - cur_stream->snd_nxt);
#endif
		TRACE_LOSS("Updating snd_nxt from %u to %u\n", 
				cur_stream->snd_nxt, ack_seq);
		cur_stream->snd_nxt = ack_seq;
		if (sndvar->sndbuf->len == 0) {
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RemoveFromSendList(mtcp, cur_stream);
		}
	}
#endif

	/* If ack_seq is previously acked, return */
	if (TCP_SEQ_GEQ(sndvar->sndbuf->head_seq, ack_seq)) {
		return;
	}

	/* Remove acked sequence from send buffer */
	rmlen = ack_seq - sndvar->sndbuf->head_seq;
	if (rmlen > 0) {
		/* Routine goes here only if there is new payload (not retransmitted) */
		uint16_t packets;

		/* If acks new data */
		packets = rmlen / sndvar->eff_mss;
		if ((rmlen / sndvar->eff_mss) * sndvar->eff_mss > rmlen) {
			packets++;
		}
		
		/* Estimate RTT and calculate rto */
		if (cur_stream->saw_timestamp) {
			EstimateRTT(mtcp, cur_stream, 
					pctx->p.cur_ts - cur_stream->rcvvar->ts_lastack_rcvd);
			sndvar->rto = (cur_stream->rcvvar->srtt >> 3) + cur_stream->rcvvar->rttvar;
			assert(sndvar->rto > 0);
		} else {
			//TODO: Need to implement timestamp estimation without timestamp
			TRACE_RTT("NOT IMPLEMENTED.\n");
		}

		/* Update congestion control variables */
		if (cur_stream->state >= TCP_ST_ESTABLISHED) {
			if (sndvar->cwnd < sndvar->ssthresh) {
				if ((sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
					sndvar->cwnd += (sndvar->mss * packets);
				}
				TRACE_CONG("slow start cwnd: %u, ssthresh: %u\n", 
						sndvar->cwnd, sndvar->ssthresh);
			} else {
				uint32_t new_cwnd = sndvar->cwnd + 
						packets * sndvar->mss * sndvar->mss / 
						sndvar->cwnd;
				if (new_cwnd > sndvar->cwnd) {
					sndvar->cwnd = new_cwnd;
				}
				//TRACE_CONG("congestion avoidance cwnd: %u, ssthresh: %u\n", 
				//		sndvar->cwnd, sndvar->ssthresh);
			}
		}

		if (SBUF_LOCK(&sndvar->write_lock)) {
			if (errno == EDEADLK)
				perror("ProcessACK: write_lock blocked\n");
			assert(0);
		}
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			SBRemove(mtcp->rbm_snd, sndvar->sndbuf, rmlen);
		sndvar->snd_una = ack_seq;
		snd_wnd_prev = sndvar->snd_wnd;
		sndvar->snd_wnd = sndvar->sndbuf->size - sndvar->sndbuf->len;

		/* If there was no available sending window */
		/* notify the newly available window to application */
#if SELECTIVE_WRITE_EVENT_NOTIFY
		if (snd_wnd_prev <= 0) {
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RaiseWriteEvent(mtcp, cur_stream);
#if SELECTIVE_WRITE_EVENT_NOTIFY
		}
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */

		SBUF_UNLOCK(&sndvar->write_lock);
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			UpdateRetransmissionTimer(mtcp, cur_stream, pctx->p.cur_ts);
	}
}
/*----------------------------------------------------------------------------*/
/* ProcessTCPPayload: merges TCP payload using receive ring buffer            */
/* Return: TRUE (1) in normal case, FALSE (0) if immediate ACK is required    */
/* CAUTION: should only be called at ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2      */
/*----------------------------------------------------------------------------*/
static inline int 
ProcessTCPPayload(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
				struct pkt_ctx *pctx)
{
	struct tcp_recv_vars *rcvvar = cur_stream->rcvvar;
	uint32_t prev_rcv_nxt;
	int ret = -1;
	bool read_lock;
	struct socket_map *walk;

	if (!cur_stream->buffer_mgmt)
		return FALSE;

	/* if seq and segment length is lower than rcv_nxt, ignore and send ack */
	if (TCP_SEQ_LT(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt)) {
		SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
			HandleCallback(mtcp, MOS_NULL, walk, cur_stream->side,
				       pctx, MOS_ON_ERROR);
		} SOCKQ_FOREACH_END;
		return FALSE;
	}
	/* if payload exceeds receiving buffer, drop and send ack */
	if (TCP_SEQ_GT(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt + rcvvar->rcv_wnd)) {
		SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
			HandleCallback(mtcp, MOS_NULL, walk, cur_stream->side,
				       pctx, MOS_ON_ERROR);
		} SOCKQ_FOREACH_END;
		return FALSE;
	}

	/* allocate receive buffer if not exist */
	if (!rcvvar->rcvbuf) {
		rcvvar->rcvbuf = tcprb_new(mtcp->bufseg_pool, g_config.mos->rmem_size, cur_stream->buffer_mgmt);
		if (!rcvvar->rcvbuf) {
			TRACE_ERROR("Stream %d: Failed to allocate receive buffer.\n", 
				    cur_stream->id);
			cur_stream->state = TCP_ST_CLOSED_RSVD;
			cur_stream->close_reason = TCP_NO_MEM;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RaiseErrorEvent(mtcp, cur_stream);
			
			return ERROR;
		}
	}

	read_lock = HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM);
	
	if (read_lock && SBUF_LOCK(&rcvvar->read_lock)) {
		if (errno == EDEADLK)
			perror("ProcessTCPPayload: read_lock blocked\n");
		assert(0);
	}
	
	prev_rcv_nxt = cur_stream->rcv_nxt;

	tcprb_t *rb = rcvvar->rcvbuf;
	loff_t off = seq2loff(rb, pctx->p.seq, (rcvvar->irs + 1));
	if (off >= 0) {
		if (!HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM) &&
			HAS_STREAM_TYPE(cur_stream, MOS_SOCK_MONITOR_STREAM_ACTIVE))
			tcprb_setpile(rb, rb->pile + tcprb_cflen(rb));
		ret = tcprb_pwrite(rb, pctx->p.payload, pctx->p.payloadlen, off);
		if (ret < 0) {
			/* We try again after warning this result to the user. */
			SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
				HandleCallback(mtcp, MOS_NULL, walk, cur_stream->side,
						   pctx, MOS_ON_ERROR);
			} SOCKQ_FOREACH_END;
			ret = tcprb_pwrite(rb, pctx->p.payload, pctx->p.payloadlen, off);
		}
	}
	/* TODO: update monitor vars */

	/* 
	 * error can pop up due to disabled buffered management
	 * (only in monitor mode). In that case, ignore the warning 
	 * message.
	 */
	if (ret < 0 && cur_stream->buffer_mgmt && mtcp->num_msp == 0)
		TRACE_ERROR("Cannot merge payload. reason: %d\n", ret);
		
	/* discard the buffer if the state is FIN_WAIT_1 or FIN_WAIT_2, 
	   meaning that the connection is already closed by the application */
	loff_t cftail = rb->pile + tcprb_cflen(rb);
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
	    cur_stream->state == TCP_ST_FIN_WAIT_2) {
		/* XXX: Do we really need to update recv vars? */
		tcprb_setpile(rb, cftail);
	}
	if (cftail > 0 && (rcvvar->irs + 1) + cftail > cur_stream->rcv_nxt) {
		RAISE_DEBUG_EVENT(mtcp, cur_stream,
				"Move rcv_nxt from %u to %u.\n",
				cur_stream->rcv_nxt, (rcvvar->irs + 1) + cftail);
		cur_stream->rcv_nxt = (rcvvar->irs + 1) + cftail;
	}
	assert(cftail - rb->pile >= 0);
	rcvvar->rcv_wnd = rb->len - (cftail - rb->pile);
	
	if (read_lock)
		SBUF_UNLOCK(&rcvvar->read_lock);
	
	
	if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, prev_rcv_nxt)) {
		/* There are some lost packets */
		return FALSE;
	}
	
	TRACE_EPOLL("Stream %d data arrived. "
		    "len: %d, ET: %llu, IN: %llu, OUT: %llu\n", 
		    cur_stream->id, pctx->p.payloadlen, 
		    cur_stream->socket? (unsigned long long)cur_stream->socket->epoll & MOS_EPOLLET : 0, 
		    cur_stream->socket? (unsigned long long)cur_stream->socket->epoll & MOS_EPOLLIN : 0, 
		    cur_stream->socket? (unsigned long long)cur_stream->socket->epoll & MOS_EPOLLOUT : 0);
	
	if (cur_stream->state == TCP_ST_ESTABLISHED)
		RaiseReadEvent(mtcp, cur_stream);

	return TRUE;
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_LISTEN (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx)
{
	
	const struct tcphdr* tcph = pctx->p.tcph;
	
	if (tcph->syn) {
		if (cur_stream->state == TCP_ST_LISTEN)
			cur_stream->rcv_nxt++;
		cur_stream->state = TCP_ST_SYN_RCVD;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE | MOS_ON_CONN_START;
		TRACE_STATE("Stream %d: TCP_ST_SYN_RCVD\n", cur_stream->id);
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		if (IS_STREAM_TYPE(cur_stream, MOS_SOCK_MONITOR_STREAM_ACTIVE)) {
			/**
			 * Passive stream context needs to initialize irs and rcv_nxt
			 * as it is not set neither during createserverstream or monitor
			 * creation.
			 */
			cur_stream->rcvvar->irs = 
				cur_stream->rcv_nxt = pctx->p.seq;
		}
	} else {
		CTRACE_ERROR("Stream %d (TCP_ST_LISTEN): "
				"Packet without SYN.\n", cur_stream->id);
	}

}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_SYN_SENT (mtcp_manager_t mtcp, tcp_stream* cur_stream,
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	/* when active open */
	if (tcph->ack) {
		/* filter the unacceptable acks */
		if (TCP_SEQ_LEQ(pctx->p.ack_seq, cur_stream->sndvar->iss)
#ifndef BE_RESILIENT_TO_PACKET_DROP
			|| TCP_SEQ_GT(pctx->p.ack_seq, cur_stream->snd_nxt)
#endif
				) {
			if (!tcph->rst) {
				cur_stream->actions |= MOS_ACT_SEND_RST;
			}
			return;
		}
		/* accept the ack */
		cur_stream->sndvar->snd_una++;
	}
	
	if (tcph->rst) {
		if (tcph->ack) {
			cur_stream->state = TCP_ST_CLOSED_RSVD;
			cur_stream->close_reason = TCP_RESET;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			if (cur_stream->socket) {
				if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
					RaiseErrorEvent(mtcp, cur_stream);
			} else {
				cur_stream->actions |= MOS_ACT_DESTROY;
			}
		}
		return;
	}

	if (tcph->ack
#ifndef BE_RESILIENT_TO_PACKET_DROP
		/* If we already lost SYNACK, let the ACK packet do the SYNACK's role */
		&& tcph->syn
#endif
	   ) {
		int ret = HandleActiveOpen(mtcp, cur_stream, pctx);
		if (!ret) {
			return;
		}

#ifdef BE_RESILIENT_TO_PACKET_DROP
		if (!tcph->syn) {
			RAISE_DEBUG_EVENT(mtcp, cur_stream,
					"We missed SYNACK. Replace it with an ACK packet.\n");
			/* correct some variables */
			cur_stream->rcv_nxt = pctx->p.seq;
		}
#endif

		cur_stream->sndvar->nrtx = 0;
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			RemoveFromRTOList(mtcp, cur_stream);
		cur_stream->state = TCP_ST_ESTABLISHED;
		cur_stream->cb_events |= /*MOS_ON_CONN_SETUP |*/ MOS_ON_TCP_STATE_CHANGE;
		TRACE_STATE("Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

		if (cur_stream->socket) {
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RaiseWriteEvent(mtcp, cur_stream);
		} else {
			TRACE_STATE("Stream %d: ESTABLISHED, but no socket\n", cur_stream->id);
			cur_stream->close_reason = TCP_ACTIVE_CLOSE;
			cur_stream->actions |= MOS_ACT_SEND_RST;
			cur_stream->actions |= MOS_ACT_DESTROY;
		}
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		if (g_config.mos->tcp_timeout > 0)
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				AddtoTimeoutList(mtcp, cur_stream);

#ifdef BE_RESILIENT_TO_PACKET_DROP
		/* Handle this ack packet */
		if (!tcph->syn)
			Handle_TCP_ST_ESTABLISHED(mtcp, cur_stream, pctx);
#endif

	} else if (tcph->syn) {
		cur_stream->state = TCP_ST_SYN_RCVD;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		TRACE_STATE("Stream %d: TCP_ST_SYN_RCVD\n", cur_stream->id);
		cur_stream->snd_nxt = cur_stream->sndvar->iss;
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_SYN_RCVD (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;
	struct tcp_send_vars *sndvar = cur_stream->sndvar;
	int ret;

	if (tcph->ack) {
		uint32_t prior_cwnd;
		/* NOTE: We do not validate the ack number because first few packets
		 * can also come out of order */

		sndvar->snd_una++;
		cur_stream->snd_nxt = pctx->p.ack_seq;
		prior_cwnd = sndvar->cwnd;
		sndvar->cwnd = ((prior_cwnd == 1)? 
				(sndvar->mss * 2): sndvar->mss);
		
		//UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
		sndvar->nrtx = 0;
		cur_stream->rcv_nxt = cur_stream->rcvvar->irs + 1;
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			RemoveFromRTOList(mtcp, cur_stream);

		cur_stream->state = TCP_ST_ESTABLISHED;
		cur_stream->cb_events |= /*MOS_ON_CONN_SETUP |*/ MOS_ON_TCP_STATE_CHANGE;
		TRACE_STATE("Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

#ifdef BE_RESILIENT_TO_PACKET_DROP
		if (pctx->p.ack_seq != sndvar->iss + 1)
			Handle_TCP_ST_ESTABLISHED(mtcp, cur_stream, pctx);
#endif

		/* update listening socket */
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM)) {

			struct tcp_listener *listener = mtcp->listener;
			
			ret = StreamEnqueue(listener->acceptq, cur_stream);
			if (ret < 0) {
				TRACE_ERROR("Stream %d: Failed to enqueue to "
						"the listen backlog!\n", cur_stream->id);
				cur_stream->close_reason = TCP_NOT_ACCEPTED;
				cur_stream->state = TCP_ST_CLOSED_RSVD;
				cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", cur_stream->id);
				cur_stream->actions |= MOS_ACT_SEND_CONTROL;
			}

			/* raise an event to the listening socket */
			if (listener->socket && (listener->socket->epoll & MOS_EPOLLIN)) {
				AddEpollEvent(mtcp->ep, 
						MOS_EVENT_QUEUE, listener->socket, MOS_EPOLLIN);
			}
		}
		
		//TRACE_DBG("Stream %d inserted into acceptq.\n", cur_stream->id);
		if (g_config.mos->tcp_timeout > 0)
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				AddtoTimeoutList(mtcp, cur_stream);

	} else {
		/* Handle retransmitted SYN packet */
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_MONITOR_STREAM_ACTIVE) &&
			tcph->syn) {
			if (pctx->p.seq == cur_stream->pair_stream->sndvar->iss) {
				TRACE_DBG("syn retransmit! (p.seq = %u / iss = %u)\n",
						  pctx->p.seq, cur_stream->pair_stream->sndvar->iss);
				cur_stream->cb_events |= MOS_ON_REXMIT;
			}
		}

		TRACE_DBG("Stream %d (TCP_ST_SYN_RCVD): No ACK.\n", 
				cur_stream->id);
		/* retransmit SYN/ACK */
		cur_stream->snd_nxt = sndvar->iss;
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_ESTABLISHED (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	if (tcph->syn) {
		/* Handle retransmitted SYNACK packet */
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_MONITOR_STREAM_ACTIVE) &&
			tcph->ack) {
			if (pctx->p.seq == cur_stream->pair_stream->sndvar->iss) {
				TRACE_DBG("syn/ack retransmit! (p.seq = %u / iss = %u)\n",
						pctx->p.seq, cur_stream->pair_stream->sndvar->iss);
				cur_stream->cb_events |= MOS_ON_REXMIT;
			}
		}

		TRACE_DBG("Stream %d (TCP_ST_ESTABLISHED): weird SYN. "
				"seq: %u, expected: %u, ack_seq: %u, expected: %u\n", 
				cur_stream->id, pctx->p.seq, cur_stream->rcv_nxt, 
				pctx->p.ack_seq, cur_stream->snd_nxt);
		cur_stream->snd_nxt = pctx->p.ack_seq;
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		return;
	}

	if (pctx->p.payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, pctx)) {
			/* if return is TRUE, send ACK */
			cur_stream->actions |= MOS_ACT_SEND_ACK_AGG;
		} else {
			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
		}
	}

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, pctx);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (!cur_stream->buffer_mgmt ||
#ifdef BE_RESILIENT_TO_PACKET_DROP
			TCP_SEQ_GEQ(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt)
#else
			pctx->p.seq + pctx->p.payloadlen == cur_stream->rcv_nxt
#endif
			) {
#ifdef BE_RESILIENT_TO_PACKET_DROP
			cur_stream->rcv_nxt = pctx->p.seq + pctx->p.payloadlen;
#endif
			cur_stream->state = TCP_ST_CLOSE_WAIT;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			TRACE_STATE("Stream %d: TCP_ST_CLOSE_WAIT\n", cur_stream->id);
			cur_stream->rcv_nxt++;
			cur_stream->actions |= MOS_ACT_SEND_CONTROL;

			/* notify FIN to application */
			RaiseReadEvent(mtcp, cur_stream);
		} else {
			RAISE_DEBUG_EVENT(mtcp, cur_stream,
					"Expected %u, but received %u (= %u + %u)\n",
					cur_stream->rcv_nxt, pctx->p.seq + pctx->p.payloadlen,
					pctx->p.seq, pctx->p.payloadlen);

			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
			return;
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_CLOSE_WAIT (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx)
{
	if (TCP_SEQ_LT(pctx->p.seq, cur_stream->rcv_nxt)) {
		TRACE_DBG("Stream %d (TCP_ST_CLOSE_WAIT): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, pctx->p.seq, cur_stream->rcv_nxt);
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		return;
	}

	if (cur_stream->sndvar->sndbuf) {
		ProcessACK(mtcp, cur_stream, pctx); 
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_LAST_ACK (mtcp_manager_t mtcp, tcp_stream* cur_stream, 
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	if (TCP_SEQ_LT(pctx->p.seq, cur_stream->rcv_nxt)) {
		TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, pctx->p.seq, cur_stream->rcv_nxt);
		return;
	}

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, pctx);
		}

		if (!cur_stream->sndvar->is_fin_sent) {
			/* the case that FIN is not sent yet */
			/* this is not ack for FIN, ignore */
			TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): "
					"No FIN sent yet.\n", cur_stream->id);
#ifdef DBGMSG
			DumpIPPacket(mtcp, pctx->p.iph, pctx->p.ip_len);
#endif
#if DUMP_STREAM
			DumpStream(mtcp, cur_stream);
			DumpControlList(mtcp, mtcp->n_sender[0]);
#endif
			return;
		}

		/* check if ACK of FIN */
		if (pctx->p.ack_seq == cur_stream->sndvar->fss + 1) {
			cur_stream->sndvar->snd_una++;
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				UpdateRetransmissionTimer(mtcp, cur_stream, pctx->p.cur_ts);
			cur_stream->state = TCP_ST_CLOSED_RSVD;
			cur_stream->close_reason = TCP_PASSIVE_CLOSE;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", 
					cur_stream->id);
			cur_stream->actions |= MOS_ACT_DESTROY;
		} else {
			TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): Not ACK of FIN. "
					"ack_seq: %u, expected: %u\n", 
					cur_stream->id, pctx->p.ack_seq, cur_stream->sndvar->fss + 1);
			//cur_stream->snd_nxt = cur_stream->sndvar->fss;
			cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		}
	} else {
		TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): No ACK\n", 
			  cur_stream->id);
		//cur_stream->snd_nxt = cur_stream->sndvar->fss;
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_FIN_WAIT_1 (mtcp_manager_t mtcp, tcp_stream* cur_stream,
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	if (TCP_SEQ_LT(pctx->p.seq, cur_stream->rcv_nxt)) {
		RAISE_DEBUG_EVENT(mtcp, cur_stream,
				"Stream %d (FIN_WAIT_1): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, pctx->p.seq, cur_stream->rcv_nxt);
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		return;
	}

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, pctx); 
		}

		if (cur_stream->sndvar->is_fin_sent &&
			((!HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM) &&
			  pctx->p.ack_seq == cur_stream->sndvar->fss) ||
			 pctx->p.ack_seq == cur_stream->sndvar->fss + 1)) {
			cur_stream->sndvar->snd_una = pctx->p.ack_seq;
			if (TCP_SEQ_GT(pctx->p.ack_seq, cur_stream->snd_nxt)) {
				TRACE_DBG("Stream %d: update snd_nxt to %u\n", 
						cur_stream->id, pctx->p.ack_seq);
				cur_stream->snd_nxt = pctx->p.ack_seq;
			}
			//cur_stream->sndvar->snd_una++;
			//UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
			cur_stream->sndvar->nrtx = 0;
			if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
				RemoveFromRTOList(mtcp, cur_stream);
			cur_stream->state = TCP_ST_FIN_WAIT_2;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			TRACE_STATE("Stream %d: TCP_ST_FIN_WAIT_2\n", 
					cur_stream->id);
		} else {
			RAISE_DEBUG_EVENT(mtcp, cur_stream,
					"Failed to transit to FIN_WAIT_2, "
					"is_fin_sent: %s, ack_seq: %u, sndvar->fss: %u\n",
					cur_stream->sndvar->is_fin_sent ? "true" : "false",
					pctx->p.ack_seq, cur_stream->sndvar->fss);
		}

	} else {
		RAISE_DEBUG_EVENT(mtcp, cur_stream,
				"Failed to transit to FIN_WAIT_2, "
				"We got a %s%s%s%s%s%s packet.",
				pctx->p.tcph->syn ? "S" : "",
				pctx->p.tcph->fin ? "F" : "",
				pctx->p.tcph->rst ? "R" : "",
				pctx->p.tcph->psh ? "P" : "",
				pctx->p.tcph->urg ? "U" : "",
				pctx->p.tcph->ack ? "A" : "");

		TRACE_DBG("Stream %d: does not contain an ack!\n", 
				cur_stream->id);
		return;
	}

	if (pctx->p.payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, pctx)) {
			/* if return is TRUE, send ACK */
			cur_stream->actions |= MOS_ACT_SEND_ACK_AGG;
		} else {
			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (!cur_stream->buffer_mgmt ||
#ifdef BE_RESILIENT_TO_PACKET_DROP
			TCP_SEQ_GEQ(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt)
#else
			pctx->p.seq + pctx->p.payloadlen == cur_stream->rcv_nxt
#endif
		   ) {
			cur_stream->rcv_nxt++;

			if (cur_stream->state == TCP_ST_FIN_WAIT_1) {
				cur_stream->state = TCP_ST_CLOSING;
				cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSING\n", cur_stream->id);

			} else if (cur_stream->state == TCP_ST_FIN_WAIT_2) {
				cur_stream->state = TCP_ST_TIME_WAIT;
				cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);
				AddtoTimewaitList(mtcp, cur_stream, pctx->p.cur_ts);
			}
			cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		} else {
			RAISE_DEBUG_EVENT(mtcp, cur_stream,
					"Expected %u, but received %u (= %u + %u)\n",
					cur_stream->rcv_nxt, pctx->p.seq + pctx->p.payloadlen,
					pctx->p.seq, pctx->p.payloadlen);

			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
			return;
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_FIN_WAIT_2 (mtcp_manager_t mtcp, tcp_stream* cur_stream,
		struct pkt_ctx *pctx)
{	
	const struct tcphdr* tcph = pctx->p.tcph;

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, pctx); 
		}
	} else {
		TRACE_DBG("Stream %d: does not contain an ack!\n", 
				cur_stream->id);
		return;
	}

	if (pctx->p.payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, pctx)) {
			/* if return is TRUE, send ACK */
			cur_stream->actions |= MOS_ACT_SEND_ACK_AGG;
		} else {
			cur_stream->actions |= MOS_ACT_SEND_ACK_NOW;
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (!cur_stream->buffer_mgmt ||
#ifdef BE_RESILIENT_TO_PACKET_DROP
			TCP_SEQ_GEQ(pctx->p.seq + pctx->p.payloadlen, cur_stream->rcv_nxt)
#else
			pctx->p.seq + pctx->p.payloadlen == cur_stream->rcv_nxt
#endif
			) {
			cur_stream->state = TCP_ST_TIME_WAIT;
			cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
			cur_stream->rcv_nxt++;
			TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);

			AddtoTimewaitList(mtcp, cur_stream, pctx->p.cur_ts);
			cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		}
	} else {
		TRACE_DBG("Stream %d (TCP_ST_FIN_WAIT_2): No FIN. "
				"seq: %u, ack_seq: %u, snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, pctx->p.seq, pctx->p.ack_seq, 
				cur_stream->snd_nxt, cur_stream->sndvar->snd_una);
#if DBGMSG
		DumpIPPacket(mtcp, pctx->p.iph, pctx->p.ip_len);
#endif
	}

}
/*----------------------------------------------------------------------------*/
static inline void
Handle_TCP_ST_CLOSING (mtcp_manager_t mtcp, tcp_stream* cur_stream,
		struct pkt_ctx *pctx)
{
	const struct tcphdr* tcph = pctx->p.tcph;

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, pctx);
		}

		if (!cur_stream->sndvar->is_fin_sent) {
			TRACE_DBG("Stream %d (TCP_ST_CLOSING): "
					"No FIN sent yet.\n", cur_stream->id);
			return;
		}

		// check if ACK of FIN
		if (pctx->p.ack_seq != cur_stream->sndvar->fss + 1) {
#if DBGMSG
			TRACE_DBG("Stream %d (TCP_ST_CLOSING): Not ACK of FIN. "
				  "ack_seq: %u, snd_nxt: %u, snd_una: %u, fss: %u\n", 
				  cur_stream->id, pctx->p.ack_seq, cur_stream->snd_nxt, 
				  cur_stream->sndvar->snd_una, cur_stream->sndvar->fss);
			DumpIPPacketToFile(stderr, pctx->p.iph, pctx->p.ip_len);
			DumpStream(mtcp, cur_stream);
#endif
			//assert(0);
			/* if the packet is not the ACK of FIN, ignore */
			return;
		}
		
		cur_stream->sndvar->snd_una = pctx->p.ack_seq;
		if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
			UpdateRetransmissionTimer(mtcp, cur_stream, pctx->p.cur_ts);
		cur_stream->state = TCP_ST_TIME_WAIT;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);
		
		AddtoTimewaitList(mtcp, cur_stream, pctx->p.cur_ts);

	} else {
		TRACE_DBG("Stream %d (TCP_ST_CLOSING): Not ACK\n",
			  cur_stream->id);
		return;
	}
}
/*----------------------------------------------------------------------------*/
void
UpdateRecvTCPContext(mtcp_manager_t mtcp, struct tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	struct tcphdr* tcph = pctx->p.tcph;
	int ret;

	assert(cur_stream);
	
	/* Validate sequence. if not valid, ignore the packet */
	if (cur_stream->state > TCP_ST_SYN_RCVD) {
		
		ret = ValidateSequence(mtcp, cur_stream, pctx);
		if (!ret) {
			TRACE_DBG("Stream %d: Unexpected sequence: %u, expected: %u\n",
					cur_stream->id, pctx->p.seq, cur_stream->rcv_nxt);
#ifdef DBGMSG
			DumpIPPacket(mtcp, pctx->p.iph, pctx->p.ip_len);
#endif
#if DUMP_STREAM
			DumpStream(mtcp, cur_stream);
#endif
			/* cur_stream->cb_events |= MOS_ON_ERROR; */
		}
	}
	/* Update receive window size */
	if (tcph->syn) {
		cur_stream->sndvar->peer_wnd = pctx->p.window;
	} else {
		cur_stream->sndvar->peer_wnd = 
				(uint32_t)pctx->p.window << cur_stream->sndvar->wscale_peer;
	}
				
	cur_stream->last_active_ts = pctx->p.cur_ts;
	if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_STREAM))
		UpdateTimeoutList(mtcp, cur_stream);

	/* Process RST: process here only if state > TCP_ST_SYN_SENT */
	if (tcph->rst) {
		cur_stream->have_reset = TRUE;
		if (cur_stream->state > TCP_ST_SYN_SENT) {
			if (ProcessRST(mtcp, cur_stream, pctx)) {
				return;
			}
		}
	}

	if (HAS_STREAM_TYPE(cur_stream, MOS_SOCK_MONITOR_STREAM_ACTIVE) &&
		pctx->p.tcph->fin) {
		if (cur_stream->state == TCP_ST_CLOSE_WAIT ||
			cur_stream->state == TCP_ST_LAST_ACK ||
			cur_stream->state == TCP_ST_CLOSING ||
			cur_stream->state == TCP_ST_TIME_WAIT) {
			/* Handle retransmitted FIN packet */
			if (pctx->p.seq == cur_stream->pair_stream->sndvar->fss) {
				TRACE_DBG("FIN retransmit! (seq = %u / fss = %u)\n",
						pctx->p.seq, cur_stream->pair_stream->sndvar->fss);
				cur_stream->cb_events |= MOS_ON_REXMIT;
			}
		}
	}

	switch (cur_stream->state) {
	case TCP_ST_LISTEN:
		Handle_TCP_ST_LISTEN(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_SYN_SENT:
		Handle_TCP_ST_SYN_SENT(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_SYN_RCVD:
		/* SYN retransmit implies our SYN/ACK was lost. Resend */
		if (tcph->syn && pctx->p.seq == cur_stream->rcvvar->irs)
			Handle_TCP_ST_LISTEN(mtcp, cur_stream, pctx);
		else
			Handle_TCP_ST_SYN_RCVD(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_ESTABLISHED:
		Handle_TCP_ST_ESTABLISHED(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_CLOSE_WAIT:
		Handle_TCP_ST_CLOSE_WAIT(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_LAST_ACK:
		Handle_TCP_ST_LAST_ACK(mtcp, cur_stream, pctx);
		break;
	
	case TCP_ST_FIN_WAIT_1:
		Handle_TCP_ST_FIN_WAIT_1(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_FIN_WAIT_2:
		Handle_TCP_ST_FIN_WAIT_2(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_CLOSING:
		Handle_TCP_ST_CLOSING(mtcp, cur_stream, pctx);
		break;

	case TCP_ST_TIME_WAIT:
		/* the only thing that can arrive in this state is a retransmission 
		   of the remote FIN. Acknowledge it, and restart the 2 MSL timeout */
		if (cur_stream->on_timewait_list) {
			RemoveFromTimewaitList(mtcp, cur_stream);
			AddtoTimewaitList(mtcp, cur_stream, pctx->p.cur_ts);
		}
		cur_stream->actions |= MOS_ACT_SEND_CONTROL;
		break;
		
	case TCP_ST_CLOSED:
	case TCP_ST_CLOSED_RSVD:
		break;

	default:
		break;
	}
		
	TRACE_STATE("Stream %d: Events: %0lx, Action: %0x\n", 
			cur_stream->id, cur_stream->cb_events, cur_stream->actions);
	return;
}
/*----------------------------------------------------------------------------*/
void
DoActionEndTCPPacket(mtcp_manager_t mtcp, struct tcp_stream *cur_stream, 
		struct pkt_ctx *pctx)
{
	int i;
	
	for (i = 1; i < MOS_ACT_CNT; i = i << 1) {
		
		if (cur_stream->actions & i) {
			switch(i) {
			case MOS_ACT_SEND_DATA:
				AddtoSendList(mtcp, cur_stream);
				break;
			case MOS_ACT_SEND_ACK_NOW:
				EnqueueACK(mtcp, cur_stream, pctx->p.cur_ts, ACK_OPT_NOW);
				break;
			case MOS_ACT_SEND_ACK_AGG:
				EnqueueACK(mtcp, cur_stream, pctx->p.cur_ts, ACK_OPT_AGGREGATE);
				break;
			case MOS_ACT_SEND_CONTROL:
				AddtoControlList(mtcp, cur_stream, pctx->p.cur_ts);
				break;
			case MOS_ACT_SEND_RST:
				if (cur_stream->state <= TCP_ST_SYN_SENT)
					SendTCPPacketStandalone(mtcp, 
							pctx->p.iph->daddr, pctx->p.tcph->dest,
							pctx->p.iph->saddr, pctx->p.tcph->source, 
							0, pctx->p.seq + 1, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
							NULL, 0, pctx->p.cur_ts, 0);
				else
					SendTCPPacketStandalone(mtcp, 
							pctx->p.iph->daddr, pctx->p.tcph->dest,
							pctx->p.iph->saddr, pctx->p.tcph->source, 
							pctx->p.ack_seq, 0, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
							NULL, 0, pctx->p.cur_ts, 0);
				break;
			case MOS_ACT_DESTROY:
				DestroyTCPStream(mtcp, cur_stream);
				break;
			default:
				assert(1);
				break;
			}
		}
	}
	
	cur_stream->actions = 0;
}
/*----------------------------------------------------------------------------*/
/**
 * Called (when monitoring mode is enabled).. for every outgoing packet to the 
 * NIC.
 */
inline void
UpdatePassiveRecvTCPContext(mtcp_manager_t mtcp, struct tcp_stream *cur_stream, 
			       struct pkt_ctx *pctx)
{
	UpdateRecvTCPContext(mtcp, cur_stream, pctx);
}
/*----------------------------------------------------------------------------*/
/* NOTE TODO: This event prediction is additional overhaed of POST_RCV hook.
 * We can transparently optimize this by disabling prediction of events which
 * are not monitored by anyone. */
inline void
PreRecvTCPEventPrediction(mtcp_manager_t mtcp, struct pkt_ctx *pctx,
			  struct tcp_stream *recvside_stream)
{
#define DOESOVERLAP(a1, a2, b1, b2) \
	((a1 != b2) && (a2 != b1) && ((a1 > b2) != (a2 > b1)))

	/* Check whether this packet is retransmitted or not. */
	tcprb_t *rb;
	if (pctx->p.payloadlen > 0 && recvside_stream->rcvvar != NULL
		&& (rb = recvside_stream->rcvvar->rcvbuf) != NULL) {
		struct _tcpfrag_t *f;
		loff_t off = seq2loff(rb, pctx->p.seq, recvside_stream->rcvvar->irs + 1);
		TAILQ_FOREACH(f, &rb->frags, link)
			if (DOESOVERLAP(f->head, f->tail, off, off + pctx->p.payloadlen)) {
				recvside_stream->cb_events |= MOS_ON_REXMIT;
				TRACE_DBG("RETX!\n");
				break;
			}
	}
}
/*----------------------------------------------------------------------------*/
