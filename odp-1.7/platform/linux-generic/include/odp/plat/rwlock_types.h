/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP rwlock
 */

#ifndef ODP_RWLOCK_TYPES_H_
#define ODP_RWLOCK_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/atomic.h>

/** @internal */
struct odp_rwlock_s {
	odp_atomic_u32_t cnt; /**< lock count
				0 lock not taken
				-1 write lock taken
				>0 read lock(s) taken */
};

typedef struct odp_rwlock_s odp_rwlock_t;

#ifdef __cplusplus
}
#endif

#endif
