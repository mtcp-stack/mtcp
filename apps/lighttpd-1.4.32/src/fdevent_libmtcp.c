#include "fdevent.h"
#include "buffer.h"
#include "log.h"

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
	 
#ifdef HAVE_LIBMTCP
#include "mtcp_api.h"
#include "mtcp_epoll.h"
/*----------------------------------------------------------------------------*/
static void
fdevent_libmtcp_epoll_free(fdevents *ev)
{
	mtcp_close(ev->srv->mctx, ev->epoll_fd);
	free(ev->_epoll_events);
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_del(fdevents *ev, int fde_ndx, int fd)
{
  /*struct mtcp_epoll_event ep;*/
	
	if (fde_ndx < 0) return -1;
#if 0		
	memset(&ep, 0, sizeof(ep));

	ep.data.sockid = fd;
	ep.data.ptr = NULL;
#endif	
	if (0 != mtcp_epoll_ctl(ev->srv->mctx, ev->epoll_fd, MTCP_EPOLL_CTL_DEL, fd, NULL/*&ep*/)) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
				"epoll_ctl failed: ", strerror(errno), ", dying");
		
		SEGFAULT();
		
		return 0;
	}
	
	
	return -1;
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_set(fdevents *ev, int fde_ndx, int fd, int events)
{
	struct mtcp_epoll_event ep;
	int add = 0;

	if (fde_ndx == -1) add = 1;
	/*memset(&ep, 0, sizeof(struct mtcp_epoll_event));*/
	ep.events = 0;
	
	if (events & FDEVENT_IN)  ep.events |= MTCP_EPOLLIN;
	if (events & FDEVENT_OUT) ep.events |= MTCP_EPOLLOUT;

	/**
	 *
	 * with EPOLLET we don't get a FDEVENT_HUP
	 * if the close is delay after everything has
	 * sent.
	 *
	 */

	ep.events |= MTCP_EPOLLERR | MTCP_EPOLLHUP /* | EPOLLET */;

	/*ep.data.ptr = NULL;*/
	ep.data.sockid = fd;

	if (0 != mtcp_epoll_ctl(ev->srv->mctx, ev->epoll_fd, add ? MTCP_EPOLL_CTL_ADD : MTCP_EPOLL_CTL_MOD, fd, &ep)) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"epoll_ctl failed: ", strerror(errno), ", dying");

		SEGFAULT();

		return 0;
	}

	return fd;
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_poll(fdevents *ev, int timeout_ms)
{
	return mtcp_epoll_wait(ev->srv->mctx, ev->epoll_fd, ev->_epoll_events,
			       ev->maxfds, timeout_ms);
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_get_revent(fdevents *ev, size_t ndx)
{
	int events = 0, e;

	e = ev->_epoll_events[ndx].events;
	if (e & MTCP_EPOLLIN) events |= FDEVENT_IN;
	if (e & MTCP_EPOLLOUT) events |= FDEVENT_OUT;
	if (e & MTCP_EPOLLERR) events |= FDEVENT_ERR;
	if (e & MTCP_EPOLLHUP) events |= FDEVENT_HUP;
	if (e & MTCP_EPOLLPRI) events |= FDEVENT_PRI;

	return events;
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_get_fd(fdevents *ev, size_t ndx)
{
	return ev->_epoll_events[ndx].data.sockid;
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_next_fdndx(fdevents *ev, int ndx)
{
	size_t i;

	UNUSED(ev);

	i = (ndx < 0) ? 0 : ndx + 1;

	return i;
}
/*----------------------------------------------------------------------------*/
int
fdevent_libmtcp_epoll_init(fdevents *ev)
{
	ev->type = FDEVENT_HANDLER_LIBMTCP;
#define SET(x)					\
	ev->x = fdevent_libmtcp_epoll_##x;

	SET(free);
	SET(poll);

	SET(event_del);
	SET(event_set);

	SET(event_next_fdndx);
	SET(event_get_fd);
	SET(event_get_revent);
	
	/* setting the max events logic based on `epserver' logic */
	if (-1 == (ev->epoll_fd = mtcp_epoll_create(ev->srv->mctx, ev->srv->max_conns * 3))) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"epoll_create failed (", strerror(errno), ")");

		return -1;
	}

	ev->_epoll_events = calloc(ev->srv->max_conns * 3, 
				  sizeof(struct mtcp_epoll_event));
	if (NULL == ev->_epoll_events) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
				"_epoll_events calloc failed!", strerror(errno), ")");
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
#else
int
fdevent_libmtcp_epoll_init(fdevents *ev)
{
	UNUSED(ev);
	
	log_error_write(ev->srv, __FILE__, __LINE__, "S",
			"libmtcp-epoll not supported, try to set server.event-handler = \"poll\" or \"select\"");
	
	return -1;
}
/*----------------------------------------------------------------------------*/
#endif /* !HAVE_LIBMTCP */
