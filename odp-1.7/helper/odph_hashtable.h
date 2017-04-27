/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP Hash Table
 */

#ifndef ODPH_HASH_TABLE_H_
#define ODPH_HASH_TABLE_H_

#include <odp/helper/table.h>

#ifdef __cplusplus
extern "C" {
#endif

odph_table_t odph_hash_table_create(const char *name,
				    uint32_t capacity,
				    uint32_t key_size,
				    uint32_t value_size);
odph_table_t odph_hash_table_lookup(const char *name);
int odph_hash_table_destroy(odph_table_t table);
int odph_hash_put_value(odph_table_t table, void *key, void *value);
int odph_hash_get_value(odph_table_t table, void *key, void *buffer,
			uint32_t buffer_size);
int odph_hash_remove_value(odph_table_t table, void *key);

extern odph_table_ops_t odph_hash_table_ops;

#ifdef __cplusplus
}
#endif

#endif

