/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef _ODP_TEST_BUFFER_H_
#define _ODP_TEST_BUFFER_H_

#include <odp_cunit_common.h>

/* test functions: */
void buffer_test_pool_alloc(void);
void buffer_test_pool_free(void);
void buffer_test_pool_alloc_multi(void);
void buffer_test_pool_free_multi(void);
void buffer_test_management_basic(void);

/* test arrays: */
extern odp_testinfo_t buffer_suite[];

/* test array init/term functions: */
int buffer_suite_init(void);
int buffer_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t buffer_suites[];

/* main test program: */
int buffer_main(void);

#endif
