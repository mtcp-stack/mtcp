/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_posix_extensions.h>

#include <sched.h>
#include <pthread.h>

#include <odp/cpumask.h>
#include <odp_core.h>
#include <odp_debug_internal.h>

int odp_cpumask_default_worker(odp_cpumask_t *mask, int num)
{
	int ret, cpu, i;
	cpu_set_t cpuset;

	ret = pthread_getaffinity_np(pthread_self(),
				     sizeof(cpu_set_t), &cpuset);
	if (ret != 0)
		ODP_ABORT("failed to read CPU affinity value\n");

	odp_cpumask_zero(mask);

	/*
	 * If no user supplied number or it's too large, then attempt
	 * to use all CPUs
	 */
	if (0 == num || CPU_SETSIZE < num)
		num = CPU_COUNT(&cpuset);

	/* build the mask, allocating down from highest numbered CPU */
	for (cpu = 0, i = CPU_SETSIZE - 1; i >= 0 && cpu < num; --i) {
		if (CPU_ISSET(i, &cpuset)) {
			ret = odp_bind_proc_to_lcore(i);
			if (ret)
				continue;
			odp_cpumask_set(mask, i);
			cpu++;
		}
	}

	if (odp_cpumask_isset(mask, 0))
		ODP_DBG("\n\tCPU0 will be used for both control and worker threads,\n"
			"\tthis will likely have a performance impact on the worker thread.\n");

	return cpu;
}

int odp_cpumask_default_control(odp_cpumask_t *mask, int num ODP_UNUSED)
{
	odp_cpumask_zero(mask);
	/* By default all control threads on CPU 0 */
	odp_cpumask_set(mask, 0);
	return 1;
}

int odp_cpumask_all_available(odp_cpumask_t *mask)
{
	odp_cpumask_t mask_work, mask_ctrl;

	odp_cpumask_default_worker(&mask_work, 0);
	odp_cpumask_default_control(&mask_ctrl, 0);
	odp_cpumask_or(mask, &mask_work, &mask_ctrl);

	return odp_cpumask_count(mask);
}

int odp_cpumask_unbind_cpu(int cpu)
{
	return odp_unbind_proc_to_lcore(cpu);
}
