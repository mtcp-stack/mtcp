#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

#include "cpu.h"
#include "eth_in.h"
#include "fhash.h"
#include "tcp_send_buffer.h"
#include "tcp_ring_buffer.h"
#include "socket.h"
#include "eth_out.h"
#include "tcp.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "mtcp_api.h"
#include "eventpoll.h"
#include "logger.h"
#include "config.h"
#include "arp.h"
#include "ip_out.h"
#include "timer.h"
#include "debug.h"
#include "event_callback.h"
#include "tcp_rb.h"
#include "tcp_stream.h"
#include "io_module.h"

#ifdef ENABLE_DPDK
/* for launching rte thread */
#include <rte_launch.h>
#include <rte_lcore.h>
#endif /* !ENABLE_DPDK */
#define PS_CHUNK_SIZE 64
#define RX_THRESH (PS_CHUNK_SIZE * 0.8)

#define ROUND_STAT FALSE
#define TIME_STAT FALSE
#define EVENT_STAT FALSE
#define TESTING FALSE

#define LOG_FILE_NAME "log"
#define MAX_FILE_NAME 1024

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define PER_STREAM_SLICE 0.1		// in ms
#define PER_STREAM_TCHECK 1			// in ms
#define PS_SELECT_TIMEOUT 100		// in us 

#define GBPS(bytes) (bytes * 8.0 / (1000 * 1000 * 1000))

/*----------------------------------------------------------------------------*/
/* handlers for threads */
struct mtcp_thread_context *g_pctx[MAX_CPUS] = {0};
struct log_thread_context *g_logctx[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/
static pthread_t g_thread[MAX_CPUS] = {0};
static pthread_t log_thread[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/
static sem_t g_init_sem[MAX_CPUS];
static sem_t g_done_sem[MAX_CPUS];
static int running[MAX_CPUS] = {0};
/*----------------------------------------------------------------------------*/
mtcp_sighandler_t app_signal_handler;
static int sigint_cnt[MAX_CPUS] = {0};
static struct timespec sigint_ts[MAX_CPUS];
/*----------------------------------------------------------------------------*/
#ifdef NETSTAT
#if NETSTAT_TOTAL
static int printer = -1;
#if ROUND_STAT
#endif /* ROUND_STAT */
#endif /* NETSTAT_TOTAL */
#endif /* NETSTAT */
/*----------------------------------------------------------------------------*/
void
HandleSignal(int signal)
{
	int i = 0;

	if (signal == SIGINT) {
#ifdef DARWIN
		int core = 0;
#else
		int core = sched_getcpu();
#endif
		struct timespec cur_ts;

		clock_gettime(CLOCK_REALTIME, &cur_ts);

		if (sigint_cnt[core] > 0 && cur_ts.tv_sec == sigint_ts[core].tv_sec) {
			for (i = 0; i < g_config.mos->num_cores; i++) {
				if (running[i]) {
					exit(0);
					g_pctx[i]->exit = TRUE;
				}
			}
		} else {
			for (i = 0; i < g_config.mos->num_cores; i++) {
				if (g_pctx[i])
					g_pctx[i]->interrupt = TRUE;
			}
			if (!app_signal_handler) {
				for (i = 0; i < g_config.mos->num_cores; i++) {
					if (running[i]) {
						exit(0);
						g_pctx[i]->exit = TRUE;
					}
				}
			}
		}
		sigint_cnt[core]++;
		clock_gettime(CLOCK_REALTIME, &sigint_ts[core]);
	}

	if (signal != SIGUSR1) {
		if (app_signal_handler) {
			app_signal_handler(signal);
		}
	}
}
/*----------------------------------------------------------------------------*/
static int 
AttachDevice(struct mtcp_thread_context* ctx)
{
	int working = -1;
	mtcp_manager_t mtcp = ctx->mtcp_manager;

	if (mtcp->iom->link_devices)
		working = mtcp->iom->link_devices(ctx);
	else
		return 0;

	return working;
}
/*----------------------------------------------------------------------------*/
#ifdef TIMESTAT
static inline void 
InitStatCounter(struct stat_counter *counter)
{
	counter->cnt = 0;
	counter->sum = 0;
	counter->max = 0;
	counter->min = 0;
}
/*----------------------------------------------------------------------------*/
static inline void 
UpdateStatCounter(struct stat_counter *counter, int64_t value)
{
	counter->cnt++;
	counter->sum += value;
	if (value > counter->max)
		counter->max = value;
	if (counter->min == 0 || value < counter->min)
		counter->min = value;
}
/*----------------------------------------------------------------------------*/
static inline uint64_t 
GetAverageStat(struct stat_counter *counter)
{
	return counter->cnt ? (counter->sum / counter->cnt) : 0;
}
/*----------------------------------------------------------------------------*/
static inline int64_t 
TimeDiffUs(struct timeval *t2, struct timeval *t1)
{
	return (t2->tv_sec - t1->tv_sec) * 1000000 + 
			(int64_t)(t2->tv_usec - t1->tv_usec);
}
/*----------------------------------------------------------------------------*/
#endif
#ifdef NETSTAT
static inline void 
PrintThreadNetworkStats(mtcp_manager_t mtcp, struct net_stat *ns)
{
	int i;

	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		ns->rx_packets[i] = mtcp->nstat.rx_packets[i] - mtcp->p_nstat.rx_packets[i];
		ns->rx_errors[i] = mtcp->nstat.rx_errors[i] - mtcp->p_nstat.rx_errors[i];
		ns->rx_bytes[i] = mtcp->nstat.rx_bytes[i] - mtcp->p_nstat.rx_bytes[i];
		ns->tx_packets[i] = mtcp->nstat.tx_packets[i] - mtcp->p_nstat.tx_packets[i];
		ns->tx_drops[i] = mtcp->nstat.tx_drops[i] - mtcp->p_nstat.tx_drops[i];
		ns->tx_bytes[i] = mtcp->nstat.tx_bytes[i] - mtcp->p_nstat.tx_bytes[i];
#if NETSTAT_PERTHREAD
		if (g_config.mos->netdev_table->ent[i]->stat_print) {
			fprintf(stderr, "[CPU%2d] %s flows: %6u, "
					"RX: %7llu(pps) (err: %5llu), %5.2lf(Gbps), "
					"TX: %7llu(pps), %5.2lf(Gbps)\n", 
					mtcp->ctx->cpu,
					g_config.mos->netdev_table->ent[i]->dev_name,
					(unsigned)mtcp->flow_cnt, 
					(long long unsigned)ns->rx_packets[i],
					(long long unsigned)ns->rx_errors[i],
					GBPS(ns->rx_bytes[i]), 
					(long long unsigned)ns->tx_packets[i],
					GBPS(ns->tx_bytes[i]));
		}
#endif
	}
	mtcp->p_nstat = mtcp->nstat;

}
/*----------------------------------------------------------------------------*/
#if ROUND_STAT
static inline void 
PrintThreadRoundStats(mtcp_manager_t mtcp, struct run_stat *rs)
{
#define ROUND_DIV (1000)
	rs->rounds = mtcp->runstat.rounds - mtcp->p_runstat.rounds;
	rs->rounds_rx = mtcp->runstat.rounds_rx - mtcp->p_runstat.rounds_rx;
	rs->rounds_rx_try = mtcp->runstat.rounds_rx_try - mtcp->p_runstat.rounds_rx_try;
	rs->rounds_tx = mtcp->runstat.rounds_tx - mtcp->p_runstat.rounds_tx;
	rs->rounds_tx_try = mtcp->runstat.rounds_tx_try - mtcp->p_runstat.rounds_tx_try;
	rs->rounds_select = mtcp->runstat.rounds_select - mtcp->p_runstat.rounds_select;
	rs->rounds_select_rx = mtcp->runstat.rounds_select_rx - mtcp->p_runstat.rounds_select_rx;
	rs->rounds_select_tx = mtcp->runstat.rounds_select_tx - mtcp->p_runstat.rounds_select_tx;
	rs->rounds_select_intr = mtcp->runstat.rounds_select_intr - mtcp->p_runstat.rounds_select_intr;
	rs->rounds_twcheck = mtcp->runstat.rounds_twcheck - mtcp->p_runstat.rounds_twcheck;
	mtcp->p_runstat = mtcp->runstat;
#if NETSTAT_PERTHREAD
	fprintf(stderr, "[CPU%2d] Rounds: %4lluK, "
			"rx: %3lluK (try: %4lluK), tx: %3lluK (try: %4lluK), "
			"ps_select: %4llu (rx: %4llu, tx: %4llu, intr: %3llu)\n", 
			mtcp->ctx->cpu, rs->rounds / ROUND_DIV, 
			rs->rounds_rx / ROUND_DIV, rs->rounds_rx_try / ROUND_DIV, 
			rs->rounds_tx / ROUND_DIV, rs->rounds_tx_try / ROUND_DIV, 
			rs->rounds_select, 
			rs->rounds_select_rx, rs->rounds_select_tx, rs->rounds_select_intr);
#endif
}
#endif /* ROUND_STAT */
/*----------------------------------------------------------------------------*/
#if TIME_STAT
static inline void
PrintThreadRoundTime(mtcp_manager_t mtcp)
{
	fprintf(stderr, "[CPU%2d] Time: (avg, max) "
			"round: (%4luus, %4luus), processing: (%4luus, %4luus), "
			"tcheck: (%4luus, %4luus), epoll: (%4luus, %4luus), "
			"handle: (%4luus, %4luus), xmit: (%4luus, %4luus), "
			"select: (%4luus, %4luus)\n", mtcp->ctx->cpu, 
			GetAverageStat(&mtcp->rtstat.round), mtcp->rtstat.round.max, 
			GetAverageStat(&mtcp->rtstat.processing), mtcp->rtstat.processing.max, 
			GetAverageStat(&mtcp->rtstat.tcheck), mtcp->rtstat.tcheck.max, 
			GetAverageStat(&mtcp->rtstat.epoll), mtcp->rtstat.epoll.max, 
			GetAverageStat(&mtcp->rtstat.handle), mtcp->rtstat.handle.max, 
			GetAverageStat(&mtcp->rtstat.xmit), mtcp->rtstat.xmit.max, 
			GetAverageStat(&mtcp->rtstat.select), mtcp->rtstat.select.max);
	
	InitStatCounter(&mtcp->rtstat.round);
	InitStatCounter(&mtcp->rtstat.processing);
	InitStatCounter(&mtcp->rtstat.tcheck);
	InitStatCounter(&mtcp->rtstat.epoll);
	InitStatCounter(&mtcp->rtstat.handle);
	InitStatCounter(&mtcp->rtstat.xmit);
	InitStatCounter(&mtcp->rtstat.select);
}
#endif
#endif /* NETSTAT */
/*----------------------------------------------------------------------------*/
#if EVENT_STAT
static inline void 
PrintEventStat(int core, struct mtcp_epoll_stat *stat)
{
	fprintf(stderr, "[CPU%2d] calls: %lu, waits: %lu, wakes: %lu, "
			"issued: %lu, registered: %lu, invalidated: %lu, handled: %lu\n", 
			core, stat->calls, stat->waits, stat->wakes, 
			stat->issued, stat->registered, stat->invalidated, stat->handled);
	memset(stat, 0, sizeof(struct mtcp_epoll_stat));
}
#endif /* EVENT_STAT */
/*----------------------------------------------------------------------------*/
#ifdef NETSTAT
static inline void
PrintNetworkStats(mtcp_manager_t mtcp, uint32_t cur_ts)
{
#define TIMEOUT 1
	int i;
	struct net_stat ns;
	bool stat_print = false;
#if ROUND_STAT
	struct run_stat rs;
#endif /* ROUND_STAT */
#ifdef NETSTAT_TOTAL
	static double peak_total_rx_gbps = 0;
	static double peak_total_tx_gbps = 0;
	static double avg_total_rx_gbps = 0;
	static double avg_total_tx_gbps = 0;

	double total_rx_gbps = 0, total_tx_gbps = 0;
	int j;
	uint32_t gflow_cnt = 0;
	struct net_stat g_nstat;
#if ROUND_STAT
	struct run_stat g_runstat;
#endif /* ROUND_STAT */
#endif /* NETSTAT_TOTAL */

	if (TS_TO_MSEC(cur_ts - mtcp->p_nstat_ts) < SEC_TO_MSEC(TIMEOUT)) {
		return;
	}

	mtcp->p_nstat_ts = cur_ts;
	gflow_cnt = 0;
	memset(&g_nstat, 0, sizeof(struct net_stat));
	for (i = 0; i < g_config.mos->num_cores; i++) {
		if (running[i]) {
			PrintThreadNetworkStats(g_mtcp[i], &ns);
#if NETSTAT_TOTAL
			gflow_cnt += g_mtcp[i]->flow_cnt;
			for (j = 0; j < g_config.mos->netdev_table->num; j++) {
				g_nstat.rx_packets[j] += ns.rx_packets[j];
				g_nstat.rx_errors[j] += ns.rx_errors[j];
				g_nstat.rx_bytes[j] += ns.rx_bytes[j];
				g_nstat.tx_packets[j] += ns.tx_packets[j];
				g_nstat.tx_drops[j] += ns.tx_drops[j];
				g_nstat.tx_bytes[j] += ns.tx_bytes[j];
			}
#endif
		}
	}
#if NETSTAT_TOTAL
	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		if (g_config.mos->netdev_table->ent[i]->stat_print) {
			fprintf(stderr, "[ ALL ] %s, "
			"RX: %7llu(pps) (err: %5llu), %5.2lf(Gbps), "
			"TX: %7llu(pps), %5.2lf(Gbps)\n",
				g_config.mos->netdev_table->ent[i]->dev_name,
				(long long unsigned)g_nstat.rx_packets[i],
				(long long unsigned)g_nstat.rx_errors[i], 
				GBPS(g_nstat.rx_bytes[i]),
				(long long unsigned)g_nstat.tx_packets[i], 
				GBPS(g_nstat.tx_bytes[i]));
			total_rx_gbps += GBPS(g_nstat.rx_bytes[i]);
			total_tx_gbps += GBPS(g_nstat.tx_bytes[i]);
			stat_print = true;
		}
	}
	if (stat_print) {
		fprintf(stderr, "[ ALL ] flows: %6u\n", gflow_cnt);
		if (avg_total_rx_gbps == 0)
			avg_total_rx_gbps = total_rx_gbps;
		else
			avg_total_rx_gbps = avg_total_rx_gbps * 0.6 + total_rx_gbps * 0.4;

		if (avg_total_tx_gbps == 0)
			avg_total_tx_gbps = total_tx_gbps;
		else
			avg_total_tx_gbps = avg_total_tx_gbps * 0.6 + total_tx_gbps * 0.4;

		if (peak_total_rx_gbps < total_rx_gbps)
			peak_total_rx_gbps = total_rx_gbps;
		if (peak_total_tx_gbps < total_tx_gbps)
			peak_total_tx_gbps = total_tx_gbps;

		fprintf(stderr, "[ PEAK ] RX: %5.2lf(Gbps), TX: %5.2lf(Gbps)\n"
						"[ RECENT AVG ] RX: %5.2lf(Gbps), TX: %5.2lf(Gbps)\n",
				peak_total_rx_gbps, peak_total_tx_gbps,
				avg_total_rx_gbps, avg_total_tx_gbps);
	}
#endif

#if ROUND_STAT
	memset(&g_runstat, 0, sizeof(struct run_stat));
	for (i = 0; i < g_config.mos->num_cores; i++) {
		if (running[i]) {
			PrintThreadRoundStats(g_mtcp[i], &rs);
#if DBGMSG
			g_runstat.rounds += rs.rounds;
			g_runstat.rounds_rx += rs.rounds_rx;
			g_runstat.rounds_rx_try += rs.rounds_rx_try;
			g_runstat.rounds_tx += rs.rounds_tx;
			g_runstat.rounds_tx_try += rs.rounds_tx_try;
			g_runstat.rounds_select += rs.rounds_select;
			g_runstat.rounds_select_rx += rs.rounds_select_rx;
			g_runstat.rounds_select_tx += rs.rounds_select_tx;
#endif
		}
	}

	TRACE_DBG("[ ALL ] Rounds: %4ldK, "
		  "rx: %3ldK (try: %4ldK), tx: %3ldK (try: %4ldK), "
		  "ps_select: %4ld (rx: %4ld, tx: %4ld)\n", 
		  g_runstat.rounds / 1000, g_runstat.rounds_rx / 1000, 
		  g_runstat.rounds_rx_try / 1000, g_runstat.rounds_tx / 1000, 
		  g_runstat.rounds_tx_try / 1000, g_runstat.rounds_select, 
		  g_runstat.rounds_select_rx, g_runstat.rounds_select_tx);
#endif /* ROUND_STAT */

#if TIME_STAT
	for (i = 0; i < g_config.mos->num_cores; i++) {
		if (running[i]) {
			PrintThreadRoundTime(g_mtcp[i]);
		}
	}
#endif

#if EVENT_STAT
	for (i = 0; i < g_config.mos->num_cores; i++) {
		if (running[i] && g_mtcp[i]->ep) {
			PrintEventStat(i, &g_mtcp[i]->ep->stat);
		}
	}
#endif

	fflush(stderr);
}
#endif /* NETSTAT */
/*----------------------------------------------------------------------------*/
static inline void
FlushMonitorReadEvents(mtcp_manager_t mtcp)
{
	struct event_queue *mtcpq;
	struct tcp_stream *cur_stream;
	struct mon_listener *walk;
	
	/* check if monitor sockets should be passed data */
	TAILQ_FOREACH(walk, &mtcp->monitors, link) {
		if (walk->socket->socktype != MOS_SOCK_MONITOR_STREAM ||
			!(mtcpq = walk->eq))
			continue;

		while (mtcpq->num_events > 0) {
			cur_stream =
				(struct tcp_stream *)mtcpq->events[mtcpq->start++].ev.data.ptr;
			/* only read events */
			if (cur_stream != NULL &&
					(cur_stream->socket->events | MOS_EPOLLIN)) {
				if (cur_stream->rcvvar != NULL &&
						cur_stream->rcvvar->rcvbuf != NULL) {
					/* no need to pass pkt context */
					struct socket_map *walk;
					SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
						HandleCallback(mtcp, MOS_NULL, walk,
							       cur_stream->side, NULL, 
							       MOS_ON_CONN_NEW_DATA);
					} SOCKQ_FOREACH_END;
				}
				/* reset the actions now */
				cur_stream->actions = 0;
			}
			if (mtcpq->start >= mtcpq->size)
				mtcpq->start = 0;
			mtcpq->num_events--;
		}
	}
}
/*----------------------------------------------------------------------------*/
static inline void
FlushBufferedReadEvents(mtcp_manager_t mtcp)
{
	int i;
	int offset;
	struct event_queue *mtcpq;
	struct tcp_stream *cur_stream;

	if (mtcp->ep == NULL) {
		TRACE_EPOLL("No epoll socket has been registered yet!\n");
		return;
	} else {
		/* case when mtcpq exists */
		mtcpq = mtcp->ep->mtcp_queue;	
		offset = mtcpq->start;
	}

	/* we will use queued-up epoll read-in events
	 * to trigger buffered read monitor events */
	for (i = 0; i < mtcpq->num_events; i++) {
		cur_stream = mtcp->smap[mtcpq->events[offset++].sockid].stream;
		/* only read events */
		/* Raise new data callback event */
		if (cur_stream != NULL &&
				(cur_stream->socket->events | MOS_EPOLLIN)) {
			if (cur_stream->rcvvar != NULL &&
					cur_stream->rcvvar->rcvbuf != NULL) {
				/* no need to pass pkt context */
				struct socket_map *walk;
				SOCKQ_FOREACH_START(walk, &cur_stream->msocks) {
					HandleCallback(mtcp, MOS_NULL, walk, cur_stream->side,
							NULL, MOS_ON_CONN_NEW_DATA);
				} SOCKQ_FOREACH_END;
			}
		}
		if (offset >= mtcpq->size)
			offset = 0;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
FlushEpollEvents(mtcp_manager_t mtcp, uint32_t cur_ts)
{
	struct mtcp_epoll *ep = mtcp->ep;
	struct event_queue *usrq = ep->usr_queue;
	struct event_queue *mtcpq = ep->mtcp_queue;

	pthread_mutex_lock(&ep->epoll_lock);
	if (ep->mtcp_queue->num_events > 0) {
		/* while mtcp_queue have events */
		/* and usr_queue is not full */
		while (mtcpq->num_events > 0 && usrq->num_events < usrq->size) {
			/* copy the event from mtcp_queue to usr_queue */
			usrq->events[usrq->end++] = mtcpq->events[mtcpq->start++];

			if (usrq->end >= usrq->size)
				usrq->end = 0;
			usrq->num_events++;

			if (mtcpq->start >= mtcpq->size)
				mtcpq->start = 0;
			mtcpq->num_events--;
		}
	}

	/* if there are pending events, wake up user */
	if (ep->waiting && (ep->usr_queue->num_events > 0 || 
				ep->usr_shadow_queue->num_events > 0)) {
		STAT_COUNT(mtcp->runstat.rounds_epoll);
		TRACE_EPOLL("Broadcasting events. num: %d, cur_ts: %u, prev_ts: %u\n", 
				ep->usr_queue->num_events, cur_ts, mtcp->ts_last_event);
		mtcp->ts_last_event = cur_ts;
		ep->stat.wakes++;
		pthread_cond_signal(&ep->epoll_cond);
	}
	pthread_mutex_unlock(&ep->epoll_lock);
}
/*----------------------------------------------------------------------------*/
static inline void 
HandleApplicationCalls(mtcp_manager_t mtcp, uint32_t cur_ts)
{
	tcp_stream *stream;
	int cnt, max_cnt;
	int handled, delayed;
	int control, send, ack;

	/* connect handling */
	while ((stream = StreamDequeue(mtcp->connectq))) {
		if (stream->state != TCP_ST_SYN_SENT) {
			TRACE_INFO("Got a connection request from app with state: %s",
				   TCPStateToString(stream));
			exit(EXIT_FAILURE);
		} else {
			stream->cb_events |= MOS_ON_CONN_START | 
				MOS_ON_TCP_STATE_CHANGE;
			/* if monitor is on... */
			if (stream->pair_stream != NULL)
				stream->pair_stream->cb_events |= 
					MOS_ON_CONN_START;
		}
		AddtoControlList(mtcp, stream, cur_ts);
	}

	/* send queue handling */
	while ((stream = StreamDequeue(mtcp->sendq))) {
		stream->sndvar->on_sendq = FALSE;
		AddtoSendList(mtcp, stream);
	}
	
	/* ack queue handling */
	while ((stream = StreamDequeue(mtcp->ackq))) {
		stream->sndvar->on_ackq = FALSE;
		EnqueueACK(mtcp, stream, cur_ts, ACK_OPT_AGGREGATE);
	}

	/* close handling */
	handled = delayed = 0;
	control = send = ack = 0;
	while ((stream = StreamDequeue(mtcp->closeq))) {
		struct tcp_send_vars *sndvar = stream->sndvar;
		sndvar->on_closeq = FALSE;
		
		if (sndvar->sndbuf) {
			sndvar->fss = sndvar->sndbuf->head_seq + sndvar->sndbuf->len;
		} else {
			sndvar->fss = stream->snd_nxt;
		}

		if (g_config.mos->tcp_timeout > 0)
			RemoveFromTimeoutList(mtcp, stream);

		if (stream->have_reset) {
			handled++;
			if (stream->state != TCP_ST_CLOSED_RSVD) {
				stream->close_reason = TCP_RESET;
				stream->state = TCP_ST_CLOSED_RSVD;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", stream->id);
				DestroyTCPStream(mtcp, stream);
			} else {
				TRACE_ERROR("Stream already closed.\n");
			}

		} else if (sndvar->on_control_list) {
			sndvar->on_closeq_int = TRUE;
			StreamInternalEnqueue(mtcp->closeq_int, stream);
			delayed++;
			if (sndvar->on_control_list)
				control++;
			if (sndvar->on_send_list)
				send++;
			if (sndvar->on_ack_list)
				ack++;

		} else if (sndvar->on_send_list || sndvar->on_ack_list) {
			handled++;
			if (stream->state == TCP_ST_ESTABLISHED) {
				stream->state = TCP_ST_FIN_WAIT_1;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_FIN_WAIT_1\n", stream->id);

			} else if (stream->state == TCP_ST_CLOSE_WAIT) {
				stream->state = TCP_ST_LAST_ACK;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_LAST_ACK\n", stream->id);
			}
			stream->control_list_waiting = TRUE;

		} else if (stream->state != TCP_ST_CLOSED_RSVD) {
			handled++;
			if (stream->state == TCP_ST_ESTABLISHED) {
				stream->state = TCP_ST_FIN_WAIT_1;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_FIN_WAIT_1\n", stream->id);

			} else if (stream->state == TCP_ST_CLOSE_WAIT) {
				stream->state = TCP_ST_LAST_ACK;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_LAST_ACK\n", stream->id);
			}
			//sndvar->rto = TCP_FIN_RTO;
			//UpdateRetransmissionTimer(mtcp, stream, mtcp->cur_ts);
			AddtoControlList(mtcp, stream, cur_ts);
		} else {
			TRACE_ERROR("Already closed connection!\n");
		}
	}
	TRACE_ROUND("Handling close connections. cnt: %d\n", cnt);

	cnt = 0;
	max_cnt = mtcp->closeq_int->count;
	while (cnt++ < max_cnt) {
		stream = StreamInternalDequeue(mtcp->closeq_int);

		if (stream->sndvar->on_control_list) {
			StreamInternalEnqueue(mtcp->closeq_int, stream);

		} else if (stream->state != TCP_ST_CLOSED_RSVD) {
			handled++;
			stream->sndvar->on_closeq_int = FALSE;
			if (stream->state == TCP_ST_ESTABLISHED) {
				stream->state = TCP_ST_FIN_WAIT_1;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_FIN_WAIT_1\n", stream->id);

			} else if (stream->state == TCP_ST_CLOSE_WAIT) {
				stream->state = TCP_ST_LAST_ACK;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_LAST_ACK\n", stream->id);
			}
			AddtoControlList(mtcp, stream, cur_ts);
		} else {
			stream->sndvar->on_closeq_int = FALSE;
			TRACE_ERROR("Already closed connection!\n");
		}
	}

	/* reset handling */
	while ((stream = StreamDequeue(mtcp->resetq))) {
		stream->sndvar->on_resetq = FALSE;
		
		if (g_config.mos->tcp_timeout > 0)
			RemoveFromTimeoutList(mtcp, stream);

		if (stream->have_reset) {
			if (stream->state != TCP_ST_CLOSED_RSVD) {
				stream->close_reason = TCP_RESET;
				stream->state = TCP_ST_CLOSED_RSVD;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", stream->id);
				DestroyTCPStream(mtcp, stream);
			} else {
				TRACE_ERROR("Stream already closed.\n");
			}

		} else if (stream->sndvar->on_control_list || 
				stream->sndvar->on_send_list || stream->sndvar->on_ack_list) {
			/* wait until all the queues are flushed */
			stream->sndvar->on_resetq_int = TRUE;
			StreamInternalEnqueue(mtcp->resetq_int, stream);

		} else {
			if (stream->state != TCP_ST_CLOSED_RSVD) {
				stream->close_reason = TCP_ACTIVE_CLOSE;
				stream->state = TCP_ST_CLOSED_RSVD;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", stream->id);
				AddtoControlList(mtcp, stream, cur_ts);
			} else {
				TRACE_ERROR("Stream already closed.\n");
			}
		}
	}
	TRACE_ROUND("Handling reset connections. cnt: %d\n", cnt);

	cnt = 0;
	max_cnt = mtcp->resetq_int->count;
	while (cnt++ < max_cnt) {
		stream = StreamInternalDequeue(mtcp->resetq_int);
		
		if (stream->sndvar->on_control_list || 
				stream->sndvar->on_send_list || stream->sndvar->on_ack_list) {
			/* wait until all the queues are flushed */
			StreamInternalEnqueue(mtcp->resetq_int, stream);

		} else {
			stream->sndvar->on_resetq_int = FALSE;

			if (stream->state != TCP_ST_CLOSED_RSVD) {
				stream->close_reason = TCP_ACTIVE_CLOSE;
				stream->state = TCP_ST_CLOSED_RSVD;
				stream->cb_events |= MOS_ON_TCP_STATE_CHANGE;
				TRACE_STATE("Stream %d: TCP_ST_CLOSED_RSVD\n", stream->id);
				AddtoControlList(mtcp, stream, cur_ts);
			} else {
				TRACE_ERROR("Stream already closed.\n");
			}
		}
	}

	/* destroy streams in destroyq */
	while ((stream = StreamDequeue(mtcp->destroyq))) {
		DestroyTCPStream(mtcp, stream);
	}

	mtcp->wakeup_flag = FALSE;
}
/*----------------------------------------------------------------------------*/
static inline void 
WritePacketsToChunks(mtcp_manager_t mtcp, uint32_t cur_ts)
{
	int thresh = g_config.mos->max_concurrency;
	int i;

	/* Set the threshold to g_config.mos->max_concurrency to send ACK immediately */
	/* Otherwise, set to appropriate value (e.g. thresh) */
	assert(mtcp->g_sender != NULL);
	if (mtcp->g_sender->control_list_cnt)
		WriteTCPControlList(mtcp, mtcp->g_sender, cur_ts, thresh);
	if (mtcp->g_sender->ack_list_cnt)
		WriteTCPACKList(mtcp, mtcp->g_sender, cur_ts, thresh);
	if (mtcp->g_sender->send_list_cnt)
		WriteTCPDataList(mtcp, mtcp->g_sender, cur_ts, thresh);

	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		assert(mtcp->n_sender[i] != NULL);
		if (mtcp->n_sender[i]->control_list_cnt)
			WriteTCPControlList(mtcp, mtcp->n_sender[i], cur_ts, thresh);
		if (mtcp->n_sender[i]->ack_list_cnt)
			WriteTCPACKList(mtcp, mtcp->n_sender[i], cur_ts, thresh);
		if (mtcp->n_sender[i]->send_list_cnt)
			WriteTCPDataList(mtcp, mtcp->n_sender[i], cur_ts, thresh);
	}
}
/*----------------------------------------------------------------------------*/
#if TESTING
static int
DestroyRemainingFlows(mtcp_manager_t mtcp)
{
	struct hashtable *ht = mtcp->tcp_flow_table;
	tcp_stream *walk;
	int cnt, i;

	cnt = 0;

	thread_printf(mtcp, mtcp->log_fp, 
			"CPU %d: Flushing remaining flows.\n", mtcp->ctx->cpu);

	for (i = 0; i < NUM_BINS; i++) {
		TAILQ_FOREACH(walk, &ht->ht_table[i], rcvvar->he_link) {
			thread_printf(mtcp, mtcp->log_fp, 
					"CPU %d: Destroying stream %d\n", mtcp->ctx->cpu, walk->id);
#ifdef DUMP_STREAM
			DumpStream(mtcp, walk);
#endif
			DestroyTCPStream(mtcp, walk);
			cnt++;
		}
	}

	return cnt;
}
#endif
/*----------------------------------------------------------------------------*/
static void 
InterruptApplication(mtcp_manager_t mtcp)
{
	/* interrupt if the mtcp_epoll_wait() is waiting */
	if (mtcp->ep) {
		pthread_mutex_lock(&mtcp->ep->epoll_lock);
		if (mtcp->ep->waiting) {
			pthread_cond_signal(&mtcp->ep->epoll_cond);
		}
		pthread_mutex_unlock(&mtcp->ep->epoll_lock);
	}
	/* interrupt if the accept() is waiting */
	if (mtcp->listener) {
		if (mtcp->listener->socket) {
			pthread_mutex_lock(&mtcp->listener->accept_lock);
			if (!(mtcp->listener->socket->opts & MTCP_NONBLOCK)) {
				pthread_cond_signal(&mtcp->listener->accept_cond);
			}
			pthread_mutex_unlock(&mtcp->listener->accept_lock);
		}
	}
}
/*----------------------------------------------------------------------------*/
void 
RunPassiveLoop(mtcp_manager_t mtcp) 
{
	sem_wait(&g_done_sem[mtcp->ctx->cpu]);
	sem_destroy(&g_done_sem[mtcp->ctx->cpu]);
	return;
}
/*----------------------------------------------------------------------------*/
static void 
RunMainLoop(struct mtcp_thread_context *ctx)
{
	mtcp_manager_t mtcp = ctx->mtcp_manager;
	int i;
	int recv_cnt;

#if E_PSIO
	int rx_inf, tx_inf;
#endif

	struct timeval cur_ts = {0};
	uint32_t ts, ts_prev;

#if TIME_STAT
	struct timeval prev_ts, processing_ts, tcheck_ts, 
				   epoll_ts, handle_ts, xmit_ts, select_ts;
#endif
	int thresh;

	gettimeofday(&cur_ts, NULL);

#if E_PSIO
	/* do nothing */
#else
#if !USE_CHUNK_BUF
	/* create packet write chunk */
	InitWriteChunks(handle, ctx->w_chunk);
	for (i = 0; i < ETH_NUM; i++) {
		ctx->w_chunk[i].cnt = 0;
		ctx->w_off[i] = 0;
		ctx->w_cur_idx[i] = 0;
	}
#endif
#endif

	TRACE_DBG("CPU %d: mtcp thread running.\n", ctx->cpu);

#if TIME_STAT
	prev_ts = cur_ts;
	InitStatCounter(&mtcp->rtstat.round);
	InitStatCounter(&mtcp->rtstat.processing);
	InitStatCounter(&mtcp->rtstat.tcheck);
	InitStatCounter(&mtcp->rtstat.epoll);
	InitStatCounter(&mtcp->rtstat.handle);
	InitStatCounter(&mtcp->rtstat.xmit);
	InitStatCounter(&mtcp->rtstat.select);
#endif

	ts = ts_prev = 0;
	while ((!ctx->done || mtcp->flow_cnt) && !ctx->exit) {
		
		STAT_COUNT(mtcp->runstat.rounds);
		recv_cnt = 0;
			
#if E_PSIO
#if 0
		event.timeout = PS_SELECT_TIMEOUT;
		NID_ZERO(event.rx_nids);
		NID_ZERO(event.tx_nids);
		NID_ZERO(rx_avail);
		//NID_ZERO(tx_avail);
#endif
		gettimeofday(&cur_ts, NULL);
#if TIME_STAT
		/* measure the inter-round delay */
		UpdateStatCounter(&mtcp->rtstat.round, TimeDiffUs(&cur_ts, &prev_ts));
		prev_ts = cur_ts;
#endif

		ts = TIMEVAL_TO_TS(&cur_ts);
		mtcp->cur_ts = ts;

		for (rx_inf = 0; rx_inf < g_config.mos->netdev_table->num; rx_inf++) {

			recv_cnt = mtcp->iom->recv_pkts(ctx, rx_inf);
			STAT_COUNT(mtcp->runstat.rounds_rx_try);

			for (i = 0; i < recv_cnt; i++) {
				uint16_t len;
				uint8_t *pktbuf;
				pktbuf = mtcp->iom->get_rptr(mtcp->ctx, rx_inf, i, &len);
				ProcessPacket(mtcp, rx_inf, i, ts, pktbuf, len);
			}

#ifdef ENABLE_DPDKR
			mtcp->iom->send_pkts(ctx, rx_inf);
			continue;
#endif
		}
		STAT_COUNT(mtcp->runstat.rounds_rx);

#else /* E_PSIO */
		gettimeofday(&cur_ts, NULL);
		ts = TIMEVAL_TO_TS(&cur_ts);
		mtcp->cur_ts = ts;
		/*
		 * Read packets into a chunk from NIC
		 * chk_w_idx : next chunk index to write packets from NIC
		 */

		STAT_COUNT(mtcp->runstat.rounds_rx_try);
		chunk.cnt = PS_CHUNK_SIZE;
		recv_cnt = ps_recv_chunk(handle, &chunk);
		if (recv_cnt < 0) {
			if (errno != EAGAIN && errno != EINTR) {
				perror("ps_recv_chunk");
				assert(0);
			}
		}

		/* 
		 * Handling Packets 
		 * chk_r_idx : next chunk index to read and handle
		*/
		for (i = 0; i < recv_cnt; i++) {
			ProcessPacket(mtcp, chunk.queue.ifindex, ts, 
					(u_char *)(chunk.buf + chunk.info[i].offset), 
					chunk.info[i].len);
		}

		if (recv_cnt > 0)
			STAT_COUNT(mtcp->runstat.rounds_rx);
#endif /* E_PSIO */
#if TIME_STAT
		gettimeofday(&processing_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.processing, 
				TimeDiffUs(&processing_ts, &cur_ts));
#endif /* TIME_STAT */

		/* Handle user defined timeout */
		struct timer *walk, *tmp;
		for (walk = TAILQ_FIRST(&mtcp->timer_list); walk != NULL; walk = tmp) {
			tmp = TAILQ_NEXT(walk, timer_link);
			if (TIMEVAL_LT(&cur_ts, &walk->exp))
				break;

			struct mtcp_context mctx = {.cpu = ctx->cpu};
			walk->cb(&mctx, walk->id, 0, 0 /* FIXME */, NULL);
			DelTimer(mtcp, walk);
		}

		/* interaction with application */
		if (mtcp->flow_cnt > 0) {

			/* check retransmission timeout and timewait expire */
#if 0
			thresh = (int)mtcp->flow_cnt / (TS_TO_USEC(PER_STREAM_TCHECK));
			assert(thresh >= 0);
			if (thresh == 0)
				thresh = 1;
			if (recv_cnt > 0 && thresh > recv_cnt)
				thresh = recv_cnt;
#else
			thresh = g_config.mos->max_concurrency;
#endif

			/* Eunyoung, you may fix this later 
			 * if there is no rcv packet, we will send as much as possible
			 */
			if (thresh == -1)
				thresh = g_config.mos->max_concurrency;

			CheckRtmTimeout(mtcp, ts, thresh);
			CheckTimewaitExpire(mtcp, ts, thresh);

			if (g_config.mos->tcp_timeout > 0 && ts != ts_prev) {
				CheckConnectionTimeout(mtcp, ts, thresh);
			}

#if TIME_STAT
		}
		gettimeofday(&tcheck_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.tcheck, 
				TimeDiffUs(&tcheck_ts, &processing_ts));

		if (mtcp->flow_cnt > 0) {
#endif /* TIME_STAT */

		}

		/* 
		 * before flushing epoll events, call monitor events for
		 * all registered `read` events
		 */
		if (mtcp->num_msp > 0)
			/* call this when only a standalone monitor is running */
			FlushMonitorReadEvents(mtcp);
			
		/* if epoll is in use, flush all the queued events */
		if (mtcp->ep) {
			FlushBufferedReadEvents(mtcp);
			FlushEpollEvents(mtcp, ts);
		}
#if TIME_STAT
		gettimeofday(&epoll_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.epoll, 
				TimeDiffUs(&epoll_ts, &tcheck_ts));
#endif /* TIME_STAT */

		if (end_app_exists && mtcp->flow_cnt > 0) {
			/* handle stream queues  */
			HandleApplicationCalls(mtcp, ts);
		}

#ifdef ENABLE_DPDKR
		continue;
#endif

#if TIME_STAT
		gettimeofday(&handle_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.handle, 
				TimeDiffUs(&handle_ts, &epoll_ts));
#endif /* TIME_STAT */

		WritePacketsToChunks(mtcp, ts);

		/* send packets from write buffer */
#if E_PSIO
		/* With E_PSIO, send until tx is available */
		int num_dev = g_config.mos->netdev_table->num;
		if (likely(mtcp->iom->send_pkts != NULL))
			for (tx_inf = 0; tx_inf < num_dev; tx_inf++) {
				mtcp->iom->send_pkts(ctx, tx_inf);
			}

#else /* E_PSIO */
		/* Without E_PSIO, try send chunks immediately */
		for (i = 0; i < g_config.mos->netdev_table->num; i++) {
#if USE_CHUNK_BUF
			/* in the case of using ps_send_chunk_buf() without E_PSIO */
			ret = FlushSendChunkBuf(mtcp, i);
#else
			/* if not using ps_send_chunk_buf() */
			ret = FlushWriteBuffer(ctx, i);
#endif
			if (ret < 0) {
				TRACE_ERROR("Failed to send chunks.\n");
			} else if (ret > 0) {
				STAT_COUNT(mtcp->runstat.rounds_tx);
			}
		}
#endif /* E_PSIO */

#if TIME_STAT
		gettimeofday(&xmit_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.xmit, 
				TimeDiffUs(&xmit_ts, &handle_ts));
#endif /* TIME_STAT */

		if (ts != ts_prev) {
			ts_prev = ts;
#ifdef NETSTAT
			if (ctx->cpu == printer) {
#ifdef RUN_ARP
				ARPTimer(mtcp, ts);
#endif
#ifdef NETSTAT
				PrintNetworkStats(mtcp, ts);
#endif
			}
#endif /* NETSTAT */
		}

		if (mtcp->iom->select)
			mtcp->iom->select(ctx);

		if (ctx->interrupt) {
			InterruptApplication(mtcp);
		}
	}

#if TESTING
	DestroyRemainingFlows(mtcp);
#endif

	TRACE_DBG("MTCP thread %d out of main loop.\n", ctx->cpu);
	/* flush logs */
	flush_log_data(mtcp);
	TRACE_DBG("MTCP thread %d flushed logs.\n", ctx->cpu);
	InterruptApplication(mtcp);
	TRACE_INFO("MTCP thread %d finished.\n", ctx->cpu);
}
/*----------------------------------------------------------------------------*/
struct mtcp_sender *
CreateMTCPSender(int ifidx)
{
	struct mtcp_sender *sender;

	sender = (struct mtcp_sender *)calloc(1, sizeof(struct mtcp_sender));
	if (!sender) {
		return NULL;
	}

	sender->ifidx = ifidx;

	TAILQ_INIT(&sender->control_list);
	TAILQ_INIT(&sender->send_list);
	TAILQ_INIT(&sender->ack_list);

	sender->control_list_cnt = 0;
	sender->send_list_cnt = 0;
	sender->ack_list_cnt = 0;

	return sender;
}
/*----------------------------------------------------------------------------*/
void 
DestroyMTCPSender(struct mtcp_sender *sender)
{
	free(sender);
}
/*----------------------------------------------------------------------------*/
static mtcp_manager_t 
InitializeMTCPManager(struct mtcp_thread_context* ctx)
{
	mtcp_manager_t mtcp;
	char log_name[MAX_FILE_NAME];
	int i;

	posix_seq_srand((unsigned)pthread_self());

	mtcp = (mtcp_manager_t)calloc(1, sizeof(struct mtcp_manager));
	if (!mtcp) {
		perror("malloc");
		CTRACE_ERROR("Failed to allocate mtcp_manager.\n");
		return NULL;
	}
	g_mtcp[ctx->cpu] = mtcp;

	mtcp->tcp_flow_table = CreateHashtable();
	if (!mtcp->tcp_flow_table) {
		CTRACE_ERROR("Falied to allocate tcp flow table.\n");
		return NULL;
	}

#ifdef HUGEPAGE
#define	IS_HUGEPAGE 1
#else
#define	IS_HUGEPAGE 0
#endif
	if (mon_app_exists) {
		/* initialize event callback */
#ifdef NEWEV
		InitEvent(mtcp);
#else
		InitEvent(mtcp, NUM_EV_TABLE);
#endif
	}

	if (!(mtcp->bufseg_pool = MPCreate(sizeof(tcpbufseg_t),
			sizeof(tcpbufseg_t) * g_config.mos->max_concurrency *
			((g_config.mos->rmem_size - 1) / UNITBUFSIZE + 1), 0))) {
		TRACE_ERROR("Failed to allocate ev_table pool\n");
		exit(0);
	}
	if (!(mtcp->sockent_pool = MPCreate(sizeof(struct sockent),
			sizeof(struct sockent) * g_config.mos->max_concurrency * 3, 0))) {
		TRACE_ERROR("Failed to allocate ev_table pool\n");
		exit(0);
	}
#ifdef USE_TIMER_POOL
	if (!(mtcp->timer_pool = MPCreate(sizeof(struct timer),
					  sizeof(struct timer) * g_config.mos->max_concurrency * 10, 0))) {
		TRACE_ERROR("Failed to allocate ev_table pool\n");
		exit(0);
	}
#endif
	mtcp->flow_pool = MPCreate(sizeof(tcp_stream),
								sizeof(tcp_stream) * g_config.mos->max_concurrency, IS_HUGEPAGE);
	if (!mtcp->flow_pool) {
		CTRACE_ERROR("Failed to allocate tcp flow pool.\n");
		return NULL;
	}
	mtcp->rv_pool = MPCreate(sizeof(struct tcp_recv_vars), 
			sizeof(struct tcp_recv_vars) * g_config.mos->max_concurrency, IS_HUGEPAGE);
	if (!mtcp->rv_pool) {
		CTRACE_ERROR("Failed to allocate tcp recv variable pool.\n");
		return NULL;
	}
	mtcp->sv_pool = MPCreate(sizeof(struct tcp_send_vars), 
			sizeof(struct tcp_send_vars) * g_config.mos->max_concurrency, IS_HUGEPAGE);
	if (!mtcp->sv_pool) {
		CTRACE_ERROR("Failed to allocate tcp send variable pool.\n");
		return NULL;
	}

	mtcp->rbm_snd = SBManagerCreate(g_config.mos->wmem_size, g_config.mos->no_ring_buffers, 
					g_config.mos->max_concurrency);
	if (!mtcp->rbm_snd) {
		CTRACE_ERROR("Failed to create send ring buffer.\n");
		return NULL;
	}

	mtcp->smap = (socket_map_t)calloc(g_config.mos->max_concurrency, sizeof(struct socket_map));
	if (!mtcp->smap) {
		perror("calloc");
		CTRACE_ERROR("Failed to allocate memory for stream map.\n");
		return NULL;
	}
	
	if (mon_app_exists) {
		mtcp->msmap = (socket_map_t)calloc(g_config.mos->max_concurrency, sizeof(struct socket_map));
		if (!mtcp->msmap) {
			perror("calloc");
			CTRACE_ERROR("Failed to allocate memory for monitor stream map.\n");
			return NULL;
		}
		
		for (i = 0; i < g_config.mos->max_concurrency; i++) {
			mtcp->msmap[i].monitor_stream = calloc(1, sizeof(struct mon_stream));
			if (!mtcp->msmap[i].monitor_stream) {
				perror("calloc");
				CTRACE_ERROR("Failed to allocate memory for monitr stream map\n");
				return NULL;
			}
		}
	}

	TAILQ_INIT(&mtcp->timer_list);
	TAILQ_INIT(&mtcp->monitors);

	TAILQ_INIT(&mtcp->free_smap);
	for (i = 0; i < g_config.mos->max_concurrency; i++) {
		mtcp->smap[i].id = i;
		mtcp->smap[i].socktype = MOS_SOCK_UNUSED;
		memset(&mtcp->smap[i].saddr, 0, sizeof(struct sockaddr_in));
		mtcp->smap[i].stream = NULL;
		TAILQ_INSERT_TAIL(&mtcp->free_smap, &mtcp->smap[i], link);
	}

	if (mon_app_exists) {
		TAILQ_INIT(&mtcp->free_msmap);
		for (i = 0; i < g_config.mos->max_concurrency; i++) {
			mtcp->msmap[i].id = i;
			mtcp->msmap[i].socktype = MOS_SOCK_UNUSED;
			memset(&mtcp->msmap[i].saddr, 0, sizeof(struct sockaddr_in));
			TAILQ_INSERT_TAIL(&mtcp->free_msmap, &mtcp->msmap[i], link);
		}
	}

	mtcp->ctx = ctx;
	mtcp->ep = NULL;

	snprintf(log_name, MAX_FILE_NAME, "%s/"LOG_FILE_NAME"_%d", 
			g_config.mos->mos_log, ctx->cpu);
	mtcp->log_fp = fopen(log_name, "w+");
	if (!mtcp->log_fp) {
		perror("fopen");
		CTRACE_ERROR("Failed to create file for logging. (%s)\n", log_name);
		return NULL;
	}
	mtcp->sp_fd = g_logctx[ctx->cpu]->pair_sp_fd;
	mtcp->logger = g_logctx[ctx->cpu];
	
	mtcp->connectq = CreateStreamQueue(BACKLOG_SIZE);
	if (!mtcp->connectq) {
		CTRACE_ERROR("Failed to create connect queue.\n");
		return NULL;
	}
	mtcp->sendq = CreateStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->sendq) {
		CTRACE_ERROR("Failed to create send queue.\n");
		return NULL;
	}
	mtcp->ackq = CreateStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->ackq) {
		CTRACE_ERROR("Failed to create ack queue.\n");
		return NULL;
	}
	mtcp->closeq = CreateStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->closeq) {
		CTRACE_ERROR("Failed to create close queue.\n");
		return NULL;
	}
	mtcp->closeq_int = CreateInternalStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->closeq_int) {
		CTRACE_ERROR("Failed to create close queue.\n");
		return NULL;
	}
	mtcp->resetq = CreateStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->resetq) {
		CTRACE_ERROR("Failed to create reset queue.\n");
		return NULL;
	}
	mtcp->resetq_int = CreateInternalStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->resetq_int) {
		CTRACE_ERROR("Failed to create reset queue.\n");
		return NULL;
	}
	mtcp->destroyq = CreateStreamQueue(g_config.mos->max_concurrency);
	if (!mtcp->destroyq) {
		CTRACE_ERROR("Failed to create destroy queue.\n");
		return NULL;
	}

	mtcp->g_sender = CreateMTCPSender(-1);
	if (!mtcp->g_sender) {
		CTRACE_ERROR("Failed to create global sender structure.\n");
		return NULL;
	}
	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		mtcp->n_sender[i] = CreateMTCPSender(i);
		if (!mtcp->n_sender[i]) {
			CTRACE_ERROR("Failed to create per-nic sender structure.\n");
			return NULL;
		}
	}
		
	mtcp->rto_store = InitRTOHashstore();
	TAILQ_INIT(&mtcp->timewait_list);
	TAILQ_INIT(&mtcp->timeout_list);

	return mtcp;
}
/*----------------------------------------------------------------------------*/
static void *
MTCPRunThread(void *arg)
{
	mctx_t mctx = (mctx_t)arg;
	int cpu = mctx->cpu;
	int working;
	struct mtcp_manager *mtcp;
	struct mtcp_thread_context *ctx;

	/* affinitize the thread to this core first */
	mtcp_core_affinitize(cpu);

	/* memory alloc after core affinitization would use local memory
	   most time */
	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		perror("calloc");
		TRACE_ERROR("Failed to calloc mtcp context.\n");
		exit(-1);
	}
	ctx->thread = pthread_self();
	ctx->cpu = cpu;
	mtcp = ctx->mtcp_manager = InitializeMTCPManager(ctx);
	if (!mtcp) {
		TRACE_ERROR("Failed to initialize mtcp manager.\n");
		exit(-1);
	}

	/* assign mtcp context's underlying I/O module */
	mtcp->iom = current_iomodule_func;

	/* I/O initializing */
	if (mtcp->iom->init_handle)
		mtcp->iom->init_handle(ctx);

	if (pthread_mutex_init(&ctx->flow_pool_lock, NULL)) {
		perror("pthread_mutex_init of ctx->flow_pool_lock\n");
		exit(-1);
	}
	
	if (pthread_mutex_init(&ctx->socket_pool_lock, NULL)) {
		perror("pthread_mutex_init of ctx->socket_pool_lock\n");
		exit(-1);
	}

	SQ_LOCK_INIT(&ctx->connect_lock, "ctx->connect_lock", exit(-1));
	SQ_LOCK_INIT(&ctx->close_lock, "ctx->close_lock", exit(-1));
	SQ_LOCK_INIT(&ctx->reset_lock, "ctx->reset_lock", exit(-1));
	SQ_LOCK_INIT(&ctx->sendq_lock, "ctx->sendq_lock", exit(-1));
	SQ_LOCK_INIT(&ctx->ackq_lock, "ctx->ackq_lock", exit(-1));
	SQ_LOCK_INIT(&ctx->destroyq_lock, "ctx->destroyq_lock", exit(-1));

	/* remember this context pointer for signal processing */
	g_pctx[cpu] = ctx;
	mlockall(MCL_CURRENT);

	// attach (nic device, queue)
	working = AttachDevice(ctx);
	if (working != 0) {
		sem_post(&g_init_sem[ctx->cpu]);
		TRACE_DBG("MTCP thread %d finished. Not attached any device\n", ctx->cpu);
		pthread_exit(NULL);
	}
	
	TRACE_DBG("CPU %d: initialization finished.\n", cpu);
	sem_post(&g_init_sem[ctx->cpu]);

	/* start the main loop */
	RunMainLoop(ctx);
	
	TRACE_DBG("MTCP thread %d finished.\n", ctx->cpu);
	
	/* signaling mTCP thread is done */
	sem_post(&g_done_sem[mctx->cpu]);
	
	//pthread_exit(NULL);
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DPDK
static int MTCPDPDKRunThread(void *arg)
{
	MTCPRunThread(arg);
	return 0;
}
#endif /* !ENABLE_DPDK */
/*----------------------------------------------------------------------------*/
mctx_t 
mtcp_create_context(int cpu)
{
	mctx_t mctx;
	int ret;

	if (cpu >=  g_config.mos->num_cores) {
		TRACE_ERROR("Failed initialize new mtcp context. "
					"Requested cpu id %d exceed the number of cores %d configured to use.\n",
					cpu, g_config.mos->num_cores);
		return NULL;
	}

        /* check if mtcp_create_context() was already initialized */
        if (g_logctx[cpu] != NULL) {
                TRACE_ERROR("%s was already initialized before!\n",
                            __FUNCTION__);
                return NULL;
        }

	ret = sem_init(&g_init_sem[cpu], 0, 0);
	if (ret) {
		TRACE_ERROR("Failed initialize init_sem.\n");
		return NULL;
	}
	
	ret = sem_init(&g_done_sem[cpu], 0, 0);
	if (ret) {
		TRACE_ERROR("Failed initialize done_sem.\n");
		return NULL;
	}

	mctx = (mctx_t)calloc(1, sizeof(struct mtcp_context));
	if (!mctx) {
		TRACE_ERROR("Failed to allocate memory for mtcp_context.\n");
		return NULL;
	}
	mctx->cpu = cpu;
	g_ctx[cpu] = mctx;

	/* initialize logger */
	g_logctx[cpu] = (struct log_thread_context *)
			calloc(1, sizeof(struct log_thread_context));
	if (!g_logctx[cpu]) {
		perror("malloc");
		TRACE_ERROR("Failed to allocate memory for log thread context.\n");
		return NULL;
	}
	InitLogThreadContext(g_logctx[cpu], cpu);
	if (pthread_create(&log_thread[cpu], 
			   NULL, ThreadLogMain, (void *)g_logctx[cpu])) {
		perror("pthread_create");
		TRACE_ERROR("Failed to create log thread\n");
		return NULL;
	}

#ifdef ENABLE_DPDK	
	/* Wake up mTCP threads (wake up I/O threads) */
	if (current_iomodule_func == &dpdk_module_func) {
		int master;
		master = rte_get_master_lcore();
		if (master == cpu) {
			lcore_config[master].ret = 0;
			lcore_config[master].state = FINISHED;
			if (pthread_create(&g_thread[cpu], 
					   NULL, MTCPRunThread, (void *)mctx) != 0) {
				TRACE_ERROR("pthread_create of mtcp thread failed!\n");
				return NULL;
			}
		} else
			rte_eal_remote_launch(MTCPDPDKRunThread, mctx, cpu);
	} else 
#endif /* !ENABLE_DPDK */
		{
			if (pthread_create(&g_thread[cpu], 
					   NULL, MTCPRunThread, (void *)mctx) != 0) {
				TRACE_ERROR("pthread_create of mtcp thread failed!\n");
				return NULL;
			}
		}
	
	sem_wait(&g_init_sem[cpu]);
	sem_destroy(&g_init_sem[cpu]);

	running[cpu] = TRUE;

#ifdef NETSTAT
#if NETSTAT_TOTAL
	if (printer < 0) {
		printer = cpu;
		TRACE_INFO("CPU %d is in charge of printing stats.\n", printer);
	}
#endif
#endif
		
	return mctx;
}
/*----------------------------------------------------------------------------*/
/**
 * TODO: It currently always returns 0. Add appropriate error return values
 */
int 
mtcp_destroy_context(mctx_t mctx)
{
	struct mtcp_thread_context *ctx = g_pctx[mctx->cpu];
	struct mtcp_manager *mtcp = ctx->mtcp_manager;
	struct log_thread_context *log_ctx = mtcp->logger;
	int ret, i;

	TRACE_DBG("CPU %d: mtcp_destroy_context()\n", mctx->cpu);

	/* close all stream sockets that are still open */
	if (!ctx->exit) {
		for (i = 0; i < g_config.mos->max_concurrency; i++) {
			if (mtcp->smap[i].socktype == MOS_SOCK_STREAM) {
				TRACE_DBG("Closing remaining socket %d (%s)\n", 
						i, TCPStateToString(mtcp->smap[i].stream));
#ifdef DUMP_STREAM
				DumpStream(mtcp, mtcp->smap[i].stream);
#endif
				mtcp_close(mctx, i);
			}
		}
	}

	ctx->done = 1;

	//pthread_kill(g_thread[mctx->cpu], SIGINT);
#ifdef ENABLE_DPDK
	ctx->exit = 1;
	/* XXX - dpdk logic changes */
	if (current_iomodule_func == &dpdk_module_func) {
		int master = rte_get_master_lcore();
		if (master == mctx->cpu)
			pthread_join(g_thread[mctx->cpu], NULL);
		else
			rte_eal_wait_lcore(mctx->cpu);
	} else 
#endif /* !ENABLE_DPDK */
		{
			pthread_join(g_thread[mctx->cpu], NULL);
		}
	
	TRACE_INFO("MTCP thread %d joined.\n", mctx->cpu);
	running[mctx->cpu] = FALSE;

#ifdef NETSTAT
#if NETSTAT_TOTAL
	if (printer == mctx->cpu) {
		for (i = 0; i < num_cpus; i++) {
			if (i != mctx->cpu && running[i]) {
				printer = i;
				break;
			}
		}
	}
#endif
#endif

	log_ctx->done = 1;
	ret = write(log_ctx->pair_sp_fd, "F", 1);
	if (ret != 1)
		TRACE_ERROR("CPU %d: Fail to signal socket pair\n", mctx->cpu);
		
	pthread_join(log_thread[ctx->cpu], NULL);
	fclose(mtcp->log_fp);
	TRACE_LOG("Log thread %d joined.\n", mctx->cpu);

	if (mtcp->connectq) {
		DestroyStreamQueue(mtcp->connectq);
		mtcp->connectq = NULL;
	}
	if (mtcp->sendq) {
		DestroyStreamQueue(mtcp->sendq);
		mtcp->sendq = NULL;
	}
	if (mtcp->ackq) {
		DestroyStreamQueue(mtcp->ackq);
		mtcp->ackq = NULL;
	}
	if (mtcp->closeq) {
		DestroyStreamQueue(mtcp->closeq);
		mtcp->closeq = NULL;
	}
	if (mtcp->closeq_int) {
		DestroyInternalStreamQueue(mtcp->closeq_int);
		mtcp->closeq_int = NULL;
	}
	if (mtcp->resetq) {
		DestroyStreamQueue(mtcp->resetq);
		mtcp->resetq = NULL;
	}
	if (mtcp->resetq_int) {
		DestroyInternalStreamQueue(mtcp->resetq_int);
		mtcp->resetq_int = NULL;
	}
	if (mtcp->destroyq) {
		DestroyStreamQueue(mtcp->destroyq);
		mtcp->destroyq = NULL;
	}

	DestroyMTCPSender(mtcp->g_sender);
	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		DestroyMTCPSender(mtcp->n_sender[i]);
	}

	MPDestroy(mtcp->rv_pool);
	MPDestroy(mtcp->sv_pool);
	MPDestroy(mtcp->flow_pool);
	
	if (mtcp->ap) {
		DestroyAddressPool(mtcp->ap);
	}

	SQ_LOCK_DESTROY(&ctx->connect_lock);
	SQ_LOCK_DESTROY(&ctx->close_lock);
	SQ_LOCK_DESTROY(&ctx->reset_lock);
	SQ_LOCK_DESTROY(&ctx->sendq_lock);
	SQ_LOCK_DESTROY(&ctx->ackq_lock);
	SQ_LOCK_DESTROY(&ctx->destroyq_lock);
	
	//TRACE_INFO("MTCP thread %d destroyed.\n", mctx->cpu);
	if (mtcp->iom->destroy_handle)
		mtcp->iom->destroy_handle(ctx);
	free(ctx);
	free(mctx);

	return 0;
}
/*----------------------------------------------------------------------------*/
mtcp_sighandler_t 
mtcp_register_signal(int signum, mtcp_sighandler_t handler)
{
	mtcp_sighandler_t prev;

	if (signum == SIGINT) {
		prev = app_signal_handler;
		app_signal_handler = handler;
	} else {
		if ((prev = signal(signum, handler)) == SIG_ERR) {
			perror("signal");
			return SIG_ERR;
		}
	}

	return prev;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_getconf(struct mtcp_conf *conf)
{
	int i, j;

	if (!conf)
		return -1;

	conf->num_cores = g_config.mos->num_cores;
	conf->max_concurrency = g_config.mos->max_concurrency;
	conf->cpu_mask = g_config.mos->cpu_mask;

	conf->rcvbuf_size = g_config.mos->rmem_size;
	conf->sndbuf_size = g_config.mos->wmem_size;

	conf->tcp_timewait = g_config.mos->tcp_tw_interval;
	conf->tcp_timeout = g_config.mos->tcp_timeout;

	i = 0;
	struct conf_block *bwalk;
	TAILQ_FOREACH(bwalk, &g_config.app_blkh, link) {
		struct app_conf *app_conf = (struct app_conf *)bwalk->conf;
		for (j = 0; j < app_conf->app_argc; j++)
			conf->app_argv[i][j] = app_conf->app_argv[j];
		conf->app_argc[i] = app_conf->app_argc;
		conf->app_cpu_mask[i] = app_conf->cpu_mask;
		i++;
	}
	conf->num_app = i;

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_setconf(const struct mtcp_conf *conf)
{
	if (!conf)
		return -1;

	g_config.mos->num_cores = conf->num_cores;
	g_config.mos->max_concurrency = conf->max_concurrency;

	g_config.mos->rmem_size = conf->rcvbuf_size;
	g_config.mos->wmem_size = conf->sndbuf_size;

	g_config.mos->tcp_tw_interval = conf->tcp_timewait;
	g_config.mos->tcp_timeout = conf->tcp_timeout;

	TRACE_CONFIG("Configuration updated by mtcp_setconf().\n");
	//PrintConfiguration();

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_init(const char *config_file)
{
	int i;
	int ret;

	if (geteuid()) {
		TRACE_CONFIG("[CAUTION] Run as root if mlock is necessary.\n");
#if defined(ENABLE_DPDK) || defined(ENABLE_DPDKR) || defined(ENABLE_NETMAP)
		TRACE_CONFIG("[CAUTION] Run the app as root!\n");
		exit(EXIT_FAILURE);
#endif
	}

	/* getting cpu and NIC */
	num_cpus = GetNumCPUs();
	assert(num_cpus >= 1);
	for (i = 0; i < num_cpus; i++) {
		g_mtcp[i] = NULL;
		running[i] = FALSE;
		sigint_cnt[i] = 0;
	}

	ret = LoadConfigurationUpperHalf(config_file);
	if (ret) {
		TRACE_CONFIG("Error occured while loading configuration.\n");
		return -1;
	}

#if defined(ENABLE_PSIO)
	current_iomodule_func = &ps_module_func;
#elif defined(ENABLE_DPDK)
	current_iomodule_func = &dpdk_module_func;
#elif defined(ENABLE_PCAP)
	current_iomodule_func = &pcap_module_func;
#elif defined(ENABLE_DPDKR)
	current_iomodule_func = &dpdkr_module_func;
#elif defined(ENABLE_NETMAP)
	current_iomodule_func = &netmap_module_func;
#endif

	if (current_iomodule_func->load_module_upper_half)
		current_iomodule_func->load_module_upper_half();

	LoadConfigurationLowerHalf();

	//PrintConfiguration();

	for (i = 0; i < g_config.mos->netdev_table->num; i++) {
		ap[i] = CreateAddressPool(g_config.mos->netdev_table->ent[i]->ip_addr, 1);
		if (!ap[i]) {
			TRACE_CONFIG("Error occured while create address pool[%d]\n",
				     i);
			return -1;
		}
	}

	//PrintInterfaceInfo();
	//PrintRoutingTable();
	//PrintARPTable();
	InitARPTable();

	if (signal(SIGUSR1, HandleSignal) == SIG_ERR) {
		perror("signal, SIGUSR1");
		return -1;
	}
	if (signal(SIGINT, HandleSignal) == SIG_ERR) {
		perror("signal, SIGINT");
		return -1;
	}
	app_signal_handler = NULL;

	printf("load_module(): %p\n", current_iomodule_func);
	/* load system-wide io module specs */
	if (current_iomodule_func->load_module_lower_half)
		current_iomodule_func->load_module_lower_half();

	GlobInitEvent();

	PrintConf(&g_config);

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_destroy()
{
	int i;

	/* wait until all threads are closed */
	for (i = 0; i < num_cpus; i++) {
		if (running[i]) {
			if (pthread_join(g_thread[i], NULL) != 0)
				return -1;
		}
	}

	for (i = 0; i < g_config.mos->netdev_table->num; i++)
		DestroyAddressPool(ap[i]);

	TRACE_INFO("All MTCP threads are joined.\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
