#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <assert.h>
#include <stdbool.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>
#include "cpu.h"
#include "rss.h"
#include "http_parsing.h"
#include "debug.h"

#define MAX_CPUS 		16

#define MAX_URL_LEN 		128
#define MAX_FILE_LEN 		128
#define HTTP_HEADER_LEN 	1024

#define IP_RANGE 		1
#define MAX_IP_STR_LEN 		16

#define BUF_SIZE 		(32 * 1024)

#define CALC_MD5SUM 		FALSE

#define TIMEVAL_TO_MSEC(t)      ((t.tv_sec * 1000) + (t.tv_usec / 1000))
#define TIMEVAL_TO_USEC(t)      ((t.tv_sec * 1000000) + (t.tv_usec))
#define TS_GT(a,b)              ((int64_t)((a)-(b)) > 0)

#ifndef TRUE
#define TRUE			(1)
#endif

#ifndef FALSE
#define FALSE			(0)
#endif

#define CONCURRENCY		1
#define BUF_LEN 		8192
#define MAX_FLOW_NUM 		(10000)
#define MAX_EVENTS 		(30000)

#define DEBUG(fmt, args...)	fprintf(stderr, "[DEBUG] " fmt "\n", ## args)
#define ERROR(fmt, args...)	fprintf(stderr, fmt "\n", ## args)
#define SAMPLE(fmt, args...)	fprintf(stdout, fmt "\n", ## args)

#define SEND_MODE 		1
#define WAIT_MODE		2
/*----------------------------------------------------------------------------*/
struct thread_context
{
	int core;
	mctx_t mctx;
};
/*----------------------------------------------------------------------------*/
void
SignalHandler(int signum)
{
	ERROR("Received SIGINT");
	exit(-1);
}
/*----------------------------------------------------------------------------*/
void
print_usage(int mode)
{
	if (mode == SEND_MODE || mode == 0) {
		ERROR("(client initiates)   usage: ./client send [ip] [port] [length (seconds)]");
	}
	if (mode == WAIT_MODE || mode == 0) {
		ERROR("(server initiates)   usage: ./client wait [length (seconds)]");
	}
}
/*----------------------------------------------------------------------------*/
int
main(int argc, char **argv) 
{
	int ret, i, c;

	//mtcp
	mctx_t mctx;
	struct mtcp_conf mcfg;
	struct thread_context *ctx;
	struct mtcp_epoll_event *events;
	struct mtcp_epoll_event ev;
	int core = 0;
	int ep_id;

	// sockets
	struct sockaddr_in saddr, daddr;
	int sockfd;
	int backlog = 3;

	// counters
	int sec_to_send;
	int wrote        = 0,
		read         = 0,
		bytes_sent   = 0,
		events_ready = 0,
		nevents      = 0,
		sent_close   = 0;

	// time
	double elapsed_time = 0.0;
	struct timeval t1, t2;
	struct timespec ts_start, now;
	time_t end_time;

	// send buffer
	char buf[BUF_LEN];
	char rcvbuf[BUF_LEN];

	// args
	int mode = 0;

	if (argc < 2) {
		print_usage(0);
		return -1;
	}

	if (strncmp(argv[1], "send", 4) == 0) {   
		if (argc < 5) { 
			print_usage(SEND_MODE);
			return -1;
		}

		mode = SEND_MODE;
		DEBUG("Send mode");

		// Parse command-line args 
		daddr.sin_family = AF_INET;
		daddr.sin_addr.s_addr = inet_addr(argv[2]);;
		daddr.sin_port = htons(atoi(argv[3]));
		sec_to_send = atoi(argv[4]);

	} else if (strncmp(argv[1], "wait", 4) == 0) {
		if (argc < 4) {
			print_usage(WAIT_MODE);
			return -1;
		}
		
		mode = WAIT_MODE;
		DEBUG("Wait mode");
		
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = inet_addr(argv[2]);
		saddr.sin_port = htons(atoi(argv[3]));
		sec_to_send = atoi(argv[4]);
		
	} else {
		ERROR("Unknown mode \"%s\"", argv[1]);
		print_usage(0);
	}
	
	if (mode == 0) {
		return -1;
	}


	// This must be done before mtcp_init
	mtcp_getconf(&mcfg);
	mcfg.num_cores = 1;
	mtcp_setconf(&mcfg);
	// Seed RNG
	srand(time(NULL));

	// Init mtcp
	DEBUG("Initializing mtcp...\n");
	if (mtcp_init("client.conf")) {
		ERROR("Failed to initialize mtcp.\n");
		return -1;
	}

	// Default simple config, this must be done after mtcp_init
	mtcp_getconf(&mcfg);
	mcfg.max_concurrency = 3 * CONCURRENCY;
	mcfg.max_num_buffers = 3 * CONCURRENCY;
	mtcp_setconf(&mcfg);

	// Catch ctrl+c to clean up
	mtcp_register_signal(SIGINT, SignalHandler);

	DEBUG("Creating thread context...");
	mtcp_core_affinitize(core);
	ctx = (struct thread_context *) calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		ERROR("Failed to create context.");
		perror("calloc");
		return -1;
	}
	ctx->core = core;
	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) {
		ERROR("Failed to create mtcp context.");
		return -1;
	}
	mctx = ctx->mctx;

	if (mode == SEND_MODE) {
		// Create pool of TCP source ports for outgoing conns
		DEBUG("Creating pool of TCP source ports...");
		mtcp_init_rss(mctx, INADDR_ANY, IP_RANGE, daddr.sin_addr.s_addr, daddr.sin_port);
	}

	DEBUG("Creating epoller...");
	ep_id = mtcp_epoll_create(ctx->mctx, mcfg.max_num_buffers);
	events = (struct mtcp_epoll_event *) calloc(mcfg.max_num_buffers, sizeof(struct mtcp_epoll_event));
	if (!events) {
		ERROR("Failed to allocate events.");
		return -1;
	}


	DEBUG("Creating socket...");
	sockfd = mtcp_socket(mctx, AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		ERROR("Failed to create socket.");
		return -1;
	}

	ret = mtcp_setsock_nonblock(mctx, sockfd);
	if (ret < 0) {
		ERROR("Failed to set socket in nonblocking mode.");
		return -1;
	}

	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = sockfd;
	mtcp_epoll_ctl(mctx, ep_id, MTCP_EPOLL_CTL_ADD, sockfd, &ev);

	if (mode == WAIT_MODE) {
		ret = mtcp_bind(mctx, sockfd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
		if (ret < 0) {
			ERROR("Failed to bind to the listening socket.");
		}

		ret = mtcp_listen(mctx, sockfd, backlog);
		if (ret < 0) {
			ERROR("Failed to listen: %s", strerror(errno));
		}

		while (1) { // loop until connected to a server, break when we can send
			nevents = mtcp_epoll_wait(mctx, ep_id, events, MAX_EVENTS, -1);
			if (nevents < 0) {
				if (errno != EINTR) {
					perror("mtcp_epoll_wait");
				}
				return -1;
			}

			for (i = 0; i < nevents; i++) {
				if (events[i].data.sockid == sockfd) {
					c = mtcp_accept(mctx, sockfd, NULL, NULL);
					if (c >= 0) {
						if (c >= MAX_FLOW_NUM) {
							ERROR("Invalid socket id %d.", c);
						}
						DEBUG("Accepted new connection");
					} else {
						ERROR("mtcp_accept() %s", strerror(errno));
					}
					mtcp_epoll_ctl(mctx, ep_id, MTCP_EPOLL_CTL_DEL, sockfd, &ev);
					sockfd = c;
					ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
					ev.data.sockid = sockfd;
					mtcp_epoll_ctl(mctx, ep_id, MTCP_EPOLL_CTL_ADD, sockfd, &ev);
					goto end_wait_loop;
				} else {
					ERROR("Received event on unknown socket.");
				}
			}
		}
	}
end_wait_loop:


	if (mode == SEND_MODE) {
		DEBUG("Connecting socket...");
		ret = mtcp_connect(mctx, sockfd, (struct sockaddr *)&daddr, sizeof(struct sockaddr_in));
		if (ret < 0) {
			ERROR("mtcp_connect failed.");
			if (errno != EINPROGRESS) {
				perror("mtcp_connect");
				mtcp_close(mctx, sockfd);
				return -1;
			}
		}
		DEBUG("Connection created.");
	}

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	end_time = ts_start.tv_sec + sec_to_send;

	memset(buf, 0x90, sizeof(char) * BUF_LEN);
	buf[BUF_LEN-1] = '\0';

	while (1) {
		wrote = mtcp_write(ctx->mctx, sockfd, buf, BUF_LEN);
		bytes_sent += wrote;
		if (wrote > 0) {
			gettimeofday(&t1, NULL);
			break;
		}
	}

	//ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
	//mtcp_epoll_ctl(mctx, ep_id, MTCP_EPOLL_CTL_ADD, sockfd, &ev);
	//

    
	while (1) { // check time
		events_ready = mtcp_epoll_wait(ctx->mctx, ep_id, events, mcfg.max_num_buffers, -1);
		for (int i = 0; i < events_ready; i++) {
			assert(sockfd == events[i].data.sockid);
			if (events[i].events & MTCP_EPOLLIN) {
				read = mtcp_read(ctx->mctx, sockfd, rcvbuf, BUF_LEN);
				if (read <= 0) {
					continue;
				} else {
					DEBUG("Got FIN-ACK from receiver (%d bytes): %s", read, rcvbuf);
					goto stop_timer; 
				}
			} else if (events[i].events == MTCP_EPOLLOUT) {
				//if (bytes_sent < sec_to_send) {
				clock_gettime(CLOCK_MONOTONIC, &now);
				if (now.tv_sec < end_time) {
					wrote = mtcp_write(ctx->mctx, sockfd, buf, BUF_LEN);
					bytes_sent += wrote;
					//DEBUG("wrote %d, total %d", wrote, bytes_sent);
				} else if (!sent_close) {
					memset(buf, 0x96, sizeof(char) * BUF_LEN);
					mtcp_write(ctx->mctx, sockfd, buf, 1);
					DEBUG("Done writing... waiting for FIN-ACK");  
					sent_close = 1;
				}
			}
		}
	}


stop_timer:
	gettimeofday(&t2, NULL);

	DEBUG("Done reading. Closing socket...");
	mtcp_close(mctx, sockfd);
	DEBUG("Socket closed.");

	printf("\n\n");
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1.0;
	elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000000.0;
	printf("Time elapsed: %f\n", elapsed_time);
	printf("Total bytes sent: %d\n", bytes_sent);
	printf("Throughput: %.3fMbit/sec\n", ((bytes_sent * 8.0 / 1000000.0) / elapsed_time));

	mtcp_destroy_context(ctx->mctx);
	free(ctx);
	mtcp_destroy();

	return 0;
}
