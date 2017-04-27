/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP recursive read/write lock
 */

#ifndef ODP_API_RWLOCK_RECURSIVE_H_
#define ODP_API_RWLOCK_RECURSIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup odp_locks
 * @details
 * <b> Recursive reader/writer lock (odp_rwlock_recursive_t) </b>
 *
 * This is recursive version of the reader/writer lock.
 * A thread can read- or write-acquire a recursive read-write lock multiple
 * times without a deadlock. To release the lock, the thread must unlock it
 * the same number of times. Recursion is supported only for a pure series of
 * read or write lock calls. Read and write lock calls must not be mixed when
 * recursing.
 *
 * For example, these are supported...
 *   * read_lock(); read_lock(); read_unlock(); read_unlock();
 *   * write_lock(); write_lock(); write_unlock(); write_unlock();
 *
 * ... but this is not supported.
 *   * read_lock(); write_lock(); write_unlock(); read_unlock();
 * @{
 */

/**
 * @typedef odp_rwlock_recursive_t
 * Recursive rwlock
 */

/**
 * Initialize recursive rwlock
 *
 * @param lock    Pointer to a lock
 */
void odp_rwlock_recursive_init(odp_rwlock_recursive_t *lock);

/**
 * Acquire recursive rwlock for reading
 *
 * This call allows the thread to acquire the same lock multiple times for
 * reading. The lock cannot be acquired for writing while holding it
 * for reading.
 *
 * @param lock    Pointer to a lock
 */
void odp_rwlock_recursive_read_lock(odp_rwlock_recursive_t *lock);

/**
 * Release recursive rwlock after reading
 *
 * @param lock    Pointer to a lock
 */
void odp_rwlock_recursive_read_unlock(odp_rwlock_recursive_t *lock);

/**
 * Acquire recursive rwlock for writing
 *
 * This call allows the thread to acquire the same lock multiple times for
 * writing. The lock cannot be acquired for reading while holding it
 * for writing.
 *
 * @param lock    Pointer to a lock
 */
void odp_rwlock_recursive_write_lock(odp_rwlock_recursive_t *lock);

/**
 * Release recursive rwlock after writing
 *
 * @param lock    Pointer to a lock
 */
void odp_rwlock_recursive_write_unlock(odp_rwlock_recursive_t *lock);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
