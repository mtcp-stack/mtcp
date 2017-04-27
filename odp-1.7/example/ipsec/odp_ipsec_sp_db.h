/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_SP_DB_H_
#define ODP_IPSEC_SP_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_ipsec_misc.h>

/**
 * Security Policy (SP) data base entry
 */
typedef struct sp_db_entry_s {
	struct sp_db_entry_s *next;        /**< Next entry on list */
	ip_addr_range_t       src_subnet;  /**< Source IPv4 subnet/range */
	ip_addr_range_t       dst_subnet;  /**< Destination IPv4 subnet/range */
	odp_bool_t            input;       /**< Direction when applied */
	odp_bool_t            esp;         /**< Enable cipher (ESP) */
	odp_bool_t            ah;          /**< Enable authentication (AH) */
} sp_db_entry_t;

/**
 * Security Policy (SP) data base global structure
 */
typedef struct sp_db_s {
	uint32_t         index;          /**< Index of next available entry */
	sp_db_entry_t   *list;		 /**< List of active entries */
	sp_db_entry_t    array[MAX_DB];	 /**< Entry storage */
} sp_db_t;

/** Global pointer to sp db */
extern sp_db_t *sp_db;

/** Initialize SP database global control structure */
void init_sp_db(void);

/**
 * Create an SP DB entry
 *
 * String is of the format "SrcSubNet:DstSubNet:(in|out):(ah|esp|both)"
 *
 * @param input  Pointer to string describing SP
 *
 * @return 0 if successful else -1
 */
int create_sp_db_entry(char *input);

/**
 * Display one SP DB entry
 *
 * @param entry  Pointer to entry to display
 */
void dump_sp_db_entry(sp_db_entry_t *entry);

/**
 * Display the SP DB
 */
void dump_sp_db(void);

#ifdef __cplusplus
}
#endif

#endif
