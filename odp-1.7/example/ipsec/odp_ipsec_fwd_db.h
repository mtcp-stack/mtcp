/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_FWD_DB_H_
#define ODP_IPSEC_FWD_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>
#include <odp/helper/eth.h>
#include <odp_ipsec_misc.h>

#define OIF_LEN 32

/**
 * Forwarding data base entry
 */
typedef struct fwd_db_entry_s {
	struct fwd_db_entry_s *next;          /**< Next entry on list */
	char                   oif[OIF_LEN];  /**< Output interface name */
	odp_queue_t            queue;         /**< Output transmit queue */
	uint8_t   src_mac[ODPH_ETHADDR_LEN];  /**< Output source MAC */
	uint8_t   dst_mac[ODPH_ETHADDR_LEN];  /**< Output destination MAC */
	ip_addr_range_t        subnet;        /**< Subnet for this router */
} fwd_db_entry_t;

/**
 * Forwarding data base global structure
 */
typedef struct fwd_db_s {
	uint32_t          index;          /**< Next available entry */
	fwd_db_entry_t   *list;           /**< List of active routes */
	fwd_db_entry_t    array[MAX_DB];  /**< Entry storage */
} fwd_db_t;

/** Global pointer to fwd db */
extern fwd_db_t *fwd_db;

/** Initialize FWD DB */
void init_fwd_db(void);

/**
 * Create a forwarding database entry
 *
 * String is of the format "SubNet:Intf:NextHopMAC"
 *
 * @param input  Pointer to string describing route
 *
 * @return 0 if successful else -1
 */
int create_fwd_db_entry(char *input);

/**
 * Scan FWD DB entries and resolve output queue and source MAC address
 *
 * @param intf   Interface name string
 * @param outq   Output queue for packet transmit
 * @param mac    MAC address of this interface
 */
void resolve_fwd_db(char *intf, odp_queue_t outq, uint8_t *mac);

/**
 * Display one fowarding database entry
 *
 * @param entry  Pointer to entry to display
 */
void dump_fwd_db_entry(fwd_db_entry_t *entry);

/**
 * Display the forwarding database
 */
void dump_fwd_db(void);

/**
 * Find a matching forwarding database entry
 *
 * @param dst_ip  Destination IPv4 address
 *
 * @return pointer to forwarding DB entry else NULL
 */
fwd_db_entry_t *find_fwd_db_entry(uint32_t dst_ip);

#ifdef __cplusplus
}
#endif

#endif
