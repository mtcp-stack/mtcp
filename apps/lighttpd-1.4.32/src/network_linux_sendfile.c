#include "network_backends.h"

#ifdef USE_LINUX_SENDFILE

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

/* on linux 2.4.29 + debian/ubuntu we have crashes if this is enabled */
#undef HAVE_POSIX_FADVISE

int network_write_chunkqueue_linuxsendfile(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	chunk *c;

	for(c = cq->first; (max_bytes > 0) && (NULL != c); c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			off_t toSend;
			ssize_t r;

			size_t num_chunks, i;
			struct iovec chunks[UIO_MAXIOV];
			chunk *tc;
			size_t num_bytes = 0;

			/* build writev list
			 *
			 * 1. limit: num_chunks < UIO_MAXIOV
			 * 2. limit: num_bytes < max_bytes
			 */
			for (num_chunks = 0, tc = c;
			     tc && tc->type == MEM_CHUNK && num_chunks < UIO_MAXIOV;
			     tc = tc->next, num_chunks++);

			for (tc = c, i = 0; i < num_chunks; tc = tc->next, i++) {
				if (tc->mem->used == 0) {
					chunks[i].iov_base = tc->mem->ptr;
					chunks[i].iov_len  = 0;
				} else {
					offset = tc->mem->ptr + tc->offset;
					toSend = tc->mem->used - 1 - tc->offset;

					chunks[i].iov_base = offset;

					/* protect the return value of writev() */
					if (toSend > max_bytes ||
					    (off_t) num_bytes + toSend > max_bytes) {
						chunks[i].iov_len = max_bytes - num_bytes;

						num_chunks = i + 1;
						break;
					} else {
						chunks[i].iov_len = toSend;
					}

					num_bytes += toSend;
				}
			}

			if ((r = writev(fd, chunks, num_chunks)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"writev failed:", strerror(errno), fd);

					return -1;
				}
			}

			/* check which chunks have been written */
			cq->bytes_out += r;
			max_bytes -= r;

			for(i = 0, tc = c; i < num_chunks; i++, tc = tc->next) {
				if (r >= (ssize_t)chunks[i].iov_len) {
					/* written */
					r -= chunks[i].iov_len;
					tc->offset += chunks[i].iov_len;

					if (chunk_finished) {
						/* skip the chunks from further touches */
						c = c->next;
					} else {
						/* chunks_written + c = c->next is done in the for()*/
						chunk_finished = 1;
					}
				} else {
					/* partially written */

					tc->offset += r;
					chunk_finished = 0;

					break;
				}
			}

			break;
		}
		case FILE_CHUNK: {
			ssize_t r;
			off_t offset;
			off_t toSend;
			stat_cache_entry *sce = NULL;

			offset = c->file.start + c->offset;
			toSend = c->file.length - c->offset;
			if (toSend > max_bytes) toSend = max_bytes;

			/* open file if not already opened */
			if (-1 == c->file.fd) {
				if (-1 == (c->file.fd = open(c->file.name->ptr, O_RDONLY))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

					return -1;
				}
#ifdef FD_CLOEXEC
				fcntl(c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
#ifdef HAVE_POSIX_FADVISE
				/* tell the kernel that we want to stream the file */
				if (-1 == posix_fadvise(c->file.fd, 0, 0, POSIX_FADV_SEQUENTIAL)) {
					if (ENOSYS != errno) {
						log_error_write(srv, __FILE__, __LINE__, "ssd",
							"posix_fadvise failed:", strerror(errno), c->file.fd);
					}
				}
#endif
			}

			if (-1 == (r = sendfile(fd, c->file.fd, &offset, toSend))) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					/* ok, we can't send more, let's try later again */
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
							"sendfile failed:", strerror(errno), fd);
					return -1;
				}
			} else if (r == 0) {
				int oerrno = errno;
				/* We got an event to write but we wrote nothing
				 *
				 * - the file shrinked -> error
				 * - the remote side closed inbetween -> remote-close */

				if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
					/* file is gone ? */
					return -1;
				}

				if (offset > sce->st.st_size) {
					/* file shrinked, close the connection */
					errno = oerrno;

					return -1;
				}

				errno = oerrno;
				return -2;
			}

#ifdef HAVE_POSIX_FADVISE
#if 0
#define K * 1024
#define M * 1024 K
#define READ_AHEAD 4 M
			/* check if we need a new chunk */
			if ((c->offset & ~(READ_AHEAD - 1)) != ((c->offset + r) & ~(READ_AHEAD - 1))) {
				/* tell the kernel that we want to stream the file */
				if (-1 == posix_fadvise(c->file.fd, (c->offset + r) & ~(READ_AHEAD - 1), READ_AHEAD, POSIX_FADV_NOREUSE)) {
					log_error_write(srv, __FILE__, __LINE__, "ssd",
						"posix_fadvise failed:", strerror(errno), c->file.fd);
				}
			}
#endif
#endif

			c->offset += r;
			cq->bytes_out += r;
			max_bytes -= r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;

				/* chunk_free() / chunk_reset() will cleanup for us but it is a ok to be faster :) */

				if (c->file.fd != -1) {
					close(c->file.fd);
					c->file.fd = -1;
				}
			}

			break;
		}
		default:

			log_error_write(srv, __FILE__, __LINE__, "ds", c, "type not known");

			return -1;
		}

		if (!chunk_finished) {
			/* not finished yet */

			break;
		}
	}

	return 0;
}

#endif
#if 0
network_linuxsendfile_init(void) {
	p->write = network_linuxsendfile_write_chunkset;
}
#endif
