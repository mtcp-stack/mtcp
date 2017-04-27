/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP timer service
 */

#ifndef ODP_TIMER_TYPES_H_
#define ODP_TIMER_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_timer
 *  @{
 **/

struct odp_timer_pool_s; /**< Forward declaration */

typedef struct odp_timer_pool_s *odp_timer_pool_t;

#define ODP_TIMER_POOL_INVALID NULL

typedef uint32_t odp_timer_t;

#define ODP_TIMER_INVALID ((uint32_t)~0U)

typedef void *odp_timeout_t;

#define ODP_TIMEOUT_INVALID NULL

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
