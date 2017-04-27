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

static odp_pool_t pkt_pool;

/** sequence number of IP packets */
odp_atomic_u32_t seq;

int classification_suite_pmr_init(void)
{
	pkt_pool = pool_create("classification_pmr_pool");
	if (ODP_POOL_INVALID == pkt_pool) {
		fprintf(stderr, "Packet pool creation failed.\n");
		return -1;
	}

	odp_atomic_init_u32(&seq, 0);
	return 0;
}

odp_pktio_t create_pktio(odp_queue_type_t q_type)
{
	odp_pktio_t pktio;
	odp_pktio_param_t pktio_param;
	int ret;

	if (pkt_pool == ODP_POOL_INVALID)
		return ODP_PKTIO_INVALID;

	odp_pktio_param_init(&pktio_param);

	if (q_type == ODP_QUEUE_TYPE_PLAIN)
		pktio_param.in_mode = ODP_PKTIN_MODE_QUEUE;
	else
		pktio_param.in_mode = ODP_PKTIN_MODE_SCHED;

	pktio = odp_pktio_open("loop", pkt_pool, &pktio_param);
	if (pktio == ODP_PKTIO_INVALID) {
		ret = odp_pool_destroy(pkt_pool);
		if (ret)
			fprintf(stderr, "unable to destroy pool.\n");
		return ODP_PKTIO_INVALID;
	}

	return pktio;
}

int create_default_inq(odp_pktio_t pktio, odp_queue_type_t qtype ODP_UNUSED)
{
	odp_queue_param_t qparam;
	odp_queue_t inq_def;
	char inq_name[ODP_QUEUE_NAME_LEN];

	odp_queue_param_init(&qparam);
	qparam.type        = ODP_QUEUE_TYPE_PKTIN;
	qparam.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	qparam.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
	qparam.sched.group = ODP_SCHED_GROUP_ALL;

	snprintf(inq_name, sizeof(inq_name), "inq-pktio-%" PRIu64,
		 odp_pktio_to_u64(pktio));
	inq_def = odp_queue_lookup(inq_name);
	if (inq_def == ODP_QUEUE_INVALID)
		inq_def = odp_queue_create(inq_name, &qparam);

	CU_ASSERT_FATAL(inq_def != ODP_QUEUE_INVALID);

	if (0 > odp_pktio_inq_setdef(pktio, inq_def))
		return -1;

	if (odp_pktio_start(pktio)) {
		fprintf(stderr, "unable to start loop\n");
		return -1;
	}

	return 0;
}

void configure_default_cos(odp_pktio_t pktio, odp_cos_t *cos,
			   odp_queue_t *queue, odp_pool_t *pool)
{
	odp_cls_cos_param_t cls_param;
	odp_pool_t default_pool;
	odp_cos_t default_cos;
	odp_queue_t default_queue;
	int retval;
	char cosname[ODP_COS_NAME_LEN];

	default_pool  = pool_create("DefaultPool");
	CU_ASSERT(default_pool != ODP_POOL_INVALID);

	default_queue = queue_create("DefaultQueue", true);
	CU_ASSERT(default_queue != ODP_QUEUE_INVALID);

	sprintf(cosname, "DefaultCos");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = default_pool;
	cls_param.queue = default_queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	default_cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT(default_cos != ODP_COS_INVALID);

	retval = odp_pktio_default_cos_set(pktio, default_cos);
	CU_ASSERT(retval == 0);

	*cos = default_cos;
	*queue = default_queue;
	*pool = default_pool;
	return;
}

int classification_suite_pmr_term(void)
{
	int retcode = 0;

	if (0 != odp_pool_destroy(pkt_pool)) {
		fprintf(stderr, "pkt_pool destroy failed.\n");
		retcode = -1;
	}

	return retcode;
}

void classification_test_pmr_term_tcp_dport(void)
{
	odp_packet_t pkt;
	odph_tcphdr_t *tcp;
	uint32_t seqno;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pool_t pool;
	odp_pool_t pool_recv;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = CLS_DEFAULT_DPORT;
	mask = 0xffff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_TCP_DPORT;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);


	queue = queue_create("tcp_dport1", true);
	CU_ASSERT(queue != ODP_QUEUE_INVALID);

	pool = pool_create("tcp_dport1");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "tcp_dport");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	tcp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	pool_recv = odp_packet_pool(pkt);
	CU_ASSERT(pool == pool_recv);
	CU_ASSERT(retqueue == queue);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));

	odp_packet_free(pkt);

	/* Other packets are delivered to default queue */
	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	tcp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT + 1);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == default_queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);

	odp_packet_free(pkt);
	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pool_destroy(pool);
	odp_pool_destroy(default_pool);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_tcp_sport(void)
{
	odp_packet_t pkt;
	odph_tcphdr_t *tcp;
	uint32_t seqno;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = CLS_DEFAULT_SPORT;
	mask = 0xffff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_TCP_SPORT;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("tcp_sport", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("tcp_sport");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "tcp_sport");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	tcp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	odp_packet_free(pkt);

	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	tcp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT + 1);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == default_queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);

	odp_packet_free(pkt);
	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_udp_dport(void)
{
	odp_packet_t pkt;
	odph_udphdr_t *udp;
	uint32_t seqno;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_pmr_match_t match;
	odp_cls_cos_param_t cls_param;
	odph_ethhdr_t *eth;

	val = CLS_DEFAULT_DPORT;
	mask = 0xffff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_UDP_DPORT;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("udp_dport", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("udp_dport");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "udp_dport");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	udp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	odp_packet_free(pkt);

	/* Other packets received in default queue */
	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	udp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT + 1);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == default_queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);

	odp_packet_free(pkt);
	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_udp_sport(void)
{
	odp_packet_t pkt;
	odph_udphdr_t *udp;
	uint32_t seqno;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_pmr_match_t match;
	odp_cls_cos_param_t cls_param;
	odph_ethhdr_t *eth;

	val = CLS_DEFAULT_SPORT;
	mask = 0xffff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_UDP_SPORT;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("udp_sport", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("udp_sport");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "udp_sport");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	udp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	odp_packet_free(pkt);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	udp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT + 1);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == default_queue);
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);
	odp_packet_free(pkt);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_ipproto(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	uint8_t val;
	uint8_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = ODPH_IPPROTO_UDP;
	mask = 0xff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_IPPROTO;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("ipproto", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("ipproto");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "ipproto");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	CU_ASSERT(retqueue == queue);
	odp_packet_free(pkt);

	/* Other packets delivered to default queue */
	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);
	CU_ASSERT(retqueue == default_queue);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	odp_packet_free(pkt);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_dmac(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	uint64_t val;
	uint64_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = CLS_DEFAULT_DMAC; /* 48 bit Ethernet Mac address */
	mask = 0xffffffffffff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_DMAC;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = ODPH_ETHADDR_LEN;

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("dmac", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("dmac");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "dmac");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	CU_ASSERT(retqueue == queue);
	odp_packet_free(pkt);

	/* Other packets delivered to default queue */
	pkt = create_packet(pkt_pool, false, &seq, false);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	memset(eth->dst.addr, 0, ODPH_ETHADDR_LEN);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);
	CU_ASSERT(retqueue == default_queue);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	odp_packet_free(pkt);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

void classification_test_pmr_term_packet_len(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = 1024;
	/*Mask value will match any packet of length 1000 - 1099*/
	mask = 0xff00;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_LEN;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("packet_len", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("packet_len");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "packet_len");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	/* create packet of payload length 1024 */
	pkt = create_packet_len(pkt_pool, false, &seq, true, 1024);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	CU_ASSERT(retqueue == queue);
	odp_packet_free(pkt);

	/* Other packets delivered to default queue */
	pkt = create_packet(pkt_pool, false, &seq, false);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == default_pool);
	CU_ASSERT(retqueue == default_queue);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	odp_packet_free(pkt);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

static void classification_test_pmr_pool_set(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	uint8_t val;
	uint8_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_pool_t pool_new;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = ODPH_IPPROTO_UDP;
	mask = 0xff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_IPPROTO;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("ipproto1", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("ipproto1");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "ipproto1");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	pool_new = pool_create("ipproto2");
	CU_ASSERT_FATAL(pool_new != ODP_POOL_INVALID);

	/* new pool is set on CoS */
	retval = odp_cls_cos_pool_set(cos, pool_new);
	CU_ASSERT(retval == 0);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool_new);
	CU_ASSERT(retqueue == queue);
	odp_packet_free(pkt);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_pool_destroy(pool_new);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

static void classification_test_pmr_queue_set(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	uint8_t val;
	uint8_t mask;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_cos_t default_cos;
	odp_pool_t default_pool;
	odp_pool_t pool;
	odp_queue_t queue_new;
	odp_pool_t recvpool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pmr_match_t match;
	odph_ethhdr_t *eth;

	val = ODPH_IPPROTO_UDP;
	mask = 0xff;
	seqno = 0;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT_FATAL(pktio != ODP_PKTIO_INVALID);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	match.term = ODP_PMR_IPPROTO;
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);

	queue = queue_create("ipproto1", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("ipproto1");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "ipproto1");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	queue_new = queue_create("ipproto2", true);
	CU_ASSERT_FATAL(queue_new != ODP_QUEUE_INVALID);

	/* new queue is set on CoS */
	retval = odp_cos_queue_set(cos, queue_new);
	CU_ASSERT(retval == 0);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	pkt = create_packet(pkt_pool, false, &seq, true);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	recvpool = odp_packet_pool(pkt);
	CU_ASSERT(recvpool == pool);
	CU_ASSERT(retqueue == queue_new);
	odp_packet_free(pkt);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue_new);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

static void classification_test_pmr_term_daddr(void)
{
	odp_packet_t pkt;
	uint32_t seqno;
	int retval;
	odp_pktio_t pktio;
	odp_queue_t queue;
	odp_queue_t retqueue;
	odp_queue_t default_queue;
	odp_pool_t pool;
	odp_pool_t default_pool;
	odp_pmr_t pmr;
	odp_cos_t cos;
	odp_cos_t default_cos;
	uint32_t addr;
	uint32_t mask;
	char cosname[ODP_QUEUE_NAME_LEN];
	odp_pmr_match_t match;
	odp_cls_cos_param_t cls_param;
	odph_ipv4hdr_t *ip;
	const char *dst_addr = "10.0.0.99/32";
	odph_ethhdr_t *eth;

	pktio = create_pktio(ODP_QUEUE_TYPE_SCHED);
	retval = create_default_inq(pktio, ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(retval == 0);

	configure_default_cos(pktio, &default_cos,
			      &default_queue, &default_pool);

	parse_ipv4_string(dst_addr, &addr, &mask);
	match.term = ODP_PMR_DIP_ADDR;
	match.val = &addr;
	match.mask = &mask;
	match.val_sz = sizeof(addr);

	pmr = odp_pmr_create(&match);
	CU_ASSERT_FATAL(pmr != ODP_PMR_INVAL);

	queue = queue_create("daddr", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	pool = pool_create("daddr");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	sprintf(cosname, "daddr");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	retval = odp_pktio_pmr_cos(pmr, pktio, cos);
	CU_ASSERT(retval == 0);

	/* packet with dst ip address matching PMR rule to be
	received in the CoS queue*/
	pkt = create_packet(pkt_pool, false, &seq, false);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);
	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	ip->dst_addr = odp_cpu_to_be_32(addr);
	ip->chksum = odph_ipv4_csum_update(pkt);

	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == queue);
	odp_packet_free(pkt);

	/* Other packets delivered to default queue */
	pkt = create_packet(pkt_pool, false, &seq, false);
	seqno = cls_pkt_get_seq(pkt);
	CU_ASSERT(seqno != TEST_SEQ_INVALID);
	eth = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	odp_pktio_mac_addr(pktio, eth->src.addr, ODPH_ETHADDR_LEN);
	odp_pktio_mac_addr(pktio, eth->dst.addr, ODPH_ETHADDR_LEN);

	enqueue_pktio_interface(pkt, pktio);

	pkt = receive_packet(&retqueue, ODP_TIME_SEC_IN_NS);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(seqno == cls_pkt_get_seq(pkt));
	CU_ASSERT(retqueue == default_queue);

	odp_cos_destroy(cos);
	odp_cos_destroy(default_cos);
	odp_pmr_destroy(pmr);
	odp_packet_free(pkt);
	destroy_inq(pktio);
	odp_pool_destroy(default_pool);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
	odp_queue_destroy(default_queue);
	odp_pktio_close(pktio);
}

odp_testinfo_t classification_suite_pmr[] = {
	ODP_TEST_INFO(classification_test_pmr_term_tcp_dport),
	ODP_TEST_INFO(classification_test_pmr_term_tcp_sport),
	ODP_TEST_INFO(classification_test_pmr_term_udp_dport),
	ODP_TEST_INFO(classification_test_pmr_term_udp_sport),
	ODP_TEST_INFO(classification_test_pmr_term_ipproto),
	ODP_TEST_INFO(classification_test_pmr_term_dmac),
	ODP_TEST_INFO(classification_test_pmr_pool_set),
	ODP_TEST_INFO(classification_test_pmr_queue_set),
	ODP_TEST_INFO(classification_test_pmr_term_daddr),
	ODP_TEST_INFO(classification_test_pmr_term_packet_len),
	ODP_TEST_INFO_NULL,
};
