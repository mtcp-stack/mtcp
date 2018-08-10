#ifndef __MTCP_API_WRAPPER_H__
#define __MTCP_API_WRAPPER_H__
/*----------------------------------------------------------------------------*/
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#ifndef DISABLE_DPDK
#include "rte_version.h"
#endif
/*----------------------------------------------------------------------------*/
typedef int (* l_socket)(int sock_domain, int sock_type, int sock_protocol);
typedef int (*l_close)(int sock_fd);
typedef int (*l_shutdown)(int sock_fd, int sock_how);
typedef int (*l_fcntl)(int sock_fd, int sock_cmd, ...);
typedef int (*l_ioctl)(int sock_fd, int sock_req, ...);
typedef int (*l_accept)(int sock_fd, struct sockaddr *sock_name,
		socklen_t *sock_namelen);
typedef int (*l_listen)(int sock_fd, int sock_backlog);
typedef int (*l_bind)(int sock_fd, const struct sockaddr * sock_addr,
		socklen_t sock_addrlen);
typedef int (*l_connect)(int sock_fd,  const struct sockaddr* sock_addr,
			socklen_t sock_len);
typedef int (*l_select)(int sock_n, fd_set *sock_readfds, fd_set *sock_writefds,
			fd_set *sock_exceptfds, struct timeval *sock_timeout);
typedef int (*l_poll)(struct pollfd *sock_fds, unsigned long sock_nfds,
			int sock_timeout);
typedef int (*l_getpeername)(int sock_fd, struct sockaddr *sock_name,
			socklen_t *sock_namelen);
typedef int (*l_getsockopt)(int sock_fd, int sock_level, int sock_optname,
			void *sock_optval, socklen_t *sock_optlen);
typedef int (*l_setsockopt)(int sock_fd, int sock_level, int sock_optname,
			const void *sock_optval, socklen_t sock_optlen);
typedef int (*l_getsockname)(int sock_fd, struct sockaddr *sock_name,
			socklen_t *sock_namelen);
typedef ssize_t (*l_sendmsg)(int sock_fd, const struct msghdr *sock_msg,
			int sock_flags);
typedef ssize_t (*l_send)(int sock_fd, const void *sock_buf, size_t sock_len,
			int sock_flags);
typedef ssize_t (*l_sendto)(int sock_fd, const void *sock_buf, size_t sock_len,
	int sock_flags, const struct sockaddr *sock_addr, socklen_t sock_addrlen);
typedef ssize_t (*l_recvfrom)(int sock_fd, void *sock_buf, size_t sock_len,
			int sock_flags, struct sockaddr *sock_from,
			socklen_t *sock_fromlen);
typedef ssize_t (*l_recv)(int sock_fd, void *sock_buf, size_t sock_len, int sock_flags);
typedef ssize_t (*l_recvmsg)(int sock_fd, struct msghdr *sock_l_msg, int sock_flags);
typedef ssize_t (*l_read)(int sock_fd, void *sock_buf, size_t sock_count);
typedef ssize_t (*l_readv)(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt);
typedef ssize_t (*l_write)(int sock_fd, const void *sock_buf, size_t sock_count);
typedef ssize_t (*l_writev)(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt);
typedef int (*l_epoll_create1)(int sock_flags);
typedef int (*l_epoll_create)(int sock_size);
typedef int (*l_epoll_ctl)(int sock_epfd, int sock_op, int sock_fd,
			struct epoll_event *sock_event);
typedef int (*l_epoll_wait)(int sock_epfd, struct epoll_event *sock_events,
			int sock_maxevents, int sock_timeout);
typedef ssize_t (*l_sendfile)(int out_fd, int in_fd, off_t *offset, size_t count);

struct mtcp_lib_funcs
{
	l_socket func_socket;
	l_close func_close;
	l_shutdown func_shutdown;
	l_fcntl func_fcntl;
	l_ioctl func_ioctl;
	l_accept func_accept;
	l_listen func_listen;
	l_bind func_bind;
	l_connect func_connect;
	l_select func_select;
	l_poll func_poll;
	l_getpeername func_getpeername;
	l_getsockopt func_getsockopt;
	l_setsockopt func_setsockopt;
	l_getsockname func_getsockname;
	l_sendmsg func_sendmsg;
	l_send func_send;
	l_sendto func_sendto;
	l_recvfrom func_recvfrom;
	l_recv func_recv;
	l_recvmsg func_recvmsg;
	l_read func_read;
	l_readv func_readv;
	l_write func_write;
	l_writev func_writev;
	l_epoll_create func_epoll_create;
	l_epoll_create1 func_epoll_create1;
	l_epoll_ctl func_epoll_ctl;
	l_epoll_wait func_epoll_wait;
	l_sendfile func_sendfile;
};  /* mtcp_lib_funcs */
/*----------------------------------------------------------------------------*/
extern struct mtcp_lib_funcs mtcp_socket_funcs;

#define MTCP_KERNEL_CALL(name) mtcp_socket_funcs.func_##name
/*----------------------------------------------------------------------------*/
#endif /* __MTCP_API_WRAPPER_H__ */
