/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "random.h"

void random_test_get_size(void)
{
	int32_t ret;
	uint8_t buf[32];

	ret = odp_random_data(buf, sizeof(buf), false);
	CU_ASSERT(ret == sizeof(buf));
}

odp_testinfo_t random_suite[] = {
	ODP_TEST_INFO(random_test_get_size),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t random_suites[] = {
	{"Random", NULL, NULL, random_suite},
	ODP_SUITE_INFO_NULL,
};

int random_main(void)
{
	int ret = odp_cunit_register(random_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
