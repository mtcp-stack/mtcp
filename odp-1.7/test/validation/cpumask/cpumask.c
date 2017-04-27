/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>

#include "odp_cunit_common.h"
#include "cpumask.h"
#include "mask_common.h"

/* default worker parameter to get all that may be available */
#define ALL_AVAILABLE 0

void cpumask_test_odp_cpumask_def_control(void)
{
	unsigned num;
	unsigned mask_count;
	unsigned max_cpus = mask_capacity();
	odp_cpumask_t mask;

	num = odp_cpumask_default_control(&mask, ALL_AVAILABLE);
	mask_count = odp_cpumask_count(&mask);

	CU_ASSERT(mask_count == num);
	CU_ASSERT(num > 0);
	CU_ASSERT(num <= max_cpus);
}

void cpumask_test_odp_cpumask_def_worker(void)
{
	unsigned num;
	unsigned mask_count;
	unsigned max_cpus = mask_capacity();
	odp_cpumask_t mask;

	num = odp_cpumask_default_worker(&mask, ALL_AVAILABLE);
	mask_count = odp_cpumask_count(&mask);

	CU_ASSERT(mask_count == num);
	CU_ASSERT(num > 0);
	CU_ASSERT(num <= max_cpus);
}

void cpumask_test_odp_cpumask_def(void)
{
	unsigned mask_count;
	unsigned num_worker;
	unsigned num_control;
	unsigned max_cpus = mask_capacity();
	unsigned available_cpus = odp_cpu_count();
	unsigned requested_cpus;
	odp_cpumask_t mask;
	unsigned cpu_id;

	CU_ASSERT(available_cpus <= max_cpus);

	if (available_cpus > 1)
		requested_cpus = available_cpus - 1;
	else
		requested_cpus = available_cpus;

	for (cpu_id = 0; cpu_id < available_cpus; cpu_id++) {
		(void)odp_cpumask_unbind_cpu(cpu_id);
	}
	num_worker = odp_cpumask_default_worker(&mask, requested_cpus);

	mask_count = odp_cpumask_count(&mask);
	CU_ASSERT(mask_count == num_worker);

	num_control = odp_cpumask_default_control(&mask, 1);
	mask_count = odp_cpumask_count(&mask);
	CU_ASSERT(mask_count == num_control);

	CU_ASSERT(num_control == 1);
	CU_ASSERT(num_worker <= available_cpus);
	CU_ASSERT(num_worker > 0);
}

odp_testinfo_t cpumask_suite[] = {
	ODP_TEST_INFO(cpumask_test_odp_cpumask_to_from_str),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_equal),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_zero),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_set),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_clr),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_isset),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_count),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_and),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_or),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_xor),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_copy),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_first),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_last),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_next),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_setall),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_def_control),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_def_worker),
	ODP_TEST_INFO(cpumask_test_odp_cpumask_def),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t cpumask_suites[] = {
	{"Cpumask", NULL, NULL, cpumask_suite},
	ODP_SUITE_INFO_NULL,
};

int cpumask_main(void)
{
	int ret = odp_cunit_register(cpumask_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
