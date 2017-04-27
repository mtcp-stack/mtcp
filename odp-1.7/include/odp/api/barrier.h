/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP execution barriers
 */

#ifndef ODP_API_BARRIER_H_
#define ODP_API_BARRIER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odp_barrier ODP BARRIER
 * Thread excution and memory ordering barriers.
 *
 * @details
 * <b> Thread execution barrier (odp_barrier_t) </b>
 *
 * Thread execution barrier synchronizes a group of threads to wait on the
 * barrier until the entire group has reached the barrier.
 *  @{
 */

/**
 * @typedef odp_barrier_t
 * ODP thread synchronization barrier
 */

/**
 * Initialize barrier with thread count.
 *
 * @param barr Pointer to a barrier variable
 * @param count Thread count
 */
void odp_barrier_init(odp_barrier_t *barr, int count);


/**
 * Synchronize thread execution on barrier.
 * Wait for all threads to arrive at the barrier until they are let loose again.
 * Threads will block (spin) until the last thread has arrived at the barrier.
 * All memory operations before the odp_barrier_wait() call will be visible
 * to all threads when they leave the barrier.
 *
 * @param barr Pointer to a barrier variable
 */
void odp_barrier_wait(odp_barrier_t *barr);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
