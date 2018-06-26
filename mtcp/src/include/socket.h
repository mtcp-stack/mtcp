#ifndef SOCKET_H
#define SOCKET_H

#include "mtcp_api.h"
#include "mtcp_epoll.h"

/*----------------------------------------------------------------------------*/
enum socket_opts
{
	MTCP_NONBLOCK		= 0x01,
	MTCP_ADDR_BIND		= 0x02, 
};
/*----------------------------------------------------------------------------*/
struct socket_map
{
	int id;
	int socktype;
	uint32_t opts;

	struct sockaddr_in saddr;

	union {
		struct tcp_stream *stream;
		struct tcp_listener *listener;
		struct mtcp_epoll *ep;
		struct pipe *pp;
	};

	uint32_t epoll;			/* registered events */
	uint32_t events;		/* available events */
	mtcp_epoll_data_t ep_data;

	TAILQ_ENTRY (socket_map) free_smap_link;

};
/*----------------------------------------------------------------------------*/
typedef struct socket_map * socket_map_t;
/*----------------------------------------------------------------------------*/
socket_map_t 
AllocateSocket(mctx_t mctx, int socktype, int need_lock);
/*----------------------------------------------------------------------------*/
void 
FreeSocket(mctx_t mctx, int sockid, int need_lock); 
/*----------------------------------------------------------------------------*/
socket_map_t 
GetSocket(mctx_t mctx, int sockid);
/*----------------------------------------------------------------------------*/
struct tcp_listener
{
	int sockid;
	socket_map_t socket;

	int backlog;
	stream_queue_t acceptq;
	
	pthread_mutex_t accept_lock;
	pthread_cond_t accept_cond;

	TAILQ_ENTRY(tcp_listener) he_link;	/* hash table entry link */
};
/*----------------------------------------------------------------------------*/

#endif /* SOCKET_H */
