/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "hash.h"

void hash_test_crc32c(void)
{
	uint32_t test_value = 0x12345678;
	uint32_t ret = odp_hash_crc32c(&test_value, 4, 0);

	CU_ASSERT(ret == 0xfa745634);

	test_value = 0x87654321;
	ret = odp_hash_crc32c(&test_value, 4, 0);

	CU_ASSERT(ret == 0xaca37da7);

	uint32_t test_values[] = {0x12345678, 0x87654321};

	ret = odp_hash_crc32c(test_values, 8, 0);

	CU_ASSERT(ret == 0xe6e910b0);
}

odp_testinfo_t hash_suite[] = {
	ODP_TEST_INFO(hash_test_crc32c),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t hash_suites[] = {
	{"Hash", NULL, NULL, hash_suite},
	ODP_SUITE_INFO_NULL
};

int hash_main(void)
{
	int ret = odp_cunit_register(hash_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;

}
