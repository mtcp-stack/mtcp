/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP ticketlock
 */

#ifndef ODP_TICKETLOCK_TYPES_H_
#define ODP_TICKETLOCK_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/atomic.h>

/** @internal */
struct odp_ticketlock_s {
	odp_atomic_u32_t  next_ticket; /**< Next ticket */
	odp_atomic_u32_t  cur_ticket;  /**< Current ticket */
};

typedef struct odp_ticketlock_s odp_ticketlock_t;

#ifdef __cplusplus
}
#endif

#endif
