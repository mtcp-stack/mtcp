#include <pthread.h>
#include <errno.h>

#include "pipe.h"
#include "eventpoll.h"
#include "tcp_stream.h"
#include "mtcp.h"
#include "debug.h"

#define PIPE_BUF_SIZE 10240

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*---------------------------------------------------------------------------*/
enum pipe_state
{
	PIPE_CLOSED, 
	PIPE_ACTIVE, 
	PIPE_CLOSE_WAIT, 
};
/*---------------------------------------------------------------------------*/
struct pipe
{
	int state;
	socket_map_t socket[2];

	char *buf;
	int buf_off;
	int buf_tail;
	int buf_len;
	int buf_size;

	pthread_mutex_t pipe_lock;
	pthread_cond_t pipe_cond;
};
/*---------------------------------------------------------------------------*/
int 
mtcp_pipe(mctx_t mctx, int pipeid[2])
{
	socket_map_t socket[2];
	struct pipe *pp;
	int ret;
	
	socket[0] = AllocateSocket(mctx, MTCP_SOCK_PIPE, FALSE);
	if (!socket[0]) {
		errno = ENFILE;
		return -1;
	}
	socket[1] = AllocateSocket(mctx, MTCP_SOCK_PIPE, FALSE);
	if (!socket[1]) {
		FreeSocket(mctx, socket[0]->id, FALSE);
		errno = ENFILE;
		return -1;
	}

	pp = (struct pipe *)calloc(1, sizeof(struct pipe));
	if (!pp) {
		/* errno set by calloc() */
		FreeSocket(mctx, socket[0]->id, FALSE);
		FreeSocket(mctx, socket[1]->id, FALSE);
		return -1;
	}

	pp->buf_size = PIPE_BUF_SIZE;
	pp->buf = (char *)malloc(pp->buf_size);
	if (!pp->buf) {
		/* errno set by malloc() */
		FreeSocket(mctx, socket[0]->id, FALSE);
		FreeSocket(mctx, socket[1]->id, FALSE);
		free(pp);
		return -1;
	}

	ret = pthread_mutex_init(&pp->pipe_lock, NULL);
	if (ret) {
		/* errno set by pthread_mutex_init() */
		FreeSocket(mctx, socket[0]->id, FALSE);
		FreeSocket(mctx, socket[1]->id, FALSE);
		free(pp->buf);
		free(pp);
		return -1;

	}
	ret = pthread_cond_init(&pp->pipe_cond, NULL);
	if (ret) {
		/* errno set by pthread_cond_init() */
		FreeSocket(mctx, socket[0]->id, FALSE);
		FreeSocket(mctx, socket[1]->id, FALSE);
		free(pp->buf);
		pthread_mutex_destroy(&pp->pipe_lock);
		free(pp);
		return -1;
	}

	pp->state = PIPE_ACTIVE;
	pp->socket[0] = socket[0];
	pp->socket[1] = socket[1];
	socket[0]->pp = pp;
	socket[1]->pp = pp;

	pipeid[0] = socket[0]->id;
	pipeid[1] = socket[1]->id;

	return 0;
	
}
/*---------------------------------------------------------------------------*/
static void 
RaiseEventToPair(mtcp_manager_t mtcp, socket_map_t socket, uint32_t event)
{
	struct pipe *pp = socket->pp;
	socket_map_t pair_socket;

	if (pp->socket[0] == socket)
		pair_socket = pp->socket[1];
	else
		pair_socket = pp->socket[0];

	if (pair_socket->opts & MTCP_NONBLOCK) {
		if (pair_socket->epoll) {
			AddEpollEvent(mtcp->ep, USR_EVENT_QUEUE, pair_socket, event);
		}
	} else {
		pthread_cond_signal(&pp->pipe_cond);
	}
}
/*---------------------------------------------------------------------------*/
int 
PipeRead(mctx_t mctx, int pipeid, char *buf, int len)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	struct pipe *pp;
	int to_read;
	int to_notify;
	int ret;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}
	socket = GetSocket(mctx, pipeid);
	if (!socket) {
		return -1;
	}
	if (socket->socktype != MTCP_SOCK_PIPE) {
		errno = EBADF;
		return -1;
	}
	pp = socket->pp;
	if (!pp) {
		errno = EBADF;
		return -1;
	}
	if (pp->state == PIPE_CLOSED) {
		errno = EINVAL;
		return -1;
	}
	if (pp->state == PIPE_CLOSE_WAIT && pp->buf_len == 0) {
		return 0;
	}

	if (len <= 0) {
		if (socket->opts & MTCP_NONBLOCK) {
			errno = EAGAIN;
			return -1;
		} else {
			return 0;
		}
	}

	pthread_mutex_lock(&pp->pipe_lock);
	if (!(socket->opts & MTCP_NONBLOCK)) {
		while (pp->buf_len == 0) {
			ret = pthread_cond_wait(&pp->pipe_cond, &pp->pipe_lock);
			if (ret) {
				/* errno set by pthread_cond_wait() */
				pthread_mutex_unlock(&pp->pipe_lock);
				return -1;
			}
		}
	}

	to_read = MIN(len, pp->buf_len);
	if (to_read <= 0) {
		pthread_mutex_unlock(&pp->pipe_lock);
		if (pp->state == PIPE_ACTIVE) {
			errno = EAGAIN;
			return -1;
		} else if (pp->state == PIPE_CLOSE_WAIT) {
			return 0;
		}
	}

	/* if the buffer was full, notify the write event to the pair socket */
	to_notify = FALSE;
	if (pp->buf_len == pp->buf_size)
		to_notify = TRUE;

	if (pp->buf_off + to_read < pp->buf_size) {
		memcpy(buf, pp->buf + pp->buf_off, to_read);
		pp->buf_off += to_read;
	} else {
		int temp_read = pp->buf_size - pp->buf_off;
		memcpy(buf, pp->buf + pp->buf_off, temp_read);
		memcpy(buf + temp_read, pp->buf, to_read - temp_read);
		pp->buf_off = to_read - temp_read;
	}
	pp->buf_len -= to_read;

	/* notify to the pair socket for new buffer space */
	if (to_notify) {
		RaiseEventToPair(mtcp, socket, MTCP_EPOLLOUT);
	}

	pthread_mutex_unlock(&pp->pipe_lock);

	/* if level triggered, raise event for remainig buffer */
	if (pp->buf_len > 0) {
		if ((socket->epoll & MTCP_EPOLLIN) && !(socket->epoll & MTCP_EPOLLET)) {
			AddEpollEvent(mtcp->ep, 
					USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLIN);
		}
	} else if (pp->state == PIPE_CLOSE_WAIT && pp->buf_len == 0) {
		AddEpollEvent(mtcp->ep, USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLIN);
	}

	return to_read;
}
/*---------------------------------------------------------------------------*/
int 
PipeWrite(mctx_t mctx, int pipeid, const char *buf, int len)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	struct pipe *pp;
	int to_write;
	int to_notify;
	int ret;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}
	socket = GetSocket(mctx, pipeid);
	if (!socket) {
		return -1;
	}
	if (socket->socktype != MTCP_SOCK_PIPE) {
		errno = EBADF;
		return -1;
	}
	pp = socket->pp;
	if (!pp) {
		errno = EBADF;
		return -1;
	}
	if (pp->state == PIPE_CLOSED) {
		errno = EINVAL;
		return -1;
	}
	if (pp->state == PIPE_CLOSE_WAIT) {
		errno = EPIPE;
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

	pthread_mutex_lock(&pp->pipe_lock);
	if (!(socket->opts & MTCP_NONBLOCK)) {
		while (pp->buf_len == pp->buf_size) {
			ret = pthread_cond_wait(&pp->pipe_cond, &pp->pipe_lock);
			if (ret) {
				/* errno set by pthread_cond_wait() */
				pthread_mutex_unlock(&pp->pipe_lock);
				return -1;
			}
		}
	}

	to_write = MIN(len, pp->buf_size - pp->buf_len);
	if (to_write <= 0) {
		pthread_mutex_unlock(&pp->pipe_lock);
		errno = EAGAIN;
		return -1;
	}

	/* if the buffer was empty, notify read event to the pair socket */
	to_notify = FALSE;
	if (pp->buf_len == 0)
		to_notify = TRUE;

	if (pp->buf_tail + to_write < pp->buf_size) {
		/* if the data fit into the buffer, copy it */
		memcpy(pp->buf + pp->buf_tail, buf, to_write);
		pp->buf_tail += to_write;
	} else {
		/* if the data overflow the buffer, wrap around the buffer */
		int temp_write = pp->buf_size - pp->buf_tail;
		memcpy(pp->buf + pp->buf_tail, buf, temp_write);
		memcpy(pp->buf, buf + temp_write, to_write - temp_write);
		pp->buf_tail = to_write - temp_write;
	}
	pp->buf_len += to_write;

	/* notify to the pair socket for the new buffers */
	if (to_notify) {
		RaiseEventToPair(mtcp, socket, MTCP_EPOLLIN);
	}

	pthread_mutex_unlock(&pp->pipe_lock);

	/* if level triggered, raise event for remainig buffer */
	if (pp->buf_len < pp->buf_size) {
		if ((socket->epoll & MTCP_EPOLLOUT) && !(socket->epoll & MTCP_EPOLLET)) {
			AddEpollEvent(mtcp->ep, 
					USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLOUT);
		}
	}

	return to_write;
}
/*----------------------------------------------------------------------------*/
int 
RaisePendingPipeEvents(mctx_t mctx, int epid, int pipeid)
{
	struct mtcp_epoll *ep = GetSocket(mctx, epid)->ep;
	socket_map_t socket = GetSocket(mctx, pipeid);
	struct pipe *pp = socket->pp;

	if (!pp)
		return -1;
	if (pp->state < PIPE_ACTIVE)
		return -1;

	/* if there are payloads already read before epoll registration */
	/* generate read event */
	if (socket->epoll & MTCP_EPOLLIN) {
		if (pp->buf_len > 0) {
			AddEpollEvent(ep, USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLIN);
		} else if (pp->state == PIPE_CLOSE_WAIT) {
			AddEpollEvent(ep, USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLIN);
		}
	}

	/* same thing to the write event */
	if (socket->epoll & MTCP_EPOLLOUT) {
		if (pp->buf_len < pp->buf_size) {
			AddEpollEvent(ep, USR_SHADOW_EVENT_QUEUE, socket, MTCP_EPOLLOUT);
		}
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
int 
PipeClose(mctx_t mctx, int pipeid)
{
	mtcp_manager_t mtcp;
	socket_map_t socket;
	struct pipe *pp;
	
	mtcp = GetMTCPManager(mctx);
	if (!mtcp) {
		return -1;
	}
	socket = GetSocket(mctx, pipeid);
	if (!socket) {
		return -1;
	}
	if (socket->socktype != MTCP_SOCK_PIPE) {
		errno = EINVAL;
		return -1;
	}
	pp = socket->pp;
	if (!pp) {
		return 0;
	}

	if (pp->state == PIPE_CLOSED) {
		return 0;
	}

	pthread_mutex_lock(&pp->pipe_lock);
	if (pp->state == PIPE_ACTIVE) {
		pp->state = PIPE_CLOSE_WAIT;
		RaiseEventToPair(mtcp, socket, MTCP_EPOLLIN);
		pthread_mutex_unlock(&pp->pipe_lock);
		return 0;
	}

	/* control reaches here only when PIPE_CLOSE_WAIT */

	if (pp->socket[0])
		pp->socket[0]->pp = NULL;
	if (pp->socket[1])
		pp->socket[1]->pp = NULL;
	
	pthread_mutex_unlock(&pp->pipe_lock);

	pthread_mutex_destroy(&pp->pipe_lock);
	pthread_cond_destroy(&pp->pipe_cond);

	free(pp->buf);

	free(pp);

	return 0;
}
/*---------------------------------------------------------------------------*/
