/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_LOOP_DB_H_
#define ODP_IPSEC_LOOP_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>
#include <odp/helper/eth.h>
#include <odp_ipsec_misc.h>

/**
 * Loopback database entry structure
 */
typedef struct loopback_db_entry_s {
	odp_queue_t inq_def;
	odp_queue_t outq_def;
	odp_pool_t  pkt_pool;
	uint8_t     mac[ODPH_ETHADDR_LEN];
} loopback_db_entry_t;

typedef struct loopback_db_s {
	loopback_db_entry_t  intf[MAX_LOOPBACK];
} loopback_db_t;

extern loopback_db_t *loopback_db;

/** Initialize loopback database global control structure */
void init_loopback_db(void);

/**
 * Create loopback DB entry for an interface
 *
 * Loopback interfaces are specified from command line with
 * an index 0-9.
 *
 * @param idx      Index of interface in database
 * @param inq_def  Input queue
 * @param outq_def Output queue
 * @param pkt_pool Pool to create packets from
 */
void create_loopback_db_entry(int idx,
			      odp_queue_t inq_def,
			      odp_queue_t outq_def,
			      odp_pool_t pkt_pool);

/**
 * Parse loop interface index
 *
 * @param b     Pointer to buffer to parse
 *
 * @return interface index (0 to (MAX_LOOPBACK - 1)) else -1
 */
static inline
int loop_if_index(char *b)
{
	int ret;
	int idx;

	/* Derive loopback interface index */
	ret = sscanf(b, "loop%d", &idx);
	if ((1 != ret) || (idx < 0) || (idx >= MAX_LOOPBACK))
		return -1;
	return idx;
}

/**
 * Query loopback DB entry MAC address
 *
 * @param idx     Loopback DB index of the interface
 *
 * @return MAC address pointer
 */
static inline
uint8_t *query_loopback_db_mac(int idx)
{
	return loopback_db->intf[idx].mac;
}

/**
 * Query loopback DB entry input queue
 *
 * @param idx     Loopback DB index of the interface
 *
 * @return ODP queue
 */
static inline
odp_queue_t query_loopback_db_inq(int idx)
{
	return loopback_db->intf[idx].inq_def;
}

/**
 * Query loopback DB entry output queue
 *
 * @param idx     Loopback DB index of the interface
 *
 * @return ODP queue
 */
static inline
odp_queue_t query_loopback_db_outq(int idx)
{
	return loopback_db->intf[idx].outq_def;
}

/**
 * Query loopback DB entry packet pool
 *
 * @param idx     Loopback DB index of the interface
 *
 * @return ODP buffer pool
 */
static inline
odp_pool_t query_loopback_db_pkt_pool(int idx)
{
	return loopback_db->intf[idx].pkt_pool;
}

#ifdef __cplusplus
}
#endif

#endif
