/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "odph_lineartable.h"
#include "odph_debug.h"
#include <odp.h>

#define     ODPH_SUCCESS	0
#define     ODPH_FAIL		-1

/** @magic word, write to the first byte of the memory block
 *  to indicate this block is used by a linear table structure
 */
#define     ODPH_LINEAR_TABLE_MAGIC_WORD   0xEFEFFEFE

/** @internal table struct
 *   For linear table, value is orgnized as a big array,
 *   and key is the index of this array, so we just need to record the
 *   content of value, and make sure the key won't overflow
 */
typedef struct {
	uint32_t magicword; /**< for check */
	uint32_t init_cap; /**< input param of capacity */
	/** given the capacity, caculate out the max supported nodes number */
	uint32_t node_sum;
	/** size of a lineartable element,including the rwlock in the head */
	uint32_t value_size;
	void *value_array; /**< value pool in array format */
	char name[ODPH_TABLE_NAME_LEN]; /**< name of the table */
} odph_linear_table_imp;

/** Note: for linear table, key must be an number, its size is fixed 4.
 *  So, we ignore the input key_size here
 */

odph_table_t odph_linear_table_create(const char *name, uint32_t capacity,
				      uint32_t ODP_IGNORED, uint32_t value_size)
{
	int idx;
	uint32_t node_num;
	odp_shm_t shmem;
	odph_linear_table_imp *tbl;

	if (strlen(name) >= ODPH_TABLE_NAME_LEN || capacity < 1 ||
	    capacity >= 0x1000 || value_size == 0) {
		printf("create para input error or less than !");
		return NULL;
	}
	/* check name confict in shm*/
	tbl = (odph_linear_table_imp *)odp_shm_addr(odp_shm_lookup(name));
	if (tbl != NULL) {
		ODPH_DBG("name already exist\n");
		return NULL;
	}

	/* alloc memory from shm */
	shmem = odp_shm_reserve(name, capacity << 20, 64, ODP_SHM_SW_ONLY);
	if (shmem == ODP_SHM_INVALID) {
		ODPH_DBG("shm reserve fail\n");
		return NULL;
	}
	tbl = (odph_linear_table_imp *)odp_shm_addr(shmem);

	/* clean this block of memory */
	memset(tbl, 0, capacity << 20);

	tbl->init_cap = capacity < 20;

	strncpy(tbl->name, name, ODPH_TABLE_NAME_LEN - 1);

	/* for linear table, the key is just the index, without confict
	 * so we just need to record the value content
	 * there is a rwlock in the head of every node
	 */

	tbl->value_size = value_size + sizeof(odp_rwlock_t);

	node_num = tbl->init_cap / tbl->value_size;
	tbl->node_sum = node_num;

	tbl->value_array = (void *)((char *)tbl
			+ sizeof(odph_linear_table_imp));

	/* initialize rwlock*/
	for (idx = 0; idx < tbl->node_sum; idx++) {
		odp_rwlock_t *lock = (odp_rwlock_t *)((char *)tbl->value_array
				+ idx * tbl->value_size);
		odp_rwlock_init(lock);
	}

	tbl->magicword = ODPH_LINEAR_TABLE_MAGIC_WORD;

	return (odph_table_t)(tbl);
}

int odph_linear_table_destroy(odph_table_t table)
{
	int ret;
	odph_linear_table_imp *linear_tbl = NULL;

	if (table != NULL) {
		linear_tbl = (odph_linear_table_imp *)table;

		/* check magicword, make sure the memory is used by a table */
		if (linear_tbl->magicword != ODPH_LINEAR_TABLE_MAGIC_WORD)
			return ODPH_FAIL;

		ret = odp_shm_free(odp_shm_lookup(linear_tbl->name));
		if (ret != 0) {
			ODPH_DBG("free fail\n");
			return ret;
		}

		return ODPH_SUCCESS;
	}
	return ODPH_FAIL;
}

odph_table_t odph_linear_table_lookup(const char *name)
{
	odph_linear_table_imp *tbl;

	if (name == NULL || strlen(name) >= ODPH_TABLE_NAME_LEN)
		return NULL;

	tbl = (odph_linear_table_imp *)odp_shm_addr(odp_shm_lookup(name));

	/* check magicword to make sure the memory block is used by a table */
	if (tbl != NULL &&
	    tbl->magicword == ODPH_LINEAR_TABLE_MAGIC_WORD &&
	    strcmp(tbl->name, name) == 0)
		return (odph_table_t)tbl;

	return NULL;
}

/* should make sure the input table exists and is available */
int odph_lineartable_put_value(odph_table_t table, void *key, void *value)
{
	odph_linear_table_imp *tbl = (odph_linear_table_imp *)table;
	uint32_t ikey = 0;
	void *entry = NULL;
	odp_rwlock_t *lock = NULL;

	if (table == NULL || key == NULL || value == NULL)
		return ODPH_FAIL;

	ikey = *(uint32_t *)key;
	if (ikey >= tbl->node_sum)
		return ODPH_FAIL;

	entry = (void *)((char *)tbl->value_array + ikey * tbl->value_size);
	lock = (odp_rwlock_t *)entry;
	entry = (char *)entry + sizeof(odp_rwlock_t);

	odp_rwlock_write_lock(lock);

	memcpy(entry, value, tbl->value_size - sizeof(odp_rwlock_t));

	odp_rwlock_write_unlock(lock);

	return ODPH_SUCCESS;
}

/* should make sure the input table exists and is available */
int odph_lineartable_get_value(odph_table_t table, void *key, void *buffer,
			       uint32_t buffer_size)
{
	odph_linear_table_imp *tbl = (odph_linear_table_imp *)table;
	uint32_t ikey = 0;
	void *entry = NULL;
	odp_rwlock_t *lock = NULL;

	if (table == NULL || key == NULL || buffer == NULL)
		return ODPH_FAIL;

	ikey = *(uint32_t *)key;
	if (ikey >= tbl->node_sum)
		return ODPH_FAIL;

	entry = (void *)((char *)tbl->value_array + ikey * tbl->value_size);
	lock = (odp_rwlock_t *)entry;
	entry = (char *)entry + sizeof(odp_rwlock_t);

	odp_rwlock_read_lock(lock);

	memcpy(buffer, entry, tbl->value_size - sizeof(odp_rwlock_t));

	odp_rwlock_read_unlock(lock);

	return ODPH_SUCCESS;
}

odph_table_ops_t odph_linear_table_ops = {
	odph_linear_table_create,
	odph_linear_table_lookup,
	odph_linear_table_destroy,
	odph_lineartable_put_value,
	odph_lineartable_get_value,
	NULL,
	};

