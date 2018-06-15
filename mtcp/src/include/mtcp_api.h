#ifndef __MTCP_API_H_
#define __MTCP_API_H_

#include <stdint.h>
#include <netinet/in.h>
#include <sys/uio.h>

#ifndef UNUSED
#define UNUSED(x)	(void)x
#endif

#ifndef INPORT_ANY
#define INPORT_ANY	(uint16_t)0
#endif

/*******************************************/
// LIBOS

#ifndef C_MAX_QUEUE_DEPTH
#define C_MAX_QUEUE_DEPTH 40
#endif

#ifndef C_MAX_SGARRAY_SIZE
#define C_MAX_SGARRAY_SIZE 10
#endif

#ifndef C_ZEUS_IO_ERR_NO
#define C_ZEUS_IO_ERR_NO (-9)
#endif


// NOTE: duplication as io-queue_c.h
// TODO: split all the DS into single h file
typedef struct Sgelem{
    void * buf;
    size_t len;
}zeus_sgelem;

typedef struct Sgarray{
    int num_bufs;
    zeus_sgelem bufs[C_MAX_SGARRAY_SIZE];
}zeus_sgarray;

/*******************************************/


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
mtcp_init(const char *config_file);

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

/** Returns the current address to which the socket sockfd is bound
 * @param [in] mctx: mtcp context
 * @param [in] addr: address buffer to be filled
 * @param [in] addrlen: amount of space pointed to by addr
 * @return 0 on success, -1 on error
 */
int
mtcp_getsockname(mctx_t mctx, int sock, struct sockaddr *addr, socklen_t *addrlen);
	
int
mtcp_getpeername(mctx_t mctx, int sockid, struct sockaddr *addr,
		 socklen_t *addrlen);

inline ssize_t
mtcp_read(mctx_t mctx, int sockid, char *buf, size_t len);

ssize_t
mtcp_recv(mctx_t mctx, int sockid, char *buf, size_t len, int flags);

/* readv should work in atomic */
int
mtcp_readv(mctx_t mctx, int sockid, const struct iovec *iov, int numIOV);

ssize_t
mtcp_write(mctx_t mctx, int sockid, const char *buf, size_t len);

/* writev should work in atomic */
int
mtcp_writev(mctx_t mctx, int sockid, const struct iovec *iov, int numIOV);


/******************************************************************************/
// libos_mtcp c interface
/******************************************************************************/

// typedef int qtoken

// network functions
int libos_mtcp_queue(int domain, int type, int protocol);
int libos_mtcp_listen(int qd, int backlog);
int libos_mtcp_bind(int qd, struct sockaddr *saddr, socklen_t size);
int libos_mtcp_accept(int qd, struct sockaddr *saddr, socklen_t *size);
int libos_mtcp_connect(int qd, struct sockaddr *saddr, socklen_t size);
int libos_mtcp_close(int qd);
// other functions
int libos_mtcp_push(int qd, zeus_sgarray *sga);
int libos_mtcp_pop(int qd, zeus_sgarray *sga);
ssize_t libost_mtpc_wait(int *qts, size_t num_qts);
ssize_t libos_mtcp_wait_all(int *qts, size_t num_qts);
ssize_t libos_mtcp_blocking_push(int qd, zeus_sgarray *sga);
ssize_t libos_mtcp_blocking_pop(int qd, zeus_sgarray *sga);

/******************************************************************************/
/******************************************************************************/

#ifdef __cplusplus
};
#endif

#endif /* __MTCP_API_H_ */
