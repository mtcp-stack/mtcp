/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP barrier
 */

#ifndef ODP_BARRIER_TYPES_H_
#define ODP_BARRIER_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/atomic.h>

/**
 * @internal
 * ODP thread synchronization barrier
 */
struct odp_barrier_s {
	uint32_t         count;  /**< Thread count */
	odp_atomic_u32_t bar;    /**< Barrier counter */
};

typedef struct odp_barrier_s odp_barrier_t;

#ifdef __cplusplus
}
#endif

#endif
