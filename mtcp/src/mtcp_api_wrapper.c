/* 
 * When binary application is accelerated, <api_funcs> are used.
 * When mTCP is part of the shared library, <mtcp_api_func>
 * are used.
 *
 * The functions are equivalents of the socket functions 
 * implemenated by glibc.
 */
/*----------------------------------------------------------------------------*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sched.h>
#include <dlfcn.h>
#include "mtcp_api_wrapper.h"
#include "mtcp_stub.h"
#include "mtcp_api.h"
#include "mtcp.h"
#include "debug.h"
/*----------------------------------------------------------------------------*/
#define LIBC_PATH1		"/lib64/libc.so.6"
#define LIBC_PATH2		"/lib/libc.so.6"
int mtcp_max_fds;
__thread int current_core;
/*
 * mTCP library initialization/close routine.
 *   1. try to override libc socket funtions
 *   2. make global structures initialization
 */
struct mtcp_lib_funcs mtcp_socket_funcs;
/*----------------------------------------------------------------------------*/
#define open_libc() {							\
		mtcp_libc_dl_handle = dlopen(LIBC_PATH1, RTLD_LAZY);	\
		if (mtcp_libc_dl_handle == NULL) {			\
			mtcp_libc_dl_handle = dlopen(LIBC_PATH2, RTLD_LAZY); \
			if (mtcp_libc_dl_handle == NULL) {		\
				TRACE_ERROR("%s\n", dlerror());		\
				return;					\
			}						\
		}							\
	}
/*----------------------------------------------------------------------------*/
#define get_addr_of_loaded_symbol(name) {				\
		char *error_str;					\
		mtcp_socket_funcs.func_##name = dlsym(mtcp_libc_dl_handle, #name); \
		if (NULL != (error_str = dlerror())) {			\
			TRACE_ERROR("%s\n", error_str);			\
		}							\
		if (mtcp_socket_funcs.func_##name == NULL) {		\
			mtcp_socket_funcs.func_##name = &mtcp_stub_##name; \
		}							\
	}
/*----------------------------------------------------------------------------*/
#ifdef RTLD_NEXT
static void *mtcp_libc_dl_handle = RTLD_NEXT;
#else
static void *mtcp_libc_dl_handle;
#endif
/*----------------------------------------------------------------------------*/
static inline int
mtcp_get_fd_from_sockid(int sock_id)
{
	return (sock_id + mtcp_max_fds);
}
/*----------------------------------------------------------------------------*/
static inline int
mtcp_get_sockid_from_fd(int fd)
{
	return (fd - mtcp_max_fds);
}
/*----------------------------------------------------------------------------*/
static inline int
not_mtcp_socket_fd(mctx_t mctx, int sock_fd)
{
	int rc;
	rc = (mctx == NULL || g_mtcp[mctx->cpu] == NULL ||
	      sock_fd < 0 || sock_fd < mtcp_max_fds) ?
		1 : 0;
	
	return rc;
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_socket(int sock_domain, int sock_type, int sock_protocol)
{
	int fd;
	struct mtcp_context mctx;
	int stype = (sock_type & ~SOCK_NONBLOCK) & ~SOCK_CLOEXEC;
	
	mctx.cpu = current_core;

	if (g_mtcp[mctx.cpu] != NULL) {
		fd = mtcp_socket(&mctx, sock_domain, stype, sock_protocol);
		if (fd >= 0) {
			mtcp_setsock_nonblock(&mctx, fd);
			return mtcp_get_fd_from_sockid(fd);
		} else
			return fd;
	} else {
		fd = MTCP_KERNEL_CALL(socket)(sock_domain, sock_type, sock_protocol);
		return fd;
	}
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
socket(int sock_domain, int sock_type, int sock_protocol)
{
	return mtcp_wrapper_socket(sock_domain, sock_type, sock_protocol);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_close(int sock_fd)
{
	struct mtcp_context mctx;
	
	mctx.cpu = current_core;	
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(close)(sock_fd);
	
	return mtcp_close(&mctx, mtcp_get_sockid_from_fd(sock_fd));
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
close(int sock_fd)
{
	return mtcp_wrapper_close(sock_fd);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_shutdown(int sock_fd, int sock_how)
{
	struct mtcp_context mctx;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(shutdown)(sock_fd, sock_how);

	return mtcp_shutdown(&mctx, mtcp_get_sockid_from_fd(sock_fd), sock_how);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
shutdown(int sock_fd, int sock_how)
{
	return mtcp_wrapper_shutdown(sock_fd, sock_how);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_fcntl(int sock_fd, int sock_cmd, void *sock_arg)
{
	struct mtcp_context mctx;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(fcntl)(sock_fd, sock_cmd, sock_arg);

	abort();
	/* not supported */
	return -1;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
fcntl(int sock_fd, int sock_cmd, ...)
{
	void *sock_arg;
	va_list libmtcp_ap;
	
	va_start(libmtcp_ap, sock_cmd);
	sock_arg = va_arg(libmtcp_ap, void *);
	va_end(libmtcp_ap);
	
	return mtcp_wrapper_fcntl(sock_fd, sock_cmd, sock_arg);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_ioctl(int sock_fd, unsigned long int sock_req, void *sock_data)
{
	struct mtcp_context mctx;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(ioctl)(sock_fd, sock_req, sock_data);

	return mtcp_socket_ioctl(&mctx, mtcp_get_sockid_from_fd(sock_fd),
				 sock_req, sock_data);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
ioctl(int sock_fd, unsigned long int sock_req, ...)
{
	void *sock_data;
	va_list mtcp_ap;
	
	va_start(mtcp_ap, sock_req);
	sock_data = va_arg(mtcp_ap, void *);
	va_end(mtcp_ap);
	
	return mtcp_wrapper_ioctl(sock_fd, sock_req, sock_data);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_accept(int sock_fd, struct sockaddr *sock_name,
		    socklen_t *sock_namelen)
{
	struct mtcp_context mctx;
	int fd;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(accept)(sock_fd, sock_name,
						sock_namelen);
	
	fd = mtcp_accept(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			 sock_name, sock_namelen);
	if (fd >= 0) {
		mtcp_setsock_nonblock(&mctx, fd);
		return mtcp_get_fd_from_sockid(fd);
	}
	return fd;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
accept(int sock_fd, struct sockaddr *sock_name,
       socklen_t *sock_namelen)
{
	return mtcp_wrapper_accept(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
accept4(int sock_fd, struct sockaddr *sock_name,
	    socklen_t *sock_namelen, int flags)
{
	int on = 1;
	int ret = ioctl(sock_fd, FIONBIO, (char*)&on);
	if (ret == -1) return ret;

	return mtcp_wrapper_accept(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_listen(int sock_fd, int sock_backlog)
{
	struct mtcp_context mctx;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(listen)(sock_fd, sock_backlog);
	
	return mtcp_listen(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			   sock_backlog);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
listen(int sock_fd, int sock_backlog)
{
	int rc;
	rc = mtcp_wrapper_listen(sock_fd, sock_backlog);
	if (rc < 0)
		TRACE_ERROR("Something weird took place!\n\n\n");
       
	return rc;
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_bind(int sock_fd, const struct sockaddr *sock_addr,
		  socklen_t sock_addrlen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(bind)(sock_fd, sock_addr, sock_addrlen);

	return mtcp_bind(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			 sock_addr, sock_addrlen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
bind(int sock_fd, const struct sockaddr *sock_addr,
     socklen_t sock_addrlen)
{
	return mtcp_wrapper_bind(sock_fd, sock_addr, sock_addrlen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_connect(int sock_fd, const struct sockaddr *sock_addr,
		     socklen_t sock_len)
{
	struct mtcp_context mctx;
	int rc;
	
	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(connect)(sock_fd, sock_addr, sock_len);

	rc = mtcp_connect(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			  sock_addr, sock_len);
	if (rc < 0)
		mtcp_setsock_nonblock(&mctx, mtcp_get_sockid_from_fd(sock_fd));

	return rc;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
connect(int sock_fd, const struct sockaddr *sock_addr,
	socklen_t sock_len)
{
	return mtcp_wrapper_connect(sock_fd, sock_addr, sock_len);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_select(int sock_n, fd_set *sock_readfds, fd_set *sock_writefds,
		    fd_set *sock_exceptfds, struct timeval *sock_timeout)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_n))
		return MTCP_KERNEL_CALL(select)(sock_n, sock_readfds,
						sock_writefds, sock_exceptfds,
						sock_timeout);
	/* Not supported */
	abort();
	return -1;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
select(int sock_n, fd_set *sock_readfds, fd_set *sock_writefds,
       fd_set *sock_exceptfds, struct timeval *sock_timeout)
{
	return mtcp_wrapper_select(sock_n, sock_readfds, sock_writefds,
				   sock_exceptfds, sock_timeout);
}
/*----------------------------------------------------------------------------*/
static inline int
mtcp_wrapper_poll(struct pollfd *sock_fds, unsigned long sock_nfds,
		  int sock_timeout)
{
	return MTCP_KERNEL_CALL(poll)(sock_fds, sock_nfds, sock_timeout);
}
/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
int
poll(struct pollfd *sock_fds, unsigned long sock_nfds,
     int sock_timeout)
{
	return mtcp_wrapper_poll(sock_fds, sock_nfds, sock_timeout);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_getpeername(int sock_fd, struct sockaddr *sock_name,
			 socklen_t *sock_namelen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(getpeername)(sock_fd, sock_name, sock_namelen);
	
	return mtcp_getpeername(&mctx, mtcp_get_sockid_from_fd(sock_fd), sock_name,
					sock_namelen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
getpeername(int sock_fd, struct sockaddr *sock_name,
	    socklen_t *sock_namelen)
{
	return mtcp_wrapper_getpeername(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_getsockopt(int sock_fd, int sock_level, int sock_optname,
			void *sock_optval, socklen_t *sock_optlen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(getsockopt)(sock_fd, sock_level,
						    sock_optname, sock_optval,
						    sock_optlen);

	return mtcp_getsockopt(&mctx, mtcp_get_sockid_from_fd(sock_fd), sock_level,
			       sock_optname, sock_optval, sock_optlen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
getsockopt(int sock_fd, int sock_level, int sock_optname,
	   void *sock_optval, socklen_t *sock_optlen)
{
	return mtcp_wrapper_getsockopt(sock_fd, sock_level, sock_optname,
				       sock_optval, sock_optlen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_setsockopt(int sock_fd, int sock_level, int sock_optname,
			const void *sock_optval, socklen_t sock_optlen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(setsockopt)(sock_fd, sock_level,
						    sock_optname,
						    (void *)sock_optval,
						    sock_optlen);

	return mtcp_setsockopt(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			       sock_level, sock_optname, sock_optval,
			       sock_optlen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
setsockopt(int sock_fd, int sock_level, int sock_optname,
	   const void *sock_optval, socklen_t sock_optlen)
{
	return mtcp_wrapper_setsockopt(sock_fd, sock_level, sock_optname,
				       (void *)sock_optval, sock_optlen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_getsockname(int sock_fd, struct sockaddr *sock_name,
			 socklen_t *sock_namelen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(getsockname)(sock_fd, sock_name, sock_namelen);

	return mtcp_getsockname(&mctx, mtcp_get_sockid_from_fd(sock_fd),
				sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
getsockname(int sock_fd, struct sockaddr *sock_name,
	    socklen_t *sock_namelen)
{
	return mtcp_wrapper_getsockname(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_sendmsg(int sock_fd, const struct msghdr *sock_msg,
		     int sock_flags)
{	 
	return MTCP_KERNEL_CALL(sendmsg)(sock_fd, sock_msg, sock_flags);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
sendmsg(int sock_fd, const struct msghdr *sock_msg,
	int sock_flags)
{
	return mtcp_wrapper_sendmsg(sock_fd, sock_msg, sock_flags );
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_send(int sock_fd, void *sock_buf, size_t sock_len,
		  int sock_flags)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(recv)(sock_fd, sock_buf,
					      sock_len, sock_flags);

	return mtcp_write(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			  sock_buf, sock_len);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
send(int sock_fd, const void *sock_buf, size_t sock_len,
     int sock_flags)
{
	return mtcp_wrapper_send(sock_fd, (void *)sock_buf,
				 sock_len, sock_flags);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline
ssize_t mtcp_wrapper_sendto(int sock_fd, const void *sock_buf, size_t sock_len,
		int sock_flags, const struct sockaddr *sock_addr, socklen_t sock_addrlen)
{

	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(sendto)(sock_fd, sock_buf, sock_len,
				sock_flags, sock_addr, sock_addrlen);

	abort();
	/* Not supported yet */
	return -1;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
sendto(int sock_fd, const void *sock_buf, size_t sock_len,
       int sock_flags, const struct sockaddr *sock_addr,
       socklen_t sock_addrlen)
{
	return mtcp_wrapper_sendto(sock_fd, (void *)sock_buf, sock_len,
				   sock_flags, sock_addr, sock_addrlen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_recvfrom(int sock_fd, void *sock_buf, size_t sock_len,
		      int sock_flags, struct sockaddr *sock_from,
		      socklen_t *sock_fromlen)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(recvfrom)(sock_fd, sock_buf, sock_len,
						  sock_flags, sock_from, sock_fromlen);

	abort();
	/* Not supported yet */
	return -1;
}
/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
ssize_t
recvfrom(int sock_fd, void *sock_buf, size_t sock_len,
	 int sock_flags, struct sockaddr *sock_from,
	 socklen_t *sock_fromlen)
{
	return mtcp_wrapper_recvfrom(sock_fd, sock_buf, sock_len, sock_flags,
				     sock_from, sock_fromlen);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_recv(int sock_fd, void *sock_buf, size_t sock_len, int sock_flags)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(recv)(sock_fd, sock_buf,
					      sock_len, sock_flags);
	
	return mtcp_recv(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			 sock_buf, sock_len, sock_flags);
}
/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
ssize_t
recv(int sock_fd, void *sock_buf, size_t sock_len, int sock_flags)
{
	return mtcp_wrapper_recv(sock_fd, sock_buf, sock_len, sock_flags);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_recvmsg(int sock_fd, struct msghdr *sock_l_msg, int sock_flags)
{	 
	return MTCP_KERNEL_CALL(recvmsg)(sock_fd, sock_l_msg, sock_flags);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
recvmsg(int sock_fd, struct msghdr *sock_l_msg, int sock_flags)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return mtcp_wrapper_recvmsg(sock_fd, sock_l_msg, sock_flags);

	abort();
	/*  Not supported */
	return -1;
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_read(int sock_fd, void *sock_buf, size_t sock_count)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(read)(sock_fd, sock_buf, sock_count);

	return mtcp_read(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			 sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
ssize_t
read(int sock_fd, void *sock_buf, size_t sock_count)
{
	return mtcp_wrapper_read(sock_fd, sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_readv(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(readv)(sock_fd, sock_iov, sock_iovcnt);

	return mtcp_readv(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			  (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
readv(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt)
{
	return mtcp_wrapper_readv(sock_fd, (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_write(int sock_fd, const void *sock_buf, size_t sock_count)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(write)(sock_fd, sock_buf, sock_count);

	return mtcp_write(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			  (void *)sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
write(int sock_fd, const void *sock_buf, size_t sock_count)
{
	return mtcp_wrapper_write(sock_fd, (void *)sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline ssize_t
mtcp_wrapper_writev(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(writev)(sock_fd, sock_iov, sock_iovcnt);

	return mtcp_writev(&mctx, mtcp_get_sockid_from_fd(sock_fd),
			   (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
writev(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt)
{
	return mtcp_wrapper_writev(sock_fd, (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_epoll_create1(int sock_flags)
{
	return MTCP_KERNEL_CALL(epoll_create1)(sock_flags);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
epoll_create1(int sock_flags)
{
	return mtcp_wrapper_epoll_create1(sock_flags);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_epoll_create(int sock_size)
{
	struct mtcp_context mctx;
	int epoll_fd;

	mctx.cpu = current_core;
	if (g_mtcp[mctx.cpu] == NULL)
		return MTCP_KERNEL_CALL(epoll_create)(sock_size);
	else {
		g_mtcp[mctx.cpu]->ep_fd = MTCP_KERNEL_CALL(epoll_create)(sock_size);
		if (g_mtcp[mctx.cpu]->ep_fd < 0) {
			abort();
			return -1;
		}
		
		/* suppose all are mtcp epoll_create */
		epoll_fd = mtcp_epoll_create(&mctx, sock_size);
		if (epoll_fd >= 0)
			return mtcp_get_fd_from_sockid(epoll_fd);
	}
		
	return epoll_fd;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
epoll_create(int sock_size)
{
	return mtcp_wrapper_epoll_create(sock_size);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_epoll_ctl(int sock_epfd, int sock_op, int sock_fd,
		       struct epoll_event *sock_event)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_epfd)
	    && not_mtcp_socket_fd(&mctx, sock_fd))
		return MTCP_KERNEL_CALL(epoll_ctl)(sock_epfd, sock_op, sock_fd,
						   sock_event);

	if (!not_mtcp_socket_fd(&mctx, sock_epfd) &&
	    !not_mtcp_socket_fd(&mctx, sock_fd)) {
		return mtcp_epoll_ctl(&mctx, mtcp_get_sockid_from_fd(sock_epfd),
				      sock_op, mtcp_get_sockid_from_fd(sock_fd),
				      (struct mtcp_epoll_event *)sock_event);
	}

	if (!not_mtcp_socket_fd(&mctx, sock_epfd))
		return MTCP_KERNEL_CALL(epoll_ctl)(g_mtcp[mctx.cpu]->ep_fd, sock_op, sock_fd, sock_event);

	/* don't expect */
	TRACE_ERROR("Something weird just happened: sock_epfd: %u, sock_fd: %u!\n",
		    sock_epfd, sock_fd);
	abort();
	return 0;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
epoll_ctl(int sock_epfd, int sock_op, int sock_fd,
	  struct epoll_event *sock_event)
{
	return mtcp_wrapper_epoll_ctl(sock_epfd, sock_op, sock_fd, sock_event);
}
/*----------------------------------------------------------------------------*/
static __attribute__((gnu_inline)) inline int
mtcp_wrapper_epoll_wait(int sock_epfd, struct epoll_event *sock_events,
			int sock_maxevents, int sock_timeout)
{
	struct mtcp_context mctx;
	int nevent;

	mctx.cpu = current_core;
	if (not_mtcp_socket_fd(&mctx, sock_epfd))
		return MTCP_KERNEL_CALL(epoll_wait)(sock_epfd, sock_events,
						    sock_maxevents, sock_timeout);
	
	nevent = mtcp_epoll_wait(&mctx, mtcp_get_sockid_from_fd(sock_epfd),
				 (void *)sock_events, sock_maxevents, sock_timeout);
        
	/* we need to restore the fd in the sock_event data structure */
	if (nevent > 0) {
		int i, fd;
		for (i = 0; i < nevent; i++) {
			fd = mtcp_get_fd_from_sockid(sock_events[i].data.fd);
			(void)fd;
		}
	}

	return nevent;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline int
epoll_wait(int sock_epfd, struct epoll_event *sock_events,
	   int sock_maxevents, int sock_timeout)
{
	return mtcp_wrapper_epoll_wait(sock_epfd, sock_events,
				       sock_maxevents, sock_timeout);
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
sendfile64(int sock_fd, int in_fd, off_t *offset, size_t count)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
	
	if (offset == NULL)
		return -1;
	
	if (not_mtcp_socket_fd(&mctx, sock_fd)) {
		 
		return MTCP_KERNEL_CALL(sendfile)(sock_fd, in_fd, offset, count);
	}

	abort();
	return -1;
}
/*----------------------------------------------------------------------------*/
__attribute__((gnu_inline)) inline ssize_t
sendfile(int sock_fd, int in_fd, off_t *offset, size_t count)
{
	struct mtcp_context mctx;

	mctx.cpu = current_core;
#if 0
	if (offset == NULL)
		return -1;
#endif
	if (not_mtcp_socket_fd(&mctx, sock_fd)) {
		 
		return MTCP_KERNEL_CALL(sendfile)(sock_fd, in_fd, offset, count);
	}

	abort();
	return -1;
}
/*----------------------------------------------------------------------------*/
int
mtcp_app_init()
{
	int ret;
	mctx_t mctx;
	int cpu;
	char *mtcp_config_file, *cpu_str, *endptr;

	ret = 0;

	/* Fetch mTCP config file */
	mtcp_config_file = getenv("MTCP_CONFIG");
	if (mtcp_config_file == NULL) {
		TRACE_ERROR("Error: Missing mtcp configuration file.\n");
		return -1;
	}

	/* init mTCP stack */
	ret = mtcp_init(mtcp_config_file);
	if (ret) {
		TRACE_ERROR("Failed to iniialize mtcp.\n");
		return -1;
	}

	/* Fetch mTCP core id */
	cpu_str = getenv("MTCP_CORE_ID");
	if (cpu_str == NULL)
		cpu = 0;
	else {
		cpu = strtol(cpu_str, &endptr, 10);
		if (cpu >= MAX_CPUS) {
			TRACE_ERROR("CPU core id is invalid.\n");
			return -1;
		}
	}

	mtcp_core_affinitize(cpu);
	mctx = mtcp_create_context(cpu);

	if (!mctx) {
		TRACE_ERROR("Failed to create mtcp context.\n");
		return -1;
	}
	return ret;
}
/*----------------------------------------------------------------------------*/
void
__mtcp_init(void)
{
	
#ifndef RTLD_NEXT
	/*
 	 * open libc for original socket call.
 	 */
	open_libc();
#endif
	/*
 	 * Get the original functions
 	 */
	get_addr_of_loaded_symbol(accept);
	get_addr_of_loaded_symbol(listen);
	get_addr_of_loaded_symbol(getpeername);
	get_addr_of_loaded_symbol(shutdown);
	get_addr_of_loaded_symbol(fcntl);
	get_addr_of_loaded_symbol(socket);
	get_addr_of_loaded_symbol(getsockopt);
	get_addr_of_loaded_symbol(setsockopt);
	get_addr_of_loaded_symbol(bind);
	get_addr_of_loaded_symbol(close);
	get_addr_of_loaded_symbol(getsockname);
	get_addr_of_loaded_symbol(recvfrom);
	get_addr_of_loaded_symbol(send);
	get_addr_of_loaded_symbol(sendto);
	get_addr_of_loaded_symbol(select);
	get_addr_of_loaded_symbol(sendmsg);
	get_addr_of_loaded_symbol(connect);
	get_addr_of_loaded_symbol(poll);
	get_addr_of_loaded_symbol(epoll_create);
	get_addr_of_loaded_symbol(epoll_create1);
	get_addr_of_loaded_symbol(epoll_ctl);
	get_addr_of_loaded_symbol(epoll_wait);
	get_addr_of_loaded_symbol(recvmsg);
	get_addr_of_loaded_symbol(recv);
	/* socket ops */
	get_addr_of_loaded_symbol(read);
	get_addr_of_loaded_symbol(readv);
	get_addr_of_loaded_symbol(write);
	get_addr_of_loaded_symbol(writev);
	get_addr_of_loaded_symbol(ioctl);
} /* __mtcp_init */
/*----------------------------------------------------------------------------*/
void
__mtcp_fini(void)
{
	//	struct mtcp_context mctx;
	//	mctx.cpu = current_core;
	
	//	mtcp_destroy_context(&mctx);
	mtcp_destroy();
#ifndef RTLD_NEXT
	dlclose( mtcp_libc_dl_handle );
#endif
}
/*----------------------------------------------------------------------------*/
void __attribute__ ((constructor)) __mtcp_init(void);
void __attribute__ ((destructor)) __mtcp_fini(void);
/*----------------------------------------------------------------------------*/
