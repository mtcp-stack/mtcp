#ifndef EVENTPOLL_H
#define EVENTPOLL_H

#include "mtcp_api.h"
#include "mtcp_epoll.h"
#include <sys/eventfd.h>

#define USE_EVENT_FD		1
/*----------------------------------------------------------------------------*/
struct mtcp_epoll_stat
{
	uint64_t calls;
	uint64_t waits;
	uint64_t wakes;

	uint64_t issued;
	uint64_t registered;
	uint64_t invalidated;
	uint64_t handled;
};
/*----------------------------------------------------------------------------*/
struct mtcp_epoll_event_int
{
	struct mtcp_epoll_event ev;
	int sockid;
};
/*----------------------------------------------------------------------------*/
enum event_queue_type
{
	USR_EVENT_QUEUE = 0, 
	USR_SHADOW_EVENT_QUEUE = 1, 
	MTCP_EVENT_QUEUE = 2
};
/*----------------------------------------------------------------------------*/
struct event_queue
{
	struct mtcp_epoll_event_int *events;
	int start;			// starting index
	int end;			// ending index
	
	int size;			// max size
	int num_events;		// number of events
};
/*----------------------------------------------------------------------------*/
struct mtcp_epoll
{
	struct event_queue *usr_queue;
	struct event_queue *usr_shadow_queue;
	struct event_queue *mtcp_queue;

	uint8_t waiting;
	struct mtcp_epoll_stat stat;

	int efd;
#ifndef USE_EVENT_FD
	pthread_cond_t epoll_cond;
#endif
	pthread_mutex_t epoll_lock;
};
/*----------------------------------------------------------------------------*/

int 
CloseEpollSocket(mctx_t mctx, int epid);

#endif /* EVENTPOLL_H */
