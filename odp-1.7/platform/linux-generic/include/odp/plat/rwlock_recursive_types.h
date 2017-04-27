/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP recursive read/write lock
 */

#ifndef ODP_RWLOCK_RECURSIVE_TYPES_H_
#define ODP_RWLOCK_RECURSIVE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/rwlock.h>
#include <odp/std_types.h>
#include <odp/thread.h>

/** @internal */
struct odp_rwlock_recursive_s {
	odp_rwlock_t lock;                     /**< the lock */
	int wr_owner;                          /**< write owner thread */
	uint32_t wr_cnt;                       /**< write recursion count */
	uint8_t  rd_cnt[ODP_THREAD_COUNT_MAX]; /**< read recursion count */
};

typedef struct odp_rwlock_recursive_s odp_rwlock_recursive_t;

#ifdef __cplusplus
}
#endif

#endif
