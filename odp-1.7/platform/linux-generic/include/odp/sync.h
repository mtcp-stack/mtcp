/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP synchronisation
 */

#ifndef ODP_PLAT_SYNC_H_
#define ODP_PLAT_SYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup odp_barrier
 *  @{
 */

static inline void odp_mb_release(void)
{
	__atomic_thread_fence(__ATOMIC_RELEASE);
}

static inline void odp_mb_acquire(void)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
}

static inline void odp_mb_full(void)
{
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
}

/**
 * @}
 */

#include <odp/api/sync.h>

#ifdef __cplusplus
}
#endif

#endif
