/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:   BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "odph_hashtable.h"
#include "odph_list_internal.h"
#include "odph_debug.h"
#include <odp.h>

#define    ODPH_SUCCESS	0
#define    ODPH_FAIL	-1

/** @magic word, write to the first byte of the memory block
 *	to indicate this block is used by a hash table structure
 */
#define    ODPH_HASH_TABLE_MAGIC_WORD	0xABABBABA

/** @support 64k buckets. Bucket is a list that composed of
 * elements with the same HASH-value but different keys
 */
#define    ODPH_MAX_BUCKET_NUM			0x10000

/** @inner element structure of hash table
 * To resolve the hash confict:
 * we put the elements with different keys but a same HASH-value
 * into a list
 */
typedef struct odph_hash_node {
	/** list structure,for list opt */
	odph_list_object list_node;
	/** Flexible Array,memory will be alloced when table has been created
	 * Its length is key_size + value_size,
	 * suppose key_size = m; value_size = n;
	 * its structure is like:
	 * k_byte1 k_byte2...k_byten v_byte1...v_bytem
	 */
	char content[0];
} odph_hash_node;

typedef struct {
	uint32_t magicword; /**< for check */
	uint32_t key_size; /**< input param when create,in Bytes */
	uint32_t value_size; /**< input param when create,in Bytes */
	uint32_t init_cap; /**< input param when create,in Bytes */
	/** multi-process support,every list has one rw lock */
	odp_rwlock_t *lock_pool;
	/** table bucket pool,every hash value has one list head */
	odph_list_head *list_head_pool;
	/** number of the list head in list_head_pool */
	uint32_t head_num;
	/** table element pool */
	odph_hash_node *hash_node_pool;
	/** number of element in the hash_node_pool */
	uint32_t hash_node_num;
	char rsv[7]; /**< Reserved,for alignment */
	char name[ODPH_TABLE_NAME_LEN]; /**< table name */
} odph_hash_table_imp;

odph_table_t odph_hash_table_create(const char *name, uint32_t capacity,
				    uint32_t key_size,
				    uint32_t value_size)
{
	int i;
	uint32_t node_num;
	odph_hash_table_imp *tbl;
	odp_shm_t shmem;
	uint32_t node_mem;

	if (strlen(name) >= ODPH_TABLE_NAME_LEN || capacity < 1 ||
	    capacity >= 0x1000 || key_size == 0 || value_size == 0) {
		ODPH_DBG("create para input error!\n");
		return NULL;
	}
	tbl = (odph_hash_table_imp *)odp_shm_addr(odp_shm_lookup(name));
	if (tbl != NULL) {
		ODPH_DBG("name already exist\n");
		return NULL;
	}
	shmem = odp_shm_reserve(name, capacity << 20, 64, ODP_SHM_SW_ONLY);
	if (shmem == ODP_SHM_INVALID) {
		ODPH_DBG("shm reserve fail\n");
		return NULL;
	}
	tbl = (odph_hash_table_imp *)odp_shm_addr(shmem);

	/* clean this block of memory */
	memset(tbl, 0, capacity << 20);

	tbl->init_cap = capacity << 20;
	strncpy(tbl->name, name, ODPH_TABLE_NAME_LEN - 1);
	tbl->key_size = key_size;
	tbl->value_size = value_size;

	/* header of this mem block is the table control struct,
	 * then the lock pool, then the list header pool
	 * the last part is the element node pool
	 */

	tbl->lock_pool = (odp_rwlock_t *)((char *)tbl
			+ sizeof(odph_hash_table_imp));
	tbl->list_head_pool = (odph_list_head *)((char *)tbl->lock_pool
			+ ODPH_MAX_BUCKET_NUM * sizeof(odp_rwlock_t));

	node_mem = tbl->init_cap - sizeof(odph_hash_table_imp)
		- ODPH_MAX_BUCKET_NUM * sizeof(odph_list_head)
		- ODPH_MAX_BUCKET_NUM * sizeof(odp_rwlock_t);

	node_num = node_mem / (sizeof(odph_hash_node) + key_size + value_size);
	tbl->hash_node_num = node_num;
	tbl->hash_node_pool = (odph_hash_node *)((char *)tbl->list_head_pool
				+ ODPH_MAX_BUCKET_NUM * sizeof(odph_list_head));

	/* init every list head and rw lock */
	for (i = 0; i < ODPH_MAX_BUCKET_NUM; i++) {
		ODPH_INIT_LIST_HEAD(&tbl->list_head_pool[i]);
		odp_rwlock_init((odp_rwlock_t *)&tbl->lock_pool[i]);
	}

	tbl->magicword = ODPH_HASH_TABLE_MAGIC_WORD;
	return (odph_table_t)tbl;
}

int odph_hash_table_destroy(odph_table_t table)
{
	int ret;

	if (table != NULL) {
		odph_hash_table_imp *hash_tbl = (odph_hash_table_imp *)table;

		if (hash_tbl->magicword != ODPH_HASH_TABLE_MAGIC_WORD)
			return ODPH_FAIL;

		ret = odp_shm_free(odp_shm_lookup(hash_tbl->name));
		if (ret != 0) {
			ODPH_DBG("free fail\n");
			return ret;
		}
		/* clean head */
		return ODPH_SUCCESS;
	}
	return ODPH_FAIL;
}

odph_table_t odph_hash_table_lookup(const char *name)
{
	odph_hash_table_imp *hash_tbl;

	if (name == NULL || strlen(name) >= ODPH_TABLE_NAME_LEN)
		return NULL;

	hash_tbl = (odph_hash_table_imp *)odp_shm_addr(odp_shm_lookup(name));
	if (hash_tbl != NULL && strcmp(hash_tbl->name, name) == 0)
		return (odph_table_t)hash_tbl;
	return NULL;
}

/**
 * Calculate has value by the input key and key_size
 * This hash algorithm is the most simple one, so we choose it as an DEMO
 * User can use any other algorithm, like CRC...
 */
uint16_t odp_key_hash(void *key, uint32_t key_size)
{
	register uint32_t hash = 0;
	uint32_t idx = (key_size == 0 ? 1 : key_size);
	uint32_t ch;

	while (idx != 0) {
		ch = (uint32_t)(*(char *)key);
		hash = hash * 131 + ch;
		idx--;
	}
	return (uint16_t)(hash & 0x0000FFFF);
}

/**
 * Get an available node from pool
 */
odph_hash_node *odp_hashnode_take(odph_table_t table)
{
	odph_hash_table_imp *tbl = (odph_hash_table_imp *)table;
	uint32_t idx;
	odph_hash_node *node;

	for (idx = 0; idx < tbl->hash_node_num; idx++) {
		/** notice: memory of one hash_node is
		 * not only sizeof(odph_hash_node)
		 * should add the size of Flexible Array
		 */
		node = (odph_hash_node *)((char *)tbl->hash_node_pool
				+ idx * (sizeof(odph_hash_node)
						+ tbl->key_size
						+ tbl->value_size));
		if (node->list_node.next == NULL &&
		    node->list_node.prev == NULL) {
			ODPH_INIT_LIST_HEAD(&node->list_node);
			return node;
		}
	}
	return NULL;
}

/**
 * Release an node to the pool
 */
void odp_hashnode_give(odph_table_t table, odph_hash_node *node)
{
	odph_hash_table_imp *tbl = (odph_hash_table_imp *)table;

	if (node == NULL)
		return;

	odph_list_del(&node->list_node);
	memset(node, 0,
	       (sizeof(odph_hash_node) + tbl->key_size + tbl->value_size));
}

/* should make sure the input table exists and is available */
int odph_hash_put_value(odph_table_t table, void *key, void *value)
{
	odph_hash_table_imp *tbl = (odph_hash_table_imp *)table;
	uint16_t hash = 0;
	odph_hash_node *node = NULL;
	char *tmp = NULL;

	if (table == NULL || key == NULL || value == NULL)
		return ODPH_FAIL;

	/* hash value is just the index of the list head in pool */
	hash = odp_key_hash(key, tbl->key_size);

	odp_rwlock_write_lock(&tbl->lock_pool[hash]);
	/* First, check if the key already exist */
	ODPH_LIST_FOR_EACH(node, &tbl->list_head_pool[hash], odph_hash_node,
			   list_node)
	{
		if (memcmp(node->content, key, tbl->key_size) == 0) {
			/* copy value content to hash node*/
			tmp = (void *)((char *)node->content + tbl->key_size);
			memcpy(tmp, value, tbl->value_size);
			odp_rwlock_write_unlock(&tbl->lock_pool[hash]);
			return ODPH_SUCCESS;
		}
	}

	/*if the key is a new one, get a new hash node form the pool */
	node = odp_hashnode_take(table);
	if (node == NULL) {
		odp_rwlock_write_unlock(&tbl->lock_pool[hash]);
		return ODPH_FAIL;
	}

	/* copy both key and value content to the hash node */
	memcpy(node->content, key, tbl->key_size);
	tmp = (void *)((char *)node->content + tbl->key_size);
	memcpy(tmp, value, tbl->value_size);

	/* add the node to list */
	odph_list_add(&node->list_node, &tbl->list_head_pool[hash]);

	odp_rwlock_write_unlock(&tbl->lock_pool[hash]);
	return ODPH_SUCCESS;
}

/* should make sure the input table exists and is available */
int odph_hash_get_value(odph_table_t table, void *key, void *buffer,
			uint32_t buffer_size)
{
	odph_hash_table_imp *tbl = (odph_hash_table_imp *)table;
	uint16_t hash = 0;
	odph_hash_node *node;
	char *tmp = NULL;

	if (table == NULL || key == NULL || buffer == NULL ||
	    buffer_size < tbl->value_size)
		return ODPH_FAIL;

	/* hash value is just the index of the list head in pool */
	hash = odp_key_hash(key, tbl->key_size);

	odp_rwlock_read_lock(&tbl->lock_pool[hash]);

	ODPH_LIST_FOR_EACH(node, &tbl->list_head_pool[hash],
			   odph_hash_node, list_node)
	{
		/* in case of hash conflict, compare the whole key */
		if (memcmp(node->content, key, tbl->key_size) == 0) {
			/* find the target */
			tmp = (void *)((char *)node->content + tbl->key_size);
			memcpy(buffer, tmp, tbl->value_size);

			odp_rwlock_read_unlock(&tbl->lock_pool[hash]);

			return ODPH_SUCCESS;
		}
	}

	odp_rwlock_read_unlock(&tbl->lock_pool[hash]);

	return ODPH_FAIL;
}

/* should make sure the input table exists and is available */
int odph_hash_remove_value(odph_table_t table, void *key)
{
	odph_hash_table_imp *tbl = (odph_hash_table_imp *)table;
	uint16_t hash = 0;
	odph_hash_node *node;

	if (table == NULL || key == NULL)
		return ODPH_FAIL;

	/* hash value is just the index of the list head in pool */
	hash = odp_key_hash(key, tbl->key_size);

	odp_rwlock_write_lock(&tbl->lock_pool[hash]);

	ODPH_LIST_FOR_EACH(node, &tbl->list_head_pool[hash], odph_hash_node,
			   list_node)
	{
		if (memcmp(node->content, key, tbl->key_size) == 0) {
			odp_hashnode_give(table, node);
			odp_rwlock_write_unlock(&tbl->lock_pool[hash]);
			return ODPH_SUCCESS;
		}
	}

	odp_rwlock_write_unlock(&tbl->lock_pool[hash]);

	return ODPH_SUCCESS;
}

odph_table_ops_t odph_hash_table_ops = {
	odph_hash_table_create,
	odph_hash_table_lookup,
	odph_hash_table_destroy,
	odph_hash_put_value,
	odph_hash_get_value,
	odph_hash_remove_value};

