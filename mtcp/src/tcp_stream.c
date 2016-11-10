#include "debug.h"
#include <string.h>

#include "config.h"
#include "tcp_stream.h"
#include "fhash.h"
#include "tcp.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "tcp_ring_buffer.h"
#include "tcp_send_buffer.h"
#include "eventpoll.h"
#include "ip_out.h"
#include "timer.h"
#include "tcp_rb.h"
/*---------------------------------------------------------------------------*/
char *state_str[] = {
	"TCP_ST_CLOSED", 
	"TCP_ST_LISTEN", 
	"TCP_ST_SYN_SENT", 
	"TCP_ST_SYN_RCVD", 
	"TCP_ST_ESTABILSHED", 
	"TCP_ST_FIN_WAIT_1", 
	"TCP_ST_FIN_WAIT_2", 
	"TCP_ST_CLOSE_WAIT", 
	"TCP_ST_CLOSING", 
	"TCP_ST_LAST_ACK", 
	"TCP_ST_TIME_WAIT",
	"TCP_ST_CLOSED_RSVD"
};
/*---------------------------------------------------------------------------*/
char *close_reason_str[] = {
	"NOT_CLOSED", 
	"CLOSE", 
	"CLOSED", 
	"CONN_FAIL", 
	"CONN_LOST", 
	"RESET", 
	"NO_MEM", 
	"DENIED", 
	"TIMEDOUT"
};
/*---------------------------------------------------------------------------*/
static __thread unsigned long next = 1;
/* Function retrieved from POSIX.1-2001 standard */
/* RAND_MAX assumed to be 32767 */
static int
posix_seq_rand(void) {
	next = next * 1103515245 + 12345;
	return ((unsigned)(next/65536) % 32768);
}
/*---------------------------------------------------------------------------*/
void
posix_seq_srand(unsigned seed) {
	next = seed % 32768;
}
/*---------------------------------------------------------------------------*/
uint32_t
FetchSeqDrift(struct tcp_stream *stream, uint32_t seq)
{
	int i = 0;
	uint8_t flag = 1;
	int count;

	i = stream->sndvar->sre_index - 1;
	if (i == -1) i = SRE_MAX - 1;
	count = 0;

	while (flag) {
		if (stream->sndvar->sre[i].seq_base == 0)
			return 0;
		else if (seq >= stream->sndvar->sre[i].seq_base)
			return stream->sndvar->sre[i].seq_off;
		
		i--;
		if (i == -1) i = SRE_MAX - 1;
		count++;
		if (count == SRE_MAX)
			flag = 1;
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
int
TcpSeqChange(socket_map_t socket, uint32_t seq_drift, int side, uint32_t seqno)
{
	struct tcp_stream *mstrm, *stream;

	if (side != MOS_SIDE_CLI && side != MOS_SIDE_SVR) {
		TRACE_ERROR("Invalid side requested!\n");
		errno = EINVAL;
		return -1;
	}
	
	mstrm = socket->monitor_stream->stream;
	stream = (side == mstrm->side) ? mstrm : mstrm->pair_stream;
	if (stream == NULL) {
		TRACE_ERROR("Stream pointer for sockid: %u not found!\n",
			    socket->id);
		errno = EBADF;
		return -1;
	}

	stream->sndvar->sre[stream->sndvar->sre_index].seq_off = seq_drift;
	stream->sndvar->sre[stream->sndvar->sre_index].seq_off +=
		(stream->sndvar->sre_index == 0) ? stream->sndvar->sre[SRE_MAX - 1].seq_off :
		stream->sndvar->sre[stream->sndvar->sre_index - 1].seq_off;
	stream->sndvar->sre[stream->sndvar->sre_index].seq_base = seqno;
	stream->sndvar->sre_index = (stream->sndvar->sre_index + 1) & (SRE_MAX - 1);

	return 0;
} 
/*---------------------------------------------------------------------------*/
/**
 * FYI: This is NOT a read-only return!
 */
int
GetFragInfo(socket_map_t sock, int side, void *optval, socklen_t *len) 
{
	struct tcp_stream *stream;

	stream = NULL;
	if (!*len || ( *len % sizeof(tcpfrag_t) != 0))
		goto frag_info_error;

	if (side != MOS_SIDE_CLI && side != MOS_SIDE_SVR) {
		TRACE_ERROR("Invalid side requested!\n");
		exit(EXIT_FAILURE);
		return -1;
	}

	struct tcp_stream *mstrm = sock->monitor_stream->stream;
	stream = (side == mstrm->side) ? mstrm : mstrm->pair_stream;

	if (stream == NULL) goto frag_info_error;
	
	/* First check if the tcp ring buffer even has anything */
	if (stream->rcvvar != NULL &&
	    stream->rcvvar->rcvbuf != NULL) {
		tcprb_t *rcvbuf = stream->rcvvar->rcvbuf;
		struct tcp_ring_fragment *out = (struct tcp_ring_fragment *)optval;
		int const maxout = *len;
		*len = 0;
		struct _tcpfrag_t *walk;
		TAILQ_FOREACH(walk, &rcvbuf->frags, link) {
			if (*len == maxout)
				break;
			out[*len].offset = walk->head;
			out[*len].len = walk->tail - walk->head;
			(*len)++;
		}
		if (*len != maxout) {
			/* set zero sentinel */
			out[*len].offset = 0;
			out[*len].len = 0;
		}
	} else
		goto frag_info_error;

	return 0;

 frag_info_error:
	optval = NULL;
	*len = 0;
	return -1;
}
/*---------------------------------------------------------------------------*/
/**
 * Comments later...
 */
int
GetBufInfo(socket_map_t sock, int side, void *optval, socklen_t *len)
{
	struct tcp_stream *stream;
	struct tcp_buf_info *tbi;
	
	tbi = (struct tcp_buf_info *)optval;
	memset(tbi, 0, sizeof(struct tcp_buf_info));
	stream = NULL;

	if (*len != sizeof(struct tcp_buf_info)) {
		errno = EINVAL;
		goto buf_info_error;
	}

	if (side != MOS_SIDE_CLI && side != MOS_SIDE_SVR) {
		TRACE_ERROR("Invalid side requested!\n");
		errno = EINVAL;
		goto buf_info_error;
	}

	struct tcp_stream *mstrm = sock->monitor_stream->stream;
	stream = (side == mstrm->side) ? mstrm : mstrm->pair_stream;

	/* First check if the tcp ring buffer even has anything */
	if (stream != NULL &&
	    stream->rcvvar != NULL &&
	    stream->rcvvar->rcvbuf != NULL) {
		tcprb_t *rcvbuf = stream->rcvvar->rcvbuf;
		tcpfrag_t *f = TAILQ_LAST(&rcvbuf->frags, flist);
		tbi->tcpbi_init_seq = stream->rcvvar->irs + 1;
		tbi->tcpbi_last_byte_read = rcvbuf->pile;
		tbi->tcpbi_next_byte_expected = rcvbuf->pile + tcprb_cflen(rcvbuf);
		tbi->tcpbi_last_byte_received = (f ? f->tail : rcvbuf->head);
	} else {
		errno = ENODATA;
		goto buf_info_error;
	}
	
	return 0;

 buf_info_error:
	optval = NULL;
	*len = 0;
	return -1;
}
/*---------------------------------------------------------------------------*/
int
DisableBuf(socket_map_t sock, int side)
{
#ifdef DBGMSG
	__PREPARE_DBGLOGGING();
#endif
	struct tcp_stream *stream;
	int rc = 0;

	switch (sock->socktype) {
	case MOS_SOCK_MONITOR_STREAM:
		if (side == MOS_SIDE_CLI)
			sock->monitor_listener->client_buf_mgmt = 0;
		else if (side == MOS_SIDE_SVR)
			sock->monitor_listener->server_buf_mgmt = 0;
		else {
			assert(0);
			TRACE_DBG("Invalid side!\n");
			rc = -1;
		}
		break;
	case MOS_SOCK_MONITOR_STREAM_ACTIVE:
		stream = sock->monitor_stream->stream;
		if (stream->side != side)
			stream = stream->pair_stream;
		assert(stream->side == side);
		stream->buffer_mgmt = 0;
		break;
	default:
		assert(0);
		TRACE_DBG("Can't disable buf for invalid socket!\n");
		rc = -1;
	}
	
	return rc;
}
/*---------------------------------------------------------------------------*/
int
GetLastTimestamp(struct tcp_stream *stream, uint32_t *usecs, socklen_t *len)
{
#ifdef DBGMSG
	__PREPARE_DBGLOGGING();
#endif
	if (*len < sizeof(uint32_t)) {
		TRACE_DBG("Size passed is not >= sizeof(uint32_t)!\n");
		return -1;
	}
	
	*usecs = (stream->last_active_ts >
		  stream->pair_stream->last_active_ts) 
		?
		TS_TO_USEC(stream->last_active_ts) : 
		TS_TO_USEC(stream->pair_stream->last_active_ts);
	
	return 0;
}
/*---------------------------------------------------------------------------*/
inline int
GetTCPState(struct tcp_stream *stream, int side,
			void *optval, socklen_t *optlen)
{
	if (!stream || !(stream = (side == stream->side) ? stream : stream->pair_stream))
		return -1;
	*(int *)optval = (int)((stream->state == TCP_ST_CLOSED_RSVD) ?
						   TCP_ST_CLOSED : stream->state);
	return 0;
}
/*---------------------------------------------------------------------------*/
inline char *
TCPStateToString(const tcp_stream *stream)
{
	return (stream) ? state_str[stream->state] : NULL;
}
/*---------------------------------------------------------------------------*/
inline void 
RaiseReadEvent(mtcp_manager_t mtcp, tcp_stream *stream)
{
	struct tcp_recv_vars *rcvvar;

	rcvvar = stream->rcvvar;
	
	if (HAS_STREAM_TYPE(stream, MOS_SOCK_STREAM)) {
		if (stream->socket && (stream->socket->epoll & MOS_EPOLLIN))
			AddEpollEvent(mtcp->ep, MOS_EVENT_QUEUE, stream->socket, MOS_EPOLLIN);
	} else if (rcvvar->rcvbuf && tcprb_cflen(rcvvar->rcvbuf) > 0) {
		/* 
		 * in case it is a monitoring socket, queue up the read events
		 * in the event_queue of only if the tcp_stream hasn't already
		 * been registered in the event queue
		 */
		int index;
		struct event_queue *eq;
		struct socket_map *walk;

		SOCKQ_FOREACH_START(walk, &stream->msocks) {
			assert(walk->socktype == MOS_SOCK_MONITOR_STREAM_ACTIVE);
			eq = walk->monitor_stream->monitor_listener->eq;

			/* if it already has read data register... then skip this step */
			if (stream->actions & MOS_ACT_READ_DATA)
				return;
			if (eq->num_events >= eq->size) {
				TRACE_ERROR("Exceeded epoll event queue! num_events: %d, "
						"size: %d\n", eq->num_events, eq->size);
				return;
			}

			index = eq->end++;
			eq->events[index].ev.events = MOS_EPOLLIN;
			eq->events[index].ev.data.ptr = (void *)stream;

			if (eq->end >= eq->size) {
				eq->end = 0;
			}
			eq->num_events++;
			stream->actions |= MOS_ACT_READ_DATA;
		} SOCKQ_FOREACH_END;
	} else {	
		TRACE_EPOLL("Stream %d: Raising read without a socket!\n", stream->id);
	}
}
/*---------------------------------------------------------------------------*/
inline void 
RaiseWriteEvent(mtcp_manager_t mtcp, tcp_stream *stream)
{
	if (stream->socket) {
		if (stream->socket->epoll & MOS_EPOLLOUT) {
			AddEpollEvent(mtcp->ep, 
				      MOS_EVENT_QUEUE, stream->socket, MOS_EPOLLOUT);
		}
	} else {
		TRACE_EPOLL("Stream %d: Raising write without a socket!\n", stream->id);
	}
}
/*---------------------------------------------------------------------------*/
inline void 
RaiseCloseEvent(mtcp_manager_t mtcp, tcp_stream *stream)
{
	if (stream->socket) {
		if (stream->socket->epoll & MOS_EPOLLRDHUP) {
			AddEpollEvent(mtcp->ep, 
					MOS_EVENT_QUEUE, stream->socket, MOS_EPOLLRDHUP);
		} else if (stream->socket->epoll & MOS_EPOLLIN) {
			AddEpollEvent(mtcp->ep, 
				      MOS_EVENT_QUEUE, stream->socket, MOS_EPOLLIN);
		}
	} else {
		TRACE_EPOLL("Stream %d: Raising close without a socket!\n", stream->id);
	}
}
/*---------------------------------------------------------------------------*/
inline int 
RaiseErrorEvent(mtcp_manager_t mtcp, tcp_stream *stream)
{
	if (stream->socket) {
		if (stream->socket->epoll & MOS_EPOLLERR) {
			/* passing closing reason for error notification */
			return AddEpollEvent(mtcp->ep, 
					     MOS_EVENT_QUEUE, stream->socket, MOS_EPOLLERR);
		}
	} else {
		TRACE_EPOLL("Stream %d: Raising error without a socket!\n", stream->id);
	}
	return -1;
}
/*----------------------------------------------------------------------------*/
int
AddMonitorStreamSockets(mtcp_manager_t mtcp, struct tcp_stream *stream)
{
	struct mtcp_context mctx; 
	int socktype;

	mctx.cpu = mtcp->ctx->cpu;
	struct mon_listener *walk;

	// traverse the passive socket's list
	TAILQ_FOREACH(walk, &mtcp->monitors, link) {
		socktype = walk->socket->socktype;

		if (socktype != MOS_SOCK_MONITOR_STREAM)
			continue;
		
		/* mtcp_bind_monitor_filter()
		 * - create an monitor active socket only for the filter-passed flows
		 * - we use the result (= tag) from DetectStreamType() to avoid
		 *   evaluating the same BPF filter twice */		
		if (!walk->is_stream_syn_filter_hit) {
			continue;
		}

		struct socket_map *s =
			AllocateSocket(&mctx, MOS_SOCK_MONITOR_STREAM_ACTIVE);
		if (!s)
			return -1;

		s->monitor_stream->socket = s;
		s->monitor_stream->stream = stream;
		s->monitor_stream->monitor_listener = walk;
		s->monitor_stream->client_buf_mgmt = walk->client_buf_mgmt;
		s->monitor_stream->server_buf_mgmt = walk->server_buf_mgmt;
		s->monitor_stream->client_mon = walk->client_mon;
		s->monitor_stream->server_mon = walk->server_mon;
#ifdef NEWEV
		s->monitor_stream->stree_dontcare =
			s->monitor_stream->monitor_listener->stree_dontcare;
		s->monitor_stream->stree_pre_rcv =
			s->monitor_stream->monitor_listener->stree_pre_rcv;
		s->monitor_stream->stree_post_snd =
			s->monitor_stream->monitor_listener->stree_post_snd;
		if (s->monitor_stream->stree_dontcare)
			stree_inc_ref(s->monitor_stream->stree_dontcare);
		if (s->monitor_stream->stree_pre_rcv)
			stree_inc_ref(s->monitor_stream->stree_pre_rcv);
		if (s->monitor_stream->stree_post_snd)
			stree_inc_ref(s->monitor_stream->stree_post_snd);
#else
		InitEvP(&s->monitor_stream->dontcare_evp,
				&walk->dontcare_evb);
		InitEvP(&s->monitor_stream->pre_tcp_evp,
				&walk->pre_tcp_evb);
		InitEvP(&s->monitor_stream->post_tcp_evp,
				&walk->post_tcp_evb);
#endif

		SOCKQ_INSERT_TAIL(&stream->msocks, s);
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
int
DestroyMonitorStreamSocket(mtcp_manager_t mtcp, socket_map_t msock)
{
	struct mtcp_context mctx;
	int socktype, sockid, rc;
	
	if (msock == NULL) {
		TRACE_DBG("Stream socket does not exist!\n");
		/* exit(-1); */
		return 0;
	}

	rc = 0;
	mctx.cpu = mtcp->ctx->cpu;
	socktype = msock->socktype;
	sockid = msock->id;

	switch (socktype) {
	case MOS_SOCK_MONITOR_STREAM_ACTIVE:
		FreeSocket(&mctx, sockid, socktype);
		break;
	case MOS_SOCK_MONITOR_RAW:
		/* do nothing since all raw sockets point to the same socket */
		break;
	default:
		TRACE_DBG("Trying to destroy a monitor socket for an unsupported type!\n");
		rc = -1;
		/* exit(-1); */
		break;
	}
	
	return rc;
}
/*---------------------------------------------------------------------------*/
tcp_stream *
CreateTCPStream(mtcp_manager_t mtcp, socket_map_t socket, int type, 
		uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport,
		unsigned int *hash)
{
	tcp_stream *stream = NULL;
	int ret;
	/* stand-alone monitor does not need this since it is single-threaded */
	bool flow_lock = type & STREAM_TYPE(MOS_SOCK_STREAM);
	//bool flow_lock = false;

	if (flow_lock)
		pthread_mutex_lock(&mtcp->ctx->flow_pool_lock);

	stream = (tcp_stream *)MPAllocateChunk(mtcp->flow_pool);
	if (!stream) {
		TRACE_ERROR("Cannot allocate memory for the stream. "
				"g_config.mos->max_concurrency: %d, concurrent: %u\n", 
				g_config.mos->max_concurrency, mtcp->flow_cnt);
		if (flow_lock)
			pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);
		return NULL;
	}
	memset(stream, 0, sizeof(tcp_stream));

	stream->rcvvar = (struct tcp_recv_vars *)MPAllocateChunk(mtcp->rv_pool);
	if (!stream->rcvvar) {
		MPFreeChunk(mtcp->flow_pool, stream);
		if (flow_lock)
			pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);
		return NULL;
	}
	memset(stream->rcvvar, 0, sizeof(struct tcp_recv_vars));

	/* stand-alone monitor does not need to do this */
	stream->sndvar = (struct tcp_send_vars *)MPAllocateChunk(mtcp->sv_pool);
	if (!stream->sndvar) {
		MPFreeChunk(mtcp->rv_pool, stream->rcvvar);
		MPFreeChunk(mtcp->flow_pool, stream);
		if (flow_lock)
			pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);
		return NULL;
	}
	//if (HAS_STREAM_TYPE(stream, MOS_SOCK_STREAM))
		memset(stream->sndvar, 0, sizeof(struct tcp_send_vars));

	stream->id = mtcp->g_id++;
	stream->saddr = saddr;
	stream->sport = sport;
	stream->daddr = daddr;
	stream->dport = dport;

	ret = HTInsert(mtcp->tcp_flow_table, stream, hash);
	if (ret < 0) {
		TRACE_ERROR("Stream %d: "
				"Failed to insert the stream into hash table.\n", stream->id);
		MPFreeChunk(mtcp->flow_pool, stream);
		if (flow_lock)
			pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);
		return NULL;
	}
	stream->on_hash_table = TRUE;
	mtcp->flow_cnt++;

	SOCKQ_INIT(&stream->msocks);

	/*
	 * if an embedded monitor is attached... 
	 *  create monitor stream socket now! 
	 * If socket type is raw.. then don't create it
	 */
	if ((mtcp->num_msp > 0) &&
		(type & STREAM_TYPE(MOS_SOCK_MONITOR_STREAM_ACTIVE)))
		if (AddMonitorStreamSockets(mtcp, stream) < 0)
			TRACE_DBG("Could not create monitor stream socket!\n");

	if (flow_lock)
		pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);

	if (socket) {
		stream->socket = socket;
		socket->stream = stream;
	}

	stream->stream_type = type;
	stream->state = TCP_ST_LISTEN;
	/* This is handled by core.c, tcp_in.c & tcp_out.c */
	/* stream->cb_events |= MOS_ON_TCP_STATE_CHANGE; */

	stream->on_rto_idx = -1;
	
	/* stand-alone monitor does not need to do this */
	stream->sndvar->mss = TCP_DEFAULT_MSS;
	stream->sndvar->wscale_mine = TCP_DEFAULT_WSCALE;
	stream->sndvar->wscale_peer = 0;

	if (HAS_STREAM_TYPE(stream, MOS_SOCK_STREAM)) {
		stream->sndvar->ip_id = 0;
		stream->sndvar->nif_out = GetOutputInterface(stream->daddr);

		stream->sndvar->iss = posix_seq_rand() % TCP_MAX_SEQ;
		//stream->sndvar->iss = 0;
		stream->snd_nxt = stream->sndvar->iss;
		stream->sndvar->snd_una = stream->sndvar->iss;
		stream->sndvar->snd_wnd = g_config.mos->wmem_size;
		stream->sndvar->rto = TCP_INITIAL_RTO;
#if USE_SPIN_LOCK
		if (pthread_spin_init(&stream->sndvar->write_lock, PTHREAD_PROCESS_PRIVATE)) {
			perror("pthread_spin_init of write_lock");
			pthread_spin_destroy(&stream->rcvvar->read_lock);
#else
		if (pthread_mutex_init(&stream->sndvar->write_lock, NULL)) {
			perror("pthread_mutex_init of write_lock");
			pthread_mutex_destroy(&stream->rcvvar->read_lock);
#endif
			return NULL;
		}
	}
	stream->rcvvar->irs = 0;

	stream->rcv_nxt = 0;
	stream->rcvvar->rcv_wnd = TCP_INITIAL_WINDOW;

	stream->rcvvar->snd_wl1 = stream->rcvvar->irs - 1;

	stream->buffer_mgmt = BUFMGMT_FULL;

	/* needs state update by default */
	stream->status_mgmt = 1;

#if USE_SPIN_LOCK
	if (pthread_spin_init(&stream->rcvvar->read_lock, PTHREAD_PROCESS_PRIVATE)) {
#else
	if (pthread_mutex_init(&stream->rcvvar->read_lock, NULL)) {
#endif
		perror("pthread_mutex_init of read_lock");
		return NULL;
	}

#ifdef STREAM
	uint8_t *sa;
	uint8_t *da;
	
	sa = (uint8_t *)&stream->saddr;
	da = (uint8_t *)&stream->daddr;
	TRACE_STREAM("CREATED NEW TCP STREAM %d: "
			"%u.%u.%u.%u(%d) -> %u.%u.%u.%u(%d) (ISS: %u)\n", stream->id, 
			sa[0], sa[1], sa[2], sa[3], ntohs(stream->sport), 
			da[0], da[1], da[2], da[3], ntohs(stream->dport), 
			stream->sndvar->iss);
#endif
	
	return stream;
}
/*----------------------------------------------------------------------------*/
inline tcp_stream *
CreateDualTCPStream(mtcp_manager_t mtcp, socket_map_t socket, int type, uint32_t saddr, 
		    uint16_t sport, uint32_t daddr, uint16_t dport, unsigned int *hash)
{
        tcp_stream *cur_stream, *paired_stream;
        struct socket_map *walk;
	
        cur_stream = CreateTCPStream(mtcp, socket, type,
				     saddr, sport, daddr, dport, hash);
        if (cur_stream == NULL) {
                TRACE_ERROR("Can't create tcp_stream!\n");
                return NULL;
        }
	
		paired_stream = CreateTCPStream(mtcp, NULL, MOS_SOCK_UNUSED,
						daddr, dport, saddr, sport, hash);
        if (paired_stream == NULL) {
                DestroyTCPStream(mtcp, cur_stream);
                TRACE_ERROR("Can't create tcp_stream!\n");
                return NULL;
        }
	
        cur_stream->pair_stream = paired_stream;
        paired_stream->pair_stream = cur_stream;
        paired_stream->socket = socket;
        SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
                SOCKQ_INSERT_TAIL(&paired_stream->msocks, walk);
        } SOCKQ_FOREACH_END;
        paired_stream->stream_type = STREAM_TYPE(MOS_SOCK_MONITOR_STREAM_ACTIVE);
	
        return cur_stream;
}
/*----------------------------------------------------------------------------*/
inline tcp_stream *
CreateClientTCPStream(mtcp_manager_t mtcp, socket_map_t socket, int type,
			uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport,
			unsigned int *hash)
{
	tcp_stream *cs;
	struct socket_map *w;

	cs = CreateTCPStream(mtcp, socket, type, daddr, dport, saddr, sport, hash);
	if (cs == NULL) {
		TRACE_ERROR("Can't create tcp_stream!\n");
		return NULL;
	}

	cs->side = MOS_SIDE_CLI;
	cs->pair_stream = NULL;

	/* if buffer management is off, then disable 
	 * monitoring tcp ring of either streams (only if stream
	 * is just monitor stream active)
	 */
	if (IS_STREAM_TYPE(cs, MOS_SOCK_MONITOR_STREAM_ACTIVE)) {
		cs->buffer_mgmt = BUFMGMT_OFF;
		SOCKQ_FOREACH_START(w, &cs->msocks) {
			uint8_t bm = w->monitor_stream->client_buf_mgmt;
			if (bm > cs->buffer_mgmt)
				cs->buffer_mgmt = bm;
			if (w->monitor_stream->monitor_listener->client_mon == 1)
				cs->status_mgmt = 1;
		} SOCKQ_FOREACH_END;
	}
	
	return cs;
}
/*----------------------------------------------------------------------------*/
inline tcp_stream *
AttachServerTCPStream(mtcp_manager_t mtcp, tcp_stream *cs, int type,
			uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport)
{
	tcp_stream *ss;
	struct socket_map *w;

	/* The 3rd arg is a temp hackk... FIXIT! TODO: XXX */
	ss = CreateTCPStream(mtcp, NULL, MOS_SOCK_UNUSED, saddr, sport, daddr, dport, NULL);
	if (ss == NULL) {
		TRACE_ERROR("Can't create tcp_stream!\n");
		return NULL;
	}

	ss->side = MOS_SIDE_SVR;
	cs->pair_stream = ss;
	ss->pair_stream = cs;
	ss->socket = cs->socket;
	SOCKQ_FOREACH_START(w, &cs->msocks) {
		SOCKQ_INSERT_TAIL(&ss->msocks, w);
	} SOCKQ_FOREACH_END;
	ss->stream_type = STREAM_TYPE(MOS_SOCK_MONITOR_STREAM_ACTIVE);

	if (IS_STREAM_TYPE(ss, MOS_SOCK_MONITOR_STREAM_ACTIVE)) {
		ss->buffer_mgmt = BUFMGMT_OFF;
		SOCKQ_FOREACH_START(w, &ss->msocks) {
			uint8_t bm = w->monitor_stream->server_buf_mgmt;
			if (bm > ss->buffer_mgmt)
				ss->buffer_mgmt = bm;
			if (w->monitor_stream->monitor_listener->server_mon == 1)
				ss->status_mgmt = 1;
		} SOCKQ_FOREACH_END;
	}

	return ss;
}
/*---------------------------------------------------------------------------*/
static void
DestroySingleTCPStream(mtcp_manager_t mtcp, tcp_stream *stream)
{
	struct sockaddr_in addr;
	int bound_addr = FALSE;
	int ret;
	/* stand-alone monitor does not need this since it is single-threaded */
	bool flow_lock = HAS_STREAM_TYPE(stream, MOS_SOCK_STREAM);

	struct socket_map *walk;

	/* Set the stream state as CLOSED */
	stream->state = TCP_ST_CLOSED_RSVD;

	SOCKQ_FOREACH_START(walk, &stream->msocks) {
		HandleCallback(mtcp, MOS_HK_RCV, walk, stream->side, NULL,
			   MOS_ON_CONN_END | MOS_ON_TCP_STATE_CHANGE | stream->cb_events);
		HandleCallback(mtcp, MOS_HK_SND, walk, stream->side, NULL,
			   MOS_ON_CONN_END | MOS_ON_TCP_STATE_CHANGE | stream->cb_events);
	} SOCKQ_FOREACH_END;

#if 0
#ifdef DUMP_STREAM
	if (stream->close_reason != TCP_ACTIVE_CLOSE && 
			stream->close_reason != TCP_PASSIVE_CLOSE) {
		thread_printf(mtcp, mtcp->log_fp, 
				"Stream %d abnormally closed.\n", stream->id);
		DumpStream(mtcp, stream);
		DumpControlList(mtcp, mtcp->n_sender[0]);
	}
#endif

#ifdef STREAM
	uint8_t *sa, *da;
	sa = (uint8_t *)&stream->saddr;
	da = (uint8_t *)&stream->daddr;
	TRACE_STREAM("DESTROY TCP STREAM %d: "
			"%u.%u.%u.%u(%d) -> %u.%u.%u.%u(%d) (%s)\n", stream->id, 
			sa[0], sa[1], sa[2], sa[3], ntohs(stream->sport), 
			da[0], da[1], da[2], da[3], ntohs(stream->dport), 
			close_reason_str[stream->close_reason]);
#endif

	if (stream->sndvar->sndbuf) {
		TRACE_FSTAT("Stream %d: send buffer "
				"cum_len: %lu, len: %u\n", stream->id, 
				stream->sndvar->sndbuf->cum_len, 
				stream->sndvar->sndbuf->len);
	}
	if (stream->rcvvar->rcvbuf) {
		TRACE_FSTAT("Stream %d: recv buffer "
				"cum_len: %lu, merged_len: %u, last_len: %u\n", stream->id, 
				stream->rcvvar->rcvbuf->cum_len, 
				stream->rcvvar->rcvbuf->merged_len, 
				stream->rcvvar->rcvbuf->last_len);
	}

#if RTM_STAT
	/* Triple duplicated ack stats */
	if (stream->sndvar->rstat.tdp_ack_cnt) {
		TRACE_FSTAT("Stream %d: triple duplicated ack: %u, "
				"retransmission bytes: %u, average rtm bytes/ack: %u\n", 
				stream->id, 
				stream->sndvar->rstat.tdp_ack_cnt, stream->sndvar->rstat.tdp_ack_bytes, 
				stream->sndvar->rstat.tdp_ack_bytes / stream->sndvar->rstat.tdp_ack_cnt);
	}

	/* Retransmission timeout stats */
	if (stream->sndvar->rstat.rto_cnt > 0) {
		TRACE_FSTAT("Stream %d: timeout count: %u, bytes: %u\n", stream->id, 
				stream->sndvar->rstat.rto_cnt, stream->sndvar->rstat.rto_bytes);
	}

	/* Recovery stats */
	if (stream->sndvar->rstat.ack_upd_cnt) {
		TRACE_FSTAT("Stream %d: snd_nxt update count: %u, "
				"snd_nxt update bytes: %u, average update bytes/update: %u\n", 
				stream->id, 
				stream->sndvar->rstat.ack_upd_cnt, stream->sndvar->rstat.ack_upd_bytes, 
				stream->sndvar->rstat.ack_upd_bytes / stream->sndvar->rstat.ack_upd_cnt);
	}
#if TCP_OPT_SACK_ENABLED
	if (stream->sndvar->rstat.sack_cnt) {
		TRACE_FSTAT("Selective ack count: %u, bytes: %u, "
				"average bytes/ack: %u\n", 
				stream->sndvar->rstat.sack_cnt, stream->sndvar->rstat.sack_bytes, 
				stream->sndvar->rstat.sack_bytes / stream->sndvar->rstat.sack_cnt);
	} else {
		TRACE_FSTAT("Selective ack count: %u, bytes: %u\n", 
				stream->sndvar->rstat.sack_cnt, stream->sndvar->rstat.sack_bytes);
	}
	if (stream->sndvar->rstat.tdp_sack_cnt) {
		TRACE_FSTAT("Selective tdp ack count: %u, bytes: %u, "
				"average bytes/ack: %u\n", 
				stream->sndvar->rstat.tdp_sack_cnt, stream->sndvar->rstat.tdp_sack_bytes, 
				stream->sndvar->rstat.tdp_sack_bytes / stream->sndvar->rstat.tdp_sack_cnt);
	} else {
		TRACE_FSTAT("Selective ack count: %u, bytes: %u\n", 
				stream->sndvar->rstat.tdp_sack_cnt, stream->sndvar->rstat.tdp_sack_bytes);
	}
#endif /* TCP_OPT_SACK_ENABLED */
#endif /* RTM_STAT */
#endif

	if (HAS_STREAM_TYPE(stream, MOS_SOCK_STREAM)) {
		/* stand-alone monitor does not need to do these */
		if (stream->is_bound_addr) {
			bound_addr = TRUE;
			addr.sin_addr.s_addr = stream->saddr;
			addr.sin_port = stream->sport;
		}

		RemoveFromControlList(mtcp, stream);
		RemoveFromSendList(mtcp, stream);
		RemoveFromACKList(mtcp, stream);
		
		if (stream->on_rto_idx >= 0)
			RemoveFromRTOList(mtcp, stream);
		
		SBUF_LOCK_DESTROY(&stream->rcvvar->read_lock);
		SBUF_LOCK_DESTROY(&stream->sndvar->write_lock);

		assert(stream->on_hash_table == TRUE);
		
		/* free ring buffers */
		if (stream->sndvar->sndbuf) {
			SBFree(mtcp->rbm_snd, stream->sndvar->sndbuf);
			stream->sndvar->sndbuf = NULL;
		}
	}

	if (stream->on_timewait_list)
		RemoveFromTimewaitList(mtcp, stream);

	if (g_config.mos->tcp_timeout > 0)
		RemoveFromTimeoutList(mtcp, stream);

	if (stream->rcvvar->rcvbuf) {
		tcprb_del(stream->rcvvar->rcvbuf);
		stream->rcvvar->rcvbuf = NULL;
	}

	if (flow_lock)
		pthread_mutex_lock(&mtcp->ctx->flow_pool_lock);

	/* remove from flow hash table */
	HTRemove(mtcp->tcp_flow_table, stream);
	stream->on_hash_table = FALSE;
	
	mtcp->flow_cnt--;

	/* if there was a corresponding monitor stream socket opened
	 * then close it */
	SOCKQ_FOREACH_START(walk, &stream->msocks) {
		SOCKQ_REMOVE(&stream->msocks, walk);
		if (stream->pair_stream == NULL)
			DestroyMonitorStreamSocket(mtcp, walk);
	} SOCKQ_FOREACH_END;

	if (stream->pair_stream != NULL) {
		/* Nullify pointer to sibliing tcp_stream's pair_stream */
		stream->pair_stream->pair_stream = NULL;
	}

	MPFreeChunk(mtcp->rv_pool, stream->rcvvar);
	MPFreeChunk(mtcp->sv_pool, stream->sndvar);
	MPFreeChunk(mtcp->flow_pool, stream);

	if (flow_lock)
		/* stand-alone monitor does not need this since it is single-threaded */
		pthread_mutex_unlock(&mtcp->ctx->flow_pool_lock);

	if (bound_addr) {
		if (mtcp->ap) {
			ret = FreeAddress(mtcp->ap, &addr);
		} else {
			int nif;
			nif = GetOutputInterface(addr.sin_addr.s_addr);
			ret = FreeAddress(ap[nif], &addr);
		}
		if (ret < 0) {
			TRACE_ERROR("(NEVER HAPPEN) Failed to free address.\n");
		}
	}

#ifdef NETSTAT
#if NETSTAT_PERTHREAD
	TRACE_STREAM("Destroyed. Remaining flows: %u\n", mtcp->flow_cnt);
#endif /* NETSTAT_PERTHREAD */
#endif /* NETSTAT */

}
/*---------------------------------------------------------------------------*/
void 
DestroyTCPStream(mtcp_manager_t mtcp, tcp_stream *stream)
{
	tcp_stream *pair_stream = stream->pair_stream;

	DestroySingleTCPStream(mtcp, stream);
	
	if (pair_stream)
		DestroySingleTCPStream(mtcp, pair_stream);
}
/*---------------------------------------------------------------------------*/
void 
DumpStream(mtcp_manager_t mtcp, tcp_stream *stream)
{
	uint8_t *sa, *da;
	struct tcp_send_vars *sndvar = stream->sndvar;
	struct tcp_recv_vars *rcvvar = stream->rcvvar;

	sa = (uint8_t *)&stream->saddr;
	da = (uint8_t *)&stream->daddr;
	thread_printf(mtcp, mtcp->log_fp, "========== Stream %u: "
			"%u.%u.%u.%u(%u) -> %u.%u.%u.%u(%u) ==========\n", stream->id, 
			sa[0], sa[1], sa[2], sa[3], ntohs(stream->sport), 
			da[0], da[1], da[2], da[3], ntohs(stream->dport));
	thread_printf(mtcp, mtcp->log_fp, 
			"Stream id: %u, type: %u, state: %s, close_reason: %s\n",  
			stream->id, stream->stream_type, 
			TCPStateToString(stream), close_reason_str[stream->close_reason]);
	if (stream->socket) {
		socket_map_t socket = stream->socket;
		thread_printf(mtcp, mtcp->log_fp, "Socket id: %d, type: %d, opts: %u\n"
				"epoll: %u (IN: %u, OUT: %u, ERR: %u, RDHUP: %u, ET: %u)\n"
				"events: %u (IN: %u, OUT: %u, ERR: %u, RDHUP: %u, ET: %u)\n", 
				socket->id, socket->socktype, socket->opts, 
				socket->epoll, socket->epoll & MOS_EPOLLIN, 
				socket->epoll & MOS_EPOLLOUT, socket->epoll & MOS_EPOLLERR, 
				socket->epoll & MOS_EPOLLRDHUP, socket->epoll & MOS_EPOLLET, 
				socket->events, socket->events & MOS_EPOLLIN, 
				socket->events & MOS_EPOLLOUT, socket->events & MOS_EPOLLERR, 
				socket->events & MOS_EPOLLRDHUP, socket->events & MOS_EPOLLET);
	} else {
		thread_printf(mtcp, mtcp->log_fp, "Socket: (null)\n");
	}

	thread_printf(mtcp, mtcp->log_fp, 
			"on_hash_table: %u, on_control_list: %u (wait: %u), on_send_list: %u, "
			"on_ack_list: %u, is_wack: %u, ack_cnt: %u\n"
			"on_rto_idx: %d, on_timewait_list: %u, on_timeout_list: %u, "
			"on_rcv_br_list: %u, on_snd_br_list: %u\n"
			"on_sendq: %u, on_ackq: %u, closed: %u, on_closeq: %u, "
			"on_closeq_int: %u, on_resetq: %u, on_resetq_int: %u\n"
			"have_reset: %u, is_fin_sent: %u, is_fin_ackd: %u, "
			"saw_timestamp: %u, sack_permit: %u, "
			"is_bound_addr: %u, need_wnd_adv: %u\n", stream->on_hash_table, 
			sndvar->on_control_list, stream->control_list_waiting, sndvar->on_send_list, 
			sndvar->on_ack_list, sndvar->is_wack, sndvar->ack_cnt, 
			stream->on_rto_idx, stream->on_timewait_list, stream->on_timeout_list, 
			stream->on_rcv_br_list, stream->on_snd_br_list, 
			sndvar->on_sendq, sndvar->on_ackq, 
			stream->closed, sndvar->on_closeq, sndvar->on_closeq_int, 
			sndvar->on_resetq, sndvar->on_resetq_int, 
			stream->have_reset, sndvar->is_fin_sent, 
			sndvar->is_fin_ackd, stream->saw_timestamp, stream->sack_permit, 
			stream->is_bound_addr, stream->need_wnd_adv);

	thread_printf(mtcp, mtcp->log_fp, "========== Send variables ==========\n");
	thread_printf(mtcp, mtcp->log_fp, 
		      "ip_id: %u, mss: %u, eff_mss: %u, wscale(me, peer): (%u, %u), nif_out: %d\n", 
		      sndvar->ip_id, sndvar->mss, sndvar->eff_mss, 
		      sndvar->wscale_mine, sndvar->wscale_peer, sndvar->nif_out);
	thread_printf(mtcp, mtcp->log_fp, 
			"snd_nxt: %u, snd_una: %u, iss: %u, fss: %u\nsnd_wnd: %u, "
			"peer_wnd: %u, cwnd: %u, ssthresh: %u\n", 
			stream->snd_nxt, sndvar->snd_una, sndvar->iss, sndvar->fss, 
			sndvar->snd_wnd, sndvar->peer_wnd, sndvar->cwnd, sndvar->ssthresh);

	if (sndvar->sndbuf) {
		thread_printf(mtcp, mtcp->log_fp, 
				"Send buffer: init_seq: %u, head_seq: %u, "
				"len: %d, cum_len: %lu, size: %d\n", 
				sndvar->sndbuf->init_seq, sndvar->sndbuf->head_seq, 
				sndvar->sndbuf->len, sndvar->sndbuf->cum_len, sndvar->sndbuf->size);
	} else {
		thread_printf(mtcp, mtcp->log_fp, "Send buffer: (null)\n");
	}
	thread_printf(mtcp, mtcp->log_fp, 
			"nrtx: %u, max_nrtx: %u, rto: %u, ts_rto: %u, "
			"ts_lastack_sent: %u\n", sndvar->nrtx, sndvar->max_nrtx, 
			sndvar->rto, sndvar->ts_rto, sndvar->ts_lastack_sent);

	thread_printf(mtcp, mtcp->log_fp, 
			"========== Receive variables ==========\n");
	thread_printf(mtcp, mtcp->log_fp, 
			"rcv_nxt: %u, irs: %u, rcv_wnd: %u, "
			"snd_wl1: %u, snd_wl2: %u\n", 
			stream->rcv_nxt, rcvvar->irs, 
			rcvvar->rcv_wnd, rcvvar->snd_wl1, rcvvar->snd_wl2);
	if (!rcvvar->rcvbuf) {
		thread_printf(mtcp, mtcp->log_fp, "Receive buffer: (null)\n");
	}
	thread_printf(mtcp, mtcp->log_fp, "last_ack_seq: %u, dup_acks: %u\n", 
			rcvvar->last_ack_seq, rcvvar->dup_acks);
	thread_printf(mtcp, mtcp->log_fp, 
			"ts_recent: %u, ts_lastack_rcvd: %u, ts_last_ts_upd: %u, "
			"ts_tw_expire: %u\n", rcvvar->ts_recent, rcvvar->ts_lastack_rcvd, 
			rcvvar->ts_last_ts_upd, rcvvar->ts_tw_expire);
	thread_printf(mtcp, mtcp->log_fp, 
			"srtt: %u, mdev: %u, mdev_max: %u, rttvar: %u, rtt_seq: %u\n", 
			rcvvar->srtt, rcvvar->mdev, rcvvar->mdev_max, 
			rcvvar->rttvar, rcvvar->rtt_seq);
}
/*---------------------------------------------------------------------------*/
