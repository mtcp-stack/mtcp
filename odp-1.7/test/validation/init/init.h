/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_INIT_H_
#define _ODP_TEST_INIT_H_

#include <odp_cunit_common.h>

/* test functions: */
void init_test_odp_init_global_replace_abort(void);
void init_test_odp_init_global_replace_log(void);
void init_test_odp_init_global(void);

/* test arrays: */
extern odp_testinfo_t init_suite_abort[];
extern odp_testinfo_t init_suite_log[];
extern odp_testinfo_t init_suite_ok[];

/* test registry: */
extern odp_suiteinfo_t init_suites_abort[];
extern odp_suiteinfo_t init_suites_log[];
extern odp_suiteinfo_t init_suites_ok[];

/* main test program: */
int init_main_abort(void);
int init_main_log(void);
int init_main_ok(void);

#endif
