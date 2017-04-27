/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	 BSD-3-Clause
 */

#include <malloc.h>
#include <odp.h>
#include <CUnit/Basic.h>
#include <odp_cunit_common.h>
#include <unistd.h>
#include "barrier.h"

#define VERBOSE			0
#define MAX_ITERATIONS		1000
#define BARRIER_ITERATIONS	64

#define SLOW_BARRIER_DELAY	400
#define BASE_DELAY		6

#define NUM_TEST_BARRIERS	BARRIER_ITERATIONS
#define NUM_RESYNC_BARRIERS	100

#define BARRIER_DELAY		10

#define GLOBAL_SHM_NAME		"GlobalLockTest"

#define UNUSED			__attribute__((__unused__))

static volatile int temp_result;

typedef __volatile uint32_t volatile_u32_t;
typedef __volatile uint64_t volatile_u64_t;

typedef struct {
	odp_atomic_u32_t wait_cnt;
} custom_barrier_t;

typedef struct {
	/* Global variables */
	uint32_t g_num_threads;
	uint32_t g_iterations;
	uint32_t g_verbose;
	uint32_t g_max_num_cores;

	odp_barrier_t test_barriers[NUM_TEST_BARRIERS];
	custom_barrier_t custom_barrier1[NUM_TEST_BARRIERS];
	custom_barrier_t custom_barrier2[NUM_TEST_BARRIERS];
	volatile_u32_t slow_thread_num;
	volatile_u32_t barrier_cnt1;
	volatile_u32_t barrier_cnt2;
	odp_barrier_t global_barrier;

} global_shared_mem_t;

/* Per-thread memory */
typedef struct {
	global_shared_mem_t *global_mem;

	int thread_id;
	int thread_core;

	volatile_u64_t delay_counter;
} per_thread_mem_t;

static odp_shm_t global_shm;
static global_shared_mem_t *global_mem;

/*
* Delay a consistent amount of time.  Ideally the amount of CPU time taken
* is linearly proportional to "iterations".  The goal is to try to do some
* work that the compiler optimizer won't optimize away, and also to
* minimize loads and stores (at least to different memory addresses)
* so as to not affect or be affected by caching issues.  This does NOT have to
* correlate to a specific number of cpu cycles or be consistent across
* CPU architectures.
*/
static void thread_delay(per_thread_mem_t *per_thread_mem, uint32_t iterations)
{
	volatile_u64_t *counter_ptr;
	uint32_t cnt;

	counter_ptr = &per_thread_mem->delay_counter;

	for (cnt = 1; cnt <= iterations; cnt++)
		(*counter_ptr)++;
}

/* Initialise per-thread memory */
static per_thread_mem_t *thread_init(void)
{
	global_shared_mem_t *global_mem;
	per_thread_mem_t *per_thread_mem;
	odp_shm_t global_shm;
	uint32_t per_thread_mem_len;

	per_thread_mem_len = sizeof(per_thread_mem_t);
	per_thread_mem = malloc(per_thread_mem_len);
	memset(per_thread_mem, 0, per_thread_mem_len);

	per_thread_mem->delay_counter = 1;

	per_thread_mem->thread_id = odp_thread_id();
	per_thread_mem->thread_core = odp_cpu_id();

	global_shm = odp_shm_lookup(GLOBAL_SHM_NAME);
	global_mem = odp_shm_addr(global_shm);
	CU_ASSERT_PTR_NOT_NULL(global_mem);

	per_thread_mem->global_mem = global_mem;

	return per_thread_mem;
}

static void thread_finalize(per_thread_mem_t *per_thread_mem)
{
	free(per_thread_mem);
}

static void custom_barrier_init(custom_barrier_t *custom_barrier,
				uint32_t num_threads)
{
	odp_atomic_init_u32(&custom_barrier->wait_cnt, num_threads);
}

static void custom_barrier_wait(custom_barrier_t *custom_barrier)
{
	volatile_u64_t counter = 1;
	uint32_t delay_cnt, wait_cnt;

	odp_atomic_sub_u32(&custom_barrier->wait_cnt, 1);

	wait_cnt = 1;
	while (wait_cnt != 0) {
		for (delay_cnt = 1; delay_cnt <= BARRIER_DELAY; delay_cnt++)
			counter++;

		wait_cnt = odp_atomic_load_u32(&custom_barrier->wait_cnt);
	}
}

static uint32_t barrier_test(per_thread_mem_t *per_thread_mem,
			     odp_bool_t no_barrier_test)
{
	global_shared_mem_t *global_mem;
	uint32_t barrier_errs, iterations, cnt, i_am_slow_thread;
	uint32_t thread_num, slow_thread_num, next_slow_thread, num_threads;
	uint32_t lock_owner_delay, barrier_cnt1, barrier_cnt2;

	thread_num = odp_thread_id();
	global_mem = per_thread_mem->global_mem;
	num_threads = global_mem->g_num_threads;
	iterations = BARRIER_ITERATIONS;

	barrier_errs = 0;
	lock_owner_delay = SLOW_BARRIER_DELAY;

	for (cnt = 1; cnt < iterations; cnt++) {
		/* Wait here until all of the threads reach this point */
		custom_barrier_wait(&global_mem->custom_barrier1[cnt]);

		barrier_cnt1 = global_mem->barrier_cnt1;
		barrier_cnt2 = global_mem->barrier_cnt2;

		if ((barrier_cnt1 != cnt) || (barrier_cnt2 != cnt)) {
			printf("thread_num=%" PRIu32 " barrier_cnts of %" PRIu32
				   " %" PRIu32 " cnt=%" PRIu32 "\n",
			       thread_num, barrier_cnt1, barrier_cnt2, cnt);
			barrier_errs++;
		}

		/* Wait here until all of the threads reach this point */
		custom_barrier_wait(&global_mem->custom_barrier2[cnt]);

		slow_thread_num = global_mem->slow_thread_num;
		i_am_slow_thread = thread_num == slow_thread_num;
		next_slow_thread = slow_thread_num + 1;
		if (num_threads < next_slow_thread)
			next_slow_thread = 1;

		/*
		* Now run the test, which involves having all but one thread
		* immediately calling odp_barrier_wait(), and one thread wait a
		* moderate amount of time and then calling odp_barrier_wait().
		* The test fails if any of the first group of threads
		* has not waited for the "slow" thread. The "slow" thread is
		* responsible for re-initializing the barrier for next trial.
		*/
		if (i_am_slow_thread) {
			thread_delay(per_thread_mem, lock_owner_delay);
			lock_owner_delay += BASE_DELAY;
			if ((global_mem->barrier_cnt1 != cnt) ||
			    (global_mem->barrier_cnt2 != cnt) ||
			    (global_mem->slow_thread_num
					!= slow_thread_num))
				barrier_errs++;
		}

		if (no_barrier_test == 0)
			odp_barrier_wait(&global_mem->test_barriers[cnt]);

		global_mem->barrier_cnt1 = cnt + 1;
		odp_mb_full();

		if (i_am_slow_thread) {
			global_mem->slow_thread_num = next_slow_thread;
			global_mem->barrier_cnt2 = cnt + 1;
			odp_mb_full();
		} else {
			while (global_mem->barrier_cnt2 != (cnt + 1))
				thread_delay(per_thread_mem, BASE_DELAY);
		}
	}

	if ((global_mem->g_verbose) && (barrier_errs != 0))
		printf("\nThread %" PRIu32 " (id=%d core=%d) had %" PRIu32
		       " barrier_errs in %" PRIu32 " iterations\n", thread_num,
		       per_thread_mem->thread_id,
		       per_thread_mem->thread_core, barrier_errs, iterations);

	return barrier_errs;
}

static void *no_barrier_functional_test(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;
	uint32_t barrier_errs;

	per_thread_mem = thread_init();
	barrier_errs = barrier_test(per_thread_mem, 1);

	/*
	* Note that the following CU_ASSERT MAY appear incorrect, but for the
	* no_barrier test it should see barrier_errs or else there is something
	* wrong with the test methodology or the ODP thread implementation.
	* So this test PASSES only if it sees barrier_errs or a single
	* worker was used.
	*/
	CU_ASSERT(barrier_errs != 0 || global_mem->g_num_threads == 1);
	thread_finalize(per_thread_mem);

	return NULL;
}

static void *barrier_functional_test(void *arg UNUSED)
{
	per_thread_mem_t *per_thread_mem;
	uint32_t barrier_errs;

	per_thread_mem = thread_init();
	barrier_errs = barrier_test(per_thread_mem, 0);

	CU_ASSERT(barrier_errs == 0);
	thread_finalize(per_thread_mem);

	return NULL;
}

static void barrier_test_init(void)
{
	uint32_t num_threads, idx;

	num_threads = global_mem->g_num_threads;

	for (idx = 0; idx < NUM_TEST_BARRIERS; idx++) {
		odp_barrier_init(&global_mem->test_barriers[idx], num_threads);
		custom_barrier_init(&global_mem->custom_barrier1[idx],
				    num_threads);
		custom_barrier_init(&global_mem->custom_barrier2[idx],
				    num_threads);
	}

	global_mem->slow_thread_num = 1;
	global_mem->barrier_cnt1 = 1;
	global_mem->barrier_cnt2 = 1;
}

/* Barrier tests */
void barrier_test_memory_barrier(void)
{
	volatile int a = 0;
	volatile int b = 0;
	volatile int c = 0;
	volatile int d = 0;

	/* Call all memory barriers to verify that those are implemented */
	a = 1;
	odp_mb_release();
	b = 1;
	odp_mb_acquire();
	c = 1;
	odp_mb_full();
	d = 1;

	/* Avoid "variable set but not used" warning */
	temp_result = a + b + c + d;
}

void barrier_test_no_barrier_functional(void)
{
	pthrd_arg arg;

	arg.numthrds = global_mem->g_num_threads;
	barrier_test_init();
	odp_cunit_thread_create(no_barrier_functional_test, &arg);
	odp_cunit_thread_exit(&arg);
}

void barrier_test_barrier_functional(void)
{
	pthrd_arg arg;

	arg.numthrds = global_mem->g_num_threads;
	barrier_test_init();
	odp_cunit_thread_create(barrier_functional_test, &arg);
	odp_cunit_thread_exit(&arg);
}

odp_testinfo_t barrier_suite_barrier[] = {
	ODP_TEST_INFO(barrier_test_memory_barrier),
	ODP_TEST_INFO(barrier_test_no_barrier_functional),
	ODP_TEST_INFO(barrier_test_barrier_functional),
	ODP_TEST_INFO_NULL
};

int barrier_init(void)
{
	uint32_t workers_count, max_threads;
	int ret = 0;
	odp_cpumask_t mask;

	if (0 != odp_init_global(NULL, NULL)) {
		fprintf(stderr, "error: odp_init_global() failed.\n");
		return -1;
	}
	if (0 != odp_init_local(ODP_THREAD_CONTROL)) {
		fprintf(stderr, "error: odp_init_local() failed.\n");
		return -1;
	}

	global_shm = odp_shm_reserve(GLOBAL_SHM_NAME,
				     sizeof(global_shared_mem_t), 64,
				     ODP_SHM_SW_ONLY);
	if (ODP_SHM_INVALID == global_shm) {
		fprintf(stderr, "Unable reserve memory for global_shm\n");
		return -1;
	}

	global_mem = odp_shm_addr(global_shm);
	memset(global_mem, 0, sizeof(global_shared_mem_t));

	global_mem->g_num_threads = MAX_WORKERS;
	global_mem->g_iterations = MAX_ITERATIONS;
	global_mem->g_verbose = VERBOSE;

	workers_count = odp_cpumask_default_worker(&mask, 0);

	max_threads = (workers_count >= MAX_WORKERS) ?
			MAX_WORKERS : workers_count;

	if (max_threads < global_mem->g_num_threads) {
		printf("Requested num of threads is too large\n");
		printf("reducing from %" PRIu32 " to %" PRIu32 "\n",
		       global_mem->g_num_threads,
		       max_threads);
		global_mem->g_num_threads = max_threads;
	}

	printf("Num of threads used = %" PRIu32 "\n",
	       global_mem->g_num_threads);

	return ret;
}

odp_suiteinfo_t barrier_suites[] = {
	{"barrier", NULL, NULL,
		barrier_suite_barrier},
	ODP_SUITE_INFO_NULL
};

int barrier_main(void)
{
	int ret;

	odp_cunit_register_global_init(barrier_init);

	ret = odp_cunit_register(barrier_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
