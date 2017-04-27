/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef _ODP_TEST_CONFIG_H_
#define _ODP_TEST_CONFIG_H_

#include <odp_cunit_common.h>

/* test functions: */
void config_test(void);

/* test arrays: */
extern odp_testinfo_t config_suite[];

/* test array init/term functions: */
int config_suite_init(void);
int config_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t config_suites[];

/* main test program: */
int config_main(void);

#endif
