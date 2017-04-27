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

#include <odp.h>
#include <odp/helper/linux.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>

#include "test_debug.h"
#include "odp_packet_internal.h"
#include "odp_packet_io_internal.h"

#ifndef PRINT
#define PRINT(fmt, ...)    \
	printf(" [Func: %s. Line: %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#endif

/** @def MAX_WORKERS
 * @brief Maximum number of worker threads
 */
#define MAX_WORKERS            32

/** @def SHM_PKT_POOL_SIZE
 * @brief Size of the shared memory block
 */
#define SHM_PKT_POOL_SIZE	(1024 * 8 * 10)
#define CLS_SHM_PKT_POOL_SIZE	(1024 * 4)

/** @def SHM_PKT_POOL_BUF_SIZE
 * @brief Buffer size of the packet pool buffer
 */
#define SHM_PKT_POOL_BUF_SIZE  1856

/** @def MAX_PKT_BURST
 * @brief Maximum number of packet in a burst
 */
#define MAX_PKT_BURST          32
#define MAX_CACHE_PKT_BURST    32

/** Maximum number of pktio queues per interface */
#define MAX_QUEUES             32

/** Maximum number of pktio interfaces */
#define MAX_PKTIOS             8

/** @def MAX_PMR_COUNT
 * @brief Maximum number of Classification Policy
 */
#define MAX_PMR_COUNT		16

/** @def MAX_MATCH_RULE_NUM
 * @brief number of rules to be matched
 */
#define MAX_MATCH_RULE_NUM	MAX_PMR_COUNT

/** @def DISPLAY_STRING_LEN
 * @brief Length of string used to display term value
 */
#define DISPLAY_STRING_LEN	32

#define MAX_PHY_QUEUE		16

#define GLOBAL_CLS_NAME		"global"

#define PHY_QUEUE_MASK		0x0000FFFF

#define THREAD_PRINT_INFO	32

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

typedef struct {
	odp_queue_t queue;	/**< Associated queue handle */
	odp_pool_t pool;	/**< Associated pool handle */
	odp_cos_t cos;		/**< Associated cos handle */
	odp_pmr_set_t pmr;		/**< Associated pmr handle */
	unsigned long queue_pkt_count; /**< count of received packets */
	unsigned long queue_drop_count; /**< count of received packets */
	char cos_name[ODP_COS_NAME_LEN];	/**< cos name */
	char if_name[ODP_COS_NAME_LEN / 2];
	char queue_name[ODP_COS_NAME_LEN / 2];
	int rule_count;			/**< match rule number of this queue */
	struct {
		odp_pmr_term_t term;	/**< odp pmr term value */
		uint64_t val;	/**< pmr term value */
		uint64_t mask;	/**< pmr term mask */
		uint32_t val_sz;	/**< size of the pmr term */
		uint32_t offset;	/**< pmr term offset */
	} rule[MAX_MATCH_RULE_NUM];

	/**< Display string for value */
	char value[MAX_MATCH_RULE_NUM][DISPLAY_STRING_LEN];
	/**< Display string for mask */
	char mask[MAX_MATCH_RULE_NUM][DISPLAY_STRING_LEN];
} global_statistics;

/**
 * Parsed command line application arguments
 */
typedef struct {
	global_statistics stats[MAX_PHY_QUEUE + 1][MAX_PMR_COUNT];
	int policy_count[MAX_PHY_QUEUE + 1];	/**< global policy count */
	int cls_create_count;
	unsigned int queue_mask;		/**< queue will create cls */
	unsigned long droped_packets;		/**< total received packets */
	unsigned long normal_queue_count;

	int if_count;		/**< Number of interfaces to be used */
	int num_workers;	/**< Number of worker threads */
	char **if_names;	/**< Array of pointers to interface names */
	pkt_in_mode_t mode;	/**< Packet input mode */
	int time;		/**< Time in seconds to run. */
	int accuracy;		/**< Number of seconds to get and print */
				/**< statistics */
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

	unsigned int queue_mask;
	int phy_queue_id;
	int sft_queue_id;
	int global_queue_id;
	char threadinfo[THREAD_PRINT_INFO];
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

static void usage(char *progname);

/************************** odp classfiter *********************/
static inline int bit_count(unsigned int data)
{
	int count = 0;

	while (data) {
		count += data & 0x01u;
		data >>= 1;
	}
	return count;
}

static inline int bit_offset(unsigned int data, int b)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (data & (1u << i)) {
			b--;
			if (b == 0)
				break;
		}
	}

	if (i == 32) {
		LOG_ERR("In data(%x) can't find %d bits\n", data, b);
		exit(-1);
	} else {
		return i;
	}
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
int parse_ipv4_port(const char *ipport, uint64_t *addr)
{
	uint16_t port;
	int ret;

	ret = sscanf(ipport, "%" SCNx16, &port);
	*addr = port;
	return ret != 1;
}

static inline
int parse_ipv4_proto(const char *proto, uint64_t *addr)
{
	uint8_t type;
	int ret;

	ret = sscanf(proto, "%" SCNx8, &type);
	*addr = type;
	return ret != 1;
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
		uint8_t u8[8];
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

static int convert_str_to_pmr_enum(char *token, odp_pmr_term_t *term,
				   uint32_t *offset)
{
	if (NULL == token)
		return -1;

	if (0 == strcasecmp(token, "ODP_PMR_IPPROTO")) {
		*term = ODP_PMR_IPPROTO;
	} else if (0 == strcasecmp(token, "ODP_PMR_SIP_ADDR")) {
		*term = ODP_PMR_SIP_ADDR;
	} else if (0 == strcasecmp(token, "ODP_PMR_DIP_ADDR")) {
		*term = ODP_PMR_DIP_ADDR;
	} else if (0 == strcasecmp(token, "ODP_PMR_TCP_SPORT")) {
		*term = ODP_PMR_TCP_SPORT;
	} else if (0 == strcasecmp(token, "ODP_PMR_TCP_DPORT")) {
		*term = ODP_PMR_TCP_DPORT;
	} else if (0 == strcasecmp(token, "ODP_PMR_CUSTOM_FRAME")) {
		errno = 0;
		*offset = strtoul(token, NULL, 0);
		if (errno)
			return -1;
		*term = ODP_PMR_CUSTOM_FRAME;
	} else {
		LOG_ERR("classification rule not supported now!");
		return -1;
	}

	return 0;
}

static int parse_pmr_policy(appl_args_t *appl_args,
			    char *argv[],
			    char *optarg)
{
	char *token;
	size_t len;
	odp_pmr_term_t term;
	global_statistics *stats;
	char *pmr_str;
	uint32_t offset;
	unsigned int phyqidx, sftqidx;
	char phyq_name[ODP_COS_NAME_LEN / 2];
	char queue_name[ODP_COS_NAME_LEN / 2];
	int i, ridx;
	int que_mask;
	int bitcount;

	PRINT("optarg = %s\n", optarg);
	que_mask = appl_args->queue_mask & PHY_QUEUE_MASK;
	if (que_mask == 0) {
		LOG_ERR("queue mask should set before rules\n");
		exit(EXIT_FAILURE);
	}

	bitcount = bit_count(que_mask);

	len = strlen(optarg);
	len++;
	pmr_str = malloc(len);
	strcpy(pmr_str, optarg);

	/* get pktio io name */
	token = strtok(pmr_str, ":");
	strcpy(phyq_name, token);
	phyq_name[ODP_COS_NAME_LEN / 2 - 1] = '\0';

	if (strcmp(phyq_name, GLOBAL_CLS_NAME) == 0) {
		phyqidx = GLOBAL_CLS_ID;
		stats = &appl_args->stats[phyqidx][0];

		if (strcmp(stats->if_name, GLOBAL_CLS_NAME) != 0) {
			strcpy(stats->if_name, GLOBAL_CLS_NAME);
			appl_args->queue_mask |= (1 << GLOBAL_CLS_ID);
		}
	} else {
		for (i = 0; i < bitcount; i++) {
			phyqidx = bit_offset(que_mask, i + 1);
			stats = &appl_args->stats[phyqidx][0];

			if (strcmp(phyq_name, stats->if_name) == 0)
				break;
		}

		if (i == bitcount) {
			phyqidx = bit_offset(que_mask,
					     ++appl_args->cls_create_count);
			stats = &appl_args->stats[phyqidx][0];
			strcpy(stats->if_name, phyq_name);

			if (appl_args->cls_create_count > MAX_PHY_QUEUE) {
				LOG_ERR("Maximum pktio reached\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	/* get queue name */
	token = strtok(NULL, ":");
	strcpy(queue_name, token);
	queue_name[ODP_COS_NAME_LEN / 2 - 1] = '\0';
	for (i = 0; i < appl_args->policy_count[phyqidx]; i++) {
		stats = &appl_args->stats[phyqidx][i];
		if (strcmp(queue_name, stats->queue_name) == 0)
			break;
	}

	sftqidx = i;
	if (sftqidx == appl_args->policy_count[phyqidx]) {
		stats = &appl_args->stats[phyqidx][sftqidx];
		strcpy(stats->queue_name, queue_name);
		if (++appl_args->policy_count[phyqidx] > MAX_PMR_COUNT - 1) {
			/* last array index is needed for default queue */
			LOG_ERR("Maximum allowed PMR reached\n");
			exit(EXIT_FAILURE);
		}

		/* Queue Name */
		strcat(stats->cos_name, queue_name);
		strcat(stats->cos_name, " @");
		strcat(stats->cos_name, phyq_name);

		PRINT("phyqidx %d,sftqidx %d,cos name : %s\n",
		      phyqidx, sftqidx, stats->cos_name);
	}

	stats = &appl_args->stats[phyqidx][sftqidx];

	/* PMR TERM */
	token = strtok(NULL, ":");
	if (convert_str_to_pmr_enum(token, &term, &offset)) {
		LOG_ERR("Invalid ODP_PMR_TERM string\n");
		exit(EXIT_FAILURE);
	}

	ridx = stats->rule_count;
	stats->rule[ridx].term = term;

	/* PMR value */
	switch (term)	{
	case ODP_PMR_DIP_ADDR:
	case ODP_PMR_SIP_ADDR:
		token = strtok(NULL, ":");
		strncpy(stats->value[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_ipv4_addr(token, &stats->rule[ridx].val);
		token = strtok(NULL, ":");
		strncpy(stats->mask[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats->rule[ridx].mask);
		stats->rule[ridx].val_sz = 4;
		stats->rule[ridx].offset = 0;
		break;
	case ODP_PMR_TCP_DPORT:
	case ODP_PMR_TCP_SPORT:
		token = strtok(NULL, ":");
		strncpy(stats->value[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_ipv4_port(token, &stats->rule[ridx].val);
		token = strtok(NULL, ":");
		strncpy(stats->mask[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats->rule[ridx].mask);
		stats->rule[ridx].val_sz = 2;
		stats->rule[ridx].offset = 0;
		break;
	case ODP_PMR_IPPROTO:
		token = strtok(NULL, ":");
		strncpy(stats->value[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_ipv4_proto(token, &stats->rule[ridx].val);
		token = strtok(NULL, ":");
		strncpy(stats->mask[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats->rule[ridx].mask);
		stats->rule[ridx].val_sz = 1;
		stats->rule[ridx].offset = 0;
		break;
	case ODP_PMR_CUSTOM_FRAME:
		token = strtok(NULL, ":");
		strncpy(stats->value[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_value(token, &stats->rule[ridx].val,
			    &stats->rule[ridx].val_sz);
		token = strtok(NULL, ":");
		strncpy(stats->mask[ridx], token,
			DISPLAY_STRING_LEN - 1);
		parse_mask(token, &stats->rule[ridx].mask);
		stats->rule[ridx].offset = offset;
		break;
	default:
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (++stats->rule_count > MAX_MATCH_RULE_NUM) {
		LOG_ERR("pmr rule is too more!\n");
		exit(EXIT_FAILURE);
	}

	free(pmr_str);
	return 0;
}

static void configure_default_cos(odp_pktio_t pktio,
				  appl_args_t *args, int qidx, odp_pool_t pool)
{
	odp_queue_param_t qparam;
	odp_pool_param_t pool_params;
	const char *queue_name = "DefaultQueue";
	const char *pool_name = "DefaultPool";
	const char *cos_name = "DefaultCos";
	odp_queue_t queue_default;
	odp_pool_t pool_default;
	odp_cos_t cos_default;
	odp_cls_cos_param_t cls_param;
	pktio_entry_t *pktio_entry;
	int qmask = gbl_args->appl.queue_mask;
	global_statistics *stats = &args->stats[qidx][args->policy_count[qidx]];
	int i, bak_index;

	odp_queue_param_init(&qparam);
	qparam.type = ODP_QUEUE_TYPE_PLAIN;

	queue_default = odp_queue_create(queue_name, &qparam);
	if (queue_default == ODP_QUEUE_INVALID) {
		LOG_ERR("Error: default queue create failed.\n");
		exit(EXIT_FAILURE);
	}

	if (NULL == pool) {
		odp_pool_param_init(&pool_params);
		pool_params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
		pool_params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
		pool_params.pkt.num     = CLS_SHM_PKT_POOL_SIZE;
		pool_params.type        = ODP_POOL_PACKET;
		pool_default = odp_pool_create(pool_name, &pool_params);
	} else {
		pool_default = pool;
	}

	if (pool_default == ODP_POOL_INVALID) {
		LOG_ERR("Error: default pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_default;
	cls_param.queue = queue_default;
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_default = odp_cls_cos_create(cos_name, &cls_param);

	if (cos_default == ODP_COS_INVALID) {
		LOG_ERR("Error: default cos create failed.\n");
		exit(EXIT_FAILURE);
	}

	/* temporary Solutions */
	pktio_entry = get_pktio_entry(pktio);
	bak_index = pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index;
	for (i = 0; i < MAX_PHY_QUEUE; i++) {
		if (qmask & (1 << i)) {
			pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1]
			.pktin.index = i;
			if (0 > odp_pktio_default_cos_set(pktio, cos_default)) {
				LOG_ERR("odp_pktio_default_cos_set failed");
				exit(EXIT_FAILURE);
			}
		}
	}
	pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index = bak_index;

	stats->cos = cos_default;
	/* add default queue to global stats */
	stats->queue = queue_default;
	stats->pool = pool_default;
	snprintf(stats->cos_name, sizeof(stats->cos_name), "%s", cos_name);
	stats->queue_pkt_count = 0;
	stats->queue_drop_count = 0;
	args->policy_count[qidx]++;
}

static void configure_cos(odp_pktio_t pktio,
			  appl_args_t *args, int phyq_id, odp_pool_t pool)
{
	char cos_name[ODP_COS_NAME_LEN];
	char queue_name[ODP_QUEUE_NAME_LEN];
	char pool_name[ODP_POOL_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pool_param_t pool_params;
	global_statistics *stats;
	odp_queue_param_t qparam;
	pktio_entry_t *pktio_entry;
	int bak_index;
	odp_pmr_set_t pmr_set;
	int rules;
	int i, j;

	for (i = 0; i < args->policy_count[phyq_id]; i++) {
		stats = &args->stats[phyq_id][i];
		rules = stats->rule_count;
		odp_pmr_match_t match[MAX_MATCH_RULE_NUM];

		for (j = 0; j < rules; j++) {
			match[j].term = stats->rule[j].term;
			match[j].val = &stats->rule[j].val;
			match[j].mask = &stats->rule[j].mask;
			match[j].val_sz = stats->rule[j].val_sz;
			match[j].offset = stats->rule[j].offset;
		}

		if (odp_pmr_match_set_create(rules, match, &pmr_set) < rules)
			LOG_DBG("waring! there are some rules droped!\n");
		stats->pmr = pmr_set;

		odp_queue_param_init(&qparam);
		qparam.type = ODP_QUEUE_TYPE_PLAIN;

		snprintf(queue_name, sizeof(queue_name), "%sQueue%d",
			 stats->cos_name, i);
		stats->queue = odp_queue_create(queue_name, &qparam);
		if (ODP_QUEUE_INVALID == stats->queue) {
			LOG_ERR("odp_queue_create failed");
			exit(EXIT_FAILURE);
		}

		if (NULL == pool) {
			odp_pool_param_init(&pool_params);
			pool_params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
			pool_params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
			pool_params.pkt.num     = CLS_SHM_PKT_POOL_SIZE;
			pool_params.type        = ODP_POOL_PACKET;

			snprintf(pool_name, sizeof(pool_name), "%sPool%d",
				 stats->cos_name, i);
			stats->pool = odp_pool_create(pool_name, &pool_params);
		} else {
			stats->pool = pool;
		}

		if (stats->pool == ODP_POOL_INVALID) {
			LOG_ERR("Error: pool create failed.\n");
			exit(EXIT_FAILURE);
		}

		snprintf(cos_name, sizeof(cos_name), "CoS%s",
			 stats->cos_name);
		odp_cls_cos_param_init(&cls_param);
		cls_param.pool = stats->pool;
		cls_param.queue = stats->queue;
		cls_param.drop_policy = ODP_COS_DROP_POOL;
		stats->cos = odp_cls_cos_create(cos_name, &cls_param);

		/* temporary Solutions */
		pktio_entry = get_pktio_entry(pktio);
		bak_index = pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1]
			.pktin.index;
		pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index =
			phyq_id;

		if (odp_pktio_pmr_match_set_cos(stats->pmr,
						pktio, stats->cos)) {
			LOG_ERR("odp_pktio_pmr_cos failed");
			exit(EXIT_FAILURE);
		}

		pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index =
			bak_index;

		stats->queue_pkt_count = 0;
		stats->queue_drop_count = 0;
	}
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

static void print_cls_statistics(appl_args_t *args, odp_pool_t pool)
{
	int i, j;
	uint32_t timeout;
	int infinite = 0;
	int pol_count = 0;
	int id;
	const char *cut_string = "-------------------------------------------";

	/* Wait for all threads to be ready*/
	odp_barrier_wait(&barrier);

	printf("%s\n", cut_string);

	printf("\n");
	/* print statistics */
	printf("CLASSIFIER EXAMPLE STATISTICS\n");
	printf("%s\n", cut_string);

	printf("CONFIGURATION\n");
	printf("\n");
	printf("COS\tVALUE\t\tMASK\n");
	printf("%s\n", cut_string);

	/* global queue */
	pol_count = args->policy_count[GLOBAL_CLS_ID];
	for (i = 0; i < pol_count; i++) {
		printf("%s\t", args->stats[GLOBAL_CLS_ID][i].cos_name);
		for (j = 0; j < args->stats[GLOBAL_CLS_ID][i].rule_count; j++) {
			printf("%s\t", args->stats[GLOBAL_CLS_ID][i].value[j]);
			printf("%s\t", args->stats[GLOBAL_CLS_ID][i].mask[j]);
		}
		printf("\n");
	}

	for (id = 0; id < MAX_PHY_QUEUE; id++) {
		pol_count = args->policy_count[id];
		for (i = 0; i < pol_count; i++) {
			printf("%s\t", args->stats[id][i].cos_name);
			for (j = 0; j < args->stats[id][i].rule_count; j++) {
				printf("%s\t", args->stats[id][i].value[j]);
				printf("%s\t", args->stats[id][i].mask[j]);
			}
			printf("\n");
		}
	}

	printf("\n");
	printf("RECEIVED PACKETS\n");

	printf("%s\n", cut_string);
	/* global cos */
	for (i = 0; i < args->policy_count[GLOBAL_CLS_ID]; i++)
		printf("%-16s |", args->stats[GLOBAL_CLS_ID][i].cos_name);

	/* normal queue */
	for (id = 0; id < MAX_PHY_QUEUE; id++)
		for (i = 0; i < args->policy_count[id]; i++)
			printf("%-16s |", args->stats[id][i].cos_name);

	printf("phycical queues");
	printf("\n");

	/* global */
	for (i = 0; i < args->policy_count[GLOBAL_CLS_ID]; i++)
		printf("%-8s %-8s|", "received", "droped");

	/* normal queue */
	for (id = 0; id < MAX_PHY_QUEUE; id++) {
		for (i = 0; i < args->policy_count[id]; i++)
			printf("%-8s %-8s|", "received", "droped");
	}

	/* phy queue */
	printf("%-8s %-8s", "received", "droped");
	printf("\n");

	timeout = args->time;

	/* Incase if default value is given for timeout
	run the loop infinitely */
	if (timeout == 0)
		infinite = 1;

	for (; timeout > 0 || infinite; timeout--) {
		/* global */
		for (i = 0; i < args->policy_count[GLOBAL_CLS_ID]; i++) {
			printf("%-8" PRIu64 " ",
			       args->stats[GLOBAL_CLS_ID][i].queue_pkt_count);
			printf("%-8" PRIu64 "|",
			       args->stats[GLOBAL_CLS_ID][i].queue_drop_count);
		}

		for (id = 0; id < MAX_PHY_QUEUE; id++) {
			for (i = 0; i < args->policy_count[id]; i++) {
				printf("%-8" PRIu64 " ",
				       args->stats[id][i].queue_pkt_count);
				printf("%-8" PRIu64 "|",
				       args->stats[id][i].queue_drop_count);
			}
		}

		printf("%-8" PRIu64 " ", args->normal_queue_count);
		printf("%-8" PRIu64, args->droped_packets);

		sleep(2);
		printf("\r");
		fflush(stdout);
	}

	printf("\n");
}

/********************** odp classfiter ***********************/

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
 * Frees packets with error and modifies pkt_tbl[] to only
 * contain packets with no detected errors.
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
 * Packet IO worker thread accessing IO resources directly
 *
 * @param arg  thread arguments of type 'thread_args_t *'
 */
static void *worker_thread(void *arg)
{
	int thr;
	odp_packet_t pkt_tbl[MAX_CACHE_PKT_BURST];
	odp_queue_t q_handle;
	odp_event_t event_tbl[MAX_PKT_BURST];
	global_statistics *stats;
	odp_pktout_queue_t pktout;
	thread_args_t *thr_args = arg;
	int phyid = thr_args->phy_queue_id;
	int sftid = thr_args->sft_queue_id;
	int events;
	int idxb;

	thr = odp_thread_id();

	stats = &gbl_args->appl.stats[phyid][sftid];
	q_handle = stats->queue;

	phyid = (phyid == GLOBAL_CLS_ID) ?
		(thr_args->global_queue_id) : (phyid);
	pktout.pktio = gbl_args->pktios[0].pktio;
	pktout.index = phyid;

	printf("thread[%02i] phy que[%i], sft que[%i], %s.\n",
	       thr, phyid, sftid, thr_args->threadinfo);
	odp_barrier_wait(&barrier);

	/* Loop packets */
	while (!exit_threads) {
		int sent;
		unsigned tx_drops;
		int i;

		events = odp_queue_deq_multi(q_handle,
					     event_tbl, MAX_PKT_BURST);
		if (odp_unlikely(events <= 0))
			continue;

		for (i = 0; i < events; ++i) {
			if (odp_event_type(event_tbl[i]) == ODP_EVENT_PACKET) {
				pkt_tbl[i] =
					odp_packet_from_event(event_tbl[i]);
			}
		}

		swap_pkt_addrs(pkt_tbl, events);
		stats->queue_pkt_count += events;

		sent = odp_pktio_send_queue(pktout, pkt_tbl, events);
		sent = odp_unlikely(sent < 0) ? 0 : sent;
		tx_drops = events - sent;

		if (odp_unlikely(tx_drops)) {
			/* Drop rejected packets */
			odp_packet_free_multi(&pkt_tbl[sent], tx_drops);
		}
	}

	/* Make sure that latest stat writes are visible to other threads */
	odp_mb_full();

	return NULL;
}

static void *classifier_thread(void *arg)
{
	int thr;
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	odp_pktin_queue_t pktin;
	int pkts;
	global_statistics *stats;
	thread_args_t *thr_args = arg;
	int phyid = thr_args->phy_queue_id;

	stats = &gbl_args->appl.stats[phyid][0];
	thr = odp_thread_id();
	printf("thread[%02i] phy que[%d], %s.\n",
	       thr, phyid, thr_args->threadinfo);
	odp_barrier_wait(&barrier);

	pktin.pktio = gbl_args->pktios[0].pktio;
	pktin.index = phyid;

	/* Loop packets and classify */
	while (!exit_threads) {
		pkts = odp_pktio_recv_queue(pktin, pkt_tbl, MAX_PKT_BURST);
		if (odp_unlikely(pkts <= 0))
			continue;

		/* classify failed */
		stats->queue_drop_count += pkts;
		odp_packet_free_multi(pkt_tbl, pkts);
	}
}

static void *queue_run_derict_mode(void *arg)
{
	int thr;
	odp_packet_t pkt_tbl[MAX_PKT_BURST];
	odp_pktin_queue_t pktin;
	odp_pktout_queue_t pktout;
	int pkts;
	int queue_id[MAX_PHY_QUEUE];
	int own_queue_count;
	thread_args_t *thr_args = arg;
	unsigned int queue_mask = thr_args->queue_mask;
	appl_args_t *app;
	int i;

	thr = odp_thread_id();
	printf("thread[%02i] %s.\n", thr, thr_args->threadinfo);
	odp_barrier_wait(&barrier);

	own_queue_count = bit_count(queue_mask);
	if ((own_queue_count > MAX_PHY_QUEUE) || (own_queue_count == 0)) {
		LOG_ERR("physical queue mask invalid! bit count = %d\n",
			own_queue_count);
		return NULL;
	}

	memset(queue_id, 0, sizeof(int) * MAX_PHY_QUEUE);
	for (i = 0; i < own_queue_count; i++)
		queue_id[i] = bit_offset(queue_mask, i + 1);

	pktin.pktio = gbl_args->pktios[0].pktio;
	pktin.index = queue_id[0];
	pktout.pktio = gbl_args->pktios[0].pktio;
	pktout.index = queue_id[0];
	app = &gbl_args->appl;

	/* Loop packets and classify */
	i = 0;
	while (!exit_threads) {
		int sent;
		unsigned tx_drops;

		pkts = odp_pktio_recv_queue(pktin, pkt_tbl, MAX_PKT_BURST);

		if (++i >= own_queue_count)
			i = 0;
		pktin.index = queue_id[i];

		if (odp_unlikely(pkts <= 0))
			continue;

		app->normal_queue_count += pkts;
		pktout.index = queue_id[i];

		swap_pkt_addrs(pkt_tbl, pkts);

		sent = odp_pktio_send_queue(pktout, pkt_tbl, pkts);
		sent = odp_unlikely(sent < 0) ? 0 : sent;
		tx_drops = pkts - sent;

		if (odp_unlikely(tx_drops)) {
			/* Drop rejected packets */
			app->droped_packets += tx_drops;
			odp_packet_free_multi(&pkt_tbl[sent], tx_drops);
		}
	}
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

	timeout = 2;

	do {
		pkts = 0;
		rx_drops = 0;
		tx_drops = 0;

		sleep(timeout);
		printf("\r");

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

			printf(" %" PRIu64 " rx drops, %" PRIu64 " tx drops",
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
 *  - workers process inequal number of interfaces, when M is not
 *  - divisible by N
 *  - needs only single queue per interface
 * otherwise
 *  - assign an interface to every Mth worker
 *  - interfaces are processed by inequal number of workers, when N is not
 *    divisible by M
 *  - tries to configure a queue per worker per interface
 *  - shares queues, if interface capability does not allows a
 *  - queue per worker
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
	       "  -m, --mode      0: Receive packets directly from pktio interface "
	       "		     (default)\n"
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
		{"policy", required_argument, NULL, 'p'},	/* cls rules */
		{"classifier", required_argument, NULL, 'f'},	/* cls number */
		{"queue_mask", required_argument, NULL, 'q'},	/* queue mask */
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
	appl_args->queue_mask = 0; /* don't create cls */

	while (1) {
		opt = getopt_long(argc, argv, "+c:+t:+a:i:m:p:f:q:d:s:e:h",
				  longopts, &long_index);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 't':
			appl_args->time = atoi(optarg);
			break;
		case 'a':
			appl_args->accuracy = atoi(optarg);
			break;
		/* parse classfiter pmr rules */
		case 'p':
			if (parse_pmr_policy(appl_args, argv, optarg))
				continue;
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
		case 'q':
			appl_args->queue_mask = atoi(optarg);
			if (appl_args->queue_mask > (1 << GLOBAL_CLS_ID)) {
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
	int i, j;
	int cpu;
	int num_workers;
	odp_shm_t shm;
	odp_cpumask_t cpumask;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	odph_ethaddr_t new_addr;
	odp_pool_param_t params;
	odp_pktio_t pktio;
	int ret;
	stats_t *stats;
	int if_count;
	unsigned int phyq_mask;
	int phyqidx, sftqidx;
	int bitcount;
	int policy_num;
	int normal_queue_workers;
	int cls_thread_num;

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

	/* Default one normal thread to rx and tx physical queue pkts */
	normal_queue_workers = 1;

	phyq_mask = gbl_args->appl.queue_mask;
	bitcount = bit_count(phyq_mask & PHY_QUEUE_MASK);
	if (bitcount != gbl_args->appl.cls_create_count) {
		LOG_ERR("Error: queue mask= %x,cls count = %d\n",
			bitcount, gbl_args->appl.cls_create_count);
		exit(EXIT_FAILURE);
	}

	cls_thread_num = 0;
	if (phyq_mask) {
		for (i = 0; i < MAX_PHY_QUEUE; i++) {
			policy_num = gbl_args->appl.policy_count[i];
			if (policy_num) {
				if (policy_num > ODP_PKTIO_MAX_PMR) {
					LOG_ERR("Error: solft queue of a physical queue is biger than max pmr supported.\n");
					exit(EXIT_FAILURE);
				}
				cls_thread_num += policy_num;
				/* one classify thread */
				cls_thread_num++;
			}
		}

		if (phyq_mask & (1 << GLOBAL_CLS_ID)) {
			cls_thread_num +=
				gbl_args->appl.policy_count[GLOBAL_CLS_ID];
		}

		/* one thread to deal default queue pkts */
		cls_thread_num++;
	}

	if (normal_queue_workers + cls_thread_num > MAX_WORKERS) {
		LOG_ERR("Error: workers and classifier thread is biger than MAX_WORKERS.\n");
		exit(EXIT_FAILURE);
	}

	/* Get default worker cpumask */
	num_workers = odp_cpumask_default_worker(&cpumask,
						 normal_queue_workers +
						 cls_thread_num);
	(void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

	PRINT("cpumaskstr = %s\n", cpumaskstr);

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
			new_addr.addr[0] = 0x00;
			new_addr.addr[1] = 0x01;
			new_addr.addr[2] = 0x02;
			new_addr.addr[3] = 0x03;
			new_addr.addr[4] = 0x04;
			new_addr.addr[5] = 0x05 + i;
			gbl_args->dst_eth_addr[i] = new_addr;
		}
	}

	gbl_args->pktios[i].pktio = ODP_PKTIO_INVALID;

	pktio = gbl_args->pktios[0].pktio;

	/* configure global Cos */
	if (phyq_mask & (1 << GLOBAL_CLS_ID)) {
		/* create soft multipule queue */
		PRINT("mask = %x\n", phyq_mask);
		configure_cos(pktio, &gbl_args->appl, GLOBAL_CLS_ID, pool);
	}

	for (i = 0; i < bitcount; i++) {
		phyqidx = bit_offset(phyq_mask & PHY_QUEUE_MASK, i + 1);
		configure_cos(pktio, &gbl_args->appl, phyqidx, pool);
	}

	/* configure default Cos */
	/* the first phy queue used for default queue */
	phyqidx = bit_offset(~phyq_mask & PHY_QUEUE_MASK, 1);
	configure_default_cos(pktio, &gbl_args->appl, phyqidx, pool);
	bind_queues();

	if (gbl_args->appl.mode == DIRECT_RECV ||
	    gbl_args->appl.mode == PLAIN_QUEUE)
		print_port_mapping();

	memset(thread_tbl, 0, sizeof(thread_tbl));

	stats = gbl_args->stats;

	odp_barrier_init(&barrier, num_workers + 1);

	/* Create worker threads */
	cpu = odp_cpumask_first(&cpumask);

	for (i = 0; i < normal_queue_workers; ++i) {
		odp_cpumask_t thd_mask;
		int queue_mask;

		gbl_args->thread[i].queue_mask = ~phyq_mask & PHY_QUEUE_MASK;
		strcpy(gbl_args->thread[i].threadinfo,
		       "physical queue rx/tx thread");
		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, cpu);
		odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
					  queue_run_derict_mode,
					  &gbl_args->thread[i],
					  ODP_THREAD_WORKER);
		cpu = odp_cpumask_next(&cpumask, cpu);
	}

	int thread_num = normal_queue_workers;
	int bitset = 1;

	for (j = 0; j < MAX_PHY_QUEUE; j++) {
		int phyid, sftid;

		policy_num = gbl_args->appl.policy_count[j];
		if (policy_num == 0)
			continue;

		thread_num += policy_num;
		sftid = 0;

		if (phyq_mask & (1 << j)) {
			phyid = bit_offset(phyq_mask & PHY_QUEUE_MASK,
					   bitset++);
			for (; i < thread_num; ++i) {
				odp_cpumask_t thd_mask;

				gbl_args->thread[i].phy_queue_id = phyid;
				gbl_args->thread[i].sft_queue_id = sftid++;
				strcpy(gbl_args->thread[i].threadinfo,
				       "solft queue rx thread");
				odp_cpumask_zero(&thd_mask);
				odp_cpumask_set(&thd_mask, cpu);
				odph_linux_pthread_create(&thread_tbl[i],
							  &thd_mask,
							  worker_thread,
							  &gbl_args->thread[i],
							  ODP_THREAD_WORKER);
				cpu = odp_cpumask_next(&cpumask, cpu);
			}

			/* classify thread */
			thread_num += 1;
			for (; i < thread_num; ++i) {
				odp_cpumask_t thd_mask;

				gbl_args->thread[i].phy_queue_id = phyid;
				strcpy(gbl_args->thread[i].threadinfo,
				       "solft queue classify thread");
				odp_cpumask_zero(&thd_mask);
				odp_cpumask_set(&thd_mask, cpu);
				odph_linux_pthread_create(&thread_tbl[i],
							  &thd_mask,
							  classifier_thread,
							  &gbl_args->thread[i],
							  ODP_THREAD_WORKER);
				cpu = odp_cpumask_next(&cpumask, cpu);
			}
		 } else {
			/* default queue will not create classify thread */
			static int default_queue_creat_count;

			if (policy_num > 1) {
				LOG_ERR("default queue is more than one!\n");
				exit(-1);
			}

			if (default_queue_creat_count > 0) {
				LOG_ERR("default queue created more than once!\n");
				exit(-1);
			}

			for (; i < thread_num; ++i) {
				odp_cpumask_t thd_mask;

				gbl_args->thread[i].phy_queue_id = j;
				gbl_args->thread[i].sft_queue_id = 0;
				strcpy(gbl_args->thread[i].threadinfo,
				       "default queue rx thread");
				odp_cpumask_zero(&thd_mask);
				odp_cpumask_set(&thd_mask, cpu);
				odph_linux_pthread_create(&thread_tbl[i],
							  &thd_mask,
							  worker_thread,
							  &gbl_args->thread[i],
							  ODP_THREAD_WORKER);
				cpu = odp_cpumask_next(&cpumask, cpu);
			}

			default_queue_creat_count++;
		 }
	}

	/* global queue */
	policy_num = gbl_args->appl.policy_count[GLOBAL_CLS_ID];
	if (policy_num) {
		int phyid, sftid;

		thread_num += policy_num;
		sftid = 0;
		phyid = GLOBAL_CLS_ID;
		for (; i < thread_num; ++i) {
			odp_cpumask_t thd_mask;

			gbl_args->thread[i].phy_queue_id = phyid;
			gbl_args->thread[i].sft_queue_id = sftid++;
			gbl_args->thread[i].global_queue_id =
				bit_offset(~phyq_mask & PHY_QUEUE_MASK, 3);
			strcpy(gbl_args->thread[i].threadinfo,
			       "global queue rx thread");
			odp_cpumask_zero(&thd_mask);
			odp_cpumask_set(&thd_mask, cpu);
			odph_linux_pthread_create(&thread_tbl[i], &thd_mask,
						  worker_thread,
						  &gbl_args->thread[i],
						  ODP_THREAD_WORKER);
			cpu = odp_cpumask_next(&cpumask, cpu);
		}
	}

	if (i != num_workers) {
		LOG_ERR("created thread %d,num_workers = %d!\n",
			i, num_workers);
		exit(-1);
	}

	for (i = 0; i < num_workers; ++i) {
		int dst_idx, num_pktio;
		odp_pktin_queue_t pktin;
		odp_pktout_queue_t pktout;
		thread_args_t *thr_args = &gbl_args->thread[i];
		int k;

		num_pktio = thr_args->num_pktio;

		for (k = 0; k < num_pktio; k++) {
			dst_idx   = thr_args->pktio[k].tx_idx;
			pktin     = thr_args->pktio[k].pktin;
			pktout    = thr_args->pktio[k].pktout;

			printf("cpu idx : %d, pktio idx : %d, pktio in : %d, pktio out : %d\n",
			       i, k, pktin.pktio, pktout.pktio);
		}
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

	print_cls_statistics(&gbl_args->appl, NULL);

	exit_threads = 1;

	/* Master thread waits for other threads to exit */
	odph_linux_pthread_join(thread_tbl, num_workers);

	free(gbl_args->appl.if_names);
	free(gbl_args->appl.if_str);
	printf("Exit\n\n");

	return ret;
}
