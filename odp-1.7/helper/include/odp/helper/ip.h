/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP IP header
 */

#ifndef ODPH_IP_H_
#define ODPH_IP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/align.h>
#include <odp/debug.h>
#include <odp/byteorder.h>
#include <odp/helper/chksum.h>

#include <string.h>

/** @addtogroup odph_header ODPH HEADER
 *  @{
 */

#define ODPH_IPV4             4  /**< IP version 4 */
#define ODPH_IPV4HDR_LEN     20  /**< Min length of IP header (no options) */
#define ODPH_IPV4HDR_IHL_MIN  5  /**< Minimum IHL value*/

/** @internal Returns IPv4 version */
#define ODPH_IPV4HDR_VER(ver_ihl) (((ver_ihl) & 0xf0) >> 4)

/** @internal Returns IPv4 header length */
#define ODPH_IPV4HDR_IHL(ver_ihl) ((ver_ihl) & 0x0f)

/** @internal Returns IPv4 DSCP */
#define ODPH_IPV4HDR_DSCP(tos) (((tos) & 0xfc) >> 2)

/** @internal Returns IPv4 Don't fragment */
#define ODPH_IPV4HDR_FLAGS_DONT_FRAG(frag_offset)  ((frag_offset) & 0x4000)

/** @internal Returns IPv4 more fragments */
#define ODPH_IPV4HDR_FLAGS_MORE_FRAGS(frag_offset)  ((frag_offset) & 0x2000)

/** @internal Returns IPv4 fragment offset */
#define ODPH_IPV4HDR_FRAG_OFFSET(frag_offset) ((frag_offset) & 0x1fff)

/** @internal Returns true if IPv4 packet is a fragment */
#define ODPH_IPV4HDR_IS_FRAGMENT(frag_offset) ((frag_offset) & 0x3fff)

/** @internal Returns IPv4 DSCP */
#define ODPH_IPV6HDR_DSCP(ver_tc_flow) (uint8_t)((((ver_tc_flow) & 0x0fc00000) >> 22) & 0xff)

/** IPv4 header */
typedef struct ODP_PACKED {
	uint8_t    ver_ihl;     /**< Version / Header length */
	uint8_t    tos;         /**< Type of service */
	odp_u16be_t tot_len;    /**< Total length */
	odp_u16be_t id;         /**< ID */
	odp_u16be_t frag_offset;/**< Fragmentation offset */
	uint8_t    ttl;         /**< Time to live */
	uint8_t    proto;       /**< Protocol */
	odp_u16sum_t chksum;    /**< Checksum */
	odp_u32be_t src_addr;   /**< Source address */
	odp_u32be_t dst_addr;   /**< Destination address */
} odph_ipv4hdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_ipv4hdr_t) == ODPH_IPV4HDR_LEN, "ODPH_IPV4HDR_T__SIZE_ERROR");

/**
 * Check if IPv4 checksum is valid
 *
 * @param pkt  ODP packet
 *
 * @return 1 if checksum is valid, otherwise 0
 */
static inline int odph_ipv4_csum_valid(odp_packet_t pkt)
{
	odp_u16be_t res = 0;
	uint16_t *w;
	int nleft = sizeof(odph_ipv4hdr_t);
	odph_ipv4hdr_t ip;
	odp_u16be_t chksum;

	if (!odp_packet_l3_offset(pkt))
		return 0;

	odp_packet_copydata_out(pkt, odp_packet_l3_offset(pkt),
				sizeof(odph_ipv4hdr_t), &ip);

	w = (uint16_t *)(void *)&ip;
	chksum = ip.chksum;
	ip.chksum = 0x0;

	res = odp_chksum(w, nleft);
	return (res == chksum) ? 1 : 0;
}

/**
 * Calculate and fill in IPv4 checksum
 *
 * @note when using this api to populate data destined for the wire
 * odp_cpu_to_be_16() can be used to remove sparse warnings
 *
 * @param pkt  ODP packet
 *
 * @return IPv4 checksum in host cpu order, or 0 on failure
 */
static inline odp_u16sum_t odph_ipv4_csum_update(odp_packet_t pkt)
{
	uint16_t *w;
	odph_ipv4hdr_t *ip;
	int nleft = sizeof(odph_ipv4hdr_t);

	if (!odp_packet_l3_offset(pkt))
		return 0;

	ip = (odph_ipv4hdr_t *)odp_packet_l3_ptr(pkt, NULL);
	w = (uint16_t *)(void *)ip;
	ip->chksum = odp_chksum(w, nleft);
	return ip->chksum;
}

/** IPv6 version */
#define ODPH_IPV6 6

/** IPv6 header length */
#define ODPH_IPV6HDR_LEN 40

/**
 * IPv6 header
 */
typedef struct ODP_PACKED {
	odp_u32be_t ver_tc_flow; /**< Version / Traffic class / Flow label */
	odp_u16be_t payload_len; /**< Payload length */
	uint8_t    next_hdr;     /**< Next header */
	uint8_t    hop_limit;    /**< Hop limit */
	uint8_t    src_addr[16]; /**< Source address */
	uint8_t    dst_addr[16]; /**< Destination address */
} odph_ipv6hdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_ipv6hdr_t) == ODPH_IPV6HDR_LEN, "ODPH_IPV6HDR_T__SIZE_ERROR");

/**
 * IPv6 Header extensions
 */
typedef struct ODP_PACKED {
	uint8_t    next_hdr;     /**< Protocol of next header */
	uint8_t    ext_len;      /**< Length of this extension in 8 byte units,
				    not counting first 8 bytes, so 0 = 8 bytes
				    1 = 16 bytes, etc. */
	uint8_t    filler[6];    /**< Fill out first 8 byte segment */
} odph_ipv6hdr_ext_t;

/** @name
 * IP protocol values (IPv4:'proto' or IPv6:'next_hdr')
 * @{*/
#define ODPH_IPPROTO_HOPOPTS 0x00 /**< IPv6 hop-by-hop options */
#define ODPH_IPPROTO_ICMP    0x01 /**< Internet Control Message Protocol (1) */
#define ODPH_IPPROTO_TCP     0x06 /**< Transmission Control Protocol (6) */
#define ODPH_IPPROTO_UDP     0x11 /**< User Datagram Protocol (17) */
#define ODPH_IPPROTO_ROUTE   0x2B /**< IPv6 Routing header (43) */
#define ODPH_IPPROTO_FRAG    0x2C /**< IPv6 Fragment (44) */
#define ODPH_IPPROTO_AH      0x33 /**< Authentication Header (51) */
#define ODPH_IPPROTO_ESP     0x32 /**< Encapsulating Security Payload (50) */
#define ODPH_IPPROTO_INVALID 0xFF /**< Reserved invalid by IANA */

/**@}*/

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif
