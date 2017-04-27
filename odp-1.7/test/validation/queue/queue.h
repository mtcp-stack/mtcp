/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_QUEUE_H_
#define _ODP_TEST_QUEUE_H_

#include <odp_cunit_common.h>

/* test functions: */
void queue_test_sunnydays(void);
void queue_test_info(void);

/* test arrays: */
extern odp_testinfo_t queue_suite[];

/* test array init/term functions: */
int queue_suite_init(void);
int queue_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t queue_suites[];

/* main test program: */
int queue_main(void);

#endif
