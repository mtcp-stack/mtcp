/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef _ODP_TEST_SYSTEM_H_
#define _ODP_TEST_SYSTEM_H_

#include <odp_cunit_common.h>

#define GIGA_HZ 1000000000ULL
#define KILO_HZ 1000ULL

/* test functions: */
void system_test_odp_version_numbers(void);
void system_test_odp_cpu_count(void);
void system_test_odp_sys_cache_line_size(void);
void system_test_odp_cpu_model_str(void);
void system_test_odp_cpu_model_str_id(void);
void system_test_odp_sys_page_size(void);
void system_test_odp_sys_huge_page_size(void);
int system_check_odp_cpu_hz(void);
void system_test_odp_cpu_hz(void);
int system_check_odp_cpu_hz_id(void);
void system_test_odp_cpu_hz_id(void);
void system_test_odp_cpu_hz_max(void);
void system_test_odp_cpu_hz_max_id(void);
void system_test_odp_cpu_cycles_max(void);
void system_test_odp_cpu_cycles(void);
void system_test_odp_cpu_cycles_diff(void);
void system_test_odp_cpu_cycles_resolution(void);

/* test arrays: */
extern odp_testinfo_t system_suite[];

/* test registry: */
extern odp_suiteinfo_t system_suites[];

/* main test program: */
int system_main(void);

#endif
