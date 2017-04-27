/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP queue - implementation internal
 */

#ifndef ODP_QUEUE_INTERNAL_H_
#define ODP_QUEUE_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/queue.h>
#include <odp_forward_typedefs_internal.h>
#include <odp_buffer_internal.h>
#include <odp_align_internal.h>
#include <odp/packet_io.h>
#include <odp/align.h>
#include <odp/hints.h>


#define USE_TICKETLOCK

#ifdef USE_TICKETLOCK
#include <odp/ticketlock.h>
#else
#include <odp/spinlock.h>
#endif

#define QUEUE_MULTI_MAX 8

#define QUEUE_STATUS_FREE         0
#define QUEUE_STATUS_DESTROYED    1
#define QUEUE_STATUS_READY        2
#define QUEUE_STATUS_NOTSCHED     3
#define QUEUE_STATUS_SCHED        4


/* forward declaration */
union queue_entry_u;

typedef int (*enq_func_t)(union queue_entry_u *, odp_buffer_hdr_t *, int);
typedef	odp_buffer_hdr_t *(*deq_func_t)(union queue_entry_u *);

typedef int (*enq_multi_func_t)(union queue_entry_u *,
				odp_buffer_hdr_t **, int, int);
typedef	int (*deq_multi_func_t)(union queue_entry_u *,
				odp_buffer_hdr_t **, int);

struct queue_entry_s {
#ifdef USE_TICKETLOCK
	odp_ticketlock_t  lock ODP_ALIGNED_CACHE;
#else
	odp_spinlock_t    lock ODP_ALIGNED_CACHE;
#endif

	odp_buffer_hdr_t *head;
	odp_buffer_hdr_t *tail;
	int               status;

	enq_func_t       enqueue ODP_ALIGNED_CACHE;
	deq_func_t       dequeue;
	enq_multi_func_t enqueue_multi;
	deq_multi_func_t dequeue_multi;

	odp_queue_t       handle;
	odp_queue_t       pri_queue;
	odp_event_t       cmd_ev;
	odp_queue_type_t  type;
	odp_queue_param_t param;
	odp_pktin_queue_t pktin;
	odp_pktio_t       pktout;
	char              name[ODP_QUEUE_NAME_LEN];
	uint64_t          order_in;
	uint64_t          order_out;
	odp_buffer_hdr_t *reorder_head;
	odp_buffer_hdr_t *reorder_tail;
	odp_atomic_u64_t  sync_in[ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE];
	odp_atomic_u64_t  sync_out[ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE];
};

union queue_entry_u {
	struct queue_entry_s s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct queue_entry_s))];
};


queue_entry_t *get_qentry(uint32_t queue_id);

int queue_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr, int sustain);
int ordered_queue_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr,
		      int systain, queue_entry_t *origin_qe, uint64_t order);
odp_buffer_hdr_t *queue_deq(queue_entry_t *queue);

int queue_enq_internal(odp_buffer_hdr_t *buf_hdr);

int queue_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num,
		    int sustain);
int queue_deq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num);

int queue_pktout_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr,
		     int sustain);
int queue_pktout_enq_multi(queue_entry_t *queue,
			   odp_buffer_hdr_t *buf_hdr[], int num, int sustain);

void queue_lock(queue_entry_t *queue);
void queue_unlock(queue_entry_t *queue);

int queue_sched_atomic(odp_queue_t handle);

int release_order(queue_entry_t *origin_qe, uint64_t order,
		  odp_pool_t pool, int enq_called);
void get_sched_order(queue_entry_t **origin_qe, uint64_t *order);
void get_sched_sync(queue_entry_t **origin_qe, uint64_t **sync, uint32_t ndx);
void sched_enq_called(void);
void sched_order_resolved(odp_buffer_hdr_t *buf_hdr);

static inline uint32_t queue_to_id(odp_queue_t handle)
{
	return _odp_typeval(handle) - 1;
}

static inline odp_queue_t queue_from_id(uint32_t queue_id)
{
	return _odp_cast_scalar(odp_queue_t, queue_id + 1);
}

static inline queue_entry_t *queue_to_qentry(odp_queue_t handle)
{
	uint32_t queue_id;

	queue_id = queue_to_id(handle);
	return get_qentry(queue_id);
}

static inline int queue_is_atomic(queue_entry_t *qe)
{
	return qe->s.param.sched.sync == ODP_SCHED_SYNC_ATOMIC;
}

static inline int queue_is_ordered(queue_entry_t *qe)
{
	return qe->s.param.sched.sync == ODP_SCHED_SYNC_ORDERED;
}

static inline odp_queue_t queue_handle(queue_entry_t *qe)
{
	return qe->s.handle;
}

static inline int queue_prio(queue_entry_t *qe)
{
	return qe->s.param.sched.prio;
}

static inline odp_buffer_hdr_t *get_buf_tail(odp_buffer_hdr_t *buf_hdr)
{
	odp_buffer_hdr_t *buf_tail = buf_hdr->link ? buf_hdr->link : buf_hdr;

	buf_hdr->next = buf_hdr->link;
	buf_hdr->link = NULL;

	while (buf_tail->next)
		buf_tail = buf_tail->next;

	return buf_tail;
}

static inline void queue_add(queue_entry_t *queue,
			     odp_buffer_hdr_t *buf_hdr)
{
	buf_hdr->next = NULL;

	if (queue->s.head)
		queue->s.tail->next = buf_hdr;
	else
		queue->s.head = buf_hdr;

	queue->s.tail = buf_hdr;
}

static inline void queue_add_list(queue_entry_t *queue,
				  odp_buffer_hdr_t *buf_head,
				  odp_buffer_hdr_t *buf_tail)
{
	if (queue->s.head)
		queue->s.tail->next = buf_head;
	else
		queue->s.head = buf_head;

	queue->s.tail = buf_tail;
}

static inline void queue_add_chain(queue_entry_t *queue,
				   odp_buffer_hdr_t *buf_hdr)
{
	queue_add_list(queue, buf_hdr, get_buf_tail(buf_hdr));
}

static inline void reorder_enq(queue_entry_t *queue,
			       uint64_t order,
			       queue_entry_t *origin_qe,
			       odp_buffer_hdr_t *buf_hdr,
			       int sustain)
{
	odp_buffer_hdr_t *reorder_buf = origin_qe->s.reorder_head;
	odp_buffer_hdr_t *reorder_prev = NULL;

	while (reorder_buf && order >= reorder_buf->order) {
		reorder_prev = reorder_buf;
		reorder_buf  = reorder_buf->next;
	}

	buf_hdr->next = reorder_buf;

	if (reorder_prev)
		reorder_prev->next = buf_hdr;
	else
		origin_qe->s.reorder_head = buf_hdr;


	if (!reorder_buf)
		origin_qe->s.reorder_tail = buf_hdr;

	buf_hdr->origin_qe     = origin_qe;
	buf_hdr->target_qe     = queue;
	buf_hdr->order         = order;
	buf_hdr->flags.sustain = sustain;
}

static inline void order_release(queue_entry_t *origin_qe, int count)
{
	uint64_t sync;
	uint32_t i;

	origin_qe->s.order_out += count;

	for (i = 0; i < origin_qe->s.param.sched.lock_count; i++) {
		sync = odp_atomic_load_u64(&origin_qe->s.sync_out[i]);
		if (sync < origin_qe->s.order_out)
			odp_atomic_fetch_add_u64(&origin_qe->s.sync_out[i],
						 origin_qe->s.order_out - sync);
	}
}

static inline int reorder_deq(queue_entry_t *queue,
			      queue_entry_t *origin_qe,
			      odp_buffer_hdr_t **reorder_tail_return,
			      odp_buffer_hdr_t **placeholder_buf_return,
			      int *release_count_return,
			      int *placeholder_count_return)
{
	odp_buffer_hdr_t *reorder_buf     = origin_qe->s.reorder_head;
	odp_buffer_hdr_t *reorder_tail    = NULL;
	odp_buffer_hdr_t *placeholder_buf = NULL;
	odp_buffer_hdr_t *next_buf;
	int               deq_count = 0;
	int               release_count = 0;
	int               placeholder_count = 0;

	while (reorder_buf &&
	       reorder_buf->order <= origin_qe->s.order_out +
	       release_count + placeholder_count) {
		/*
		 * Elements on the reorder list fall into one of
		 * three categories:
		 *
		 * 1. Those destined for the same queue.  These
		 *    can be enq'd now if they were waiting to
		 *    be unblocked by this enq.
		 *
		 * 2. Those representing placeholders for events
		 *    whose ordering was released by a prior
		 *    odp_schedule_release_ordered() call.  These
		 *    can now just be freed.
		 *
		 * 3. Those representing events destined for another
		 *    queue. These cannot be consolidated with this
		 *    enq since they have a different target.
		 *
		 * Detecting an element with an order sequence gap, an
		 * element in category 3, or running out of elements
		 * stops the scan.
		 */
		next_buf = reorder_buf->next;

		if (odp_likely(reorder_buf->target_qe == queue)) {
			/* promote any chain */
			odp_buffer_hdr_t *reorder_link =
				reorder_buf->link;

			if (reorder_link) {
				reorder_buf->next = reorder_link;
				reorder_buf->link = NULL;
				while (reorder_link->next)
					reorder_link = reorder_link->next;
				reorder_link->next = next_buf;
				reorder_tail = reorder_link;
			} else {
				reorder_tail = reorder_buf;
			}

			deq_count++;
			if (!reorder_buf->flags.sustain)
				release_count++;
			reorder_buf = next_buf;
		} else if (!reorder_buf->target_qe) {
			if (reorder_tail)
				reorder_tail->next = next_buf;
			else
				origin_qe->s.reorder_head = next_buf;

			reorder_buf->next = placeholder_buf;
			placeholder_buf = reorder_buf;

			reorder_buf = next_buf;
			placeholder_count++;
		} else {
			break;
		}
	}

	*reorder_tail_return = reorder_tail;
	*placeholder_buf_return = placeholder_buf;
	*release_count_return = release_count;
	*placeholder_count_return = placeholder_count;

	return deq_count;
}

static inline void reorder_complete(queue_entry_t *origin_qe,
				    odp_buffer_hdr_t **reorder_buf_return,
				    odp_buffer_hdr_t **placeholder_buf,
				    int placeholder_append)
{
	odp_buffer_hdr_t *reorder_buf = origin_qe->s.reorder_head;
	odp_buffer_hdr_t *next_buf;

	*reorder_buf_return = NULL;
	if (!placeholder_append)
		*placeholder_buf = NULL;

	while (reorder_buf &&
	       reorder_buf->order <= origin_qe->s.order_out) {
		next_buf = reorder_buf->next;

		if (!reorder_buf->target_qe) {
			origin_qe->s.reorder_head = next_buf;
			reorder_buf->next         = *placeholder_buf;
			*placeholder_buf          = reorder_buf;

			reorder_buf = next_buf;
			order_release(origin_qe, 1);
		} else if (reorder_buf->flags.sustain) {
			reorder_buf = next_buf;
		} else {
			*reorder_buf_return = origin_qe->s.reorder_head;
			origin_qe->s.reorder_head =
				origin_qe->s.reorder_head->next;
			break;
		}
	}
}

static inline void get_queue_order(queue_entry_t **origin_qe, uint64_t *order,
				   odp_buffer_hdr_t *buf_hdr)
{
	if (buf_hdr && buf_hdr->origin_qe) {
		*origin_qe = buf_hdr->origin_qe;
		*order     = buf_hdr->order;
	} else {
		get_sched_order(origin_qe, order);
	}
}

void queue_destroy_finalize(queue_entry_t *qe);

#ifdef __cplusplus
}
#endif

#endif
