/* Copyright (c) 2015, Linaro Limited
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
#include <strings.h>
#include <errno.h>
#include <stdio.h>

/** @def MAX_WORKERS
 * @brief Maximum number of worker threads
 */
#define MAX_WORKERS 32

/** @def SHM_PKT_POOL_SIZE
 * @brief Size of the shared memory block
 */
#define SHM_PKT_POOL_SIZE (512 * 2048)

/** @def SHM_PKT_POOL_BUF_SIZE
 * @brief Buffer size of the packet pool buffer
 */
#define SHM_PKT_POOL_BUF_SIZE 1856

/** @def MAX_PMR_COUNT
 * @brief Maximum number of Classification Policy
 */
#define MAX_PMR_COUNT 8

/** @def DISPLAY_STRING_LEN
 * @brief Length of string used to display term value
 */
#define DISPLAY_STRING_LEN 32

/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ? \
			    strrchr((file_name), '/') + 1 : (file_name))

typedef struct {
	odp_queue_t	 queue;           /**< Associated queue handle */
	odp_pool_t	 pool;            /**< Associated pool handle */
	odp_cos_t	 cos;             /**< Associated cos handle */
	odp_pmr_t	 pmr;             /**< Associated pmr handle */
	odp_atomic_u64_t queue_pkt_count; /**< count of received packets */
	odp_atomic_u64_t pool_pkt_count;  /**< count of received packets */

	char cos_name[ODP_COS_NAME_LEN];  /**< cos name */

	struct {
		odp_pmr_term_t term;      /**< odp pmr term value */
		uint64_t       val;       /**< pmr term value */
		uint64_t       mask;      /**< pmr term mask */
		uint32_t       val_sz;    /**< size of the pmr term*/
		uint32_t       offset;    /**< pmr term offset */
	} rule;

	char value[DISPLAY_STRING_LEN];   /**< Display string for value */
	char mask[DISPLAY_STRING_LEN];    /**< Display string for mask */
} global_statistics;

typedef struct {
	global_statistics stats[MAX_PMR_COUNT];
	int		  policy_count;  /**< global policy count */
	int		  appl_mode;     /**< application mode */
	odp_atomic_u64_t  total_packets; /**< total received packets */
	int		  cpu_count;     /**< Number of CPUs to use */
	uint32_t	  time;          /**< Number of seconds to run */
	char		 *if_name;       /**< pointer to interface names */
} appl_args_t;

enum packet_mode {
	APPL_MODE_DROP,         /**< Packet is dropped */
	APPL_MODE_REPLY         /**< Packet is sent back */
};

/* helper funcs */
static int drop_err_pkts(odp_packet_t pkt_tbl[], unsigned len);
static void swap_pkt_addrs(odp_packet_t pkt_tbl[], unsigned len);
static void parse_args(int argc, char *argv[], appl_args_t *appl_args);
static void print_info(char *progname, appl_args_t *appl_args);
static void usage(char *progname);
static void configure_cos(odp_pktio_t pktio, appl_args_t *args);
static void configure_default_cos(odp_pktio_t pktio, appl_args_t *args);
static int convert_str_to_pmr_enum(char *token, odp_pmr_term_t *term,
				   uint32_t *offset);
static int parse_pmr_policy(appl_args_t *appl_args, char *argv[], char *optarg);

static inline
void print_cls_statistics(appl_args_t *args)
{
	int i;
	uint32_t timeout;
	int infinite = 0;

	printf("\n");
	for (i = 0; i < 40; i++)
		printf("-");

	printf("\n");

	/* print statistics */
	printf("CLASSIFIER EXAMPLE STATISTICS\n");
	for (i = 0; i < 40; i++)
		printf("-");

	printf("\n");
	printf("CONFIGURATION\n");
	printf("\n");
	printf("COS\tVALUE\t\tMASK\n");
	for (i = 0; i < 40; i++)
		printf("-");

	printf("\n");
	for (i = 0; i < args->policy_count - 1; i++) {
		printf("%s\t", args->stats[i].cos_name);
		printf("%s\t", args->stats[i].value);
		printf("%s\n", args->stats[i].mask);
	}

	printf("\n");
	printf("RECEIVED PACKETS\n");
	for (i = 0; i < 40; i++)
		printf("-");

	printf("\n");
	for (i = 0; i < args->policy_count; i++)
		printf("%-12s |", args->stats[i].cos_name);

	printf("Total Packets");
	printf("\n");
	for (i = 0; i < args->policy_count; i++)
		printf("%-6s %-6s|", "queue", "pool");

	printf("\n");

	timeout = args->time;

	/* Incase if default value is given for timeout
	   run the loop infinitely */
	if (timeout == 0)
		infinite = 1;

	for (; timeout > 0 || infinite; timeout--) {
		for (i = 0; i < args->policy_count; i++) {
			printf("%-6" PRIu64 " ",
			       odp_atomic_load_u64(&args->stats[i]
						   .queue_pkt_count));
			printf("%-6" PRIu64 "|",
			       odp_atomic_load_u64(&args->stats[i]
						   .pool_pkt_count));
		}

		printf("%-" PRIu64, odp_atomic_load_u64(&args->
							total_packets));

		sleep(1);
		printf("\r");
		fflush(stdout);
	}

	printf("\n");
}

static inline
int parse_ipv4_addr(const char *ipaddress, uint64_t *addr)
{
	uint32_t b[4];
	int converted;

	converted = sscanf(ipaddress,
			   "%" SCNu32 ".%" SCNu32 ".%" SCNu32 ".%" SCNu32 "",
			   &b[3], &b[2], &b[1], &b[0]);
	if (4 != converted)
		return -1;

	if ((b[0] > 255) || (b[1] > 255) || (b[2] > 255) || (b[3] > 255))
		return -1;

	*addr = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;

	return 0;
}

static inline
int parse_mask(const char *str, uint64_t *mask)
{
	uint64_t b;
	int ret;

	ret = sscanf(str, "%" SCNx64, &b);
	*mask = b;
	return ret != 1;
}

static
int parse_value(const char *str, uint64_t *val, uint32_t *val_sz)
{
	size_t len;
	size_t i;
	int converted;

	union {
		uint64_t u64;
		uint8_t	 u8[8];
	} buf = {.u64 = 0};

	len = strlen(str);
	if (len > 2 * sizeof(buf))
		return -1;

	for (i = 0; i < len; i += 2) {
		converted = sscanf(&str[i], "%2" SCNx8, &buf.u8[i / 2]);
		if (1 != converted)
			return -1;
	}

	*val = buf.u64;
	*val_sz = len / 2;
	return 0;
}

/**
 * Create a pktio handle, optionally associating a default input queue.
 *
 * @param dev Device name
 * @param pool Associated Packet Pool
 *
 * @return The handle of the created pktio object.
 * @retval ODP_PKTIO_INVALID if the create fails.
 */
static odp_pktio_t create_pktio(const char *dev, odp_pool_t pool)
{
	odp_pktio_t pktio;
	odp_queue_t inq_def;
	odp_queue_param_t qparam;
	char inq_name[ODP_QUEUE_NAME_LEN];
	int  ret;
	odp_pktio_param_t pktio_param;

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;

	/* Open a packet IO instance */
	pktio = odp_pktio_open(dev, pool, &pktio_param);
	if (pktio == ODP_PKTIO_INVALID) {
		if (odp_errno() == EPERM)
			EXAMPLE_ERR("Root level permission required\n");

		EXAMPLE_ERR("pktio create failed for %s\n", dev);
		exit(EXIT_FAILURE);
	}

	odp_queue_param_init(&qparam);
	qparam.type = ODP_QUEUE_TYPE_PKTIN;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	snprintf(inq_name, sizeof(inq_name), "%" PRIu64 "-pktio_inq_def",
		 odp_pktio_to_u64(pktio));
	inq_name[ODP_QUEUE_NAME_LEN - 1] = '\0';

	inq_def = odp_queue_create(inq_name, &qparam);
	if (inq_def == ODP_QUEUE_INVALID) {
		EXAMPLE_ERR("pktio inq create failed for %s\n", dev);
		exit(EXIT_FAILURE);
	}

	ret = odp_pktio_inq_setdef(pktio, inq_def);
	if (ret != 0) {
		EXAMPLE_ERR("default input-Q setup for %s\n", dev);
		exit(EXIT_FAILURE);
	}

	printf("  created pktio:%02" PRIu64
	       ", dev:%s, queue mode (ATOMIC queues)\n"
	       "  \tdefault pktio%02" PRIu64
	       "-INPUT queue:%" PRIu64 "\n",
	       odp_pktio_to_u64(pktio), dev,
	       odp_pktio_to_u64(pktio), odp_queue_to_u64(inq_def));

	return pktio;
}

/**
 * Worker threads to receive the packet
 *
 */
static void *pktio_receive_thread(void *arg)
{
	int thr;
	odp_queue_t outq_def;
	odp_packet_t pkt;
	odp_pool_t  pool;
	odp_event_t ev;
	unsigned long err_cnt = 0;
	odp_queue_t queue;
	int i;

	thr = odp_thread_id();

	appl_args_t *appl = (appl_args_t *)arg;
	global_statistics *stats;

	/* Loop packets */
	for (;; ) {
		odp_pktio_t pktio_tmp;

		/* Use schedule to get buf from any input queue */
		ev = odp_schedule(&queue, ODP_SCHED_WAIT);

		/* Loop back to receive packets incase of invalid event */
		if (odp_unlikely(ev == ODP_EVENT_INVALID))
			continue;

		pkt = odp_packet_from_event(ev);

		/* Total packets received */
		odp_atomic_inc_u64(&appl->total_packets);

		/* Drop packets with errors */
		if (odp_unlikely(drop_err_pkts(&pkt, 1) == 0)) {
			EXAMPLE_ERR("Drop frame - err_cnt:%lu\n", ++err_cnt);
			continue;
		}

		pktio_tmp = odp_packet_input(pkt);
		outq_def  = odp_pktio_outq_getdef(pktio_tmp);

		if (outq_def == ODP_QUEUE_INVALID) {
			EXAMPLE_ERR("  [%02i] Error: def output-Q query\n",
				    thr);
			return NULL;
		}

		pool = odp_packet_pool(pkt);

		/* Swap Eth MACs and possibly IP-addrs before sending back */
		swap_pkt_addrs(&pkt, 1);
		for (i = 0; i < MAX_PMR_COUNT; i++) {
			stats = &appl->stats[i];
			if (queue == stats->queue)
				odp_atomic_inc_u64(&stats->queue_pkt_count);

			if (pool == stats->pool)
				odp_atomic_inc_u64(&stats->pool_pkt_count);
		}

		if (appl->appl_mode == APPL_MODE_DROP) {
			odp_packet_free(pkt);
		} else if (odp_queue_enq(outq_def, ev)) {
			EXAMPLE_ERR("  [%i] Queue enqueue failed.\n",
				    thr);
			odp_packet_free(pkt);
			continue;
		}
	}

	return NULL;
}

static void configure_default_cos(odp_pktio_t pktio, appl_args_t *args)
{
	odp_queue_param_t qparam;
	const char *queue_name = "DefaultQueue";
	const char *pool_name  = "DefaultPool";
	const char *cos_name = "DefaultCos";
	odp_queue_t queue_default;
	odp_pool_t  pool_default;
	odp_cos_t   cos_default;
	odp_pool_param_t pool_params;
	odp_cls_cos_param_t cls_param;
	global_statistics  *stats = args->stats;

	odp_queue_param_init(&qparam);
	qparam.type = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	queue_default = odp_queue_create(queue_name, &qparam);
	if (queue_default == ODP_QUEUE_INVALID) {
		EXAMPLE_ERR("Error: default queue create failed.\n");
		exit(EXIT_FAILURE);
	}

	odp_pool_param_init(&pool_params);
	pool_params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	pool_params.pkt.len = SHM_PKT_POOL_BUF_SIZE;
	pool_params.pkt.num = SHM_PKT_POOL_SIZE / SHM_PKT_POOL_BUF_SIZE;
	pool_params.type = ODP_POOL_PACKET;
	pool_default = odp_pool_create(pool_name, &pool_params);

	if (pool_default == ODP_POOL_INVALID) {
		EXAMPLE_ERR("Error: default pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	odp_cls_cos_param_init(&cls_param);
	cls_param.pool	= pool_default;
	cls_param.queue = queue_default;
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_default = odp_cls_cos_create(cos_name, &cls_param);

	if (cos_default == ODP_COS_INVALID) {
		EXAMPLE_ERR("Error: default cos create failed.\n");
		exit(EXIT_FAILURE);
	}

	if (0 > odp_pktio_default_cos_set(pktio, cos_default)) {
		EXAMPLE_ERR("odp_pktio_default_cos_set failed");
		exit(EXIT_FAILURE);
	}

	stats[args->policy_count].cos = cos_default;

	/* add default queue to global stats */
	stats[args->policy_count].queue = queue_default;
	stats[args->policy_count].pool	= pool_default;
	snprintf(stats[args->policy_count].cos_name,
		 sizeof(stats[args->policy_count].cos_name),
		 "%s", cos_name);
	odp_atomic_init_u64(&stats[args->policy_count].queue_pkt_count, 0);
	odp_atomic_init_u64(&stats[args->policy_count].pool_pkt_count, 0);
	args->policy_count++;
}

static void configure_cos(odp_pktio_t pktio, appl_args_t *args)
{
	char cos_name[ODP_COS_NAME_LEN];
	char queue_name[ODP_QUEUE_NAME_LEN];
	char pool_name[ODP_POOL_NAME_LEN];
	odp_pool_param_t pool_params;
	odp_cls_cos_param_t cls_param;
	int i;
	global_statistics  *stats;
	odp_queue_param_t   qparam;

	for (i = 0; i < args->policy_count; i++) {
		stats = &args->stats[i];

		const odp_pmr_match_t match = {
			.term = stats->rule.term,
			.val  = &stats->rule.val,
			.mask = &stats->rule.mask,
			.val_sz = stats->rule.val_sz,
			.offset = stats->rule.offset
		};

		stats->pmr = odp_pmr_create(&match);
		odp_queue_param_init(&qparam);
		qparam.type = ODP_QUEUE_TYPE_SCHED;
		qparam.sched.prio  = i % odp_schedule_num_prio();
		qparam.sched.sync  = ODP_SCHED_SYNC_PARALLEL;
		qparam.sched.group = ODP_SCHED_GROUP_ALL;

		snprintf(queue_name, sizeof(queue_name), "%sQueue%d",
			 args->stats[i].cos_name, i);
		stats->queue = odp_queue_create(queue_name, &qparam);
		if (ODP_QUEUE_INVALID == stats->queue) {
			EXAMPLE_ERR("odp_queue_create failed");
			exit(EXIT_FAILURE);
		}

		odp_pool_param_init(&pool_params);
		pool_params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
		pool_params.pkt.len = SHM_PKT_POOL_BUF_SIZE;
		pool_params.pkt.num = SHM_PKT_POOL_SIZE /
				      SHM_PKT_POOL_BUF_SIZE;
		pool_params.type = ODP_POOL_PACKET;

		snprintf(pool_name, sizeof(pool_name), "%sPool%d",
			 args->stats[i].cos_name, i);
		stats->pool = odp_pool_create(pool_name, &pool_params);

		if (stats->pool == ODP_POOL_INVALID) {
			EXAMPLE_ERR("Error: default pool create failed.\n");
			exit(EXIT_FAILURE);
		}

		snprintf(cos_name, sizeof(cos_name), "CoS%s",
			 stats->cos_name);
		odp_cls_cos_param_init(&cls_param);
		cls_param.pool	= stats->pool;
		cls_param.queue = stats->queue;
		cls_param.drop_policy = ODP_COS_DROP_POOL;
		stats->cos = odp_cls_cos_create(cos_name, &cls_param);

		if (0 > odp_pktio_pmr_cos(stats->pmr, pktio, stats->cos)) {
			EXAMPLE_ERR("odp_pktio_pmr_cos failed");
			exit(EXIT_FAILURE);
		}

		odp_atomic_init_u64(&stats->queue_pkt_count, 0);
		odp_atomic_init_u64(&stats->pool_pkt_count, 0);
	}
}

/**
 * ODP Classifier example main function
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
	odp_pktio_t pktio;
	appl_args_t *args;
	odp_queue_t inq;
	odp_shm_t   shm;

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

	/* Reserve memory for args from shared mem */
	shm = odp_shm_reserve("cls_shm_args", sizeof(appl_args_t),
			      ODP_CACHE_LINE_SIZE, 0);

	if (shm == ODP_SHM_INVALID) {
		EXAMPLE_ERR("Error: shared mem reserve failed.\n");
		exit(EXIT_FAILURE);
	}

	args = odp_shm_addr(shm);

	if (args == NULL) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}

	memset(args, 0, sizeof(*args));

	/* Parse and store the application arguments */
	parse_args(argc, argv, args);

	/* Print both system and application information */
	print_info(NO_PATH(argv[0]), args);

	/* Default to system CPU count unless user specified */
	num_workers = MAX_WORKERS;
	if (args->cpu_count)
		num_workers = args->cpu_count;

	/* Get default worker cpumask */
	num_workers = odp_cpumask_default_worker(&cpumask, num_workers);
	(void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

	printf("num worker threads: %i\n", num_workers);
	printf("first CPU:          %i\n", odp_cpumask_first(&cpumask));
	printf("cpu mask:           %s\n", cpumaskstr);

	/* Create packet pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num = SHM_PKT_POOL_SIZE / SHM_PKT_POOL_BUF_SIZE;
	params.type = ODP_POOL_PACKET;

	pool = odp_pool_create("packet_pool", &params);

	if (pool == ODP_POOL_INVALID) {
		EXAMPLE_ERR("Error: packet pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	/* odp_pool_print(pool); */
	odp_atomic_init_u64(&args->total_packets, 0);

	/* create pktio per interface */
	pktio = create_pktio(args->if_name, pool);

	configure_cos(pktio, args);

	/* configure default Cos */
	configure_default_cos(pktio, args);

	if (odp_pktio_start(pktio)) {
		EXAMPLE_ERR("Error: unable to start pktio.\n");
		exit(EXIT_FAILURE);
	}

	/* Create and init worker threads */
	memset(thread_tbl, 0, sizeof(thread_tbl));

	cpu = odp_cpumask_first(&cpumask);
	for (i = 0; i < num_workers; ++i) {
		odp_cpumask_t thd_mask;

		/*
		 * Calls odp_thread_create(cpu) for each thread
		 */
		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, cpu);
		odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
					  pktio_receive_thread,
					  args, ODP_THREAD_WORKER);
		cpu = odp_cpumask_next(&cpumask, cpu);
	}

	print_cls_statistics(args);

	for (i = 0; i < args->policy_count; i++) {
		odp_cos_destroy(args->stats[i].cos);
		odp_pool_destroy(args->stats[i].pool);
		odp_queue_destroy(args->stats[i].queue);
		odp_pool_destroy(args->stats[i].pool);
	}

	free(args->if_name);
	odp_shm_free(shm);
	odp_pool_destroy(pool);
	inq = odp_pktio_inq_getdef(pktio);
	odp_pktio_inq_remdef(pktio);
	odp_queue_destroy(inq);
	odp_pktio_close(pktio);
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
			pkt_tbl[j - 1] = pkt;
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

static int convert_str_to_pmr_enum(char *token, odp_pmr_term_t *term,
				   uint32_t *offset)
{
	int ret = -1;

	if (NULL == token)
		return -1;

	if (!strcasecmp(token, "ODP_PMR_SIP_ADDR")) {
		*term = ODP_PMR_SIP_ADDR;
		ret = 0;
	} else {
		errno = 0;
		*offset = strtoul(token, NULL, 0);
		if (errno)
			return -1;

		*term = ODP_PMR_CUSTOM_FRAME;
		ret = 0;
	}

	return ret;
}

static int parse_pmr_policy(appl_args_t *appl_args, char *argv[], char *optarg)
{
	int policy_count;
	char *token;
	size_t len;
	odp_pmr_term_t term;
	global_statistics *stats;
	char *pmr_str;
	uint32_t offset;

	policy_count = appl_args->policy_count;
	stats = appl_args->stats;

	/* last array index is needed for default queue */
	if (policy_count >= MAX_PMR_COUNT - 1) {
		EXAMPLE_ERR("Maximum allowed PMR reached\n");
		return -1;
	}

	len = strlen(optarg);
	len++;
	pmr_str = malloc(len);
	strcpy(pmr_str, optarg);

	/* PMR TERM */
	token = strtok(pmr_str, ":");
	if (convert_str_to_pmr_enum(token, &term, &offset)) {
		EXAMPLE_ERR("Invalid ODP_PMR_TERM string\n");
		exit(EXIT_FAILURE);
	}

	stats[policy_count].rule.term = term;

	/* PMR value */
	switch (term) {
	case ODP_PMR_SIP_ADDR:
		token = strtok(NULL, ":");
		strncpy(stats[policy_count].value, token,
			DISPLAY_STRING_LEN - 1);
		parse_ipv4_addr(token, &stats[policy_count].rule.val);
		token = strtok(NULL, ":");
		strncpy(stats[policy_count].mask, token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats[policy_count].rule.mask);
		stats[policy_count].rule.val_sz = 4;
		stats[policy_count].rule.offset = 0;
		break;
	case ODP_PMR_CUSTOM_FRAME:
		token = strtok(NULL, ":");
		strncpy(stats[policy_count].value, token,
			DISPLAY_STRING_LEN - 1);
		parse_value(token, &stats[policy_count].rule.val,
			    &stats[policy_count].rule.val_sz);
		token = strtok(NULL, ":");
		strncpy(stats[policy_count].mask, token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats[policy_count].rule.mask);
		stats[policy_count].rule.offset = offset;
		break;
	default:
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Queue Name */
	token = strtok(NULL, ":");

	strncpy(stats[policy_count].cos_name, token, ODP_QUEUE_NAME_LEN - 1);
	appl_args->policy_count++;
	free(pmr_str);
	return 0;
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
	size_t len;
	int i;
	int interface = 0;
	int policy = 0;

	static struct option longopts[] = {
		{"count",     required_argument, NULL,
		 'c'},
		{"interface", required_argument, NULL,
		 'i'},                                          /* return 'i' */
		{"policy",    required_argument, NULL,
		 'p'},                                          /* return 'p' */
		{"mode",      required_argument, NULL,
		 'm'},                                          /* return 'm' */
		{"time",      required_argument, NULL,
		 't'},                                          /* return 't' */
		{"help",      no_argument,	 NULL,
		 'h'},                                          /* return 'h' */
		{NULL,	      0,		 NULL,
		 0}
	};

	while (1) {
		opt = getopt_long(argc, argv, "+c:t:i:p:m:t:h",
				  longopts, &long_index);

		if (opt == -1)
			break;  /* No more options */

		switch (opt) {
		case 'c':
			appl_args->cpu_count = atoi(optarg);
			break;
		case 'p':
			if (0 > parse_pmr_policy(appl_args, argv, optarg))
				continue;

			policy = 1;
			break;
		case 't':
			appl_args->time = atoi(optarg);
			break;
		case 'i':
			len = strlen(optarg);
			if (len == 0) {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}

			len += 1;       /* add room for '\0' */

			appl_args->if_name = malloc(len);
			if (appl_args->if_name == NULL) {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}

			strcpy(appl_args->if_name, optarg);
			interface = 1;
			break;

		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'm':
			i = atoi(optarg);
			if (i == 0)
				appl_args->appl_mode = APPL_MODE_DROP;
			else
				appl_args->appl_mode = APPL_MODE_REPLY;

			break;

		default:
			break;
		}
	}

	if (!interface || !policy) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (appl_args->if_name == NULL) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	optind = 1;             /* reset 'extern optind' from the getopt lib */
}

/**
 * Print system and application info
 */
static void print_info(char *progname, appl_args_t *appl_args)
{
	printf("\n"
	       "ODP system info\n"
	       "---------------\n"
	       "ODP API version: %s\n"
	       "CPU model:       %s\n"
	       "CPU freq (hz):   %" PRIu64 "\n"
	       "Cache line size: %i\n"
	       "CPU count:       %i\n"
	       "\n",
	       odp_version_api_str(), odp_cpu_model_str(),
	       odp_cpu_hz_max(), odp_sys_cache_line_size(),
	       odp_cpu_count());

	printf("Running ODP appl: \"%s\"\n"
	       "-----------------\n"
	       "Using IF:%s      ",
	       progname, appl_args->if_name);
	printf("\n\n");
	fflush(NULL);
}

/**
 * Prinf usage information
 */
static void usage(char *progname)
{
	printf("\n"
	       "OpenDataPlane Classifier example.\n"
	       "Usage: %s OPTIONS\n"
	       "  E.g. %s -i eth1 -m 0 -p \"ODP_PMR_SIP_ADDR:10.10.10.5:FFFFFFFF:queue1\" \\\n"
	       "\t\t\t-p \"ODP_PMR_SIP_ADDR:10.10.10.7:000000FF:queue2\" \\\n"
	       "\t\t\t-p \"ODP_PMR_SIP_ADDR:10.5.5.10:FFFFFF00:queue3\"\n"
	       "\n"
	       "For the above example configuration the following will be the packet distribution\n"
	       "queue1\t\tPackets with source ip address 10.10.10.5\n"
	       "queue2\t\tPackets with source ip address whose last 8 bits match 7\n"
	       "queue3\t\tPackets with source ip address in the subnet 10.5.5.0\n"
	       "\n"
	       "Mandatory OPTIONS:\n"
	       "  -i, --interface Eth interface\n"
	       "  -p, --policy [<odp_pmr_term_t>|<offset>]:<value>:<mask bits>:<queue name>\n"
	       "\n"
	       "<odp_pmr_term_t>	Packet Matching Rule defined with odp_pmr_term_t "
	       "for the policy\n"
	       "<offset>		Absolute offset in bytes from frame start to define a "
	       "ODP_PMR_CUSTOM_FRAME Packet Matching Rule for the policy\n"
	       "\n"
	       "<value>		PMR value to be matched.\n"
	       "\n"
	       "<mask  bits>		PMR mask bits to be applied on the PMR term value\n"
	       "\n"
	       "Optional OPTIONS\n"
	       "  -c, --count <number> CPU count.\n"
	       "                       default: CPU core count.\n"
	       "\n"
	       "  -m, --mode		0: Packet Drop mode. Received packets will be dropped\n"
	       "			!0: Packet ICMP mode. Received packets will be sent back\n"
	       "                       default: Packet Drop mode\n"
	       "\n"
	       " -t, --timeout		!0: Time for which the classifier will be run in seconds\n"
	       "			0: Runs in infinite loop\n"
	       "			default: Runs in infinite loop\n"
	       "\n"
	       "  -h, --help		Display help and exit.\n"
	       "\n", NO_PATH(
		       progname), NO_PATH(progname)
	      );
}
