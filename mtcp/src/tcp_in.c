#include <assert.h>

#include "tcp_util.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "tcp_ring_buffer.h"
#include "eventpoll.h"
#include "debug.h"
#include "timer.h"
#include "ip_in.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define VERIFY_RX_CHECKSUM TRUE
#define RECOVERY_AFTER_LOSS TRUE
#define SELECTIVE_WRITE_EVENT_NOTIFY TRUE

/*----------------------------------------------------------------------------*/
static inline int 
FilterSYNPacket(mtcp_manager_t mtcp, uint32_t ip, uint16_t port)
{
	struct sockaddr_in *addr;
	struct tcp_listener *listener;

	/* TODO: This listening logic should be revised */

	/* if not the address we want, drop */
	listener = (struct tcp_listener *)ListenerHTSearch(mtcp->listeners, &port);
	if (listener == NULL)	return FALSE;

	addr = &listener->socket->saddr;

	if (addr->sin_port == port) {
		if (addr->sin_addr.s_addr != INADDR_ANY) {
			if (ip == addr->sin_addr.s_addr) {
				return TRUE;
			}
			return FALSE;
		} else {
			int i;

			for (i = 0; i < CONFIG.eths_num; i++) {
				if (ip == CONFIG.eths[i].ip_addr) {
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	return FALSE;
}
/*----------------------------------------------------------------------------*/
static inline tcp_stream *
HandlePassiveOpen(mtcp_manager_t mtcp, uint32_t cur_ts, const struct iphdr *iph, 
		const struct tcphdr *tcph, uint32_t seq, uint16_t window)
{
	tcp_stream *cur_stream = NULL;

	/* create new stream and add to flow hash table */
	cur_stream = CreateTCPStream(mtcp, NULL, MTCP_SOCK_STREAM, 
			iph->daddr, tcph->dest, iph->saddr, tcph->source);
	if (!cur_stream) {
		TRACE_ERROR("INFO: Could not allocate tcp_stream!\n");
		return FALSE;
	}
	cur_stream->rcvvar->irs = seq;
	cur_stream->sndvar->peer_wnd = window;
	cur_stream->rcv_nxt = cur_stream->rcvvar->irs;
	cur_stream->sndvar->cwnd = 1;
	ParseTCPOptions(cur_stream, cur_ts, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);

	return cur_stream;
}
/*----------------------------------------------------------------------------*/
static inline int
HandleActiveOpen(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, uint16_t window)
{
	cur_stream->rcvvar->irs = seq;
	cur_stream->snd_nxt = ack_seq;
	cur_stream->sndvar->peer_wnd = window;
	cur_stream->rcvvar->snd_wl1 = cur_stream->rcvvar->irs - 1;
	cur_stream->rcv_nxt = cur_stream->rcvvar->irs + 1;
	cur_stream->rcvvar->last_ack_seq = ack_seq;
	ParseTCPOptions(cur_stream, cur_ts, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);
	cur_stream->sndvar->cwnd = ((cur_stream->sndvar->cwnd == 1)? 
			(cur_stream->sndvar->mss * 2): cur_stream->sndvar->mss);
	cur_stream->sndvar->ssthresh = cur_stream->sndvar->mss * 10;
	UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);

	return TRUE;
}
/*----------------------------------------------------------------------------*/
/* ValidateSequence: validates sequence number of the segment                 */
/* Return: TRUE if acceptable, FALSE if not acceptable                        */
/*----------------------------------------------------------------------------*/
static inline int
ValidateSequence(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, int payloadlen)
{
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
					seq, ts.ts_val, cur_stream->rcvvar->ts_recent);
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
			return FALSE;
		} else {
			/* valid timestamp */
			if (TCP_SEQ_GT(ts.ts_val, cur_stream->rcvvar->ts_recent)) {
				TRACE_TSTAMP("Timestamp update. cur: %u, prior: %u "
					"(time diff: %uus)\n", 
					ts.ts_val, cur_stream->rcvvar->ts_recent, 
					TS_TO_USEC(cur_ts - cur_stream->rcvvar->ts_last_ts_upd));
				cur_stream->rcvvar->ts_last_ts_upd = cur_ts;
			}

			cur_stream->rcvvar->ts_recent = ts.ts_val;
			cur_stream->rcvvar->ts_lastack_rcvd = ts.ts_ref;
		}
	}

	/* TCP sequence validation */
	if (!TCP_SEQ_BETWEEN(seq + payloadlen, cur_stream->rcv_nxt, 
				cur_stream->rcv_nxt + cur_stream->rcvvar->rcv_wnd)) {

		/* if RST bit is set, ignore the segment */
		if (tcph->rst)
			return FALSE;

		if (cur_stream->state == TCP_ST_ESTABLISHED) {
			/* check if it is to get window advertisement */
			if (seq + 1 == cur_stream->rcv_nxt) {
#if 0
				TRACE_DBG("Window update request. (seq: %u, rcv_wnd: %u)\n", 
						seq, cur_stream->rcvvar->rcv_wnd);
#endif
				EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
				return FALSE;

			}

			if (TCP_SEQ_LEQ(seq, cur_stream->rcv_nxt)) {
				EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
			} else {
				EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
			}
		} else {
			if (cur_stream->state == TCP_ST_TIME_WAIT) {
				TRACE_DBG("Stream %d: tw expire update to %u\n", 
						cur_stream->id, cur_stream->rcvvar->ts_tw_expire);
				AddtoTimewaitList(mtcp, cur_stream, cur_ts);
			}
			AddtoControlList(mtcp, cur_stream, cur_ts);
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
ProcessRST(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t ack_seq)
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
		if (ack_seq == cur_stream->snd_nxt) {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_RESET;
			DestroyTCPStream(mtcp, cur_stream);
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
		cur_stream->state = TCP_ST_CLOSED;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		DestroyTCPStream(mtcp, cur_stream);
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
		//cur_stream->state = TCP_ST_CLOSED;
		//DestroyTCPStream(mtcp, cur_stream);
		cur_stream->state = TCP_ST_CLOSE_WAIT;
		cur_stream->close_reason = TCP_RESET;
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
ProcessACK(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, 
		uint16_t window, int payloadlen)
{
	struct tcp_send_vars *sndvar = cur_stream->sndvar;
	uint32_t cwindow, cwindow_prev;
	uint32_t rmlen;
	uint32_t snd_wnd_prev;
	uint32_t right_wnd_edge;
	uint8_t dup;
	int ret;

	cwindow = window;
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

	/* If ack_seq is previously acked, return */
	if (TCP_SEQ_GT(sndvar->snd_una, ack_seq)) {
		return;
	}
	
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
#if 0
		TRACE_CLWND("Window update. "
				"ack: %u, peer_wnd: %u, snd_nxt-snd_una: %u\n", 
				ack_seq, cwindow, cur_stream->snd_nxt - sndvar->snd_una);
#endif
		if (cwindow_prev < cur_stream->snd_nxt - sndvar->snd_una && 
				sndvar->peer_wnd >= cur_stream->snd_nxt - sndvar->snd_una) {
			TRACE_CLWND("%u Broadcasting client window update! "
					"ack_seq: %u, peer_wnd: %u (before: %u), "
					"(snd_nxt - snd_una: %u)\n", 
					cur_stream->id, ack_seq, sndvar->peer_wnd, cwindow_prev, 
					cur_stream->snd_nxt - sndvar->snd_una);
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
		if (ack_seq == cur_stream->rcvvar->last_ack_seq && payloadlen == 0) {
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

		AddtoSendList(mtcp, cur_stream);

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
					cur_ts - cur_stream->rcvvar->ts_lastack_rcvd);
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
		ret = SBRemove(mtcp->rbm_snd, sndvar->sndbuf, rmlen);
		sndvar->snd_una = ack_seq;
		snd_wnd_prev = sndvar->snd_wnd;
		sndvar->snd_wnd = sndvar->sndbuf->size - sndvar->sndbuf->len;

		/* If there was no available sending window */
		/* notify the newly available window to application */
#if SELECTIVE_WRITE_EVENT_NOTIFY
		if (snd_wnd_prev <= 0) {
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */
			RaiseWriteEvent(mtcp, cur_stream);
#if SELECTIVE_WRITE_EVENT_NOTIFY
		}
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */

		SBUF_UNLOCK(&sndvar->write_lock);
		UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
	}

	UNUSED(ret);
}
/*----------------------------------------------------------------------------*/
/* ProcessTCPPayload: merges TCP payload using receive ring buffer            */
/* Return: TRUE (1) in normal case, FALSE (0) if immediate ACK is required    */
/* CAUTION: should only be called at ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2      */
/*----------------------------------------------------------------------------*/
static inline int 
ProcessTCPPayload(mtcp_manager_t mtcp, tcp_stream *cur_stream, 
		uint32_t cur_ts, uint8_t *payload, uint32_t seq, int payloadlen)
{
	struct tcp_recv_vars *rcvvar = cur_stream->rcvvar;
	uint32_t prev_rcv_nxt;
	int ret;

	/* if seq and segment length is lower than rcv_nxt, ignore and send ack */
	if (TCP_SEQ_LT(seq + payloadlen, cur_stream->rcv_nxt)) {
		return FALSE;
	}
	/* if payload exceeds receiving buffer, drop and send ack */
	if (TCP_SEQ_GT(seq + payloadlen, cur_stream->rcv_nxt + rcvvar->rcv_wnd)) {
		return FALSE;
	}

	/* allocate receive buffer if not exist */
	if (!rcvvar->rcvbuf) {
		rcvvar->rcvbuf = RBInit(mtcp->rbm_rcv, rcvvar->irs + 1);
		if (!rcvvar->rcvbuf) {
			TRACE_ERROR("Stream %d: Failed to allocate receive buffer.\n", 
					cur_stream->id);
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_NO_MEM;
			RaiseErrorEvent(mtcp, cur_stream);

			return ERROR;
		}
	}

	if (SBUF_LOCK(&rcvvar->read_lock)) {
		if (errno == EDEADLK)
			perror("ProcessTCPPayload: read_lock blocked\n");
		assert(0);
	}

	prev_rcv_nxt = cur_stream->rcv_nxt;
	ret = RBPut(mtcp->rbm_rcv, 
			rcvvar->rcvbuf, payload, (uint32_t)payloadlen, seq);
	if (ret < 0) {
		TRACE_ERROR("Cannot merge payload. reason: %d\n", ret);
	}

	/* discard the buffer if the state is FIN_WAIT_1 or FIN_WAIT_2, 
	   meaning that the connection is already closed by the application */
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2) {
		RBRemove(mtcp->rbm_rcv, 
				rcvvar->rcvbuf, rcvvar->rcvbuf->merged_len, AT_MTCP);
	}
	cur_stream->rcv_nxt = rcvvar->rcvbuf->head_seq + rcvvar->rcvbuf->merged_len;
	rcvvar->rcv_wnd = rcvvar->rcvbuf->size - rcvvar->rcvbuf->merged_len;

	SBUF_UNLOCK(&rcvvar->read_lock);

	if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, prev_rcv_nxt)) {
		/* There are some lost packets */
		return FALSE;
	}

	TRACE_EPOLL("Stream %d data arrived. "
			"len: %d, ET: %u, IN: %u, OUT: %u\n", 
			cur_stream->id, payloadlen, 
			cur_stream->socket? cur_stream->socket->epoll & MTCP_EPOLLET : 0, 
			cur_stream->socket? cur_stream->socket->epoll & MTCP_EPOLLIN : 0, 
			cur_stream->socket? cur_stream->socket->epoll & MTCP_EPOLLOUT : 0);

	if (cur_stream->state == TCP_ST_ESTABLISHED) {
		if (!cur_stream->read_off)	
			RaiseReadEvent(mtcp, cur_stream);
	}

	return TRUE;
}
/*----------------------------------------------------------------------------*/
static inline tcp_stream *
CreateNewFlowHTEntry(mtcp_manager_t mtcp, uint32_t cur_ts, const struct iphdr *iph, 
		int ip_len, const struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
		int payloadlen, uint16_t window)
{
	tcp_stream *cur_stream;
	int ret; 
	
	if (tcph->syn && !tcph->ack) {
		/* handle the SYN */
		ret = FilterSYNPacket(mtcp, iph->daddr, tcph->dest);
		if (!ret) {
			TRACE_DBG("Refusing SYN packet.\n");
#ifdef DBGMSG
			DumpIPPacket(mtcp, iph, ip_len);
#endif
			SendTCPPacketStandalone(mtcp, 
					iph->daddr, tcph->dest, iph->saddr, tcph->source, 
					0, seq + payloadlen + 1, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
					NULL, 0, cur_ts, 0);

			return NULL;
		}

		/* now accept the connection */
		cur_stream = HandlePassiveOpen(mtcp, 
				cur_ts, iph, tcph, seq, window);
		if (!cur_stream) {
			TRACE_DBG("Not available space in flow pool.\n");
#ifdef DBGMSG
			DumpIPPacket(mtcp, iph, ip_len);
#endif
			SendTCPPacketStandalone(mtcp, 
					iph->daddr, tcph->dest, iph->saddr, tcph->source, 
					0, seq + payloadlen + 1, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
					NULL, 0, cur_ts, 0);

			return NULL;
		}

		return cur_stream;
	} else if (tcph->rst) {
		TRACE_DBG("Reset packet comes\n");
#ifdef DBGMSG
		DumpIPPacket(mtcp, iph, ip_len);
#endif
		/* for the reset packet, just discard */
		return NULL;
	} else {
		TRACE_DBG("Weird packet comes.\n");
#ifdef DBGMSG
		DumpIPPacket(mtcp, iph, ip_len);
#endif
		/* TODO: for else, discard and send a RST */
		/* if the ACK bit is off, respond with seq 0: 
		   <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
		   else (ACK bit is on):
		   <SEQ=SEG.ACK><CTL=RST>
		   */
		if (tcph->ack) {
			SendTCPPacketStandalone(mtcp, 
					iph->daddr, tcph->dest, iph->saddr, tcph->source, 
					ack_seq, 0, 0, TCP_FLAG_RST, NULL, 0, cur_ts, 0);
		} else {
			SendTCPPacketStandalone(mtcp, 
					iph->daddr, tcph->dest, iph->saddr, tcph->source, 
					0, seq + payloadlen, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
					NULL, 0, cur_ts, 0);
		}
		return NULL;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_LISTEN (mtcp_manager_t mtcp, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph) {
	
	if (tcph->syn) {
		if (cur_stream->state == TCP_ST_LISTEN)
			cur_stream->rcv_nxt++;
		cur_stream->state = TCP_ST_SYN_RCVD;
		TRACE_STATE("Stream %d: TCP_ST_SYN_RCVD\n", cur_stream->id);
		AddtoControlList(mtcp, cur_stream, cur_ts);
	} else {
		CTRACE_ERROR("Stream %d (TCP_ST_LISTEN): "
				"Packet without SYN.\n", cur_stream->id);
	}

}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_SYN_SENT (mtcp_manager_t mtcp, uint32_t cur_ts, 
		tcp_stream* cur_stream, const struct iphdr* iph, struct tcphdr* tcph,
		uint32_t seq, uint32_t ack_seq, int payloadlen, uint16_t window)
{
	/* when active open */
	if (tcph->ack) {
		/* filter the unacceptable acks */
		if (TCP_SEQ_LEQ(ack_seq, cur_stream->sndvar->iss) || 
				TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
			if (!tcph->rst) {
				SendTCPPacketStandalone(mtcp, 
						iph->daddr, tcph->dest, iph->saddr, tcph->source, 
						ack_seq, 0, 0, TCP_FLAG_RST, NULL, 0, cur_ts, 0);
			}
			return;
		}
		/* accept the ack */
		cur_stream->sndvar->snd_una++;
	}
	
	if (tcph->rst) {
		if (tcph->ack) {
			cur_stream->state = TCP_ST_CLOSE_WAIT;
			cur_stream->close_reason = TCP_RESET;
			if (cur_stream->socket) {
				RaiseErrorEvent(mtcp, cur_stream);
			} else {
				DestroyTCPStream(mtcp, cur_stream);
			}
		}
		return;
	}

	if (tcph->syn) {
		if (tcph->ack) {
			int ret = HandleActiveOpen(mtcp, 
					cur_stream, cur_ts, tcph, seq, ack_seq, window);
			if (!ret) {
				return;
			}
			
			cur_stream->sndvar->nrtx = 0;
			cur_stream->rcv_nxt = cur_stream->rcvvar->irs + 1;
			RemoveFromRTOList(mtcp, cur_stream);
			cur_stream->state = TCP_ST_ESTABLISHED;
			TRACE_STATE("Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

			if (cur_stream->socket) {
				RaiseWriteEvent(mtcp, cur_stream);
			} else {
				TRACE_STATE("Stream %d: ESTABLISHED, but no socket\n", cur_stream->id);
				SendTCPPacketStandalone(mtcp, 
						iph->daddr, tcph->dest, iph->saddr, tcph->source, 
						0, seq + payloadlen + 1, 0, TCP_FLAG_RST | TCP_FLAG_ACK, 
						NULL, 0, cur_ts, 0);
				cur_stream->close_reason = TCP_ACTIVE_CLOSE;
				DestroyTCPStream(mtcp, cur_stream);
				return;
			}
			AddtoControlList(mtcp, cur_stream, cur_ts);
			if (CONFIG.tcp_timeout > 0)
				AddtoTimeoutList(mtcp, cur_stream);

		} else {
			cur_stream->state = TCP_ST_SYN_RCVD;
			TRACE_STATE("Stream %d: TCP_ST_SYN_RCVD\n", cur_stream->id);
			cur_stream->snd_nxt = cur_stream->sndvar->iss;
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_SYN_RCVD (mtcp_manager_t mtcp, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t ack_seq) 
{
	struct tcp_send_vars *sndvar = cur_stream->sndvar;
	int ret;
	if (tcph->ack) {
		struct tcp_listener *listener;
		uint32_t prior_cwnd;
		/* check if ACK of SYN */
		if (ack_seq != sndvar->iss + 1) {
			CTRACE_ERROR("Stream %d (TCP_ST_SYN_RCVD): "
					"weird ack_seq: %u, iss: %u\n", 
					cur_stream->id, ack_seq, sndvar->iss);
			TRACE_DBG("Stream %d (TCP_ST_SYN_RCVD): "
					"weird ack_seq: %u, iss: %u\n", 
					cur_stream->id, ack_seq, sndvar->iss);
			return;
		}

		sndvar->snd_una++;
		cur_stream->snd_nxt = ack_seq;
		prior_cwnd = sndvar->cwnd;
		sndvar->cwnd = ((prior_cwnd == 1)? 
				(sndvar->mss * 2): sndvar->mss);
		
		//UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
		sndvar->nrtx = 0;
		cur_stream->rcv_nxt = cur_stream->rcvvar->irs + 1;
		RemoveFromRTOList(mtcp, cur_stream);

		cur_stream->state = TCP_ST_ESTABLISHED;
		TRACE_STATE("Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

		/* update listening socket */
		listener = (struct tcp_listener *)ListenerHTSearch(mtcp->listeners, &tcph->dest);

		ret = StreamEnqueue(listener->acceptq, cur_stream);
		if (ret < 0) {
			TRACE_ERROR("Stream %d: Failed to enqueue to "
					"the listen backlog!\n", cur_stream->id);
			cur_stream->close_reason = TCP_NOT_ACCEPTED;
			cur_stream->state = TCP_ST_CLOSED;
			TRACE_STATE("Stream %d: TCP_ST_CLOSED\n", cur_stream->id);
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}
		//TRACE_DBG("Stream %d inserted into acceptq.\n", cur_stream->id);
		if (CONFIG.tcp_timeout > 0)
			AddtoTimeoutList(mtcp, cur_stream);

		/* raise an event to the listening socket */
		if (listener->socket && (listener->socket->epoll & MTCP_EPOLLIN)) {
			AddEpollEvent(mtcp->ep, 
					MTCP_EVENT_QUEUE, listener->socket, MTCP_EPOLLIN);
		}

	} else {
		TRACE_DBG("Stream %d (TCP_ST_SYN_RCVD): No ACK.\n", 
				cur_stream->id);
		/* retransmit SYN/ACK */
		cur_stream->snd_nxt = sndvar->iss;
		AddtoControlList(mtcp, cur_stream, cur_ts);
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_ESTABLISHED (mtcp_manager_t mtcp, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
		uint8_t *payload, int payloadlen, uint16_t window) 
{
	if (tcph->syn) {
		TRACE_DBG("Stream %d (TCP_ST_ESTABLISHED): weird SYN. "
				"seq: %u, expected: %u, ack_seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt, 
				ack_seq, cur_stream->snd_nxt);
		cur_stream->snd_nxt = ack_seq;
		AddtoControlList(mtcp, cur_stream, cur_ts);
		return;
	}

	if (payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, 
				cur_ts, payload, seq, payloadlen)) {
			/* if return is TRUE, send ACK */
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		} else {
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}
	
	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, cur_ts, 
					tcph, seq, ack_seq, window, payloadlen);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->state = TCP_ST_CLOSE_WAIT;
			TRACE_STATE("Stream %d: TCP_ST_CLOSE_WAIT\n", cur_stream->id);
			cur_stream->rcv_nxt++;
			AddtoControlList(mtcp, cur_stream, cur_ts);

			/* notify FIN to application */
			if (!cur_stream->read_off)
				RaiseReadEvent(mtcp, cur_stream);
		} else {
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
			return;
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_CLOSE_WAIT (mtcp_manager_t mtcp, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, 
		int payloadlen, uint16_t window) 
{
	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_DBG("Stream %d (TCP_ST_CLOSE_WAIT): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt);
		AddtoControlList(mtcp, cur_stream, cur_ts);
		return;
	}

	if (cur_stream->sndvar->sndbuf) {
		ProcessACK(mtcp, cur_stream, cur_ts, 
				tcph, seq, ack_seq, window, payloadlen);
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_LAST_ACK (mtcp_manager_t mtcp, uint32_t cur_ts, const struct iphdr *iph,
		int ip_len, tcp_stream* cur_stream, struct tcphdr* tcph, 
		uint32_t seq, uint32_t ack_seq, int payloadlen, uint16_t window) 
{

	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt);
		return;
	}

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, cur_ts, 
					tcph, seq, ack_seq, window, payloadlen);
		}

		if (!cur_stream->sndvar->is_fin_sent) {
			/* the case that FIN is not sent yet */
			/* this is not ack for FIN, ignore */
			TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): "
					"No FIN sent yet.\n", cur_stream->id);
#ifdef DBGMSG
			DumpIPPacket(mtcp, iph, ip_len);
#endif
#if DUMP_STREAM
			DumpStream(mtcp, cur_stream);
			DumpControlList(mtcp, mtcp->n_sender[0]);
#endif
			return;
		}

		/* check if ACK of FIN */
		if (ack_seq == cur_stream->sndvar->fss + 1) {
			cur_stream->sndvar->snd_una++;
			UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_PASSIVE_CLOSE;
			TRACE_STATE("Stream %d: TCP_ST_CLOSED\n", 
					cur_stream->id);
			DestroyTCPStream(mtcp, cur_stream);
		} else {
			TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): Not ACK of FIN. "
					"ack_seq: %u, expected: %u\n", 
					cur_stream->id, ack_seq, cur_stream->sndvar->fss + 1);
			//cur_stream->snd_nxt = cur_stream->sndvar->fss;
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}
	} else {
		CTRACE_ERROR("Stream %d (TCP_ST_LAST_ACK): No ACK\n", 
				cur_stream->id);
		//cur_stream->snd_nxt = cur_stream->sndvar->fss;
		AddtoControlList(mtcp, cur_stream, cur_ts);
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_FIN_WAIT_1 (mtcp_manager_t mtcp, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, 
		uint8_t *payload, int payloadlen, uint16_t window) 
{

	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_DBG("Stream %d (TCP_ST_LAST_ACK): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt);
		AddtoControlList(mtcp, cur_stream, cur_ts);
		return;
	}

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, cur_ts, 
					tcph, seq, ack_seq, window, payloadlen);
		}

		if (cur_stream->sndvar->is_fin_sent && 
				ack_seq == cur_stream->sndvar->fss + 1) {
			cur_stream->sndvar->snd_una = ack_seq;
			if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
				TRACE_DBG("Stream %d: update snd_nxt to %u\n", 
						cur_stream->id, ack_seq);
				cur_stream->snd_nxt = ack_seq;
			}
			//cur_stream->sndvar->snd_una++;
			//UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);
			cur_stream->sndvar->nrtx = 0;
			RemoveFromRTOList(mtcp, cur_stream);
			cur_stream->state = TCP_ST_FIN_WAIT_2;
			TRACE_STATE("Stream %d: TCP_ST_FIN_WAIT_2\n", 
					cur_stream->id);
		}

	} else {
		TRACE_DBG("Stream %d: does not contain an ack!\n", 
				cur_stream->id);
		return;
	}

	if (payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, 
				cur_ts, payload, seq, payloadlen)) {
			/* if return is TRUE, send ACK */
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		} else {
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->rcv_nxt++;

			if (cur_stream->state == TCP_ST_FIN_WAIT_1) {
				cur_stream->state = TCP_ST_CLOSING;
				TRACE_STATE("Stream %d: TCP_ST_CLOSING\n", cur_stream->id);

			} else if (cur_stream->state == TCP_ST_FIN_WAIT_2) {
				cur_stream->state = TCP_ST_TIME_WAIT;
				TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);
				AddtoTimewaitList(mtcp, cur_stream, cur_ts);
			}
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_FIN_WAIT_2 (mtcp_manager_t mtcp, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
		uint8_t *payload, int payloadlen, uint16_t window)
{
	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, cur_ts, 
					tcph, seq, ack_seq, window, payloadlen);
		}
	} else {
		TRACE_DBG("Stream %d: does not contain an ack!\n", 
				cur_stream->id);
		return;
	}

	if (payloadlen > 0) {
		if (ProcessTCPPayload(mtcp, cur_stream, 
				cur_ts, payload, seq, payloadlen)) {
			/* if return is TRUE, send ACK */
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		} else {
			EnqueueACK(mtcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->state = TCP_ST_TIME_WAIT;
			cur_stream->rcv_nxt++;
			TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);

			AddtoTimewaitList(mtcp, cur_stream, cur_ts);
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}
#if 0
	} else {
		TRACE_DBG("Stream %d (TCP_ST_FIN_WAIT_2): No FIN. "
				"seq: %u, ack_seq: %u, snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, seq, ack_seq, 
				cur_stream->snd_nxt, cur_stream->sndvar->snd_una);
#if DBGMSG
		DumpIPPacket(mtcp, iph, ip_len);
#endif
#endif
	}

}
/*----------------------------------------------------------------------------*/
static inline void
Handle_TCP_ST_CLOSING (mtcp_manager_t mtcp, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
		int payloadlen, uint16_t window) {

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			ProcessACK(mtcp, cur_stream, cur_ts, 
					tcph, seq, ack_seq, window, payloadlen);
		}

		if (!cur_stream->sndvar->is_fin_sent) {
			TRACE_DBG("Stream %d (TCP_ST_CLOSING): "
					"No FIN sent yet.\n", cur_stream->id);
			return;
		}

		// check if ACK of FIN
		if (ack_seq != cur_stream->sndvar->fss + 1) {
#if 0
			CTRACE_ERROR("Stream %d (TCP_ST_CLOSING): Not ACK of FIN. "
					"ack_seq: %u, snd_nxt: %u, snd_una: %u, fss: %u\n", 
					cur_stream->id, ack_seq, cur_stream->snd_nxt, 
					cur_stream->sndvar->snd_una, cur_stream->sndvar->fss);
			DumpIPPacketToFile(stderr, iph, ip_len);
			DumpStream(mtcp, cur_stream);
#endif
			//assert(0);
			/* if the packet is not the ACK of FIN, ignore */
			return;
		}
		
		cur_stream->sndvar->snd_una = ack_seq;
		cur_stream->snd_nxt = ack_seq;
		UpdateRetransmissionTimer(mtcp, cur_stream, cur_ts);

		cur_stream->state = TCP_ST_TIME_WAIT;
		TRACE_STATE("Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);
		
		AddtoTimewaitList(mtcp, cur_stream, cur_ts);

	} else {
		CTRACE_ERROR("Stream %d (TCP_ST_CLOSING): Not ACK\n", 
				cur_stream->id);
		return;
	}
}
/*----------------------------------------------------------------------------*/
int
ProcessTCPPacket(mtcp_manager_t mtcp, 
		 uint32_t cur_ts, const int ifidx, const struct iphdr *iph, int ip_len)
{
	struct tcphdr* tcph = (struct tcphdr *) ((u_char *)iph + (iph->ihl << 2));
	uint8_t *payload    = (uint8_t *)tcph + (tcph->doff << 2);
	int payloadlen = ip_len - (payload - (u_char *)iph);
	tcp_stream s_stream;
	tcp_stream *cur_stream = NULL;
	uint32_t seq = ntohl(tcph->seq);
	uint32_t ack_seq = ntohl(tcph->ack_seq);
	uint16_t window = ntohs(tcph->window);
	uint16_t check;
	int ret;
	int rc = -1;

	/* Check ip packet invalidation */	
	if (ip_len < ((iph->ihl + tcph->doff) << 2))
		return ERROR;

#if VERIFY_RX_CHECKSUM
#ifndef DISABLE_HWCSUM
	if (mtcp->iom->dev_ioctl != NULL)
		rc = mtcp->iom->dev_ioctl(mtcp->ctx, ifidx,
					  PKT_RX_TCP_CSUM, NULL);
#endif
	if (rc == -1) {
		check = TCPCalcChecksum((uint16_t *)tcph, 
					(tcph->doff << 2) + payloadlen, iph->saddr, iph->daddr);
		if (check) {
			TRACE_DBG("Checksum Error: Original: 0x%04x, calculated: 0x%04x\n", 
				  tcph->check, TCPCalcChecksum((uint16_t *)tcph, 
				  (tcph->doff << 2) + payloadlen, iph->saddr, iph->daddr));
			tcph->check = 0;
			return ERROR;
		}
	}
#endif

#if defined(NETSTAT) && defined(ENABLELRO)
	mtcp->nstat.rx_gdptbytes += payloadlen;
#endif /* NETSTAT */

	s_stream.saddr = iph->daddr;
	s_stream.sport = tcph->dest;
	s_stream.daddr = iph->saddr;
	s_stream.dport = tcph->source;

	if (!(cur_stream = StreamHTSearch(mtcp->tcp_flow_table, &s_stream))) {
		/* not found in flow table */
		cur_stream = CreateNewFlowHTEntry(mtcp, cur_ts, iph, ip_len, tcph, 
				seq, ack_seq, payloadlen, window);
		if (!cur_stream)
			return TRUE;
	}

	/* Validate sequence. if not valid, ignore the packet */
	if (cur_stream->state > TCP_ST_SYN_RCVD) {
		ret = ValidateSequence(mtcp, cur_stream, 
				cur_ts, tcph, seq, ack_seq, payloadlen);
		if (!ret) {
			TRACE_DBG("Stream %d: Unexpected sequence: %u, expected: %u\n",
					cur_stream->id, seq, cur_stream->rcv_nxt);
#ifdef DBGMSG
			DumpIPPacket(mtcp, iph, ip_len);
#endif
#ifdef DUMP_STREAM
			DumpStream(mtcp, cur_stream);
#endif
			return TRUE;
		}
	}

	/* Update receive window size */
	if (tcph->syn) {
		cur_stream->sndvar->peer_wnd = window;
	} else {
		cur_stream->sndvar->peer_wnd = 
				(uint32_t)window << cur_stream->sndvar->wscale_peer;
	}
				
	cur_stream->last_active_ts = cur_ts;
	UpdateTimeoutList(mtcp, cur_stream);

	/* Process RST: process here only if state > TCP_ST_SYN_SENT */
	if (tcph->rst) {
		cur_stream->have_reset = TRUE;
		if (cur_stream->state > TCP_ST_SYN_SENT) {
			if (ProcessRST(mtcp, cur_stream, ack_seq)) {
				return TRUE;
			}
		}
	}

	switch (cur_stream->state) {
	case TCP_ST_LISTEN:
		Handle_TCP_ST_LISTEN(mtcp, cur_ts, cur_stream, tcph);
		break;

	case TCP_ST_SYN_SENT:
		Handle_TCP_ST_SYN_SENT(mtcp, cur_ts, cur_stream, iph, tcph, 
				seq, ack_seq, payloadlen, window);
		break;

	case TCP_ST_SYN_RCVD:
		/* SYN retransmit implies our SYN/ACK was lost. Resend */
		if (tcph->syn && seq == cur_stream->rcvvar->irs)
			Handle_TCP_ST_LISTEN(mtcp, cur_ts, cur_stream, tcph);
		else {
			Handle_TCP_ST_SYN_RCVD(mtcp, cur_ts, cur_stream, tcph, ack_seq);
			if (payloadlen > 0 && cur_stream->state == TCP_ST_ESTABLISHED) {
				Handle_TCP_ST_ESTABLISHED(mtcp, cur_ts, cur_stream, tcph,
							  seq, ack_seq, payload,
							  payloadlen, window);
			}
		}
		break;

	case TCP_ST_ESTABLISHED:
		Handle_TCP_ST_ESTABLISHED(mtcp, cur_ts, cur_stream, tcph, 
				seq, ack_seq, payload, payloadlen, window);
		break;

	case TCP_ST_CLOSE_WAIT:
		Handle_TCP_ST_CLOSE_WAIT(mtcp, cur_ts, cur_stream, tcph, seq, ack_seq,
				payloadlen, window);
		break;

	case TCP_ST_LAST_ACK:
		Handle_TCP_ST_LAST_ACK(mtcp, cur_ts, iph, ip_len, cur_stream, tcph, 
				seq, ack_seq, payloadlen, window);
		break;
	
	case TCP_ST_FIN_WAIT_1:
		Handle_TCP_ST_FIN_WAIT_1(mtcp, cur_ts, cur_stream, tcph, seq, ack_seq,
				payload, payloadlen, window);
		break;

	case TCP_ST_FIN_WAIT_2:
		Handle_TCP_ST_FIN_WAIT_2(mtcp, cur_ts, cur_stream, tcph, seq, ack_seq, 
				payload, payloadlen, window);
		break;

	case TCP_ST_CLOSING:
		Handle_TCP_ST_CLOSING(mtcp, cur_ts, cur_stream, tcph, seq, ack_seq,
				payloadlen, window);
		break;

	case TCP_ST_TIME_WAIT:
		/* the only thing that can arrive in this state is a retransmission 
		   of the remote FIN. Acknowledge it, and restart the 2 MSL timeout */
		if (cur_stream->on_timewait_list) {
			RemoveFromTimewaitList(mtcp, cur_stream);
			AddtoTimewaitList(mtcp, cur_stream, cur_ts);
		}
		AddtoControlList(mtcp, cur_stream, cur_ts);
		break;

	case TCP_ST_CLOSED:
		break;

	}

	return TRUE;
}
