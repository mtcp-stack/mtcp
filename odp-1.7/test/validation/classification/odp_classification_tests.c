/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include "odp_classification_testsuites.h"
#include "classification.h"
#include <odp_cunit_common.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/udp.h>
#include <odp/helper/tcp.h>

static odp_cos_t cos_list[CLS_ENTRIES];
static odp_pmr_t pmr_list[CLS_ENTRIES];
static odp_queue_t queue_list[CLS_ENTRIES];
static odp_pool_t pool_list[CLS_ENTRIES];
static odp_pmr_set_t pmr_set;

static odp_pool_t pool_default;
static odp_pktio_t pktio_loop;

/** sequence number of IP packets */
odp_atomic_u32_t seq;

int classification_suite_init(void)
{
	odp_queue_t inq_def;
	odp_queue_param_t qparam;
	char queuename[ODP_QUEUE_NAME_LEN];
	int i;
	int ret;
	odp_pktio_param_t pktio_param;

	pool_default = pool_create("classification_pool");
	if (ODP_POOL_INVALID == pool_default) {
		fprintf(stderr, "Packet pool creation failed.\n");
		return -1;
	}

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;

	pktio_loop = odp_pktio_open("loop", pool_default, &pktio_param);
	if (pktio_loop == ODP_PKTIO_INVALID) {
		ret = odp_pool_destroy(pool_default);
		if (ret)
			fprintf(stderr, "unable to destroy pool.\n");
		return -1;
	}

	odp_queue_param_init(&qparam);
	qparam.type        = ODP_QUEUE_TYPE_PKTIN;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;

	sprintf(queuename, "%s", "inq_loop");
	inq_def = odp_queue_create(queuename, &qparam);
	odp_pktio_inq_setdef(pktio_loop, inq_def);

	for (i = 0; i < CLS_ENTRIES; i++)
		cos_list[i] = ODP_COS_INVALID;

	for (i = 0; i < CLS_ENTRIES; i++)
		pmr_list[i] = ODP_PMR_INVAL;

	for (i = 0; i < CLS_ENTRIES; i++)
		queue_list[i] = ODP_QUEUE_INVALID;

	for (i = 0; i < CLS_ENTRIES; i++)
		pool_list[i] = ODP_POOL_INVALID;

	odp_atomic_init_u32(&seq, 0);

	ret = odp_pktio_start(pktio_loop);
	if (ret) {
		fprintf(stderr, "unable to start loop\n");
		return -1;
	}

	return 0;
}

int classification_suite_term(void)
{
	int i;
	int retcode = 0;

	if (0 >	destroy_inq(pktio_loop)) {
		fprintf(stderr, "destroy pktio inq failed.\n");
		retcode = -1;
	}

	if (0 > odp_pktio_close(pktio_loop)) {
		fprintf(stderr, "pktio close failed.\n");
		retcode = -1;
	}

	if (0 != odp_pool_destroy(pool_default)) {
		fprintf(stderr, "pool_default destroy failed.\n");
		retcode = -1;
	}

	for (i = 0; i < CLS_ENTRIES; i++)
		odp_cos_destroy(cos_list[i]);

	for (i = 0; i < CLS_ENTRIES; i++)
		odp_pmr_destroy(pmr_list[i]);

	for (i = 0; i < CLS_ENTRIES; i++)
		odp_queue_destroy(queue_list[i]);

	for (i = 0; i < CLS_ENTRIES; i++)
		odp_pool_destroy(pool_list[i]);

	return retcode;
}

void configure_cls_pmr_chain(void)
{
	/* PKTIO --> PMR_SRC(SRC IP ADDR) --> PMR_DST (TCP SPORT) */

	/* Packet matching only the SRC IP ADDR should be delivered
	in queue[CLS_PMR_CHAIN_SRC] and a packet matching both SRC IP ADDR and
	TCP SPORT should be delivered to queue[CLS_PMR_CHAIN_DST] */

	uint16_t val;
	uint16_t maskport;
	int retval;
	char cosname[ODP_QUEUE_NAME_LEN];
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;
	char queuename[ODP_QUEUE_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];
	uint32_t addr;
	uint32_t mask;
	odp_pmr_match_t match;


	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_NORMAL;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	qparam.sched.lock_count = ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE;
	sprintf(queuename, "%s", "SrcQueue");

	queue_list[CLS_PMR_CHAIN_SRC] = odp_queue_create(queuename, &qparam);

	CU_ASSERT_FATAL(queue_list[CLS_PMR_CHAIN_SRC] != ODP_QUEUE_INVALID);

	sprintf(poolname, "%s", "SrcPool");
	pool_list[CLS_PMR_CHAIN_SRC] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_PMR_CHAIN_SRC] != ODP_POOL_INVALID);

	sprintf(cosname, "SrcCos");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_PMR_CHAIN_SRC];
	cls_param.queue = queue_list[CLS_PMR_CHAIN_SRC];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_PMR_CHAIN_SRC] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_PMR_CHAIN_SRC] != ODP_COS_INVALID);


	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_NORMAL;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	sprintf(queuename, "%s", "DstQueue");

	queue_list[CLS_PMR_CHAIN_DST] = odp_queue_create(queuename, &qparam);
	CU_ASSERT_FATAL(queue_list[CLS_PMR_CHAIN_DST] != ODP_QUEUE_INVALID);

	sprintf(poolname, "%s", "DstPool");
	pool_list[CLS_PMR_CHAIN_DST] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_PMR_CHAIN_DST] != ODP_POOL_INVALID);

	sprintf(cosname, "DstCos");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_PMR_CHAIN_DST];
	cls_param.queue = queue_list[CLS_PMR_CHAIN_DST];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_PMR_CHAIN_DST] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_PMR_CHAIN_DST] != ODP_COS_INVALID);

	parse_ipv4_string(CLS_PMR_CHAIN_SADDR, &addr, &mask);
	match.term = ODP_PMR_SIP_ADDR;
	match.val = &addr;
	match.mask = &mask;
	match.val_sz = sizeof(addr);
	pmr_list[CLS_PMR_CHAIN_SRC] = odp_pmr_create(&match);
	CU_ASSERT_FATAL(pmr_list[CLS_PMR_CHAIN_SRC] != ODP_PMR_INVAL);

	val = CLS_PMR_CHAIN_PORT;
	maskport = 0xffff;
	match.term = find_first_supported_l3_pmr();
	match.val = &val;
	match.mask = &maskport;
	match.val_sz = sizeof(val);
	pmr_list[CLS_PMR_CHAIN_DST] = odp_pmr_create(&match);
	CU_ASSERT_FATAL(pmr_list[CLS_PMR_CHAIN_DST] != ODP_PMR_INVAL);

	retval = odp_pktio_pmr_cos(pmr_list[CLS_PMR_CHAIN_SRC], pktio_loop,
				   cos_list[CLS_PMR_CHAIN_SRC]);
	CU_ASSERT(retval == 0);

	retval = odp_cos_pmr_cos(pmr_list[CLS_PMR_CHAIN_DST],
				 cos_list[CLS_PMR_CHAIN_SRC],
				 cos_list[CLS_PMR_CHAIN_DST]);
	CU_ASSERT(retval == 0);
}

void test_cls_pmr_chain(void)
{
	odp_packet_t pkt;
	odph_ipv4hdr_t *ip;
	odp_queue_t queue;
	odp_pool_t pool;
	uint32_t addr = 0;
	uint32_t mask;
	uint32_t seqno = 0;

	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	parse_ipv4_string(CLS_PMR_CHAIN_SADDR, &addr, &mask);
	ip->src_addr = odp_cpu_to_be_32(addr);
	ip->chksum = 0;
	ip->chksum = odph_ipv4_csum_update(pkt);

	set_first_supported_pmr_port(pkt, CLS_PMR_CHAIN_PORT);

	enqueue_pktio_interface(pkt, pktio_loop);

	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(queue == queue_list[CLS_PMR_CHAIN_DST]);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_PMR_CHAIN_DST]);
	odp_packet_free(pkt);

	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	parse_ipv4_string(CLS_PMR_CHAIN_SADDR, &addr, &mask);
	ip->src_addr = odp_cpu_to_be_32(addr);
	ip->chksum = 0;
	ip->chksum = odph_ipv4_csum_update(pkt);

	enqueue_pktio_interface(pkt, pktio_loop);
	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(queue == queue_list[CLS_PMR_CHAIN_SRC]);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_PMR_CHAIN_SRC]);
	odp_packet_free(pkt);
}

void configure_pktio_default_cos(void)
{
	int retval;
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;
	char cosname[ODP_COS_NAME_LEN];
	char queuename[ODP_QUEUE_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];

	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	sprintf(queuename, "%s", "DefaultQueue");
	queue_list[CLS_DEFAULT] = odp_queue_create(queuename, &qparam);
	CU_ASSERT_FATAL(queue_list[CLS_DEFAULT] != ODP_QUEUE_INVALID);

	sprintf(poolname, "DefaultPool");
	pool_list[CLS_DEFAULT] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_DEFAULT] != ODP_POOL_INVALID);

	sprintf(cosname, "DefaultCoS");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_DEFAULT];
	cls_param.queue = queue_list[CLS_DEFAULT];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_DEFAULT] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_DEFAULT] != ODP_COS_INVALID);

	retval = odp_pktio_default_cos_set(pktio_loop, cos_list[CLS_DEFAULT]);
	CU_ASSERT(retval == 0);
}

void test_pktio_default_cos(void)
{
	odp_packet_t pkt;
	odp_queue_t queue;
	uint32_t seqno = 0;
	odp_pool_t pool;
	/* create a default packet */
	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	enqueue_pktio_interface(pkt, pktio_loop);

	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	/* Default packet should be received in default queue */
	CU_ASSERT(queue == queue_list[CLS_DEFAULT]);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_DEFAULT]);

	odp_packet_free(pkt);
}

void configure_pktio_error_cos(void)
{
	int retval;
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;
	char queuename[ODP_QUEUE_NAME_LEN];
	char cosname[ODP_COS_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];

	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_LOWEST;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	sprintf(queuename, "%s", "ErrorCos");

	queue_list[CLS_ERROR] = odp_queue_create(queuename, &qparam);
	CU_ASSERT_FATAL(queue_list[CLS_ERROR] != ODP_QUEUE_INVALID);

	sprintf(poolname, "ErrorPool");
	pool_list[CLS_ERROR] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_ERROR] != ODP_POOL_INVALID);

	sprintf(cosname, "%s", "ErrorCos");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_ERROR];
	cls_param.queue = queue_list[CLS_ERROR];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_ERROR] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_ERROR] != ODP_COS_INVALID);

	retval = odp_pktio_error_cos_set(pktio_loop, cos_list[CLS_ERROR]);
	CU_ASSERT(retval == 0);
}

void test_pktio_error_cos(void)
{
	odp_queue_t queue;
	odp_packet_t pkt;
	odp_pool_t pool;

	/*Create an error packet */
	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	odph_ipv4hdr_t *ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);

	/* Incorrect IpV4 version */
	ip->ver_ihl = 8 << 4 | ODPH_IPV4HDR_IHL_MIN;
	ip->chksum = 0;
	enqueue_pktio_interface(pkt, pktio_loop);

	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	/* Error packet should be received in error queue */
	CU_ASSERT(queue == queue_list[CLS_ERROR]);
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_ERROR]);
	odp_packet_free(pkt);
}

void classification_test_pktio_set_skip(void)
{
	int retval;
	size_t offset = 5;
	retval = odp_pktio_skip_set(pktio_loop, offset);
	CU_ASSERT(retval == 0);

	retval = odp_pktio_skip_set(ODP_PKTIO_INVALID, offset);
	CU_ASSERT(retval < 0);

	/* reset skip value to zero as validation suite expects
	offset to be zero*/

	retval = odp_pktio_skip_set(pktio_loop, 0);
	CU_ASSERT(retval == 0);
}

void classification_test_pktio_set_headroom(void)
{
	size_t headroom;
	int retval;
	headroom = 5;
	retval = odp_pktio_headroom_set(pktio_loop, headroom);
	CU_ASSERT(retval == 0);

	retval = odp_pktio_headroom_set(ODP_PKTIO_INVALID, headroom);
	CU_ASSERT(retval < 0);
}

void configure_cos_with_l2_priority(void)
{
	uint8_t num_qos = CLS_L2_QOS_MAX;
	odp_cos_t cos_tbl[CLS_L2_QOS_MAX];
	odp_queue_t queue_tbl[CLS_L2_QOS_MAX];
	odp_pool_t pool;
	uint8_t qos_tbl[CLS_L2_QOS_MAX];
	char cosname[ODP_COS_NAME_LEN];
	char queuename[ODP_QUEUE_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];
	int retval;
	int i;
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;

	/** Initialize scalar variable qos_tbl **/
	for (i = 0; i < CLS_L2_QOS_MAX; i++)
		qos_tbl[i] = 0;

	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	for (i = 0; i < num_qos; i++) {
		qparam.sched.prio = ODP_SCHED_PRIO_LOWEST - i;
		sprintf(queuename, "%s_%d", "L2_Queue", i);
		queue_tbl[i] = odp_queue_create(queuename, &qparam);
		CU_ASSERT_FATAL(queue_tbl[i] != ODP_QUEUE_INVALID);
		queue_list[CLS_L2_QOS_0 + i] = queue_tbl[i];

		sprintf(poolname, "%s_%d", "L2_Pool", i);
		pool = pool_create(poolname);
		CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);
		pool_list[CLS_L2_QOS_0 + i] = pool;

		sprintf(cosname, "%s_%d", "L2_Cos", i);
		odp_cls_cos_param_init(&cls_param);
		cls_param.pool = pool;
		cls_param.queue = queue_tbl[i];
		cls_param.drop_policy = ODP_COS_DROP_POOL;
		cos_tbl[i] = odp_cls_cos_create(cosname, &cls_param);
		if (cos_tbl[i] == ODP_COS_INVALID)
			break;

		cos_list[CLS_L2_QOS_0 + i] = cos_tbl[i];
		qos_tbl[i] = i;
	}
	/* count 'i' is passed instead of num_qos to handle the rare scenario
	if the odp_cls_cos_create() failed in the middle*/
	retval = odp_cos_with_l2_priority(pktio_loop, i, qos_tbl, cos_tbl);
	CU_ASSERT(retval == 0);
}

void test_cos_with_l2_priority(void)
{
	odp_packet_t pkt;
	odph_ethhdr_t *ethhdr;
	odph_vlanhdr_t *vlan;
	odp_queue_t queue;
	odp_pool_t pool;
	uint32_t seqno = 0;

	uint8_t i;
	for (i = 0; i < CLS_L2_QOS_MAX; i++) {
		pkt = create_packet(pool_default, true, &seq, true);
		CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
		seqno = cls_pkt_get_seq(pkt);
		CU_ASSERT(seqno != TEST_SEQ_INVALID);
		ethhdr = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
		vlan = (odph_vlanhdr_t *)(&ethhdr->type);
		vlan->tci = odp_cpu_to_be_16(i << 13);
		enqueue_pktio_interface(pkt, pktio_loop);
		pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
		CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
		CU_ASSERT(queue == queue_list[CLS_L2_QOS_0 + i]);
		pool = odp_packet_pool(pkt);
		CU_ASSERT(pool == pool_list[CLS_L2_QOS_0 + i]);
		CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
		odp_packet_free(pkt);
	}
}

void configure_pmr_cos(void)
{
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pmr_match_t match;
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;
	char cosname[ODP_COS_NAME_LEN];
	char queuename[ODP_QUEUE_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];

	val = CLS_PMR_PORT;
	mask = 0xffff;
	match.term = find_first_supported_l3_pmr();
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr_list[CLS_PMR] = odp_pmr_create(&match);
	CU_ASSERT_FATAL(pmr_list[CLS_PMR] != ODP_PMR_INVAL);

	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_HIGHEST;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	sprintf(queuename, "%s", "PMR_CoS");

	queue_list[CLS_PMR] = odp_queue_create(queuename, &qparam);
	CU_ASSERT_FATAL(queue_list[CLS_PMR] != ODP_QUEUE_INVALID);

	sprintf(poolname, "PMR_Pool");
	pool_list[CLS_PMR] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_PMR] != ODP_POOL_INVALID);

	sprintf(cosname, "PMR_CoS");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_PMR];
	cls_param.queue = queue_list[CLS_PMR];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_PMR] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_PMR] != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr_list[CLS_PMR], pktio_loop,
				   cos_list[CLS_PMR]);
	CU_ASSERT(retval == 0);
}

void test_pmr_cos(void)
{
	odp_packet_t pkt;
	odp_queue_t queue;
	odp_pool_t pool;
	uint32_t seqno = 0;

	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	set_first_supported_pmr_port(pkt, CLS_PMR_PORT);
	enqueue_pktio_interface(pkt, pktio_loop);
	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(queue == queue_list[CLS_PMR]);
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_PMR]);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	odp_packet_free(pkt);
}

void configure_pktio_pmr_match_set_cos(void)
{
	int retval;
	odp_pmr_match_t pmr_terms[2];
	uint16_t val;
	uint16_t maskport;
	int num_terms = 2; /* one pmr for each L3 and L4 */
	odp_queue_param_t qparam;
	odp_cls_cos_param_t cls_param;
	char cosname[ODP_COS_NAME_LEN];
	char queuename[ODP_QUEUE_NAME_LEN];
	char poolname[ODP_POOL_NAME_LEN];
	uint32_t addr = 0;
	uint32_t mask;

	parse_ipv4_string(CLS_PMR_SET_SADDR, &addr, &mask);
	pmr_terms[0].term = ODP_PMR_SIP_ADDR;
	pmr_terms[0].val = &addr;
	pmr_terms[0].mask = &mask;
	pmr_terms[0].val_sz = sizeof(addr);


	val = CLS_PMR_SET_PORT;
	maskport = 0xffff;
	pmr_terms[1].term = find_first_supported_l3_pmr();
	pmr_terms[1].val = &val;
	pmr_terms[1].mask = &maskport;
	pmr_terms[1].val_sz = sizeof(val);

	retval = odp_pmr_match_set_create(num_terms, pmr_terms, &pmr_set);
	CU_ASSERT(retval > 0);

	odp_queue_param_init(&qparam);
	qparam.type       = ODP_QUEUE_TYPE_SCHED;
	qparam.sched.prio = ODP_SCHED_PRIO_HIGHEST;
	qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;
	sprintf(queuename, "%s", "cos_pmr_set_queue");

	queue_list[CLS_PMR_SET] = odp_queue_create(queuename, &qparam);
	CU_ASSERT_FATAL(queue_list[CLS_PMR_SET] != ODP_QUEUE_INVALID);

	sprintf(poolname, "cos_pmr_set_pool");
	pool_list[CLS_PMR_SET] = pool_create(poolname);
	CU_ASSERT_FATAL(pool_list[CLS_PMR_SET] != ODP_POOL_INVALID);

	sprintf(cosname, "cos_pmr_set");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool_list[CLS_PMR_SET];
	cls_param.queue = queue_list[CLS_PMR_SET];
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_list[CLS_PMR_SET] = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_list[CLS_PMR_SET] != ODP_COS_INVALID);

	retval = odp_pktio_pmr_match_set_cos(pmr_set, pktio_loop,
					     cos_list[CLS_PMR_SET]);
	CU_ASSERT(retval == 0);
}

void test_pktio_pmr_match_set_cos(void)
{
	uint32_t addr = 0;
	uint32_t mask;
	odph_ipv4hdr_t *ip;
	odp_packet_t pkt;
	odp_pool_t pool;
	odp_queue_t queue;
	uint32_t seqno = 0;

	pkt = create_packet(pool_default, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	parse_ipv4_string(CLS_PMR_SET_SADDR, &addr, &mask);
	ip->src_addr = odp_cpu_to_be_32(addr);
	ip->chksum = 0;
	ip->chksum = odph_ipv4_csum_update(pkt);

	set_first_supported_pmr_port(pkt, CLS_PMR_SET_PORT);
	enqueue_pktio_interface(pkt, pktio_loop);
	pkt = receive_packet(&queue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(queue == queue_list[CLS_PMR_SET]);
	pool = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_list[CLS_PMR_SET]);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	odp_packet_free(pkt);
}

void classification_test_pmr_terms_avail(void)
{
	int retval;
	/* Since this API called at the start of the suite the return value
	should be greater than 0 */
	retval = odp_pmr_terms_avail();
	CU_ASSERT(retval > 0);
}

void classification_test_pmr_terms_cap(void)
{
	unsigned long long retval;
	/* Need to check different values for different platforms */
	retval = odp_pmr_terms_cap();
	CU_ASSERT(retval & (1 << ODP_PMR_IPPROTO));
}

void classification_test_pktio_configure(void)
{
	/* Configure the Different CoS for the pktio interface */
	if (TEST_DEFAULT)
		configure_pktio_default_cos();
	if (TEST_ERROR)
		configure_pktio_error_cos();
	if (TEST_PMR_CHAIN)
		configure_cls_pmr_chain();
	if (TEST_L2_QOS)
		configure_cos_with_l2_priority();
	if (TEST_PMR)
		configure_pmr_cos();
	if (TEST_PMR_SET)
		configure_pktio_pmr_match_set_cos();
}

void classification_test_pktio_test(void)
{
	/* Test Different CoS on the pktio interface */
	if (TEST_DEFAULT)
		test_pktio_default_cos();
	if (TEST_ERROR)
		test_pktio_error_cos();
	if (TEST_PMR_CHAIN)
		test_cls_pmr_chain();
	if (TEST_L2_QOS)
		test_cos_with_l2_priority();
	if (TEST_PMR)
		test_pmr_cos();
	if (TEST_PMR_SET)
		test_pktio_pmr_match_set_cos();
}

odp_testinfo_t classification_suite[] = {
	ODP_TEST_INFO(classification_test_pmr_terms_avail),
	ODP_TEST_INFO(classification_test_pktio_set_skip),
	ODP_TEST_INFO(classification_test_pktio_set_headroom),
	ODP_TEST_INFO(classification_test_pmr_terms_cap),
	ODP_TEST_INFO(classification_test_pktio_configure),
	ODP_TEST_INFO(classification_test_pktio_test),
	ODP_TEST_INFO_NULL,
};
