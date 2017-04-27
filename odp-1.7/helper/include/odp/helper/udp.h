/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP UDP header
 */

#ifndef ODPH_UDP_H_
#define ODPH_UDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/align.h>
#include <odp/debug.h>
#include <odp/byteorder.h>

/** @addtogroup odph_header ODPH HEADER
 *  @{
 */

/** UDP header length */
#define ODPH_UDPHDR_LEN 8

/** UDP header */
typedef struct ODP_PACKED {
	odp_u16be_t src_port; /**< Source port */
	odp_u16be_t dst_port; /**< Destination port */
	odp_u16be_t length;   /**< UDP datagram length in bytes (header+data) */
	odp_u16be_t chksum;   /**< UDP header and data checksum (0 if not used)*/
} odph_udphdr_t;

/**
 * UDP checksum
 *
 * This function uses odp packet to calc checksum
 *
 * @param pkt  calculate chksum for pkt
 * @return  checksum value in BE endianness
 */
static inline uint16_t odph_ipv4_udp_chksum(odp_packet_t pkt)
{
	odph_ipv4hdr_t	*iph;
	odph_udphdr_t	*udph;
	uint32_t	sum;
	uint16_t	udplen, *buf;
	union {
		uint8_t v8[2];
		uint16_t v16;
	} val;

	if (odp_packet_l4_offset(pkt) == ODP_PACKET_OFFSET_INVALID)
		return 0;
	iph = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	udph = (odph_udphdr_t *)odp_packet_l4_ptr(pkt, NULL);
	/* 32-bit sum of UDP pseudo-header, seen as a series of 16-bit words */
	sum = (iph->src_addr & 0xFFFF) + (iph->src_addr >> 16) +
			(iph->dst_addr & 0xFFFF) + (iph->dst_addr >> 16) +
			udph->length;
	val.v8[0] = 0;
	val.v8[1] = iph->proto;
	sum += val.v16;
	/* 32-bit sum of UDP header (checksum field cleared) and UDP data, seen
	 * as a series of 16-bit words */
	udplen = odp_be_to_cpu_16(udph->length);
	buf = (uint16_t *)((void *)udph);
	for ( ; udplen > 1; udplen -= 2)
		sum += *buf++;
	/* Length is not a multiple of 2 bytes */
	if (udplen) {
		val.v8[0] = *buf;
		val.v8[1] = 0;
		sum += val.v16;
	}
	/* Fold sum to 16 bits */
	sum = (sum & 0xFFFF) + (sum >> 16);
	/* Add carrier (0/1) to result */
	sum += (sum >> 16);
	/* 1's complement */
	sum = ~sum;
	/* Return checksum in BE endianness */
	return (sum == 0x0) ? 0xFFFF : sum;
}

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_udphdr_t) == ODPH_UDPHDR_LEN, "ODPH_UDPHDR_T__SIZE_ERROR");

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
