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

typedef struct cls_test_packet {
	odp_u32be_t magic;
	odp_u32be_t seq;
} cls_test_packet_t;

int destroy_inq(odp_pktio_t pktio)
{
	odp_queue_t inq;
	odp_event_t ev;

	inq = odp_pktio_inq_getdef(pktio);

	if (inq == ODP_QUEUE_INVALID) {
		CU_FAIL("attempting to destroy invalid inq");
		return -1;
	}

	if (0 > odp_pktio_stop(pktio))
		return -1;

	if (0 > odp_pktio_inq_remdef(pktio))
		return -1;

	while (1) {
		ev = odp_schedule(NULL, ODP_SCHED_NO_WAIT);

		if (ev != ODP_EVENT_INVALID)
			odp_event_free(ev);
		else
			break;
	}

	return odp_queue_destroy(inq);
}

int cls_pkt_set_seq(odp_packet_t pkt)
{
	static uint32_t seq;
	cls_test_packet_t data;
	uint32_t offset;
	odph_ipv4hdr_t *ip;
	odph_tcphdr_t *tcp;
	int status;

	data.magic = DATA_MAGIC;
	data.seq = ++seq;

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	offset = odp_packet_l4_offset(pkt);
	CU_ASSERT_FATAL(offset != 0);

	if (ip->proto == ODPH_IPPROTO_UDP)
		status = odp_packet_copydata_in(pkt, offset + ODPH_UDPHDR_LEN,
						sizeof(data), &data);
	else {
		tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
		status = odp_packet_copydata_in(pkt, offset + tcp->hl * 4,
						sizeof(data), &data);
	}

	return status;
}

uint32_t cls_pkt_get_seq(odp_packet_t pkt)
{
	uint32_t offset;
	cls_test_packet_t data;
	odph_ipv4hdr_t *ip;
	odph_tcphdr_t *tcp;

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	offset = odp_packet_l4_offset(pkt);

	if (!offset && !ip)
		return TEST_SEQ_INVALID;

	if (ip->proto == ODPH_IPPROTO_UDP)
		odp_packet_copydata_out(pkt, offset + ODPH_UDPHDR_LEN,
					sizeof(data), &data);
	else {
		tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
		odp_packet_copydata_out(pkt, offset + tcp->hl * 4,
					sizeof(data), &data);
	}

	if (data.magic == DATA_MAGIC)
		return data.seq;

	return TEST_SEQ_INVALID;
}

int parse_ipv4_string(const char *ipaddress, uint32_t *addr, uint32_t *mask)
{
	int b[4];
	int qualifier = 32;
	int converted;

	if (strchr(ipaddress, '/')) {
		converted = sscanf(ipaddress, "%d.%d.%d.%d/%d",
				   &b[3], &b[2], &b[1], &b[0],
				   &qualifier);
		if (5 != converted)
			return -1;
	} else {
		converted = sscanf(ipaddress, "%d.%d.%d.%d",
				   &b[3], &b[2], &b[1], &b[0]);
		if (4 != converted)
			return -1;
	}

	if ((b[0] > 255) || (b[1] > 255) || (b[2] > 255) || (b[3] > 255))
		return -1;
	if (!qualifier || (qualifier > 32))
		return -1;

	*addr = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;
	if (mask)
		*mask = ~(0xFFFFFFFF & ((1ULL << (32 - qualifier)) - 1));

	return 0;
}

void enqueue_pktio_interface(odp_packet_t pkt, odp_pktio_t pktio)
{
	odp_event_t ev;
	odp_queue_t defqueue;

	defqueue  = odp_pktio_outq_getdef(pktio);
	CU_ASSERT(defqueue != ODP_QUEUE_INVALID);

	ev = odp_packet_to_event(pkt);
	CU_ASSERT(odp_queue_enq(defqueue, ev) == 0);
}

odp_packet_t receive_packet(odp_queue_t *queue, uint64_t ns)
{
	odp_event_t ev;

	ev = odp_schedule(queue, ns);
	return odp_packet_from_event(ev);
}

odp_queue_t queue_create(const char *queuename, bool sched)
{
	odp_queue_t queue;
	odp_queue_param_t qparam;

	if (sched) {
		odp_queue_param_init(&qparam);
		qparam.type       = ODP_QUEUE_TYPE_SCHED;
		qparam.sched.prio = ODP_SCHED_PRIO_HIGHEST;
		qparam.sched.sync = ODP_SCHED_SYNC_PARALLEL;
		qparam.sched.group = ODP_SCHED_GROUP_ALL;

		queue = odp_queue_create(queuename, &qparam);
	} else {
		queue = odp_queue_create(queuename, NULL);
	}

	return queue;
}

odp_pool_t pool_create(const char *poolname)
{
	odp_pool_param_t param;

	odp_pool_param_init(&param);
	param.pkt.seg_len = SHM_PKT_BUF_SIZE;
	param.pkt.len     = SHM_PKT_BUF_SIZE;
	param.pkt.num     = SHM_PKT_NUM_BUFS;
	param.type        = ODP_POOL_PACKET;

	return odp_pool_create(poolname, &param);
}

odp_packet_t create_packet(odp_pool_t pool, bool vlan,
			   odp_atomic_u32_t *seq, bool flag_udp)
{
	return create_packet_len(pool, vlan, seq, flag_udp, 0);
}

odp_packet_t create_packet_len(odp_pool_t pool, bool vlan,
			       odp_atomic_u32_t *seq, bool flag_udp,
			       uint16_t len)
{
	uint32_t seqno;
	odph_ethhdr_t *ethhdr;
	odph_udphdr_t *udp;
	odph_tcphdr_t *tcp;
	odph_ipv4hdr_t *ip;
	uint16_t payload_len;
	uint64_t src_mac = CLS_DEFAULT_SMAC;
	uint64_t dst_mac = CLS_DEFAULT_DMAC;
	uint64_t dst_mac_be;
	uint32_t addr = 0;
	uint32_t mask;
	int offset;
	odp_packet_t pkt;
	int packet_len = 0;

	/* 48 bit ethernet address needs to be left shifted for proper
	value after changing to be*/
	dst_mac_be = odp_cpu_to_be_64(dst_mac);
	if (dst_mac != dst_mac_be)
		dst_mac_be = dst_mac_be >> (64 - 8 * ODPH_ETHADDR_LEN);

	payload_len = sizeof(cls_test_packet_t) + len;
	packet_len += ODPH_ETHHDR_LEN;
	packet_len += ODPH_IPV4HDR_LEN;
	if (flag_udp)
		packet_len += ODPH_UDPHDR_LEN;
	else
		packet_len += ODPH_TCPHDR_LEN;
	packet_len += payload_len;

	if (vlan)
		packet_len += ODPH_VLANHDR_LEN;

	pkt = odp_packet_alloc(pool, packet_len);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);

	/* Ethernet Header */
	offset = 0;
	odp_packet_l2_offset_set(pkt, offset);
	ethhdr = (odph_ethhdr_t *)odp_packet_l2_ptr(pkt, NULL);
	memcpy(ethhdr->src.addr, &src_mac, ODPH_ETHADDR_LEN);
	memcpy(ethhdr->dst.addr, &dst_mac_be, ODPH_ETHADDR_LEN);
	offset += sizeof(odph_ethhdr_t);
	if (vlan) {
		/* Default vlan header */
		uint8_t *parseptr;
		odph_vlanhdr_t *vlan;

		vlan = (odph_vlanhdr_t *)(&ethhdr->type);
		parseptr = (uint8_t *)vlan;
		vlan->tci = odp_cpu_to_be_16(0);
		vlan->tpid = odp_cpu_to_be_16(ODPH_ETHTYPE_VLAN);
		offset += sizeof(odph_vlanhdr_t);
		parseptr += sizeof(odph_vlanhdr_t);
		odp_u16be_t *type = (odp_u16be_t *)(void *)parseptr;
		*type = odp_cpu_to_be_16(ODPH_ETHTYPE_IPV4);
	} else {
		ethhdr->type =	odp_cpu_to_be_16(ODPH_ETHTYPE_IPV4);
	}

	odp_packet_l3_offset_set(pkt, offset);

	/* ipv4 */
	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);

	parse_ipv4_string(CLS_DEFAULT_DADDR, &addr, &mask);
	ip->dst_addr = odp_cpu_to_be_32(addr);

	parse_ipv4_string(CLS_DEFAULT_SADDR, &addr, &mask);
	ip->src_addr = odp_cpu_to_be_32(addr);
	ip->ver_ihl = ODPH_IPV4 << 4 | ODPH_IPV4HDR_IHL_MIN;
	if (flag_udp)
		ip->tot_len = odp_cpu_to_be_16(ODPH_UDPHDR_LEN + payload_len +
					       ODPH_IPV4HDR_LEN);
	else
		ip->tot_len = odp_cpu_to_be_16(ODPH_TCPHDR_LEN + payload_len +
					       ODPH_IPV4HDR_LEN);

	ip->ttl = 128;
	if (flag_udp)
		ip->proto = ODPH_IPPROTO_UDP;
	else
		ip->proto = ODPH_IPPROTO_TCP;

	seqno = odp_atomic_fetch_inc_u32(seq);
	ip->id = odp_cpu_to_be_16(seqno);
	ip->chksum = 0;
	ip->chksum = odph_ipv4_csum_update(pkt);
	offset += ODPH_IPV4HDR_LEN;

	/* udp */
	if (flag_udp) {
		odp_packet_l4_offset_set(pkt, offset);
		udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
		udp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT);
		udp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT);
		udp->length = odp_cpu_to_be_16(payload_len + ODPH_UDPHDR_LEN);
		udp->chksum = 0;
	} else {
		odp_packet_l4_offset_set(pkt, offset);
		tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
		tcp->src_port = odp_cpu_to_be_16(CLS_DEFAULT_SPORT);
		tcp->dst_port = odp_cpu_to_be_16(CLS_DEFAULT_DPORT);
		tcp->hl = ODPH_TCPHDR_LEN / 4;
		/* TODO: checksum field has to be updated */
		tcp->cksm = 0;
	}

	/* set pkt sequence number */
	cls_pkt_set_seq(pkt);

	return pkt;
}

odp_pmr_term_t find_first_supported_l3_pmr(void)
{
	unsigned long long cap;
	odp_pmr_term_t term = ODP_PMR_TCP_DPORT;

	/* choose supported PMR */
	cap = odp_pmr_terms_cap();
	if (cap & (1 << ODP_PMR_UDP_SPORT))
		term = ODP_PMR_UDP_SPORT;
	else if (cap & (1 << ODP_PMR_UDP_DPORT))
		term = ODP_PMR_UDP_DPORT;
	else if (cap & (1 << ODP_PMR_TCP_SPORT))
		term = ODP_PMR_TCP_SPORT;
	else if (cap & (1 << ODP_PMR_TCP_DPORT))
		term = ODP_PMR_TCP_DPORT;
	else
		CU_FAIL("Implementations doesn't support any TCP/UDP PMR");

	return term;
}

int set_first_supported_pmr_port(odp_packet_t pkt, uint16_t port)
{
	odph_udphdr_t *udp;
	odph_tcphdr_t *tcp;
	odp_pmr_term_t term;

	udp = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	tcp = (odph_tcphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	port = odp_cpu_to_be_16(port);
	term = find_first_supported_l3_pmr();
	switch (term) {
	case ODP_PMR_UDP_SPORT:
		udp->src_port = port;
		break;
	case ODP_PMR_UDP_DPORT:
		udp->dst_port = port;
		break;
	case ODP_PMR_TCP_DPORT:
		tcp->dst_port = port;
		break;
	case ODP_PMR_TCP_SPORT:
		tcp->src_port = port;
		break;
	default:
		CU_FAIL("Unsupported L3 term");
		return -1;
	}

	return 0;
}
