/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 */

/* For rand_r and nanosleep */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <time.h>
#include <odp.h>
#include "odp_cunit_common.h"
#include "test_debug.h"
#include "timer.h"

/** @private Timeout range in milliseconds (ms) */
#define RANGE_MS 2000

/** @private Number of timers per thread */
#define NTIMERS 2000

/** @private Barrier for thread synchronisation */
static odp_barrier_t test_barrier;

/** @private Timeout pool handle used by all threads */
static odp_pool_t tbp;

/** @private Timer pool handle used by all threads */
static odp_timer_pool_t tp;

/** @private Count of timeouts delivered too late */
static odp_atomic_u32_t ndelivtoolate;

/** @private Sum of all allocated timers from all threads. Thread-local
 * caches may make this number lower than the capacity of the pool  */
static odp_atomic_u32_t timers_allocated;

/** @private min() function */
static int min(int a, int b)
{
	return a < b ? a : b;
}

/* @private Timer helper structure */
struct test_timer {
	odp_timer_t tim; /* Timer handle */
	odp_event_t ev;  /* Timeout event */
	odp_event_t ev2; /* Copy of event handle */
	uint64_t tick; /* Expiration tick or TICK_INVALID */
};

#define TICK_INVALID (~(uint64_t)0)

void timer_test_timeout_pool_alloc(void)
{
	odp_pool_t pool;
	const int num = 3;
	odp_timeout_t tmo[num];
	odp_event_t ev;
	int index;
	char wrong_type = 0;
	odp_pool_param_t params = {
			.tmo = {
				.num   = num,
			},
			.type  = ODP_POOL_TIMEOUT,
	};

	pool = odp_pool_create("timeout_pool_alloc", &params);
	odp_pool_print(pool);

	/* Try to allocate num items from the pool */
	for (index = 0; index < num; index++) {
		tmo[index] = odp_timeout_alloc(pool);

		if (tmo[index] == ODP_TIMEOUT_INVALID)
			break;

		ev = odp_timeout_to_event(tmo[index]);
		if (odp_event_type(ev) != ODP_EVENT_TIMEOUT)
			wrong_type = 1;
	}

	/* Check that the pool had at least num items */
	CU_ASSERT(index == num);
	/* index points out of buffer[] or it point to an invalid buffer */
	index--;

	/* Check that the pool had correct buffers */
	CU_ASSERT(wrong_type == 0);

	for (; index >= 0; index--)
		odp_timeout_free(tmo[index]);

	CU_ASSERT(odp_pool_destroy(pool) == 0);
}

void timer_test_timeout_pool_free(void)
{
	odp_pool_t pool;
	odp_timeout_t tmo;
	odp_pool_param_t params = {
			.tmo = {
				.num   = 1,
			},
			.type  = ODP_POOL_TIMEOUT,
	};

	pool = odp_pool_create("timeout_pool_free", &params);
	odp_pool_print(pool);

	/* Allocate the only timeout from the pool */
	tmo = odp_timeout_alloc(pool);
	CU_ASSERT_FATAL(tmo != ODP_TIMEOUT_INVALID);

	/* Pool should have only one timeout */
	CU_ASSERT_FATAL(odp_timeout_alloc(pool) == ODP_TIMEOUT_INVALID)

	odp_timeout_free(tmo);

	/* Check that the timeout was returned back to the pool */
	tmo = odp_timeout_alloc(pool);
	CU_ASSERT_FATAL(tmo != ODP_TIMEOUT_INVALID);

	odp_timeout_free(tmo);
	CU_ASSERT(odp_pool_destroy(pool) == 0);
}

void timer_test_odp_timer_cancel(void)
{
	odp_pool_t pool;
	odp_pool_param_t params;
	odp_timer_pool_param_t tparam;
	odp_timer_pool_t tp;
	odp_queue_t queue;
	odp_timer_t tim;
	odp_event_t ev;
	odp_timeout_t tmo;
	odp_timer_set_t rc;
	uint64_t tick;

	params.tmo.num = 1;
	params.type    = ODP_POOL_TIMEOUT;
	pool = odp_pool_create("tmo_pool_for_cancel", &params);

	if (pool == ODP_POOL_INVALID)
		CU_FAIL_FATAL("Timeout pool create failed");

	tparam.res_ns     = 100 * ODP_TIME_MSEC_IN_NS;
	tparam.min_tmo    = 1   * ODP_TIME_SEC_IN_NS;
	tparam.max_tmo    = 10  * ODP_TIME_SEC_IN_NS;
	tparam.num_timers = 1;
	tparam.priv       = 0;
	tparam.clk_src    = ODP_CLOCK_CPU;
	tp = odp_timer_pool_create("timer_pool0", &tparam);
	if (tp == ODP_TIMER_POOL_INVALID)
		CU_FAIL_FATAL("Timer pool create failed");

	/* Start all created timer pools */
	odp_timer_pool_start();

	queue = odp_queue_create("timer_queue", NULL);
	if (queue == ODP_QUEUE_INVALID)
		CU_FAIL_FATAL("Queue create failed");

	#define USER_PTR ((void *)0xdead)
	tim = odp_timer_alloc(tp, queue, USER_PTR);
	if (tim == ODP_TIMER_INVALID)
		CU_FAIL_FATAL("Failed to allocate timer");

	ev = odp_timeout_to_event(odp_timeout_alloc(pool));
	if (ev == ODP_EVENT_INVALID)
		CU_FAIL_FATAL("Failed to allocate timeout");

	tick = odp_timer_ns_to_tick(tp, 2 * ODP_TIME_SEC_IN_NS);

	rc = odp_timer_set_rel(tim, tick, &ev);
	if (rc != ODP_TIMER_SUCCESS)
		CU_FAIL_FATAL("Failed to set timer (relative time)");

	ev = ODP_EVENT_INVALID;
	if (odp_timer_cancel(tim, &ev) != 0)
		CU_FAIL_FATAL("Failed to cancel timer (relative time)");

	if (ev == ODP_EVENT_INVALID)
		CU_FAIL_FATAL("Cancel did not return event");

	tmo = odp_timeout_from_event(ev);
	if (tmo == ODP_TIMEOUT_INVALID)
		CU_FAIL_FATAL("Cancel did not return timeout");

	if (odp_timeout_timer(tmo) != tim)
		CU_FAIL("Cancel invalid tmo.timer");

	if (odp_timeout_user_ptr(tmo) != USER_PTR)
		CU_FAIL("Cancel invalid tmo.user_ptr");

	odp_timeout_free(tmo);

	ev = odp_timer_free(tim);
	if (ev != ODP_EVENT_INVALID)
		CU_FAIL_FATAL("Free returned event");

	odp_timer_pool_destroy(tp);

	if (odp_queue_destroy(queue) != 0)
		CU_FAIL_FATAL("Failed to destroy queue");

	if (odp_pool_destroy(pool) != 0)
		CU_FAIL_FATAL("Failed to destroy pool");
}

/* @private Handle a received (timeout) event */
static void handle_tmo(odp_event_t ev, bool stale, uint64_t prev_tick)
{
	CU_ASSERT_FATAL(ev != ODP_EVENT_INVALID); /* Internal error */
	if (odp_event_type(ev) != ODP_EVENT_TIMEOUT) {
		/* Not a timeout event */
		CU_FAIL("Unexpected event type received");
		return;
	}
	/* Read the metadata from the timeout */
	odp_timeout_t tmo = odp_timeout_from_event(ev);
	odp_timer_t tim = odp_timeout_timer(tmo);
	uint64_t tick = odp_timeout_tick(tmo);
	struct test_timer *ttp = odp_timeout_user_ptr(tmo);

	if (tim == ODP_TIMER_INVALID)
		CU_FAIL("odp_timeout_timer() invalid timer");
	if (!ttp)
		CU_FAIL("odp_timeout_user_ptr() null user ptr");

	if (ttp && ttp->ev2 != ev)
		CU_FAIL("odp_timeout_user_ptr() wrong user ptr");
	if (ttp && ttp->tim != tim)
		CU_FAIL("odp_timeout_timer() wrong timer");
	if (stale) {
		if (odp_timeout_fresh(tmo))
			CU_FAIL("Wrong status (fresh) for stale timeout");
		/* Stale timeout => local timer must have invalid tick */
		if (ttp && ttp->tick != TICK_INVALID)
			CU_FAIL("Stale timeout for active timer");
	} else {
		if (!odp_timeout_fresh(tmo))
			CU_FAIL("Wrong status (stale) for fresh timeout");
		/* Fresh timeout => local timer must have matching tick */
		if (ttp && ttp->tick != tick) {
			LOG_DBG("Wrong tick: expected %" PRIu64 " actual %" PRIu64 "\n",
				ttp->tick, tick);
			CU_FAIL("odp_timeout_tick() wrong tick");
		}
		/* Check that timeout was delivered 'timely' */
		if (tick > odp_timer_current_tick(tp))
			CU_FAIL("Timeout delivered early");
		if (tick < prev_tick) {
			LOG_DBG("Too late tick: %" PRIu64 " prev_tick %" PRIu64"\n",
				tick, prev_tick);
			/* We don't report late timeouts using CU_FAIL */
			odp_atomic_inc_u32(&ndelivtoolate);
		}
	}

	if (ttp) {
		/* Internal error */
		CU_ASSERT_FATAL(ttp->ev == ODP_EVENT_INVALID);
		ttp->ev = ev;
	}
}

/* @private Worker thread entrypoint which performs timer alloc/set/cancel/free
 * tests */
static void *worker_entrypoint(void *arg TEST_UNUSED)
{
	int thr = odp_thread_id();
	uint32_t i, allocated;
	unsigned seed = thr;
	int rc;

	odp_queue_t queue = odp_queue_create("timer_queue", NULL);
	if (queue == ODP_QUEUE_INVALID)
		CU_FAIL_FATAL("Queue create failed");

	struct test_timer *tt = malloc(sizeof(struct test_timer) * NTIMERS);
	if (!tt)
		CU_FAIL_FATAL("malloc failed");

	/* Prepare all timers */
	for (i = 0; i < NTIMERS; i++) {
		tt[i].ev = odp_timeout_to_event(odp_timeout_alloc(tbp));
		if (tt[i].ev == ODP_EVENT_INVALID) {
			LOG_DBG("Failed to allocate timeout (%d/%d)\n",
				i, NTIMERS);
			break;
		}
		tt[i].tim = odp_timer_alloc(tp, queue, &tt[i]);
		if (tt[i].tim == ODP_TIMER_INVALID) {
			LOG_DBG("Failed to allocate timer (%d/%d)\n",
				i, NTIMERS);
			odp_timeout_free(tt[i].ev);
			break;
		}
		tt[i].ev2 = tt[i].ev;
		tt[i].tick = TICK_INVALID;
	}
	allocated = i;
	odp_atomic_fetch_add_u32(&timers_allocated, allocated);

	odp_barrier_wait(&test_barrier);

	/* Initial set all timers with a random expiration time */
	uint32_t nset = 0;
	for (i = 0; i < allocated; i++) {
		uint64_t tck = odp_timer_current_tick(tp) + 1 +
			       odp_timer_ns_to_tick(tp,
						    (rand_r(&seed) % RANGE_MS)
						    * 1000000ULL);
		odp_timer_set_t rc;
		rc = odp_timer_set_abs(tt[i].tim, tck, &tt[i].ev);
		if (rc != ODP_TIMER_SUCCESS) {
			CU_FAIL("Failed to set timer");
		} else {
			tt[i].tick = tck;
			nset++;
		}
	}

	/* Step through wall time, 1ms at a time and check for expired timers */
	uint32_t nrcv = 0;
	uint32_t nreset = 0;
	uint32_t ncancel = 0;
	uint32_t ntoolate = 0;
	uint32_t ms;
	uint64_t prev_tick = odp_timer_current_tick(tp);

	for (ms = 0; ms < 7 * RANGE_MS / 10; ms++) {
		odp_event_t ev;
		while ((ev = odp_queue_deq(queue)) != ODP_EVENT_INVALID) {
			/* Subtract one from prev_tick to allow for timeouts
			 * to be delivered a tick late */
			handle_tmo(ev, false, prev_tick - 1);
			nrcv++;
		}
		prev_tick = odp_timer_current_tick(tp);
		i = rand_r(&seed) % allocated;
		if (tt[i].ev == ODP_EVENT_INVALID &&
		    (rand_r(&seed) % 2 == 0)) {
			/* Timer active, cancel it */
			rc = odp_timer_cancel(tt[i].tim, &tt[i].ev);
			if (rc != 0)
				/* Cancel failed, timer already expired */
				ntoolate++;
			tt[i].tick = TICK_INVALID;
			ncancel++;
		} else {
			if (tt[i].ev != ODP_EVENT_INVALID)
				/* Timer inactive => set */
				nset++;
			else
				/* Timer active => reset */
				nreset++;
			uint64_t tck = 1 + odp_timer_ns_to_tick(tp,
				       (rand_r(&seed) % RANGE_MS) * 1000000ULL);
			odp_timer_set_t rc;
			uint64_t cur_tick;
			/* Loop until we manage to read cur_tick and set a
			 * relative timer in the same tick */
			do {
				cur_tick = odp_timer_current_tick(tp);
				rc = odp_timer_set_rel(tt[i].tim,
						       tck, &tt[i].ev);
			} while (cur_tick != odp_timer_current_tick(tp));
			if (rc == ODP_TIMER_TOOEARLY ||
			    rc == ODP_TIMER_TOOLATE) {
				CU_FAIL("Failed to set timer (tooearly/toolate)");
			} else if (rc != ODP_TIMER_SUCCESS) {
				/* Set/reset failed, timer already expired */
				ntoolate++;
			} else if (rc == ODP_TIMER_SUCCESS) {
				/* Save expected expiration tick on success */
				tt[i].tick = cur_tick + tck;
			}
		}
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000; /* 1ms */
		if (nanosleep(&ts, NULL) < 0)
			CU_FAIL_FATAL("nanosleep failed");
	}

	/* Cancel and free all timers */
	uint32_t nstale = 0;
	for (i = 0; i < allocated; i++) {
		(void)odp_timer_cancel(tt[i].tim, &tt[i].ev);
		tt[i].tick = TICK_INVALID;
		if (tt[i].ev == ODP_EVENT_INVALID)
			/* Cancel too late, timer already expired and
			 * timeout enqueued */
			nstale++;
		if (odp_timer_free(tt[i].tim) != ODP_EVENT_INVALID)
			CU_FAIL("odp_timer_free");
	}

	LOG_DBG("Thread %u: %" PRIu32 " timers set\n", thr, nset);
	LOG_DBG("Thread %u: %" PRIu32 " timers reset\n", thr, nreset);
	LOG_DBG("Thread %u: %" PRIu32 " timers cancelled\n", thr, ncancel);
	LOG_DBG("Thread %u: %" PRIu32 " timers reset/cancelled too late\n",
		thr, ntoolate);
	LOG_DBG("Thread %u: %" PRIu32 " timeouts received\n", thr, nrcv);
	LOG_DBG("Thread %u: %" PRIu32 " stale timeout(s) after odp_timer_free()\n",
		thr, nstale);

	/* Delay some more to ensure timeouts for expired timers can be
	 * received. Can not use busy loop here to make background timer
	 * thread finish their work. */
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = (3 * RANGE_MS / 10 + 50) * ODP_TIME_MSEC_IN_NS;
	if (nanosleep(&ts, NULL) < 0)
		CU_FAIL_FATAL("nanosleep failed");

	while (nstale != 0) {
		odp_event_t ev = odp_queue_deq(queue);
		if (ev != ODP_EVENT_INVALID) {
			handle_tmo(ev, true, 0/*Dont' care for stale tmo's*/);
			nstale--;
		} else {
			CU_FAIL("Failed to receive stale timeout");
			break;
		}
	}
	/* Check if there any more (unexpected) events */
	odp_event_t ev = odp_queue_deq(queue);
	if (ev != ODP_EVENT_INVALID)
		CU_FAIL("Unexpected event received");

	rc = odp_queue_destroy(queue);
	CU_ASSERT(rc == 0);
	for (i = 0; i < allocated; i++) {
		if (tt[i].ev != ODP_EVENT_INVALID)
			odp_event_free(tt[i].ev);
	}

	free(tt);
	LOG_DBG("Thread %u: exiting\n", thr);
	return NULL;
}

/* @private Timer test case entrypoint */
void timer_test_odp_timer_all(void)
{
	int rc;
	odp_pool_param_t params;
	odp_timer_pool_param_t tparam;
	/* Reserve at least one core for running other processes so the timer
	 * test hopefully can run undisturbed and thus get better timing
	 * results. */
	int num_workers = min(odp_cpu_count() - 1, MAX_WORKERS);
	/* On a single-CPU machine run at least one thread */
	if (num_workers < 1)
		num_workers = 1;

	/* Create timeout pools */
	params.tmo.num = (NTIMERS + 1) * num_workers;
	params.type    = ODP_POOL_TIMEOUT;
	tbp = odp_pool_create("tmo_pool", &params);
	if (tbp == ODP_POOL_INVALID)
		CU_FAIL_FATAL("Timeout pool create failed");

#define NAME "timer_pool"
#define RES (10 * ODP_TIME_MSEC_IN_NS / 3)
#define MIN (10 * ODP_TIME_MSEC_IN_NS / 3)
#define MAX (1000000 * ODP_TIME_MSEC_IN_NS)
	/* Create a timer pool */
	tparam.res_ns = RES;
	tparam.min_tmo = MIN;
	tparam.max_tmo = MAX;
	tparam.num_timers = num_workers * NTIMERS;
	tparam.priv = 0;
	tparam.clk_src = ODP_CLOCK_CPU;
	tp = odp_timer_pool_create(NAME, &tparam);
	if (tp == ODP_TIMER_POOL_INVALID)
		CU_FAIL_FATAL("Timer pool create failed");

	/* Start all created timer pools */
	odp_timer_pool_start();

	odp_timer_pool_info_t tpinfo;
	if (odp_timer_pool_info(tp, &tpinfo) != 0)
		CU_FAIL("odp_timer_pool_info");
	CU_ASSERT(strcmp(tpinfo.name, NAME) == 0);
	CU_ASSERT(tpinfo.param.res_ns == RES);
	CU_ASSERT(tpinfo.param.min_tmo == MIN);
	CU_ASSERT(tpinfo.param.max_tmo == MAX);
	CU_ASSERT(strcmp(tpinfo.name, NAME) == 0);

	LOG_DBG("#timers..: %u\n", NTIMERS);
	LOG_DBG("Tmo range: %u ms (%" PRIu64 " ticks)\n", RANGE_MS,
		odp_timer_ns_to_tick(tp, 1000000ULL * RANGE_MS));

	uint64_t tick;
	for (tick = 0; tick < 1000000000000ULL; tick += 1000000ULL) {
		uint64_t ns = odp_timer_tick_to_ns(tp, tick);
		uint64_t t2 = odp_timer_ns_to_tick(tp, ns);
		if (tick != t2)
			CU_FAIL("Invalid conversion tick->ns->tick");
	}

	/* Initialize barrier used by worker threads for synchronization */
	odp_barrier_init(&test_barrier, num_workers);

	/* Initialize the shared timeout counter */
	odp_atomic_init_u32(&ndelivtoolate, 0);

	/* Initialize the number of finally allocated elements */
	odp_atomic_init_u32(&timers_allocated, 0);

	/* Create and start worker threads */
	pthrd_arg thrdarg;
	thrdarg.testcase = 0;
	thrdarg.numthrds = num_workers;
	odp_cunit_thread_create(worker_entrypoint, &thrdarg);

	/* Wait for worker threads to exit */
	odp_cunit_thread_exit(&thrdarg);
	LOG_DBG("Number of timeouts delivered/received too late: %" PRIu32 "\n",
		odp_atomic_load_u32(&ndelivtoolate));

	/* Check some statistics after the test */
	if (odp_timer_pool_info(tp, &tpinfo) != 0)
		CU_FAIL("odp_timer_pool_info");
	CU_ASSERT(tpinfo.param.num_timers == (unsigned)num_workers * NTIMERS);
	CU_ASSERT(tpinfo.cur_timers == 0);
	CU_ASSERT(tpinfo.hwm_timers == odp_atomic_load_u32(&timers_allocated));

	/* Destroy timer pool, all timers must have been freed */
	odp_timer_pool_destroy(tp);

	/* Destroy timeout pool, all timeouts must have been freed */
	rc = odp_pool_destroy(tbp);
	CU_ASSERT(rc == 0);

	CU_PASS("ODP timer test");
}

odp_testinfo_t timer_suite[] = {
	ODP_TEST_INFO(timer_test_timeout_pool_alloc),
	ODP_TEST_INFO(timer_test_timeout_pool_free),
	ODP_TEST_INFO(timer_test_odp_timer_cancel),
	ODP_TEST_INFO(timer_test_odp_timer_all),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t timer_suites[] = {
	{"Timer", NULL, NULL, timer_suite},
	ODP_SUITE_INFO_NULL,
};

int timer_main(void)
{
	int ret = odp_cunit_register(timer_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
