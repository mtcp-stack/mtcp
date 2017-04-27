/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP timeout descriptor - implementation internal
 */

#ifndef ODP_TIMER_INTERNAL_H_
#define ODP_TIMER_INTERNAL_H_

#include <odp/align.h>
#include <odp/debug.h>
#include <odp_buffer_internal.h>
#include <odp_pool_internal.h>
#include <odp/timer.h>

/**
 * Internal Timeout header
 */
typedef struct {
	/* common buffer header */
	odp_buffer_hdr_t buf_hdr;

	/* Requested expiration time */
	uint64_t expiration;
	/* User ptr inherited from parent timer */
	void *user_ptr;
	/* Parent timer */
	odp_timer_t timer;
} odp_timeout_hdr_t;

typedef struct odp_timeout_hdr_stride {
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(odp_timeout_hdr_t))];
} odp_timeout_hdr_stride;

#endif
