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
#include <limits.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>
#include "cpu.h"
#include "rss.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"

#define MAX_URL_LEN 128
#define MAX_FILE_LEN 128
#define HTTP_HEADER_LEN 1024

#define IP_RANGE 1
#define MAX_IP_STR_LEN 16

#define BUF_SIZE (8*1024)

#define CALC_MD5SUM FALSE

#define TIMEVAL_TO_MSEC(t)		((t.tv_sec * 1000) + (t.tv_usec / 1000))
#define TIMEVAL_TO_USEC(t)		((t.tv_sec * 1000000) + (t.tv_usec))
#define TS_GT(a,b)				((int64_t)((a)-(b)) > 0)

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#ifndef MAX_CPUS
#define MAX_CPUS		16
#endif
/*----------------------------------------------------------------------------*/
static pthread_t app_thread[MAX_CPUS];
static mctx_t g_mctx[MAX_CPUS];
static int done[MAX_CPUS];
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
static int fd_offset;
/*----------------------------------------------------------------------------*/
static int fio = FALSE;
static char outfile[MAX_FILE_LEN + 1];
/*----------------------------------------------------------------------------*/
static char host[MAX_IP_STR_LEN + 1] = {'\0'};
static char url[MAX_URL_LEN + 1] = {'\0'};
static in_addr_t daddr;
static in_port_t dport;
static in_addr_t saddr;
/*----------------------------------------------------------------------------*/
static int total_flows;
static int flows[MAX_CPUS];
static int flowcnt = 0;
static int concurrency;
static int max_fds;
static int response_size = 0;
/*----------------------------------------------------------------------------*/
struct wget_stat
{
	uint64_t waits;
	uint64_t events;
	uint64_t connects;
	uint64_t reads;
	uint64_t writes;
	uint64_t completes;

	uint64_t errors;
	uint64_t timedout;

	uint64_t sum_resp_time;
	uint64_t max_resp_time;
};
/*----------------------------------------------------------------------------*/
struct thread_context
{
	int core;

	mctx_t mctx;
	int ep;
	struct wget_vars *wvars;

	int target;
	int started;
	int errors;
	int incompletes;
	int done;
	int pending;

	struct wget_stat stat;
};
typedef struct thread_context* thread_context_t;
/*----------------------------------------------------------------------------*/
struct wget_vars
{
	int request_sent;

	char response[HTTP_HEADER_LEN];
	int resp_len;
	int headerset;
	uint32_t header_len;
	uint64_t file_len;
	uint64_t recv;
	uint64_t write;

	struct timeval t_start;
	struct timeval t_end;
	
	int fd;
};
/*----------------------------------------------------------------------------*/
static struct thread_context *g_ctx[MAX_CPUS] = {0};
static struct wget_stat *g_stat[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/
thread_context_t 
CreateContext(int core)
{
	thread_context_t ctx;

	ctx = (thread_context_t)calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		perror("malloc");
		TRACE_ERROR("Failed to allocate memory for thread context.\n");
		return NULL;
	}
	ctx->core = core;

	ctx->mctx = mtcp_create_context(core);
	if (!ctx->mctx) {
		TRACE_ERROR("Failed to create mtcp context.\n");
		free(ctx);
		return NULL;
	}
	g_mctx[core] = ctx->mctx;

	return ctx;
}
/*----------------------------------------------------------------------------*/
void 
DestroyContext(thread_context_t ctx) 
{
	g_stat[ctx->core] = NULL;
	mtcp_destroy_context(ctx->mctx);
	free(ctx);
}
/*----------------------------------------------------------------------------*/
static inline int 
CreateConnection(thread_context_t ctx)
{
	struct epoll_event ev;
	struct sockaddr_in addr;
	int sockid;
	int ret;

	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		TRACE_INFO("Failed to create socket!\n");
		return -1;
	}
	memset(&ctx->wvars[sockid - fd_offset], 0, sizeof(struct wget_vars));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = daddr;
	addr.sin_port = dport;
	
	ret = connect(sockid, 
			(struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		if (errno != EINPROGRESS) {
			perror("connect");
			close(sockid);
			return -1;
		}
	}

	ctx->started++;
	ctx->pending++;
	ctx->stat.connects++;

	ev.events = EPOLLOUT;
	ev.data.fd = sockid;
	epoll_ctl(ctx->ep, EPOLL_CTL_ADD, sockid, &ev);

	return sockid;
}
/*----------------------------------------------------------------------------*/
static inline void 
CloseConnection(thread_context_t ctx, int sockid)
{
	epoll_ctl(ctx->ep, EPOLL_CTL_DEL, sockid, NULL);
	close(sockid);
	ctx->pending--;
	ctx->done++;
	assert(ctx->pending >= 0);
	while (ctx->pending < concurrency && ctx->started < ctx->target) {
		if (CreateConnection(ctx) < 0) {
			done[ctx->core] = TRUE;
			break;
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline int 
SendHTTPRequest(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
	char request[HTTP_HEADER_LEN];
	struct epoll_event ev;
	int wr;
	int len;

	wv->headerset = FALSE;
	wv->recv = 0;
	wv->header_len = wv->file_len = 0;

	snprintf(request, HTTP_HEADER_LEN, "GET %s HTTP/1.0\r\n"
			"User-Agent: Wget/1.12 (linux-gnu)\r\n"
			"Accept: */*\r\n"
			"Host: %s\r\n"
//			"Connection: Keep-Alive\r\n\r\n", 
			"Connection: Close\r\n\r\n", 
			url, host);
	len = strlen(request);

	wr = write(sockid, request, len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
	ctx->stat.writes += wr;
	TRACE_APP("Socket %d HTTP Request of %d bytes. sent.\n", sockid, wr);
	wv->request_sent = TRUE;

	ev.events = EPOLLIN;
	ev.data.fd = sockid;
	epoll_ctl(ctx->ep, EPOLL_CTL_MOD, sockid, &ev);

	gettimeofday(&wv->t_start, NULL);

	char fname[MAX_FILE_LEN + 1];
	if (fio) {
		snprintf(fname, MAX_FILE_LEN, "%s.%d", outfile, flowcnt++);
		wv->fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (wv->fd < 0) {
			TRACE_APP("Failed to open file descriptor for %s\n", fname);
			exit(1);
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static inline int 
DownloadComplete(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
#ifdef APP
	mctx_t mctx = ctx->mctx;
#endif
	uint64_t tdiff;

	TRACE_APP("Socket %d File download complete!\n", sockid);
	gettimeofday(&wv->t_end, NULL);
	CloseConnection(ctx, sockid);
	ctx->stat.completes++;
	if (response_size == 0) {
		response_size = wv->recv;
		fprintf(stderr, "Response size set to %d\n", response_size);
	} else {
		if (wv->recv != response_size) {
			fprintf(stderr, "Response size mismatch! mine: %ld, theirs: %d\n", 
					wv->recv, response_size);
		}
	}
	tdiff = (wv->t_end.tv_sec - wv->t_start.tv_sec) * 1000000 + 
			(wv->t_end.tv_usec - wv->t_start.tv_usec);
	TRACE_APP("Socket %d Total received bytes: %lu (%luMB)\n", 
			sockid, wv->recv, wv->recv / 1000000);
	TRACE_APP("Socket %d Total spent time: %lu us\n", sockid, tdiff);
	if (tdiff > 0) {
		TRACE_APP("Socket %d Average bandwidth: %lf[MB/s]\n", 
				sockid, (double)wv->recv / tdiff);
	}
	ctx->stat.sum_resp_time += tdiff;
	if (tdiff > ctx->stat.max_resp_time)
		ctx->stat.max_resp_time = tdiff;

	if (fio && wv->fd > 0)
		close(wv->fd);

	return 0;
}
/*----------------------------------------------------------------------------*/
static inline int
HandleReadEvent(thread_context_t ctx, int sockid, struct wget_vars *wv)
{
	//mctx_t mctx = ctx->mctx;
	char buf[BUF_SIZE];
	char *pbuf;
	int rd, copy_len;

	rd = 1;
	while (rd > 0) {
		rd = read(sockid, buf, BUF_SIZE);
		if (rd <= 0)
			break;
		ctx->stat.reads += rd;

		TRACE_APP("Socket %d: read ret: %d, total_recv: %lu, "
				"header_set: %d, header_len: %u, file_len: %lu\n", 
				sockid, rd, wv->recv + rd, 
				wv->headerset, wv->header_len, wv->file_len);

		pbuf = buf;
		if (!wv->headerset) {
			copy_len = MIN(rd, HTTP_HEADER_LEN - wv->resp_len);
			memcpy(wv->response + wv->resp_len, buf, copy_len);
			wv->resp_len += copy_len;
			wv->header_len = find_http_header(wv->response, wv->resp_len);
			if (wv->header_len > 0) {
				wv->response[wv->header_len] = '\0';
				wv->file_len = http_header_long_val(wv->response, 
						CONTENT_LENGTH_HDR, sizeof(CONTENT_LENGTH_HDR) - 1);
				if (wv->file_len < 0) {
					/* failed to find the Content-Length field */
					wv->recv += rd;
					rd = 0;
					CloseConnection(ctx, sockid);
					return 0;
				}

				TRACE_APP("Socket %d Parsed response header. "
						"Header length: %u, File length: %lu (%luMB)\n", 
						sockid, wv->header_len, 
						wv->file_len, wv->file_len / 1024 / 1024);
				wv->headerset = TRUE;
				wv->recv += (rd - (wv->resp_len - wv->header_len));
				
				pbuf += (rd - (wv->resp_len - wv->header_len));
				rd = (wv->resp_len - wv->header_len);
				//printf("Successfully parse header.\n");
				//fflush(stdout);

			} else {
				/* failed to parse response header */
#if 0
				printf("[CPU %d] Socket %d Failed to parse response header."
						" Data: \n%s\n", ctx->core, sockid, wv->response);
				fflush(stdout);
#endif
				wv->recv += rd;
				rd = 0;
				ctx->stat.errors++;
				ctx->errors++;
				CloseConnection(ctx, sockid);
				return 0;
			}
			//pbuf += wv->header_len;
			//wv->recv += wv->header_len;
			//rd -= wv->header_len;
		}
		wv->recv += rd;
		
		if (fio && wv->fd > 0) {
			int wr = 0;
			while (wr < rd) {
				int _wr = write(wv->fd, pbuf + wr, rd - wr);
				assert (_wr == rd - wr);
				 if (_wr < 0) {
					 perror("write");
					 TRACE_ERROR("Failed to write.\n");
					 assert(0);
					 break;
				 }
				 wr += _wr;	
				 wv->write += _wr;
			}
		}
		
		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			break;
		}
	}

	if (rd > 0) {
		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			TRACE_APP("Socket %d Done Write: "
					"header: %u file: %lu recv: %lu write: %lu\n", 
					sockid, wv->header_len, wv->file_len, 
					wv->recv - wv->header_len, wv->write);
			DownloadComplete(ctx, sockid, wv);

			return 0;
		}

	} else if (rd == 0) {
		/* connection closed by remote host */
		TRACE_DBG("Socket %d connection closed with server.\n", sockid);

		if (wv->header_len && (wv->recv >= wv->header_len + wv->file_len)) {
			DownloadComplete(ctx, sockid, wv);
		} else {
			ctx->stat.errors++;
			ctx->incompletes++;
			CloseConnection(ctx, sockid);
		}

	} else if (rd < 0) {
		if (errno != EAGAIN) {
			TRACE_DBG("Socket %d: read() error %s\n", 
					sockid, strerror(errno));
			ctx->stat.errors++;
			ctx->errors++;
			CloseConnection(ctx, sockid);
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
void 
PrintStats()
{
#define LINE_LEN 2048
	char line[LINE_LEN];
	int total_trans;
	int i;

	total_trans = 0;
	line[0] = '\0';
	//sprintf(line, "Trans/s: ");
	for (i = 0; i < core_limit; i++) {
		//sprintf(line + strlen(line), "%6d  ", g_trans[i]);
		sprintf(line + strlen(line), "[CPU%2d] %7d trans/s  ", i, g_trans[i]);
		total_trans += g_trans[i];
		g_trans[i] = 0;
		if (i % 4 == 3)
			sprintf(line + strlen(line), "\n");
	}
	fprintf(stderr, "%s", line);
	fprintf(stderr, "[ ALL ] %7d trans/s\n", total_trans);
	//sprintf(line + strlen(line), "total: %6d", total_trans);
	//printf("%s\n", line);

	//fprintf(stderr, "Transactions/s: %d\n", total_trans);
	fflush(stderr);
}
#endif
/*----------------------------------------------------------------------------*/
static void 
PrintStats()
{
	struct wget_stat total = {0};
	struct wget_stat *st;
	uint64_t avg_resp_time;
	uint64_t total_resp_time = 0;
	int i;

	for (i = 0; i < core_limit; i++) {
		st = g_stat[i];

		if (st == NULL) continue;
		avg_resp_time = st->completes? st->sum_resp_time / st->completes : 0;
#if 0
		fprintf(stderr, "[CPU%2d] epoll_wait: %5lu, event: %7lu, "
				"connect: %7lu, read: %4lu MB, write: %4lu MB, "
				"completes: %7lu (resp_time avg: %4lu, max: %6lu us), "
				"errors: %2lu (timedout: %2lu)\n", 
				i, st->waits, st->events, st->connects, 
				st->reads / 1024 / 1024, st->writes / 1024 / 1024, 
				st->completes, avg_resp_time, st->max_resp_time, 
				st->errors, st->timedout);
#endif

		total.waits += st->waits;
		total.events += st->events;
		total.connects += st->connects;
		total.reads += st->reads;
		total.writes += st->writes;
		total.completes += st->completes;
		total_resp_time += avg_resp_time;
		if (st->max_resp_time > total.max_resp_time)
			total.max_resp_time = st->max_resp_time;
		total.errors += st->errors;
		total.timedout += st->timedout;

		memset(st, 0, sizeof(struct wget_stat));		
	}
	fprintf(stderr, "[ ALL ] connect: %7lu, read: %4lu MB, write: %4lu MB, "
			"completes: %7lu (resp_time avg: %4lu, max: %6lu us)\n", 
			total.connects, 
			total.reads / 1024 / 1024, total.writes / 1024 / 1024, 
			total.completes, total_resp_time / core_limit, total.max_resp_time);
#if 0
	fprintf(stderr, "[ ALL ] epoll_wait: %5lu, event: %7lu, "
			"connect: %7lu, read: %4lu MB, write: %4lu MB, "
			"completes: %7lu (resp_time avg: %4lu, max: %6lu us), "
			"errors: %2lu (timedout: %2lu)\n", 
			total.waits, total.events, total.connects, 
			total.reads / 1024 / 1024, total.writes / 1024 / 1024, 
			total.completes, total_resp_time / core_limit, total.max_resp_time, 
			total.errors, total.timedout);
#endif
}
/*----------------------------------------------------------------------------*/
void *
RunWgetMain(void *arg)
{
	thread_context_t ctx;
	mctx_t mctx;
	int core = *(int *)arg;
	struct in_addr daddr_in;
	int n, maxevents;
	int ep;
	struct epoll_event *events;
	int nevents;
	struct wget_vars *wvars;
	int i;

	struct timeval cur_tv, prev_tv;
	//uint64_t cur_ts, prev_ts;

	mtcp_core_affinitize(core);

	ctx = CreateContext(core);
	if (!ctx) {
		return NULL;
	}
	mctx = ctx->mctx;
	g_ctx[core] = ctx;
	g_stat[core] = &ctx->stat;
	srand(time(NULL));

	mtcp_init_rss(mctx, saddr, IP_RANGE, daddr, dport);

	n = flows[core];
	if (n == 0) {
		TRACE_DBG("Application thread %d finished.\n", core);
		pthread_exit(NULL);
		return NULL;
	}
	ctx->target = n;

	daddr_in.s_addr = daddr;
	fprintf(stderr, "Thread %d handles %d flows. connecting to %s:%u\n", 
			core, n, inet_ntoa(daddr_in), ntohs(dport));

	/* Initialization */
	maxevents = max_fds * 3;
	ep = epoll_create(maxevents);
	if (ep < 0) {
		TRACE_ERROR("Failed to create epoll struct!n");
		exit(EXIT_FAILURE);
	}
	events = (struct epoll_event *)
			calloc(maxevents, sizeof(struct epoll_event));
	if (!events) {
		TRACE_ERROR("Failed to allocate events!\n");
		exit(EXIT_FAILURE);
	}
	ctx->ep = ep;

	wvars = (struct wget_vars *)calloc(max_fds * 3, sizeof(struct wget_vars));
	if (!wvars) {
		TRACE_ERROR("Failed to create wget variables!\n");
		exit(EXIT_FAILURE);
	}
	ctx->wvars = wvars;

	ctx->started = ctx->done = ctx->pending = 0;
	ctx->errors = ctx->incompletes = 0;

	gettimeofday(&cur_tv, NULL);
	//prev_ts = TIMEVAL_TO_USEC(cur_tv);
	prev_tv = cur_tv;

	while (!done[core]) {
		gettimeofday(&cur_tv, NULL);
		//cur_ts = TIMEVAL_TO_USEC(cur_tv);

		/* print statistics every second */
		if (core == 0 && cur_tv.tv_sec > prev_tv.tv_sec) {
		  	PrintStats();
			prev_tv = cur_tv;
		}

		while (ctx->pending < concurrency && ctx->started < ctx->target) {
			if (CreateConnection(ctx) < 0) {
				done[core] = TRUE;
				break;
			}
		}

		nevents = epoll_wait(ep, events, maxevents, -1);
		ctx->stat.waits++;
	
		if (nevents < 0) {
			if (errno != EINTR) {
				TRACE_ERROR("epoll_wait failed! ret: %d\n", nevents);
			}
			done[core] = TRUE;
			break;
		} else {
			ctx->stat.events += nevents;
		}

		for (i = 0; i < nevents; i++) {

			if (events[i].events & EPOLLERR) {
				int err;
				socklen_t len = sizeof(err);

				TRACE_APP("[CPU %d] Error on socket %d\n", 
						core, events[i].data.fd);
				ctx->stat.errors++;
				ctx->errors++;
				if (getsockopt(events[i].data.fd, 
							SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
					if (err == ETIMEDOUT)
						ctx->stat.timedout++;
				}
				CloseConnection(ctx, events[i].data.fd);

			} else if (events[i].events & EPOLLIN) {
				HandleReadEvent(ctx, 
						events[i].data.fd, &wvars[events[i].data.fd - fd_offset]);

			} else if (events[i].events == EPOLLOUT) {
				struct wget_vars *wv = &wvars[events[i].data.fd - fd_offset];

				if (!wv->request_sent) {
					SendHTTPRequest(ctx, events[i].data.fd, wv);
				} else {
					//TRACE_DBG("Request already sent.\n");
				}

			} else {
				TRACE_ERROR("Socket %d: event: %s\n", 
						events[i].data.fd, EventToString(events[i].events));
				assert(0);
			}
		}

		if (ctx->done >= ctx->target) {
			fprintf(stdout, "[CPU %d] Completed %d connections, "
					"errors: %d incompletes: %d\n", 
					ctx->core, ctx->done, ctx->errors, ctx->incompletes);
			break;
		}
	}

	TRACE_INFO("Wget thread %d waiting for mtcp to be destroyed.\n", core);
	DestroyContext(ctx);

	TRACE_DBG("Wget thread %d finished.\n", core);
	pthread_exit(NULL);
	return NULL;
}
/*----------------------------------------------------------------------------*/
void 
SignalHandler(int signum)
{
	int i;

	for (i = 0; i < core_limit; i++) {
		done[i] = TRUE;
	}
}
/*----------------------------------------------------------------------------*/
int 
main(int argc, char **argv)
{
	struct mtcp_conf mcfg;
	struct rlimit r;	
	char *conf_file;
	int cores[MAX_CPUS];
	int flow_per_thread;
	int flow_remainder_cnt;
	int total_concurrency = 0;
	int ret;
	int i, o;
	int process_cpu;

	if (argc < 3) {
		TRACE_CONFIG("Too few arguments!\n");
		TRACE_CONFIG("Usage: %s url #flows [output]\n", argv[0]);
		return FALSE;
	}

	if (strlen(argv[1]) > MAX_URL_LEN) {
		TRACE_CONFIG("Length of URL should be smaller than %d!\n", MAX_URL_LEN);
		return FALSE;
	}

	char* slash_p = strchr(argv[1], '/');
	if (slash_p) {
		strncpy(host, argv[1], slash_p - argv[1]);
		strncpy(url, strchr(argv[1], '/'), MAX_URL_LEN);
	} else {
		strncpy(host, argv[1], MAX_IP_STR_LEN);
		strncpy(url, "/", 2);
	}

	conf_file = NULL;
	process_cpu = -1;
	daddr = inet_addr(host);
	dport = htons(80);
	saddr = INADDR_ANY;

	total_flows = mystrtol(argv[2], 10);
	if (total_flows <= 0) {
		TRACE_CONFIG("Number of flows should be large than 0.\n");
		return FALSE;
	}

	num_cores = GetNumCPUs();
	core_limit = num_cores;
	concurrency = 100;

	ret = getrlimit(RLIMIT_NOFILE, &r);
	if (ret == -1) {
		TRACE_CONFIG("Can't get max number of file descriptors!\n");
		exit(EXIT_FAILURE);
	} else {
		fd_offset = r.rlim_cur;
	}
	
	while (-1 != (o = getopt(argc, argv, "N:c:o:n:f:"))) {
		switch(o) {
		case 'N':
			core_limit = mystrtol(optarg, 10);
			if (core_limit > num_cores) {
				TRACE_CONFIG("CPU limit should be smaller than the "
					     "number of CPUS: %d\n", num_cores);
				return FALSE;
			} else if (core_limit < 1) {
				TRACE_CONFIG("CPU limit should be greater than 0\n");
				return FALSE;
			}
			/** 
			 * it is important that core limit is set 
			 * before mtcp_init() is called. You can
			 * not set core_limit after mtcp_init()
			 */
			mtcp_getconf(&mcfg);
			mcfg.num_cores = core_limit;
			mtcp_setconf(&mcfg);
			break;
		case 'c':
			total_concurrency = mystrtol(optarg, 10);
			break;
		case 'o':
			if (strlen(optarg) > MAX_FILE_LEN) {
				TRACE_CONFIG("Output file length should be smaller than %d!\n", 
					     MAX_FILE_LEN);
				return FALSE;
			}
			fio = TRUE;
			strncpy(outfile, optarg, MAX_FILE_LEN);
			break;
		case 'n':
			process_cpu = mystrtol(optarg, 10);
			if (process_cpu > core_limit) {
				TRACE_CONFIG("Starting CPU is way off limits!\n");
				return FALSE;
			}
			break;
		case 'f':
			conf_file = optarg;
			break;
		}
	}

	if (total_flows < core_limit) {
		core_limit = total_flows;
	}

	/* per-core concurrency = total_concurrency / # cores */
	if (total_concurrency > 0)
		concurrency = total_concurrency / core_limit;

	/* set the max number of fds 3x larger than concurrency */
	max_fds = concurrency * 3;

	TRACE_CONFIG("Application configuration:\n");
	TRACE_CONFIG("URL: %s\n", url);
	TRACE_CONFIG("# of total_flows: %d\n", total_flows);
	TRACE_CONFIG("# of cores: %d\n", core_limit);
	TRACE_CONFIG("Concurrency: %d\n", total_concurrency);
	if (fio) {
		TRACE_CONFIG("Output file: %s\n", outfile);
	}

	if (conf_file == NULL) {
		TRACE_ERROR("mTCP configuration file is not set!\n");
		exit(EXIT_FAILURE);
	}
	
	ret = mtcp_init(conf_file);
	if (ret) {
		TRACE_ERROR("Failed to initialize mtcp.\n");
		exit(EXIT_FAILURE);
	}
	mtcp_getconf(&mcfg);
	mcfg.max_concurrency = max_fds;
	mcfg.max_num_buffers = max_fds;
	mtcp_setconf(&mcfg);

	mtcp_register_signal(SIGINT, SignalHandler);

	flow_per_thread = total_flows / core_limit;
	flow_remainder_cnt = total_flows % core_limit;
	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		cores[i] = i;
		done[i] = FALSE;
		flows[i] = flow_per_thread;

		if (flow_remainder_cnt-- > 0)
			flows[i]++;

		if (flows[i] == 0)
			continue;

		if (pthread_create(&app_thread[i], 
					NULL, RunWgetMain, (void *)&cores[i])) {
			perror("pthread_create");
			TRACE_ERROR("Failed to create wget thread.\n");
			exit(-1);
		}

		if (process_cpu != -1)
			break;
	}

	for (i = ((process_cpu == -1) ? 0 : process_cpu); i < core_limit; i++) {
		pthread_join(app_thread[i], NULL);
		TRACE_INFO("Wget thread %d joined.\n", i);

		if (process_cpu != -1)
			break;
	}

	mtcp_destroy();
	return 0;
}
/*----------------------------------------------------------------------------*/
