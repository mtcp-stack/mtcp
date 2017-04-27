/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_MASK_COMMON_H_
#define ODP_MASK_COMMON_H_

/*
 * The same set of tests are used for testing both the odp_thrmask_ and
 * odp_cpumask_ APIs.
 *
 * To build the thrmask tests TEST_THRMASK must be defined.
 */
#ifdef TEST_THRMASK
typedef odp_thrmask_t _odp_mask_t;
#define MASK_API_PREFIX(n) odp_thrmask_##n
#define MASK_TESTFUNC(n) void thread_test_odp_thrmask_##n(void)
#else
typedef odp_cpumask_t _odp_mask_t;
#define MASK_API_PREFIX(n) odp_cpumask_##n
#define MASK_TESTFUNC(n) void cpumask_test_odp_cpumask_##n(void)
#endif

#define _odp_mask_from_str MASK_API_PREFIX(from_str)
#define _odp_mask_to_str   MASK_API_PREFIX(to_str)
#define _odp_mask_equal    MASK_API_PREFIX(equal)
#define _odp_mask_zero     MASK_API_PREFIX(zero)
#define _odp_mask_set      MASK_API_PREFIX(set)
#define _odp_mask_clr      MASK_API_PREFIX(clr)
#define _odp_mask_isset    MASK_API_PREFIX(isset)
#define _odp_mask_count    MASK_API_PREFIX(count)
#define _odp_mask_and      MASK_API_PREFIX(and)
#define _odp_mask_or       MASK_API_PREFIX(or)
#define _odp_mask_xor      MASK_API_PREFIX(xor)
#define _odp_mask_copy     MASK_API_PREFIX(copy)
#define _odp_mask_first    MASK_API_PREFIX(first)
#define _odp_mask_next     MASK_API_PREFIX(next)
#define _odp_mask_last     MASK_API_PREFIX(last)
#define _odp_mask_setall   MASK_API_PREFIX(setall)

unsigned mask_capacity(void);

MASK_TESTFUNC(to_from_str);
MASK_TESTFUNC(equal);
MASK_TESTFUNC(zero);
MASK_TESTFUNC(set);
MASK_TESTFUNC(clr);
MASK_TESTFUNC(isset);
MASK_TESTFUNC(count);
MASK_TESTFUNC(and);
MASK_TESTFUNC(or);
MASK_TESTFUNC(xor);
MASK_TESTFUNC(copy);
MASK_TESTFUNC(first);
MASK_TESTFUNC(last);
MASK_TESTFUNC(next);
MASK_TESTFUNC(setall);

#endif
