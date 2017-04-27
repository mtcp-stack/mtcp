/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP spinlock
 */

#ifndef ODP_API_SPINLOCK_H_
#define ODP_API_SPINLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup odp_locks
 * @details
 * <b> Spin lock (odp_spinlock_t) </b>
 *
 * Spinlock simply re-tries to acquire the lock as long as takes to succeed.
 * Spinlock is not fair since some threads may succeed more often than others.
 * @{
 */

/**
 * @typedef odp_spinlock_t
 * ODP spinlock
 */

/**
 * Initialize spin lock.
 *
 * @param splock Pointer to a spin lock
 */
void odp_spinlock_init(odp_spinlock_t *splock);


/**
 * Acquire spin lock.
 *
 * @param splock Pointer to a spin lock
 */
void odp_spinlock_lock(odp_spinlock_t *splock);


/**
 * Try to acquire spin lock.
 *
 * @param splock Pointer to a spin lock
 *
 * @retval 1 lock acquired
 * @retval 0 lock not acquired
 */
int odp_spinlock_trylock(odp_spinlock_t *splock);


/**
 * Release spin lock.
 *
 * @param splock Pointer to a spin lock
 */
void odp_spinlock_unlock(odp_spinlock_t *splock);


/**
 * Check if spin lock is busy (locked).
 *
 * @param splock Pointer to a spin lock
 *
 * @retval 1 lock busy (locked)
 * @retval 0 lock not busy.
 */
int odp_spinlock_is_locked(odp_spinlock_t *splock);



/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
