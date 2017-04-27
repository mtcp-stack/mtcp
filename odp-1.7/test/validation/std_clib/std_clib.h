/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_STD_CLIB_H_
#define _ODP_TEST_STD_CLIB_H_

#include <odp_cunit_common.h>

/* test arrays: */
extern odp_testinfo_t std_clib_suite[];

/* test registry: */
extern odp_suiteinfo_t std_clib_suites[];

/* main test program: */
int std_clib_main(void);

#endif
