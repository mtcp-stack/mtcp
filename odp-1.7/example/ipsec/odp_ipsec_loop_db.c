/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include <example_debug.h>

#include <odp.h>

#include <odp_ipsec_loop_db.h>

loopback_db_t *loopback_db;

void init_loopback_db(void)
{
	int idx;
	odp_shm_t shm;

	shm = odp_shm_reserve("loopback_db",
			      sizeof(loopback_db_t),
			      ODP_CACHE_LINE_SIZE,
			      0);

	loopback_db = odp_shm_addr(shm);

	if (loopback_db == NULL) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(loopback_db, 0, sizeof(*loopback_db));

	for (idx = 0; idx < MAX_LOOPBACK; idx++) {
		loopback_db->intf[idx].inq_def = ODP_QUEUE_INVALID;
		loopback_db->intf[idx].outq_def = ODP_QUEUE_INVALID;
	}
}

void create_loopback_db_entry(int idx,
			      odp_queue_t inq_def,
			      odp_queue_t outq_def,
			      odp_pool_t pkt_pool)
{
	loopback_db_entry_t *entry = &loopback_db->intf[idx];

	/* Save queues */
	entry->inq_def = inq_def;
	entry->outq_def = outq_def;
	entry->pkt_pool = pkt_pool;

	/* Create dummy MAC address */
	memset(entry->mac, (0xF0 | idx), sizeof(entry->mac));
}
