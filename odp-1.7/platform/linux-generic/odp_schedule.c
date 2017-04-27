/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <string.h>
#include <odp/schedule.h>
#include <odp_schedule_internal.h>
#include <odp/align.h>
#include <odp/queue.h>
#include <odp/shared_memory.h>
#include <odp/buffer.h>
#include <odp/pool.h>
#include <odp_internal.h>
#include <odp/config.h>
#include <odp_debug_internal.h>
#include <odp/thread.h>
#include <odp/time.h>
#include <odp/spinlock.h>
#include <odp/hints.h>
#include <odp/cpu.h>

#include <odp_queue_internal.h>
#include <odp_packet_io_internal.h>

odp_thrmask_t sched_mask_all;

/* Number of schedule commands.
 * One per scheduled queue and packet interface */
#define NUM_SCHED_CMD (ODP_CONFIG_QUEUES + ODP_CONFIG_PKTIO_ENTRIES)

/* Priority queues per priority */
#define QUEUES_PER_PRIO  4

/* Packet input poll cmd queues */
#define POLL_CMD_QUEUES  4

/* Maximum number of dequeues */
#define MAX_DEQ 4

/* Maximum number of packet input queues per command */
#define MAX_PKTIN 8

/* Mask of queues per priority */
typedef uint8_t pri_mask_t;

_ODP_STATIC_ASSERT((8*sizeof(pri_mask_t)) >= QUEUES_PER_PRIO,
		   "pri_mask_t_is_too_small");

/* Internal: Start of named groups in group mask arrays */
#define _ODP_SCHED_GROUP_NAMED (ODP_SCHED_GROUP_CONTROL + 1)

typedef struct {
	odp_queue_t    pri_queue[ODP_CONFIG_SCHED_PRIOS][QUEUES_PER_PRIO];
	pri_mask_t     pri_mask[ODP_CONFIG_SCHED_PRIOS];
	odp_spinlock_t mask_lock;

	odp_spinlock_t poll_cmd_lock;
	struct {
		odp_queue_t queue;
		uint16_t    num;
	} poll_cmd[POLL_CMD_QUEUES];

	odp_pool_t     pool;
	odp_shm_t      shm;
	uint32_t       pri_count[ODP_CONFIG_SCHED_PRIOS][QUEUES_PER_PRIO];
	odp_spinlock_t grp_lock;
	struct {
		char           name[ODP_SCHED_GROUP_NAME_LEN];
		odp_thrmask_t *mask;
	} sched_grp[ODP_CONFIG_SCHED_GRPS];
} sched_t;

/* Schedule command */
typedef struct {
	int           cmd;

	union {
		queue_entry_t *qe;

		struct {
			odp_pktio_t   pktio;
			int           num;
			int           index[MAX_PKTIN];
			pktio_entry_t *pe;
		};
	};
} sched_cmd_t;

#define SCHED_CMD_DEQUEUE    0
#define SCHED_CMD_POLL_PKTIN 1


typedef struct {
	int thr;
	int num;
	int index;
	int pause;
	uint32_t pktin_polls;
	odp_queue_t pri_queue;
	odp_event_t cmd_ev;
	queue_entry_t *qe;
	queue_entry_t *origin_qe;
	odp_buffer_hdr_t *buf_hdr[MAX_DEQ];
	uint64_t order;
	uint64_t sync[ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE];
	odp_pool_t pool;
	int enq_called;
	int ignore_ordered_context;
} sched_local_t;

/* Global scheduler context */
static sched_t *sched;

/* Thread local scheduler context */
static __thread sched_local_t sched_local;

/* Internal routine to get scheduler thread mask addrs */
odp_thrmask_t *thread_sched_grp_mask(int index);

static void sched_local_init(void)
{
	memset(&sched_local, 0, sizeof(sched_local_t));

	sched_local.thr       = odp_thread_id();
	sched_local.pri_queue = ODP_QUEUE_INVALID;
	sched_local.cmd_ev    = ODP_EVENT_INVALID;
}

int odp_schedule_init_global(void)
{
	odp_shm_t shm;
	odp_pool_t pool;
	int i, j;
	odp_pool_param_t params;

	ODP_DBG("Schedule init ...\n");
	shm = odp_shm_reserve("odp_scheduler",
			      sizeof(sched_t),
			      ODP_CACHE_LINE_SIZE, 0);

	sched = odp_shm_addr(shm);

	if (sched == NULL) {
		ODP_ERR("Schedule init: Shm reserve failed.\n");
		return -1;
	}

	memset(sched, 0, sizeof(sched_t));

	odp_pool_param_init(&params);
	params.buf.size  = sizeof(sched_cmd_t);
	params.buf.align = 0;
	params.buf.num   = NUM_SCHED_CMD;
	params.type      = ODP_POOL_BUFFER;

	pool = odp_pool_create("odp_sched_pool", &params);

	if (pool == ODP_POOL_INVALID) {
		ODP_ERR("Schedule init: Pool create failed.\n");
		return -1;
	}

	sched->pool = pool;
	sched->shm  = shm;
	odp_spinlock_init(&sched->mask_lock);

	for (i = 0; i < ODP_CONFIG_SCHED_PRIOS; i++) {
		odp_queue_t queue;
		char name[] = "odp_priXX_YY";

		name[7] = '0' + i / 10;
		name[8] = '0' + i - 10*(i / 10);

		for (j = 0; j < QUEUES_PER_PRIO; j++) {
			name[10] = '0' + j / 10;
			name[11] = '0' + j - 10*(j / 10);

			queue = odp_queue_create(name, NULL);

			if (queue == ODP_QUEUE_INVALID) {
				ODP_ERR("Sched init: Queue create failed.\n");
				return -1;
			}

			sched->pri_queue[i][j] = queue;
			sched->pri_mask[i]     = 0;
		}
	}

	odp_spinlock_init(&sched->poll_cmd_lock);
	for (i = 0; i < POLL_CMD_QUEUES; i++) {
		odp_queue_t queue;
		char name[] = "odp_poll_cmd_YY";

		name[13] = '0' + i / 10;
		name[14] = '0' + i - 10 * (i / 10);

		queue = odp_queue_create(name, NULL);

		if (queue == ODP_QUEUE_INVALID) {
			ODP_ERR("Sched init: Queue create failed.\n");
			return -1;
		}

		sched->poll_cmd[i].queue = queue;
	}

	odp_spinlock_init(&sched->grp_lock);

	for (i = 0; i < ODP_CONFIG_SCHED_GRPS; i++) {
		memset(sched->sched_grp[i].name, 0, ODP_SCHED_GROUP_NAME_LEN);
		sched->sched_grp[i].mask = thread_sched_grp_mask(i);
	}

	odp_thrmask_setall(&sched_mask_all);

	ODP_DBG("done\n");

	return 0;
}

int odp_schedule_term_global(void)
{
	int ret = 0;
	int rc = 0;
	int i, j;
	odp_event_t  ev;

	for (i = 0; i < ODP_CONFIG_SCHED_PRIOS; i++) {
		for (j = 0; j < QUEUES_PER_PRIO; j++) {
			odp_queue_t  pri_q;

			pri_q = sched->pri_queue[i][j];

			while ((ev = odp_queue_deq(pri_q)) !=
			      ODP_EVENT_INVALID) {
				odp_buffer_t buf;
				sched_cmd_t *sched_cmd;
				queue_entry_t *qe;
				odp_buffer_hdr_t *buf_hdr[1];
				int num;

				buf = odp_buffer_from_event(ev);
				sched_cmd = odp_buffer_addr(buf);
				qe  = sched_cmd->qe;
				num = queue_deq_multi(qe, buf_hdr, 1);

				if (num < 0)
					queue_destroy_finalize(qe);

				if (num > 0)
					ODP_ERR("Queue not empty\n");
			}

			if (odp_queue_destroy(pri_q)) {
				ODP_ERR("Pri queue destroy fail.\n");
				rc = -1;
			}
		}
	}

	for (i = 0; i < POLL_CMD_QUEUES; i++) {
		odp_queue_t queue = sched->poll_cmd[i].queue;

		while ((ev = odp_queue_deq(queue)) != ODP_EVENT_INVALID)
			odp_event_free(ev);

		if (odp_queue_destroy(queue)) {
			ODP_ERR("Poll cmd queue destroy failed\n");
			rc = -1;
		}
	}

	if (odp_pool_destroy(sched->pool) != 0) {
		ODP_ERR("Pool destroy fail.\n");
		rc = -1;
	}

	ret = odp_shm_free(sched->shm);
	if (ret < 0) {
		ODP_ERR("Shm free failed for odp_scheduler");
		rc = -1;
	}

	return rc;
}

int odp_schedule_init_local(void)
{
	sched_local_init();
	return 0;
}

int odp_schedule_term_local(void)
{
	if (sched_local.num) {
		ODP_ERR("Locally pre-scheduled events exist.\n");
		return -1;
	}

	odp_schedule_release_context();

	sched_local_init();
	return 0;
}

static int pri_id_queue(odp_queue_t queue)
{
	return (QUEUES_PER_PRIO-1) & (queue_to_id(queue));
}

static odp_queue_t pri_set(int id, int prio)
{
	odp_spinlock_lock(&sched->mask_lock);
	sched->pri_mask[prio] |= 1 << id;
	sched->pri_count[prio][id]++;
	odp_spinlock_unlock(&sched->mask_lock);

	return sched->pri_queue[prio][id];
}

static void pri_clr(int id, int prio)
{
	odp_spinlock_lock(&sched->mask_lock);

	/* Clear mask bit when last queue is removed*/
	sched->pri_count[prio][id]--;

	if (sched->pri_count[prio][id] == 0)
		sched->pri_mask[prio] &= (uint8_t)(~(1 << id));

	odp_spinlock_unlock(&sched->mask_lock);
}

static odp_queue_t pri_set_queue(odp_queue_t queue, int prio)
{
	int id = pri_id_queue(queue);

	return pri_set(id, prio);
}

static void pri_clr_queue(odp_queue_t queue, int prio)
{
	int id = pri_id_queue(queue);

	pri_clr(id, prio);
}

int schedule_queue_init(queue_entry_t *qe)
{
	odp_buffer_t buf;
	sched_cmd_t *sched_cmd;

	buf = odp_buffer_alloc(sched->pool);

	if (buf == ODP_BUFFER_INVALID)
		return -1;

	sched_cmd      = odp_buffer_addr(buf);
	sched_cmd->cmd = SCHED_CMD_DEQUEUE;
	sched_cmd->qe  = qe;

	qe->s.cmd_ev    = odp_buffer_to_event(buf);
	qe->s.pri_queue = pri_set_queue(queue_handle(qe), queue_prio(qe));

	return 0;
}

void schedule_queue_destroy(queue_entry_t *qe)
{
	odp_event_free(qe->s.cmd_ev);

	pri_clr_queue(queue_handle(qe), queue_prio(qe));

	qe->s.cmd_ev    = ODP_EVENT_INVALID;
	qe->s.pri_queue = ODP_QUEUE_INVALID;
}

static int poll_cmd_queue_idx(odp_pktio_t pktio, int in_queue_idx)
{
	return (POLL_CMD_QUEUES - 1) & (pktio_to_id(pktio) ^ in_queue_idx);
}

void schedule_pktio_start(odp_pktio_t pktio, int num_in_queue,
			  int in_queue_idx[])
{
	odp_buffer_t buf;
	sched_cmd_t *sched_cmd;
	odp_queue_t queue;
	int i, idx;

	buf = odp_buffer_alloc(sched->pool);

	if (buf == ODP_BUFFER_INVALID)
		ODP_ABORT("Sched pool empty\n");

	sched_cmd        = odp_buffer_addr(buf);
	sched_cmd->cmd   = SCHED_CMD_POLL_PKTIN;
	sched_cmd->pktio = pktio;
	sched_cmd->num   = num_in_queue;
	sched_cmd->pe    = get_pktio_entry(pktio);

	if (num_in_queue > MAX_PKTIN)
		ODP_ABORT("Too many input queues for scheduler\n");

	for (i = 0; i < num_in_queue; i++)
		sched_cmd->index[i] = in_queue_idx[i];

	idx = poll_cmd_queue_idx(pktio, in_queue_idx[0]);

	odp_spinlock_lock(&sched->poll_cmd_lock);
	sched->poll_cmd[idx].num++;
	odp_spinlock_unlock(&sched->poll_cmd_lock);

	queue = sched->poll_cmd[idx].queue;

	if (odp_queue_enq(queue, odp_buffer_to_event(buf)))
		ODP_ABORT("schedule_pktio_start failed\n");
}

static void schedule_pktio_stop(sched_cmd_t *sched_cmd)
{
	int idx = poll_cmd_queue_idx(sched_cmd->pktio, sched_cmd->index[0]);

	odp_spinlock_lock(&sched->poll_cmd_lock);
	sched->poll_cmd[idx].num--;
	odp_spinlock_unlock(&sched->poll_cmd_lock);
}

void odp_schedule_release_atomic(void)
{
	if (sched_local.pri_queue != ODP_QUEUE_INVALID &&
	    sched_local.num       == 0) {
		/* Release current atomic queue */
		if (odp_queue_enq(sched_local.pri_queue, sched_local.cmd_ev))
			ODP_ABORT("odp_schedule_release_atomic failed\n");
		sched_local.pri_queue = ODP_QUEUE_INVALID;
	}
}

void odp_schedule_release_ordered(void)
{
	if (sched_local.origin_qe) {
		int rc = release_order(sched_local.origin_qe,
				       sched_local.order,
				       sched_local.pool,
				       sched_local.enq_called);
		if (rc == 0)
			sched_local.origin_qe = NULL;
	}
}

void odp_schedule_release_context(void)
{
	if (sched_local.origin_qe) {
		release_order(sched_local.origin_qe, sched_local.order,
			      sched_local.pool, sched_local.enq_called);
		sched_local.origin_qe = NULL;
	} else
		odp_schedule_release_atomic();
}

static inline int copy_events(odp_event_t out_ev[], unsigned int max)
{
	int i = 0;

	while (sched_local.num && max) {
		out_ev[i] =
			(odp_event_t)(sched_local.buf_hdr[sched_local.index]);
		sched_local.index++;
		sched_local.num--;
		max--;
		i++;
	}

	return i;
}

/*
 * Schedule queues
 */
static int schedule(odp_queue_t *out_queue, odp_event_t out_ev[],
		    unsigned int max_num, unsigned int max_deq)
{
	int i, j;
	int ret;
	uint32_t k;
	int id;
	odp_event_t ev;
	odp_buffer_t buf;
	sched_cmd_t *sched_cmd;

	if (sched_local.num) {
		ret = copy_events(out_ev, max_num);

		if (out_queue)
			*out_queue = queue_handle(sched_local.qe);

		return ret;
	}

	odp_schedule_release_context();

	if (odp_unlikely(sched_local.pause))
		return 0;

	/* Schedule events */
	for (i = 0; i < ODP_CONFIG_SCHED_PRIOS; i++) {

		if (sched->pri_mask[i] == 0)
			continue;

		id = sched_local.thr & (QUEUES_PER_PRIO - 1);

		for (j = 0; j < QUEUES_PER_PRIO; j++, id++) {
			odp_queue_t  pri_q;
			queue_entry_t *qe;
			int num;
			int qe_grp;

			if (id >= QUEUES_PER_PRIO)
				id = 0;

			if (odp_unlikely((sched->pri_mask[i] & (1 << id)) == 0))
				continue;

			pri_q = sched->pri_queue[i][id];
			ev    = odp_queue_deq(pri_q);

			if (ev == ODP_EVENT_INVALID)
				continue;

			buf       = odp_buffer_from_event(ev);
			sched_cmd = odp_buffer_addr(buf);

			qe     = sched_cmd->qe;
			qe_grp = qe->s.param.sched.group;

			if (qe_grp > ODP_SCHED_GROUP_ALL &&
			    !odp_thrmask_isset(sched->sched_grp[qe_grp].mask,
					       sched_local.thr)) {
				/* This thread is not eligible for work from
				 * this queue, so continue scheduling it.
				 */
				if (odp_queue_enq(pri_q, ev))
					ODP_ABORT("schedule failed\n");
				continue;
			}

			/* For ordered queues we want consecutive events to
			 * be dispatched to separate threads, so do not cache
			 * them locally.
			 */
			if (queue_is_ordered(qe))
				max_deq = 1;
			num = queue_deq_multi(qe, sched_local.buf_hdr, max_deq);

			if (num < 0) {
				/* Destroyed queue */
				queue_destroy_finalize(qe);
				continue;
			}

			if (num == 0) {
				/* Remove empty queue from scheduling */
				continue;
			}

			sched_local.num   = num;
			sched_local.index = 0;
			sched_local.qe    = qe;
			ret = copy_events(out_ev, max_num);

			if (queue_is_ordered(qe)) {
				/* Continue scheduling ordered queues */
				if (odp_queue_enq(pri_q, ev))
					ODP_ABORT("schedule failed\n");
				/* Cache order info about this event */
				sched_local.origin_qe = qe;
				sched_local.order =
					sched_local.buf_hdr[0]->order;
				sched_local.pool =
					sched_local.buf_hdr[0]->pool_hdl;
				for (k = 0;
				     k < qe->s.param.sched.lock_count;
				     k++) {
					sched_local.sync[k] =
						sched_local.buf_hdr[0]->sync[k];
				}
				sched_local.enq_called = 0;
			} else if (queue_is_atomic(qe)) {
				/* Hold queue during atomic access */
				sched_local.pri_queue = pri_q;
				sched_local.cmd_ev    = ev;
			} else {
				/* Continue scheduling the queue */
				if (odp_queue_enq(pri_q, ev))
					ODP_ABORT("schedule failed\n");
			}

			/* Output the source queue handle */
			if (out_queue)
				*out_queue = queue_handle(qe);

			return ret;
		}
	}

	/*
	 * Poll packet input when there are no events
	 *   * Each thread starts the search for a poll command from its
	 *     preferred command queue. If the queue is empty, it moves to other
	 *     queues.
	 *   * Most of the times, the search stops on the first command found to
	 *     optimize multi-threaded performance. A small portion of polls
	 *     have to do full iteration to avoid packet input starvation when
	 *     there are less threads than command queues.
	 */
	id = sched_local.thr & (POLL_CMD_QUEUES - 1);

	for (i = 0; i < POLL_CMD_QUEUES; i++, id++) {
		odp_queue_t cmd_queue;

		if (id == POLL_CMD_QUEUES)
			id = 0;

		if (sched->poll_cmd[id].num == 0)
			continue;

		cmd_queue = sched->poll_cmd[id].queue;
		ev = odp_queue_deq(cmd_queue);

		if (ev == ODP_EVENT_INVALID)
			continue;

		buf       = odp_buffer_from_event(ev);
		sched_cmd = odp_buffer_addr(buf);

		if (sched_cmd->cmd != SCHED_CMD_POLL_PKTIN)
			ODP_ABORT("Bad poll command\n");

		/* Poll packet input */
		if (pktin_poll(sched_cmd->pe,
			       sched_cmd->num,
			       sched_cmd->index)) {
			/* Stop scheduling the pktio */
			schedule_pktio_stop(sched_cmd);
			odp_buffer_free(buf);
		} else {
			/* Continue scheduling the pktio */
			if (odp_queue_enq(cmd_queue, ev))
				ODP_ABORT("Poll command enqueue failed\n");

			/* Do not iterate through all pktin poll command queues
			 * every time. */
			if (odp_likely(sched_local.pktin_polls & 0xf))
				break;
		}
	}

	sched_local.pktin_polls++;
	return 0;
}


static int schedule_loop(odp_queue_t *out_queue, uint64_t wait,
			 odp_event_t out_ev[],
			 unsigned int max_num, unsigned int max_deq)
{
	odp_time_t next, wtime;
	int first = 1;
	int ret;

	while (1) {
		ret = schedule(out_queue, out_ev, max_num, max_deq);

		if (ret)
			break;

		if (wait == ODP_SCHED_WAIT)
			continue;

		if (wait == ODP_SCHED_NO_WAIT)
			break;

		if (first) {
			wtime = odp_time_local_from_ns(wait);
			next = odp_time_sum(odp_time_local(), wtime);
			first = 0;
			continue;
		}

		if (odp_time_cmp(next, odp_time_local()) < 0)
			break;
	}

	return ret;
}


odp_event_t odp_schedule(odp_queue_t *out_queue, uint64_t wait)
{
	odp_event_t ev;

	ev = ODP_EVENT_INVALID;

	schedule_loop(out_queue, wait, &ev, 1, MAX_DEQ);

	return ev;
}


int odp_schedule_multi(odp_queue_t *out_queue, uint64_t wait,
		       odp_event_t events[], int num)
{
	return schedule_loop(out_queue, wait, events, num, MAX_DEQ);
}


void odp_schedule_pause(void)
{
	sched_local.pause = 1;
}

void odp_schedule_resume(void)
{
	sched_local.pause = 0;
}

uint64_t odp_schedule_wait_time(uint64_t ns)
{
	return ns;
}

int odp_schedule_num_prio(void)
{
	return ODP_CONFIG_SCHED_PRIOS;
}

odp_schedule_group_t odp_schedule_group_create(const char *name,
					       const odp_thrmask_t *mask)
{
	odp_schedule_group_t group = ODP_SCHED_GROUP_INVALID;
	int i;

	odp_spinlock_lock(&sched->grp_lock);

	for (i = _ODP_SCHED_GROUP_NAMED; i < ODP_CONFIG_SCHED_GRPS; i++) {
		if (sched->sched_grp[i].name[0] == 0) {
			strncpy(sched->sched_grp[i].name, name,
				ODP_SCHED_GROUP_NAME_LEN - 1);
			odp_thrmask_copy(sched->sched_grp[i].mask, mask);
			group = (odp_schedule_group_t)i;
			break;
		}
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return group;
}

int odp_schedule_group_destroy(odp_schedule_group_t group)
{
	int ret;

	odp_spinlock_lock(&sched->grp_lock);

	if (group < ODP_CONFIG_SCHED_GRPS &&
	    group >= _ODP_SCHED_GROUP_NAMED &&
	    sched->sched_grp[group].name[0] != 0) {
		odp_thrmask_zero(sched->sched_grp[group].mask);
		memset(sched->sched_grp[group].name, 0,
		       ODP_SCHED_GROUP_NAME_LEN);
		ret = 0;
	} else {
		ret = -1;
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return ret;
}

odp_schedule_group_t odp_schedule_group_lookup(const char *name)
{
	odp_schedule_group_t group = ODP_SCHED_GROUP_INVALID;
	int i;

	odp_spinlock_lock(&sched->grp_lock);

	for (i = _ODP_SCHED_GROUP_NAMED; i < ODP_CONFIG_SCHED_GRPS; i++) {
		if (strcmp(name, sched->sched_grp[i].name) == 0) {
			group = (odp_schedule_group_t)i;
			break;
		}
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return group;
}

int odp_schedule_group_join(odp_schedule_group_t group,
			    const odp_thrmask_t *mask)
{
	int ret;

	odp_spinlock_lock(&sched->grp_lock);

	if (group < ODP_CONFIG_SCHED_GRPS &&
	    group >= _ODP_SCHED_GROUP_NAMED &&
	    sched->sched_grp[group].name[0] != 0) {
		odp_thrmask_or(sched->sched_grp[group].mask,
			       sched->sched_grp[group].mask,
			       mask);
		ret = 0;
	} else {
		ret = -1;
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return ret;
}

int odp_schedule_group_leave(odp_schedule_group_t group,
			     const odp_thrmask_t *mask)
{
	int ret;

	odp_spinlock_lock(&sched->grp_lock);

	if (group < ODP_CONFIG_SCHED_GRPS &&
	    group >= _ODP_SCHED_GROUP_NAMED &&
	    sched->sched_grp[group].name[0] != 0) {
		odp_thrmask_t leavemask;

		odp_thrmask_xor(&leavemask, mask, &sched_mask_all);
		odp_thrmask_and(sched->sched_grp[group].mask,
				sched->sched_grp[group].mask,
				&leavemask);
		ret = 0;
	} else {
		ret = -1;
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return ret;
}

int odp_schedule_group_thrmask(odp_schedule_group_t group,
			       odp_thrmask_t *thrmask)
{
	int ret;

	odp_spinlock_lock(&sched->grp_lock);

	if (group < ODP_CONFIG_SCHED_GRPS &&
	    group >= _ODP_SCHED_GROUP_NAMED &&
	    sched->sched_grp[group].name[0] != 0) {
		*thrmask = *sched->sched_grp[group].mask;
		ret = 0;
	} else {
		ret = -1;
	}

	odp_spinlock_unlock(&sched->grp_lock);
	return ret;
}

/* This function is a no-op in linux-generic */
void odp_schedule_prefetch(int num ODP_UNUSED)
{
}

void odp_schedule_order_lock(unsigned lock_index)
{
	queue_entry_t *origin_qe;
	uint64_t sync, sync_out;

	origin_qe = sched_local.origin_qe;
	if (!origin_qe || lock_index >= origin_qe->s.param.sched.lock_count)
		return;

	sync = sched_local.sync[lock_index];
	sync_out = odp_atomic_load_u64(&origin_qe->s.sync_out[lock_index]);
	ODP_ASSERT(sync >= sync_out);

	/* Wait until we are in order. Note that sync_out will be incremented
	 * both by unlocks as well as order resolution, so we're OK if only
	 * some events in the ordered flow need to lock.
	 */
	while (sync != sync_out) {
		odp_cpu_pause();
		sync_out =
			odp_atomic_load_u64(&origin_qe->s.sync_out[lock_index]);
	}
}

void odp_schedule_order_unlock(unsigned lock_index)
{
	queue_entry_t *origin_qe;

	origin_qe = sched_local.origin_qe;
	if (!origin_qe || lock_index >= origin_qe->s.param.sched.lock_count)
		return;
	ODP_ASSERT(sched_local.sync[lock_index] ==
		   odp_atomic_load_u64(&origin_qe->s.sync_out[lock_index]));

	/* Release the ordered lock */
	odp_atomic_fetch_inc_u64(&origin_qe->s.sync_out[lock_index]);
}

void sched_enq_called(void)
{
	sched_local.enq_called = 1;
}

void get_sched_order(queue_entry_t **origin_qe, uint64_t *order)
{
	if (sched_local.ignore_ordered_context) {
		sched_local.ignore_ordered_context = 0;
		*origin_qe = NULL;
	} else {
		*origin_qe = sched_local.origin_qe;
		*order     = sched_local.order;
	}
}

void sched_order_resolved(odp_buffer_hdr_t *buf_hdr)
{
	if (buf_hdr)
		buf_hdr->origin_qe = NULL;
	sched_local.origin_qe = NULL;
}

int schedule_queue(const queue_entry_t *qe)
{
	sched_local.ignore_ordered_context = 1;
	return odp_queue_enq(qe->s.pri_queue, qe->s.cmd_ev);
}
