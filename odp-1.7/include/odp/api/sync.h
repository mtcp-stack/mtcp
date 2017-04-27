/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP memory barriers
 */

#ifndef ODP_API_SYNC_H_
#define ODP_API_SYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup odp_barrier
 * @details
 * <b> Memory barriers </b>
 *
 * Memory barriers enforce ordering of memory load and store operations
 * specified before and after the barrier. These barriers may affect both
 * compiler optimizations and CPU out-of-order execution. All ODP
 * synchronization mechanisms (e.g. execution barriers, locks, queues, etc )
 * include all necessary memory barriers, so these calls are not needed when
 * using those. Also ODP atomic operations have memory ordered versions. These
 * explicit barriers may be needed when thread synchronization is based on
 * a non-ODP defined mechanism. Depending on the HW platform, heavy usage of
 * memory barriers may cause significant performance degradation.
 *
 *  @{
 */

/**
 * Memory barrier for release operations
 *
 * This memory barrier has release semantics. It synchronizes with a pairing
 * barrier for acquire operations. The releasing and acquiring threads
 * synchronize through shared memory. The releasing thread must call this
 * barrier before signaling the acquiring thread. After the acquiring thread
 * receives the signal, it must call odp_mb_acquire() before it reads the
 * memory written by the releasing thread.
 *
 * This call is not needed when using ODP defined synchronization mechanisms.
 *
 * @see odp_mb_acquire()
 */
void odp_mb_release(void);

/**
 * Memory barrier for acquire operations
 *
 * This memory barrier has acquire semantics. It synchronizes with a pairing
 * barrier for release operations. The releasing and acquiring threads
 * synchronize through shared memory. The releasing thread must call
 * odp_mb_release() before signaling the acquiring thread. After the acquiring
 * thread receives the signal, it must call this barrier before it reads the
 * memory written by the releasing thread.
 *
 * This call is not needed when using ODP defined synchronization mechanisms.
 *
 * @see odp_mb_release()
 */
void odp_mb_acquire(void);

/**
 * Full memory barrier
 *
 * This is a full memory barrier. It guarantees that all load and store
 * operations specified before it are visible to other threads before
 * all load and store operations specified after it.
 *
 * This call is not needed when using ODP defined synchronization mechanisms.
 */
void odp_mb_full(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
