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
#include <sys/epoll.h>
	 
#ifdef HAVE_LIBMTCP
#include "mtcp_api.h"
#include "mtcp_epoll.h"
/*----------------------------------------------------------------------------*/
static void
fdevent_libmtcp_epoll_free(fdevents *ev)
{
	close(ev->epoll_fd);
	free(ev->_epoll_events);
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_del(fdevents *ev, int fde_ndx, int fd)
{
	if (fde_ndx < 0) return -1;
#if 0		
	memset(&ep, 0, sizeof(ep));

	ep.data.sockid = fd;
	ep.data.ptr = NULL;
#endif	
	if (0 != epoll_ctl(ev->epoll_fd, EPOLL_CTL_DEL, fd, NULL/*&ep*/)) {
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
	struct epoll_event ep;
	int add = 0;

	if (fde_ndx == -1) add = 1;
	ep.events = 0;
	
	if (events & FDEVENT_IN)  ep.events |= EPOLLIN;
	if (events & FDEVENT_OUT) ep.events |= EPOLLOUT;

	/**
	 *
	 * with EPOLLET we don't get a FDEVENT_HUP
	 * if the close is delay after everything has
	 * sent.
	 *
	 */

	ep.events |= EPOLLERR | EPOLLHUP /* | EPOLLET */;

	/*ep.data.ptr = NULL;*/
	ep.data.fd = fd;

	if (0 != epoll_ctl(ev->epoll_fd, add ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ep)) {
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
	return epoll_wait(ev->epoll_fd, ev->_epoll_events,
			  ev->maxfds, timeout_ms);	
}
/*----------------------------------------------------------------------------*/
static int
fdevent_libmtcp_epoll_event_get_revent(fdevents *ev, size_t ndx)
{
	int events = 0, e;

	e = ev->_epoll_events[ndx].events;
	if (e & EPOLLIN) events |= FDEVENT_IN;
	if (e & EPOLLOUT) events |= FDEVENT_OUT;
	if (e & EPOLLERR) events |= FDEVENT_ERR;
	if (e & EPOLLHUP) events |= FDEVENT_HUP;
	if (e & EPOLLPRI) events |= FDEVENT_PRI;

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
	if (-1 == (ev->epoll_fd = epoll_create(ev->srv->max_conns * 3))) {
		log_error_write(ev->srv, __FILE__, __LINE__, "SSS",
			"epoll_create failed (", strerror(errno), ")");

		return -1;
	}

	ev->_epoll_events = calloc(ev->srv->max_conns * 3, 
				  sizeof(struct epoll_event));
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
