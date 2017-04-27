/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP timer service
 *
 */

/* Check if compiler supports 16-byte atomics. GCC needs -mcx16 flag on x86 */
/* Using spin lock actually seems faster on Core2 */
#ifdef ODP_ATOMIC_U128
/* TB_NEEDS_PAD defined if sizeof(odp_buffer_t) != 8 */
#define TB_NEEDS_PAD
#define TB_SET_PAD(x) ((x).pad = 0)
#else
#define TB_SET_PAD(x) (void)(x)
#endif

#include <odp_posix_extensions.h>

#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <odp/align.h>
#include <odp_align_internal.h>
#include <odp/atomic.h>
#include <odp_atomic_internal.h>
#include <odp/buffer.h>
#include <odp_buffer_inlines.h>
#include <odp/cpu.h>
#include <odp/pool.h>
#include <odp_pool_internal.h>
#include <odp/debug.h>
#include <odp_debug_internal.h>
#include <odp/event.h>
#include <odp/hints.h>
#include <odp_internal.h>
#include <odp/queue.h>
#include <odp/shared_memory.h>
#include <odp/spinlock.h>
#include <odp/std_types.h>
#include <odp/sync.h>
#include <odp/time.h>
#include <odp/timer.h>
#include <odp_timer_internal.h>

#define TMO_UNUSED   ((uint64_t)0xFFFFFFFFFFFFFFFF)
/* TMO_INACTIVE is or-ed with the expiration tick to indicate an expired timer.
 * The original expiration tick (63 bits) is still available so it can be used
 * for checking the freshness of received timeouts */
#define TMO_INACTIVE ((uint64_t)0x8000000000000000)

#ifdef __ARM_ARCH
#define PREFETCH(ptr) __builtin_prefetch((ptr), 0, 0)
#else
#define PREFETCH(ptr) (void)(ptr)
#endif

/******************************************************************************
 * Mutual exclusion in the absence of CAS16
 *****************************************************************************/

#ifndef ODP_ATOMIC_U128
#define NUM_LOCKS 1024
static _odp_atomic_flag_t locks[NUM_LOCKS]; /* Multiple locks per cache line! */
#define IDX2LOCK(idx) (&locks[(idx) % NUM_LOCKS])
#endif

/******************************************************************************
 * Translation between timeout buffer and timeout header
 *****************************************************************************/

static odp_timeout_hdr_t *timeout_hdr_from_buf(odp_buffer_t buf)
{
	return (odp_timeout_hdr_t *)(void *)odp_buf_to_hdr(buf);
}

static odp_timeout_hdr_t *timeout_hdr(odp_timeout_t tmo)
{
	odp_buffer_t buf = odp_buffer_from_event(odp_timeout_to_event(tmo));

	return timeout_hdr_from_buf(buf);
}

/******************************************************************************
 * odp_timer abstract datatype
 *****************************************************************************/

typedef struct tick_buf_s {
	odp_atomic_u64_t exp_tck;/* Expiration tick or TMO_xxx */
	odp_buffer_t tmo_buf;/* ODP_BUFFER_INVALID if timer not active */
#ifdef TB_NEEDS_PAD
	uint32_t pad;/* Need to be able to access padding for successful CAS */
#endif
} tick_buf_t
#ifdef ODP_ATOMIC_U128
ODP_ALIGNED(16) /* 16-byte atomic operations need properly aligned addresses */
#endif
;

_ODP_STATIC_ASSERT(sizeof(tick_buf_t) == 16, "sizeof(tick_buf_t) == 16");

typedef struct odp_timer_s {
	void *user_ptr;
	odp_queue_t queue;/* Used for free list when timer is free */
} odp_timer;

static void timer_init(odp_timer *tim,
		tick_buf_t *tb,
		odp_queue_t _q,
		void *_up)
{
	tim->queue = _q;
	tim->user_ptr = _up;
	tb->tmo_buf = ODP_BUFFER_INVALID;
	/* All pad fields need a defined and constant value */
	TB_SET_PAD(*tb);
	/* Release the timer by setting timer state to inactive */
	_odp_atomic_u64_store_mm(&tb->exp_tck, TMO_INACTIVE, _ODP_MEMMODEL_RLS);
}

/* Teardown when timer is freed */
static void timer_fini(odp_timer *tim, tick_buf_t *tb)
{
	ODP_ASSERT(tb->exp_tck.v == TMO_UNUSED);
	ODP_ASSERT(tb->tmo_buf == ODP_BUFFER_INVALID);
	tim->queue = ODP_QUEUE_INVALID;
	tim->user_ptr = NULL;
}

static inline uint32_t get_next_free(odp_timer *tim)
{
	/* Reusing 'queue' for next free index */
	return _odp_typeval(tim->queue);
}

static inline void set_next_free(odp_timer *tim, uint32_t nf)
{
	ODP_ASSERT(tim->queue == ODP_QUEUE_INVALID);
	/* Reusing 'queue' for next free index */
	tim->queue = _odp_cast_scalar(odp_queue_t, nf);
}

/******************************************************************************
 * odp_timer_pool abstract datatype
 * Inludes alloc and free timer
 *****************************************************************************/

typedef struct odp_timer_pool_s {
/* Put frequently accessed fields in the first cache line */
	odp_atomic_u64_t cur_tick;/* Current tick value */
	uint64_t min_rel_tck;
	uint64_t max_rel_tck;
	tick_buf_t *tick_buf; /* Expiration tick and timeout buffer */
	odp_timer *timers; /* User pointer and queue handle (and lock) */
	odp_atomic_u32_t high_wm;/* High watermark of allocated timers */
	odp_spinlock_t lock;
	uint32_t num_alloc;/* Current number of allocated timers */
	uint32_t first_free;/* 0..max_timers-1 => free timer */
	uint32_t tp_idx;/* Index into timer_pool array */
	odp_timer_pool_param_t param;
	char name[ODP_TIMER_POOL_NAME_LEN];
	odp_shm_t shm;
	timer_t timerid;
	int notify_overrun;
	pthread_t timer_thread; /* pthread_t of timer thread */
	pid_t timer_thread_id; /* gettid() for timer thread */
	int timer_thread_exit; /* request to exit for timer thread */
} odp_timer_pool;

#define MAX_TIMER_POOLS 255 /* Leave one for ODP_TIMER_INVALID */
#define INDEX_BITS 24
static odp_atomic_u32_t num_timer_pools;
static odp_timer_pool *timer_pool[MAX_TIMER_POOLS];

static inline odp_timer_pool *handle_to_tp(odp_timer_t hdl)
{
	uint32_t tp_idx = hdl >> INDEX_BITS;

	if (odp_likely(tp_idx < MAX_TIMER_POOLS)) {
		odp_timer_pool *tp = timer_pool[tp_idx];

		if (odp_likely(tp != NULL))
			return timer_pool[tp_idx];
	}
	ODP_ABORT("Invalid timer handle %#x\n", hdl);
}

static inline uint32_t handle_to_idx(odp_timer_t hdl,
		struct odp_timer_pool_s *tp)
{
	uint32_t idx = hdl & ((1U << INDEX_BITS) - 1U);

	PREFETCH(&tp->tick_buf[idx]);
	if (odp_likely(idx < odp_atomic_load_u32(&tp->high_wm)))
		return idx;
	ODP_ABORT("Invalid timer handle %#x\n", hdl);
}

static inline odp_timer_t tp_idx_to_handle(struct odp_timer_pool_s *tp,
		uint32_t idx)
{
	ODP_ASSERT(idx < (1U << INDEX_BITS));
	return (tp->tp_idx << INDEX_BITS) | idx;
}

/* Forward declarations */
static void itimer_init(odp_timer_pool *tp);
static void itimer_fini(odp_timer_pool *tp);

static odp_timer_pool *odp_timer_pool_new(
	const char *_name,
	const odp_timer_pool_param_t *param)
{
	uint32_t tp_idx = odp_atomic_fetch_add_u32(&num_timer_pools, 1);

	if (odp_unlikely(tp_idx >= MAX_TIMER_POOLS)) {
		/* Restore the previous value */
		odp_atomic_sub_u32(&num_timer_pools, 1);
		__odp_errno = ENFILE; /* Table overflow */
		return NULL;
	}
	size_t sz0 = ODP_ALIGN_ROUNDUP(sizeof(odp_timer_pool),
			ODP_CACHE_LINE_SIZE);
	size_t sz1 = ODP_ALIGN_ROUNDUP(sizeof(tick_buf_t) * param->num_timers,
			ODP_CACHE_LINE_SIZE);
	size_t sz2 = ODP_ALIGN_ROUNDUP(sizeof(odp_timer) * param->num_timers,
			ODP_CACHE_LINE_SIZE);
	odp_shm_t shm = odp_shm_reserve(_name, sz0 + sz1 + sz2,
			ODP_CACHE_LINE_SIZE, ODP_SHM_SW_ONLY);
	if (odp_unlikely(shm == ODP_SHM_INVALID))
		ODP_ABORT("%s: timer pool shm-alloc(%zuKB) failed\n",
			  _name, (sz0 + sz1 + sz2) / 1024);
	odp_timer_pool *tp = (odp_timer_pool *)odp_shm_addr(shm);

	odp_atomic_init_u64(&tp->cur_tick, 0);
	snprintf(tp->name, sizeof(tp->name), "%s", _name);
	tp->shm = shm;
	tp->param = *param;
	tp->min_rel_tck = odp_timer_ns_to_tick(tp, param->min_tmo);
	tp->max_rel_tck = odp_timer_ns_to_tick(tp, param->max_tmo);
	tp->num_alloc = 0;
	odp_atomic_init_u32(&tp->high_wm, 0);
	tp->first_free = 0;
	tp->notify_overrun = 1;
	tp->tick_buf = (void *)((char *)odp_shm_addr(shm) + sz0);
	tp->timers = (void *)((char *)odp_shm_addr(shm) + sz0 + sz1);
	/* Initialize all odp_timer entries */
	uint32_t i;

	for (i = 0; i < tp->param.num_timers; i++) {
		tp->timers[i].queue = ODP_QUEUE_INVALID;
		set_next_free(&tp->timers[i], i + 1);
		tp->timers[i].user_ptr = NULL;
		odp_atomic_init_u64(&tp->tick_buf[i].exp_tck, TMO_UNUSED);
		tp->tick_buf[i].tmo_buf = ODP_BUFFER_INVALID;
	}
	tp->tp_idx = tp_idx;
	odp_spinlock_init(&tp->lock);
	timer_pool[tp_idx] = tp;
	if (tp->param.clk_src == ODP_CLOCK_CPU)
		itimer_init(tp);
	return tp;
}

static void block_sigalarm(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
}

static void stop_timer_thread(odp_timer_pool *tp)
{
	int ret;

	ODP_DBG("stop\n");
	tp->timer_thread_exit = 1;
	ret = pthread_join(tp->timer_thread, NULL);
	if (ret != 0)
		ODP_ABORT("unable to join thread, err %d\n", ret);
}

static void odp_timer_pool_del(odp_timer_pool *tp)
{
	odp_spinlock_lock(&tp->lock);
	timer_pool[tp->tp_idx] = NULL;

	/* Stop timer triggering */
	if (tp->param.clk_src == ODP_CLOCK_CPU)
		itimer_fini(tp);

	stop_timer_thread(tp);

	if (tp->num_alloc != 0) {
		/* It's a programming error to attempt to destroy a */
		/* timer pool which is still in use */
		ODP_ABORT("%s: timers in use\n", tp->name);
	}
	int rc = odp_shm_free(tp->shm);

	if (rc != 0)
		ODP_ABORT("Failed to free shared memory (%d)\n", rc);
}

static inline odp_timer_t timer_alloc(odp_timer_pool *tp,
				      odp_queue_t queue,
				      void *user_ptr)
{
	odp_timer_t hdl;

	odp_spinlock_lock(&tp->lock);
	if (odp_likely(tp->num_alloc < tp->param.num_timers)) {
		tp->num_alloc++;
		/* Remove first unused timer from free list */
		ODP_ASSERT(tp->first_free != tp->param.num_timers);
		uint32_t idx = tp->first_free;
		odp_timer *tim = &tp->timers[idx];

		tp->first_free = get_next_free(tim);
		/* Initialize timer */
		timer_init(tim, &tp->tick_buf[idx], queue, user_ptr);
		if (odp_unlikely(tp->num_alloc >
				 odp_atomic_load_u32(&tp->high_wm)))
			/* Update high_wm last with release model to
			 * ensure timer initialization is visible */
			_odp_atomic_u32_store_mm(&tp->high_wm,
						 tp->num_alloc,
						 _ODP_MEMMODEL_RLS);
		hdl = tp_idx_to_handle(tp, idx);
	} else {
		__odp_errno = ENFILE; /* Reusing file table overflow */
		hdl = ODP_TIMER_INVALID;
	}
	odp_spinlock_unlock(&tp->lock);
	return hdl;
}

static odp_buffer_t timer_cancel(odp_timer_pool *tp,
		uint32_t idx,
		uint64_t new_state);

static inline odp_buffer_t timer_free(odp_timer_pool *tp, uint32_t idx)
{
	odp_timer *tim = &tp->timers[idx];

	/* Free the timer by setting timer state to unused and
	 * grab any timeout buffer */
	odp_buffer_t old_buf = timer_cancel(tp, idx, TMO_UNUSED);

	/* Destroy timer */
	timer_fini(tim, &tp->tick_buf[idx]);

	/* Insert timer into free list */
	odp_spinlock_lock(&tp->lock);
	set_next_free(tim, tp->first_free);
	tp->first_free = idx;
	ODP_ASSERT(tp->num_alloc != 0);
	tp->num_alloc--;
	odp_spinlock_unlock(&tp->lock);

	return old_buf;
}

/******************************************************************************
 * Operations on timers
 * expire/reset/cancel timer
 *****************************************************************************/

static bool timer_reset(uint32_t idx,
		uint64_t abs_tck,
		odp_buffer_t *tmo_buf,
		odp_timer_pool *tp)
{
	bool success = true;
	tick_buf_t *tb = &tp->tick_buf[idx];

	if (tmo_buf == NULL || *tmo_buf == ODP_BUFFER_INVALID) {
#ifdef ODP_ATOMIC_U128
		tick_buf_t new, old;

		do {
			/* Relaxed and non-atomic read of current values */
			old.exp_tck.v = tb->exp_tck.v;
			old.tmo_buf = tb->tmo_buf;
			TB_SET_PAD(old);
			/* Check if there actually is a timeout buffer
			 * present */
			if (old.tmo_buf == ODP_BUFFER_INVALID) {
				/* Cannot reset a timer with neither old nor
				 * new timeout buffer */
				success = false;
				break;
			}
			/* Set up new values */
			new.exp_tck.v = abs_tck;
			new.tmo_buf = old.tmo_buf;
			TB_SET_PAD(new);
			/* Atomic CAS will fail if we experienced torn reads,
			 * retry update sequence until CAS succeeds */
		} while (!_odp_atomic_u128_cmp_xchg_mm(
					(_odp_atomic_u128_t *)tb,
					(_uint128_t *)&old,
					(_uint128_t *)&new,
					_ODP_MEMMODEL_RLS,
					_ODP_MEMMODEL_RLX));
#else
#ifdef __ARM_ARCH
		/* Since barriers are not good for C-A15, we take an
		 * alternative approach using relaxed memory model */
		uint64_t old;
		/* Swap in new expiration tick, get back old tick which
		 * will indicate active/inactive timer state */
		old = _odp_atomic_u64_xchg_mm(&tb->exp_tck, abs_tck,
			_ODP_MEMMODEL_RLX);
		if ((old & TMO_INACTIVE) != 0) {
			/* Timer was inactive (cancelled or expired),
			 * we can't reset a timer without a timeout buffer.
			 * Attempt to restore inactive state, we don't
			 * want this timer to continue as active without
			 * timeout as this will trigger unnecessary and
			 * aborted expiration attempts.
			 * We don't care if we fail, then some other thread
			 * reset or cancelled the timer. Without any
			 * synchronization between the threads, we have a
			 * data race and the behavior is undefined */
			(void)_odp_atomic_u64_cmp_xchg_strong_mm(
					&tb->exp_tck,
					&abs_tck,
					old,
					_ODP_MEMMODEL_RLX,
					_ODP_MEMMODEL_RLX);
			success = false;
		}
#else
		/* Take a related lock */
		while (_odp_atomic_flag_tas(IDX2LOCK(idx)))
			/* While lock is taken, spin using relaxed loads */
			while (_odp_atomic_flag_load(IDX2LOCK(idx)))
				odp_cpu_pause();

		/* Only if there is a timeout buffer can be reset the timer */
		if (odp_likely(tb->tmo_buf != ODP_BUFFER_INVALID)) {
			/* Write the new expiration tick */
			tb->exp_tck.v = abs_tck;
		} else {
			/* Cannot reset a timer with neither old nor new
			 * timeout buffer */
			success = false;
		}

		/* Release the lock */
		_odp_atomic_flag_clear(IDX2LOCK(idx));
#endif
#endif
	} else {
		/* We have a new timeout buffer which replaces any old one */
		/* Fill in some (constant) header fields for timeout events */
		if (odp_event_type(odp_buffer_to_event(*tmo_buf)) ==
		    ODP_EVENT_TIMEOUT) {
			/* Convert from buffer to timeout hdr */
			odp_timeout_hdr_t *tmo_hdr =
				timeout_hdr_from_buf(*tmo_buf);
			tmo_hdr->timer = tp_idx_to_handle(tp, idx);
			tmo_hdr->user_ptr = tp->timers[idx].user_ptr;
			/* expiration field filled in when timer expires */
		}
		/* Else ignore buffers of other types */
		odp_buffer_t old_buf = ODP_BUFFER_INVALID;
#ifdef ODP_ATOMIC_U128
		tick_buf_t new, old;

		new.exp_tck.v = abs_tck;
		new.tmo_buf = *tmo_buf;
		TB_SET_PAD(new);
		/* We are releasing the new timeout buffer to some other
		 * thread */
		_odp_atomic_u128_xchg_mm((_odp_atomic_u128_t *)tb,
					 (_uint128_t *)&new,
					 (_uint128_t *)&old,
					 _ODP_MEMMODEL_ACQ_RLS);
		old_buf = old.tmo_buf;
#else
		/* Take a related lock */
		while (_odp_atomic_flag_tas(IDX2LOCK(idx)))
			/* While lock is taken, spin using relaxed loads */
			while (_odp_atomic_flag_load(IDX2LOCK(idx)))
				odp_cpu_pause();

		/* Swap in new buffer, save any old buffer */
		old_buf = tb->tmo_buf;
		tb->tmo_buf = *tmo_buf;

		/* Write the new expiration tick */
		tb->exp_tck.v = abs_tck;

		/* Release the lock */
		_odp_atomic_flag_clear(IDX2LOCK(idx));
#endif
		/* Return old timeout buffer */
		*tmo_buf = old_buf;
	}
	return success;
}

static odp_buffer_t timer_cancel(odp_timer_pool *tp,
				 uint32_t idx,
				 uint64_t new_state)
{
	tick_buf_t *tb = &tp->tick_buf[idx];
	odp_buffer_t old_buf;

#ifdef ODP_ATOMIC_U128
	tick_buf_t new, old;
	/* Update the timer state (e.g. cancel the current timeout) */
	new.exp_tck.v = new_state;
	/* Swap out the old buffer */
	new.tmo_buf = ODP_BUFFER_INVALID;
	TB_SET_PAD(new);
	_odp_atomic_u128_xchg_mm((_odp_atomic_u128_t *)tb,
				 (_uint128_t *)&new, (_uint128_t *)&old,
				 _ODP_MEMMODEL_RLX);
	old_buf = old.tmo_buf;
#else
	/* Take a related lock */
	while (_odp_atomic_flag_tas(IDX2LOCK(idx)))
		/* While lock is taken, spin using relaxed loads */
		while (_odp_atomic_flag_load(IDX2LOCK(idx)))
			odp_cpu_pause();

	/* Update the timer state (e.g. cancel the current timeout) */
	tb->exp_tck.v = new_state;

	/* Swap out the old buffer */
	old_buf = tb->tmo_buf;
	tb->tmo_buf = ODP_BUFFER_INVALID;

	/* Release the lock */
	_odp_atomic_flag_clear(IDX2LOCK(idx));
#endif
	/* Return the old buffer */
	return old_buf;
}

static unsigned timer_expire(odp_timer_pool *tp, uint32_t idx, uint64_t tick)
{
	odp_timer *tim = &tp->timers[idx];
	tick_buf_t *tb = &tp->tick_buf[idx];
	odp_buffer_t tmo_buf = ODP_BUFFER_INVALID;
	uint64_t exp_tck;
#ifdef ODP_ATOMIC_U128
	/* Atomic re-read for correctness */
	exp_tck = _odp_atomic_u64_load_mm(&tb->exp_tck, _ODP_MEMMODEL_RLX);
	/* Re-check exp_tck */
	if (odp_likely(exp_tck <= tick)) {
		/* Attempt to grab timeout buffer, replace with inactive timer
		 * and invalid buffer */
		tick_buf_t new, old;

		old.exp_tck.v = exp_tck;
		old.tmo_buf = tb->tmo_buf;
		TB_SET_PAD(old);
		/* Set the inactive/expired bit keeping the expiration tick so
		 * that we can check against the expiration tick of the timeout
		 * when it is received */
		new.exp_tck.v = exp_tck | TMO_INACTIVE;
		new.tmo_buf = ODP_BUFFER_INVALID;
		TB_SET_PAD(new);
		int succ = _odp_atomic_u128_cmp_xchg_mm(
				(_odp_atomic_u128_t *)tb,
				(_uint128_t *)&old, (_uint128_t *)&new,
				_ODP_MEMMODEL_RLS, _ODP_MEMMODEL_RLX);
		if (succ)
			tmo_buf = old.tmo_buf;
		/* Else CAS failed, something changed => skip timer
		 * this tick, it will be checked again next tick */
	}
	/* Else false positive, ignore */
#else
	/* Take a related lock */
	while (_odp_atomic_flag_tas(IDX2LOCK(idx)))
		/* While lock is taken, spin using relaxed loads */
		while (_odp_atomic_flag_load(IDX2LOCK(idx)))
			odp_cpu_pause();
	/* Proper check for timer expired */
	exp_tck = tb->exp_tck.v;
	if (odp_likely(exp_tck <= tick)) {
		/* Verify that there is a timeout buffer */
		if (odp_likely(tb->tmo_buf != ODP_BUFFER_INVALID)) {
			/* Grab timeout buffer, replace with inactive timer
			 * and invalid buffer */
			tmo_buf = tb->tmo_buf;
			tb->tmo_buf = ODP_BUFFER_INVALID;
			/* Set the inactive/expired bit keeping the expiration
			 * tick so that we can check against the expiration
			 * tick of the timeout when it is received */
			tb->exp_tck.v |= TMO_INACTIVE;
		}
		/* Else somehow active timer without user buffer */
	}
	/* Else false positive, ignore */
	/* Release the lock */
	_odp_atomic_flag_clear(IDX2LOCK(idx));
#endif
	if (odp_likely(tmo_buf != ODP_BUFFER_INVALID)) {
		/* Fill in expiration tick for timeout events */
		if (odp_event_type(odp_buffer_to_event(tmo_buf)) ==
		    ODP_EVENT_TIMEOUT) {
			/* Convert from buffer to timeout hdr */
			odp_timeout_hdr_t *tmo_hdr =
				timeout_hdr_from_buf(tmo_buf);
			tmo_hdr->expiration = exp_tck;
			/* timer and user_ptr fields filled in when timer
			 * was set */
		}
		/* Else ignore events of other types */
		/* Post the timeout to the destination queue */
		int rc = odp_queue_enq(tim->queue,
				       odp_buffer_to_event(tmo_buf));
		if (odp_unlikely(rc != 0)) {
			odp_buffer_free(tmo_buf);
			ODP_ABORT("Failed to enqueue timeout buffer (%d)\n",
				  rc);
		}
		return 1;
	}

	/* Else false positive, ignore */
	return 0;
}

static unsigned odp_timer_pool_expire(odp_timer_pool_t tpid, uint64_t tick)
{
	tick_buf_t *array = &tpid->tick_buf[0];
	uint32_t high_wm = _odp_atomic_u32_load_mm(&tpid->high_wm,
			_ODP_MEMMODEL_ACQ);
	unsigned nexp = 0;
	uint32_t i;

	ODP_ASSERT(high_wm <= tpid->param.num_timers);
	for (i = 0; i < high_wm;) {
#ifdef __ARM_ARCH
		/* As a rare occurrence, we can outsmart the HW prefetcher
		 * and the compiler (GCC -fprefetch-loop-arrays) with some
		 * tuned manual prefetching (32x16=512B ahead), seems to
		 * give 30% better performance on ARM C-A15 */
		PREFETCH(&array[i + 32]);
#endif
		/* Non-atomic read for speed */
		uint64_t exp_tck = array[i++].exp_tck.v;

		if (odp_unlikely(exp_tck <= tick)) {
			/* Attempt to expire timer */
			nexp += timer_expire(tpid, i - 1, tick);
		}
	}
	return nexp;
}

/******************************************************************************
 * POSIX timer support
 * Functions that use Linux/POSIX per-process timers and related facilities
 *****************************************************************************/

static void timer_notify(odp_timer_pool *tp)
{
	int overrun;
	int64_t prev_tick;

	if (tp->notify_overrun) {
		overrun = timer_getoverrun(tp->timerid);
		if (overrun) {
			ODP_ERR("\n\t%d ticks overrun on timer pool \"%s\", timer resolution too high\n",
				overrun, tp->name);
			tp->notify_overrun = 0;
		}
	}

#ifdef __ARM_ARCH
	odp_timer *array = &tp->timers[0];
	uint32_t i;
	/* Prefetch initial cache lines (match 32 above) */
	for (i = 0; i < 32; i += ODP_CACHE_LINE_SIZE / sizeof(array[0]))
		PREFETCH(&array[i]);
#endif
	prev_tick = odp_atomic_fetch_inc_u64(&tp->cur_tick);

	/* Scan timer array, looking for timers to expire */
	(void)odp_timer_pool_expire(tp, prev_tick);

	/* Else skip scan of timers. cur_tick was updated and next itimer
	 * invocation will process older expiration ticks as well */
}

static void *timer_thread(void *arg)
{
	odp_timer_pool *tp = (odp_timer_pool *)arg;
	sigset_t sigset;
	int ret;
	struct timespec tmo;
	siginfo_t si;

	tp->timer_thread_id = (pid_t)syscall(SYS_gettid);

	tmo.tv_sec = 0;
	tmo.tv_nsec = ODP_TIME_MSEC_IN_NS * 100;

	sigemptyset(&sigset);
	/* unblock sigalarm in this thread */
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	sigaddset(&sigset, SIGALRM);

	while (1) {
		ret = sigtimedwait(&sigset, &si, &tmo);
		if (tp->timer_thread_exit) {
			tp->timer_thread_id = 0;
			return NULL;
		}
		if (ret > 0)
			timer_notify(tp);
	}

	return NULL;
}

static void itimer_init(odp_timer_pool *tp)
{
	struct sigevent   sigev;
	struct itimerspec ispec;
	uint64_t res, sec, nsec;
	int ret;

	ODP_DBG("Creating POSIX timer for timer pool %s, period %"
		PRIu64" ns\n", tp->name, tp->param.res_ns);

	tp->timer_thread_id = 0;
	ret = pthread_create(&tp->timer_thread, NULL, timer_thread, tp);
	if (ret)
		ODP_ABORT("unable to create timer thread\n");

	/* wait thread set tp->timer_thread_id */
	do {
		sched_yield();
	} while (tp->timer_thread_id == 0);

	memset(&sigev, 0, sizeof(sigev));
	sigev.sigev_notify          = SIGEV_THREAD_ID;
	sigev.sigev_value.sival_ptr = tp;
	sigev._sigev_un._tid = tp->timer_thread_id;
	sigev.sigev_signo = SIGALRM;

	if (timer_create(CLOCK_MONOTONIC, &sigev, &tp->timerid))
		ODP_ABORT("timer_create() returned error %s\n",
			  strerror(errno));

	res  = tp->param.res_ns;
	sec  = res / ODP_TIME_SEC_IN_NS;
	nsec = res - sec * ODP_TIME_SEC_IN_NS;

	memset(&ispec, 0, sizeof(ispec));
	ispec.it_interval.tv_sec  = (time_t)sec;
	ispec.it_interval.tv_nsec = (long)nsec;
	ispec.it_value.tv_sec     = (time_t)sec;
	ispec.it_value.tv_nsec    = (long)nsec;

	if (timer_settime(tp->timerid, 0, &ispec, NULL))
		ODP_ABORT("timer_settime() returned error %s\n",
			  strerror(errno));
}

static void itimer_fini(odp_timer_pool *tp)
{
	if (timer_delete(tp->timerid) != 0)
		ODP_ABORT("timer_delete() returned error %s\n",
			  strerror(errno));
}

/******************************************************************************
 * Public API functions
 * Some parameter checks and error messages
 * No modificatios of internal state
 *****************************************************************************/
odp_timer_pool_t
odp_timer_pool_create(const char *name,
		      const odp_timer_pool_param_t *param)
{
	/* Verify that buffer pool can be used for timeouts */
	/* Verify that we have a valid (non-zero) timer resolution */
	if (param->res_ns == 0) {
		__odp_errno = EINVAL;
		return NULL;
	}
	odp_timer_pool_t tp = odp_timer_pool_new(name, param);
	return tp;
}

void odp_timer_pool_start(void)
{
	/* Nothing to do here, timer pools are started by the create call */
}

void odp_timer_pool_destroy(odp_timer_pool_t tpid)
{
	odp_timer_pool_del(tpid);
}

uint64_t odp_timer_tick_to_ns(odp_timer_pool_t tpid, uint64_t ticks)
{
	return ticks * tpid->param.res_ns;
}

uint64_t odp_timer_ns_to_tick(odp_timer_pool_t tpid, uint64_t ns)
{
	return (uint64_t)(ns / tpid->param.res_ns);
}

uint64_t odp_timer_current_tick(odp_timer_pool_t tpid)
{
	/* Relaxed atomic read for lowest overhead */
	return odp_atomic_load_u64(&tpid->cur_tick);
}

int odp_timer_pool_info(odp_timer_pool_t tpid,
			odp_timer_pool_info_t *buf)
{
	buf->param = tpid->param;
	buf->cur_timers = tpid->num_alloc;
	buf->hwm_timers = odp_atomic_load_u32(&tpid->high_wm);
	buf->name = tpid->name;
	return 0;
}

odp_timer_t odp_timer_alloc(odp_timer_pool_t tpid,
			    odp_queue_t queue,
			    void *user_ptr)
{
	if (odp_unlikely(queue == ODP_QUEUE_INVALID))
		ODP_ABORT("%s: Invalid queue handle\n", tpid->name);
	/* We don't care about the validity of user_ptr because we will not
	 * attempt to dereference it */
	odp_timer_t hdl = timer_alloc(tpid, queue, user_ptr);

	if (odp_likely(hdl != ODP_TIMER_INVALID)) {
		/* Success */
		return hdl;
	}
	/* errno set by timer_alloc() */
	return ODP_TIMER_INVALID;
}

odp_event_t odp_timer_free(odp_timer_t hdl)
{
	odp_timer_pool *tp = handle_to_tp(hdl);
	uint32_t idx = handle_to_idx(hdl, tp);
	odp_buffer_t old_buf = timer_free(tp, idx);

	return odp_buffer_to_event(old_buf);
}

int odp_timer_set_abs(odp_timer_t hdl,
		      uint64_t abs_tck,
		      odp_event_t *tmo_ev)
{
	odp_timer_pool *tp = handle_to_tp(hdl);
	uint32_t idx = handle_to_idx(hdl, tp);
	uint64_t cur_tick = odp_atomic_load_u64(&tp->cur_tick);

	if (odp_unlikely(abs_tck < cur_tick + tp->min_rel_tck))
		return ODP_TIMER_TOOEARLY;
	if (odp_unlikely(abs_tck > cur_tick + tp->max_rel_tck))
		return ODP_TIMER_TOOLATE;
	if (timer_reset(idx, abs_tck, (odp_buffer_t *)tmo_ev, tp))
		return ODP_TIMER_SUCCESS;
	else
		return ODP_TIMER_NOEVENT;
}

int odp_timer_set_rel(odp_timer_t hdl,
		      uint64_t rel_tck,
		      odp_event_t *tmo_ev)
{
	odp_timer_pool *tp = handle_to_tp(hdl);
	uint32_t idx = handle_to_idx(hdl, tp);
	uint64_t abs_tck = odp_atomic_load_u64(&tp->cur_tick) + rel_tck;

	if (odp_unlikely(rel_tck < tp->min_rel_tck))
		return ODP_TIMER_TOOEARLY;
	if (odp_unlikely(rel_tck > tp->max_rel_tck))
		return ODP_TIMER_TOOLATE;
	if (timer_reset(idx, abs_tck, (odp_buffer_t *)tmo_ev, tp))
		return ODP_TIMER_SUCCESS;
	else
		return ODP_TIMER_NOEVENT;
}

int odp_timer_cancel(odp_timer_t hdl, odp_event_t *tmo_ev)
{
	odp_timer_pool *tp = handle_to_tp(hdl);
	uint32_t idx = handle_to_idx(hdl, tp);
	/* Set the expiration tick of the timer to TMO_INACTIVE */
	odp_buffer_t old_buf = timer_cancel(tp, idx, TMO_INACTIVE);

	if (old_buf != ODP_BUFFER_INVALID) {
		*tmo_ev = odp_buffer_to_event(old_buf);
		return 0; /* Active timer cancelled, timeout returned */
	} else {
		return -1; /* Timer already expired, no timeout returned */
	}
}

odp_timeout_t odp_timeout_from_event(odp_event_t ev)
{
	/* This check not mandated by the API specification */
	if (odp_event_type(ev) != ODP_EVENT_TIMEOUT)
		ODP_ABORT("Event not a timeout");
	return (odp_timeout_t)ev;
}

odp_event_t odp_timeout_to_event(odp_timeout_t tmo)
{
	return (odp_event_t)tmo;
}

int odp_timeout_fresh(odp_timeout_t tmo)
{
	const odp_timeout_hdr_t *hdr = timeout_hdr(tmo);
	odp_timer_t hdl = hdr->timer;
	odp_timer_pool *tp = handle_to_tp(hdl);
	uint32_t idx = handle_to_idx(hdl, tp);
	tick_buf_t *tb = &tp->tick_buf[idx];
	uint64_t exp_tck = odp_atomic_load_u64(&tb->exp_tck);

	/* Return true if the timer still has the same expiration tick
	 * (ignoring the inactive/expired bit) as the timeout */
	return hdr->expiration == (exp_tck & ~TMO_INACTIVE);
}

odp_timer_t odp_timeout_timer(odp_timeout_t tmo)
{
	return timeout_hdr(tmo)->timer;
}

uint64_t odp_timeout_tick(odp_timeout_t tmo)
{
	return timeout_hdr(tmo)->expiration;
}

void *odp_timeout_user_ptr(odp_timeout_t tmo)
{
	return timeout_hdr(tmo)->user_ptr;
}

odp_timeout_t odp_timeout_alloc(odp_pool_t pool)
{
	odp_buffer_t buf = odp_buffer_alloc(pool);

	if (odp_unlikely(buf == ODP_BUFFER_INVALID))
		return ODP_TIMEOUT_INVALID;
	return odp_timeout_from_event(odp_buffer_to_event(buf));
}

void odp_timeout_free(odp_timeout_t tmo)
{
	odp_event_t ev = odp_timeout_to_event(tmo);

	odp_buffer_free(odp_buffer_from_event(ev));
}

int odp_timer_init_global(void)
{
#ifndef ODP_ATOMIC_U128
	uint32_t i;

	for (i = 0; i < NUM_LOCKS; i++)
		_odp_atomic_flag_clear(&locks[i]);
#else
	ODP_DBG("Using lock-less timer implementation\n");
#endif
	odp_atomic_init_u32(&num_timer_pools, 0);

	block_sigalarm();

	return 0;
}

int odp_timer_term_global(void)
{
	return 0;
}
