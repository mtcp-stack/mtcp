/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "std_clib.h"

#include <string.h>

#define PATTERN 0x5e

static void std_clib_test_memcpy(void)
{
	uint8_t src[] = {0, 1,  2,  3,  4,  5,  6,  7,
			 8, 9, 10, 11, 12, 13, 14, 15};
	uint8_t dst[16];
	int ret;

	memset(dst, 0, sizeof(dst));

	odp_memcpy(dst, src, sizeof(dst));

	ret = memcmp(dst, src, sizeof(dst));

	CU_ASSERT(ret == 0);
}

static void std_clib_test_memset(void)
{
	uint8_t data[] = {0, 1,  2,  3,  4,  5,  6,  7,
			  8, 9, 10, 11, 12, 13, 14, 15};
	uint8_t ref[16];
	int ret;

	odp_memset(data, PATTERN, sizeof(data));

	memset(ref, PATTERN, sizeof(ref));

	ret = memcmp(data, ref, sizeof(data));

	CU_ASSERT(ret == 0);
}

static void std_clib_test_memcmp(void)
{
	uint8_t data[]       = {1,  2,  3,  4,  5,  6,  7,  8,
				9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t equal[]      = {1,  2,  3,  4,  5,  6,  7,  8,
				9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t greater_11[] = {1,  2,  3,  4,  5,  6,  7,  8,
				9, 10, 99, 12, 13, 14, 15, 16};
	uint8_t less_6[]     = {1,  2,  3,  4,  5,  2,  7,  8,
				9, 10, 11, 12, 13, 14, 15, 16};
	size_t i;

	CU_ASSERT(odp_memcmp(data, equal, 0) == 0);
	CU_ASSERT(odp_memcmp(data, equal, sizeof(data)) == 0);
	CU_ASSERT(odp_memcmp(data, equal, sizeof(data) - 3) == 0);

	CU_ASSERT(odp_memcmp(greater_11, data, sizeof(data)) > 0);
	CU_ASSERT(odp_memcmp(greater_11, data, 11) > 0);
	CU_ASSERT(odp_memcmp(greater_11, data, 10) == 0);

	CU_ASSERT(odp_memcmp(less_6, data, sizeof(data)) < 0);
	CU_ASSERT(odp_memcmp(less_6, data, 6) < 0);
	CU_ASSERT(odp_memcmp(less_6, data, 5) == 0);

	for (i = 0; i < sizeof(data); i++) {
		uint8_t tmp;

		CU_ASSERT(odp_memcmp(data, equal, i + 1) == 0);
		tmp      = equal[i];
		equal[i] = 88;
		CU_ASSERT(odp_memcmp(data, equal, i + 1) < 0);
		equal[i] = 0;
		CU_ASSERT(odp_memcmp(data, equal, i + 1) > 0);
		equal[i] = tmp;
	}
}

odp_testinfo_t std_clib_suite[] = {
	ODP_TEST_INFO(std_clib_test_memcpy),
	ODP_TEST_INFO(std_clib_test_memset),
	ODP_TEST_INFO(std_clib_test_memcmp),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t std_clib_suites[] = {
	{"Std C library", NULL, NULL, std_clib_suite},
	ODP_SUITE_INFO_NULL
};

int std_clib_main(void)
{
	int ret = odp_cunit_register(std_clib_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
