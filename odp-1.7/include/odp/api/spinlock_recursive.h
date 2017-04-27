/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP recursive spinlock
 */

#ifndef ODP_API_SPINLOCK_RECURSIVE_H_
#define ODP_API_SPINLOCK_RECURSIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup odp_locks
 * @details
 * <b> Recursive spin lock (odp_spinlock_recursive_t) </b>
 *
 * This is recursive version of the spin lock. A thread can acquire the lock
 * multiple times without a deadlock. To release the lock, the thread must
 * unlock it the same number of times.
 * @{
 */

/**
 * @typedef odp_spinlock_recursive_t
 * Recursive spinlock
 */

/**
 * Initialize recursive spinlock.
 *
 * @param lock    Pointer to a lock
 */
void odp_spinlock_recursive_init(odp_spinlock_recursive_t *lock);

/**
 * Acquire recursive spinlock.
 *
 * @param lock    Pointer to a lock
 */
void odp_spinlock_recursive_lock(odp_spinlock_recursive_t *lock);

/**
 * Try to acquire recursive spinlock.
 *
 * @param lock    Pointer to a lock
 *
 * @retval 1 lock acquired
 * @retval 0 lock not acquired
 */
int odp_spinlock_recursive_trylock(odp_spinlock_recursive_t *lock);

/**
 * Release recursive spinlock.
 *
 * @param lock    Pointer to a lock
 */
void odp_spinlock_recursive_unlock(odp_spinlock_recursive_t *lock);

/**
 * Check if recursive spinlock is locked.
 *
 * @param lock    Pointer to a lock
 *
 * @retval 1 lock is locked
 * @retval 0 lock is not locked
 */
int odp_spinlock_recursive_is_locked(odp_spinlock_recursive_t *lock);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
