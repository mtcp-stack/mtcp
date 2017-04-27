/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP thread API
 */

#ifndef ODP_API_THREAD_H_
#define ODP_API_THREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup odp_thread ODP THREAD
 *  @{
 */

/**
 * @def ODP_THREAD_COUNT_MAX
 * Maximum number of threads supported in build time. Use
 * odp_thread_count_max() for maximum number of threads supported in run time,
 * which depend on system configuration and may be lower than this number.
 */

/**
 * Thread type
 */
typedef enum odp_thread_type_e {
	/**
	 * Worker thread
	 *
	 * Worker threads do most part of ODP application packet processing.
	 * These threads provide high packet and data rates, with low and
	 * predictable latency. Typically, worker threads are pinned to isolated
	 * CPUs and packets are processed in a run-to-completion loop with very
	 * low interference from the operating system.
	 */
	ODP_THREAD_WORKER = 0,

	/**
	 * Control thread
	 *
	 * Control threads do not participate the main packet flow through the
	 * system, but e.g. control or monitor the worker threads, or handle
	 * exceptions. These threads may perform general purpose processing,
	 * use system calls, share the CPU with other threads and be interrupt
	 * driven.
	 */
	ODP_THREAD_CONTROL
} odp_thread_type_t;


/**
 * Get thread identifier
 *
 * Returns the thread identifier of the current thread. Thread ids range from 0
 * to odp_thread_count_max() - 1. The ODP thread id is assigned by
 * odp_init_local() and freed by odp_term_local(). Thread id is unique within
 * the ODP instance.
 *
 * @return Thread identifier of the current thread
 */
int odp_thread_id(void);

/**
 * Thread count
 *
 * Returns the current ODP thread count. This is the number of active threads
 * running the ODP instance. Each odp_init_local() call increments and each
 * odp_term_local() call decrements the count. The count is always between 1 and
 * odp_thread_count_max().
 *
 * @return Current thread count
 */
int odp_thread_count(void);

/**
 * Maximum thread count
 *
 * Returns the maximum thread count, which is a constant value and set in
 * ODP initialization phase. This may be lower than ODP_THREAD_COUNT_MAX.
 *
 * @return Maximum thread count
 */
int odp_thread_count_max(void);

/**
 * Thread type
 *
 * Returns the thread type of the current thread.
 *
 * @return Thread type
 */
odp_thread_type_t odp_thread_type(void);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
