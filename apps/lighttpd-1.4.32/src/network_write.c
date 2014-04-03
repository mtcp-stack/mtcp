#include "network_backends.h"

#include "network.h"
#include "fdevent.h"
#include "log.h"
#include "stat_cache.h"

#include "sys-socket.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

int network_write_chunkqueue_write(server *srv, connection *con, int fd, chunkqueue *cq, off_t max_bytes) {
	chunk *c;

	for(c = cq->first; (max_bytes > 0) && (NULL != c); c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			off_t toSend;
			ssize_t r;

			if (c->mem->used == 0) {
				chunk_finished = 1;
				break;
			}

			offset = c->mem->ptr + c->offset;
			toSend = c->mem->used - 1 - c->offset;
			if (toSend > max_bytes) toSend = max_bytes;

#ifdef __WIN32
			if ((r = send(fd, offset, toSend, 0)) < 0) {
				/* no error handling for windows... */
				log_error_write(srv, __FILE__, __LINE__, "ssd", "send failed: ", strerror(errno), fd);

				return -1;
			}
#else
			if ((r = write(fd, offset, toSend)) < 0) {
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
						"write failed:", strerror(errno), fd);

					return -1;
				}
			}
#endif

			c->offset += r;
			cq->bytes_out += r;
			max_bytes -= r;

			if (c->offset == (off_t)c->mem->used - 1) {
				chunk_finished = 1;
			}

			break;
		}
		case FILE_CHUNK: {
#ifdef USE_MMAP
			char *p = NULL;
#endif
			ssize_t r;
			off_t offset;
			off_t toSend;
			stat_cache_entry *sce = NULL;
			int ifd;

			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			offset = c->file.start + c->offset;
			toSend = c->file.length - c->offset;

			if (toSend > max_bytes) toSend = max_bytes;

			if (offset > sce->st.st_size) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "file was shrinked:", c->file.name);

				return -1;
			}

			if (-1 == (ifd = open(c->file.name->ptr, O_RDONLY))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "open failed: ", strerror(errno));

				return -1;
			}

#ifdef USE_MMAP
			if (MAP_FAILED == (p = mmap(0, sce->st.st_size, PROT_READ, MAP_SHARED, ifd, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "mmap failed: ", strerror(errno));

				close(ifd);

				return -1;
			}
			close(ifd);

			if ((r = write(fd, p + offset, toSend)) <= 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					r = 0;
					break;
				case EPIPE:
				case ECONNRESET:
					munmap(p, sce->st.st_size);
					return -2;
				default:
					log_error_write(srv, __FILE__, __LINE__, "ssd",
						"write failed:", strerror(errno), fd);
					munmap(p, sce->st.st_size);

					return -1;
				}
			}

			munmap(p, sce->st.st_size);
#else /* USE_MMAP */
			buffer_prepare_copy(srv->tmp_buf, toSend);

			lseek(ifd, offset, SEEK_SET);
			if (-1 == (toSend = read(ifd, srv->tmp_buf->ptr, toSend))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "read: ", strerror(errno));
				close(ifd);

				return -1;
			}
			close(ifd);

#ifdef __WIN32
			if ((r = send(fd, srv->tmp_buf->ptr, toSend, 0)) < 0) {
				/* no error handling for windows... */
				log_error_write(srv, __FILE__, __LINE__, "ssd", "send failed: ", strerror(errno), fd);

				return -1;
			}
#else /* __WIN32 */
			if ((r = write(fd, srv->tmp_buf->ptr, toSend)) < 0) {
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
						"write failed:", strerror(errno), fd);

					return -1;
				}
			}
#endif /* __WIN32 */
#endif /* USE_MMAP */

			c->offset += r;
			cq->bytes_out += r;
			max_bytes -= r;

			if (c->offset == c->file.length) {
				chunk_finished = 1;
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

#if 0
network_write_init(void) {
	p->write = network_write_write_chunkset;
}
#endif
