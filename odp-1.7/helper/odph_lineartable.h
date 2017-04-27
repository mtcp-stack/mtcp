/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP Linear Table
 */

#ifndef ODPH_LINEAR_TABLE_H_
#define ODPH_LINEAR_TABLE_H_

#include <stdint.h>
#include <odp/helper/table.h>

#ifdef __cplusplus
extern "C" {
#endif

odph_table_t odph_linear_table_create(const char *name,
				      uint32_t capacity,
				      uint32_t ODP_IGNORED,
				      uint32_t value_size);
odph_table_t odph_linear_table_lookup(const char *name);
int odph_linear_table_destroy(odph_table_t table);
int odph_linear_put_value(odph_table_t table, void *key, void *value);
int odph_linear_get_value(odph_table_t table, void *key, void *buffer,
			  uint32_t buffer_size);

extern odph_table_ops_t odph_linear_table_ops;

#ifdef __cplusplus
}
#endif

#endif

