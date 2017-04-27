/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/queue.h>
#include <odp_queue_internal.h>
#include <odp/std_types.h>
#include <odp/align.h>
#include <odp/buffer.h>
#include <odp_buffer_internal.h>
#include <odp_pool_internal.h>
#include <odp_buffer_inlines.h>
#include <odp_internal.h>
#include <odp/shared_memory.h>
#include <odp/schedule.h>
#include <odp_schedule_internal.h>
#include <odp/config.h>
#include <odp_packet_io_internal.h>
#include <odp_packet_io_queue.h>
#include <odp_debug_internal.h>
#include <odp/hints.h>
#include <odp/sync.h>

#ifdef USE_TICKETLOCK
#include <odp/ticketlock.h>
#define LOCK(a)      odp_ticketlock_lock(a)
#define UNLOCK(a)    odp_ticketlock_unlock(a)
#define LOCK_INIT(a) odp_ticketlock_init(a)
#define LOCK_TRY(a)  odp_ticketlock_trylock(a)
#else
#include <odp/spinlock.h>
#define LOCK(a)      odp_spinlock_lock(a)
#define UNLOCK(a)    odp_spinlock_unlock(a)
#define LOCK_INIT(a) odp_spinlock_init(a)
#define LOCK_TRY(a)  odp_spinlock_trylock(a)
#endif

#include <string.h>

#define RESOLVE_ORDER 0
#define SUSTAIN_ORDER 1

#define NOAPPEND 0
#define APPEND   1

typedef struct queue_table_t {
	queue_entry_t  queue[ODP_CONFIG_QUEUES];
} queue_table_t;

static queue_table_t *queue_tbl;

static inline void get_qe_locks(queue_entry_t *qe1, queue_entry_t *qe2)
{
	/* Special case: enq to self */
	if (qe1 == qe2) {
		LOCK(&qe1->s.lock);
		return;
	}

       /* Since any queue can be either a source or target, queues do not have
	* a natural locking hierarchy.  Create one by using the qentry address
	* as the ordering mechanism.
	*/

	if (qe1 < qe2) {
		LOCK(&qe1->s.lock);
		LOCK(&qe2->s.lock);
	} else {
		LOCK(&qe2->s.lock);
		LOCK(&qe1->s.lock);
	}
}

static inline void free_qe_locks(queue_entry_t *qe1, queue_entry_t *qe2)
{
	UNLOCK(&qe1->s.lock);
	if (qe1 != qe2)
		UNLOCK(&qe2->s.lock);
}

queue_entry_t *get_qentry(uint32_t queue_id)
{
	return &queue_tbl->queue[queue_id];
}

static int queue_init(queue_entry_t *queue, const char *name,
		      const odp_queue_param_t *param)
{
	strncpy(queue->s.name, name, ODP_QUEUE_NAME_LEN - 1);

	if (param) {
		memcpy(&queue->s.param, param, sizeof(odp_queue_param_t));
		if (queue->s.param.sched.lock_count >
		    ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE)
			return -1;

		if (param->type == ODP_QUEUE_TYPE_SCHED)
			queue->s.param.deq_mode = ODP_QUEUE_OP_DISABLED;
	} else {
		/* Defaults */
		odp_queue_param_init(&queue->s.param);
		queue->s.param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
		queue->s.param.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
		queue->s.param.sched.group = ODP_SCHED_GROUP_ALL;
	}

	queue->s.type = queue->s.param.type;

	switch (queue->s.type) {
	case ODP_QUEUE_TYPE_PKTIN:
		queue->s.enqueue = pktin_enqueue;
		queue->s.dequeue = pktin_dequeue;
		queue->s.enqueue_multi = pktin_enq_multi;
		queue->s.dequeue_multi = pktin_deq_multi;
		break;
	case ODP_QUEUE_TYPE_PKTOUT:
		queue->s.enqueue = queue_pktout_enq;
		queue->s.dequeue = pktout_dequeue;
		queue->s.enqueue_multi = queue_pktout_enq_multi;
		queue->s.dequeue_multi = pktout_deq_multi;
		break;
	default:
		queue->s.enqueue = queue_enq;
		queue->s.dequeue = queue_deq;
		queue->s.enqueue_multi = queue_enq_multi;
		queue->s.dequeue_multi = queue_deq_multi;
		break;
	}

	queue->s.pktin = PKTIN_INVALID;

	queue->s.head = NULL;
	queue->s.tail = NULL;

	queue->s.reorder_head = NULL;
	queue->s.reorder_tail = NULL;

	queue->s.pri_queue = ODP_QUEUE_INVALID;
	queue->s.cmd_ev    = ODP_EVENT_INVALID;
	return 0;
}

int odp_queue_init_global(void)
{
	uint32_t i, j;
	odp_shm_t shm;

	ODP_DBG("Queue init ...\n");
	shm = odp_shm_reserve("odp_queues",
			      sizeof(queue_table_t),
			      sizeof(queue_entry_t), 0);

	queue_tbl = odp_shm_addr(shm);

	if (queue_tbl == NULL)
		return -1;

	memset(queue_tbl, 0, sizeof(queue_table_t));

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		/* init locks */
		queue_entry_t *queue = get_qentry(i);

		LOCK_INIT(&queue->s.lock);
		for (j = 0; j < ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE; j++) {
			odp_atomic_init_u64(&queue->s.sync_in[j], 0);
			odp_atomic_init_u64(&queue->s.sync_out[j], 0);
		}
		queue->s.handle = queue_from_id(i);
	}

	ODP_DBG("done\n");
	ODP_DBG("Queue init global\n");
	ODP_DBG("  struct queue_entry_s size %zu\n",
		sizeof(struct queue_entry_s));
	ODP_DBG("  queue_entry_t size        %zu\n",
		sizeof(queue_entry_t));
	ODP_DBG("\n");

	return 0;
}

int odp_queue_term_global(void)
{
	int ret = 0;
	int rc = 0;
	queue_entry_t *queue;
	int i;

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue = &queue_tbl->queue[i];
		LOCK(&queue->s.lock);
		if (queue->s.status != QUEUE_STATUS_FREE) {
			ODP_ERR("Not destroyed queue: %s\n", queue->s.name);
			rc = -1;
		}
		UNLOCK(&queue->s.lock);
	}

	ret = odp_shm_free(odp_shm_lookup("odp_queues"));
	if (ret < 0) {
		ODP_ERR("shm free failed for odp_queues");
		rc = -1;
	}

	return rc;
}

odp_queue_type_t odp_queue_type(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.type;
}

odp_schedule_sync_t odp_queue_sched_type(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.sync;
}

odp_schedule_prio_t odp_queue_sched_prio(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.prio;
}

odp_schedule_group_t odp_queue_sched_group(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.group;
}

int odp_queue_lock_count(odp_queue_t handle)
{
	queue_entry_t *queue = queue_to_qentry(handle);

	return queue->s.param.sched.sync == ODP_SCHED_SYNC_ORDERED ?
		(int)queue->s.param.sched.lock_count : -1;
}

odp_queue_t odp_queue_create(const char *name, const odp_queue_param_t *param)
{
	uint32_t i;
	queue_entry_t *queue;
	odp_queue_t handle = ODP_QUEUE_INVALID;
	odp_queue_type_t type;

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue = &queue_tbl->queue[i];

		if (queue->s.status != QUEUE_STATUS_FREE)
			continue;

		LOCK(&queue->s.lock);
		if (queue->s.status == QUEUE_STATUS_FREE) {
			if (queue_init(queue, name, param)) {
				UNLOCK(&queue->s.lock);
				return handle;
			}

			type = queue->s.type;

			if (type == ODP_QUEUE_TYPE_SCHED ||
			    type == ODP_QUEUE_TYPE_PKTIN)
				queue->s.status = QUEUE_STATUS_NOTSCHED;
			else
				queue->s.status = QUEUE_STATUS_READY;

			handle = queue->s.handle;
			UNLOCK(&queue->s.lock);
			break;
		}
		UNLOCK(&queue->s.lock);
	}

	if (handle != ODP_QUEUE_INVALID &&
	    (type == ODP_QUEUE_TYPE_SCHED || type == ODP_QUEUE_TYPE_PKTIN)) {
		if (schedule_queue_init(queue)) {
			ODP_ERR("schedule queue init failed\n");
			return ODP_QUEUE_INVALID;
		}
	}

	return handle;
}

void queue_destroy_finalize(queue_entry_t *queue)
{
	LOCK(&queue->s.lock);

	if (queue->s.status == QUEUE_STATUS_DESTROYED) {
		queue->s.status = QUEUE_STATUS_FREE;
		schedule_queue_destroy(queue);
	}
	UNLOCK(&queue->s.lock);
}

int odp_queue_destroy(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	if (handle == ODP_QUEUE_INVALID)
		return -1;

	LOCK(&queue->s.lock);
	if (queue->s.status == QUEUE_STATUS_FREE) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("queue \"%s\" already free\n", queue->s.name);
		return -1;
	}
	if (queue->s.status == QUEUE_STATUS_DESTROYED) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("queue \"%s\" already destroyed\n", queue->s.name);
		return -1;
	}
	if (queue->s.head != NULL) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("queue \"%s\" not empty\n", queue->s.name);
		return -1;
	}
	if (queue_is_ordered(queue) && queue->s.reorder_head) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("queue \"%s\" reorder queue not empty\n",
			queue->s.name);
		return -1;
	}

	switch (queue->s.status) {
	case QUEUE_STATUS_READY:
		queue->s.status = QUEUE_STATUS_FREE;
		break;
	case QUEUE_STATUS_NOTSCHED:
		queue->s.status = QUEUE_STATUS_FREE;
		schedule_queue_destroy(queue);
		break;
	case QUEUE_STATUS_SCHED:
		/* Queue is still in scheduling */
		queue->s.status = QUEUE_STATUS_DESTROYED;
		break;
	default:
		ODP_ABORT("Unexpected queue status\n");
	}
	UNLOCK(&queue->s.lock);

	return 0;
}

int odp_queue_context_set(odp_queue_t handle, void *context)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);
	odp_mb_full();
	queue->s.param.context = context;
	odp_mb_full();
	return 0;
}

void *odp_queue_context(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);
	return queue->s.param.context;
}

odp_queue_t odp_queue_lookup(const char *name)
{
	uint32_t i;

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue_entry_t *queue = &queue_tbl->queue[i];

		if (queue->s.status == QUEUE_STATUS_FREE ||
		    queue->s.status == QUEUE_STATUS_DESTROYED)
			continue;

		LOCK(&queue->s.lock);
		if (strcmp(name, queue->s.name) == 0) {
			/* found it */
			UNLOCK(&queue->s.lock);
			return queue->s.handle;
		}
		UNLOCK(&queue->s.lock);
	}

	return ODP_QUEUE_INVALID;
}

int queue_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr, int sustain)
{
	queue_entry_t *origin_qe;
	uint64_t order;

	get_queue_order(&origin_qe, &order, buf_hdr);

	/* Handle enqueues from ordered queues separately */
	if (origin_qe)
		return ordered_queue_enq(queue, buf_hdr, sustain,
					 origin_qe, order);

	LOCK(&queue->s.lock);

	if (odp_unlikely(queue->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	queue_add(queue, buf_hdr);

	if (queue->s.status == QUEUE_STATUS_NOTSCHED) {
		queue->s.status = QUEUE_STATUS_SCHED;
		UNLOCK(&queue->s.lock);
		if (schedule_queue(queue))
			ODP_ABORT("schedule_queue failed\n");
		return 0;
	}

	UNLOCK(&queue->s.lock);
	return 0;
}

int ordered_queue_enq(queue_entry_t *queue,
		      odp_buffer_hdr_t *buf_hdr,
		      int sustain,
		      queue_entry_t *origin_qe,
		      uint64_t order)
{
	odp_buffer_hdr_t *reorder_buf;
	odp_buffer_hdr_t *next_buf;
	odp_buffer_hdr_t *reorder_tail;
	odp_buffer_hdr_t *placeholder_buf = NULL;
	int               release_count, placeholder_count;
	int               sched = 0;

	/* Need two locks for enq operations from ordered queues */
	get_qe_locks(origin_qe, queue);

	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY ||
			 queue->s.status < QUEUE_STATUS_READY)) {
		free_qe_locks(queue, origin_qe);
		ODP_ERR("Bad queue status\n");
		ODP_ERR("queue = %s, origin q = %s, buf = %p\n",
			queue->s.name, origin_qe->s.name, buf_hdr);
		return -1;
	}

	/* Remember that enq was called for this order */
	sched_enq_called();

	/* We can only complete this enq if we're in order */
	if (order > origin_qe->s.order_out) {
		reorder_enq(queue, order, origin_qe, buf_hdr, sustain);

		/* This enq can't complete until order is restored, so
		 * we're done here.
		 */
		free_qe_locks(queue, origin_qe);
		return 0;
	}

	/* Resolve order if requested */
	if (!sustain) {
		order_release(origin_qe, 1);
		sched_order_resolved(buf_hdr);
	}

	/* Update queue status */
	if (queue->s.status == QUEUE_STATUS_NOTSCHED) {
		queue->s.status = QUEUE_STATUS_SCHED;
		sched = 1;
	}

	/* We're in order, however the reorder queue may have other buffers
	 * sharing this order on it and this buffer must not be enqueued ahead
	 * of them. If the reorder queue is empty we can short-cut and
	 * simply add to the target queue directly.
	 */

	if (!origin_qe->s.reorder_head) {
		queue_add_chain(queue, buf_hdr);
		free_qe_locks(queue, origin_qe);

		/* Add queue to scheduling */
		if (sched && schedule_queue(queue))
			ODP_ABORT("schedule_queue failed\n");
		return 0;
	}

	/* The reorder_queue is non-empty, so sort this buffer into it.  Note
	 * that we force the sustain bit on here because we'll be removing
	 * this immediately and we already accounted for this order earlier.
	 */
	reorder_enq(queue, order, origin_qe, buf_hdr, 1);

	/* Pick up this element, and all others resolved by this enq,
	 * and add them to the target queue.
	 */
	reorder_deq(queue, origin_qe, &reorder_tail, &placeholder_buf,
		    &release_count, &placeholder_count);

	/* Move the list from the reorder queue to the target queue */
	if (queue->s.head)
		queue->s.tail->next = origin_qe->s.reorder_head;
	else
		queue->s.head       = origin_qe->s.reorder_head;
	queue->s.tail               = reorder_tail;
	origin_qe->s.reorder_head   = reorder_tail->next;
	reorder_tail->next          = NULL;

	/* Reflect resolved orders in the output sequence */
	order_release(origin_qe, release_count + placeholder_count);

	/* Now handle any resolved orders for events destined for other
	 * queues, appending placeholder bufs as needed.
	 */
	if (origin_qe != queue)
		UNLOCK(&queue->s.lock);

	/* Add queue to scheduling */
	if (sched && schedule_queue(queue))
		ODP_ABORT("schedule_queue failed\n");

	reorder_complete(origin_qe, &reorder_buf, &placeholder_buf, APPEND);
	UNLOCK(&origin_qe->s.lock);

	if (reorder_buf)
		queue_enq_internal(reorder_buf);

	/* Free all placeholder bufs that are now released */
	while (placeholder_buf) {
		next_buf = placeholder_buf->next;
		odp_buffer_free((odp_buffer_t)placeholder_buf);
		placeholder_buf = next_buf;
	}

	return 0;
}

int queue_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
		    int num, int sustain)
{
	int sched = 0;
	int i, rc;
	odp_buffer_hdr_t *tail;
	queue_entry_t *origin_qe;
	uint64_t order;

	/* Chain input buffers together */
	for (i = 0; i < num - 1; i++)
		buf_hdr[i]->next = buf_hdr[i + 1];

	tail = buf_hdr[num - 1];
	buf_hdr[num - 1]->next = NULL;

	/* Handle ordered enqueues commonly via links */
	get_queue_order(&origin_qe, &order, buf_hdr[0]);
	if (origin_qe) {
		buf_hdr[0]->link = buf_hdr[0]->next;
		rc = ordered_queue_enq(queue, buf_hdr[0], sustain,
				       origin_qe, order);
		return rc == 0 ? num : rc;
	}

	/* Handle unordered enqueues */
	LOCK(&queue->s.lock);
	if (odp_unlikely(queue->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	/* Empty queue */
	if (queue->s.head == NULL)
		queue->s.head = buf_hdr[0];
	else
		queue->s.tail->next = buf_hdr[0];

	queue->s.tail = tail;

	if (queue->s.status == QUEUE_STATUS_NOTSCHED) {
		queue->s.status = QUEUE_STATUS_SCHED;
		sched = 1; /* retval: schedule queue */
	}
	UNLOCK(&queue->s.lock);

	/* Add queue to scheduling */
	if (sched && schedule_queue(queue))
		ODP_ABORT("schedule_queue failed\n");

	return num; /* All events enqueued */
}

int odp_queue_enq_multi(odp_queue_t handle, const odp_event_t ev[], int num)
{
	queue_entry_t *queue;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	return num == 0 ? 0 : queue->s.enqueue_multi(queue,
		(odp_buffer_hdr_t **)ev, num, SUSTAIN_ORDER);
}

int odp_queue_enq(odp_queue_t handle, odp_event_t ev)
{
	odp_buffer_hdr_t *buf_hdr;
	queue_entry_t *queue;

	queue   = queue_to_qentry(handle);
	buf_hdr = odp_buf_to_hdr(odp_buffer_from_event(ev));

	/* No chains via this entry */
	buf_hdr->link = NULL;

	return queue->s.enqueue(queue, buf_hdr, SUSTAIN_ORDER);
}

int queue_enq_internal(odp_buffer_hdr_t *buf_hdr)
{
	return buf_hdr->target_qe->s.enqueue(buf_hdr->target_qe, buf_hdr,
					     buf_hdr->flags.sustain);
}

odp_buffer_hdr_t *queue_deq(queue_entry_t *queue)
{
	odp_buffer_hdr_t *buf_hdr;
	uint32_t i;

	LOCK(&queue->s.lock);

	if (queue->s.head == NULL) {
		/* Already empty queue */
		if (queue->s.status == QUEUE_STATUS_SCHED)
			queue->s.status = QUEUE_STATUS_NOTSCHED;

		UNLOCK(&queue->s.lock);
		return NULL;
	}

	buf_hdr       = queue->s.head;
	queue->s.head = buf_hdr->next;
	buf_hdr->next = NULL;

	/* Note that order should really be assigned on enq to an
	 * ordered queue rather than deq, however the logic is simpler
	 * to do it here and has the same effect.
	 */
	if (queue_is_ordered(queue)) {
		buf_hdr->origin_qe = queue;
		buf_hdr->order = queue->s.order_in++;
		for (i = 0; i < queue->s.param.sched.lock_count; i++) {
			buf_hdr->sync[i] =
				odp_atomic_fetch_inc_u64(&queue->s.sync_in[i]);
		}
		buf_hdr->flags.sustain = SUSTAIN_ORDER;
	} else {
		buf_hdr->origin_qe = NULL;
	}

	if (queue->s.head == NULL) {
		/* Queue is now empty */
		queue->s.tail = NULL;
	}

	UNLOCK(&queue->s.lock);

	return buf_hdr;
}

int queue_deq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num)
{
	odp_buffer_hdr_t *hdr;
	int i;
	uint32_t j;

	LOCK(&queue->s.lock);
	if (odp_unlikely(queue->s.status < QUEUE_STATUS_READY)) {
		/* Bad queue, or queue has been destroyed.
		 * Scheduler finalizes queue destroy after this. */
		UNLOCK(&queue->s.lock);
		return -1;
	}

	hdr = queue->s.head;

	if (hdr == NULL) {
		/* Already empty queue */
		if (queue->s.status == QUEUE_STATUS_SCHED)
			queue->s.status = QUEUE_STATUS_NOTSCHED;

		UNLOCK(&queue->s.lock);
		return 0;
	}

	for (i = 0; i < num && hdr; i++) {
		buf_hdr[i]       = hdr;
		hdr              = hdr->next;
		buf_hdr[i]->next = NULL;
		if (queue_is_ordered(queue)) {
			buf_hdr[i]->origin_qe = queue;
			buf_hdr[i]->order     = queue->s.order_in++;
			for (j = 0; j < queue->s.param.sched.lock_count; j++) {
				buf_hdr[i]->sync[j] =
					odp_atomic_fetch_inc_u64
					(&queue->s.sync_in[j]);
			}
			buf_hdr[i]->flags.sustain = SUSTAIN_ORDER;
		} else {
			buf_hdr[i]->origin_qe = NULL;
		}
	}

	queue->s.head = hdr;

	if (hdr == NULL) {
		/* Queue is now empty */
		queue->s.tail = NULL;
	}

	UNLOCK(&queue->s.lock);

	return i;
}

int odp_queue_deq_multi(odp_queue_t handle, odp_event_t events[], int num)
{
	queue_entry_t *queue;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	return queue->s.dequeue_multi(queue, (odp_buffer_hdr_t **)events, num);
}

odp_event_t odp_queue_deq(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue   = queue_to_qentry(handle);

	return (odp_event_t)queue->s.dequeue(queue);
}

int queue_pktout_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr,
		     int sustain)
{
	queue_entry_t *origin_qe;
	uint64_t order;
	int rc;

	/* Special processing needed only if we came from an ordered queue */
	get_queue_order(&origin_qe, &order, buf_hdr);
	if (!origin_qe)
		return pktout_enqueue(queue, buf_hdr);

	/* Must lock origin_qe for ordered processing */
	LOCK(&origin_qe->s.lock);
	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(&origin_qe->s.lock);
		ODP_ERR("Bad origin queue status\n");
		return -1;
	}

	/* We can only complete the enq if we're in order */
	sched_enq_called();
	if (order > origin_qe->s.order_out) {
		reorder_enq(queue, order, origin_qe, buf_hdr, sustain);

		/* This enq can't complete until order is restored, so
		 * we're done here.
		 */
		UNLOCK(&origin_qe->s.lock);
		return 0;
	}

	/* Perform our enq since we're in order.
	 * Note: Don't hold the origin_qe lock across an I/O operation!
	 */
	UNLOCK(&origin_qe->s.lock);

	/* Handle any chained buffers (internal calls) */
	if (buf_hdr->link) {
		odp_buffer_hdr_t *buf_hdrs[QUEUE_MULTI_MAX];
		odp_buffer_hdr_t *next_buf;
		int num = 0;

		next_buf = buf_hdr->link;
		buf_hdr->link = NULL;

		while (next_buf) {
			buf_hdrs[num++] = next_buf;
			next_buf = next_buf->next;
		}

		rc = pktout_enq_multi(queue, buf_hdrs, num);
		if (rc < num)
			return -1;
	} else {
		rc = pktout_enqueue(queue, buf_hdr);
		if (rc)
			return rc;
	}

	/* Reacquire the lock following the I/O send. Note that we're still
	 * guaranteed to be in order here since we haven't released
	 * order yet.
	 */
	LOCK(&origin_qe->s.lock);
	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(&origin_qe->s.lock);
		ODP_ERR("Bad origin queue status\n");
		return -1;
	}

	/* Account for this ordered enq */
	if (!sustain) {
		order_release(origin_qe, 1);
		sched_order_resolved(NULL);
	}

	/* Now check to see if our successful enq has unblocked other buffers
	 * in the origin's reorder queue.
	 */
	odp_buffer_hdr_t *reorder_buf;
	odp_buffer_hdr_t *next_buf;
	odp_buffer_hdr_t *reorder_tail;
	odp_buffer_hdr_t *xmit_buf;
	odp_buffer_hdr_t *placeholder_buf;
	int               release_count, placeholder_count;

	/* Send released buffers as well */
	if (reorder_deq(queue, origin_qe, &reorder_tail, &placeholder_buf,
			&release_count, &placeholder_count)) {
		xmit_buf = origin_qe->s.reorder_head;
		origin_qe->s.reorder_head = reorder_tail->next;
		reorder_tail->next = NULL;
		UNLOCK(&origin_qe->s.lock);

		do {
			next_buf = xmit_buf->next;
			pktout_enqueue(queue, xmit_buf);
			xmit_buf = next_buf;
		} while (xmit_buf);

		/* Reacquire the origin_qe lock to continue */
		LOCK(&origin_qe->s.lock);
		if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
			UNLOCK(&origin_qe->s.lock);
			ODP_ERR("Bad origin queue status\n");
			return -1;
		}
	}

	/* Update the order sequence to reflect the deq'd elements */
	order_release(origin_qe, release_count + placeholder_count);

	/* Now handle sends to other queues that are ready to go */
	reorder_complete(origin_qe, &reorder_buf, &placeholder_buf, APPEND);

	/* We're fully done with the origin_qe at last */
	UNLOCK(&origin_qe->s.lock);

	/* Now send the next buffer to its target queue */
	if (reorder_buf)
		queue_enq_internal(reorder_buf);

	/* Free all placeholder bufs that are now released */
	while (placeholder_buf) {
		next_buf = placeholder_buf->next;
		odp_buffer_free((odp_buffer_t)placeholder_buf);
		placeholder_buf = next_buf;
	}

	return 0;
}

int queue_pktout_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
			   int num, int sustain)
{
	int i, rc;
	queue_entry_t *origin_qe;
	uint64_t order;

	/* If we're not ordered, handle directly */
	get_queue_order(&origin_qe, &order, buf_hdr[0]);
	if (!origin_qe)
		return pktout_enq_multi(queue, buf_hdr, num);

	/* Chain input buffers together */
	for (i = 0; i < num - 1; i++)
		buf_hdr[i]->next = buf_hdr[i + 1];

	buf_hdr[num - 1]->next = NULL;

	/* Handle commonly via links */
	buf_hdr[0]->link = buf_hdr[0]->next;
	rc = queue_pktout_enq(queue, buf_hdr[0], sustain);
	return rc == 0 ? num : rc;
}

void queue_lock(queue_entry_t *queue)
{
	LOCK(&queue->s.lock);
}

void queue_unlock(queue_entry_t *queue)
{
	UNLOCK(&queue->s.lock);
}

void odp_queue_param_init(odp_queue_param_t *params)
{
	memset(params, 0, sizeof(odp_queue_param_t));
	params->type = ODP_QUEUE_TYPE_PLAIN;
	params->enq_mode = ODP_QUEUE_OP_MT;
	params->deq_mode = ODP_QUEUE_OP_MT;
}

/* These routines exists here rather than in odp_schedule
 * because they operate on queue interenal structures
 */
int release_order(queue_entry_t *origin_qe, uint64_t order,
		  odp_pool_t pool, int enq_called)
{
	odp_buffer_t placeholder_buf;
	odp_buffer_hdr_t *placeholder_buf_hdr, *reorder_buf, *next_buf;

	/* Must lock the origin queue to process the release */
	LOCK(&origin_qe->s.lock);

	/* If we are in order we can release immediately since there can be no
	 * confusion about intermediate elements
	 */
	if (order <= origin_qe->s.order_out) {
		reorder_buf = origin_qe->s.reorder_head;

		/* We're in order, however there may be one or more events on
		 * the reorder queue that are part of this order. If that is
		 * the case, remove them and let ordered_queue_enq() handle
		 * them and resolve the order for us.
		 */
		if (reorder_buf && reorder_buf->order == order) {
			odp_buffer_hdr_t *reorder_head = reorder_buf;

			next_buf = reorder_buf->next;

			while (next_buf && next_buf->order == order) {
				reorder_buf = next_buf;
				next_buf    = next_buf->next;
			}

			origin_qe->s.reorder_head = reorder_buf->next;
			reorder_buf->next = NULL;

			UNLOCK(&origin_qe->s.lock);
			reorder_head->link = reorder_buf->next;
			return ordered_queue_enq(reorder_head->target_qe,
						 reorder_head, RESOLVE_ORDER,
						 origin_qe, order);
		}

		/* Reorder queue has no elements for this order, so it's safe
		 * to resolve order here
		 */
		order_release(origin_qe, 1);

		/* Check if this release allows us to unblock waiters.  At the
		 * point of this call, the reorder list may contain zero or
		 * more placeholders that need to be freed, followed by zero
		 * or one complete reorder buffer chain. Note that since we
		 * are releasing order, we know no further enqs for this order
		 * can occur, so ignore the sustain bit to clear out our
		 * element(s) on the reorder queue
		 */
		reorder_complete(origin_qe, &reorder_buf,
				 &placeholder_buf_hdr, NOAPPEND);

		/* Now safe to unlock */
		UNLOCK(&origin_qe->s.lock);

		/* If reorder_buf has a target, do the enq now */
		if (reorder_buf)
			queue_enq_internal(reorder_buf);

		while (placeholder_buf_hdr) {
			odp_buffer_hdr_t *placeholder_next =
				placeholder_buf_hdr->next;

			odp_buffer_free((odp_buffer_t)placeholder_buf_hdr);
			placeholder_buf_hdr = placeholder_next;
		}

		return 0;
	}

	/* If we are not in order we need a placeholder to represent our
	 * "place in line" unless we have issued enqs, in which case we
	 * already have a place in the reorder queue. If we need a
	 * placeholder, use an element from the same pool we were scheduled
	 * with is from, otherwise just ensure that the final element for our
	 * order is not marked sustain.
	 */
	if (enq_called) {
		reorder_buf = NULL;
		next_buf    = origin_qe->s.reorder_head;

		while (next_buf && next_buf->order <= order) {
			reorder_buf = next_buf;
			next_buf = next_buf->next;
		}

		if (reorder_buf && reorder_buf->order == order) {
			reorder_buf->flags.sustain = 0;
			UNLOCK(&origin_qe->s.lock);
			return 0;
		}
	}

	placeholder_buf = odp_buffer_alloc(pool);

	/* Can't release if no placeholder is available */
	if (odp_unlikely(placeholder_buf == ODP_BUFFER_INVALID)) {
		UNLOCK(&origin_qe->s.lock);
		return -1;
	}

	placeholder_buf_hdr = odp_buf_to_hdr(placeholder_buf);

	/* Copy info to placeholder and add it to the reorder queue */
	placeholder_buf_hdr->origin_qe     = origin_qe;
	placeholder_buf_hdr->order         = order;
	placeholder_buf_hdr->flags.sustain = 0;

	reorder_enq(NULL, order, origin_qe, placeholder_buf_hdr, 0);

	UNLOCK(&origin_qe->s.lock);
	return 0;
}

int odp_queue_info(odp_queue_t handle, odp_queue_info_t *info)
{
	uint32_t queue_id;
	queue_entry_t *queue;
	int status;

	if (odp_unlikely(info == NULL)) {
		ODP_ERR("Unable to store info, NULL ptr given\n");
		return -1;
	}

	queue_id = queue_to_id(handle);

	if (odp_unlikely(queue_id >= ODP_CONFIG_QUEUES)) {
		ODP_ERR("Invalid queue handle:%" PRIu64 "\n",
			odp_queue_to_u64(handle));
		return -1;
	}

	queue = get_qentry(queue_id);

	LOCK(&queue->s.lock);
	status = queue->s.status;

	if (odp_unlikely(status == QUEUE_STATUS_FREE ||
			 status == QUEUE_STATUS_DESTROYED)) {
		UNLOCK(&queue->s.lock);
		ODP_ERR("Invalid queue status:%d\n", status);
		return -1;
	}

	info->name = queue->s.name;
	info->param = queue->s.param;

	UNLOCK(&queue->s.lock);

	return 0;
}
