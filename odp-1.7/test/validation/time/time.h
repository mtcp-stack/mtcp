/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_TIME_H_
#define _ODP_TEST_TIME_H_

#include <odp_cunit_common.h>

/* test functions: */
void time_test_constants(void);
void time_test_local_res(void);
void time_test_global_res(void);
void time_test_local_conversion(void);
void time_test_global_conversion(void);
void time_test_local_monotony(void);
void time_test_global_monotony(void);
void time_test_local_cmp(void);
void time_test_global_cmp(void);
void time_test_local_diff(void);
void time_test_global_diff(void);
void time_test_local_sum(void);
void time_test_global_sum(void);
void time_test_local_wait_until(void);
void time_test_global_wait_until(void);
void time_test_wait_ns(void);
void time_test_local_to_u64(void);
void time_test_global_to_u64(void);

/* test arrays: */
extern odp_testinfo_t time_suite_time[];

/* test registry: */
extern odp_suiteinfo_t time_suites[];

/* main test program: */
int time_main(void);

#endif
