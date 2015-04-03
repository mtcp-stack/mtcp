#ifndef __MTCP_API_H_
#define __MTCP_API_H_

#include <stdint.h>
#include <netinet/in.h>
#include <sys/uio.h>

#define UNUSED(x)	(void)x

#ifdef __cplusplus
extern "C" {
#endif

enum socket_type
{
	MTCP_SOCK_UNUSED, 
	MTCP_SOCK_STREAM, 
	MTCP_SOCK_PROXY, 
	MTCP_SOCK_LISTENER, 
	MTCP_SOCK_EPOLL, 
	MTCP_SOCK_PIPE, 
};

struct mtcp_conf
{
	int num_cores;
	int max_concurrency;

	int max_num_buffers;
	int rcvbuf_size;
	int sndbuf_size;

	int tcp_timewait;
	int tcp_timeout;
};

typedef struct mtcp_context *mctx_t;

int 
mtcp_init(char *config_file);

void 
mtcp_destroy();

int 
mtcp_getconf(struct mtcp_conf *conf);

int 
mtcp_setconf(const struct mtcp_conf *conf);

int 
mtcp_core_affinitize(int cpu);

mctx_t 
mtcp_create_context(int cpu);

void 
mtcp_destroy_context(mctx_t mctx);

typedef void (*mtcp_sighandler_t)(int);

mtcp_sighandler_t 
mtcp_register_signal(int signum, mtcp_sighandler_t handler);

int 
mtcp_pipe(mctx_t mctx, int pipeid[2]);

int 
mtcp_getsockopt(mctx_t mctx, int sockid, int level, 
		int optname, void *optval, socklen_t *optlen);

int 
mtcp_setsockopt(mctx_t mctx, int sockid, int level, 
		int optname, const void *optval, socklen_t optlen);

int 
mtcp_setsock_nonblock(mctx_t mctx, int sockid);

/* mtcp_socket_ioctl: similar to ioctl, 
   but only FIONREAD is supported currently */
int 
mtcp_socket_ioctl(mctx_t mctx, int sockid, int request, void *argp);

int 
mtcp_socket(mctx_t mctx, int domain, int type, int protocol);

int 
mtcp_bind(mctx_t mctx, int sockid, 
		const struct sockaddr *addr, socklen_t addrlen);

int 
mtcp_listen(mctx_t mctx, int sockid, int backlog);

int 
mtcp_accept(mctx_t mctx, int sockid, struct sockaddr *addr, socklen_t *addrlen);

int 
mtcp_init_rss(mctx_t mctx, in_addr_t saddr_base, int num_addr, 
		in_addr_t daddr, in_addr_t dport);

int 
mtcp_connect(mctx_t mctx, int sockid, 
		const struct sockaddr *addr, socklen_t addrlen);

int 
mtcp_close(mctx_t mctx, int sockid);

int 
mtcp_abort(mctx_t mctx, int sockid);

int
mtcp_read(mctx_t mctx, int sockid, char *buf, int len);

/* readv should work in atomic */
int
mtcp_readv(mctx_t mctx, int sockid, struct iovec *iov, int numIOV);

int
mtcp_write(mctx_t mctx, int sockid, char *buf, int len);

/* writev should work in atomic */
int
mtcp_writev(mctx_t mctx, int sockid, struct iovec *iov, int numIOV);

#if 0
int
mtcp_delete(mctx_t mctx, int sockid, int len);
#endif

#ifdef __cplusplus
};
#endif

#endif /* __MTCP_API_H_ */
