/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP schedule types
 */

#ifndef ODP_API_SCHEDULE_TYPES_H_
#define ODP_API_SCHEDULE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_scheduler
 *  @{
 */

/**
 * @typedef odp_schedule_prio_t
 * Scheduler priority level
 */

/**
 * @def ODP_SCHED_PRIO_HIGHEST
 * Highest scheduling priority
 */

/**
 * @def ODP_SCHED_PRIO_NORMAL
 * Normal scheduling priority
 */

/**
 * @def ODP_SCHED_PRIO_LOWEST
 * Lowest scheduling priority
 */

/**
 * @def ODP_SCHED_PRIO_DEFAULT
 * Default scheduling priority. User does not care about the selected priority
 * level - throughput, load balacing and synchronization features are more
 * important than priority scheduling.
 */

/**
 * @typedef odp_schedule_sync_t
 * Scheduler synchronization method
 */

/**
 * @def ODP_SCHED_SYNC_PARALLEL
 * Parallel scheduled queues
 *
 * The scheduler performs priority scheduling, load balancing, pre-fetching, etc
 * functions but does not provide additional event synchronization or ordering.
 * It's free to schedule events from single parallel queue to multiple threads
 * for concurrent processing. Application is responsible for queue context
 * synchronization and event ordering (SW synchronization).
 */

/**
 * @def ODP_SCHED_SYNC_ATOMIC
 * Atomic queue synchronization
 *
 * Events from an atomic queue can be scheduled only to a single thread at a
 * time. The thread is guaranteed to have exclusive (atomic) access to the
 * associated queue context, which enables the user to avoid SW synchronization.
 * Atomic queue also helps to maintain event ordering since only one thread at
 * a time is able to process events from a queue.
 *
 * The atomic queue synchronization context is dedicated to the thread until it
 * requests another event from the scheduler, which implicitly releases the
 * context. User may allow the scheduler to release the context earlier than
 * that by calling odp_schedule_release_atomic().
 */

/**
 * @def ODP_SCHED_SYNC_ORDERED
 * Ordered queue synchronization
 *
 * Events from an ordered queue can be scheduled to multiple threads for
 * concurrent processing but still maintain the original event order. This
 * enables the user to achieve high single flow throughput by avoiding
 * SW synchronization for ordering between threads.
 *
 * The source queue (dequeue) ordering is maintained when
 * events are enqueued to their destination queue(s) within the same ordered
 * queue synchronization context. A thread holds the context until it
 * requests another event from the scheduler, which implicitly releases the
 * context. User may allow the scheduler to release the context earlier than
 * that by calling odp_schedule_release_ordered().
 *
 * Events from the same (source) queue appear in their original order
 * when dequeued from a destination queue. The destination queue can have any
 * queue type and synchronization method. Event ordering is based on the
 * received event(s), but also other (newly allocated or stored) events are
 * ordered when enqueued within the same ordered context. Events not enqueued
 * (e.g. freed or stored) within the context are considered missing from
 * reordering and are skipped at this time (but can be ordered again within
 * another context).
 */

/**
 * @typedef odp_schedule_group_t
 * Scheduler thread group
 */

/**
 * @def ODP_SCHED_GROUP_ALL
 * Group of all threads. All active worker and control threads belong to this
 * group. The group is automatically updated when new threads enter or old
 * threads exit ODP.
 */

/**
 * @def ODP_SCHED_GROUP_WORKER
 * Group of all worker threads. All active worker threads belong to this
 * group. The group is automatically updated when new worker threads enter or
 * old threads exit ODP.
 */

/** Scheduler parameters */
typedef	struct odp_schedule_param_t {
	/** Priority level */
	odp_schedule_prio_t  prio;
	/** Synchronization method */
	odp_schedule_sync_t  sync;
	/** Thread group */
	odp_schedule_group_t group;
	/** Ordered lock count for this queue */
	unsigned lock_count;
} odp_schedule_param_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
