/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP queue
 */

#ifndef ODP_API_QUEUE_H_
#define ODP_API_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/schedule_types.h>
#include <odp/event.h>

/** @defgroup odp_queue ODP QUEUE
 *  Macros and operation on a queue.
 *  @{
 */

/**
 * @typedef odp_queue_t
 * ODP queue
 */

/**
 * @typedef odp_queue_group_t
 * Queue group instance type
 */

/**
 * @def ODP_QUEUE_INVALID
 * Invalid queue
 */

/**
 * @def ODP_QUEUE_NAME_LEN
 * Maximum queue name length in chars
 */

/**
 * Queue type
 */
typedef enum odp_queue_type_t {
	/** Plain queue
	  *
	  * Plain queues offer simple FIFO storage of events. Application may
	  * dequeue directly from these queues. */
	ODP_QUEUE_TYPE_PLAIN = 0,

	/** Scheduled queue
	  *
	  * Scheduled queues are connected to the scheduler. Application must
	  * not dequeue events directly from these queues but use the scheduler
	  * instead. */
	ODP_QUEUE_TYPE_SCHED,

	/** To be removed */
	ODP_QUEUE_TYPE_PKTIN,

	/** To be removed */
	ODP_QUEUE_TYPE_PKTOUT
} odp_queue_type_t;

/**
 * Queue operation mode
 */
typedef enum odp_queue_op_mode_t {
	/** Multi-thread safe operation
	  *
	  * Queue operation (enqueue or dequeue) is multi-thread safe. Any
	  * number of application threads may perform the operation
	  * concurrently. */
	ODP_QUEUE_OP_MT = 0,

	/** Not multi-thread safe operation
	  *
	  * Queue operation (enqueue or dequeue) may not be multi-thread safe.
	  * Application ensures synchronization between threads so that
	  * simultaneously only single thread attempts the operation on
	  * the same queue. */
	ODP_QUEUE_OP_MT_UNSAFE,

	/** Disabled
	  *
	  * Direct enqueue or dequeue operation from application is disabled.
	  * An attempt to enqueue/dequeue directly will result undefined
	  * behaviour. Various ODP functions (e.g. packet input, timer,
	  * crypto, scheduler, etc) are able to perform enqueue or
	  * dequeue operations normally on the queue.
	  * */
	ODP_QUEUE_OP_DISABLED

} odp_queue_op_mode_t;

/**
 * ODP Queue parameters
 */
typedef struct odp_queue_param_t {
	/** Queue type
	  *
	  * Valid values for other parameters in this structure depend on
	  * the queue type. */
	odp_queue_type_t type;

	/** Enqueue mode
	  *
	  * Default value for both queue types is ODP_QUEUE_OP_MT. Application
	  * may enable performance optimizations by defining MT_UNSAFE or
	  * DISABLED modes when applicaple. */
	odp_queue_op_mode_t enq_mode;

	/** Dequeue mode
	  *
	  * For PLAIN queues, the default value is ODP_QUEUE_OP_MT. Application
	  * may enable performance optimizations by defining MT_UNSAFE or
	  * DISABLED modes when applicaple. However, when a plain queue is input
	  * to the implementation (e.g. a queue for packet output), the
	  * parameter is ignored in queue creation and the value is
	  * ODP_QUEUE_OP_DISABLED.
	  *
	  * For SCHED queues, the parameter is ignored in queue creation and
	  * the value is ODP_QUEUE_OP_DISABLED. */
	odp_queue_op_mode_t deq_mode;

	/** Scheduler parameters
	  *
	  * These parameters are considered only when queue type is
	  * ODP_QUEUE_TYPE_SCHED. */
	odp_schedule_param_t sched;

	/** Queue context pointer
	  *
	  * User defined context pointer associated with the queue. The same
	  * pointer can be accessed with odp_queue_context() and
	  * odp_queue_context_set() calls. The implementation may read the
	  * pointer for prefetching the context data. Default value of the
	  * pointer is NULL. */
	void *context;
} odp_queue_param_t;

/**
 * Queue create
 *
 * Create a queue according to the queue parameters. Queue type is specified by
 * queue parameter 'type'. Use odp_queue_param_init() to initialize parameters
 * into their default values. Default values are also used when 'param' pointer
 * is NULL. The default queue type is ODP_QUEUE_TYPE_PLAIN.
 *
 * @param name    Queue name
 * @param param   Queue parameters. Uses defaults if NULL.
 *
 * @return Queue handle
 * @retval ODP_QUEUE_INVALID on failure
 */
odp_queue_t odp_queue_create(const char *name, const odp_queue_param_t *param);

/**
 * Destroy ODP queue
 *
 * Destroys ODP queue. The queue must be empty and detached from other
 * ODP API (crypto, pktio, etc). Application must ensure that no other
 * operations on this queue are invoked in parallel. Otherwise behavior
 * is undefined.
 *
 * @param queue    Queue handle
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_queue_destroy(odp_queue_t queue);

/**
 * Find a queue by name
 *
 * @param name    Queue name
 *
 * @return Queue handle
 * @retval ODP_QUEUE_INVALID on failure
 */
odp_queue_t odp_queue_lookup(const char *name);

/**
 * Set queue context
 *
 * It is the responsibility of the user to ensure that the queue context
 * is stored in a location accessible by all threads that attempt to
 * access it.
 *
 * @param queue    Queue handle
 * @param context  Address to the queue context
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_queue_context_set(odp_queue_t queue, void *context);

/**
 * Get queue context
 *
 * @param queue    Queue handle
 *
 * @return pointer to the queue context
 * @retval NULL on failure
 */
void *odp_queue_context(odp_queue_t queue);

/**
 * Queue enqueue
 *
 * Enqueue the 'ev' on 'queue'. On failure the event is not consumed, the caller
 * has to take care of it.
 *
 * @param queue   Queue handle
 * @param ev      Event handle
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_queue_enq(odp_queue_t queue, odp_event_t ev);

/**
 * Enqueue multiple events to a queue
 *
 * Enqueue the events from 'events[]' on 'queue'. A successful call returns the
 * actual number of events enqueued. If return value is less than 'num', the
 * remaining events at the end of events[] are not consumed, and the caller
 * has to take care of them.
 *
 * @param queue   Queue handle
 * @param[in] events Array of event handles
 * @param num     Number of event handles to enqueue
 *
 * @return Number of events actually enqueued (0 ... num)
 * @retval <0 on failure
 */
int odp_queue_enq_multi(odp_queue_t queue, const odp_event_t events[], int num);

/**
 * Queue dequeue
 *
 * Dequeues next event from head of the queue. Cannot be used for
 * ODP_QUEUE_TYPE_SCHED type queues (use odp_schedule() instead).
 *
 * @param queue   Queue handle
 *
 * @return Event handle
 * @retval ODP_EVENT_INVALID on failure (e.g. queue empty)
 */
odp_event_t odp_queue_deq(odp_queue_t queue);

/**
 * Dequeue multiple events from a queue
 *
 * Dequeues multiple events from head of the queue. Cannot be used for
 * ODP_QUEUE_TYPE_SCHED type queues (use odp_schedule() instead).
 *
 * @param queue   Queue handle
 * @param[out] events  Array of event handles for output
 * @param num     Maximum number of events to dequeue

 * @return Number of events actually dequeued (0 ... num)
 * @retval <0 on failure
 */
int odp_queue_deq_multi(odp_queue_t queue, odp_event_t events[], int num);

/**
 * Queue type
 *
 * @param queue   Queue handle
 *
 * @return Queue type
 */
odp_queue_type_t odp_queue_type(odp_queue_t queue);

/**
 * Queue schedule type
 *
 * @param queue   Queue handle
 *
 * @return Queue schedule synchronization type
 */
odp_schedule_sync_t odp_queue_sched_type(odp_queue_t queue);

/**
 * Queue priority
 *
 * @note Passing an invalid queue_handle will result in UNDEFINED behavior
 *
 * @param queue   Queue handle
 *
 * @return Queue schedule priority
 */
odp_schedule_prio_t odp_queue_sched_prio(odp_queue_t queue);

/**
 * Queue group
 *
 * @note Passing an invalid queue_handle will result in UNDEFINED behavior
 *
 * @param queue   Queue handle
 *
 * @return Queue schedule group
 */
odp_schedule_group_t odp_queue_sched_group(odp_queue_t queue);

/**
 * Queue lock count
 *
 * Return number of ordered locks associated with this ordered queue.
 * Lock count is defined in odp_schedule_param_t.
 *
 * @param queue   Queue handle
 *
 * @return Number of ordered locks associated with this ordered queue
 * @retval <0 Specified queue is not ordered
 */
int odp_queue_lock_count(odp_queue_t queue);

/**
 * Get printable value for an odp_queue_t
 *
 * @param hdl  odp_queue_t handle to be printed
 * @return     uint64_t value that can be used to print/display this
 *             handle
 *
 * @note This routine is intended to be used for diagnostic purposes
 * to enable applications to generate a printable value that represents
 * an odp_queue_t handle.
 */
uint64_t odp_queue_to_u64(odp_queue_t hdl);

/**
 * Initialize queue params
 *
 * Initialize an odp_queue_param_t to its default values for all fields
 *
 * @param param   Address of the odp_queue_param_t to be initialized
 */
void odp_queue_param_init(odp_queue_param_t *param);

/**
 * Queue information
 * Retrieve information about a queue with odp_queue_info()
 */
typedef struct odp_queue_info_t {
	const char *name;         /**< queue name */
	odp_queue_param_t param;  /**< queue parameters */
} odp_queue_info_t;

/**
 * Retrieve information about a queue
 *
 * Invalid queue handles or handles to free/destroyed queues leads to
 * undefined behaviour. Not intended for fast path use.
 *
 * @param      queue   Queue handle
 * @param[out] info    Queue info pointer for output
 *
 * @retval 0 Success
 * @retval <0 Failure.  Info could not be retrieved.
 */
int odp_queue_info(odp_queue_t queue, odp_queue_info_t *info);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
