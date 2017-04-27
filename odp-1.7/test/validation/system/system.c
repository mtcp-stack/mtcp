/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <ctype.h>
#include <odp.h>
#include <odp/cpumask.h>
#include "odp_cunit_common.h"
#include "test_debug.h"
#include "system.h"

#define DIFF_TRY_NUM			160
#define RES_TRY_NUM			10

void system_test_odp_version_numbers(void)
{
	int char_ok = 0;
	char version_string[128];
	char *s = version_string;

	strncpy(version_string, odp_version_api_str(),
		sizeof(version_string) - 1);

	while (*s) {
		if (isdigit((int)*s) || (strncmp(s, ".", 1) == 0)) {
			char_ok = 1;
			s++;
		} else {
			char_ok = 0;
			LOG_DBG("\nBAD VERSION=%s\n", version_string);
			break;
		}
	}
	CU_ASSERT(char_ok);
}

void system_test_odp_cpu_count(void)
{
	int cpus;

	cpus = odp_cpu_count();
	CU_ASSERT(0 < cpus);
}

void system_test_odp_cpu_cycles(void)
{
	uint64_t c2, c1;

	c1 = odp_cpu_cycles();
	odp_time_wait_ns(100);
	c2 = odp_cpu_cycles();

	CU_ASSERT(c2 != c1);
}

void system_test_odp_cpu_cycles_max(void)
{
	uint64_t c2, c1;
	uint64_t max1, max2;

	max1 = odp_cpu_cycles_max();
	odp_time_wait_ns(100);
	max2 = odp_cpu_cycles_max();

	CU_ASSERT(max1 >= UINT32_MAX / 2);
	CU_ASSERT(max1 == max2);

	c1 = odp_cpu_cycles();
	odp_time_wait_ns(1000);
	c2 = odp_cpu_cycles();

	CU_ASSERT(c1 <= max1 && c2 <= max1);
}

void system_test_odp_cpu_cycles_resolution(void)
{
	int i;
	uint64_t res;
	uint64_t c2, c1, max;

	max = odp_cpu_cycles_max();

	res = odp_cpu_cycles_resolution();
	CU_ASSERT(res != 0);
	CU_ASSERT(res < max / 1024);

	for (i = 0; i < RES_TRY_NUM; i++) {
		c1 = odp_cpu_cycles();
		odp_time_wait_ns(100 * ODP_TIME_MSEC_IN_NS + i);
		c2 = odp_cpu_cycles();

		CU_ASSERT(c1 % res == 0);
		CU_ASSERT(c2 % res == 0);
	}
}

void system_test_odp_cpu_cycles_diff(void)
{
	int i;
	uint64_t c2, c1, c3, max;
	uint64_t tmp, diff, res;

	res = odp_cpu_cycles_resolution();
	max = odp_cpu_cycles_max();

	/* check resolution for wrap */
	c1 = max - 2 * res;
	do
		c2 = odp_cpu_cycles();
	while (c1 < c2);

	diff = odp_cpu_cycles_diff(c1, c1);
	CU_ASSERT(diff == 0);

	/* wrap */
	tmp = c2 + (max - c1) + res;
	diff = odp_cpu_cycles_diff(c2, c1);
	CU_ASSERT(diff == tmp);
	CU_ASSERT(diff % res == 0);

	/* no wrap, revert args */
	tmp = c1 - c2;
	diff = odp_cpu_cycles_diff(c1, c2);
	CU_ASSERT(diff == tmp);
	CU_ASSERT(diff % res == 0);

	c3 = odp_cpu_cycles();
	for (i = 0; i < DIFF_TRY_NUM; i++) {
		c1 = odp_cpu_cycles();
		odp_time_wait_ns(100 * ODP_TIME_MSEC_IN_NS + i);
		c2 = odp_cpu_cycles();

		CU_ASSERT(c2 != c1);
		CU_ASSERT(c1 % res == 0);
		CU_ASSERT(c2 % res == 0);
		CU_ASSERT(c1 <= max && c2 <= max);

		if (c2 > c1)
			tmp = c2 - c1;
		else
			tmp = c2 + (max - c1) + res;

		diff = odp_cpu_cycles_diff(c2, c1);
		CU_ASSERT(diff == tmp);
		CU_ASSERT(diff % res == 0);

		/* wrap is detected and verified */
		if (c2 < c1)
			break;
	}

	/* wrap was detected, no need to continue */
	if (i < DIFF_TRY_NUM)
		return;

	/* wrap has to be detected if possible */
	CU_ASSERT(max > UINT32_MAX);
	CU_ASSERT((max - c3) > UINT32_MAX);

	printf("wrap was not detected...");
}

void system_test_odp_sys_cache_line_size(void)
{
	uint64_t cache_size;

	cache_size = odp_sys_cache_line_size();
	CU_ASSERT(0 < cache_size);
	CU_ASSERT(ODP_CACHE_LINE_SIZE == cache_size);
}

void system_test_odp_cpu_model_str(void)
{
	char model[128];

	snprintf(model, 128, "%s", odp_cpu_model_str());
	CU_ASSERT(strlen(model) > 0);
	CU_ASSERT(strlen(model) < 127);
}

void system_test_odp_cpu_model_str_id(void)
{
	char model[128];
	odp_cpumask_t mask;
	int i, num, cpu;

	num = odp_cpumask_all_available(&mask);
	cpu = odp_cpumask_first(&mask);

	for (i = 0; i < num; i++) {
		snprintf(model, 128, "%s", odp_cpu_model_str_id(cpu));
		CU_ASSERT(strlen(model) > 0);
		CU_ASSERT(strlen(model) < 127);
		cpu = odp_cpumask_next(&mask, cpu);
	}
}

void system_test_odp_sys_page_size(void)
{
	uint64_t page;

	page = odp_sys_page_size();
	CU_ASSERT(0 < page);
	CU_ASSERT(ODP_PAGE_SIZE == page);
}

void system_test_odp_sys_huge_page_size(void)
{
	uint64_t page;

	page = odp_sys_huge_page_size();
	CU_ASSERT(0 < page);
}

int system_check_odp_cpu_hz(void)
{
	if (odp_cpu_hz() == 0) {
		fprintf(stderr, "odp_cpu_hz is not supported, skipping\n");
		return ODP_TEST_INACTIVE;
	}

	return ODP_TEST_ACTIVE;
}

void system_test_odp_cpu_hz(void)
{
	uint64_t hz = odp_cpu_hz();

	/* Test value sanity: less than 10GHz */
	CU_ASSERT(hz < 10 * GIGA_HZ);

	/* larger than 1kHz */
	CU_ASSERT(hz > 1 * KILO_HZ);
}

int system_check_odp_cpu_hz_id(void)
{
	uint64_t hz;
	odp_cpumask_t mask;
	int i, num, cpu;

	num = odp_cpumask_all_available(&mask);
	cpu = odp_cpumask_first(&mask);

	for (i = 0; i < num; i++) {
		hz = odp_cpu_hz_id(cpu);
		if (hz == 0) {
			fprintf(stderr, "cpu %d does not support"
				" odp_cpu_hz_id(),"
				"skip that test\n", cpu);
			return ODP_TEST_INACTIVE;
		}
		cpu = odp_cpumask_next(&mask, cpu);
	}

	return ODP_TEST_ACTIVE;
}

void system_test_odp_cpu_hz_id(void)
{
	uint64_t hz;
	odp_cpumask_t mask;
	int i, num, cpu;

	num = odp_cpumask_all_available(&mask);
	cpu = odp_cpumask_first(&mask);

	for (i = 0; i < num; i++) {
		hz = odp_cpu_hz_id(cpu);
		/* Test value sanity: less than 10GHz */
		CU_ASSERT(hz < 10 * GIGA_HZ);
		/* larger than 1kHz */
		CU_ASSERT(hz > 1 * KILO_HZ);
		cpu = odp_cpumask_next(&mask, cpu);
	}
}

void system_test_odp_cpu_hz_max(void)
{
	uint64_t hz;

	hz = odp_cpu_hz_max();
	CU_ASSERT(0 < hz);
}

void system_test_odp_cpu_hz_max_id(void)
{
	uint64_t hz;
	odp_cpumask_t mask;
	int i, num, cpu;

	num = odp_cpumask_all_available(&mask);
	cpu = odp_cpumask_first(&mask);

	for (i = 0; i < num; i++) {
		hz = odp_cpu_hz_max_id(cpu);
		CU_ASSERT(0 < hz);
		cpu = odp_cpumask_next(&mask, cpu);
	}
}

odp_testinfo_t system_suite[] = {
	ODP_TEST_INFO(system_test_odp_version_numbers),
	ODP_TEST_INFO(system_test_odp_cpu_count),
	ODP_TEST_INFO(system_test_odp_sys_cache_line_size),
	ODP_TEST_INFO(system_test_odp_cpu_model_str),
	ODP_TEST_INFO(system_test_odp_cpu_model_str_id),
	ODP_TEST_INFO(system_test_odp_sys_page_size),
	ODP_TEST_INFO(system_test_odp_sys_huge_page_size),
	ODP_TEST_INFO_CONDITIONAL(system_test_odp_cpu_hz,
				  system_check_odp_cpu_hz),
	ODP_TEST_INFO_CONDITIONAL(system_test_odp_cpu_hz_id,
				  system_check_odp_cpu_hz_id),
	ODP_TEST_INFO(system_test_odp_cpu_hz_max),
	ODP_TEST_INFO(system_test_odp_cpu_hz_max_id),
	ODP_TEST_INFO(system_test_odp_cpu_cycles),
	ODP_TEST_INFO(system_test_odp_cpu_cycles_max),
	ODP_TEST_INFO(system_test_odp_cpu_cycles_resolution),
	ODP_TEST_INFO(system_test_odp_cpu_cycles_diff),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t system_suites[] = {
	{"System Info", NULL, NULL, system_suite},
	ODP_SUITE_INFO_NULL,
};

int system_main(void)
{
	int ret = odp_cunit_register(system_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
