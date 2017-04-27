/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP schedule
 */

#ifndef ODP_API_SCHEDULE_H_
#define ODP_API_SCHEDULE_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <odp/std_types.h>
#include <odp/event.h>
#include <odp/queue.h>
#include <odp/schedule_types.h>
#include <odp/thrmask.h>

/** @defgroup odp_scheduler ODP SCHEDULER
 *  Operations on the scheduler.
 *  @{
 */

/**
 * @def ODP_SCHED_WAIT
 * Wait infinitely
 */

/**
 * @def ODP_SCHED_NO_WAIT
 * Do not wait
 */

/**
 * @def ODP_SCHED_GROUP_NAME_LEN
 * Maximum schedule group name length in chars
 */

/**
 * @def ODP_SCHED_GROUP_INVALID
 * Invalid scheduler group
 */

/**
 * @def ODP_SCHED_GROUP_ALL
 * Predefined scheduler group of all threads
 */

/**
 * @def ODP_SCHED_GROUP_WORKER
 * Predefined scheduler group of all worker threads
 */

/**
 * @def ODP_SCHED_GROUP_CONTROL
 * Predefined scheduler group of all control threads
 */

/**
 * Schedule wait time
 *
 * Converts nanoseconds to wait values for other schedule functions.
 *
 * @param ns Nanoseconds
 *
 * @return Value for the wait parameter in schedule functions
 */
uint64_t odp_schedule_wait_time(uint64_t ns);

/**
 * Schedule
 *
 * Schedules all queues created with ODP_QUEUE_TYPE_SCHED type. Returns
 * next highest priority event which is available for the calling thread.
 * Outputs the source queue of the event. If there's no event available, waits
 * for an event according to the wait parameter setting. Returns
 * ODP_EVENT_INVALID if reaches end of the wait period.
 *
 * When returns an event, the thread holds the queue synchronization context
 * (atomic or ordered) until the next odp_schedule() or odp_schedule_multi()
 * call. The next call implicitly releases the current context and potentially
 * returns with a new context. User can allow early context release (e.g., see
 * odp_schedule_release_atomic() and odp_schedule_release_ordered()) for
 * performance optimization.
 *
 * @param from    Output parameter for the source queue (where the event was
 *                dequeued from). Ignored if NULL.
 * @param wait    Minimum time to wait for an event. Waits indefinitely if set
 *                to ODP_SCHED_WAIT. Does not wait if set to ODP_SCHED_NO_WAIT.
 *                Use odp_schedule_wait_time() to convert time to other wait
 *                values.
 *
 * @return Next highest priority event
 * @retval ODP_EVENT_INVALID on timeout and no events available
 *
 * @see odp_schedule_multi(), odp_schedule_release_atomic(),
 * odp_schedule_release_ordered()
 */
odp_event_t odp_schedule(odp_queue_t *from, uint64_t wait);

/**
 * Schedule multiple events
 *
 * Like odp_schedule(), but returns multiple events from a queue. The caller
 * specifies the maximum number of events it is willing to accept. The
 * scheduler is under no obligation to return more than a single event but
 * will never return more than the number specified by the caller. The return
 * code specifies the number of events returned and all of these events always
 * originate from the same source queue and share the same scheduler
 * synchronization context.
 *
 * @param from    Output parameter for the source queue (where the event was
 *                dequeued from). Ignored if NULL.
 * @param wait    Minimum time to wait for an event. Waits infinitely, if set to
 *                ODP_SCHED_WAIT. Does not wait, if set to ODP_SCHED_NO_WAIT.
 *                Use odp_schedule_wait_time() to convert time to other wait
 *                values.
 * @param events  Event array for output
 * @param num     Maximum number of events to output
 *
 * @return Number of events outputed (0 ... num)
 */
int odp_schedule_multi(odp_queue_t *from, uint64_t wait, odp_event_t events[],
		       int num);

/**
 * Pause scheduling
 *
 * Pause global scheduling for this thread. After this call, all schedule calls
 * will return only locally pre-scheduled events (if any). User can exit the
 * schedule loop only after the schedule function indicates that there's no more
 * (pre-scheduled) events.
 *
 * Must be used with odp_schedule() and odp_schedule_multi() before exiting (or
 * stalling) the schedule loop.
 */
void odp_schedule_pause(void);

/**
 * Resume scheduling
 *
 * Resume global scheduling for this thread. After this call, all schedule calls
 * will schedule normally (perform global scheduling).
 */
void odp_schedule_resume(void);

/**
 * Release the current atomic context
 *
 * This call is valid only for source queues with atomic synchronization. It
 * hints the scheduler that the user has completed critical section processing
 * in the current atomic context. The scheduler is now allowed to schedule
 * events from the same queue to another thread. However, the context may be
 * still held until the next odp_schedule() or odp_schedule_multi() call - this
 * call allows but does not force the scheduler to release the context early.
 *
 * Early atomic context release may increase parallelism and thus system
 * performance, but user needs to design carefully the split into critical vs.
 * non-critical sections.
 */
void odp_schedule_release_atomic(void);

/**
 * Release the current ordered context
 *
 * This call is valid only for source queues with ordered synchronization. It
 * hints the scheduler that the user has done all enqueues that need to maintain
 * event order in the current ordered context. The scheduler is allowed to
 * release the ordered context of this thread and avoid reordering any following
 * enqueues. However, the context may be still held until the next
 * odp_schedule() or odp_schedule_multi() call - this call allows but does not
 * force the scheduler to release the context early.
 *
 * Early ordered context release may increase parallelism and thus system
 * performance, since scheduler may start reordering events sooner than the next
 * schedule call.
 */
void odp_schedule_release_ordered(void);

/**
 * Prefetch events for next schedule call
 *
 * Hint the scheduler that application is about to finish processing the current
 * event(s) and will soon request more events. The scheduling context status is
 * not affect. The call does not guarantee that the next schedule call will
 * return any number of events. It may improve system performance, since the
 * scheduler may prefetch the next (batch of) event(s) in parallel to
 * application processing the current event(s).
 *
 * @param num     Number of events to prefetch
 */
void odp_schedule_prefetch(int num);

/**
 * Number of scheduling priorities
 *
 * @return Number of scheduling priorities
 */
int odp_schedule_num_prio(void);

/**
 * Schedule group create
 *
 * Creates a schedule group with the thread mask. Only threads in the
 * mask will receive events from a queue that belongs to the schedule group.
 * Thread masks of various schedule groups may overlap. There are predefined
 * groups such as ODP_SCHED_GROUP_ALL and ODP_SCHED_GROUP_WORKER, which are
 * always present and automatically updated. Group name is optional
 * (may be NULL) and can have ODP_SCHED_GROUP_NAME_LEN characters in maximum.
 *
 * @param name    Schedule group name
 * @param mask    Thread mask
 *
 * @return Schedule group handle
 * @retval ODP_SCHED_GROUP_INVALID on failure
 *
 * @see ODP_SCHED_GROUP_ALL, ODP_SCHED_GROUP_WORKER
 */
odp_schedule_group_t odp_schedule_group_create(const char *name,
					       const odp_thrmask_t *mask);

/**
 * Schedule group destroy
 *
 * Destroys a schedule group. All queues belonging to the schedule group must
 * be destroyed before destroying the group. Other operations on this group
 * must not be invoked in parallel.
 *
 * @param group   Schedule group handle
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_schedule_group_destroy(odp_schedule_group_t group);

/**
 * Look up a schedule group by name
 *
 * Return the handle of a schedule group from its name
 *
 * @param name   Name of schedule group
 *
 * @return Handle of schedule group for specified name
 * @retval ODP_SCHEDULE_GROUP_INVALID No matching schedule group found
 */
odp_schedule_group_t odp_schedule_group_lookup(const char *name);

/**
 * Join a schedule group
 *
 * Join a threadmask to an existing schedule group
 *
 * @param group  Schdule group handle
 * @param mask   Thread mask
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_schedule_group_join(odp_schedule_group_t group,
			    const odp_thrmask_t *mask);

/**
 * Leave a schedule group
 *
 * Remove a threadmask from an existing schedule group
 *
 * @param group  Schedule group handle
 * @param mask   Thread mask
 *
 * @retval 0 on success
 * @retval <0 on failure
 *
 * @note Leaving a schedule group means threads in the specified mask will no
 * longer receive events from queues belonging to the specified schedule
 * group. This effect is not instantaneous, however, and events that have been
 * prestaged may still be presented to the masked threads.
 */
int odp_schedule_group_leave(odp_schedule_group_t group,
			     const odp_thrmask_t *mask);

/**
 * Get a schedule group's thrmask
 *
 * @param      group   Schedule group handle
 * @param[out] thrmask The current thrmask used for this schedule group
 *
 * @retval 0  On success
 * @retval <0 Invalid group specified
 */
int odp_schedule_group_thrmask(odp_schedule_group_t group,
			       odp_thrmask_t *thrmask);

/**
 * Acquire ordered context lock
 *
 * This call is valid only when holding an ordered synchronization context.
 * Ordered locks are used to protect critical sections that are executed
 * within an ordered context. Threads enter the critical section in the order
 * determined by the context (source queue). Lock ordering is automatically
 * skipped for threads that release the context instead of using the lock.
 *
 * The number of ordered locks available is set by the lock_count parameter of
 * the schedule parameters passed to odp_queue_create(), which must be less
 * than or equal to the ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE configuration
 * option. If this routine is called outside of an ordered context or with a
 * lock_index that exceeds the number of available ordered locks in this
 * context results are undefined. The number of ordered locks associated with
 * a given ordered queue may be queried by the odp_queue_lock_count() API.
 *
 * Each ordered lock may be used only once per ordered context. If events
 * are to be processed with multiple ordered critical sections, each should
 * be protected by its own ordered lock. This promotes maximum parallelism by
 * allowing order to maintained on a more granular basis. If an ordered lock
 * is used multiple times in the same ordered context results are undefined.
 *
 * @param lock_index Index of the ordered lock in the current context to be
 *                   acquired. Must be in the range 0..odp_queue_lock_count()
 *                   - 1
 */
void odp_schedule_order_lock(unsigned lock_index);

/**
 * Release ordered context lock
 *
 * This call is valid only when holding an ordered synchronization context.
 * Release a previously locked ordered context lock.
 *
 * @param lock_index Index of the ordered lock in the current context to be
 *                   released. Results are undefined if the caller does not
 *                   hold this lock. Must be in the range
 *                   0..odp_queue_lock_count() - 1
 */
void odp_schedule_order_unlock(unsigned lock_index);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
