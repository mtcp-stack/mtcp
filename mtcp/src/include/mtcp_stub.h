/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MTCP_STUB_H_
#define __MTCP_STUB_H_
#include <sys/epoll.h>
#include <poll.h>
/*----------------------------------------------------------------------------*/
/* definitions for mtcp socket API stub functions */
int mtcp_stub_socket(int sock_domain, int sock_type, int sock_protocol);
int mtcp_stub_close(int sock_fd);
int mtcp_stub_shutdown(int sock_fd, int sock_how);
int mtcp_stub_fcntl(int sock_fd, int sock_cmd, ...);
int mtcp_stub_ioctl(int sock_fd, int sock_req, ...);
int mtcp_stub_accept(int sock_fd, struct sockaddr *sock_name,
		      socklen_t *sock_namelen);
int mtcp_stub_listen(int sock_fd, int sock_backlog);
int mtcp_stub_bind(int sock_fd, const struct sockaddr * sock_addr,
		socklen_t sock_addrlen);
int mtcp_stub_connect(int sock_fd,  const struct sockaddr* sock_addr,
			socklen_t sock_len);
int mtcp_stub_select(int sock_n, fd_set *sock_readfds, fd_set *sock_writefds,
			fd_set *sock_exceptfds, struct timeval *sock_timeout);
int mtcp_stub_poll(struct pollfd *sock_fds, unsigned long sock_nfds,
			int sock_timeout);
int mtcp_stub_getpeername(int sock_fd, struct sockaddr *sock_name,
			socklen_t *sock_namelen);
int mtcp_stub_getsockopt(int sock_fd, int sock_level, int sock_optname,
			void *sock_optval, socklen_t *sock_optlen);
int mtcp_stub_setsockopt(int sock_fd, int sock_level, int sock_optname,
			const void *sock_optval, socklen_t sock_optlen);
int mtcp_stub_getsockname(int sock_fd, struct sockaddr *sock_name,
			socklen_t *sock_namelen);
ssize_t mtcp_stub_sendmsg(int sock_fd, const struct msghdr *sock_msg,
			int sock_flags);
ssize_t mtcp_stub_send(int sock_fd, const void *sock_buf, size_t sock_len,
			int sock_flags);
ssize_t mtcp_stub_sendto(int sock_fd, const void *sock_buf, size_t sock_len,
	int sock_flags, const struct sockaddr *sock_addr, socklen_t sock_addrlen);
ssize_t mtcp_stub_recvfrom(int sock_fd, void *sock_buf, size_t sock_len,
			int sock_flags, struct sockaddr *sock_from,
			socklen_t *sock_fromlen);
ssize_t mtcp_stub_recv(int sock_fd, void *sock_buf, size_t sock_len, int sock_flags);
ssize_t mtcp_stub_recvmsg(int sock_fd, struct msghdr *sock_l_msg, int sock_flags);
ssize_t mtcp_stub_read(int sock_fd, void *sock_buf, size_t sock_count);
ssize_t mtcp_stub_readv(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt);
ssize_t mtcp_stub_write(int sock_fd, const void *sock_buf, size_t sock_count);
ssize_t mtcp_stub_writev(int sock_fd, const struct iovec *sock_iov, int sock_iovcnt);
int mtcp_stub_epoll_create1(int sock_flags);
int mtcp_stub_epoll_create(int sock_size);
int mtcp_stub_epoll_ctl(int sock_epfd, int sock_op, int sock_fd,
			struct epoll_event *sock_event);
int mtcp_stub_epoll_wait(int sock_epfd, struct epoll_event *sock_events,
			int sock_maxevents, int sock_timeout);
/*----------------------------------------------------------------------------*/
#endif /* !__MTCP_STUB_H_ */
