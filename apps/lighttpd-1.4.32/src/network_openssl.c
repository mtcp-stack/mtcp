#include "network_backends.h"

#ifdef USE_OPENSSL

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
#include <assert.h>

# include <openssl/ssl.h>
# include <openssl/err.h>

int network_write_chunkqueue_openssl(server *srv, connection *con, SSL *ssl, chunkqueue *cq, off_t max_bytes) {
	int ssl_r;
	chunk *c;

	/* this is a 64k sendbuffer
	 *
	 * it has to stay at the same location all the time to satisfy the needs
	 * of SSL_write to pass the SAME parameter in case of a _WANT_WRITE
	 *
	 * the buffer is allocated once, is NOT realloced and is NOT freed at shutdown
	 * -> we expect a 64k block to 'leak' in valgrind
	 *
	 *
	 * In reality we would like to use mmap() but we don't have a guarantee that
	 * we get the same mmap() address for each call. On openbsd the mmap() address
	 * even randomized.
	 *   That means either we keep the mmap() open or we do a read() into a
	 * constant buffer
	 * */
#define LOCAL_SEND_BUFSIZE (64 * 1024)
	static char *local_send_buffer = NULL;

	/* the remote side closed the connection before without shutdown request
	 * - IE
	 * - wget
	 * if keep-alive is disabled */

	if (con->keep_alive == 0) {
		SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
	}

	for(c = cq->first; (max_bytes > 0) && (NULL != c); c = c->next) {
		int chunk_finished = 0;

		switch(c->type) {
		case MEM_CHUNK: {
			char * offset;
			off_t toSend;
			ssize_t r;

			if (c->mem->used == 0 || c->mem->used == 1) {
				chunk_finished = 1;
				break;
			}

			offset = c->mem->ptr + c->offset;
			toSend = c->mem->used - 1 - c->offset;
			if (toSend > max_bytes) toSend = max_bytes;

			/**
			 * SSL_write man-page
			 *
			 * WARNING
			 *        When an SSL_write() operation has to be repeated because of
			 *        SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, it must be
			 *        repeated with the same arguments.
			 *
			 */

			ERR_clear_error();
			r = SSL_write(ssl, offset, toSend);

			if (con->renegotiations > 1 && con->conf.ssl_disable_client_renegotiation) {
				log_error_write(srv, __FILE__, __LINE__, "s", "SSL: renegotiation initiated by client");
				return -1;
			}

			if (r <= 0) {
				unsigned long err;

				switch ((ssl_r = SSL_get_error(ssl, r))) {
				case SSL_ERROR_WANT_WRITE:
					break;
				case SSL_ERROR_SYSCALL:
					/* perhaps we have error waiting in our error-queue */
					if (0 != (err = ERR_get_error())) {
						do {
							log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
									ssl_r, r,
									ERR_error_string(err, NULL));
						} while((err = ERR_get_error()));
					} else if (r == -1) {
						/* no, but we have errno */
						switch(errno) {
						case EPIPE:
						case ECONNRESET:
							return -2;
						default:
							log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL:",
									ssl_r, r, errno,
									strerror(errno));
							break;
						}
					} else {
						/* neither error-queue nor errno ? */
						log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL (error):",
								ssl_r, r, errno,
								strerror(errno));
					}

					return  -1;
				case SSL_ERROR_ZERO_RETURN:
					/* clean shutdown on the remote side */

					if (r == 0) return -2;

					/* fall through */
				default:
					while((err = ERR_get_error())) {
						log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
								ssl_r, r,
								ERR_error_string(err, NULL));
					}

					return  -1;
				}
			} else {
				c->offset += r;
				cq->bytes_out += r;
				max_bytes -= r;
			}

			if (c->offset == (off_t)c->mem->used - 1) {
				chunk_finished = 1;
			}

			break;
		}
		case FILE_CHUNK: {
			char *s;
			ssize_t r;
			stat_cache_entry *sce = NULL;
			int ifd;
			int write_wait = 0;

			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, c->file.name, &sce)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						strerror(errno), c->file.name);
				return -1;
			}

			if (NULL == local_send_buffer) {
				local_send_buffer = malloc(LOCAL_SEND_BUFSIZE);
				assert(local_send_buffer);
			}

			do {
				off_t offset = c->file.start + c->offset;
				off_t toSend = c->file.length - c->offset;
				if (toSend > max_bytes) toSend = max_bytes;

				if (toSend > LOCAL_SEND_BUFSIZE) toSend = LOCAL_SEND_BUFSIZE;

				if (-1 == (ifd = open(c->file.name->ptr, O_RDONLY))) {
					log_error_write(srv, __FILE__, __LINE__, "ss", "open failed:", strerror(errno));

					return -1;
				}


				lseek(ifd, offset, SEEK_SET);
				if (-1 == (toSend = read(ifd, local_send_buffer, toSend))) {
					close(ifd);
					log_error_write(srv, __FILE__, __LINE__, "ss", "read failed:", strerror(errno));
					return -1;
				}

				s = local_send_buffer;

				close(ifd);

				ERR_clear_error();
				r = SSL_write(ssl, s, toSend);

				if (con->renegotiations > 1 && con->conf.ssl_disable_client_renegotiation) {
					log_error_write(srv, __FILE__, __LINE__, "s", "SSL: renegotiation initiated by client");
					return -1;
				}

				if (r <= 0) {
					unsigned long err;

					switch ((ssl_r = SSL_get_error(ssl, r))) {
					case SSL_ERROR_WANT_WRITE:
						write_wait = 1;
						break;
					case SSL_ERROR_SYSCALL:
						/* perhaps we have error waiting in our error-queue */
						if (0 != (err = ERR_get_error())) {
							do {
								log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
										ssl_r, r,
										ERR_error_string(err, NULL));
							} while((err = ERR_get_error()));
						} else if (r == -1) {
							/* no, but we have errno */
							switch(errno) {
							case EPIPE:
							case ECONNRESET:
								return -2;
							default:
								log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL:",
										ssl_r, r, errno,
										strerror(errno));
								break;
							}
						} else {
							/* neither error-queue nor errno ? */
							log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL (error):",
									ssl_r, r, errno,
									strerror(errno));
						}

						return  -1;
					case SSL_ERROR_ZERO_RETURN:
						/* clean shutdown on the remote side */

						if (r == 0)  return -2;

						/* fall thourgh */
					default:
						while((err = ERR_get_error())) {
							log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
									ssl_r, r,
									ERR_error_string(err, NULL));
						}

						return -1;
					}
				} else {
					c->offset += r;
					cq->bytes_out += r;
					max_bytes -= r;
				}

				if (c->offset == c->file.length) {
					chunk_finished = 1;
				}
			} while (!chunk_finished && !write_wait && max_bytes > 0);

			break;
		}
		default:
			log_error_write(srv, __FILE__, __LINE__, "s", "type not known");

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
network_openssl_init(void) {
	p->write_ssl = network_openssl_write_chunkset;
}
#endif
