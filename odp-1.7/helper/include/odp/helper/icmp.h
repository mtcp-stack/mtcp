/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:		 BSD-3-Clause
 */


/**
 * @file
 *
 * ODP ICMP header
 */

#ifndef ODPH_ICMP_H_
#define ODPH_ICMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/align.h>
#include <odp/debug.h>
#include <odp/byteorder.h>

/** @addtogroup odph_header ODPH HEADER
 *  @{
 */

/** ICMP header length */
#define ODPH_ICMPHDR_LEN 8

/** ICMP header */
typedef struct ODP_PACKED {
	uint8_t type;		/**< message type */
	uint8_t code;		/**< type sub-code */
	odp_u16sum_t chksum;	/**< checksum of icmp header */
	union {
		struct {
			odp_u16be_t id;
			odp_u16be_t sequence;
		} echo;			/**< echo datagram */
		odp_u32be_t gateway;	/**< gateway address */
		struct {
			odp_u16be_t __unused;
			odp_u16be_t mtu;
		} frag;			/**< path mtu discovery */
	} un;			/**< icmp sub header */
} odph_icmphdr_t;

#define ICMP_ECHOREPLY		0	/**< Echo Reply			*/
#define ICMP_DEST_UNREACH	3	/**< Destination Unreachable	*/
#define ICMP_SOURCE_QUENCH	4	/**< Source Quench		*/
#define ICMP_REDIRECT		5	/**< Redirect (change route)	*/
#define ICMP_ECHO		8	/**< Echo Request		*/
#define ICMP_TIME_EXCEEDED	11	/**< Time Exceeded		*/
#define ICMP_PARAMETERPROB	12	/**< Parameter Problem		*/
#define ICMP_TIMESTAMP		13	/**< Timestamp Request		*/
#define ICMP_TIMESTAMPREPLY	14	/**< Timestamp Reply		*/
#define ICMP_INFO_REQUEST	15	/**< Information Request	*/
#define ICMP_INFO_REPLY		16	/**< Information Reply		*/
#define ICMP_ADDRESS		17	/**< Address Mask Request	*/
#define ICMP_ADDRESSREPLY	18	/**< Address Mask Reply		*/
#define NR_ICMP_TYPES		18	/**< Number of icmp types	*/

/* Codes for UNREACH. */
#define ICMP_NET_UNREACH	0	/**< Network Unreachable	*/
#define ICMP_HOST_UNREACH	1	/**< Host Unreachable		*/
#define ICMP_PROT_UNREACH	2	/**< Protocol Unreachable	*/
#define ICMP_PORT_UNREACH	3	/**< Port Unreachable		*/
#define ICMP_FRAG_NEEDED	4	/**< Fragmentation Needed/DF set*/
#define ICMP_SR_FAILED		5	/**< Source Route failed	*/
#define ICMP_NET_UNKNOWN	6	/**< Network Unknown		*/
#define ICMP_HOST_UNKNOWN	7	/**< Host Unknown		*/
#define ICMP_HOST_ISOLATED	8	/**< Host Isolated		*/
#define ICMP_NET_ANO		9	/**< ICMP_NET_ANO		*/
#define ICMP_HOST_ANO		10	/**< ICMP_HOST_ANO		*/
#define ICMP_NET_UNR_TOS	11	/**< ICMP_NET_UNR_TOS		*/
#define ICMP_HOST_UNR_TOS	12	/**< ICMP_HOST_UNR_TOS		*/
#define ICMP_PKT_FILTERED	13	/**< Packet filtered		*/
#define ICMP_PREC_VIOLATION	14	/**< Precedence violation	*/
#define ICMP_PREC_CUTOFF	15	/**< Precedence cut off		*/
#define NR_ICMP_UNREACH		15	/**< instead of hardcoding
							immediate value */

/* Codes for REDIRECT. */
#define ICMP_REDIR_NET		0	/**< Redirect Net		*/
#define ICMP_REDIR_HOST		1	/**< Redirect Host		*/
#define ICMP_REDIR_NETTOS	2	/**< Redirect Net for TOS	*/
#define ICMP_REDIR_HOSTTOS	3	/**< Redirect Host for TOS	*/

/* Codes for TIME_EXCEEDED. */
#define ICMP_EXC_TTL		0	/**< TTL count exceeded		*/
#define ICMP_EXC_FRAGTIME	1	/**< Fragment Reass time
								exceeded*/

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_icmphdr_t) == ODPH_ICMPHDR_LEN, "ODPH_ICMPHDR_T__SIZE_ERROR");

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
