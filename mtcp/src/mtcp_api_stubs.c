/* 
 * file contains API stubs to all socket API functions
 */
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/uio.h>
/* 
 * pointers to this functions will be used when __sock_func table
 * will be not initialized 
 */
/*----------------------------------------------------------------------------*/
int
mtcp_stub_socket(int sock_domain, int sock_type, int sock_protocol)
{
	return socket(sock_domain, sock_type, sock_protocol);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_close(int sock_fd)
{
	return close(sock_fd);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_shutdown(int sock_fd, int sock_how)
{
	return shutdown(sock_fd, sock_how);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_fcntl(int sock_fd, int sock_cmd, ...)
{
	void *sock_arg;
	va_list mtcp_stub_ap;
	
	va_start(mtcp_stub_ap, sock_cmd);
	sock_arg = va_arg(mtcp_stub_ap, void *);
	va_end(mtcp_stub_ap);

	return fcntl(sock_fd, sock_cmd, sock_arg);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_ioctl(int sock_fd, int sock_req, ...)
{
	void *sock_data;
	va_list mtcp_stub_ap;

	va_start(mtcp_stub_ap, sock_req);
	sock_data = va_arg(mtcp_stub_ap, void *);
	va_end(mtcp_stub_ap);
	
	return ioctl(sock_fd, sock_req, sock_data);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_accept(int sock_fd, struct sockaddr *sock_name,
		 socklen_t *sock_namelen)
{
	return accept(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_listen(int sock_fd, int sock_backlog)
{
	return listen(sock_fd, sock_backlog);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_bind(int sock_fd, const struct sockaddr *sock_addr,
	       socklen_t sock_addrlen)
{
	return bind(sock_fd, sock_addr, sock_addrlen);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_connect(int sock_fd,  const struct sockaddr *sock_addr,
		  socklen_t sock_len)
{
	return connect(sock_fd, sock_addr, sock_len);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_select(int sock_n, fd_set *sock_readfds, fd_set *sock_writefds,
		 fd_set *sock_exceptfds, struct timeval *sock_timeout)
{
	return select(sock_n, sock_readfds, sock_writefds,
		      sock_exceptfds, sock_timeout);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_poll(struct pollfd *sock_fds, unsigned long sock_nfds,
	       int sock_timeout)
{
	return poll(sock_fds, sock_nfds, sock_timeout);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_getpeername(int sock_fd, struct sockaddr *sock_name,
		      socklen_t *sock_namelen)
{
	return getpeername(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_getsockopt(int sock_fd, int sock_level, int sock_optname,
		     void *sock_optval, socklen_t *sock_optlen)
{
	return getsockopt(sock_fd, sock_level, sock_optname,
			  sock_optval, sock_optlen);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_setsockopt(int sock_fd, int sock_level, int sock_optname,
		     const void *sock_optval, socklen_t sock_optlen)
{
	return setsockopt(sock_fd, sock_level, sock_optname,
			  (void *)sock_optval, sock_optlen);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_getsockname(int sock_fd, struct sockaddr *sock_name,
		      socklen_t *sock_namelen)
{
	return getsockname(sock_fd, sock_name, sock_namelen);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_sendmsg(int sock_fd, const struct msghdr *sock_msg,
		  int sock_flags)
{
	return sendmsg(sock_fd, sock_msg, sock_flags );
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_send(int sock_fd, const void *sock_buf, size_t sock_len,
	       int sock_flags)
{
	return send(sock_fd, (void *)sock_buf, sock_len, sock_flags );
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_sendto(int sock_fd, const void *sock_buf, size_t sock_len,
		 int sock_flags, const struct sockaddr *sock_addr,
		 socklen_t sock_addrlen)
{
	return sendto(sock_fd, (void *)sock_buf, sock_len, sock_flags,
		      sock_addr, sock_addrlen);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_recvfrom(int sock_fd, void *sock_buf, size_t sock_len,
		   int sock_flags, struct sockaddr *sock_from,
		   socklen_t *sock_fromlen)
{
	return recvfrom(sock_fd, sock_buf, sock_len, sock_flags,
			sock_from, sock_fromlen);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_recv(int sock_fd, void *sock_buf, size_t sock_len, int sock_flags)
{
	return recv(sock_fd, sock_buf, sock_len, sock_flags);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_recvmsg(int sock_fd, struct msghdr *sock_l_msg, int sock_flags)
{
	return recvmsg(sock_fd, sock_l_msg, sock_flags);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_read(int sock_fd, void *sock_buf, size_t sock_count)
{
	return read(sock_fd, sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_readv(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt)
{
	return readv(sock_fd, (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_write(int sock_fd, const void *sock_buf, size_t sock_count)
{
	return write(sock_fd, (void *)sock_buf, sock_count);
}
/*----------------------------------------------------------------------------*/
ssize_t
mtcp_stub_writev(int sock_fd, const struct iovec *sock_iov,
		 int sock_iovcnt)
{
	return writev(sock_fd, (struct iovec *)sock_iov, sock_iovcnt);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_epoll_create1(int sock_flags)
{
	return epoll_create1(sock_flags);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_epoll_create(int sock_size)
{
	return epoll_create(sock_size);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_epoll_ctl(int sock_epfd, int sock_op, int sock_fd,
		    struct epoll_event *sock_event)
{
	return epoll_ctl(sock_epfd, sock_op, sock_fd, sock_event);
}
/*----------------------------------------------------------------------------*/
int
mtcp_stub_epoll_wait(int sock_epfd, struct epoll_event *sock_events,
		     int sock_maxevents, int sock_timeout)
{
	return epoll_wait(sock_epfd, sock_events, sock_maxevents,
			  sock_timeout);
}
/*----------------------------------------------------------------------------*/
