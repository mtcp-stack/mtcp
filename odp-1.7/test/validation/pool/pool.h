/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef _ODP_TEST_POOL_H_
#define _ODP_TEST_POOL_H_

#include <odp_cunit_common.h>

/* test functions: */
void pool_test_create_destroy_buffer(void);
void pool_test_create_destroy_packet(void);
void pool_test_create_destroy_timeout(void);
void pool_test_create_destroy_buffer_shm(void);
void pool_test_lookup_info_print(void);

/* test arrays: */
extern odp_testinfo_t pool_suite[];

/* test registry: */
extern odp_suiteinfo_t pool_suites[];

/* main test program: */
int pool_main(void);

#endif
