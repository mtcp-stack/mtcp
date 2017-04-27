/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP recursive spinlock
 */

#ifndef ODP_SPINLOCK_RECURSIVE_TYPES_H_
#define ODP_SPINLOCK_RECURSIVE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/spinlock.h>
#include <odp/std_types.h>

/** @internal */
struct odp_spinlock_recursive_s {
	odp_spinlock_t lock; /**< the lock */
	int owner;           /**< thread owning the lock */
	uint32_t cnt;        /**< recursion count */
};

typedef struct odp_spinlock_recursive_s odp_spinlock_recursive_t;

#ifdef __cplusplus
}
#endif

#endif
