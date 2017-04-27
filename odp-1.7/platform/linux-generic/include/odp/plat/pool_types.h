/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP pool
 */

#ifndef ODP_POOL_TYPES_H_
#define ODP_POOL_TYPES_H_

#include <odp/std_types.h>
#include <odp/plat/strong_types.h>
#include <odp/plat/event_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_buffer
 *  Operations on a pool.
 *  @{
 */

typedef ODP_HANDLE_T(odp_pool_t);

#define ODP_POOL_INVALID _odp_cast_scalar(odp_pool_t, 0xffffffff)

/**
 * Pool type
 */
typedef enum odp_pool_type_t {
	ODP_POOL_BUFFER  = ODP_EVENT_BUFFER,
	ODP_POOL_PACKET  = ODP_EVENT_PACKET,
	ODP_POOL_TIMEOUT = ODP_EVENT_TIMEOUT,
} odp_pool_type_t;

/** Get printable format of odp_pool_t */
static inline uint64_t odp_pool_to_u64(odp_pool_t hdl)
{
	return _odp_pri(hdl);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
