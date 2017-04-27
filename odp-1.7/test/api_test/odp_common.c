/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP test application common
 */

#include <string.h>
#include <odp.h>
#include <odp/helper/linux.h>
#include <odp_common.h>
#include <test_debug.h>

#define MAX_WORKERS           32            /**< Max worker threads */

/* Globals */
static odph_linux_pthread_t thread_tbl[MAX_WORKERS]; /**< worker threads table*/
static int num_workers;				    /**< number of workers 	*/

/**
 * Print system information
 */
void odp_print_system_info(void)
{
	odp_cpumask_t cpumask;
	char str[ODP_CPUMASK_STR_SIZE];

	memset(str, 1, sizeof(str));

	odp_cpumask_zero(&cpumask);

	odp_cpumask_from_str(&cpumask, "0x1");
	(void)odp_cpumask_to_str(&cpumask, str, sizeof(str));

	printf("\n");
	printf("ODP system info\n");
	printf("---------------\n");
	printf("ODP API version: %s\n",        odp_version_api_str());
	printf("CPU model:       %s\n",        odp_cpu_model_str());
	printf("CPU freq (hz):   %"PRIu64"\n", odp_cpu_hz_max());
	printf("Cache line size: %i\n",        odp_sys_cache_line_size());
	printf("CPU count:       %i\n",        odp_cpu_count());
	printf("CPU mask:        %s\n",        str);

	printf("\n");
}

/** test init globals and call odp_init_global() */
int odp_test_global_init(void)
{
	memset(thread_tbl, 0, sizeof(thread_tbl));

	if (odp_init_global(NULL, NULL)) {
		LOG_ERR("ODP global init failed.\n");
		return -1;
	}

	num_workers = odp_cpu_count();
	/* force to max CPU count */
	if (num_workers > MAX_WORKERS)
		num_workers = MAX_WORKERS;

	return 0;
}

/** create test thread */
int odp_test_thread_create(void *func_ptr(void *), pthrd_arg *arg)
{
	odp_cpumask_t cpumask;

	/* Create and init additional threads */
	odp_cpumask_default_worker(&cpumask, arg->numthrds);
	odph_linux_pthread_create(thread_tbl, &cpumask, func_ptr,
				  (void *)arg, ODP_THREAD_WORKER);

	return 0;
}

/** exit from test thread */
int odp_test_thread_exit(pthrd_arg *arg)
{
	/* Wait for other threads to exit */
	odph_linux_pthread_join(thread_tbl, arg->numthrds);

	return 0;
}
