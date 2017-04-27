/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Classification Inlines
 * Classification Inlines Functions
 */
#ifndef __ODP_CLASSIFICATION_INLINES_H_
#define __ODP_CLASSIFICATION_INLINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/debug.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/ipsec.h>
#include <odp/helper/udp.h>
#include <odp/helper/tcp.h>
#include <odp_packet_internal.h>

/* PMR term value verification function
These functions verify the given PMR term value with the value in the packet
These following functions return 1 on success and 0 on failure
*/

static inline int verify_pmr_packet_len(odp_packet_hdr_t *pkt_hdr,
					pmr_term_value_t *term_value)
{
	if (term_value->val == (packet_len(pkt_hdr) &
				     term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_ip_proto(const uint8_t *pkt_addr,
				      odp_packet_hdr_t *pkt_hdr,
				      pmr_term_value_t *term_value)
{
	const odph_ipv4hdr_t *ip;
	uint8_t proto;

	if (!pkt_hdr->input_flags.ipv4)
		return 0;
	ip = (const odph_ipv4hdr_t *)(pkt_addr + pkt_hdr->l3_offset);
	proto = ip->proto;
	if (term_value->val == (proto & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_ipv4_saddr(const uint8_t *pkt_addr,
					odp_packet_hdr_t *pkt_hdr,
					pmr_term_value_t *term_value)
{
	const odph_ipv4hdr_t *ip;
	uint32_t ipaddr;

	if (!pkt_hdr->input_flags.ipv4)
		return 0;
	ip = (const odph_ipv4hdr_t *)(pkt_addr + pkt_hdr->l3_offset);
	ipaddr = odp_be_to_cpu_32(ip->src_addr);
	if (term_value->val == (ipaddr & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_ipv4_daddr(const uint8_t *pkt_addr,
					odp_packet_hdr_t *pkt_hdr,
					pmr_term_value_t *term_value)
{
	const odph_ipv4hdr_t *ip;
	uint32_t ipaddr;

	if (!pkt_hdr->input_flags.ipv4)
		return 0;
	ip = (const odph_ipv4hdr_t *)(pkt_addr + pkt_hdr->l3_offset);
	ipaddr = odp_be_to_cpu_32(ip->dst_addr);
	if (term_value->val == (ipaddr & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_tcp_sport(const uint8_t *pkt_addr,
				       odp_packet_hdr_t *pkt_hdr,
				       pmr_term_value_t *term_value)
{
	uint16_t sport;
	const odph_tcphdr_t *tcp;

	if (!pkt_hdr->input_flags.tcp)
		return 0;
	tcp = (const odph_tcphdr_t *)(pkt_addr + pkt_hdr->l4_offset);
	sport = odp_be_to_cpu_16(tcp->src_port);
	if (term_value->val == (sport & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_tcp_dport(const uint8_t *pkt_addr,
				       odp_packet_hdr_t *pkt_hdr,
				       pmr_term_value_t *term_value)
{
	uint16_t dport;
	const odph_tcphdr_t *tcp;

	if (!pkt_hdr->input_flags.tcp)
		return 0;
	tcp = (const odph_tcphdr_t *)(pkt_addr + pkt_hdr->l4_offset);
	dport = odp_be_to_cpu_16(tcp->dst_port);
	if (term_value->val == (dport & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_udp_dport(const uint8_t *pkt_addr,
				       odp_packet_hdr_t *pkt_hdr,
				       pmr_term_value_t *term_value)
{
	uint16_t dport;
	const odph_udphdr_t *udp;

	if (!pkt_hdr->input_flags.udp)
		return 0;
	udp = (const odph_udphdr_t *)(pkt_addr + pkt_hdr->l4_offset);
	dport = odp_be_to_cpu_16(udp->dst_port);
	if (term_value->val == (dport & term_value->mask))
			return 1;

	return 0;
}

static inline int verify_pmr_udp_sport(const uint8_t *pkt_addr,
				       odp_packet_hdr_t *pkt_hdr,
				       pmr_term_value_t *term_value)
{
	uint16_t sport;
	const odph_udphdr_t *udp;

	if (!pkt_hdr->input_flags.udp)
		return 0;
	udp = (const odph_udphdr_t *)(pkt_addr + pkt_hdr->l4_offset);
	sport = odp_be_to_cpu_16(udp->src_port);
	if (term_value->val == (sport & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_dmac(const uint8_t *pkt_addr,
				  odp_packet_hdr_t *pkt_hdr,
				  pmr_term_value_t *term_value)
{
	uint64_t dmac = 0;
	uint64_t dmac_be = 0;
	const odph_ethhdr_t *eth;

	if (!pkt_hdr->input_flags.eth)
		return 0;

	eth = (const odph_ethhdr_t *)(pkt_addr + pkt_hdr->l2_offset);
	memcpy(&dmac_be, eth->dst.addr, ODPH_ETHADDR_LEN);
	dmac = odp_be_to_cpu_64(dmac_be);
	/* since we are converting a 48 bit ethernet address from BE to cpu
	format using odp_be_to_cpu_64() the last 16 bits needs to be right
	shifted */
	if (dmac_be != dmac)
		dmac = dmac >> (64 - (ODPH_ETHADDR_LEN * 8));

	if (term_value->val == (dmac & term_value->mask))
		return 1;
	return 0;
}

static inline int verify_pmr_ipv6_saddr(const uint8_t *pkt_addr ODP_UNUSED,
					odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
					pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_ipv6_daddr(const uint8_t *pkt_addr ODP_UNUSED,
					odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
					pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_vlan_id_0(const uint8_t *pkt_addr ODP_UNUSED,
				       odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
				       pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_vlan_id_x(const uint8_t *pkt_addr ODP_UNUSED,
				       odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
				       pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_ipsec_spi(const uint8_t *pkt_addr,
				       odp_packet_hdr_t *pkt_hdr,
				       pmr_term_value_t *term_value)
{
	uint32_t spi;

	if (!pkt_hdr->input_flags.ipsec)
		return 0;

	pkt_addr += pkt_hdr->l4_offset;

	if (pkt_hdr->l4_protocol == ODPH_IPPROTO_AH) {
		const odph_ahhdr_t *ahhdr = (const odph_ahhdr_t *)pkt_addr;

		spi = odp_be_to_cpu_32(ahhdr->spi);
	} else if (pkt_hdr->l4_protocol == ODPH_IPPROTO_ESP) {
		const odph_esphdr_t *esphdr = (const odph_esphdr_t *)pkt_addr;

		spi = odp_be_to_cpu_32(esphdr->spi);
	} else {
		return 0;
	}

	if (term_value->val == (spi & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_ld_vni(const uint8_t *pkt_addr ODP_UNUSED,
				    odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
				    pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_custom_frame(const uint8_t *pkt_addr,
					  odp_packet_hdr_t *pkt_hdr,
					  pmr_term_value_t *term_value)
{
	uint64_t val = 0;
	uint32_t offset = term_value->offset;
	uint32_t val_sz = term_value->val_sz;

	ODP_ASSERT(val_sz <= ODP_PMR_TERM_BYTES_MAX);

	if (packet_len(pkt_hdr) <= offset + val_sz)
		return 0;

	memcpy(&val, pkt_addr + offset, val_sz);
	if (term_value->val == (val & term_value->mask))
		return 1;

	return 0;
}

static inline int verify_pmr_eth_type_0(const uint8_t *pkt_addr ODP_UNUSED,
					odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
					pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}

static inline int verify_pmr_eth_type_x(const uint8_t *pkt_addr ODP_UNUSED,
					odp_packet_hdr_t *pkt_hdr ODP_UNUSED,
					pmr_term_value_t *term_value ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	return 0;
}
#ifdef __cplusplus
}
#endif
#endif
