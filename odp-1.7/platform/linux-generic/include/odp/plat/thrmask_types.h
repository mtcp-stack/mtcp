/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP thread masks
 */

#ifndef ODP_THRMASK_TYPES_H_
#define ODP_THRMASK_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_thread
 *  @{
 */

#include <odp/cpumask.h>

/**
 * Minimum size of output buffer for odp_thrmask_to_str()
 */
#define ODP_THRMASK_STR_SIZE ODP_CPUMASK_STR_SIZE

/**
 * Thread mask
 *
 * Don't access directly, use access functions.
 */
typedef struct odp_thrmask_t {
	odp_cpumask_t m; /**< @private Mask*/
} odp_thrmask_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
