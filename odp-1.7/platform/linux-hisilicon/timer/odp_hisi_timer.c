/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/queue.h>

#include <odp/atomic.h>
#include <odp/random.h>
#include <odp/hints.h>

#include <odp_common.h>
#include <odp_cycles.h>

/* #include <odp_per_lcore.h> */
#include <odp_memory.h>
#include <odp_mmdistrict.h>

/* #include <odp_launch.h> */
#include <odp_base.h>

/* #include <odp_per_lcore.h> */
#include <odp_core.h>

/* #include <odp_branch_prediction.h> */
#include <odp/spinlock.h>
#include <odp/sync.h>

#include "odp_hisi_timer.h"
#include "odp_hisi_atomic.h"
#include "odp_debug_internal.h"
LIST_HEAD(odp_timer_list, odp_hisi_timer);

struct priv_timer {
	/**< dummy timer instance to head up list */
	struct odp_hisi_timer pending_head;
	odp_spinlock_t	      list_lock; /**< lock to protect list access */

	/** per-core variable that true if a timer was updated on this
	 *  core since last reset of the variable */
	int updated;

	/** track the current depth of the skiplist */
	unsigned curr_skiplist_depth;

	unsigned prev_core;          /**< used for core round robin */
} __odp_cache_aligned;

/** per-core private info for timers */
static struct priv_timer priv_timer[ODP_MAX_CORE];

/* when debug is enabled, store some statistics */
#define __TIMER_STAT_ADD(name, n) do {} while (0)

/* Init the timer library. */
void odp_hisi_timer_subsystem_init(void)
{
	unsigned core_id;

	/* since priv_timer is static, it's zeroed by default, so only init some
	 * fields.
	 */
	for (core_id = 0; core_id < ODP_MAX_CORE; core_id++) {
		odp_spinlock_init(&priv_timer[core_id].list_lock);
		priv_timer[core_id].prev_core = core_id;
	}
}

/* Initialize the timer handle tim for use */
void odp_hisi_timer_init(struct odp_hisi_timer *tim)
{
	union odp_hisi_timer_status status;

	status.state = ODP_HISI_TIMER_STOP;
	status.owner = ODP_HISI_TIMER_NO_OWNER;
	tim->status.u32 = status.u32;
}

/*
 * if timer is pending or stopped (or running on the same core than
 * us), mark timer as configuring, and on success return the previous
 * status of the timer
 */
static int timer_set_config_state(struct odp_hisi_timer	      *tim,
				  union odp_hisi_timer_status *ret_prev_status)
{
	union odp_hisi_timer_status prev_status, status;
	int success = 0;
	unsigned core_id;

	core_id = odp_core_id();

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	while (success == 0) {
		prev_status.u32 = tim->status.u32;

		/* timer is running on another core, exit */
		if ((prev_status.state == ODP_HISI_TIMER_RUNNING) &&
		    (prev_status.owner != (uint16_t)core_id))
			return -1;

		/* timer is being configured on another core */
		if (prev_status.state == ODP_HISI_TIMER_CONFIG)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as being configured */
		status.state = ODP_HISI_TIMER_CONFIG;
		status.owner = (int16_t)core_id;
		success =
			odp_atomic_cmpset_u32_a64(
				(odp_atomic_u32_t *)&tim->status.u32,
				prev_status.u32,
				status.u32);
	}

	ret_prev_status->u32 = prev_status.u32;
	return 0;
}

/*
 * if timer is pending, mark timer as running
 */
static int timer_set_running_state(struct odp_hisi_timer *tim)
{
	union odp_hisi_timer_status prev_status, status;
	unsigned core_id = odp_core_id();
	int success = 0;

	/* wait that the timer is in correct status before update,
	 * and mark it as running */
	while (success == 0) {
		prev_status.u32 = tim->status.u32;

		/* timer is not pending anymore */
		if (prev_status.state != ODP_HISI_TIMER_PENDING)
			return -1;

		/* here, we know that timer is stopped or pending,
		 * mark it atomically as being configured */
		status.state = ODP_HISI_TIMER_RUNNING;
		status.owner = (int16_t)core_id;
		success =
			odp_atomic_cmpset_u32_a64(
				(odp_atomic_u32_t *)&tim->status.u32,
				prev_status.u32,
				status.u32);
	}

	return 0;
}

/*
 * Return a skiplist level for a new entry.
 * This probabalistically gives a level with p=1/4 that an entry at level n
 * will also appear at level n+1.
 */
static uint32_t timer_get_skiplist_level(unsigned curr_depth)
{
	/* probability value is 1/4, i.e. all at level 0, 1 in 4 is at level 1,
	 * 1 in 16 at level 2, 1 in 64 at level 3, etc. Calculated using lowest
	 * bit position of a (pseudo)random number.
	 */

	/* uint32_t rand = odp_rand() & (UINT32_MAX - 1); */
	uint32_t rand;

	odp_random_data((uint8_t *)&rand, sizeof(int32_t), 0);
	rand = rand & (UINT32_MAX - 1);

	uint32_t level = rand == 0 ? MAX_SKIPLIST_DEPTH :
			 (odp_bsf32(rand) - 1) / 2;

	/* limit the levels used to one above our current level, so we don't,
	 * for instance, have a level 0 and a level 7 without anything between
	 */
	if (level > curr_depth)
		level = curr_depth;

	if (level >= MAX_SKIPLIST_DEPTH)
		level = MAX_SKIPLIST_DEPTH - 1;

	return level;
}

/*
 * For a given time value, get the entries at each level which
 * are <= that time value.
 */
static void timer_get_prev_entries(uint64_t time_val, unsigned tim_core,
				   struct odp_hisi_timer **prev)
{
	unsigned lvl = priv_timer[tim_core].curr_skiplist_depth;

	prev[lvl] = &priv_timer[tim_core].pending_head;
	while (lvl != 0) {
		lvl--;
		prev[lvl] = prev[lvl + 1];
		while (prev[lvl]->sl_next[lvl] &&
		       prev[lvl]->sl_next[lvl]->expire <= time_val)
			prev[lvl] = prev[lvl]->sl_next[lvl];
	}
}

/*
 * Given a timer node in the skiplist, find the previous entries for it at
 * all skiplist levels.
 */
static void timer_get_prev_entries_for_node(struct odp_hisi_timer  *tim,
					    unsigned		    tim_core,
					    struct odp_hisi_timer **prev)
{
	int i;

	/* get a specific entry in the list, look for just lower than the time
	 * values, and then increment on each level individually if necessary
	 */
	timer_get_prev_entries(tim->expire - 1, tim_core, prev);
	for (i = priv_timer[tim_core].curr_skiplist_depth - 1; i >= 0; i--)
		while (prev[i]->sl_next[i] && (prev[i]->sl_next[i] != tim) &&
		       (prev[i]->sl_next[i]->expire <= tim->expire))
			prev[i] = prev[i]->sl_next[i];
}

/*
 * add in list, lock if needed
 * timer must be in config state
 * timer must not be in a list
 */
static void timer_add(struct odp_hisi_timer *tim,
		      unsigned tim_core, int local_is_locked)
{
	unsigned core_id = odp_core_id();
	unsigned lvl;
	struct odp_hisi_timer *prev[MAX_SKIPLIST_DEPTH + 1];

	/* if timer needs to be scheduled on another core, we need to
	 * lock the list; if it is on local core, we need to lock if
	 * we are not called from odp_hisi_timer_manage() */
	if ((tim_core != core_id) || !local_is_locked)
		odp_spinlock_lock(&priv_timer[tim_core].list_lock);

	/* find where exactly this element goes in the list of elements
	 * for each depth. */
	timer_get_prev_entries(tim->expire, tim_core, prev);

	/* now assign it a new level and add at that level */
	const unsigned tim_level = timer_get_skiplist_level(
		priv_timer[tim_core].curr_skiplist_depth);

	if (tim_level == priv_timer[tim_core].curr_skiplist_depth)
		priv_timer[tim_core].curr_skiplist_depth++;

	lvl = tim_level;
	while (lvl > 0) {
		tim->sl_next[lvl] = prev[lvl]->sl_next[lvl];
		prev[lvl]->sl_next[lvl] = tim;
		lvl--;
	}

	tim->sl_next[0] = prev[0]->sl_next[0];
	prev[0]->sl_next[0] = tim;

	/* save the lowest list entry into the expire field of the dummy hdr
	 * NOTE: this is not atomic on 32-bit*/
	priv_timer[tim_core].pending_head.expire =
		priv_timer[tim_core].pending_head.sl_next[0]->expire;

	if ((tim_core != core_id) || !local_is_locked)
		odp_spinlock_unlock(&priv_timer[tim_core].list_lock);
}

/*
 * del from list, lock if needed
 * timer must be in config state
 * timer must be in a list
 */
static void timer_del(struct odp_hisi_timer	 *tim,
		      union odp_hisi_timer_status prev_status,
		      int			  local_is_locked)
{
	unsigned core_id = odp_core_id();
	unsigned prev_owner = prev_status.owner;
	int i;
	struct odp_hisi_timer *prev[MAX_SKIPLIST_DEPTH + 1];

	/* if timer needs is pending another core, we need to lock the
	 * list; if it is on local core, we need to lock if we are not
	 * called from odp_hisi_timer_manage() */
	if ((prev_owner != core_id) || !local_is_locked)
		odp_spinlock_lock(&priv_timer[prev_owner].list_lock);

	/* save the lowest list entry into the expire field of the dummy hdr.
	 * NOTE: this is not atomic on 32-bit */
	if (tim == priv_timer[prev_owner].pending_head.sl_next[0])
		priv_timer[prev_owner].pending_head.expire =
			((!tim->sl_next[0]) ? 0 : tim->sl_next[0]->expire);

	/* adjust pointers from previous entries to point past this */
	timer_get_prev_entries_for_node(tim, prev_owner, prev);
	for (i = priv_timer[prev_owner].curr_skiplist_depth - 1; i >= 0; i--)
		if (prev[i]->sl_next[i] == tim)
			prev[i]->sl_next[i] = tim->sl_next[i];

	/* in case we deleted last entry at a level, adjust down max level */
	for (i = priv_timer[prev_owner].curr_skiplist_depth - 1; i >= 0; i--)
		if (!priv_timer[prev_owner].pending_head.sl_next[i])
			priv_timer[prev_owner].curr_skiplist_depth--;
		else
			break;

	if ((prev_owner != core_id) || !local_is_locked)
		odp_spinlock_unlock(&priv_timer[prev_owner].list_lock);
}

/* Reset and start the timer associated with the timer handle (private func) */
static int __odp_hisi_timer_reset(struct odp_hisi_timer *tim, uint64_t expire,
				  uint64_t period, unsigned tim_core,
				  odp_timer_cb_t fct, void *arg,
				  int local_is_locked)
{
	union odp_hisi_timer_status prev_status, status;
	int ret;
	unsigned core_id = odp_core_id();

	/* round robin for tim_core */
	if (tim_core == (unsigned)CORE_ID_ANY) {
		if (core_id < ODP_MAX_CORE) {
			/* ODP thread with valid core_id */
			tim_core = odp_get_next_core(
				priv_timer[core_id].prev_core,
				0, 1);
			priv_timer[core_id].prev_core = tim_core;
		} else {
			/* non-ODP thread do not run odp_hisi_timer_manage(),
			* so schedule the timer on the first enabled core. */
			tim_core = odp_get_next_core(CORE_ID_ANY, 0, 1);
		}
	}

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(reset, 1);
	if ((prev_status.state == ODP_HISI_TIMER_RUNNING) &&
	    (core_id < ODP_MAX_CORE))
		priv_timer[core_id].updated = 1;

	/* remove it from list */
	if (prev_status.state == ODP_HISI_TIMER_PENDING) {
		timer_del(tim, prev_status, local_is_locked);
		__TIMER_STAT_ADD(pending, -1);
	}

	tim->period = period;
	tim->expire = expire;
	tim->f = fct;
	tim->arg = arg;

	__TIMER_STAT_ADD(pending, 1);
	timer_add(tim, tim_core, local_is_locked);

	/* update state: as we are in CONFIG state, only us can modify
	 * the state so we don't need to use cmpset() here */
	odp_mb_full();
	status.state = ODP_HISI_TIMER_PENDING;
	status.owner = (int16_t)tim_core;
	tim->status.u32 = status.u32;

	return 0;
}

/* Reset and start the timer associated with the timer handle tim */
int odp_hisi_timer_reset(struct odp_hisi_timer *tim, uint64_t ticks,
			 enum odp_hisi_timer_type type, unsigned tim_core,
			 odp_timer_cb_t fct, void *arg)
{
	uint64_t cur_time = odp_get_tsc_cycles();
	uint64_t period;

	if (odp_unlikely((tim_core != (unsigned)CORE_ID_ANY) &&
			 (!odp_core_is_enabled(tim_core))))
		return -1;

	if (type == PERIODICAL)
		period = ticks;
	else
		period = 0;

	return __odp_hisi_timer_reset(tim, cur_time + ticks, period, tim_core,
				      fct, arg, 0);
}

/* loop until odp_hisi_timer_reset() succeed */
void odp_hisi_timer_reset_sync(struct odp_hisi_timer *tim, uint64_t ticks,
			       enum odp_hisi_timer_type type, unsigned tim_core,
			       odp_timer_cb_t fct, void *arg)
{
	while (odp_hisi_timer_reset(tim, ticks, type, tim_core,
				    fct, arg) != 0)
		odp_pause();
}

/* Stop the timer associated with the timer handle tim */
int odp_hisi_timer_stop(struct odp_hisi_timer *tim)
{
	union odp_hisi_timer_status prev_status, status;
	unsigned core_id = odp_core_id();
	int ret;

	/* wait that the timer is in correct status before update,
	 * and mark it as being configured */
	ret = timer_set_config_state(tim, &prev_status);
	if (ret < 0)
		return -1;

	__TIMER_STAT_ADD(stop, 1);
	if ((prev_status.state == ODP_HISI_TIMER_RUNNING) &&
	    (core_id < ODP_MAX_CORE))
		priv_timer[core_id].updated = 1;

	/* remove it from list */
	if (prev_status.state == ODP_HISI_TIMER_PENDING) {
		timer_del(tim, prev_status, 0);
		__TIMER_STAT_ADD(pending, -1);
	}

	/* mark timer as stopped */
	odp_mb_full();
	status.state = ODP_HISI_TIMER_STOP;
	status.owner = ODP_HISI_TIMER_NO_OWNER;
	tim->status.u32 = status.u32;

	return 0;
}

/* loop until odp_hisi_timer_stop() succeed */
void odp_hisi_timer_stop_sync(struct odp_hisi_timer *tim)
{
	while (odp_hisi_timer_stop(tim) != 0)
		odp_pause();
}

/* Test the PENDING status of the timer handle tim */
int odp_hisi_timer_pending(struct odp_hisi_timer *tim)
{
	return tim->status.state == ODP_HISI_TIMER_PENDING;
}

/* must be called periodically, run all timer that expired */
void odp_hisi_timer_manage(void)
{
	union odp_hisi_timer_status status;
	struct odp_hisi_timer *tim, *next_tim;
	unsigned core_id = odp_core_id();
	struct odp_hisi_timer *prev[MAX_SKIPLIST_DEPTH + 1];
	uint64_t cur_time;
	int i, ret;

	/* timer manager only runs on ODP thread with valid core_id */
	assert(core_id < ODP_MAX_CORE);

	__TIMER_STAT_ADD(manage, 1);

	/* optimize for the case where per-cpu list is empty */
	if (!priv_timer[core_id].pending_head.sl_next[0])
		return;

	cur_time = odp_get_tsc_cycles();

#ifdef HODP_ARCH_X86_64

	/* on 64bit value cached in the pending_head.expired will be updated
	 * atomically, so we can consult that for a quick check here outside the
	 * lock */
	if (odp_likely(priv_timer[core_id].pending_head.expire > cur_time))
		return;
#endif

	/* browse ordered list, add expired timers in 'expired' list */
	odp_spinlock_lock(&priv_timer[core_id].list_lock);

	/* if nothing to do just unlock and return */
	if ((!priv_timer[core_id].pending_head.sl_next[0]) ||
	    (priv_timer[core_id].pending_head.sl_next[0]->expire > cur_time))
		goto done;

	/* save start of list of expired timers */
	tim = priv_timer[core_id].pending_head.sl_next[0];

	/* break the existing list at current time point */
	timer_get_prev_entries(cur_time, core_id, prev);
	for (i = priv_timer[core_id].curr_skiplist_depth - 1; i >= 0; i--) {
		priv_timer[core_id].pending_head.sl_next[i] =
			prev[i]->sl_next[i];
		if (!prev[i]->sl_next[i])
			priv_timer[core_id].curr_skiplist_depth--;

		prev[i]->sl_next[i] = NULL;
	}

	/* now scan expired list and call callbacks */
	for (; tim; tim = next_tim) {
		next_tim = tim->sl_next[0];

		ret = timer_set_running_state(tim);

		/* this timer was not pending, continue */
		if (ret < 0)
			continue;

		odp_spinlock_unlock(&priv_timer[core_id].list_lock);

		priv_timer[core_id].updated = 0;

		/* execute callback function with list unlocked */
		tim->f(tim, tim->arg);

		odp_spinlock_lock(&priv_timer[core_id].list_lock);
		__TIMER_STAT_ADD(pending, -1);

		/* the timer was stopped or reloaded by the callback
		 * function, we have nothing to do here */
		if (priv_timer[core_id].updated == 1)
			continue;

		if (tim->period == 0) {
			/* remove from done list and mark timer as stopped */
			status.state = ODP_HISI_TIMER_STOP;
			status.owner = ODP_HISI_TIMER_NO_OWNER;
			odp_mb_full();
			tim->status.u32 = status.u32;
		} else {
			/* keep it in list and mark timer as pending */
			status.state = ODP_HISI_TIMER_PENDING;
			__TIMER_STAT_ADD(pending, 1);
			status.owner = (int16_t)core_id;
			odp_mb_full();
			tim->status.u32 = status.u32;
			__odp_hisi_timer_reset(tim, cur_time + tim->period,
					       tim->period, core_id, tim->f,
					       tim->arg, 1);
		}
	}

	/* update the next to expire timer value */
	priv_timer[core_id].pending_head.expire =
		(!priv_timer[core_id].pending_head.sl_next[0]) ? 0 :
		priv_timer[core_id].pending_head.sl_next[0]->expire;
done:

	/* job finished, unlock the list lock */
	odp_spinlock_unlock(&priv_timer[core_id].list_lock);
}

/* dump statistics about timers */
void odp_hisi_timer_dump_stats(FILE *f)
{
#ifdef ODP_TIMER_DEBUG
	struct odp_hisi_timer_debug_stats sum;
	unsigned core_id;

	memset(&sum, 0, sizeof(sum));
	for (core_id = 0; core_id < ODP_MAX_CORE; core_id++) {
		sum.reset   += priv_timer[core_id].stats.reset;
		sum.stop    += priv_timer[core_id].stats.stop;
		sum.manage  += priv_timer[core_id].stats.manage;
		sum.pending += priv_timer[core_id].stats.pending;
	}

	fprintf(f, "Timer statistics:\n");
	fprintf(f, "  reset = %d\n", sum.reset);
	fprintf(f, "  stop = %d\n", sum.stop);
	fprintf(f, "  manage = %d\n", sum.manage);
	fprintf(f, "  pending = %d\n", sum.pending);
#else
	fprintf(f, "No timer statistics, "
		"ODP_TIMER_DEBUG is disabled\n");
#endif
}
