#include <sys/queue.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#ifdef DARWIN
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include "mtcp.h"
#include "mtcp_api.h"
#include "tcp_in.h"
#include "tcp_stream.h"
#include "tcp_out.h"
#include "ip_out.h"
#include "eventpoll.h"
#include "pipe.h"
#include "fhash.h"
#include "addr_pool.h"
#include "rss.h"
#include "config.h"
#include "debug.h"
#include "eventpoll.h"
#include "mos_api.h"
#include "tcp_rb.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*----------------------------------------------------------------------------*/
/** Stop monitoring the socket! (function prototype)
 * @param [in] mctx: mtcp context
 * @param [in] sock: monitoring stream socket id
 * @param [in] side: side of monitoring (client side, server side or both)
 *
 * This function is now DEPRECATED and is only used within mOS core...
 */
int
mtcp_cb_stop(mctx_t mctx, int sock, int side);
/*----------------------------------------------------------------------------*/
/** Reset the connection (send RST packets to both sides)
 *  (We need to decide the API for this.)
 */
//int
//mtcp_cb_reset(mctx_t mctx, int sock, int side);
/*----------------------------------------------------------------------------*/
inline mtcp_manager_t 
GetMTCPManager(mctx_t mctx)
{
	if (!mctx) {
		errno = EACCES;
		return NULL;
	}

	if (mctx->cpu < 0 || mctx->cpu >= num_cpus) {
		errno = EINVAL;
		return NULL;
	}

	if (!g_mtcp[mctx->cpu] || g_mtcp[mctx->cpu]->ctx->done || g_mtcp[mctx->cpu]->ctx->exit) {
		errno = EPERM;
		return NULL;
	}

	return g_mtcp[mctx->cpu];
}
/*----------------------------------------------------------------------------*/
static inline int 
GetSocketError(socket_map_t socket, void *optval, socklen_t *optlen)
{
	tcp_stream *cur_stream;

	if (!socket->stream) {
		errno = EBADF;
		return -1;
	}

	cur_stream = socket->stream;
	if (cur_stream->state == TCP_ST_CLOSED_RSVD) {
		if (cur_stream->close_reason == TCP_TIMEDOUT || 
				cur_stream->close_reason == TCP_CONN_FAIL || 
				cur_stream->close_reason == TCP_CONN_LOST) {
			*(int *)optval = ETIMEDOUT;
			*optlen = sizeof(int);

			return 0;
		}
	}

	if (cur_stream->state == TCP_ST_CLOSE_WAIT || 
			cur_stream->state == TCP_ST_CLOSED_RSVD) { 
		if (cur_stream->close_reason == TCP_RESET) {
			*(int *)optval = ECONNRESET;
			*optlen = sizeof(int);

			return 0;
		}
	}

	if (cur_stream->state == TCP_ST_SYN_SENT &&
	    errno == EINPROGRESS) {
		*(int *)optval = errno;
		*optlen = sizeof(int);
		
		return -1;
        }

	/*
	 * `base case`: If socket sees no so_error, then
	 * this also means close_reason will always be
	 * TCP_NOT_CLOSED. 
	 */
	if (cur_stream->close_reason == TCP_NOT_CLOSED) {
		*(int *)optval = 0;
		*optlen = sizeof(int);
		
		return 0;
	}
	
	errno = ENOSYS;
	return -1;
}
/*----------------------------------------------------------------------------*/
int
mtcp_getsockname(mctx_t mctx, int sockid, struct sockaddr *addr,
		 socklen_t *addrlen)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	socket = &mtcp->smap[sockid];
	if (socket->socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (*addrlen <= 0) {
		TRACE_API("Invalid addrlen: %d\n", *addrlen);
		errno = EINVAL;
		return -1;
	}
	
	if (socket->socktype != MOS_SOCK_STREAM_LISTEN && 
	    socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	*(struct sockaddr_in *)addr = socket->saddr;
        *addrlen = sizeof(socket->saddr);

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_getsockopt(mctx_t mctx, int sockid, int level, 
		int optname, void *optval, socklen_t *optlen)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	switch (level) {
	case SOL_SOCKET:
		socket = &mtcp->smap[sockid];
		if (socket->socktype == MOS_SOCK_UNUSED) {
			TRACE_API("Invalid socket id: %d\n", sockid);
			errno = EBADF;
			return -1;
		}
		
		if (socket->socktype != MOS_SOCK_STREAM_LISTEN && 
		    socket->socktype != MOS_SOCK_STREAM) {
			TRACE_API("Invalid socket id: %d\n", sockid);
			errno = ENOTSOCK;
			return -1;
		}
		
		if (optname == SO_ERROR) {
			if (socket->socktype == MOS_SOCK_STREAM) {
				return GetSocketError(socket, optval, optlen);
			}
		}
		break;
	case SOL_MONSOCKET:
		/* check if the calling thread is in MOS context */
		if (mtcp->ctx->thread != pthread_self()) {
			errno = EPERM;
			return -1;
		}
		/*
		 * All options will only work for active 
		 * monitor stream sockets
		 */
		socket = &mtcp->msmap[sockid];
		if (socket->socktype != MOS_SOCK_MONITOR_STREAM_ACTIVE) {
			TRACE_API("Invalid socket id: %d\n", sockid);
			errno = ENOTSOCK;
			return -1;
		}

		switch (optname) {
		case MOS_FRAGINFO_CLIBUF:
			return GetFragInfo(socket, MOS_SIDE_CLI, optval, optlen);
		case MOS_FRAGINFO_SVRBUF:
			return GetFragInfo(socket, MOS_SIDE_SVR, optval, optlen);
		case MOS_INFO_CLIBUF:
			return GetBufInfo(socket, MOS_SIDE_CLI, optval, optlen);
		case MOS_INFO_SVRBUF:
			return GetBufInfo(socket, MOS_SIDE_SVR, optval, optlen);
		case MOS_TCP_STATE_CLI:
			return GetTCPState(socket->monitor_stream->stream, MOS_SIDE_CLI,
							   optval, optlen);
		case MOS_TCP_STATE_SVR:
			return GetTCPState(socket->monitor_stream->stream, MOS_SIDE_SVR,
							   optval, optlen);
		case MOS_TIMESTAMP:
			return GetLastTimestamp(socket->monitor_stream->stream,
						(uint32_t *)optval, 
						optlen);
		default: 
		  TRACE_API("can't recognize the optname=%d\n", optname);
		  assert(0);
		}
		break;
	}
	errno = ENOSYS;
	return -1;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_setsockopt(mctx_t mctx, int sockid, int level, 
		int optname, const void *optval, socklen_t optlen)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcprb_t *rb;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	switch (level) {
	case SOL_SOCKET:
		socket = &mtcp->smap[sockid];
		if (socket->socktype == MOS_SOCK_UNUSED) {
			TRACE_API("Invalid socket id: %d\n", sockid);
			errno = EBADF;
			return -1;
		}

		if (socket->socktype != MOS_SOCK_STREAM_LISTEN && 
		    socket->socktype != MOS_SOCK_STREAM) {
			TRACE_API("Invalid socket id: %d\n", sockid);
			errno = ENOTSOCK;
			return -1;
		}
		break;
	case SOL_MONSOCKET:
		socket = &mtcp->msmap[sockid];
		/* 
		 * checking of calling thread to be in MOS context is
		 * disabled since both optnames can be called from
		 * `application' context (on passive sockets)
		 */
		/* 
		 * if (mtcp->ctx->thread != pthread_self())
		 * return -1;
		 */
		
		switch (optname) {
		case MOS_CLIBUF:
#if 0
			if (socket->socktype != MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				errno = EBADF;
				return -1;
			}
#endif
#ifdef DISABLE_DYN_RESIZE
			if (*(int *)optval != 0)
				return -1;
			if (socket->socktype == MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				rb = (socket->monitor_stream->stream->side == MOS_SIDE_CLI) ?
					socket->monitor_stream->stream->rcvvar->rcvbuf :
					socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
				if (rb) {
					tcprb_resize_meta(rb, 0);
					tcprb_resize(rb, 0);
				}
			}
			return DisableBuf(socket, MOS_SIDE_CLI);
#else
			rb = (socket->monitor_stream->stream->side == MOS_SIDE_CLI) ?
				socket->monitor_stream->stream->rcvvar->rcvbuf :
				socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
			if (tcprb_resize_meta(rb, *(int *)optval) < 0)
				return -1;
			return tcprb_resize(rb,
					(((int)rb->metalen - 1) / UNITBUFSIZE + 1) * UNITBUFSIZE);
#endif
		case MOS_SVRBUF:
#if 0
			if (socket->socktype != MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				errno = EBADF;
				return -1;
			}
#endif
#ifdef DISABLE_DYN_RESIZE
			if (*(int *)optval != 0)
				return -1;
			if (socket->socktype == MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				rb = (socket->monitor_stream->stream->side == MOS_SIDE_SVR) ?
					socket->monitor_stream->stream->rcvvar->rcvbuf :
					socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
				if (rb) {
					tcprb_resize_meta(rb, 0);
					tcprb_resize(rb, 0);
				}
			}
			return DisableBuf(socket, MOS_SIDE_SVR);
#else
			rb = (socket->monitor_stream->stream->side == MOS_SIDE_SVR) ?
				socket->monitor_stream->stream->rcvvar->rcvbuf :
				socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
			if (tcprb_resize_meta(rb, *(int *)optval) < 0)
				return -1;
			return tcprb_resize(rb,
					(((int)rb->metalen - 1) / UNITBUFSIZE + 1) * UNITBUFSIZE);
#endif
		case MOS_FRAG_CLIBUF:
#if 0
			if (socket->socktype != MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				errno = EBADF;
				return -1;
			}
#endif
#ifdef DISABLE_DYN_RESIZE
			if (*(int *)optval != 0)
				return -1;
			if (socket->socktype == MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				rb = (socket->monitor_stream->stream->side == MOS_SIDE_CLI) ?
					socket->monitor_stream->stream->rcvvar->rcvbuf :
					socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
				if (rb)
					tcprb_resize(rb, 0);
			}
			return 0;
#else
			rb = (socket->monitor_stream->stream->side == MOS_SIDE_CLI) ?
				socket->monitor_stream->stream->rcvvar->rcvbuf :
				socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
			if (rb->len == 0)
				return tcprb_resize_meta(rb, *(int *)optval);
			else
				return -1;
#endif
		case MOS_FRAG_SVRBUF:
#if 0
			if (socket->socktype != MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				errno = EBADF;
				return -1;
			}
#endif
#ifdef DISABLE_DYN_RESIZE
			if (*(int *)optval != 0)
				return -1;
			if (socket->socktype == MOS_SOCK_MONITOR_STREAM_ACTIVE) {
				rb = (socket->monitor_stream->stream->side == MOS_SIDE_SVR) ?
					socket->monitor_stream->stream->rcvvar->rcvbuf :
					socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
				if (rb)
					tcprb_resize(rb, 0);
			}
			return 0;
#else
			rb = (socket->monitor_stream->stream->side == MOS_SIDE_SVR) ?
				socket->monitor_stream->stream->rcvvar->rcvbuf :
				socket->monitor_stream->stream->pair_stream->rcvvar->rcvbuf;
			if (rb->len == 0)
				return tcprb_resize_meta(rb, *(int *)optval);
			else
				return -1;
#endif
		case MOS_MONLEVEL:
#ifdef OLD_API
			assert(*(int *)optval == MOS_NO_CLIBUF || 
			       *(int *)optval == MOS_NO_SVRBUF);
			return DisableBuf(socket, 
					  (*(int *)optval == MOS_NO_CLIBUF) ? 
					  MOS_SIDE_CLI : MOS_SIDE_SVR);
#endif
		case MOS_SEQ_REMAP:
			return TcpSeqChange(socket, 
					    (uint32_t)((seq_remap_info *)optval)->seq_off, 
					    ((seq_remap_info *)optval)->side,
					    mtcp->pctx->p.seq);
		case MOS_STOP_MON:
			return mtcp_cb_stop(mctx, sockid, *(int *)optval);
		default: 
			TRACE_API("invalid optname=%d\n", optname);
			assert(0);
		}
		break;
	}

	errno = ENOSYS;
	return -1;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_setsock_nonblock(mctx_t mctx, int sockid)
{
	mtcp_manager_t mtcp;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	mtcp->smap[sockid].opts |= MTCP_NONBLOCK;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_ioctl(mctx_t mctx, int sockid, int request, void *argp)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	/* only support stream socket */
	socket = &mtcp->smap[sockid];
	
	if (socket->socktype != MOS_SOCK_STREAM_LISTEN && 
		socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (!argp) {
		errno = EFAULT;
		return -1;
	}

	if (request == FIONREAD) {
		tcp_stream *cur_stream;
		tcprb_t *rbuf;

		cur_stream = socket->stream;
		if (!cur_stream) {
			errno = EBADF;
			return -1;
		}
		
		rbuf = cur_stream->rcvvar->rcvbuf;
		*(int *)argp = (rbuf) ? tcprb_cflen(rbuf) : 0;

	} else if (request == FIONBIO) {
		/* 
		 * sockets can only be set to blocking/non-blocking 
		 * modes during initialization
		 */
		if ((*(int *)argp))
			mtcp->smap[sockid].opts |= MTCP_NONBLOCK;
		else
			mtcp->smap[sockid].opts &= ~MTCP_NONBLOCK;
	} else {
		errno = EINVAL;
		return -1;
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int 
mtcp_monitor(mctx_t mctx, socket_map_t sock) 
{
	mtcp_manager_t mtcp;
	struct mon_listener *monitor;
	int sockid = sock->id;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->msmap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (!(mtcp->msmap[sockid].socktype == MOS_SOCK_MONITOR_STREAM ||
	      mtcp->msmap[sockid].socktype == MOS_SOCK_MONITOR_RAW)) {
		TRACE_API("Not a monitor socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	monitor = (struct mon_listener *)calloc(1, sizeof(struct mon_listener));
	if (!monitor) {
		/* errno set from the malloc() */
		errno = ENOMEM;
		return -1;
	}

	/* create a monitor-specific event queue */
	monitor->eq = CreateEventQueue(g_config.mos->max_concurrency);
	if (!monitor->eq) {
		TRACE_API("Can't create event queue (concurrency: %d) for "
			  "monitor read event registrations!\n",
			  g_config.mos->max_concurrency);
		free(monitor);
		errno = ENOMEM;
		return -1;
	}

	/* set monitor-related basic parameters */
#ifndef NEWEV
	monitor->ude_id = UDE_OFFSET;
#endif
	monitor->socket = sock;
	monitor->client_buf_mgmt = monitor->server_buf_mgmt = BUFMGMT_FULL;

	/* perform both sides monitoring by default */
	monitor->client_mon = monitor->server_mon = 1;

	/* add monitor socket to the monitor list */
	TAILQ_INSERT_TAIL(&mtcp->monitors, monitor, link);

	mtcp->msmap[sockid].monitor_listener = monitor;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_socket(mctx_t mctx, int domain, int type, int protocol)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (domain != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (type == SOCK_STREAM) {
		type = MOS_SOCK_STREAM;
	} else if (type == MOS_SOCK_MONITOR_STREAM ||
		   type == MOS_SOCK_MONITOR_RAW) {
		/* do nothing for the time being */
	} else {
		/* Not supported type */
		errno = EINVAL;
		return -1;
	}

	socket = AllocateSocket(mctx, type);
	if (!socket) {
		errno = ENFILE;
		return -1;
	}

	if (type == MOS_SOCK_MONITOR_STREAM || 
	    type == MOS_SOCK_MONITOR_RAW) {
		mtcp_manager_t mtcp = GetMTCPManager(mctx);
		if (!mtcp) {
			errno = EACCES;
			return -1;
		}
		mtcp_monitor(mctx, socket);
#ifdef NEWEV
		socket->monitor_listener->stree_dontcare = NULL;
		socket->monitor_listener->stree_pre_rcv = NULL;
		socket->monitor_listener->stree_post_snd = NULL;
#else
		InitEvB(mtcp, &socket->monitor_listener->dontcare_evb);
		InitEvB(mtcp, &socket->monitor_listener->pre_tcp_evb);
		InitEvB(mtcp, &socket->monitor_listener->post_tcp_evb);
#endif
	}

	return socket->id;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_bind(mctx_t mctx, int sockid, 
		const struct sockaddr *addr, socklen_t addrlen)
{
	mtcp_manager_t mtcp;
	struct sockaddr_in *addr_in;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (mtcp->smap[sockid].socktype != MOS_SOCK_STREAM && 
			mtcp->smap[sockid].socktype != MOS_SOCK_STREAM_LISTEN) {
		TRACE_API("Not a stream socket id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (!addr) {
		TRACE_API("Socket %d: empty address!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	if (mtcp->smap[sockid].opts & MTCP_ADDR_BIND) {
		TRACE_API("Socket %d: adress already bind for this socket.\n", sockid);
		errno = EINVAL;
		return -1;
	}

	/* we only allow bind() for AF_INET address */
	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		TRACE_API("Socket %d: invalid argument!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	if (mtcp->listener) {
		TRACE_API("Address already bound!\n");
		errno = EINVAL;
		return -1;
	}
	addr_in = (struct sockaddr_in *)addr;
	mtcp->smap[sockid].saddr = *addr_in;
	mtcp->smap[sockid].opts |= MTCP_ADDR_BIND;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_listen(mctx_t mctx, int sockid, int backlog)
{
	mtcp_manager_t mtcp;
	struct tcp_listener *listener;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_STREAM) {
		mtcp->smap[sockid].socktype = MOS_SOCK_STREAM_LISTEN;
	}
	
	if (mtcp->smap[sockid].socktype != MOS_SOCK_STREAM_LISTEN) {
		TRACE_API("Not a listening socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (backlog <= 0 || backlog > g_config.mos->max_concurrency) {
		errno = EINVAL;
		return -1;
	}

	listener = (struct tcp_listener *)calloc(1, sizeof(struct tcp_listener));
	if (!listener) {
		/* errno set from the malloc() */
		errno = ENOMEM;
		return -1;
	}

	listener->sockid = sockid;
	listener->backlog = backlog;
	listener->socket = &mtcp->smap[sockid];

	if (pthread_cond_init(&listener->accept_cond, NULL)) {
		perror("pthread_cond_init of ctx->accept_cond\n");
		/* errno set by pthread_cond_init() */
		return -1;
	}
	if (pthread_mutex_init(&listener->accept_lock, NULL)) {
		perror("pthread_mutex_init of ctx->accept_lock\n");
		/* errno set by pthread_mutex_init() */
		return -1;
	}

	listener->acceptq = CreateStreamQueue(backlog);
	if (!listener->acceptq) {
		errno = ENOMEM;
		return -1;
	}
	
	mtcp->smap[sockid].listener = listener;
	mtcp->listener = listener;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_accept(mctx_t mctx, int sockid, struct sockaddr *addr, socklen_t *addrlen)
{
	mtcp_manager_t mtcp;
	struct tcp_listener *listener;
	socket_map_t socket;
	tcp_stream *accepted = NULL;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	/* requires listening socket */
	if (mtcp->smap[sockid].socktype != MOS_SOCK_STREAM_LISTEN) {
		errno = EINVAL;
		return -1;
	}

	listener = mtcp->smap[sockid].listener;

	/* dequeue from the acceptq without lock first */
	/* if nothing there, acquire lock and cond_wait */
	accepted = StreamDequeue(listener->acceptq);
	if (!accepted) {
		if (listener->socket->opts & MTCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;

		} else {
			pthread_mutex_lock(&listener->accept_lock);
			while ((accepted = StreamDequeue(listener->acceptq)) == NULL) {
				pthread_cond_wait(&listener->accept_cond, &listener->accept_lock);
		
				if (mtcp->ctx->done || mtcp->ctx->exit) {
					pthread_mutex_unlock(&listener->accept_lock);
					errno = EINTR;
					return -1;
				}
			}
			pthread_mutex_unlock(&listener->accept_lock);
		}
	}

	if (!accepted) {
		TRACE_ERROR("[NEVER HAPPEN] Empty accept queue!\n");
	}

	if (!accepted->socket) {
		socket = AllocateSocket(mctx, MOS_SOCK_STREAM);
		if (!socket) {
			TRACE_ERROR("Failed to create new socket!\n");
			/* TODO: destroy the stream */
			errno = ENFILE;
			return -1;
		}
		socket->stream = accepted;
		accepted->socket = socket;

		/* set socket addr parameters */
		socket->saddr.sin_family = AF_INET;
		socket->saddr.sin_port = accepted->dport;
		socket->saddr.sin_addr.s_addr = accepted->daddr;

		/* if monitor is enabled, complete the socket assignment */
		if (socket->stream->pair_stream != NULL)
			socket->stream->pair_stream->socket = socket;
	}

	if (!(listener->socket->epoll & MOS_EPOLLET) &&
	    !StreamQueueIsEmpty(listener->acceptq))
		AddEpollEvent(mtcp->ep, 
			      USR_SHADOW_EVENT_QUEUE,
			      listener->socket, MOS_EPOLLIN);

	TRACE_API("Stream %d accepted.\n", accepted->id);

	if (addr && addrlen) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = accepted->dport;
		addr_in->sin_addr.s_addr = accepted->daddr;
		*addrlen = sizeof(struct sockaddr_in);
	}

	return accepted->socket->id;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_init_rss(mctx_t mctx, in_addr_t saddr_base, int num_addr, 
		in_addr_t daddr, in_addr_t dport)
{
	mtcp_manager_t mtcp;
	addr_pool_t ap;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (saddr_base == INADDR_ANY) {
		int nif_out;

		/* for the INADDR_ANY, find the output interface for the destination
		   and set the saddr_base as the ip address of the output interface */
		nif_out = GetOutputInterface(daddr);
		saddr_base = g_config.mos->netdev_table->ent[nif_out]->ip_addr;
	}

	ap = CreateAddressPoolPerCore(mctx->cpu, num_cpus, 
			saddr_base, num_addr, daddr, dport);
	if (!ap) {
		errno = ENOMEM;
		return -1;
	}

	mtcp->ap = ap;

	return 0;
}
/*----------------------------------------------------------------------------*/
int
eval_bpf_5tuple(struct sfbpf_program fcode,
				in_addr_t saddr, in_port_t sport,
				in_addr_t daddr, in_port_t dport) {
	uint8_t buf[TOTAL_TCP_HEADER_LEN];
	struct ethhdr *ethh;
	struct iphdr *iph;
	struct tcphdr *tcph;

	ethh = (struct ethhdr *)buf;
	ethh->h_proto = htons(ETH_P_IP);
	iph = (struct iphdr *)(ethh + 1);
	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + TCP_HEADER_LEN);
	iph->id = htons(0);
	iph->protocol = IPPROTO_TCP;
	iph->saddr = saddr;
	iph->daddr = daddr;
	iph->check = 0;
	tcph = (struct tcphdr *)(iph + 1);
	tcph->source = sport;
	tcph->dest = dport;
	
	return EVAL_BPFFILTER(fcode, (uint8_t *)iph - sizeof(struct ethhdr),
						 TOTAL_TCP_HEADER_LEN);
}
/*----------------------------------------------------------------------------*/
int 
mtcp_connect(mctx_t mctx, int sockid, 
		const struct sockaddr *addr, socklen_t addrlen)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcp_stream *cur_stream;
	struct sockaddr_in *addr_in;
	in_addr_t dip;
	in_port_t dport;
	int is_dyn_bound = FALSE;
	int ret;
	int cnt_match = 0;
	struct mon_listener *walk;
	struct sfbpf_program fcode;

	cur_stream = NULL;
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (mtcp->smap[sockid].socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (!addr) {
		TRACE_API("Socket %d: empty address!\n", sockid);
		errno = EFAULT;
		return -1;
	}

	/* we only allow bind() for AF_INET address */
	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		TRACE_API("Socket %d: invalid argument!\n", sockid);
		errno = EAFNOSUPPORT;
		return -1;
	}

	socket = &mtcp->smap[sockid];
	if (socket->stream) {
		TRACE_API("Socket %d: stream already exist!\n", sockid);
		if (socket->stream->state >= TCP_ST_ESTABLISHED) {
			errno = EISCONN;
		} else {
			errno = EALREADY;
		}
		return -1;
	}

	addr_in = (struct sockaddr_in *)addr;
	dip = addr_in->sin_addr.s_addr;
	dport = addr_in->sin_port;

	/* address binding */
	if (socket->opts & MTCP_ADDR_BIND && 
	    socket->saddr.sin_port != INPORT_ANY &&
	    socket->saddr.sin_addr.s_addr != INADDR_ANY) {
		int rss_core;
	
		rss_core = GetRSSCPUCore(socket->saddr.sin_addr.s_addr, dip, 
				socket->saddr.sin_port, dport, num_queues);

		if (rss_core != mctx->cpu) {
			errno = EINVAL;
			return -1;
		}
	} else {
		if (mtcp->ap) {
			ret = FetchAddress(mtcp->ap, 
					mctx->cpu, num_queues, addr_in, &socket->saddr);
		} else {
			ret = FetchAddress(ap[GetOutputInterface(dip)], 
					   mctx->cpu, num_queues, addr_in, &socket->saddr);
		}
		if (ret < 0) {
			errno = EAGAIN;
			return -1;
		}
		socket->opts |= MTCP_ADDR_BIND;
		is_dyn_bound = TRUE;
	}

	cnt_match = 0;
	if (mtcp->num_msp > 0) {		
		TAILQ_FOREACH(walk, &mtcp->monitors, link) {
			fcode = walk->stream_syn_fcode;
			if (!(ISSET_BPFFILTER(fcode) &&
				  eval_bpf_5tuple(fcode, socket->saddr.sin_addr.s_addr,
								  socket->saddr.sin_port,
								  dip, dport) == 0)) {
				walk->is_stream_syn_filter_hit = 1; // set the 'filter hit' flag to 1
				cnt_match++;
			}
		}
	}

	if (mtcp->num_msp > 0 && cnt_match > 0) {
		/* 150820 dhkim: XXX: embedded mode is not verified */
#if 1
		cur_stream = CreateClientTCPStream(mtcp, socket,
						 STREAM_TYPE(MOS_SOCK_STREAM) |
						 STREAM_TYPE(MOS_SOCK_MONITOR_STREAM_ACTIVE),
						 socket->saddr.sin_addr.s_addr, 
						 socket->saddr.sin_port, dip, dport, NULL);
#else
		cur_stream = CreateDualTCPStream(mtcp, socket,
						 STREAM_TYPE(MOS_SOCK_STREAM) |
						 STREAM_TYPE(MOS_SOCK_MONITOR_STREAM_ACTIVE),
						 socket->saddr.sin_addr.s_addr, 
						 socket->saddr.sin_port, dip, dport, NULL);
#endif
	}
	else
		cur_stream = CreateTCPStream(mtcp, socket, STREAM_TYPE(MOS_SOCK_STREAM),
					     socket->saddr.sin_addr.s_addr,
					     socket->saddr.sin_port, dip, dport, NULL);
	if (!cur_stream) {
		TRACE_ERROR("Socket %d: failed to create tcp_stream!\n", sockid);
		errno = ENOMEM;
		return -1;
	}

	if (is_dyn_bound)
		cur_stream->is_bound_addr = TRUE;
	cur_stream->sndvar->cwnd = 1;
	cur_stream->sndvar->ssthresh = cur_stream->sndvar->mss * 10;
	cur_stream->side = MOS_SIDE_CLI;
	/* if monitor is enabled, update the pair stream side as well */
	if (cur_stream->pair_stream) {
		cur_stream->pair_stream->side = MOS_SIDE_SVR;
		/* 
		 * if buffer management is off, then disable 
		 * monitoring tcp ring of server...
		 * if there is even a single monitor asking for
		 * buffer management, enable it (that's why the
		 * need for the loop)
		 */
		cur_stream->pair_stream->buffer_mgmt = BUFMGMT_OFF;
		struct socket_map *walk;
		SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
			uint8_t bm = walk->monitor_stream->monitor_listener->server_buf_mgmt;
			if (bm > cur_stream->pair_stream->buffer_mgmt) {
				cur_stream->pair_stream->buffer_mgmt = bm;
				break;
			}
		} SOCKQ_FOREACH_END;
	}

	cur_stream->state = TCP_ST_SYN_SENT;
	cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;

	TRACE_STATE("Stream %d: TCP_ST_SYN_SENT\n", cur_stream->id);

	SQ_LOCK(&mtcp->ctx->connect_lock);
	ret = StreamEnqueue(mtcp->connectq, cur_stream);
	SQ_UNLOCK(&mtcp->ctx->connect_lock);
	mtcp->wakeup_flag = TRUE;
	if (ret < 0) {
		TRACE_ERROR("Socket %d: failed to enqueue to conenct queue!\n", sockid);
		SQ_LOCK(&mtcp->ctx->destroyq_lock);
		StreamEnqueue(mtcp->destroyq, cur_stream);
		SQ_UNLOCK(&mtcp->ctx->destroyq_lock);
		errno = EAGAIN;
		return -1;
	}

	/* if nonblocking socket, return EINPROGRESS */
	if (socket->opts & MTCP_NONBLOCK) {
		errno = EINPROGRESS;
		return -1;

	} else {
		while (1) {
			if (!cur_stream) {
				TRACE_ERROR("STREAM DESTROYED\n");
				errno = ETIMEDOUT;
				return -1;
			}
			if (cur_stream->state > TCP_ST_ESTABLISHED) {
				TRACE_ERROR("Socket %d: weird state %s\n", 
						sockid, TCPStateToString(cur_stream));
				// TODO: how to handle this?
				errno = ENOSYS;
				return -1;
			}

			if (cur_stream->state == TCP_ST_ESTABLISHED) {
				break;
			}
			usleep(1000);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static inline int 
CloseStreamSocket(mctx_t mctx, int sockid)
{
	mtcp_manager_t mtcp;
	tcp_stream *cur_stream;
	int ret;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	cur_stream = mtcp->smap[sockid].stream;
	if (!cur_stream) {
		TRACE_API("Socket %d: stream does not exist.\n", sockid);
		errno = ENOTCONN;
		return -1;
	}

	if (cur_stream->closed) {
		TRACE_API("Socket %d (Stream %u): already closed stream\n", 
				sockid, cur_stream->id);
		return 0;
	}
	cur_stream->closed = TRUE;
		
	TRACE_API("Stream %d: closing the stream.\n", cur_stream->id);

	/* 141029 dhkim: Check this! */
	cur_stream->socket = NULL;

	if (cur_stream->state == TCP_ST_CLOSED_RSVD) {
		TRACE_API("Stream %d at TCP_ST_CLOSED_RSVD. destroying the stream.\n", 
				cur_stream->id);
		SQ_LOCK(&mtcp->ctx->destroyq_lock);
		StreamEnqueue(mtcp->destroyq, cur_stream);
		mtcp->wakeup_flag = TRUE;
		SQ_UNLOCK(&mtcp->ctx->destroyq_lock);
		return 0;

	} else if (cur_stream->state == TCP_ST_SYN_SENT) {
#if 1
		SQ_LOCK(&mtcp->ctx->destroyq_lock);
		StreamEnqueue(mtcp->destroyq, cur_stream);
		SQ_UNLOCK(&mtcp->ctx->destroyq_lock);
		mtcp->wakeup_flag = TRUE;
#endif
		return -1;

	} else if (cur_stream->state != TCP_ST_ESTABLISHED && 
			cur_stream->state != TCP_ST_CLOSE_WAIT) {
		TRACE_API("Stream %d at state %s\n", 
				cur_stream->id, TCPStateToString(cur_stream));
		errno = EBADF;
		return -1;
	}
	
	SQ_LOCK(&mtcp->ctx->close_lock);
	cur_stream->sndvar->on_closeq = TRUE;
	ret = StreamEnqueue(mtcp->closeq, cur_stream);
	mtcp->wakeup_flag = TRUE;
	SQ_UNLOCK(&mtcp->ctx->close_lock);

	if (ret < 0) {
		TRACE_ERROR("(NEVER HAPPEN) Failed to enqueue the stream to close.\n");
		errno = EAGAIN;
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static inline int 
CloseListeningSocket(mctx_t mctx, int sockid)
{
	mtcp_manager_t mtcp;
	struct tcp_listener *listener;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	listener = mtcp->smap[sockid].listener;
	if (!listener) {
		errno = EINVAL;
		return -1;
	}

	if (listener->acceptq) {
		DestroyStreamQueue(listener->acceptq);
		listener->acceptq = NULL;
	}

	pthread_mutex_lock(&listener->accept_lock);
	pthread_cond_signal(&listener->accept_cond);
	pthread_mutex_unlock(&listener->accept_lock);

	pthread_cond_destroy(&listener->accept_cond);
	pthread_mutex_destroy(&listener->accept_lock);

	free(listener);
	mtcp->smap[sockid].listener = NULL;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_close(mctx_t mctx, int sockid)
{
	mtcp_manager_t mtcp;
	int ret;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	TRACE_API("Socket %d: mtcp_close called.\n", sockid);

	switch (mtcp->smap[sockid].socktype) {
	case MOS_SOCK_STREAM:
		ret = CloseStreamSocket(mctx, sockid);
		break;

	case MOS_SOCK_STREAM_LISTEN:
		ret = CloseListeningSocket(mctx, sockid);
		break;

	case MOS_SOCK_EPOLL:
		ret = CloseEpollSocket(mctx, sockid);
		break;

	case MOS_SOCK_PIPE:
		ret = PipeClose(mctx, sockid);
		break;

	default:
		errno = EINVAL;
		ret = -1;
		break;
	}
	
	FreeSocket(mctx, sockid, mtcp->smap[sockid].socktype);

	return ret;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_abort(mctx_t mctx, int sockid)
{
	mtcp_manager_t mtcp;
	tcp_stream *cur_stream;
	int ret;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (mtcp->smap[sockid].socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (mtcp->smap[sockid].socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	cur_stream = mtcp->smap[sockid].stream;
	if (!cur_stream) {
		TRACE_API("Stream %d: does not exist.\n", sockid);
		errno = ENOTCONN;
		return -1;
	}

	TRACE_API("Socket %d: mtcp_abort()\n", sockid);
	
	FreeSocket(mctx, sockid, mtcp->smap[sockid].socktype);
	cur_stream->socket = NULL;

	if (cur_stream->state == TCP_ST_CLOSED_RSVD) {
		TRACE_API("Stream %d: connection already reset.\n", sockid);
		return ERROR;

	} else if (cur_stream->state == TCP_ST_SYN_SENT) {
		/* TODO: this should notify event failure to all 
		   previous read() or write() calls */
		cur_stream->state = TCP_ST_CLOSED_RSVD;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		SQ_LOCK(&mtcp->ctx->destroyq_lock);
		StreamEnqueue(mtcp->destroyq, cur_stream);
		SQ_UNLOCK(&mtcp->ctx->destroyq_lock);
		mtcp->wakeup_flag = TRUE;
		return 0;

	} else if (cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_LAST_ACK || 
			cur_stream->state == TCP_ST_TIME_WAIT) {
		cur_stream->state = TCP_ST_CLOSED_RSVD;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		cur_stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
		SQ_LOCK(&mtcp->ctx->destroyq_lock);
		StreamEnqueue(mtcp->destroyq, cur_stream);
		SQ_UNLOCK(&mtcp->ctx->destroyq_lock);
		mtcp->wakeup_flag = TRUE;
		return 0;
	}

	/* the stream structure will be destroyed after sending RST */
	if (cur_stream->sndvar->on_resetq) {
		TRACE_ERROR("Stream %d: calling mtcp_abort() "
				"when in reset queue.\n", sockid);
		errno = ECONNRESET;
		return -1;
	}
	SQ_LOCK(&mtcp->ctx->reset_lock);
	cur_stream->sndvar->on_resetq = TRUE;
	ret = StreamEnqueue(mtcp->resetq, cur_stream);
	SQ_UNLOCK(&mtcp->ctx->reset_lock);
	mtcp->wakeup_flag = TRUE;

	if (ret < 0) {
		TRACE_ERROR("(NEVER HAPPEN) Failed to enqueue the stream to close.\n");
		errno = EAGAIN;
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static inline int
PeekForUser(mtcp_manager_t mtcp, tcp_stream *cur_stream, char *buf, int len)
{
	struct tcp_recv_vars *rcvvar = cur_stream->rcvvar;
	int copylen;
	tcprb_t *rb = rcvvar->rcvbuf;

	if ((copylen = tcprb_ppeek(rb, (uint8_t *)buf, len, rb->pile)) <= 0) {
		errno = EAGAIN;
		return -1;
	}

	return copylen;
}
/*----------------------------------------------------------------------------*/
static inline int
CopyToUser(mtcp_manager_t mtcp, tcp_stream *cur_stream, char *buf, int len)
{
	struct tcp_recv_vars *rcvvar = cur_stream->rcvvar;
	int copylen;
	tcprb_t *rb = rcvvar->rcvbuf;
	if ((copylen = tcprb_ppeek(rb, (uint8_t *)buf, len, rb->pile)) <= 0) {
		errno = EAGAIN;
		return -1;
	}
	tcprb_setpile(rb, rb->pile + copylen);

	rcvvar->rcv_wnd = rb->len - tcprb_cflen(rb);
	//printf("rcv_wnd: %d\n", rcvvar->rcv_wnd);

	/* Advertise newly freed receive buffer */
	if (cur_stream->need_wnd_adv) {
		if (rcvvar->rcv_wnd > cur_stream->sndvar->eff_mss) {
			if (!cur_stream->sndvar->on_ackq) {
				SQ_LOCK(&mtcp->ctx->ackq_lock);
				cur_stream->sndvar->on_ackq = TRUE;
				StreamEnqueue(mtcp->ackq, cur_stream); /* this always success */
				SQ_UNLOCK(&mtcp->ctx->ackq_lock);
				cur_stream->need_wnd_adv = FALSE;
				mtcp->wakeup_flag = TRUE;
			}
		}
	}

	return copylen;
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_recv(mctx_t mctx, int sockid, char *buf, size_t len, int flags)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcp_stream *cur_stream;
	struct tcp_recv_vars *rcvvar;
	int event_remaining, merged_len;
	int ret;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}
	
	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	socket = &mtcp->smap[sockid];
	if (socket->socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (socket->socktype == MOS_SOCK_PIPE) {
		return PipeRead(mctx, sockid, buf, len);
	}
	
	if (socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}
	
	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	cur_stream = socket->stream;
	if (!cur_stream || !cur_stream->rcvvar || !cur_stream->rcvvar->rcvbuf ||
	    !(cur_stream->state >= TCP_ST_ESTABLISHED && 
	      cur_stream->state <= TCP_ST_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}
	
	rcvvar = cur_stream->rcvvar;
	
	merged_len = tcprb_cflen(rcvvar->rcvbuf);
	
	/* if CLOSE_WAIT, return 0 if there is no payload */
	if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
		if (!rcvvar->rcvbuf)
			return 0;
		
		if (merged_len == 0)
			return 0;
	}
	
	/* return EAGAIN if no receive buffer */
	if (socket->opts & MTCP_NONBLOCK) {
		if (!rcvvar->rcvbuf || merged_len == 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	
	SBUF_LOCK(&rcvvar->read_lock);

	switch (flags) {
	case 0:
		ret = CopyToUser(mtcp, cur_stream, buf, len);
		break;
	case MSG_PEEK:
		ret = PeekForUser(mtcp, cur_stream, buf, len);
		break;
	default:
		SBUF_UNLOCK(&rcvvar->read_lock);
		ret = -1;
		errno = EINVAL;
		return ret;
	}
	
	merged_len = tcprb_cflen(rcvvar->rcvbuf);
	event_remaining = FALSE;
	/* if there are remaining payload, generate EPOLLIN */
	/* (may due to insufficient user buffer) */
	if (socket->epoll & MOS_EPOLLIN) {
		if (!(socket->epoll & MOS_EPOLLET) && merged_len > 0) {
			event_remaining = TRUE;
		}
	}
	/* if waiting for close, notify it if no remaining data */
	if (cur_stream->state == TCP_ST_CLOSE_WAIT && 
	    merged_len == 0 && ret > 0) {
		event_remaining = TRUE;
	}
	
	SBUF_UNLOCK(&rcvvar->read_lock);
	
	if (event_remaining) {
		if (socket->epoll) {
			AddEpollEvent(mtcp->ep, 
				      USR_SHADOW_EVENT_QUEUE, socket, MOS_EPOLLIN);
		}
	}
	
	TRACE_API("Stream %d: mtcp_recv() returning %d\n", cur_stream->id, ret);
	return ret;
}
/*----------------------------------------------------------------------------*/
inline ssize_t
mtcp_read(mctx_t mctx, int sockid, char *buf, size_t len)
{
	return mtcp_recv(mctx, sockid, buf, len, 0);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_readv(mctx_t mctx, int sockid, const struct iovec *iov, int numIOV)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcp_stream *cur_stream;
	struct tcp_recv_vars *rcvvar;
	int ret, bytes_read, i;
	int event_remaining, merged_len;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	socket = &mtcp->smap[sockid];
	if (socket->socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	/* stream should be in ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
	cur_stream = socket->stream;
	if (!cur_stream || 
			!(cur_stream->state >= TCP_ST_ESTABLISHED && 
			  cur_stream->state <= TCP_ST_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	rcvvar = cur_stream->rcvvar;

	merged_len = tcprb_cflen(rcvvar->rcvbuf);

	/* if CLOSE_WAIT, return 0 if there is no payload */
	if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
		if (!rcvvar->rcvbuf)
			return 0;
		
		if (merged_len == 0)
			return 0;
	}

	/* return EAGAIN if no receive buffer */
	if (socket->opts & MTCP_NONBLOCK) {
		if (!rcvvar->rcvbuf || merged_len == 0) {
			errno = EAGAIN;
			return -1;
		}
	}
	
	SBUF_LOCK(&rcvvar->read_lock);
	
	/* read and store the contents to the vectored buffers */ 
	bytes_read = 0;
	for (i = 0; i < numIOV; i++) {
		if (iov[i].iov_len <= 0)
			continue;

		ret = CopyToUser(mtcp, cur_stream, iov[i].iov_base, iov[i].iov_len);
		if (ret <= 0)
			break;

		bytes_read += ret;

		if (ret < iov[i].iov_len)
			break;
	}

	merged_len = tcprb_cflen(rcvvar->rcvbuf);

	event_remaining = FALSE;
	/* if there are remaining payload, generate read event */
	/* (may due to insufficient user buffer) */
	if (socket->epoll & MOS_EPOLLIN) {
		if (!(socket->epoll & MOS_EPOLLET) && merged_len > 0) {
			event_remaining = TRUE;
		}
	}
	/* if waiting for close, notify it if no remaining data */
	if (cur_stream->state == TCP_ST_CLOSE_WAIT && 
			merged_len == 0 && bytes_read > 0) {
		event_remaining = TRUE;
	}

	SBUF_UNLOCK(&rcvvar->read_lock);

	if(event_remaining) {
		if (socket->epoll & MOS_EPOLLIN && !(socket->epoll & MOS_EPOLLET)) {
			AddEpollEvent(mtcp->ep, 
				      USR_SHADOW_EVENT_QUEUE, socket, MOS_EPOLLIN);
		}
	}

	TRACE_API("Stream %d: mtcp_readv() returning %d\n", 
			cur_stream->id, bytes_read);
	return bytes_read;
}
/*----------------------------------------------------------------------------*/
static inline int 
CopyFromUser(mtcp_manager_t mtcp, tcp_stream *cur_stream, const char *buf, int len)
{
	struct tcp_send_vars *sndvar = cur_stream->sndvar;
	int sndlen;
	int ret;

	sndlen = MIN((int)sndvar->snd_wnd, len);
	if (sndlen <= 0) {
		errno = EAGAIN;
		return -1;
	}

	/* allocate send buffer if not exist */
	if (!sndvar->sndbuf) {
		sndvar->sndbuf = SBInit(mtcp->rbm_snd, sndvar->iss + 1);
		if (!sndvar->sndbuf) {
			cur_stream->close_reason = TCP_NO_MEM;
			/* notification may not required due to -1 return */
			errno = ENOMEM;
			return -1;
		}
	}

	ret = SBPut(mtcp->rbm_snd, sndvar->sndbuf, buf, sndlen);
	assert(ret == sndlen);
	sndvar->snd_wnd = sndvar->sndbuf->size - sndvar->sndbuf->len;
	if (ret <= 0) {
		TRACE_ERROR("SBPut failed. reason: %d (sndlen: %u, len: %u\n", 
				ret, sndlen, sndvar->sndbuf->len);
		errno = EAGAIN;
		return -1;
	}
	
	if (sndvar->snd_wnd <= 0) {
		TRACE_SNDBUF("%u Sending buffer became full!! snd_wnd: %u\n", 
				cur_stream->id, sndvar->snd_wnd);
	}

	return ret;
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_write(mctx_t mctx, int sockid, const char *buf, size_t len)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcp_stream *cur_stream;
	struct tcp_send_vars *sndvar;
	int ret;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	socket = &mtcp->smap[sockid];
	if (socket->socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (socket->socktype == MOS_SOCK_PIPE) {
		return PipeWrite(mctx, sockid, buf, len);
	}

	if (socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}
	
	cur_stream = socket->stream;
	if (!cur_stream || 
			!(cur_stream->state == TCP_ST_ESTABLISHED || 
			  cur_stream->state == TCP_ST_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	if (len <= 0) {
		if (socket->opts & MTCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			return 0;
		}
	}

	sndvar = cur_stream->sndvar;

	SBUF_LOCK(&sndvar->write_lock);
	ret = CopyFromUser(mtcp, cur_stream, buf, len);

	SBUF_UNLOCK(&sndvar->write_lock);

	if (ret > 0 && !(sndvar->on_sendq || sndvar->on_send_list)) {
		SQ_LOCK(&mtcp->ctx->sendq_lock);
		sndvar->on_sendq = TRUE;
		StreamEnqueue(mtcp->sendq, cur_stream);		/* this always success */
		SQ_UNLOCK(&mtcp->ctx->sendq_lock);
		mtcp->wakeup_flag = TRUE;
	}

	if (ret == 0 && (socket->opts & MTCP_NONBLOCK)) {
		ret = -1;
		errno = EAGAIN;
	}

	/* if there are remaining sending buffer, generate write event */
	if (sndvar->snd_wnd > 0) {
		if (socket->epoll & MOS_EPOLLOUT && !(socket->epoll & MOS_EPOLLET)) {
			AddEpollEvent(mtcp->ep, 
				      USR_SHADOW_EVENT_QUEUE, socket, MOS_EPOLLOUT);
		}
	}

	TRACE_API("Stream %d: mtcp_write() returning %d\n", cur_stream->id, ret);
	return ret;
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_writev(mctx_t mctx, int sockid, const struct iovec *iov, int numIOV)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	tcp_stream *cur_stream;
	struct tcp_send_vars *sndvar;
	int ret, to_write, i;

	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}

	if (sockid < 0 || sockid >= g_config.mos->max_concurrency) {
		TRACE_API("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	socket = &mtcp->smap[sockid];
	if (socket->socktype == MOS_SOCK_UNUSED) {
		TRACE_API("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (socket->socktype != MOS_SOCK_STREAM) {
		TRACE_API("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}
	
	cur_stream = socket->stream;
	if (!cur_stream || 
			!(cur_stream->state == TCP_ST_ESTABLISHED || 
			  cur_stream->state == TCP_ST_CLOSE_WAIT)) {
		errno = ENOTCONN;
		return -1;
	}

	sndvar = cur_stream->sndvar;
	SBUF_LOCK(&sndvar->write_lock);

	/* write from the vectored buffers */ 
	to_write = 0;
	for (i = 0; i < numIOV; i++) {
		if (iov[i].iov_len <= 0)
			continue;

		ret = CopyFromUser(mtcp, cur_stream, iov[i].iov_base, iov[i].iov_len);
		if (ret <= 0)
			break;

		to_write += ret;

		if (ret < iov[i].iov_len)
			break;
	}
	SBUF_UNLOCK(&sndvar->write_lock);

	if (to_write > 0 && !(sndvar->on_sendq || sndvar->on_send_list)) {
		SQ_LOCK(&mtcp->ctx->sendq_lock);
		sndvar->on_sendq = TRUE;
		StreamEnqueue(mtcp->sendq, cur_stream);		/* this always success */
		SQ_UNLOCK(&mtcp->ctx->sendq_lock);
		mtcp->wakeup_flag = TRUE;
	}

	if (to_write == 0 && (socket->opts & MTCP_NONBLOCK)) {
		to_write = -1;
		errno = EAGAIN;
	}

	/* if there are remaining sending buffer, generate write event */
	if (sndvar->snd_wnd > 0) {
		if (socket->epoll & MOS_EPOLLOUT && !(socket->epoll & MOS_EPOLLET)) {
			AddEpollEvent(mtcp->ep, 
				      USR_SHADOW_EVENT_QUEUE, socket, MOS_EPOLLOUT);
		}
	}

	TRACE_API("Stream %d: mtcp_writev() returning %d\n", 
			cur_stream->id, to_write);
	return to_write;
}
/*----------------------------------------------------------------------------*/
uint32_t
mtcp_get_connection_cnt(mctx_t mctx)
{
	mtcp_manager_t mtcp;
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		errno = EACCES;
		return -1;
	}
	
	if (mtcp->num_msp > 0)
		return mtcp->flow_cnt / 2;
	else
		return mtcp->flow_cnt;
}
/*----------------------------------------------------------------------------*/
