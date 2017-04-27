/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP event
 */

#ifndef ODP_EVENT_TYPES_H_
#define ODP_EVENT_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/plat/strong_types.h>

/** @defgroup odp_event ODP EVENT
 *  Operations on an event.
 *  @{
 */

typedef ODP_HANDLE_T(odp_event_t);

#define ODP_EVENT_INVALID _odp_cast_scalar(odp_event_t, 0)

/**
 * Event types
 */
typedef enum odp_event_type_t {
	ODP_EVENT_BUFFER       = 1,
	ODP_EVENT_PACKET       = 2,
	ODP_EVENT_TIMEOUT      = 3,
	ODP_EVENT_CRYPTO_COMPL = 4,
} odp_event_type_t;

/** Get printable format of odp_event_t */
static inline uint64_t odp_event_to_u64(odp_event_t hdl)
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
