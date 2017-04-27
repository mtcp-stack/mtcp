/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP time service
 */

#ifndef ODP_TIME_TYPES_H_
#define ODP_TIME_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_time
 *  @{
 **/

/**
 * @internal Time structure used to isolate linux-generic implementation from
 * the linux timespec structure, which is dependent on POSIX extension level.
 */
typedef struct odp_time_t {
	int64_t tv_sec;      /**< @internal Seconds */
	int64_t tv_nsec;     /**< @internal Nanoseconds */
} odp_time_t;

#define ODP_TIME_NULL ((odp_time_t){0, 0})

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
