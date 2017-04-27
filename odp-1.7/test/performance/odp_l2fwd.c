/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * @example odp_l2fwd.c  ODP basic forwarding application
 */

/** enable strtok */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <test_debug.h>

#include <odp.h>
#include <odp/helper/linux.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>

/** @def MAX_WORKERS
 * @brief Maximum number of worker threads
 */
#define MAX_WORKERS            32

/** @def SHM_PKT_POOL_SIZE
 * @brief Size of the shared memory block
 */
#define SHM_PKT_POOL_SIZE      8192

/** @def SHM_PKT_POOL_BUF_SIZE
 * @brief Buffer size of the packet pool buffer
 */
#define SHM_PKT_POOL_BUF_SIZE  1856

/** @def MAX_PKT_BURST
 * @brief Maximum number of packet in a burst
 */
#define MAX_PKT_BURST          32

/** Maximum number of pktio queues per interface */
#define MAX_QUEUES             32

/** Maximum number of pktio interfaces */
#define MAX_PKTIOS             8

/**
 * Packet input mode
 */
typedef enum pkt_in_mode_t {
	DIRECT_RECV,
	PLAIN_QUEUE,
	SCHED_PARALLEL,
	SCHED_ATOMIC,
	SCHED_ORDERED,
} pkt_in_mode_t;

/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ? \
			    strrchr((file_name), '/') + 1 : (file_name))
/**
 * Parsed command line application arguments
 */
typedef struct {
	int cpu_count;
	int if_count;		/**< Number of interfaces to be used */
	int num_workers;	/**< Number of worker threads */
	char **if_names;	/**< Array of pointers to interface names */
	pkt_in_mode_t mode;	/**< Packet input mode */
	int time;		/**< Time in seconds to run. */
	int accuracy;		/**< Number of seconds to get and print statistics */
	char *if_str;		/**< Storage for interface names */
	int dst_change;		/**< Change destination eth addresses */
	int src_change;		/**< Change source eth addresses */
	int error_check;        /**< Check packet errors */
} appl_args_t;

static int exit_threads;	/**< Break workers loop if set to 1 */

/**
 * Statistics
 */
typedef union {
	struct {
		/** Number of forwarded packets */
		uint64_t packets;
		/** Packets dropped due to receive error */
		uint64_t rx_drops;
		/** Packets dropped due to transmit error */
		uint64_t tx_drops;
	} s;

	uint8_t padding[ODP_CACHE_LINE_SIZE];
} stats_t ODP_ALIGNED_CACHE;

/**
 * Thread specific arguments
 */
typedef struct thread_args_t {
	int thr_idx;
	int num_pktio;

	struct {
		odp_pktio_t rx_pktio;
		odp_pktio_t tx_pktio;
		odp_pktin_queue_t pktin;
		odp_pktout_queue_t pktout;
		odp_queue_t rx_queue;
		int rx_idx;
		int tx_idx;
		int rx_queue_idx;
		int tx_queue_idx;
	} pktio[MAX_PKTIOS];

	stats_t *stats;	/**< Pointer to per thread stats */
} thread_args_t;

/**
 * Grouping of all global data
 */
typedef struct {
	/** Per thread packet stats */
	stats_t stats[MAX_WORKERS];
	/** Application (parsed) arguments */
	appl_args_t appl;
	/** Thread specific arguments */
	thread_args_t thread[MAX_WORKERS];
	/** Table of port ethernet addresses */
	odph_ethaddr_t port_eth_addr[MAX_PKTIOS];
	/** Table of dst ethernet addresses */
	odph_ethaddr_t dst_eth_addr[MAX_PKTIOS];
	/** Table of dst ports */
	int dst_port[MAX_PKTIOS];
	/** Table of pktio handles */
	struct {
		odp_pktio_t pktio;
		odp_pktin_queue_t pktin[MAX_QUEUES];
		odp_pktout_queue_t pktout[MAX_QUEUES];
		odp_queue_t rx_q[MAX_QUEUES];
		int num_rx_thr;
		int num_tx_thr;
		int num_rx_queue;
		int num_tx_queue;
		int next_rx_queue;
		int next_tx_queue;
	} pktios[MAX_PKTIOS];
} args_t;

/** Global pointer to args */
static args_t *gbl_args;
/** Global barrier to synchronize main and workers */
static odp_barrier_t barrier;

/**
 * Lookup the destination port for a given packet
 *
 * @param pkt  ODP packet handle
 */
static inline int lookup_dest_port(odp_packet_t pkt)
{
	int i, src_idx;
	odp_pktio_t pktio_src;

	pktio_src = odp_packet_input(pkt);

	for (src_idx = -1, i = 0; gbl_args->pktios[i].pktio
				  != ODP_PKTIO_INVALID; ++i)
		if (gbl_args->pktios[i].pktio == pktio_src)
			src_idx = i;

	if (src_idx == -1)
		LOG_ABORT("Failed to determine pktio input\n");

	return gbl_args->dst_port[src_idx];
}

/**
 * Drop packets which input parsing marked as containing errors.
 *
 * Frees packets with error and modifies pkt_tbl[] to only contain packets with
 * no detected errors.
 *
 * @param pkt_tbl  Array of packets
 * @param num      Number of packets in pkt_tbl[]
 *
 * @return Number of packets dropped
 */
static inline int drop_err_pkts(odp_packet_t pkt_tbl[], unsigned num)
{
	odp_packet_t pkt;
	unsigned dropped = 0;
	unsigned i, j;

	for (i = 0, j = 0; i < num; ++i) {
		pkt = pkt_tbl[i];

		if (odp_unlikely(odp_packet_has_error(pkt))) {
			odp_packet_free(pkt); /* Drop */
			dropped++;
		} else if (odp_unlikely(i != j++)) {
			pkt_tbl[j - 1] = pkt;
		}
	}

	return dropped;
}

/**
 * Fill packets' eth addresses according to the destination port
 *
 * @param pkt_tbl  Array of packets
 * @param num      Number of packets in the array
 * @param dst_port Destination port
 */
static inline void fill_eth_addrs(odp_packet_t pkt_tbl[],
				  unsigned num, int dst_port)
{
	odp_packet_t pkt;
	odph_ethhdr_t *eth;
	unsigned i;

	if (!gbl_args->appl.dst_change && !gbl_args->appl.src_change)
		return;

	for (i = 0; i < num; ++i) {
		pkt = pkt_tbl[i];
		if (odp_packet_has_eth(pkt)) {
			eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);

			if (gbl_args->appl.src_change)
				eth->src = gbl_args->port_eth_addr[dst_port];

			if (gbl_args->appl.dst_change)
				eth->dst = gbl_args->dst_eth_addr[dst_port];
		}
	}
}

/**
 * Packet IO worker thread using scheduled queues
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *run_worker_sched_mode(void *arg)
{
	odp_event_t  ev_tbl[MAX_PKT_BURST];
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	int pkts;
	int thr;
	uint64_t wait;
	int dst_idx;
	int thr_idx;
	int i;
	odp_pktout_queue_t pktout[MAX_PKTIOS];
	thread_args_t *thr_args = arg;
	stats_t *stats = thr_args->stats;

	thr = odp_thread_id();
	thr_idx = thr_args->thr_idx;

	memset(pktout, 0, sizeof(pktout));
	for (i = 0; i < gbl_args->appl.if_count; i++) {
		if (gbl_args->pktios[i].num_tx_queue ==
		    gbl_args->appl.num_workers)
			pktout[i] = gbl_args->pktios[i].pktout[thr_idx];
		else if (gbl_args->pktios[i].num_tx_queue == 1)
			pktout[i] = gbl_args->pktios[i].pktout[0];
		else
			LOG_ABORT("Bad number of output queues %i\n", i);
	}

	printf("[%02i] SCHEDULED QUEUE mode\n", thr);
	odp_barrier_wait(&barrier);

	wait = odp_schedule_wait_time(ODP_TIME_MSEC_IN_NS * 100);

	/* Loop packets */
	while (!exit_threads) {
		int sent;
		unsigned tx_drops;

		pkts = odp_schedule_multi(NULL, wait, ev_tbl, MAX_PKT_BURST);

		if (pkts <= 0)
			continue;

		for (i = 0; i < pkts; i++)
			pkt_tbl[i] = odp_packet_from_event(ev_tbl[i]);

		if (gbl_args->appl.error_check) {
			int rx_drops;

			/* Drop packets with errors */
			rx_drops = drop_err_pkts(pkt_tbl, pkts);

			if (odp_unlikely(rx_drops)) {
				stats->s.rx_drops += rx_drops;
				if (pkts == rx_drops)
					continue;

				pkts -= rx_drops;
			}
		}

		/* packets from the same queue are from the same interface */
		dst_idx = lookup_dest_port(pkt_tbl[0]);
		fill_eth_addrs(pkt_tbl, pkts, dst_idx);
		sent = odp_pktio_send_queue(pktout[dst_idx], pkt_tbl, pkts);

		sent     = odp_unlikely(sent < 0) ? 0 : sent;
		tx_drops = pkts - sent;

		if (odp_unlikely(tx_drops)) {
			stats->s.tx_drops += tx_drops;

			/* Drop rejected packets */
			for (i = sent; i < pkts; i++)
				odp_packet_free(pkt_tbl[i]);
		}

		stats->s.packets += pkts;
	}

	/* Make sure that latest stat writes are visible to other threads */
	odp_mb_full();

	return NULL;
}

/**
 * Packet IO worker thread using plain queues
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *run_worker_plain_queue_mode(void *arg)
{
	int thr;
	int pkts;
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	int dst_idx, num_pktio;
	odp_queue_t queue;
	odp_pktout_queue_t pktout;
	int pktio = 0;
	thread_args_t *thr_args = arg;
	stats_t *stats = thr_args->stats;

	thr = odp_thread_id();

	num_pktio = thr_args->num_pktio;
	dst_idx   = thr_args->pktio[pktio].tx_idx;
	queue     = thr_args->pktio[pktio].rx_queue;
	pktout    = thr_args->pktio[pktio].pktout;

	printf("[%02i] num pktios %i, PLAIN QUEUE mode\n", thr, num_pktio);
	odp_barrier_wait(&barrier);

	/* Loop packets */
	while (!exit_threads) {
		int sent;
		unsigned tx_drops;
		odp_event_t event[MAX_PKT_BURST];
		int i;

		if (num_pktio > 1) {
			dst_idx   = thr_args->pktio[pktio].tx_idx;
			queue     = thr_args->pktio[pktio].rx_queue;
			pktout    = thr_args->pktio[pktio].pktout;
			pktio++;
			if (pktio == num_pktio)
				pktio = 0;
		}

		pkts = odp_queue_deq_multi(queue, event, MAX_PKT_BURST);
		if (odp_unlikely(pkts <= 0))
			continue;

		for (i = 0; i < pkts; i++)
			pkt_tbl[i] = odp_packet_from_event(event[i]);

		if (gbl_args->appl.error_check) {
			int rx_drops;

			/* Drop packets with errors */
			rx_drops = drop_err_pkts(pkt_tbl, pkts);

			if (odp_unlikely(rx_drops)) {
				stats->s.rx_drops += rx_drops;
				if (pkts == rx_drops)
					continue;

				pkts -= rx_drops;
			}
		}

		fill_eth_addrs(pkt_tbl, pkts, dst_idx);

		sent = odp_pktio_send_queue(pktout, pkt_tbl, pkts);

		sent     = odp_unlikely(sent < 0) ? 0 : sent;
		tx_drops = pkts - sent;

		if (odp_unlikely(tx_drops)) {
			int i;

			stats->s.tx_drops += tx_drops;

			/* Drop rejected packets */
			for (i = sent; i < pkts; i++)
				odp_packet_free(pkt_tbl[i]);
		}

		stats->s.packets += pkts;
	}

	/* Make sure that latest stat writes are visible to other threads */
	odp_mb_full();

	return NULL;
}

/**
 * Packet IO worker thread accessing IO resources directly
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *run_worker_direct_mode(void *arg)
{
	int thr;
	int pkts;
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	int dst_idx, num_pktio;
	odp_pktin_queue_t pktin;
	odp_pktout_queue_t pktout;
	int pktio = 0;
	thread_args_t *thr_args = arg;
	stats_t *stats = thr_args->stats;

	thr = odp_thread_id();

	num_pktio = thr_args->num_pktio;
	dst_idx   = thr_args->pktio[pktio].tx_idx;
	pktin     = thr_args->pktio[pktio].pktin;
	pktout    = thr_args->pktio[pktio].pktout;

	printf("[%02i] num pktios %i, DIRECT RECV mode\n", thr, num_pktio);
	odp_barrier_wait(&barrier);

	/* Loop packets */
	while (!exit_threads) {
		int sent;
		unsigned tx_drops;

		if (num_pktio > 1) {
			dst_idx   = thr_args->pktio[pktio].tx_idx;
			pktin     = thr_args->pktio[pktio].pktin;
			pktout    = thr_args->pktio[pktio].pktout;
			pktio++;
			if (pktio == num_pktio)
				pktio = 0;
		}

		pkts = odp_pktio_recv_queue(pktin, pkt_tbl, MAX_PKT_BURST);
		if (odp_unlikely(pkts <= 0))
			continue;

		if (gbl_args->appl.error_check) {
			int rx_drops;

			/* Drop packets with errors */
			rx_drops = drop_err_pkts(pkt_tbl, pkts);

			if (odp_unlikely(rx_drops)) {
				stats->s.rx_drops += rx_drops;
				if (pkts == rx_drops)
					continue;

				pkts -= rx_drops;
			}
		}

		fill_eth_addrs(pkt_tbl, pkts, dst_idx);

		sent = odp_pktio_send_queue(pktout, pkt_tbl, pkts);

		sent     = odp_unlikely(sent < 0) ? 0 : sent;
		tx_drops = pkts - sent;

		if (odp_unlikely(tx_drops)) {
			int i;

			stats->s.tx_drops += tx_drops;

			/* Drop rejected packets */
			for (i = sent; i < pkts; i++)
				odp_packet_free(pkt_tbl[i]);
		}

		stats->s.packets += pkts;
	}

	/* Make sure that latest stat writes are visible to other threads */
	odp_mb_full();

	return NULL;
}

/**
 * Create a pktio handle, optionally associating a default input queue.
 *
 * @param dev   Name of device to open
 * @param index Pktio index
 * @param pool  Pool to associate with device for packet RX/TX
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static int create_pktio(const char *dev, int idx, int num_rx, int num_tx,
			odp_pool_t pool)
{
	odp_pktio_t pktio;
	odp_pktio_param_t pktio_param;
	odp_schedule_sync_t  sync_mode;
	odp_pktio_capability_t capa;
	odp_pktin_queue_param_t in_queue_param;
	odp_pktout_queue_param_t out_queue_param;
	odp_pktio_op_mode_t mode_rx = ODP_PKTIO_OP_MT_UNSAFE;
	odp_pktio_op_mode_t mode_tx = ODP_PKTIO_OP_MT_UNSAFE;

	odp_pktio_param_init(&pktio_param);

	if (gbl_args->appl.mode == DIRECT_RECV) {
		pktio_param.in_mode = ODP_PKTIN_MODE_DIRECT;
		pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;
	} else if (gbl_args->appl.mode == PLAIN_QUEUE) {
		pktio_param.in_mode = ODP_PKTIN_MODE_QUEUE;
		pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;
	} else {
		pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;
		pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;
	}

	pktio = odp_pktio_open(dev, pool, &pktio_param);
	if (pktio == ODP_PKTIO_INVALID) {
		LOG_ERR("Error: failed to open %s\n", dev);
		return -1;
	}

	printf("created pktio %" PRIu64 " (%s)\n",
	       odp_pktio_to_u64(pktio), dev);

	if (odp_pktio_capability(pktio, &capa)) {
		LOG_ERR("Error: capability query failed %s\n", dev);
		return -1;
	}

	if (num_rx > (int)capa.max_input_queues) {
		printf("Sharing %i input queues between %i workers\n",
		       capa.max_input_queues, num_rx);
		num_rx = capa.max_input_queues;
		mode_rx = ODP_PKTIO_OP_MT;
	}

	odp_pktin_queue_param_init(&in_queue_param);
	odp_pktout_queue_param_init(&out_queue_param);

	if (gbl_args->appl.mode == DIRECT_RECV ||
	    gbl_args->appl.mode == PLAIN_QUEUE) {

		if (num_tx > (int)capa.max_output_queues) {
			printf("Sharing %i output queues between %i workers\n",
			       capa.max_output_queues, num_tx);
			num_tx = capa.max_output_queues;
			mode_tx = ODP_PKTIO_OP_MT;
		}

		in_queue_param.op_mode = mode_rx;
		in_queue_param.hash_enable = 1;
		in_queue_param.hash_proto.proto.ipv4_udp = 1;
		in_queue_param.num_queues  = num_rx;

		if (odp_pktin_queue_config(pktio, &in_queue_param)) {
			LOG_ERR("Error: input queue config failed %s\n", dev);
			return -1;
		}

		out_queue_param.op_mode = mode_tx;
		out_queue_param.num_queues  = num_tx;

		if (odp_pktout_queue_config(pktio, &out_queue_param)) {
			LOG_ERR("Error: output queue config failed %s\n", dev);
			return -1;
		}

		if (gbl_args->appl.mode == DIRECT_RECV) {
			if (odp_pktin_queue(pktio, gbl_args->pktios[idx].pktin,
					    num_rx) != num_rx) {
				LOG_ERR("Error: pktin queue query failed %s\n",
					dev);
				return -1;
			}
		} else { /* PLAIN QUEUE */
			if (odp_pktin_event_queue(pktio,
						  gbl_args->pktios[idx].rx_q,
						  num_rx) != num_rx) {
				LOG_ERR("Error: input queue query failed %s\n",
					dev);
				return -1;
			}
		}

		if (odp_pktout_queue(pktio, gbl_args->pktios[idx].pktout,
				     num_tx) != num_tx) {
			LOG_ERR("Error: pktout queue query failed %s\n", dev);
			return -1;
		}

		printf("created %i input and %i output queues on (%s)\n",
		       num_rx, num_tx, dev);

		gbl_args->pktios[idx].num_rx_queue = num_rx;
		gbl_args->pktios[idx].num_tx_queue = num_tx;
		gbl_args->pktios[idx].pktio  = pktio;

		return 0;
	}

	if (num_tx > (int)capa.max_output_queues) {
		printf("Sharing 1 output queue between %i workers\n",
		       num_tx);
		num_tx = 1;
		mode_tx = ODP_PKTIO_OP_MT;
	}

	if (gbl_args->appl.mode == SCHED_ATOMIC)
		sync_mode = ODP_SCHED_SYNC_ATOMIC;
	else if (gbl_args->appl.mode == SCHED_ORDERED)
		sync_mode = ODP_SCHED_SYNC_ORDERED;
	else
		sync_mode = ODP_SCHED_SYNC_PARALLEL;

	in_queue_param.hash_enable = 1;
	in_queue_param.hash_proto.proto.ipv4_udp = 1;
	in_queue_param.num_queues  = num_rx;
	in_queue_param.queue_param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	in_queue_param.queue_param.sched.sync  = sync_mode;
	in_queue_param.queue_param.sched.group = ODP_SCHED_GROUP_ALL;

	if (odp_pktin_queue_config(pktio, &in_queue_param)) {
		LOG_ERR("Error: input queue config failed %s\n", dev);
		return -1;
	}

	out_queue_param.op_mode = mode_tx;
	out_queue_param.num_queues  = num_tx;

	if (odp_pktout_queue_config(pktio, &out_queue_param)) {
		LOG_ERR("Error: output queue config failed %s\n", dev);
		return -1;
	}

	if (odp_pktout_queue(pktio, gbl_args->pktios[idx].pktout, num_tx)
	    != num_tx) {
		LOG_ERR("Error: pktout queue query failed %s\n", dev);
		return -1;
	}

	printf("created %i input and %i output queues on (%s)\n",
	       num_rx, num_tx, dev);

	gbl_args->pktios[idx].num_rx_queue = num_rx;
	gbl_args->pktios[idx].num_tx_queue = num_tx;
	gbl_args->pktios[idx].pktio        = pktio;

	return 0;
}

/**
 *  Print statistics
 *
 * @param num_workers Number of worker threads
 * @param thr_stats Pointer to stats storage
 * @param duration Number of seconds to loop in
 * @param timeout Number of seconds for stats calculation
 *
 */
static int print_speed_stats(int num_workers, stats_t *thr_stats,
			     int duration, int timeout)
{
	uint64_t pkts = 0;
	uint64_t pkts_prev = 0;
	uint64_t pps;
	uint64_t rx_drops, tx_drops;
	uint64_t maximum_pps = 0;
	int i;
	int elapsed = 0;
	int stats_enabled = 1;
	int loop_forever = (duration == 0);

	if (timeout <= 0) {
		stats_enabled = 0;
		timeout = 1;
	}
	/* Wait for all threads to be ready*/
	odp_barrier_wait(&barrier);

	do {
		pkts = 0;
		rx_drops = 0;
		tx_drops = 0;

		sleep(timeout);

		for (i = 0; i < num_workers; i++) {
			pkts += thr_stats[i].s.packets;
			rx_drops += thr_stats[i].s.rx_drops;
			tx_drops += thr_stats[i].s.tx_drops;
		}
		if (stats_enabled) {
			pps = (pkts - pkts_prev) / timeout;
			if (pps > maximum_pps)
				maximum_pps = pps;
			printf("%" PRIu64 " pps, %" PRIu64 " max pps, ",  pps,
			       maximum_pps);

			printf(" %" PRIu64 " rx drops, %" PRIu64 " tx drops\n",
			       rx_drops, tx_drops);

			pkts_prev = pkts;
		}
		elapsed += timeout;
	} while (loop_forever || (elapsed < duration));

	if (stats_enabled)
		printf("TEST RESULT: %" PRIu64 " maximum packets per second.\n",
		       maximum_pps);

	return pkts > 100 ? 0 : -1;
}

static void print_port_mapping(void)
{
	int if_count, num_workers;
	int thr, pktio;

	if_count    = gbl_args->appl.if_count;
	num_workers = gbl_args->appl.num_workers;

	printf("\nWorker mapping table (port[queue])\n--------------------\n");

	for (thr = 0; thr < num_workers; thr++) {
		int rx_idx, tx_idx;
		int rx_queue_idx, tx_queue_idx;
		thread_args_t *thr_args = &gbl_args->thread[thr];
		int num = thr_args->num_pktio;

		printf("Worker %i\n", thr);

		for (pktio = 0; pktio < num; pktio++) {
			rx_idx = thr_args->pktio[pktio].rx_idx;
			tx_idx = thr_args->pktio[pktio].tx_idx;
			rx_queue_idx = thr_args->pktio[pktio].rx_queue_idx;
			tx_queue_idx = thr_args->pktio[pktio].tx_queue_idx;
			printf("  %i[%i] ->  %i[%i]\n",
			       rx_idx, rx_queue_idx, tx_idx, tx_queue_idx);
		}
	}

	printf("\nPort config\n--------------------\n");

	for (pktio = 0; pktio < if_count; pktio++) {
		const char *dev = gbl_args->appl.if_names[pktio];

		printf("Port %i (%s)\n", pktio, dev);
		printf("  rx workers %i\n",
		       gbl_args->pktios[pktio].num_rx_thr);
		printf("  tx workers %i\n",
		       gbl_args->pktios[pktio].num_tx_thr);
		printf("  rx queues %i\n",
		       gbl_args->pktios[pktio].num_rx_queue);
		printf("  tx queues %i\n",
		       gbl_args->pktios[pktio].num_tx_queue);
	}

	printf("\n");
}

/**
 * Find the destination port for a given input port
 *
 * @param port  Input port index
 */
static int find_dest_port(int port)
{
	/* Even number of ports */
	if (gbl_args->appl.if_count % 2 == 0)
		return (port % 2 == 0) ? port + 1 : port - 1;

	/* Odd number of ports */
	if (port == gbl_args->appl.if_count - 1)
		return 0;
	else
		return port + 1;
}

/*
 * Bind worker threads to interfaces and calculate number of queues needed
 *
 * less workers (N) than interfaces (M)
 *  - assign each worker to process every Nth interface
 *  - workers process inequal number of interfaces, when M is not divisible by N
 *  - needs only single queue per interface
 * otherwise
 *  - assign an interface to every Mth worker
 *  - interfaces are processed by inequal number of workers, when N is not
 *    divisible by M
 *  - tries to configure a queue per worker per interface
 *  - shares queues, if interface capability does not allows a queue per worker
 */
static void bind_workers(void)
{
	int if_count, num_workers;
	int rx_idx, tx_idx, thr, pktio;
	thread_args_t *thr_args;

	if_count    = gbl_args->appl.if_count;
	num_workers = gbl_args->appl.num_workers;

	/* initialize port forwarding table */
	for (rx_idx = 0; rx_idx < if_count; rx_idx++)
		gbl_args->dst_port[rx_idx] = find_dest_port(rx_idx);

	if (if_count > num_workers) {
		thr = 0;

		for (rx_idx = 0; rx_idx < if_count; rx_idx++) {
			thr_args = &gbl_args->thread[thr];
			pktio    = thr_args->num_pktio;
			tx_idx   = gbl_args->dst_port[rx_idx];
			thr_args->pktio[pktio].rx_idx = rx_idx;
			thr_args->pktio[pktio].tx_idx = tx_idx;
			thr_args->num_pktio++;

			gbl_args->pktios[rx_idx].num_rx_thr++;
			gbl_args->pktios[tx_idx].num_tx_thr++;

			thr++;
			if (thr >= num_workers)
				thr = 0;
		}
	} else {
		rx_idx = 0;

		for (thr = 0; thr < num_workers; thr++) {
			thr_args = &gbl_args->thread[thr];
			pktio    = thr_args->num_pktio;
			tx_idx   = gbl_args->dst_port[rx_idx];
			thr_args->pktio[pktio].rx_idx = rx_idx;
			thr_args->pktio[pktio].tx_idx = tx_idx;
			thr_args->num_pktio++;

			gbl_args->pktios[rx_idx].num_rx_thr++;
			gbl_args->pktios[tx_idx].num_tx_thr++;

			rx_idx++;
			if (rx_idx >= if_count)
				rx_idx = 0;
		}
	}
}

/*
 * Bind queues to threads and fill in missing thread arguments (handles)
 */
static void bind_queues(void)
{
	int num_workers;
	int thr, pktio;

	num_workers = gbl_args->appl.num_workers;

	for (thr = 0; thr < num_workers; thr++) {
		int rx_idx, tx_idx;
		thread_args_t *thr_args = &gbl_args->thread[thr];
		int num = thr_args->num_pktio;

		for (pktio = 0; pktio < num; pktio++) {
			int rx_queue, tx_queue;

			rx_idx   = thr_args->pktio[pktio].rx_idx;
			tx_idx   = thr_args->pktio[pktio].tx_idx;
			rx_queue = gbl_args->pktios[rx_idx].next_rx_queue;
			tx_queue = gbl_args->pktios[tx_idx].next_tx_queue;

			thr_args->pktio[pktio].rx_queue_idx = rx_queue;
			thr_args->pktio[pktio].tx_queue_idx = tx_queue;
			thr_args->pktio[pktio].pktin =
				gbl_args->pktios[rx_idx].pktin[rx_queue];
			thr_args->pktio[pktio].pktout =
				gbl_args->pktios[tx_idx].pktout[tx_queue];
			thr_args->pktio[pktio].rx_queue =
				gbl_args->pktios[rx_idx].rx_q[rx_queue];
			thr_args->pktio[pktio].rx_pktio =
				gbl_args->pktios[rx_idx].pktio;
			thr_args->pktio[pktio].tx_pktio =
				gbl_args->pktios[tx_idx].pktio;

			rx_queue++;
			tx_queue++;

			if (rx_queue >= gbl_args->pktios[rx_idx].num_rx_queue)
				rx_queue = 0;
			if (tx_queue >= gbl_args->pktios[tx_idx].num_tx_queue)
				tx_queue = 0;

			gbl_args->pktios[rx_idx].next_rx_queue = rx_queue;
			gbl_args->pktios[tx_idx].next_tx_queue = tx_queue;
		}
	}
}

/**
 * Prinf usage information
 */
static void usage(char *progname)
{
	printf("\n"
	       "OpenDataPlane L2 forwarding application.\n"
	       "\n"
	       "Usage: %s OPTIONS\n"
	       "  E.g. %s -i eth0,eth1,eth2,eth3 -m 0 -t 1\n"
	       " In the above example,\n"
	       " eth0 will send pkts to eth1 and vice versa\n"
	       " eth2 will send pkts to eth3 and vice versa\n"
	       "\n"
	       "Mandatory OPTIONS:\n"
	       "  -i, --interface Eth interfaces (comma-separated, no spaces)\n"
	       "                  Interface count min 1, max %i\n"
	       "\n"
	       "Optional OPTIONS\n"
	       "  -m, --mode      0: Receive packets directly from pktio interface (default)\n"
	       "                  1: Receive packets through scheduler sync parallel queues\n"
	       "                  2: Receive packets through scheduler sync atomic queues\n"
	       "                  3: Receive packets through scheduler sync ordered queues\n"
	       "                  4: Receive packets through plain queues\n"
	       "  -c, --count <number> CPU count.\n"
	       "  -t, --time  <number> Time in seconds to run.\n"
	       "  -a, --accuracy <number> Time in seconds get print statistics\n"
	       "                          (default is 1 second).\n"
	       "  -d, --dst_change  0: Don't change packets' dst eth addresses (default)\n"
	       "                    1: Change packets' dst eth addresses\n"
	       "  -s, --src_change  0: Don't change packets' src eth addresses\n"
	       "                    1: Change packets' src eth addresses (default)\n"
	       "  -e, --error_check 0: Don't check packet errors (default)\n"
	       "                    1: Check packet errors\n"
	       "  -h, --help           Display help and exit.\n\n"
	       " environment variables: ODP_PKTIO_DISABLE_NETMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMSG\n"
	       " can be used to advanced pkt I/O selection for linux-generic\n"
	       "\n", NO_PATH(progname), NO_PATH(progname), MAX_PKTIOS
	    );
}

/**
 * Parse and store the command line arguments
 *
 * @param argc       argument count
 * @param argv[]     argument vector
 * @param appl_args  Store application arguments here
 */
static void parse_args(int argc, char *argv[], appl_args_t *appl_args)
{
	int opt;
	int long_index;
	char *token;
	size_t len;
	int i;
	static struct option longopts[] = {
		{"count", required_argument, NULL, 'c'},
		{"time", required_argument, NULL, 't'},
		{"accuracy", required_argument, NULL, 'a'},
		{"interface", required_argument, NULL, 'i'},
		{"mode", required_argument, NULL, 'm'},
		{"dst_change", required_argument, NULL, 'd'},
		{"src_change", required_argument, NULL, 's'},
		{"error_check", required_argument, NULL, 'e'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	appl_args->time = 0; /* loop forever if time to run is 0 */
	appl_args->accuracy = 1; /* get and print pps stats second */
	appl_args->src_change = 1; /* change eth src address by default */
	appl_args->error_check = 0; /* don't check packet errors by default */

	while (1) {
		opt = getopt_long(argc, argv, "+c:+t:+a:i:m:d:s:e:h",
				  longopts, &long_index);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 'c':
			appl_args->cpu_count = atoi(optarg);
			break;
		case 't':
			appl_args->time = atoi(optarg);
			break;
		case 'a':
			appl_args->accuracy = atoi(optarg);
			break;
			/* parse packet-io interface names */
		case 'i':
			len = strlen(optarg);
			if (len == 0) {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			len += 1;	/* add room for '\0' */

			appl_args->if_str = malloc(len);
			if (appl_args->if_str == NULL) {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}

			/* count the number of tokens separated by ',' */
			strcpy(appl_args->if_str, optarg);
			for (token = strtok(appl_args->if_str, ","), i = 0;
			     token != NULL;
			     token = strtok(NULL, ","), i++)
				;

			appl_args->if_count = i;

			if (appl_args->if_count < 1 ||
			    appl_args->if_count > MAX_PKTIOS) {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}

			/* allocate storage for the if names */
			appl_args->if_names =
			    calloc(appl_args->if_count, sizeof(char *));

			/* store the if names (reset names string) */
			strcpy(appl_args->if_str, optarg);
			for (token = strtok(appl_args->if_str, ","), i = 0;
			     token != NULL; token = strtok(NULL, ","), i++) {
				appl_args->if_names[i] = token;
			}
			break;
		case 'm':
			i = atoi(optarg);
			if (i == 1)
				appl_args->mode = SCHED_PARALLEL;
			else if (i == 2)
				appl_args->mode = SCHED_ATOMIC;
			else if (i == 3)
				appl_args->mode = SCHED_ORDERED;
			else if (i == 4)
				appl_args->mode = PLAIN_QUEUE;
			else
				appl_args->mode = DIRECT_RECV;
			break;
		case 'd':
			appl_args->dst_change = atoi(optarg);
			break;
		case 's':
			appl_args->src_change = atoi(optarg);
			break;
		case 'e':
			appl_args->error_check = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}

	if (appl_args->if_count == 0) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	optind = 1;		/* reset 'extern optind' from the getopt lib */
}

/**
 * Print system and application info
 */
static void print_info(char *progname, appl_args_t *appl_args)
{
	int i;

	printf("\n"
	       "ODP system info\n"
	       "---------------\n"
	       "ODP API version: %s\n"
	       "CPU model:       %s\n"
	       "CPU freq (hz):   %" PRIu64 "\n"
	       "Cache line size: %i\n"
	       "CPU count:       %i\n"
	       "\n",
	       odp_version_api_str(), odp_cpu_model_str(), odp_cpu_hz_max(),
	       odp_sys_cache_line_size(), odp_cpu_count());

	printf("Running ODP appl: \"%s\"\n"
	       "-----------------\n"
	       "IF-count:        %i\n"
	       "Using IFs:      ",
	       progname, appl_args->if_count);
	for (i = 0; i < appl_args->if_count; ++i)
		printf(" %s", appl_args->if_names[i]);
	printf("\n"
	       "Mode:            ");
	if (appl_args->mode == DIRECT_RECV)
		printf("DIRECT_RECV");
	else if (appl_args->mode == PLAIN_QUEUE)
		printf("PLAIN_QUEUE");
	else if (appl_args->mode == SCHED_PARALLEL)
		printf("SCHED_PARALLEL");
	else if (appl_args->mode == SCHED_ATOMIC)
		printf("SCHED_ATOMIC");
	else if (appl_args->mode == SCHED_ORDERED)
		printf("SCHED_ORDERED");
	printf("\n\n");
	fflush(NULL);
}

static void gbl_args_init(args_t *args)
{
	int pktio, queue;

	memset(args, 0, sizeof(args_t));

	for (pktio = 0; pktio < MAX_PKTIOS; pktio++) {
		args->pktios[pktio].pktio = ODP_PKTIO_INVALID;

		for (queue = 0; queue < MAX_QUEUES; queue++)
			args->pktios[pktio].rx_q[queue] = ODP_QUEUE_INVALID;
	}
}

/**
 * ODP L2 forwarding main function
 */
int main(int argc, char *argv[])
{
	odph_linux_pthread_t thread_tbl[MAX_WORKERS];
	odp_pool_t pool;
	int i;
	int cpu;
	int num_workers;
	odp_shm_t shm;
	odp_cpumask_t cpumask;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	odph_ethaddr_t new_addr;
	odp_pool_param_t params;
	int ret;
	stats_t *stats;
	int if_count;
	void *(*thr_run_func)(void *);

	/* Init ODP before calling anything else */
	if (odp_init_global(NULL, NULL)) {
		LOG_ERR("Error: ODP global init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Init this thread */
	if (odp_init_local(ODP_THREAD_CONTROL)) {
		LOG_ERR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Reserve memory for args from shared mem */
	shm = odp_shm_reserve("shm_args", sizeof(args_t),
			      ODP_CACHE_LINE_SIZE, 0);
	gbl_args = odp_shm_addr(shm);

	if (gbl_args == NULL) {
		LOG_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	gbl_args_init(gbl_args);

	/* Parse and store the application arguments */
	parse_args(argc, argv, &gbl_args->appl);

	/* Print both system and application information */
	print_info(NO_PATH(argv[0]), &gbl_args->appl);

	/* Default to system CPU count unless user specified */
	num_workers = MAX_WORKERS;
	if (gbl_args->appl.cpu_count)
		num_workers = gbl_args->appl.cpu_count;

	/* Get default worker cpumask */
	num_workers = odp_cpumask_default_worker(&cpumask, num_workers);
	(void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

	gbl_args->appl.num_workers = num_workers;

	for (i = 0; i < num_workers; i++)
		gbl_args->thread[i].thr_idx    = i;

	if_count = gbl_args->appl.if_count;

	printf("num worker threads: %i\n", num_workers);
	printf("first CPU:          %i\n", odp_cpumask_first(&cpumask));
	printf("cpu mask:           %s\n", cpumaskstr);

	/* Create packet pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_SIZE;
	params.type        = ODP_POOL_PACKET;

	pool = odp_pool_create("packet pool", &params);

	if (pool == ODP_POOL_INVALID) {
		LOG_ERR("Error: packet pool create failed.\n");
		exit(EXIT_FAILURE);
	}
	odp_pool_print(pool);

	bind_workers();

	for (i = 0; i < if_count; ++i) {
		const char *dev = gbl_args->appl.if_names[i];
		int num_rx, num_tx;

		/* A queue per worker in scheduled mode */
		num_rx = num_workers;
		num_tx = num_workers;

		if (gbl_args->appl.mode == DIRECT_RECV ||
		    gbl_args->appl.mode == PLAIN_QUEUE) {
			/* A queue per assigned worker */
			num_rx = gbl_args->pktios[i].num_rx_thr;
			num_tx = gbl_args->pktios[i].num_tx_thr;
		}

		if (create_pktio(dev, i, num_rx, num_tx, pool))
			exit(EXIT_FAILURE);

		/* Save interface ethernet address */
		if (odp_pktio_mac_addr(gbl_args->pktios[i].pktio,
				       gbl_args->port_eth_addr[i].addr,
				       ODPH_ETHADDR_LEN) != ODPH_ETHADDR_LEN) {
			LOG_ERR("Error: interface ethernet address unknown\n");
			exit(EXIT_FAILURE);
		}

		/* Save destination eth address */
		if (gbl_args->appl.dst_change) {
			/* 02:00:00:00:00:XX */
			memset(&new_addr, 0, sizeof(odph_ethaddr_t));
			new_addr.addr[0] = 0x02;
			new_addr.addr[5] = i;
			gbl_args->dst_eth_addr[i] = new_addr;
		}
	}

	gbl_args->pktios[i].pktio = ODP_PKTIO_INVALID;

	bind_queues();

	if (gbl_args->appl.mode == DIRECT_RECV ||
	    gbl_args->appl.mode == PLAIN_QUEUE)
		print_port_mapping();

	memset(thread_tbl, 0, sizeof(thread_tbl));

	stats = gbl_args->stats;

	odp_barrier_init(&barrier, num_workers + 1);

	if (gbl_args->appl.mode == DIRECT_RECV)
		thr_run_func = run_worker_direct_mode;
	else if (gbl_args->appl.mode == PLAIN_QUEUE)
		thr_run_func = run_worker_plain_queue_mode;
	else /* SCHED_PARALLEL / SCHED_ATOMIC / SCHED_ORDERED */
		thr_run_func = run_worker_sched_mode;

	/* Create worker threads */
	cpu = odp_cpumask_first(&cpumask);
	for (i = 0; i < num_workers; ++i) {
		odp_cpumask_t thd_mask;

		gbl_args->thread[i].stats = &stats[i];

		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, cpu);
		odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
					  thr_run_func,
					  &gbl_args->thread[i],
					  ODP_THREAD_WORKER);
		cpu = odp_cpumask_next(&cpumask, cpu);
	}

	/* Start packet receive and transmit */
	for (i = 0; i < if_count; ++i) {
		odp_pktio_t pktio;

		pktio = gbl_args->pktios[i].pktio;
		ret   = odp_pktio_start(pktio);
		if (ret) {
			LOG_ERR("Error: unable to start %s\n",
				gbl_args->appl.if_names[i]);
			exit(EXIT_FAILURE);
		}
	}

	ret = print_speed_stats(num_workers, stats, gbl_args->appl.time,
				gbl_args->appl.accuracy);
	exit_threads = 1;

	/* Master thread waits for other threads to exit */
	odph_linux_pthread_join(thread_tbl, num_workers);

	free(gbl_args->appl.if_names);
	free(gbl_args->appl.if_str);
	printf("Exit\n\n");

	return ret;
}
