/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * @example odp_example_ipsec.c  ODP basic packet IO cross connect with IPsec test application
 */

/* enable strtok */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <example_debug.h>

#include <odp.h>

#include <odp/helper/linux.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/icmp.h>
#include <odp/helper/ipsec.h>

#include <stdbool.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include <odp_ipsec_misc.h>
#include <odp_ipsec_sa_db.h>
#include <odp_ipsec_sp_db.h>
#include <odp_ipsec_fwd_db.h>
#include <odp_ipsec_loop_db.h>
#include <odp_ipsec_cache.h>
#include <odp_ipsec_stream.h>

#define MAX_WORKERS     32   /**< maximum number of worker threads */

/**
 * Parsed command line application arguments
 */
typedef struct {
	int cpu_count;
	int if_count;		/**< Number of interfaces to be used */
	char **if_names;	/**< Array of pointers to interface names */
	crypto_api_mode_e mode;	/**< Crypto API preferred mode */
	odp_pool_t pool;	/**< Buffer pool for packet IO */
	char *if_str;		/**< Storage for interface names */
} appl_args_t;

/**
 * Grouping of both parsed CL args and thread specific args - alloc together
 */
typedef struct {
	/** Application (parsed) arguments */
	appl_args_t appl;
} args_t;

/* helper funcs */
static void parse_args(int argc, char *argv[], appl_args_t *appl_args);
static void print_info(char *progname, appl_args_t *appl_args);
static void usage(char *progname);

/** Global pointer to args */
static args_t *args;

/**
 * Buffer pool for packet IO
 */
#define SHM_PKT_POOL_BUF_COUNT 1024
#define SHM_PKT_POOL_BUF_SIZE  4096
#define SHM_PKT_POOL_SIZE      (SHM_PKT_POOL_BUF_COUNT * SHM_PKT_POOL_BUF_SIZE)

static odp_pool_t pkt_pool = ODP_POOL_INVALID;

/**
 * Buffer pool for crypto session output packets
 */
#define SHM_OUT_POOL_BUF_COUNT 1024
#define SHM_OUT_POOL_BUF_SIZE  4096
#define SHM_OUT_POOL_SIZE      (SHM_OUT_POOL_BUF_COUNT * SHM_OUT_POOL_BUF_SIZE)

static odp_pool_t out_pool = ODP_POOL_INVALID;

/** ATOMIC queue for IPsec sequence number assignment */
static odp_queue_t seqnumq;

/** ORDERED queue (eventually) for per packet crypto API completion events */
static odp_queue_t completionq;

/** Synchronize threads before packet processing begins */
static odp_barrier_t sync_barrier;

/**
 * Packet processing states/steps
 */
typedef enum {
	PKT_STATE_INPUT_VERIFY,        /**< Verify IPv4 and ETH */
	PKT_STATE_IPSEC_IN_CLASSIFY,   /**< Initiate input IPsec */
	PKT_STATE_IPSEC_IN_FINISH,     /**< Finish input IPsec */
	PKT_STATE_ROUTE_LOOKUP,        /**< Use DST IP to find output IF */
	PKT_STATE_IPSEC_OUT_CLASSIFY,  /**< Intiate output IPsec */
	PKT_STATE_IPSEC_OUT_SEQ,       /**< Assign IPsec sequence numbers */
	PKT_STATE_IPSEC_OUT_FINISH,    /**< Finish output IPsec */
	PKT_STATE_TRANSMIT,            /**< Send packet to output IF queue */
} pkt_state_e;

/**
 * Packet processing result codes
 */
typedef enum {
	PKT_CONTINUE,    /**< No events posted, keep processing */
	PKT_POSTED,      /**< Event posted, stop processing */
	PKT_DROP,        /**< Reason to drop detected, stop processing */
	PKT_DONE         /**< Finished with packet, stop processing */
} pkt_disposition_e;

/**
 * Per packet IPsec processing context
 */
typedef struct {
	uint8_t  ip_tos;         /**< Saved IP TOS value */
	uint16_t ip_frag_offset; /**< Saved IP flags value */
	uint8_t  ip_ttl;         /**< Saved IP TTL value */
	int      hdr_len;        /**< Length of IPsec headers */
	int      trl_len;        /**< Length of IPsec trailers */
	uint16_t tun_hdr_offset; /**< Offset of tunnel header from
				      buffer start */
	uint16_t ah_offset;      /**< Offset of AH header from buffer start */
	uint16_t esp_offset;     /**< Offset of ESP header from buffer start */

	/* Input only */
	uint32_t src_ip;         /**< SA source IP address */
	uint32_t dst_ip;         /**< SA dest IP address */

	/* Output only */
	odp_crypto_op_params_t params;  /**< Parameters for crypto call */
	uint32_t *ah_seq;               /**< AH sequence number location */
	uint32_t *esp_seq;              /**< ESP sequence number location */
	uint16_t *tun_hdr_id;           /**< Tunnel header ID > */
} ipsec_ctx_t;

/**
 * Per packet processing context
 */
typedef struct {
	odp_buffer_t buffer;  /**< Buffer for context */
	pkt_state_e  state;   /**< Next processing step */
	ipsec_ctx_t  ipsec;   /**< IPsec specific context */
	odp_queue_t  outq;    /**< transmit queue */
} pkt_ctx_t;

#define SHM_CTX_POOL_BUF_SIZE  (sizeof(pkt_ctx_t))
#define SHM_CTX_POOL_BUF_COUNT (SHM_PKT_POOL_BUF_COUNT + SHM_OUT_POOL_BUF_COUNT)
#define SHM_CTX_POOL_SIZE      (SHM_CTX_POOL_BUF_COUNT * SHM_CTX_POOL_BUF_SIZE)

static odp_pool_t ctx_pool = ODP_POOL_INVALID;

/**
 * Get per packet processing context from packet buffer
 *
 * @param pkt  Packet
 *
 * @return pointer to context area
 */
static
pkt_ctx_t *get_pkt_ctx_from_pkt(odp_packet_t pkt)
{
	return (pkt_ctx_t *)odp_packet_user_ptr(pkt);
}

/**
 * Allocate per packet processing context and associate it with
 * packet buffer
 *
 * @param pkt  Packet
 *
 * @return pointer to context area
 */
static
pkt_ctx_t *alloc_pkt_ctx(odp_packet_t pkt)
{
	odp_buffer_t ctx_buf = odp_buffer_alloc(ctx_pool);
	pkt_ctx_t *ctx;

	if (odp_unlikely(ODP_BUFFER_INVALID == ctx_buf))
		return NULL;

	ctx = odp_buffer_addr(ctx_buf);
	memset(ctx, 0, sizeof(*ctx));
	ctx->buffer = ctx_buf;
	odp_packet_user_ptr_set(pkt, ctx);

	return ctx;
}

/**
 * Release per packet resources
 *
 * @param ctx  Packet context
 */
static
void free_pkt_ctx(pkt_ctx_t *ctx)
{
	odp_buffer_free(ctx->buffer);
}

/**
 * Example supports either polling queues or using odp_schedule
 */
typedef odp_queue_t (*queue_create_func_t)
		    (const char *, const odp_queue_param_t *);
typedef odp_event_t (*schedule_func_t) (odp_queue_t *);

static queue_create_func_t queue_create;
static schedule_func_t schedule;

#define MAX_POLL_QUEUES 256

static odp_queue_t poll_queues[MAX_POLL_QUEUES];
static int num_polled_queues;

/**
 * odp_queue_create wrapper to enable polling versus scheduling
 */
static
odp_queue_t polled_odp_queue_create(const char *name,
				    const odp_queue_param_t *param)
{
	odp_queue_t my_queue;
	odp_queue_param_t qp;
	odp_queue_type_t type;

	odp_queue_param_init(&qp);
	if (param)
		memcpy(&qp, param, sizeof(odp_queue_param_t));

	type = qp.type;

	if (ODP_QUEUE_TYPE_SCHED == type) {
		printf("%s: change %s to PLAIN\n", __func__, name);
		qp.type = ODP_QUEUE_TYPE_PLAIN;
	}

	my_queue = odp_queue_create(name, &qp);

	if ((ODP_QUEUE_TYPE_SCHED == type) || (ODP_QUEUE_TYPE_PKTIN == type)) {
		poll_queues[num_polled_queues++] = my_queue;
		printf("%s : adding %"PRIu64"\n", __func__,
		       odp_queue_to_u64(my_queue));
	}

	return my_queue;
}

static inline
odp_event_t odp_schedule_cb(odp_queue_t *from)
{
	return odp_schedule(from, ODP_SCHED_WAIT);
}

/**
 * odp_schedule replacement to poll queues versus using ODP scheduler
 */
static
odp_event_t polled_odp_schedule_cb(odp_queue_t *from)
{
	int idx = 0;

	while (1) {
		if (idx >= num_polled_queues)
			idx = 0;

		odp_queue_t queue = poll_queues[idx++];
		odp_event_t buf;

		buf = odp_queue_deq(queue);

		if (ODP_EVENT_INVALID != buf) {
			*from = queue;
			return buf;
		}
	}

	*from = ODP_QUEUE_INVALID;
	return ODP_EVENT_INVALID;
}

/**
 * IPsec pre argument processing intialization
 */
static
void ipsec_init_pre(void)
{
	odp_queue_param_t qparam;
	odp_pool_param_t params;

	/*
	 * Create queues
	 *
	 *  - completion queue (should eventually be ORDERED)
	 *  - sequence number queue (must be ATOMIC)
	 */
	odp_queue_param_init(&qparam);
	qparam.type        = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio  = ODP_SCHED_PRIO_HIGHEST;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;

	completionq = queue_create("completion", &qparam);
	if (ODP_QUEUE_INVALID == completionq) {
		EXAMPLE_ERR("Error: completion queue creation failed\n");
		exit(EXIT_FAILURE);
	}

	qparam.type        = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio  = ODP_SCHED_PRIO_HIGHEST;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;

	seqnumq = queue_create("seqnum", &qparam);
	if (ODP_QUEUE_INVALID == seqnumq) {
		EXAMPLE_ERR("Error: sequence number queue creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Create output buffer pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_OUT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_OUT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_BUF_COUNT;
	params.type        = ODP_POOL_PACKET;

	out_pool = odp_pool_create("out_pool", &params);

	if (ODP_POOL_INVALID == out_pool) {
		EXAMPLE_ERR("Error: message pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize our data bases */
	init_sp_db();
	init_sa_db();
	init_tun_db();
	init_ipsec_cache();
}

/**
 * IPsec post argument processing intialization
 *
 * Resolve SP DB with SA DB and create corresponding IPsec cache entries
 *
 * @param api_mode  Mode to use when invoking per packet crypto API
 */
static
void ipsec_init_post(crypto_api_mode_e api_mode)
{
	sp_db_entry_t *entry;

	/* Attempt to find appropriate SA for each SP */
	for (entry = sp_db->list; NULL != entry; entry = entry->next) {
		sa_db_entry_t *cipher_sa = NULL;
		sa_db_entry_t *auth_sa = NULL;
		tun_db_entry_t *tun = NULL;

		if (entry->esp) {
			cipher_sa = find_sa_db_entry(&entry->src_subnet,
						     &entry->dst_subnet,
						     1);
			tun = find_tun_db_entry(cipher_sa->src_ip,
						cipher_sa->dst_ip);
		}
		if (entry->ah) {
			auth_sa = find_sa_db_entry(&entry->src_subnet,
						   &entry->dst_subnet,
						   0);
			tun = find_tun_db_entry(auth_sa->src_ip,
						auth_sa->dst_ip);
		}

		if (cipher_sa || auth_sa) {
			if (create_ipsec_cache_entry(cipher_sa,
						     auth_sa,
						     tun,
						     api_mode,
						     entry->input,
						     completionq,
						     out_pool)) {
				EXAMPLE_ERR("Error: IPSec cache entry failed.\n"
						);
				exit(EXIT_FAILURE);
			}
		} else {
			printf(" WARNING: SA not found for SP\n");
			dump_sp_db_entry(entry);
		}
	}
}

/**
 * Initialize loopback
 *
 * Initialize ODP queues to create our own idea of loopbacks, which allow
 * testing without physical interfaces.  Interface name string will be of
 * the format "loopX" where X is the decimal number of the interface.
 *
 * @param intf     Loopback interface name string
 */
static
void initialize_loop(char *intf)
{
	int idx;
	odp_queue_t outq_def;
	odp_queue_t inq_def;
	char queue_name[ODP_QUEUE_NAME_LEN];
	odp_queue_param_t qparam;
	uint8_t *mac;
	char mac_str[MAX_STRING];

	/* Derive loopback interface index */
	idx = loop_if_index(intf);
	if (idx < 0) {
		EXAMPLE_ERR("Error: loopback \"%s\" invalid\n", intf);
		exit(EXIT_FAILURE);
	}

	/* Create input queue */
	odp_queue_param_init(&qparam);
	qparam.type        = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	snprintf(queue_name, sizeof(queue_name), "%i-loop_inq_def", idx);
	queue_name[ODP_QUEUE_NAME_LEN - 1] = '\0';

	inq_def = queue_create(queue_name, &qparam);
	if (ODP_QUEUE_INVALID == inq_def) {
		EXAMPLE_ERR("Error: input queue creation failed for %s\n",
			    intf);
		exit(EXIT_FAILURE);
	}
	/* Create output queue */
	snprintf(queue_name, sizeof(queue_name), "%i-loop_outq_def", idx);
	queue_name[ODP_QUEUE_NAME_LEN - 1] = '\0';

	outq_def = queue_create(queue_name, NULL);
	if (ODP_QUEUE_INVALID == outq_def) {
		EXAMPLE_ERR("Error: output queue creation failed for %s\n",
			    intf);
		exit(EXIT_FAILURE);
	}

	/* Initialize the loopback DB entry */
	create_loopback_db_entry(idx, inq_def, outq_def, pkt_pool);
	mac = query_loopback_db_mac(idx);

	printf("Created loop:%02i, queue mode (ATOMIC queues)\n"
	       "          default loop%02i-INPUT queue:%" PRIu64 "\n"
	       "          default loop%02i-OUTPUT queue:%" PRIu64 "\n"
	       "          source mac address %s\n",
	       idx, idx, odp_queue_to_u64(inq_def), idx,
	       odp_queue_to_u64(outq_def),
	       mac_addr_str(mac_str, mac));

	/* Resolve any routes using this interface for output */
	resolve_fwd_db(intf, outq_def, mac);
}

/**
 * Initialize interface
 *
 * Initialize ODP pktio and queues, query MAC address and update
 * forwarding database.
 *
 * @param intf     Interface name string
 */
static
void initialize_intf(char *intf)
{
	odp_pktio_t pktio;
	odp_queue_t outq_def;
	odp_queue_t inq_def;
	char inq_name[ODP_QUEUE_NAME_LEN];
	odp_queue_param_t qparam;
	int ret;
	uint8_t src_mac[ODPH_ETHADDR_LEN];
	char src_mac_str[MAX_STRING];
	odp_pktio_param_t pktio_param;

	odp_pktio_param_init(&pktio_param);

	if (getenv("ODP_IPSEC_USE_POLL_QUEUES"))
		pktio_param.in_mode = ODP_PKTIN_MODE_QUEUE;
	else
		pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;

	/*
	 * Open a packet IO instance for thread and get default output queue
	 */
	pktio = odp_pktio_open(intf, pkt_pool, &pktio_param);
	if (ODP_PKTIO_INVALID == pktio) {
		EXAMPLE_ERR("Error: pktio create failed for %s\n", intf);
		exit(EXIT_FAILURE);
	}
	outq_def = odp_pktio_outq_getdef(pktio);

	/*
	 * Create and set the default INPUT queue associated with the 'pktio'
	 * resource
	 */
	odp_queue_param_init(&qparam);
	qparam.type        = ODP_QUEUE_TYPE_PKTIN;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	snprintf(inq_name, sizeof(inq_name), "%" PRIu64 "-pktio_inq_def",
		 odp_pktio_to_u64(pktio));
	inq_name[ODP_QUEUE_NAME_LEN - 1] = '\0';

	inq_def = queue_create(inq_name, &qparam);
	if (ODP_QUEUE_INVALID == inq_def) {
		EXAMPLE_ERR("Error: pktio queue creation failed for %s\n",
			    intf);
		exit(EXIT_FAILURE);
	}

	ret = odp_pktio_inq_setdef(pktio, inq_def);
	if (ret) {
		EXAMPLE_ERR("Error: default input-Q setup for %s\n", intf);
		exit(EXIT_FAILURE);
	}

	ret = odp_pktio_start(pktio);
	if (ret) {
		EXAMPLE_ERR("Error: unable to start %s\n", intf);
		exit(EXIT_FAILURE);
	}

	/* Read the source MAC address for this interface */
	ret = odp_pktio_mac_addr(pktio, src_mac, sizeof(src_mac));
	if (ret <= 0) {
		EXAMPLE_ERR("Error: failed during MAC address get for %s\n",
			    intf);
		exit(EXIT_FAILURE);
	}

	printf("Created pktio:%02" PRIu64 ", queue mode (ATOMIC queues)\n"
	       "          default pktio%02" PRIu64 "-INPUT queue:%" PRIu64 "\n"
	       "          source mac address %s\n",
	       odp_pktio_to_u64(pktio), odp_pktio_to_u64(pktio),
	       odp_queue_to_u64(inq_def),
	       mac_addr_str(src_mac_str, src_mac));

	/* Resolve any routes using this interface for output */
	resolve_fwd_db(intf, outq_def, src_mac);
}

/**
 * Packet Processing - Input verification
 *
 * @param pkt  Packet to inspect
 * @param ctx  Packet process context (not used)
 *
 * @return PKT_CONTINUE if good, supported packet else PKT_DROP
 */
static
pkt_disposition_e do_input_verify(odp_packet_t pkt,
				  pkt_ctx_t *ctx EXAMPLE_UNUSED)
{
	if (odp_unlikely(odp_packet_has_error(pkt)))
		return PKT_DROP;

	if (!odp_packet_has_eth(pkt))
		return PKT_DROP;

	if (!odp_packet_has_ipv4(pkt))
		return PKT_DROP;

	return PKT_CONTINUE;
}

/**
 * Packet Processing - Route lookup in forwarding database
 *
 * @param pkt  Packet to route
 * @param ctx  Packet process context
 *
 * @return PKT_CONTINUE if route found else PKT_DROP
 */
static
pkt_disposition_e do_route_fwd_db(odp_packet_t pkt, pkt_ctx_t *ctx)
{
	odph_ipv4hdr_t *ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	fwd_db_entry_t *entry;

	entry = find_fwd_db_entry(odp_be_to_cpu_32(ip->dst_addr));

	if (entry) {
		odph_ethhdr_t *eth =
			(odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);

		memcpy(&eth->dst, entry->dst_mac, ODPH_ETHADDR_LEN);
		memcpy(&eth->src, entry->src_mac, ODPH_ETHADDR_LEN);
		ctx->outq = entry->queue;

		return PKT_CONTINUE;
	}

	return PKT_DROP;
}

/**
 * Packet Processing - Input IPsec packet classification
 *
 * Verify the received packet has IPsec headers and a match
 * in the IPsec cache, if so issue crypto request else skip
 * input crypto.
 *
 * @param pkt   Packet to classify
 * @param ctx   Packet process context
 * @param skip  Pointer to return "skip" indication
 *
 * @return PKT_CONTINUE if done else PKT_POSTED
 */
static
pkt_disposition_e do_ipsec_in_classify(odp_packet_t pkt,
				       pkt_ctx_t *ctx,
				       odp_bool_t *skip,
				       odp_crypto_op_result_t *result)
{
	uint8_t *buf = odp_packet_data(pkt);
	odph_ipv4hdr_t *ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	int hdr_len;
	odph_ahhdr_t *ah = NULL;
	odph_esphdr_t *esp = NULL;
	ipsec_cache_entry_t *entry;
	odp_crypto_op_params_t params;
	odp_bool_t posted = 0;

	/* Default to skip IPsec */
	*skip = TRUE;

	/* Check IP header for IPSec protocols and look it up */
	hdr_len = locate_ipsec_headers(ip, &ah, &esp);
	if (!ah && !esp)
		return PKT_CONTINUE;
	entry = find_ipsec_cache_entry_in(odp_be_to_cpu_32(ip->src_addr),
					  odp_be_to_cpu_32(ip->dst_addr),
					  ah,
					  esp);
	if (!entry)
		return PKT_CONTINUE;

	/* Account for configured ESP IV length in packet */
	hdr_len += entry->esp.iv_len;

	/* Initialize parameters block */
	memset(&params, 0, sizeof(params));
	params.ctx = ctx;
	params.session = entry->state.session;
	params.pkt = pkt;
	params.out_pkt = entry->in_place ? pkt : ODP_PACKET_INVALID;

	/*Save everything to context */
	ctx->ipsec.ip_tos = ip->tos;
	ctx->ipsec.ip_frag_offset = odp_be_to_cpu_16(ip->frag_offset);
	ctx->ipsec.ip_ttl = ip->ttl;
	ctx->ipsec.ah_offset = ah ? ((uint8_t *)ah) - buf : 0;
	ctx->ipsec.esp_offset = esp ? ((uint8_t *)esp) - buf : 0;
	ctx->ipsec.hdr_len = hdr_len;
	ctx->ipsec.trl_len = 0;
	ctx->ipsec.src_ip = entry->src_ip;
	ctx->ipsec.dst_ip = entry->dst_ip;

	/*If authenticating, zero the mutable fields build the request */
	if (ah) {
		ip->chksum = 0;
		ip->tos = 0;
		ip->frag_offset = 0;
		ip->ttl = 0;

		params.auth_range.offset = ((uint8_t *)ip) - buf;
		params.auth_range.length = odp_be_to_cpu_16(ip->tot_len);
		params.hash_result_offset = ah->icv - buf;
	}

	/* If deciphering build request */
	if (esp) {
		params.cipher_range.offset = ipv4_data_p(ip) + hdr_len - buf;
		params.cipher_range.length = ipv4_data_len(ip) - hdr_len;
		params.override_iv_ptr = esp->iv;
	}

	/* Issue crypto request */
	*skip = FALSE;
	ctx->state = PKT_STATE_IPSEC_IN_FINISH;
	if (odp_crypto_operation(&params,
				 &posted,
				 result)) {
		abort();
	}
	return (posted) ? PKT_POSTED : PKT_CONTINUE;
}

/**
 * Packet Processing - Input IPsec packet processing cleanup
 *
 * @param pkt  Packet to handle
 * @param ctx  Packet process context
 *
 * @return PKT_CONTINUE if successful else PKT_DROP
 */
static
pkt_disposition_e do_ipsec_in_finish(odp_packet_t pkt,
				     pkt_ctx_t *ctx,
				     odp_crypto_op_result_t *result)
{
	odph_ipv4hdr_t *ip;
	int hdr_len = ctx->ipsec.hdr_len;
	int trl_len = 0;

	/* Check crypto result */
	if (!result->ok) {
		if (!is_crypto_compl_status_ok(&result->cipher_status))
			return PKT_DROP;
		if (!is_crypto_compl_status_ok(&result->auth_status))
			return PKT_DROP;
	}
	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);

	/*
	 * Finish auth
	 */
	if (ctx->ipsec.ah_offset) {
		uint8_t *buf = odp_packet_data(pkt);
		odph_ahhdr_t *ah;

		ah = (odph_ahhdr_t *)(ctx->ipsec.ah_offset + buf);
		ip->proto = ah->next_header;
	}

	/*
	 * Finish cipher by finding ESP trailer and processing
	 *
	 * NOTE: ESP authentication ICV not supported
	 */
	if (ctx->ipsec.esp_offset) {
		uint8_t *eop = (uint8_t *)(ip) + odp_be_to_cpu_16(ip->tot_len);
		odph_esptrl_t *esp_t = (odph_esptrl_t *)(eop) - 1;

		ip->proto = esp_t->next_header;
		trl_len += esp_t->pad_len + sizeof(*esp_t);
	}

	/* We have a tunneled IPv4 packet */
	if (ip->proto == ODPH_IPV4) {
		odp_packet_pull_head(pkt, sizeof(*ip) + hdr_len);
		odp_packet_pull_tail(pkt, trl_len);
		odph_ethhdr_t *eth;

		eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
		eth->type = ODPH_ETHTYPE_IPV4;
		ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);

		/* Check inbound policy */
		if ((ip->src_addr != ctx->ipsec.src_ip ||
		     ip->dst_addr != ctx->ipsec.dst_ip))
			return PKT_DROP;

		return PKT_CONTINUE;
	}

	/* Finalize the IPv4 header */
	ipv4_adjust_len(ip, -(hdr_len + trl_len));
	ip->ttl = ctx->ipsec.ip_ttl;
	ip->tos = ctx->ipsec.ip_tos;
	ip->frag_offset = odp_cpu_to_be_16(ctx->ipsec.ip_frag_offset);
	ip->chksum = 0;
	odph_ipv4_csum_update(pkt);

	/* Correct the packet length and move payload into position */
	memmove(ipv4_data_p(ip),
		ipv4_data_p(ip) + hdr_len,
		odp_be_to_cpu_16(ip->tot_len));
	odp_packet_pull_tail(pkt, hdr_len + trl_len);

	/* Fall through to next state */
	return PKT_CONTINUE;
}

/**
 * Packet Processing - Output IPsec packet classification
 *
 * Verify the outbound packet has a match in the IPsec cache,
 * if so issue prepend IPsec headers and prepare parameters
 * for crypto API call.  Post the packet to ATOMIC queue so
 * that sequence numbers can be applied in packet order as
 * the next processing step.
 *
 * @param pkt   Packet to classify
 * @param ctx   Packet process context
 * @param skip  Pointer to return "skip" indication
 *
 * @return PKT_CONTINUE if done else PKT_POSTED
 */
static
pkt_disposition_e do_ipsec_out_classify(odp_packet_t pkt,
					pkt_ctx_t *ctx,
					odp_bool_t *skip)
{
	uint8_t *buf = odp_packet_data(pkt);
	odph_ipv4hdr_t *ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	uint16_t ip_data_len = ipv4_data_len(ip);
	uint8_t *ip_data = ipv4_data_p(ip);
	ipsec_cache_entry_t *entry;
	odp_crypto_op_params_t params;
	int hdr_len = 0;
	int trl_len = 0;
	odph_ahhdr_t *ah = NULL;
	odph_esphdr_t *esp = NULL;

	/* Default to skip IPsec */
	*skip = TRUE;

	/* Find record */
	entry = find_ipsec_cache_entry_out(odp_be_to_cpu_32(ip->src_addr),
					   odp_be_to_cpu_32(ip->dst_addr),
					   ip->proto);
	if (!entry)
		return PKT_CONTINUE;

	/* Save IPv4 stuff */
	ctx->ipsec.ip_tos = ip->tos;
	ctx->ipsec.ip_frag_offset = odp_be_to_cpu_16(ip->frag_offset);
	ctx->ipsec.ip_ttl = ip->ttl;

	/* Initialize parameters block */
	memset(&params, 0, sizeof(params));
	params.session = entry->state.session;
	params.ctx = ctx;
	params.pkt = pkt;
	params.out_pkt = entry->in_place ? pkt : ODP_PACKET_INVALID;

	if (entry->mode == IPSEC_SA_MODE_TUNNEL) {
		hdr_len += sizeof(odph_ipv4hdr_t);
		ip_data = (uint8_t *)ip;
		ip_data_len += sizeof(odph_ipv4hdr_t);
	}
	/* Compute ah and esp, determine length of headers, move the data */
	if (entry->ah.alg) {
		ah = (odph_ahhdr_t *)(ip_data + hdr_len);
		hdr_len += sizeof(odph_ahhdr_t);
		hdr_len += entry->ah.icv_len;
	}
	if (entry->esp.alg) {
		esp = (odph_esphdr_t *)(ip_data + hdr_len);
		hdr_len += sizeof(odph_esphdr_t);
		hdr_len += entry->esp.iv_len;
	}
	memmove(ip_data + hdr_len, ip_data, ip_data_len);
	ip_data += hdr_len;

	/* update outer header in tunnel mode */
	if (entry->mode == IPSEC_SA_MODE_TUNNEL) {
		/* tunnel addresses */
		ip->src_addr = odp_cpu_to_be_32(entry->tun_src_ip);
		ip->dst_addr = odp_cpu_to_be_32(entry->tun_dst_ip);
	}

	/* For cipher, compute encrypt length, build headers and request */
	if (esp) {
		uint32_t encrypt_len;
		odph_esptrl_t *esp_t;

		encrypt_len = ESP_ENCODE_LEN(ip_data_len +
					     sizeof(*esp_t),
					     entry->esp.block_len);
		trl_len = encrypt_len - ip_data_len;

		esp->spi = odp_cpu_to_be_32(entry->esp.spi);
		memcpy(esp + 1, entry->state.iv, entry->esp.iv_len);

		esp_t = (odph_esptrl_t *)(ip_data + encrypt_len) - 1;
		esp_t->pad_len     = trl_len - sizeof(*esp_t);
		if (entry->mode == IPSEC_SA_MODE_TUNNEL)
			esp_t->next_header = ODPH_IPV4;
		else
			esp_t->next_header = ip->proto;
		ip->proto = ODPH_IPPROTO_ESP;

		params.cipher_range.offset = ip_data - buf;
		params.cipher_range.length = encrypt_len;
	}

	/* For authentication, build header clear mutables and build request */
	if (ah) {
		memset(ah, 0, sizeof(*ah) + entry->ah.icv_len);
		ah->spi = odp_cpu_to_be_32(entry->ah.spi);
		ah->ah_len = 1 + (entry->ah.icv_len / 4);
		if (entry->mode == IPSEC_SA_MODE_TUNNEL && !esp)
			ah->next_header = ODPH_IPV4;
		else
			ah->next_header = ip->proto;
		ip->proto = ODPH_IPPROTO_AH;

		ip->chksum = 0;
		ip->tos = 0;
		ip->frag_offset = 0;
		ip->ttl = 0;

		params.auth_range.offset = ((uint8_t *)ip) - buf;
		params.auth_range.length =
			odp_be_to_cpu_16(ip->tot_len) + (hdr_len + trl_len);
		params.hash_result_offset = ah->icv - buf;
	}

	/* Set IPv4 length before authentication */
	ipv4_adjust_len(ip, hdr_len + trl_len);
	if (!odp_packet_push_tail(pkt, hdr_len + trl_len))
		return PKT_DROP;

	/* Save remaining context */
	ctx->ipsec.hdr_len = hdr_len;
	ctx->ipsec.trl_len = trl_len;
	ctx->ipsec.ah_offset = ah ? ((uint8_t *)ah) - buf : 0;
	ctx->ipsec.esp_offset = esp ? ((uint8_t *)esp) - buf : 0;
	ctx->ipsec.tun_hdr_offset = (entry->mode == IPSEC_SA_MODE_TUNNEL) ?
				       ((uint8_t *)ip - buf) : 0;
	ctx->ipsec.ah_seq = &entry->state.ah_seq;
	ctx->ipsec.esp_seq = &entry->state.esp_seq;
	ctx->ipsec.tun_hdr_id = &entry->state.tun_hdr_id;
	memcpy(&ctx->ipsec.params, &params, sizeof(params));

	*skip = FALSE;

	return PKT_POSTED;
}

/**
 * Packet Processing - Output IPsec packet sequence number assignment
 *
 * Assign the necessary sequence numbers and then issue the crypto API call
 *
 * @param pkt  Packet to handle
 * @param ctx  Packet process context
 *
 * @return PKT_CONTINUE if done else PKT_POSTED
 */
static
pkt_disposition_e do_ipsec_out_seq(odp_packet_t pkt,
				   pkt_ctx_t *ctx,
				   odp_crypto_op_result_t *result)
{
	uint8_t *buf = odp_packet_data(pkt);
	odp_bool_t posted = 0;

	/* We were dispatched from atomic queue, assign sequence numbers */
	if (ctx->ipsec.ah_offset) {
		odph_ahhdr_t *ah;

		ah = (odph_ahhdr_t *)(ctx->ipsec.ah_offset + buf);
		ah->seq_no = odp_cpu_to_be_32((*ctx->ipsec.ah_seq)++);
	}
	if (ctx->ipsec.esp_offset) {
		odph_esphdr_t *esp;

		esp = (odph_esphdr_t *)(ctx->ipsec.esp_offset + buf);
		esp->seq_no = odp_cpu_to_be_32((*ctx->ipsec.esp_seq)++);
	}
	if (ctx->ipsec.tun_hdr_offset) {
		odph_ipv4hdr_t *ip;
		int ret;

		ip = (odph_ipv4hdr_t *)(ctx->ipsec.tun_hdr_offset + buf);
		ip->id = odp_cpu_to_be_16((*ctx->ipsec.tun_hdr_id)++);
		if (!ip->id) {
			/* re-init tunnel hdr id */
			ret = odp_random_data((uint8_t *)ctx->ipsec.tun_hdr_id,
					      sizeof(*ctx->ipsec.tun_hdr_id),
					      1);
			if (ret != sizeof(*ctx->ipsec.tun_hdr_id))
				abort();
		}
	}

	/* Issue crypto request */
	if (odp_crypto_operation(&ctx->ipsec.params,
				 &posted,
				 result)) {
		abort();
	}
	return (posted) ? PKT_POSTED : PKT_CONTINUE;
}

/**
 * Packet Processing - Output IPsec packet processing cleanup
 *
 * @param pkt  Packet to handle
 * @param ctx  Packet process context
 *
 * @return PKT_CONTINUE if successful else PKT_DROP
 */
static
pkt_disposition_e do_ipsec_out_finish(odp_packet_t pkt,
				      pkt_ctx_t *ctx,
				      odp_crypto_op_result_t *result)
{
	odph_ipv4hdr_t *ip;

	/* Check crypto result */
	if (!result->ok) {
		if (!is_crypto_compl_status_ok(&result->cipher_status))
			return PKT_DROP;
		if (!is_crypto_compl_status_ok(&result->auth_status))
			return PKT_DROP;
	}
	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);

	/* Finalize the IPv4 header */
	ip->ttl = ctx->ipsec.ip_ttl;
	ip->tos = ctx->ipsec.ip_tos;
	ip->frag_offset = odp_cpu_to_be_16(ctx->ipsec.ip_frag_offset);
	ip->chksum = 0;
	odph_ipv4_csum_update(pkt);

	/* Fall through to next state */
	return PKT_CONTINUE;
}

/**
 * Packet IO worker thread
 *
 * Loop calling odp_schedule to obtain packets from one of three sources,
 * and continue processing the packet based on the state stored in its
 * per packet context.
 *
 *  - Input interfaces (i.e. new work)
 *  - Sequence number assignment queue
 *  - Per packet crypto API completion queue
 *
 * @param arg  Required by "odph_linux_pthread_create", unused
 *
 * @return NULL (should never return)
 */
static
void *pktio_thread(void *arg EXAMPLE_UNUSED)
{
	int thr;
	odp_packet_t pkt;
	odp_event_t ev;
	unsigned long pkt_cnt = 0;

	thr = odp_thread_id();

	printf("Pktio thread [%02i] starts\n", thr);

	odp_barrier_wait(&sync_barrier);

	/* Loop packets */
	for (;;) {
		pkt_disposition_e rc;
		pkt_ctx_t   *ctx;
		odp_queue_t  dispatchq;
		odp_crypto_op_result_t result;

		/* Use schedule to get event from any input queue */
		ev = schedule(&dispatchq);

		/* Determine new work versus completion or sequence number */
		if (ODP_EVENT_PACKET == odp_event_type(ev)) {
			pkt = odp_packet_from_event(ev);
			if (seqnumq == dispatchq) {
				ctx = get_pkt_ctx_from_pkt(pkt);
			} else {
				ctx = alloc_pkt_ctx(pkt);
				if (!ctx) {
					odp_packet_free(pkt);
					continue;
				}
				ctx->state = PKT_STATE_INPUT_VERIFY;
			}
		} else if (ODP_EVENT_CRYPTO_COMPL == odp_event_type(ev)) {
			odp_crypto_compl_t compl;

			compl = odp_crypto_compl_from_event(ev);
			odp_crypto_compl_result(compl, &result);
			odp_crypto_compl_free(compl);
			pkt = result.pkt;
			ctx = result.ctx;
		} else {
			abort();
		}

		/*
		 * We now have a packet and its associated context. Loop here
		 * executing processing based on the current state value stored
		 * in the context as long as the processing return code
		 * indicates PKT_CONTINUE.
		 *
		 * For other return codes:
		 *
		 *  o PKT_DONE   - finished with the packet
		 *  o PKT_DROP   - something incorrect about the packet, drop it
		 *  o PKT_POSTED - packet/event has been queued for later
		 */
		do {
			odp_bool_t skip = FALSE;

			switch (ctx->state) {
			case PKT_STATE_INPUT_VERIFY:

				rc = do_input_verify(pkt, ctx);
				ctx->state = PKT_STATE_IPSEC_IN_CLASSIFY;
				break;

			case PKT_STATE_IPSEC_IN_CLASSIFY:

				ctx->state = PKT_STATE_ROUTE_LOOKUP;
				rc = do_ipsec_in_classify(pkt,
							  ctx,
							  &skip,
							  &result);
				break;

			case PKT_STATE_IPSEC_IN_FINISH:

				rc = do_ipsec_in_finish(pkt, ctx, &result);
				ctx->state = PKT_STATE_ROUTE_LOOKUP;
				break;

			case PKT_STATE_ROUTE_LOOKUP:

				rc = do_route_fwd_db(pkt, ctx);
				ctx->state = PKT_STATE_IPSEC_OUT_CLASSIFY;
				break;

			case PKT_STATE_IPSEC_OUT_CLASSIFY:

				rc = do_ipsec_out_classify(pkt,
							   ctx,
							   &skip);
				if (odp_unlikely(skip)) {
					ctx->state = PKT_STATE_TRANSMIT;
				} else {
					ctx->state = PKT_STATE_IPSEC_OUT_SEQ;
					if (odp_queue_enq(seqnumq, ev))
						rc = PKT_DROP;
				}
				break;

			case PKT_STATE_IPSEC_OUT_SEQ:

				ctx->state = PKT_STATE_IPSEC_OUT_FINISH;
				rc = do_ipsec_out_seq(pkt, ctx, &result);
				break;

			case PKT_STATE_IPSEC_OUT_FINISH:

				rc = do_ipsec_out_finish(pkt, ctx, &result);
				ctx->state = PKT_STATE_TRANSMIT;
				break;

			case PKT_STATE_TRANSMIT:

				if (odp_queue_enq(ctx->outq, ev)) {
					odp_event_free(ev);
					rc = PKT_DROP;
				} else {
					rc = PKT_DONE;
				}
				break;

			default:
				rc = PKT_DROP;
				break;
			}
		} while (PKT_CONTINUE == rc);

		/* Free context on drop or transmit */
		if ((PKT_DROP == rc) || (PKT_DONE == rc))
			free_pkt_ctx(ctx);

		/* Check for drop */
		if (PKT_DROP == rc)
			odp_packet_free(pkt);

		/* Print packet counts every once in a while */
		if (PKT_DONE == rc) {
			if (odp_unlikely(pkt_cnt++ % 1000 == 0)) {
				printf("  [%02i] pkt_cnt:%lu\n", thr, pkt_cnt);
				fflush(NULL);
			}
		}
	}

	/* unreachable */
	return NULL;
}

/**
 * ODP ipsec example main function
 */
int
main(int argc, char *argv[])
{
	odph_linux_pthread_t thread_tbl[MAX_WORKERS];
	int num_workers;
	int i;
	int stream_count;
	odp_shm_t shm;
	odp_cpumask_t cpumask;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	odp_pool_param_t params;

	/* create by default scheduled queues */
	queue_create = odp_queue_create;
	schedule = odp_schedule_cb;

	/* check for using poll queues */
	if (getenv("ODP_IPSEC_USE_POLL_QUEUES")) {
		queue_create = polled_odp_queue_create;
		schedule = polled_odp_schedule_cb;
	}

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
	shm = odp_shm_reserve("shm_args", sizeof(args_t), ODP_CACHE_LINE_SIZE,
			      0);

	args = odp_shm_addr(shm);

	if (NULL == args) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(args, 0, sizeof(*args));

	/* Must init our databases before parsing args */
	ipsec_init_pre();
	init_fwd_db();
	init_loopback_db();
	init_stream_db();

	/* Parse and store the application arguments */
	parse_args(argc, argv, &args->appl);

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

	/* Create a barrier to synchronize thread startup */
	odp_barrier_init(&sync_barrier, num_workers);

	/* Create packet buffer pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_BUF_COUNT;
	params.type        = ODP_POOL_PACKET;

	pkt_pool = odp_pool_create("packet_pool", &params);

	if (ODP_POOL_INVALID == pkt_pool) {
		EXAMPLE_ERR("Error: packet pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Create context buffer pool */
	params.buf.size  = SHM_CTX_POOL_BUF_SIZE;
	params.buf.align = 0;
	params.buf.num   = SHM_CTX_POOL_BUF_COUNT;
	params.type      = ODP_POOL_BUFFER;

	ctx_pool = odp_pool_create("ctx_pool", &params);

	if (ODP_POOL_INVALID == ctx_pool) {
		EXAMPLE_ERR("Error: context pool create failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Populate our IPsec cache */
	printf("Using %s mode for crypto API\n\n",
	       (CRYPTO_API_SYNC == args->appl.mode) ? "SYNC" :
	       (CRYPTO_API_ASYNC_IN_PLACE == args->appl.mode) ?
	       "ASYNC_IN_PLACE" : "ASYNC_NEW_BUFFER");
	ipsec_init_post(args->appl.mode);

	/* Initialize interfaces (which resolves FWD DB entries */
	for (i = 0; i < args->appl.if_count; i++) {
		if (!strncmp("loop", args->appl.if_names[i], strlen("loop")))
			initialize_loop(args->appl.if_names[i]);
		else
			initialize_intf(args->appl.if_names[i]);
	}

	/* If we have test streams build them before starting workers */
	resolve_stream_db();
	stream_count = create_stream_db_inputs();

	/*
	 * Create and init worker threads
	 */
	odph_linux_pthread_create(thread_tbl, &cpumask,
				  pktio_thread, NULL, ODP_THREAD_WORKER);

	/*
	 * If there are streams attempt to verify them else
	 * wait indefinitely
	 */
	if (stream_count) {
		odp_bool_t done;

		do {
			done = verify_stream_db_outputs();
			sleep(1);
		} while (!done);
		printf("All received\n");
	} else {
		odph_linux_pthread_join(thread_tbl, num_workers);
	}

	free(args->appl.if_names);
	free(args->appl.if_str);
	printf("Exit\n\n");

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
	char *token;
	size_t len;
	int rc = 0;
	int i;

	static struct option longopts[] = {
		{"count", required_argument, NULL, 'c'},
		{"interface", required_argument, NULL, 'i'},	/* return 'i' */
		{"mode", required_argument, NULL, 'm'},		/* return 'm' */
		{"route", required_argument, NULL, 'r'},	/* return 'r' */
		{"policy", required_argument, NULL, 'p'},	/* return 'p' */
		{"ah", required_argument, NULL, 'a'},		/* return 'a' */
		{"esp", required_argument, NULL, 'e'},		/* return 'e' */
		{"tunnel", required_argument, NULL, 't'},       /* return 't' */
		{"stream", required_argument, NULL, 's'},	/* return 's' */
		{"help", no_argument, NULL, 'h'},		/* return 'h' */
		{NULL, 0, NULL, 0}
	};

	printf("\nParsing command line options\n");

	appl_args->mode = 0;  /* turn off async crypto API by default */

	while (!rc) {
		opt = getopt_long(argc, argv, "+c:i:m:h:r:p:a:e:t:s:",
				  longopts, &long_index);

		if (-1 == opt)
			break;	/* No more options */

		switch (opt) {
		case 'c':
			appl_args->cpu_count = atoi(optarg);
			break;
			/* parse packet-io interface names */
		case 'i':
			len = strlen(optarg);
			if (0 == len) {
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

			if (0 == appl_args->if_count) {
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
			appl_args->mode = atoi(optarg);
			break;

		case 'r':
			rc = create_fwd_db_entry(optarg);
			break;

		case 'p':
			rc = create_sp_db_entry(optarg);
			break;

		case 'a':
			rc = create_sa_db_entry(optarg, FALSE);
			break;

		case 'e':
			rc = create_sa_db_entry(optarg, TRUE);
			break;

		case 't':
			rc = create_tun_db_entry(optarg);
			break;

		case 's':
			rc = create_stream_db_entry(optarg);
			break;

		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;

		default:
			break;
		}
	}

	if (rc) {
		printf("ERROR: failed parsing -%c option\n", opt);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (0 == appl_args->if_count) {
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

	printf("\n");

	dump_fwd_db();
	dump_sp_db();
	dump_sa_db();
	dump_tun_db();
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
	       " -i, --interface Eth interfaces (comma-separated, no spaces)\n"
	       " -m, --mode   0: SYNC\n"
	       "              1: ASYNC_IN_PLACE\n"
	       "              2: ASYNC_NEW_BUFFER\n"
	       "         Default: 0: SYNC api mode\n"
	       "\n"
	       "Routing / IPSec OPTIONS:\n"
	       " -r, --route SubNet:Intf:NextHopMAC\n"
	       " -p, --policy SrcSubNet:DstSubNet:(in|out):(ah|esp|both)\n"
	       " -e, --esp SrcIP:DstIP:(3des|null):SPI:Key192\n"
	       " -a, --ah SrcIP:DstIP:(sha256|md5|null):SPI:Key(256|128)\n"
	       "\n"
	       "  Where: NextHopMAC is raw hex/dot notation, i.e. 03.BA.44.9A.CE.02\n"
	       "         IP is decimal/dot notation, i.e. 192.168.1.1\n"
	       "         SubNet is decimal/dot/slash notation, i.e 192.168.0.0/16\n"
	       "         SPI is raw hex, 32 bits\n"
	       "         KeyXXX is raw hex, XXX bits long\n"
	       "\n"
	       "  Examples:\n"
	       "     -r 192.168.222.0/24:p8p1:08.00.27.F5.8B.DB\n"
	       "     -p 192.168.111.0/24:192.168.222.0/24:out:esp\n"
	       "     -e 192.168.111.2:192.168.222.2:3des:201:656c8523255ccc23a66c1917aa0cf30991fce83532a4b224\n"
	       "     -a 192.168.111.2:192.168.222.2:md5:201:a731649644c5dee92cbd9c2e7e188ee6\n"
	       "\n"
	       "Optional OPTIONS\n"
	       "  -c, --count <number> CPU count.\n"
	       "  -h, --help           Display help and exit.\n"
	       " environment variables: ODP_PKTIO_DISABLE_NETMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMAP\n"
	       "                        ODP_PKTIO_DISABLE_SOCKET_MMSG\n"
	       " can be used to advanced pkt I/O selection for linux-generic\n"
	       "                        ODP_IPSEC_USE_POLL_QUEUES\n"
	       " to enable use of poll queues instead of scheduled (default)\n"
	       "                        ODP_IPSEC_STREAM_VERIFY_MDEQ\n"
	       " to enable use of multiple dequeue for queue draining during\n"
	       " stream verification instead of single dequeue (default)\n"
	       "\n", NO_PATH(progname), NO_PATH(progname)
		);
}
