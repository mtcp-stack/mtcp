/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <test_debug.h>
#include <odp.h>
#include <odp/helper/linux.h>

#define NUMBER_WORKERS 16 /* 0 = max */

static void *worker_fn(void *arg TEST_UNUSED)
{
	/* depend on the odp helper to call odp_init_local */
	printf("Worker thread on CPU %d\n", odp_cpu_id());

	return 0;
}

/* Create additional dataplane processes */
int main(int argc TEST_UNUSED, char *argv[] TEST_UNUSED)
{
	odp_cpumask_t cpu_mask;
	int num_workers;
	int cpu;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	int ret;
	odph_linux_process_t proc[NUMBER_WORKERS];

	if (odp_init_global(NULL, NULL)) {
		LOG_ERR("Error: ODP global init failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_init_local(ODP_THREAD_CONTROL)) {
		LOG_ERR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* discover how many processes this system can support */
	num_workers = odp_cpumask_default_worker(&cpu_mask, NUMBER_WORKERS);
	if (num_workers < NUMBER_WORKERS) {
		printf("System can only support %d processes and not the %d requested\n",
		       num_workers, NUMBER_WORKERS);
	}

	/* generate a summary for the user */
	(void)odp_cpumask_to_str(&cpu_mask, cpumaskstr, sizeof(cpumaskstr));
	printf("default cpu mask:           %s\n", cpumaskstr);
	printf("default num worker processes: %i\n", num_workers);

	cpu = odp_cpumask_first(&cpu_mask);
	printf("the first CPU:              %i\n", cpu);

	/* reserve cpu 0 for the control plane so remove it
	from the default mask */
	odp_cpumask_clr(&cpu_mask, 0);
	num_workers = odp_cpumask_count(&cpu_mask);
	(void)odp_cpumask_to_str(&cpu_mask, cpumaskstr, sizeof(cpumaskstr));
	printf("new cpu mask:               %s\n", cpumaskstr);
	printf("new num worker processes:     %i\n\n", num_workers);

	/* Fork worker processes */
	ret = odph_linux_process_fork_n(proc, &cpu_mask);

	if (ret < 0) {
		LOG_ERR("Fork workers failed %i\n", ret);
		return -1;
	}

	if (ret == 0) {
		/* Child process */
		worker_fn(NULL);
	} else {
		/* Parent process */
		odph_linux_process_wait_n(proc, num_workers);

		if (odp_term_global()) {
			LOG_ERR("Error: ODP global term failed.\n");
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
