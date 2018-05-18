#include "timer.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "stat.h"
#include "debug.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*----------------------------------------------------------------------------*/
struct rto_hashstore*
InitRTOHashstore()
{
	int i;
	struct rto_hashstore* hs = calloc(1, sizeof(struct rto_hashstore));
	if (!hs) {
		TRACE_ERROR("calloc: InitHashStore");
		return 0;
	}

	for (i = 0; i < RTO_HASH; i++)
		TAILQ_INIT(&hs->rto_list[i]);
		
	TAILQ_INIT(&hs->rto_list[RTO_HASH]);

	return hs;
}
/*----------------------------------------------------------------------------*/
inline void 
AddtoRTOList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (!mtcp->rto_list_cnt) {
		mtcp->rto_store->rto_now_idx = 0;
		mtcp->rto_store->rto_now_ts = cur_stream->sndvar->ts_rto;
	}

	if (cur_stream->on_rto_idx < 0 ) {
		if (cur_stream->on_timewait_list) {
			TRACE_ERROR("Stream %u: cannot be in both "
					"rto and timewait list.\n", cur_stream->id);
#ifdef DUMP_STREAM
			DumpStream(mtcp, cur_stream);
#endif
			return;
		}

		int diff = (int32_t)(cur_stream->sndvar->ts_rto - mtcp->rto_store->rto_now_ts);
		if (diff < RTO_HASH) {
			int offset= (diff + mtcp->rto_store->rto_now_idx) % RTO_HASH;
			cur_stream->on_rto_idx = offset;
			TAILQ_INSERT_TAIL(&(mtcp->rto_store->rto_list[offset]), 
					cur_stream, sndvar->timer_link);
		} else {
			cur_stream->on_rto_idx = RTO_HASH;
			TAILQ_INSERT_TAIL(&(mtcp->rto_store->rto_list[RTO_HASH]), 
					cur_stream, sndvar->timer_link);
		}
		mtcp->rto_list_cnt++;
	}
}
/*----------------------------------------------------------------------------*/
inline void 
RemoveFromRTOList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (cur_stream->on_rto_idx < 0) {
//		assert(0);
		return;
	}
	
	TAILQ_REMOVE(&mtcp->rto_store->rto_list[cur_stream->on_rto_idx], 
			cur_stream, sndvar->timer_link);
	cur_stream->on_rto_idx = -1;

	mtcp->rto_list_cnt--;
}
/*----------------------------------------------------------------------------*/
inline void 
AddtoTimewaitList(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t cur_ts)
{
	cur_stream->rcvvar->ts_tw_expire = cur_ts + CONFIG.tcp_timewait;

	if (cur_stream->on_timewait_list) {
		// Update list in sorted way by ts_tw_expire
		TAILQ_REMOVE(&mtcp->timewait_list, cur_stream, sndvar->timer_link);
		TAILQ_INSERT_TAIL(&mtcp->timewait_list, cur_stream, sndvar->timer_link);	
	} else {
		if (cur_stream->on_rto_idx >= 0) {
			TRACE_DBG("Stream %u: cannot be in both "
					"timewait and rto list.\n", cur_stream->id);
			//assert(0);
#ifdef DUMP_STREAM
			DumpStream(mtcp, cur_stream);
#endif
			RemoveFromRTOList(mtcp, cur_stream);
		}

		cur_stream->on_timewait_list = TRUE;
		TAILQ_INSERT_TAIL(&mtcp->timewait_list, cur_stream, sndvar->timer_link);
		mtcp->timewait_list_cnt++;
	}
}
/*----------------------------------------------------------------------------*/
inline void 
RemoveFromTimewaitList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (!cur_stream->on_timewait_list) {
		assert(0);
		return;
	}
	
	TAILQ_REMOVE(&mtcp->timewait_list, cur_stream, sndvar->timer_link);
	cur_stream->on_timewait_list = FALSE;
	mtcp->timewait_list_cnt--;
}
/*----------------------------------------------------------------------------*/
inline void 
AddtoTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (cur_stream->on_timeout_list) {
		assert(0);
		return;
	}

	cur_stream->on_timeout_list = TRUE;
	TAILQ_INSERT_TAIL(&mtcp->timeout_list, cur_stream, sndvar->timeout_link);
	mtcp->timeout_list_cnt++;
}
/*----------------------------------------------------------------------------*/
inline void 
RemoveFromTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (cur_stream->on_timeout_list) {
		cur_stream->on_timeout_list = FALSE;
		TAILQ_REMOVE(&mtcp->timeout_list, cur_stream, sndvar->timeout_link);
		mtcp->timeout_list_cnt--;
	}
}
/*----------------------------------------------------------------------------*/
inline void 
UpdateTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream)
{
	if (cur_stream->on_timeout_list) {
		TAILQ_REMOVE(&mtcp->timeout_list, cur_stream, sndvar->timeout_link);
		TAILQ_INSERT_TAIL(&mtcp->timeout_list, cur_stream, sndvar->timeout_link);
	}
}
/*----------------------------------------------------------------------------*/
inline void
UpdateRetransmissionTimer(mtcp_manager_t mtcp, 
		tcp_stream *cur_stream, uint32_t cur_ts)
{
	/* Update the retransmission timer */
	assert(cur_stream->sndvar->rto > 0);
	cur_stream->sndvar->nrtx = 0;

	/* if in rto list, remove it */
	if (cur_stream->on_rto_idx >= 0) {
		RemoveFromRTOList(mtcp, cur_stream);
	}

	/* Reset retransmission timeout */
	if (TCP_SEQ_GT(cur_stream->snd_nxt, cur_stream->sndvar->snd_una)) {
		/* there are packets sent but not acked */
		/* update rto timestamp */
		cur_stream->sndvar->ts_rto = cur_ts + cur_stream->sndvar->rto;
		AddtoRTOList(mtcp, cur_stream);

	} else {
		/* all packets are acked */
		TRACE_RTO("All packets are acked. snd_una: %u, snd_nxt: %u\n", 
				cur_stream->sndvar->snd_una, cur_stream->snd_nxt);
	}
}
/*----------------------------------------------------------------------------*/
int 
HandleRTO(mtcp_manager_t mtcp, uint32_t cur_ts, tcp_stream *cur_stream)
{
	uint8_t backoff;

	TRACE_RTO("Stream %d Timeout! rto: %u (%ums), snd_una: %u, snd_nxt: %u\n", 
			cur_stream->id, cur_stream->sndvar->rto, TS_TO_MSEC(cur_stream->sndvar->rto), 
			cur_stream->sndvar->snd_una, cur_stream->snd_nxt);
	assert(cur_stream->sndvar->rto > 0);

	/* if the stream is ready to be closed, don't handle RTO */
	if (cur_stream->close_reason != TCP_NOT_CLOSED)
		return 0;
	
	/* count number of retransmissions */
	if (cur_stream->sndvar->nrtx < TCP_MAX_RTX) {
		cur_stream->sndvar->nrtx++;
	} else {
		/* if it exceeds the threshold, destroy and notify to application */
		TRACE_RTO("Stream %d: Exceed MAX_RTX\n", cur_stream->id);
		if (cur_stream->state < TCP_ST_ESTABLISHED) {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_CONN_FAIL;
			DestroyTCPStream(mtcp, cur_stream);
		} else {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_CONN_LOST;
			if (cur_stream->socket) {
				RaiseErrorEvent(mtcp, cur_stream);
			} else {
				DestroyTCPStream(mtcp, cur_stream);
			}
		}
		
		return ERROR;
	}
	if (cur_stream->sndvar->nrtx > cur_stream->sndvar->max_nrtx) {
		cur_stream->sndvar->max_nrtx = cur_stream->sndvar->nrtx;
	}

	/* update rto timestamp */
	if (cur_stream->state >= TCP_ST_ESTABLISHED) {
		uint32_t rto_prev;
		backoff = MIN(cur_stream->sndvar->nrtx, TCP_MAX_BACKOFF);

		rto_prev = cur_stream->sndvar->rto;
		cur_stream->sndvar->rto = ((cur_stream->rcvvar->srtt >> 3) + 
				cur_stream->rcvvar->rttvar) << backoff;
		if (cur_stream->sndvar->rto <= 0) {
			TRACE_RTO("Stream %d current rto: %u, prev: %u, state: %s\n", 
					cur_stream->id, cur_stream->sndvar->rto, rto_prev, 
					TCPStateToString(cur_stream));
			cur_stream->sndvar->rto = rto_prev;
		}
	} else if (cur_stream->state >= TCP_ST_SYN_SENT) {
		/* if there is no rtt measured, update rto based on the previous one */
		if (cur_stream->sndvar->nrtx < TCP_MAX_BACKOFF) {
			cur_stream->sndvar->rto <<= 1;
		}
	}
	//cur_stream->sndvar->ts_rto = cur_ts + cur_stream->sndvar->rto;

	/* reduce congestion window and ssthresh */
	cur_stream->sndvar->ssthresh = MIN(cur_stream->sndvar->cwnd, cur_stream->sndvar->peer_wnd) / 2;
	if (cur_stream->sndvar->ssthresh < (2 * cur_stream->sndvar->mss)) {
		cur_stream->sndvar->ssthresh = cur_stream->sndvar->mss * 2;
	}
	cur_stream->sndvar->cwnd = cur_stream->sndvar->mss;
	TRACE_CONG("Stream %d Timeout. cwnd: %u, ssthresh: %u\n", 
			cur_stream->id, cur_stream->sndvar->cwnd, cur_stream->sndvar->ssthresh);

#if RTM_STAT
	/* update retransmission stats */
	cur_stream->sndvar->rstat.rto_cnt++;
	cur_stream->sndvar->rstat.rto_bytes += (cur_stream->snd_nxt - cur_stream->sndvar->snd_una);
#endif

	/* Retransmission */
	if (cur_stream->state == TCP_ST_SYN_SENT) {
		/* SYN lost */
		if (cur_stream->sndvar->nrtx > TCP_MAX_SYN_RETRY) {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_CONN_FAIL;
			TRACE_RTO("Stream %d: SYN retries exceed maximum retries.\n", 
					cur_stream->id);
			if (cur_stream->socket) {
				RaiseErrorEvent(mtcp, cur_stream);
			} else {
				DestroyTCPStream(mtcp, cur_stream);
			}

			return ERROR;
		}
		TRACE_RTO("Stream %d Retransmit SYN. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);

	} else if (cur_stream->state == TCP_ST_SYN_RCVD) {
		/* SYN/ACK lost */
		TRACE_RTO("Stream %d: Retransmit SYN/ACK. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);

	} else if (cur_stream->state == TCP_ST_ESTABLISHED) {
		/* Data lost */
		TRACE_RTO("Stream %d: Retransmit data. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);

	} else if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
		/* Data lost */
		TRACE_RTO("Stream %d: Retransmit data. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);

	} else if (cur_stream->state == TCP_ST_LAST_ACK) {
		/* FIN/ACK lost */
		TRACE_RTO("Stream %d: Retransmit FIN/ACK. "
				"snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);

	} else if (cur_stream->state == TCP_ST_FIN_WAIT_1) {
		/* FIN lost */
		TRACE_RTO("Stream %d: Retransmit FIN. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);
	} else if (cur_stream->state == TCP_ST_CLOSING) {
		TRACE_RTO("Stream %d: Retransmit ACK. snd_nxt: %u, snd_una: %u\n", 
				cur_stream->id, cur_stream->snd_nxt, cur_stream->sndvar->snd_una);
		//TRACE_DBG("Stream %d: Retransmitting at CLOSING\n", cur_stream->id);

	} else {
		TRACE_ERROR("Stream %d: not implemented state! state: %s, rto: %u\n", 
				cur_stream->id, 
				TCPStateToString(cur_stream), cur_stream->sndvar->rto);
		assert(0);
		return ERROR;
	}

	if (cur_stream->have_reset &&
	    cur_stream->state == TCP_ST_SYN_RCVD) {
		DestroyTCPStream(mtcp, cur_stream);
		return 0;
	}

	cur_stream->snd_nxt = cur_stream->sndvar->snd_una;
	if (cur_stream->state == TCP_ST_ESTABLISHED || 
			cur_stream->state == TCP_ST_CLOSE_WAIT) {
		/* retransmit data at ESTABLISHED state */
		AddtoSendList(mtcp, cur_stream);

	} else if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_LAST_ACK) {

		if (cur_stream->sndvar->fss == 0) {
			TRACE_ERROR("Stream %u: fss not set.\n", cur_stream->id);
		}
		/* decide to retransmit data or control packet */
		if (TCP_SEQ_LT(cur_stream->snd_nxt, cur_stream->sndvar->fss)) {
			/* need to retransmit data */
			if (cur_stream->sndvar->on_control_list) {
				RemoveFromControlList(mtcp, cur_stream);
			}
			cur_stream->control_list_waiting = TRUE;
			AddtoSendList(mtcp, cur_stream);

		} else {
			/* need to retransmit control packet */
			AddtoControlList(mtcp, cur_stream, cur_ts);
		}

	} else {
		AddtoControlList(mtcp, cur_stream, cur_ts);
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static inline void 
RearrangeRTOStore(mtcp_manager_t mtcp) {
	tcp_stream *walk, *next;
	struct rto_head* rto_list = &mtcp->rto_store->rto_list[RTO_HASH];
	int cnt = 0;

	for (walk = TAILQ_FIRST(rto_list);
			walk != NULL; walk = next) {
		next = TAILQ_NEXT(walk, sndvar->timer_link);

		int diff = (int32_t)(mtcp->rto_store->rto_now_ts - walk->sndvar->ts_rto);
		if (diff < RTO_HASH) {
			int offset = (diff + mtcp->rto_store->rto_now_idx) % RTO_HASH;
			TAILQ_REMOVE(&mtcp->rto_store->rto_list[RTO_HASH],
					            walk, sndvar->timer_link);
			walk->on_rto_idx = offset;
			TAILQ_INSERT_TAIL(&(mtcp->rto_store->rto_list[offset]),
					walk, sndvar->timer_link);
		}
		cnt++;
	}	
}
/*----------------------------------------------------------------------------*/
void
CheckRtmTimeout(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh)
{
	tcp_stream *walk, *next;
	struct rto_head* rto_list;
	int cnt;
	
	if (!mtcp->rto_list_cnt) {
		return;
	}

	STAT_COUNT(mtcp->runstat.rounds_rtocheck);

	cnt = 0;
			
	while (1) {
		
		rto_list = &mtcp->rto_store->rto_list[mtcp->rto_store->rto_now_idx];
		if ((int32_t)(cur_ts - mtcp->rto_store->rto_now_ts) < 0) {
			break;
		}
		
		for (walk = TAILQ_FIRST(rto_list);
				walk != NULL; walk = next) {
			if (++cnt > thresh) {
				break;
			}
			next = TAILQ_NEXT(walk, sndvar->timer_link);

			TRACE_LOOP("Inside rto list. cnt: %u, stream: %d\n", 
					cnt, walk->s_id);

			if (walk->on_rto_idx >= 0) {
				TAILQ_REMOVE(rto_list, walk, sndvar->timer_link);
				mtcp->rto_list_cnt--;
				walk->on_rto_idx = -1;
				HandleRTO(mtcp, cur_ts, walk);
			} else {
				TRACE_ERROR("Stream %d: not on rto list.\n", walk->id);
#ifdef DUMP_STREAM
				DumpStream(mtcp, walk);
#endif
			}
		}

		if (cnt > thresh) {
			break;
		} else {
			mtcp->rto_store->rto_now_idx = (mtcp->rto_store->rto_now_idx + 1) % RTO_HASH;
			mtcp->rto_store->rto_now_ts++;
			if (!(mtcp->rto_store->rto_now_idx % 1000)) {
				RearrangeRTOStore(mtcp);
			}
		}

	}

	TRACE_ROUND("Checking retransmission timeout. cnt: %d\n", cnt);
}
/*----------------------------------------------------------------------------*/
void 
CheckTimewaitExpire(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh)
{
	tcp_stream *walk, *next;
	int cnt;

	STAT_COUNT(mtcp->runstat.rounds_twcheck);

	cnt = 0;

	for (walk = TAILQ_FIRST(&mtcp->timewait_list); 
				walk != NULL; walk = next) {
		if (++cnt > thresh)
			break;
		next = TAILQ_NEXT(walk, sndvar->timer_link);
		
		TRACE_LOOP("Inside timewait list. cnt: %u, stream: %d\n", 
				cnt, walk->s_id);
		
		if (walk->on_timewait_list) {
			if ((int32_t)(cur_ts - walk->rcvvar->ts_tw_expire) >= 0) {
				if (!walk->sndvar->on_control_list) {
					
					TAILQ_REMOVE(&mtcp->timewait_list, walk, sndvar->timer_link);
					walk->on_timewait_list = FALSE;
					mtcp->timewait_list_cnt--;

					walk->state = TCP_ST_CLOSED;
					walk->close_reason = TCP_ACTIVE_CLOSE;
					TRACE_STATE("Stream %d: TCP_ST_CLOSED\n", walk->id);
					DestroyTCPStream(mtcp, walk);
				}
			} else {
				break;
			}
		} else {
			TRACE_ERROR("Stream %d: not on timewait list.\n", walk->id);
#ifdef DUMP_STREAM
			DumpStream(mtcp, walk);
#endif
		}
	}

	TRACE_ROUND("Checking timewait timeout. cnt: %d\n", cnt);
}
/*----------------------------------------------------------------------------*/
void 
CheckConnectionTimeout(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh)
{
	tcp_stream *walk, *next;
	int cnt;

	STAT_COUNT(mtcp->runstat.rounds_tocheck);

	cnt = 0;
	for (walk = TAILQ_FIRST(&mtcp->timeout_list);
			walk != NULL; walk = next) {
		if (++cnt > thresh)
			break;
		next = TAILQ_NEXT(walk, sndvar->timeout_link);

		if ((int32_t)(cur_ts - walk->last_active_ts) >= 
				CONFIG.tcp_timeout) {

			walk->on_timeout_list = FALSE;
			TAILQ_REMOVE(&mtcp->timeout_list, walk, sndvar->timeout_link);
			mtcp->timeout_list_cnt--;
			walk->state = TCP_ST_CLOSED;
			walk->close_reason = TCP_TIMEDOUT;
			if (walk->socket) {
				RaiseErrorEvent(mtcp, walk);
			} else {
				DestroyTCPStream(mtcp, walk);
			}
		} else {
			break;
		}

	}
}
/*----------------------------------------------------------------------------*/
