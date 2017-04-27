/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <example_debug.h>

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
#define SHM_PKT_POOL_SIZE      (512*2048)

/** @def SHM_PKT_POOL_BUF_SIZE
 * @brief Buffer size of the packet pool buffer
 */
#define SHM_PKT_POOL_BUF_SIZE  1856

/** @def MAX_PKT_BURST
 * @brief Maximum number of packet bursts
 */
#define MAX_PKT_BURST          16

/** @def APPL_MODE_PKT_BURST
 * @brief The application will handle pakcets in bursts
 */
#define APPL_MODE_PKT_BURST    0

/** @def APPL_MODE_PKT_QUEUE
 * @brief The application will handle packets in queues
 */
#define APPL_MODE_PKT_QUEUE    1

/** @def APPL_MODE_PKT_SCHED
 * @brief The application will handle packets with sheduler
 */
#define APPL_MODE_PKT_SCHED    2

/** @def PRINT_APPL_MODE(x)
 * @brief Macro to print the current status of how the application handles
 * packets.
 */
#define PRINT_APPL_MODE(x) printf("%s(%i)\n", #x, (x))

/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ? \
			    strrchr((file_name), '/') + 1 : (file_name))
/**
 * Parsed command line application arguments
 */
typedef struct {
	int cpu_count;		/**< Number of CPUs to use */
	int if_count;		/**< Number of interfaces to be used */
	char **if_names;	/**< Array of pointers to interface names */
	int mode;		/**< Packet IO mode */
	char *if_str;		/**< Storage for interface names */
} appl_args_t;

/**
 * Thread specific arguments
 */
typedef struct {
	char *pktio_dev;	/**< Interface name to use */
	int mode;		/**< Thread mode */
} thread_args_t;

/**
 * Grouping of both parsed CL args and thread specific args - alloc together
 */
typedef struct {
	/** Application (parsed) arguments */
	appl_args_t appl;
	/** Thread specific arguments */
	thread_args_t thread[MAX_WORKERS];
} args_t;

/** Global pointer to args */
static args_t *args;

/* helper funcs */
static int drop_err_pkts(odp_packet_t pkt_tbl[], unsigned len);
static void swap_pkt_addrs(odp_packet_t pkt_tbl[], unsigned len);
static void parse_args(int argc, char *argv[], appl_args_t *appl_args);
static void print_info(char *progname, appl_args_t *appl_args);
static void usage(char *progname);

/**
 * Create a pktio handle, optionally associating a default input queue.
 *
 * @param dev Name of device to open
 * @param pool Pool to associate with device for packet RX/TX
 * @param mode Packet processing mode for this device (BURST or QUEUE)
 *
 * @return The handle of the created pktio object.
 * @retval ODP_PKTIO_INVALID if the create fails.
 */
static odp_pktio_t create_pktio(const char *dev, odp_pool_t pool, int mode)
{
	odp_pktio_t pktio;
	odp_queue_t inq_def;
	odp_queue_param_t qparam;
	char inq_name[ODP_QUEUE_NAME_LEN];
	int ret;
	odp_pktio_param_t pktio_param;

	odp_pktio_param_init(&pktio_param);

	switch (mode) {
	case  APPL_MODE_PKT_BURST:
		pktio_param.in_mode = ODP_PKTIN_MODE_DIRECT;
		break;
	case APPL_MODE_PKT_QUEUE:
		pktio_param.in_mode = ODP_PKTIN_MODE_QUEUE;
		break;
	case APPL_MODE_PKT_SCHED:
		pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;
		break;
	default:
		EXAMPLE_ABORT("invalid mode %d\n", mode);
	}

	/* Open a packet IO instance */
	pktio = odp_pktio_open(dev, pool, &pktio_param);
	if (pktio == ODP_PKTIO_INVALID)
		EXAMPLE_ABORT("Error: pktio create failed for %s\n", dev);

	snprintf(inq_name, sizeof(inq_name), "%" PRIu64 "-pktio_inq_def",
		 odp_pktio_to_u64(pktio));
	inq_name[ODP_QUEUE_NAME_LEN - 1] = '\0';

	switch (mode) {
	case  APPL_MODE_PKT_BURST:
		/* no further setup needed for burst mode */
		ret = odp_pktio_start(pktio);
		if (ret != 0)
			EXAMPLE_ABORT("Error: unable to start %s\n", dev);
		return pktio;
	case APPL_MODE_PKT_QUEUE:
		odp_queue_param_init(&qparam);
		qparam.type = ODP_QUEUE_TYPE_PKTIN;
		inq_def = odp_queue_create(inq_name, &qparam);
		break;
	case APPL_MODE_PKT_SCHED:
		odp_queue_param_init(&qparam);
		qparam.type        = ODP_QUEUE_TYPE_PKTIN;
		qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
		qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
		qparam.sched.group = ODP_SCHED_GROUP_ALL;
		inq_def = odp_queue_create(inq_name, &qparam);
		break;
	default:
		EXAMPLE_ABORT("invalid mode %d\n", mode);
	}

	if (inq_def == ODP_QUEUE_INVALID)
		EXAMPLE_ABORT("Error: pktio inq create failed for %s\n", dev);

	ret = odp_pktio_inq_setdef(pktio, inq_def);
	if (ret != 0)
		EXAMPLE_ABORT("Error: default input-Q setup for %s\n", dev);

	ret = odp_pktio_start(pktio);
	if (ret != 0)
		EXAMPLE_ABORT("Error: unable to start %s\n", dev);

	printf("  created pktio:%02" PRIu64
	       ", dev:%s, queue mode (ATOMIC queues)\n"
	       "  \tdefault pktio%02" PRIu64 "-INPUT queue:%" PRIu64 "\n",
	       odp_pktio_to_u64(pktio), dev,
	       odp_pktio_to_u64(pktio), odp_queue_to_u64(inq_def));

	return pktio;
}

/**
 * Packet IO loopback worker thread using ODP queues
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *pktio_queue_thread(void *arg)
{
	int thr;
	odp_pktio_t pktio;
	thread_args_t *thr_args;
	odp_queue_t outq_def;
	odp_queue_t inq;
	odp_packet_t pkt;
	odp_event_t ev;
	unsigned long pkt_cnt = 0;
	unsigned long err_cnt = 0;

	thr = odp_thread_id();
	thr_args = arg;

	pktio = odp_pktio_lookup(thr_args->pktio_dev);
	if (pktio == ODP_PKTIO_INVALID) {
		EXAMPLE_ERR("  [%02i] Error: lookup of pktio %s failed\n",
			    thr, thr_args->pktio_dev);
		return NULL;
	}

	printf("  [%02i] looked up pktio:%02" PRIu64
	       ", queue mode (ATOMIC queues)\n"
	       "         default pktio%02" PRIu64 "-INPUT queue:%" PRIu64 "\n",
	       thr, odp_pktio_to_u64(pktio), odp_pktio_to_u64(pktio),
	       odp_queue_to_u64(odp_pktio_inq_getdef(pktio)));

	if (thr_args->mode == APPL_MODE_PKT_QUEUE)
		inq = odp_pktio_inq_getdef(pktio);
	else
		inq = ODP_QUEUE_INVALID;

	/* Loop packets */
	for (;;) {
		odp_pktio_t pktio_tmp;

		if (inq != ODP_QUEUE_INVALID) {
			ev = odp_queue_deq(inq);
			pkt = odp_packet_from_event(ev);
			if (!odp_packet_is_valid(pkt))
				continue;
		} else {
			ev = odp_schedule(NULL, ODP_SCHED_WAIT);
			pkt = odp_packet_from_event(ev);
		}

		/* Drop packets with errors */
		if (odp_unlikely(drop_err_pkts(&pkt, 1) == 0)) {
			EXAMPLE_ERR("Drop frame - err_cnt:%lu\n", ++err_cnt);
			continue;
		}

		pktio_tmp = odp_packet_input(pkt);
		outq_def = odp_pktio_outq_getdef(pktio_tmp);

		if (outq_def == ODP_QUEUE_INVALID) {
			EXAMPLE_ERR("  [%02i] Error: def output-Q query\n",
				    thr);
			return NULL;
		}

		/* Swap Eth MACs and possibly IP-addrs before sending back */
		swap_pkt_addrs(&pkt, 1);

		/* Enqueue the packet for output */
		if (odp_queue_enq(outq_def, ev)) {
			EXAMPLE_ERR("  [%i] Queue enqueue failed.\n", thr);
			odp_packet_free(pkt);
			continue;
		}

		/* Print packet counts every once in a while */
		if (odp_unlikely(pkt_cnt++ % 100000 == 0)) {
			printf("  [%02i] pkt_cnt:%lu\n", thr, pkt_cnt);
			fflush(NULL);
		}
	}

/* unreachable */
	return NULL;
}

/**
 * Packet IO loopback worker thread using bursts from/to IO resources
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *pktio_ifburst_thread(void *arg)
{
	int thr;
	odp_pktio_t pktio;
	thread_args_t *thr_args;
	int pkts, pkts_ok;
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	unsigned long pkt_cnt = 0;
	unsigned long err_cnt = 0;
	unsigned long tmp = 0;

	thr = odp_thread_id();
	thr_args = arg;

	pktio = odp_pktio_lookup(thr_args->pktio_dev);
	if (pktio == ODP_PKTIO_INVALID) {
		EXAMPLE_ERR("  [%02i] Error: lookup of pktio %s failed\n",
			    thr, thr_args->pktio_dev);
		return NULL;
	}

	printf("  [%02i] looked up pktio:%02" PRIu64 ", burst mode\n",
	       thr, odp_pktio_to_u64(pktio));

	/* Loop packets */
	for (;;) {
		pkts = odp_pktio_recv(pktio, pkt_tbl, MAX_PKT_BURST);
		if (pkts > 0) {
			/* Drop packets with errors */
			pkts_ok = drop_err_pkts(pkt_tbl, pkts);
			if (pkts_ok > 0) {
				int sent;

				/* Swap Eth MACs and IP-addrs */
				swap_pkt_addrs(pkt_tbl, pkts_ok);
				sent = odp_pktio_send(pktio, pkt_tbl, pkts_ok);
				sent = sent > 0 ? sent : 0;
				if (odp_unlikely(sent < pkts_ok)) {
					err_cnt += pkts_ok - sent;
					do
						odp_packet_free(pkt_tbl[sent]);
					while (++sent < pkts_ok);
				}
			}

			if (odp_unlikely(pkts_ok != pkts))
				EXAMPLE_ERR("Dropped frames:%u - err_cnt:%lu\n",
					    pkts-pkts_ok, ++err_cnt);

			/* Print packet counts every once in a while */
			tmp += pkts_ok;
			if (odp_unlikely((tmp >= 100000) || /* OR first print:*/
			    ((pkt_cnt == 0) && ((tmp-1) < MAX_PKT_BURST)))) {
				pkt_cnt += tmp;
				printf("  [%02i] pkt_cnt:%lu\n", thr, pkt_cnt);
				fflush(NULL);
				tmp = 0;
			}
		}
	}

/* unreachable */
	return NULL;
}

/**
 * ODP packet example main function
 */
int main(int argc, char *argv[])
{
	odph_linux_pthread_t thread_tbl[MAX_WORKERS];
	odp_pool_t pool;
	int num_workers;
	int i;
	int cpu;
	odp_cpumask_t cpumask;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	odp_pool_param_t params;

	args = calloc(1, sizeof(args_t));
	if (args == NULL) {
		EXAMPLE_ERR("Error: args mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Parse and store the application arguments */
	parse_args(argc, argv, &args->appl);

	/* Init ODP before calling anything else */
	if (odp_init_global(NULL, NULL)) {
		EXAMPLE_ERR("Error: ODP global init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Init this thread */
	if (odp_init_local(ODP_THREAD_CONTROL)) {
		EXAMPLE_ERR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Print both system and application information */
	print_info(NO_PATH(argv[0]), &args->appl);

	/* Default to system CPU count unless user specified */
	num_workers = MAX_WORKERS;
	if (args->appl.cpu_count)
		num_workers = args->appl.cpu_count;

	/* Get default worker cpumask */
	num_workers = odp_cpumask_default_worker(&cpumask, num_workers);
	(void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

	printf("num worker threads: %i\n", num_workers);
	printf("first CPU:          %i\n", odp_cpumask_first(&cpumask));
	printf("cpu mask:           %s\n", cpumaskstr);

	/* Create packet pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_SIZE/SHM_PKT_POOL_BUF_SIZE;
	params.type        = ODP_POOL_PACKET;

	pool = odp_pool_create("packet_pool", &params);

	if (pool == ODP_POOL_INVALID) {
		EXAMPLE_ERR("Error: packet pool create failed.\n");
		exit(EXIT_FAILURE);
	}
	odp_pool_print(pool);

	/* Create a pktio instance for each interface */
	for (i = 0; i < args->appl.if_count; ++i)
		create_pktio(args->appl.if_names[i], pool, args->appl.mode);

	/* Create and init worker threads */
	memset(thread_tbl, 0, sizeof(thread_tbl));

	cpu = odp_cpumask_first(&cpumask);
	for (i = 0; i < num_workers; ++i) {
		odp_cpumask_t thd_mask;
		void *(*thr_run_func)(void *);
		int if_idx;

		if_idx = i % args->appl.if_count;

		args->thread[i].pktio_dev = args->appl.if_names[if_idx];
		args->thread[i].mode = args->appl.mode;

		if (args->appl.mode == APPL_MODE_PKT_BURST)
			thr_run_func = pktio_ifburst_thread;
		else /* APPL_MODE_PKT_QUEUE */
			thr_run_func = pktio_queue_thread;
		/*
		 * Create threads one-by-one instead of all-at-once,
		 * because each thread might get different arguments.
		 * Calls odp_thread_create(cpu) for each thread
		 */
		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, cpu);
		odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
					  thr_run_func,
					  &args->thread[i],
					  ODP_THREAD_WORKER);
		cpu = odp_cpumask_next(&cpumask, cpu);
	}

	/* Master thread waits for other threads to exit */
	odph_linux_pthread_join(thread_tbl, num_workers);

	free(args->appl.if_names);
	free(args->appl.if_str);
	free(args);
	printf("Exit\n\n");

	return 0;
}

/**
 * Drop packets which input parsing marked as containing errors.
 *
 * Frees packets with error and modifies pkt_tbl[] to only contain packets with
 * no detected errors.
 *
 * @param pkt_tbl  Array of packet
 * @param len      Length of pkt_tbl[]
 *
 * @return Number of packets with no detected error
 */
static int drop_err_pkts(odp_packet_t pkt_tbl[], unsigned len)
{
	odp_packet_t pkt;
	unsigned pkt_cnt = len;
	unsigned i, j;

	for (i = 0, j = 0; i < len; ++i) {
		pkt = pkt_tbl[i];

		if (odp_unlikely(odp_packet_has_error(pkt))) {
			odp_packet_free(pkt); /* Drop */
			pkt_cnt--;
		} else if (odp_unlikely(i != j++)) {
			pkt_tbl[j-1] = pkt;
		}
	}

	return pkt_cnt;
}

/**
 * Swap eth src<->dst and IP src<->dst addresses
 *
 * @param pkt_tbl  Array of packets
 * @param len      Length of pkt_tbl[]
 */

static void swap_pkt_addrs(odp_packet_t pkt_tbl[], unsigned len)
{
	odp_packet_t pkt;
	odph_ethhdr_t *eth;
	odph_ethaddr_t tmp_addr;
	odph_ipv4hdr_t *ip;
	odp_u32be_t ip_tmp_addr; /* tmp ip addr */
	unsigned i;

	for (i = 0; i < len; ++i) {
		pkt = pkt_tbl[i];
		if (odp_packet_has_eth(pkt)) {
			eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);

			tmp_addr = eth->dst;
			eth->dst = eth->src;
			eth->src = tmp_addr;

			if (odp_packet_has_ipv4(pkt)) {
				/* IPv4 */
				ip = (odph_ipv4hdr_t *)
					odp_packet_l3_ptr(pkt, NULL);

				ip_tmp_addr  = ip->src_addr;
				ip->src_addr = ip->dst_addr;
				ip->dst_addr = ip_tmp_addr;
			}
		}
	}
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
		{"interface", required_argument, NULL, 'i'},	/* return 'i' */
		{"mode", required_argument, NULL, 'm'},		/* return 'm' */
		{"help", no_argument, NULL, 'h'},		/* return 'h' */
		{NULL, 0, NULL, 0}
	};

	appl_args->mode = APPL_MODE_PKT_SCHED;

	while (1) {
		opt = getopt_long(argc, argv, "+c:i:+m:t:h",
				  longopts, &long_index);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 'c':
			appl_args->cpu_count = atoi(optarg);
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

			if (appl_args->if_count == 0) {
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
			switch (i) {
			case 0:
				appl_args->mode = APPL_MODE_PKT_BURST;
				break;
			case 1:
				appl_args->mode = APPL_MODE_PKT_QUEUE;
				break;
			case 2:
				appl_args->mode = APPL_MODE_PKT_SCHED;
				break;
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;

		default:
			break;
		}
	}

	if (appl_args->if_count == 0 || appl_args->mode == -1) {
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
	       "CPU freq (hz):   %"PRIu64"\n"
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
	switch (appl_args->mode) {
	case APPL_MODE_PKT_BURST:
		PRINT_APPL_MODE(APPL_MODE_PKT_BURST);
		break;
	case APPL_MODE_PKT_QUEUE:
		PRINT_APPL_MODE(APPL_MODE_PKT_QUEUE);
		break;
	case APPL_MODE_PKT_SCHED:
		PRINT_APPL_MODE(APPL_MODE_PKT_SCHED);
		break;
	}
	printf("\n\n");
	fflush(NULL);
}

/**
 * Prinf usage information
 */
static void usage(char *progname)
{
	printf("\n"
	       "Usage: %s OPTIONS\n"
	       "  E.g. %s -i eth1,eth2,eth3 -m 0\n"
	       "\n"
	       "OpenDataPlane example application.\n"
	       "\n"
	       "Mandatory OPTIONS:\n"
	       "  -i, --interface Eth interfaces (comma-separated, no spaces)\n"
	       "\n"
	       "Optional OPTIONS\n"
	       "  -c, --count <number> CPU count.\n"
	       "  -m, --mode      0: Receive and send directly (no queues)\n"
	       "                  1: Receive and send via queues.\n"
	       "                  2: Receive via scheduler, send via queues.\n"
	       "  -h, --help           Display help and exit.\n"
	       " environment variables: ODP_PKTIO_DISABLE_NETMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMSG\n"
	       " can be used to advanced pkt I/O selection for linux-generic\n"
	       "\n", NO_PATH(progname), NO_PATH(progname)
	    );
}
