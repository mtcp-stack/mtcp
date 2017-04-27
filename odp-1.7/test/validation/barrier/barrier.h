/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_BARRIER_H_
#define _ODP_TEST_BARRIER_H_

#include <odp_cunit_common.h>

/* test functions: */
void barrier_test_memory_barrier(void);
void barrier_test_no_barrier_functional(void);
void barrier_test_barrier_functional(void);

/* test arrays: */
extern odp_testinfo_t barrier_suite_barrier[];

/* test registry: */
extern odp_suiteinfo_t barrier_suites[];

/* executable init/term functions: */
int barrier_init(void);

/* main test program: */
int barrier_main(void);

#endif
