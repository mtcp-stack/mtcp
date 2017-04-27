/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include <mask_common.h>
#include <test_debug.h>
#include "thread.h"

/* Test thread entry and exit synchronization barriers */
odp_barrier_t bar_entry;
odp_barrier_t bar_exit;

void thread_test_odp_cpu_id(void)
{
	(void)odp_cpu_id();
	CU_PASS();
}

void thread_test_odp_thread_id(void)
{
	(void)odp_thread_id();
	CU_PASS();
}

void thread_test_odp_thread_count(void)
{
	(void)odp_thread_count();
	CU_PASS();
}

static void *thread_func(void *arg TEST_UNUSED)
{
	/* indicate that thread has started */
	odp_barrier_wait(&bar_entry);

	CU_ASSERT(odp_thread_type() == ODP_THREAD_WORKER);

	/* wait for indication that we can exit */
	odp_barrier_wait(&bar_exit);

	return NULL;
}

void thread_test_odp_thrmask_worker(void)
{
	odp_thrmask_t mask;
	int ret;
	pthrd_arg args = { .testcase = 0, .numthrds = 1 };

	CU_ASSERT_FATAL(odp_thread_type() == ODP_THREAD_CONTROL);

	odp_barrier_init(&bar_entry, args.numthrds + 1);
	odp_barrier_init(&bar_exit,  args.numthrds + 1);

	/* should start out with 0 worker threads */
	ret = odp_thrmask_worker(&mask);
	CU_ASSERT(ret == odp_thrmask_count(&mask));
	CU_ASSERT(ret == 0);

	/* start the test thread(s) */
	ret = odp_cunit_thread_create(thread_func, &args);
	CU_ASSERT(ret == args.numthrds);

	if (ret != args.numthrds)
		return;

	/* wait for thread(s) to start */
	odp_barrier_wait(&bar_entry);

	ret = odp_thrmask_worker(&mask);
	CU_ASSERT(ret == odp_thrmask_count(&mask));
	CU_ASSERT(ret == args.numthrds);
	CU_ASSERT(ret <= odp_thread_count_max());

	/* allow thread(s) to exit */
	odp_barrier_wait(&bar_exit);

	odp_cunit_thread_exit(&args);
}

void thread_test_odp_thrmask_control(void)
{
	odp_thrmask_t mask;
	int ret;

	CU_ASSERT(odp_thread_type() == ODP_THREAD_CONTROL);

	/* should start out with 1 worker thread */
	ret = odp_thrmask_control(&mask);
	CU_ASSERT(ret == odp_thrmask_count(&mask));
	CU_ASSERT(ret == 1);
}

odp_testinfo_t thread_suite[] = {
	ODP_TEST_INFO(thread_test_odp_cpu_id),
	ODP_TEST_INFO(thread_test_odp_thread_id),
	ODP_TEST_INFO(thread_test_odp_thread_count),
	ODP_TEST_INFO(thread_test_odp_thrmask_to_from_str),
	ODP_TEST_INFO(thread_test_odp_thrmask_equal),
	ODP_TEST_INFO(thread_test_odp_thrmask_zero),
	ODP_TEST_INFO(thread_test_odp_thrmask_set),
	ODP_TEST_INFO(thread_test_odp_thrmask_clr),
	ODP_TEST_INFO(thread_test_odp_thrmask_isset),
	ODP_TEST_INFO(thread_test_odp_thrmask_count),
	ODP_TEST_INFO(thread_test_odp_thrmask_and),
	ODP_TEST_INFO(thread_test_odp_thrmask_or),
	ODP_TEST_INFO(thread_test_odp_thrmask_xor),
	ODP_TEST_INFO(thread_test_odp_thrmask_copy),
	ODP_TEST_INFO(thread_test_odp_thrmask_first),
	ODP_TEST_INFO(thread_test_odp_thrmask_last),
	ODP_TEST_INFO(thread_test_odp_thrmask_next),
	ODP_TEST_INFO(thread_test_odp_thrmask_worker),
	ODP_TEST_INFO(thread_test_odp_thrmask_control),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t thread_suites[] = {
	{"thread", NULL, NULL, thread_suite},
	ODP_SUITE_INFO_NULL,
};

int thread_main(void)
{
	int ret = odp_cunit_register(thread_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
