/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp.h>
#include "odp_cunit_common.h"
#include "pool.h"

static int pool_name_number = 1;
static const int default_buffer_size = 1500;
static const int default_buffer_num = 1000;

static void pool_create_destroy(odp_pool_param_t *params)
{
	odp_pool_t pool;
	char pool_name[ODP_POOL_NAME_LEN];

	snprintf(pool_name, sizeof(pool_name),
		 "test_pool-%d", pool_name_number++);

	pool = odp_pool_create(pool_name, params);
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);
	CU_ASSERT(odp_pool_to_u64(pool) !=
		  odp_pool_to_u64(ODP_POOL_INVALID));
	CU_ASSERT(odp_pool_destroy(pool) == 0);
}

void pool_test_create_destroy_buffer(void)
{
	odp_pool_param_t params = {
			.buf = {
				.size  = default_buffer_size,
				.align = ODP_CACHE_LINE_SIZE,
				.num   = default_buffer_num,
			},
			.type = ODP_POOL_BUFFER,
	};

	pool_create_destroy(&params);
}

void pool_test_create_destroy_packet(void)
{
	odp_pool_param_t params = {
			.pkt = {
				.seg_len = 0,
				.len = default_buffer_size,
				.num   = default_buffer_num,
			},
			.type = ODP_POOL_PACKET,
	};

	pool_create_destroy(&params);
}

void pool_test_create_destroy_timeout(void)
{
	odp_pool_param_t params = {
			.tmo = {
				.num   = default_buffer_num,
			},
			.type = ODP_POOL_TIMEOUT,
	};

	pool_create_destroy(&params);
}

void pool_test_lookup_info_print(void)
{
	odp_pool_t pool;
	const char pool_name[] = "pool_for_lookup_test";
	odp_pool_info_t info;
	odp_pool_param_t params = {
			.buf = {
				.size  = default_buffer_size,
				.align = ODP_CACHE_LINE_SIZE,
				.num  = default_buffer_num,
			},
			.type  = ODP_POOL_BUFFER,
	};

	pool = odp_pool_create(pool_name, &params);
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	pool = odp_pool_lookup(pool_name);
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	CU_ASSERT_FATAL(odp_pool_info(pool, &info) == 0);
	CU_ASSERT(strncmp(pool_name, info.name, sizeof(pool_name)) == 0);
	CU_ASSERT(params.buf.size <= info.params.buf.size);
	CU_ASSERT(params.buf.align <= info.params.buf.align);
	CU_ASSERT(params.buf.num <= info.params.buf.num);
	CU_ASSERT(params.type == info.params.type);

	odp_pool_print(pool);

	CU_ASSERT(odp_pool_destroy(pool) == 0);
}

odp_testinfo_t pool_suite[] = {
	ODP_TEST_INFO(pool_test_create_destroy_buffer),
	ODP_TEST_INFO(pool_test_create_destroy_packet),
	ODP_TEST_INFO(pool_test_create_destroy_timeout),
	ODP_TEST_INFO(pool_test_lookup_info_print),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t pool_suites[] = {
	{ .pName = "Pool tests",
			.pTests = pool_suite,
	},
	ODP_SUITE_INFO_NULL,
};

int pool_main(void)
{
	int ret = odp_cunit_register(pool_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
