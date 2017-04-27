/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_API_RWLOCK_H_
#define ODP_API_RWLOCK_H_

/**
 * @file
 *
 * ODP RW Locks
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odp_locks ODP LOCKS
 * @details
 * <b> Reader / writer lock (odp_rwlock_t) </b>
 *
 * A reader/writer lock allows multiple simultaneous readers but only one
 * writer at a time. A thread that wants write access will have to wait until
 * there are no threads that want read access. This casues a risk for
 * starvation.
 * @{
 */

/**
 * @typedef odp_rwlock_t
 * ODP reader/writer lock
 */


/**
 * Initialize a reader/writer lock.
 *
 * @param rwlock Pointer to a reader/writer lock
 */
void odp_rwlock_init(odp_rwlock_t *rwlock);

/**
 * Acquire read permission on a reader/writer lock.
 *
 * @param rwlock Pointer to a reader/writer lock
 */
void odp_rwlock_read_lock(odp_rwlock_t *rwlock);

/**
 * Release read permission on a reader/writer lock.
 *
 * @param rwlock Pointer to a reader/writer lock
 */
void odp_rwlock_read_unlock(odp_rwlock_t *rwlock);

/**
 * Acquire write permission on a reader/writer lock.
 *
 * @param rwlock Pointer to a reader/writer lock
 */
void odp_rwlock_write_lock(odp_rwlock_t *rwlock);

/**
 * Release write permission on a reader/writer lock.
 *
 * @param rwlock Pointer to a reader/writer lock
 */
void odp_rwlock_write_unlock(odp_rwlock_t *rwlock);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ODP_RWLOCK_H_ */
