/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP ethernet header
 */

#ifndef ODPH_ETH_H_
#define ODPH_ETH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/byteorder.h>
#include <odp/align.h>
#include <odp/debug.h>

/** @addtogroup odph_header ODPH HEADER
 *  @{
 */

#define ODPH_ETHADDR_LEN     6    /**< Ethernet address length */
#define ODPH_ETHHDR_LEN      14   /**< Ethernet header length */
#define ODPH_VLANHDR_LEN     4    /**< VLAN header length */
#define ODPH_ETH_LEN_MIN     60   /**< Min frame length (excl CRC 4 bytes) */
#define ODPH_ETH_LEN_MIN_CRC 64   /**< Min frame length (incl CRC 4 bytes) */
#define ODPH_ETH_LEN_MAX     1514 /**< Max frame length (excl CRC 4 bytes) */
#define ODPH_ETH_LEN_MAX_CRC 1518 /**< Max frame length (incl CRC 4 bytes) */

/**
 * Ethernet MAC address
 */
typedef struct ODP_PACKED {
	uint8_t addr[ODPH_ETHADDR_LEN]; /**< @private Address */
} odph_ethaddr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_ethaddr_t) == ODPH_ETHADDR_LEN, "ODPH_ETHADDR_T__SIZE_ERROR");

/**
 * Ethernet header
 */
typedef struct ODP_PACKED {
	odph_ethaddr_t dst; /**< Destination address */
	odph_ethaddr_t src; /**< Source address */
	odp_u16be_t type;   /**< Type */
} odph_ethhdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_ethhdr_t) == ODPH_ETHHDR_LEN, "ODPH_ETHHDR_T__SIZE_ERROR");

/**
 * VLAN header
 *
 * @todo Check usage of tpid vs ethertype. Check outer VLAN TPID.
 */
typedef struct ODP_PACKED {
	odp_u16be_t tpid;  /**< Tag protocol ID (located after ethhdr.src) */
	odp_u16be_t tci;   /**< Priority / CFI / VLAN ID */
} odph_vlanhdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_vlanhdr_t) == ODPH_VLANHDR_LEN, "ODPH_VLANHDR_T__SIZE_ERROR");


/* Ethernet header Ether Type ('type') values, a selected few */
#define ODPH_ETHTYPE_IPV4       0x0800 /**< Internet Protocol version 4 */
#define ODPH_ETHTYPE_ARP        0x0806 /**< Address Resolution Protocol */
#define ODPH_ETHTYPE_RARP       0x8035 /**< Reverse Address Resolution Protocol*/
#define ODPH_ETHTYPE_VLAN       0x8100 /**< VLAN-tagged frame IEEE 802.1Q */
#define ODPH_ETHTYPE_VLAN_OUTER 0x88A8 /**< Stacked VLANs/QinQ, outer-tag/S-TAG*/
#define ODPH_ETHTYPE_IPV6       0x86dd /**< Internet Protocol version 6 */
#define ODPH_ETHTYPE_FLOW_CTRL  0x8808 /**< Ethernet flow control */
#define ODPH_ETHTYPE_MPLS       0x8847 /**< MPLS unicast */
#define ODPH_ETHTYPE_MPLS_MCAST 0x8848 /**< MPLS multicast */
#define ODPH_ETHTYPE_MACSEC     0x88E5 /**< MAC security IEEE 802.1AE */
#define ODPH_ETHTYPE_1588       0x88F7 /**< Precision Time Protocol IEEE 1588 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
