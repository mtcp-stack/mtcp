/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <odp.h>
#include "odp_cunit_common.h"
#include "time.h"

#define BUSY_LOOP_CNT		30000000    /* used for t > min resolution */
#define BUSY_LOOP_CNT_LONG	12000000000 /* used for t > 4 sec */
#define MIN_TIME_RATE		32000
#define MAX_TIME_RATE		15000000000
#define DELAY_TOLERANCE		20000000	    /* deviation for delay */

static uint64_t local_res;
static uint64_t global_res;

typedef odp_time_t time_cb(void);
typedef uint64_t time_res_cb(void);
typedef odp_time_t time_from_ns_cb(uint64_t ns);

void time_test_constants(void)
{
	uint64_t ns;

	ns = ODP_TIME_SEC_IN_NS / 1000;
	CU_ASSERT(ns == ODP_TIME_MSEC_IN_NS);
	ns /= 1000;
	CU_ASSERT(ns == ODP_TIME_USEC_IN_NS);
}

static void time_test_res(time_res_cb time_res, uint64_t *res)
{
	uint64_t rate;

	rate = time_res();
	CU_ASSERT(rate > MIN_TIME_RATE);
	CU_ASSERT(rate < MAX_TIME_RATE);

	*res = ODP_TIME_SEC_IN_NS / rate;
	if (ODP_TIME_SEC_IN_NS % rate)
		(*res)++;
}

void time_test_local_res(void)
{
	time_test_res(odp_time_local_res, &local_res);
}

void time_test_global_res(void)
{
	time_test_res(odp_time_global_res, &global_res);
}

/* check that related conversions come back to the same value */
static void time_test_conversion(time_from_ns_cb time_from_ns, uint64_t res)
{
	uint64_t ns1, ns2;
	odp_time_t time;
	uint64_t upper_limit, lower_limit;

	ns1 = 100;
	time = time_from_ns(ns1);

	ns2 = odp_time_to_ns(time);

	/* need to check within arithmetic tolerance that the same
	 * value in ns is returned after conversions */
	upper_limit = ns1 + res;
	lower_limit = ns1 - res;
	CU_ASSERT((ns2 <= upper_limit) && (ns2 >= lower_limit));

	ns1 = 60 * 11 * ODP_TIME_SEC_IN_NS;
	time = time_from_ns(ns1);

	ns2 = odp_time_to_ns(time);

	/* need to check within arithmetic tolerance that the same
	 * value in ns is returned after conversions */
	upper_limit = ns1 + res;
	lower_limit = ns1 - res;
	CU_ASSERT((ns2 <= upper_limit) && (ns2 >= lower_limit));

	/* test on 0 */
	ns1 = odp_time_to_ns(ODP_TIME_NULL);
	CU_ASSERT(ns1 == 0);
}

void time_test_local_conversion(void)
{
	time_test_conversion(odp_time_local_from_ns, local_res);
}

void time_test_global_conversion(void)
{
	time_test_conversion(odp_time_global_from_ns, global_res);
}

static void time_test_monotony(time_cb time)
{
	volatile uint64_t count = 0;
	odp_time_t t1, t2, t3;
	uint64_t ns1, ns2, ns3;

	t1 = time();

	while (count < BUSY_LOOP_CNT) {
		count++;
	};

	t2 = time();

	while (count < BUSY_LOOP_CNT_LONG) {
		count++;
	};

	t3 = time();

	ns1 = odp_time_to_ns(t1);
	ns2 = odp_time_to_ns(t2);
	ns3 = odp_time_to_ns(t3);

	CU_ASSERT(ns2 > ns1);
	CU_ASSERT(ns3 > ns2);
}

void time_test_local_monotony(void)
{
	time_test_monotony(odp_time_local);
}

void time_test_global_monotony(void)
{
	time_test_monotony(odp_time_global);
}

static void time_test_cmp(time_cb time, time_from_ns_cb time_from_ns)
{
	/* volatile to stop optimization of busy loop */
	volatile int count = 0;
	odp_time_t t1, t2, t3;

	t1 = time();

	while (count < BUSY_LOOP_CNT) {
		count++;
	};

	t2 = time();

	while (count < BUSY_LOOP_CNT * 2) {
		count++;
	};

	t3 = time();

	CU_ASSERT(odp_time_cmp(t2, t1) > 0);
	CU_ASSERT(odp_time_cmp(t3, t2) > 0);
	CU_ASSERT(odp_time_cmp(t3, t1) > 0);
	CU_ASSERT(odp_time_cmp(t1, t2) < 0);
	CU_ASSERT(odp_time_cmp(t2, t3) < 0);
	CU_ASSERT(odp_time_cmp(t1, t3) < 0);
	CU_ASSERT(odp_time_cmp(t1, t1) == 0);
	CU_ASSERT(odp_time_cmp(t2, t2) == 0);
	CU_ASSERT(odp_time_cmp(t3, t3) == 0);

	t2 = time_from_ns(60 * 10 * ODP_TIME_SEC_IN_NS);
	t1 = time_from_ns(3);

	CU_ASSERT(odp_time_cmp(t2, t1) > 0);
	CU_ASSERT(odp_time_cmp(t1, t2) < 0);

	t1 = time_from_ns(0);
	CU_ASSERT(odp_time_cmp(t1, ODP_TIME_NULL) == 0);
}

void time_test_local_cmp(void)
{
	time_test_cmp(odp_time_local, odp_time_local_from_ns);
}

void time_test_global_cmp(void)
{
	time_test_cmp(odp_time_global, odp_time_global_from_ns);
}

/* check that a time difference gives a reasonable result */
static void time_test_diff(time_cb time,
			   time_from_ns_cb time_from_ns,
			   uint64_t res)
{
	/* volatile to stop optimization of busy loop */
	volatile int count = 0;
	odp_time_t diff, t1, t2;
	uint64_t nsdiff, ns1, ns2, ns;
	uint64_t upper_limit, lower_limit;

	/* test timestamp diff */
	t1 = time();

	while (count < BUSY_LOOP_CNT) {
		count++;
	};

	t2 = time();
	CU_ASSERT(odp_time_cmp(t2, t1) > 0);

	diff = odp_time_diff(t2, t1);
	CU_ASSERT(odp_time_cmp(diff, ODP_TIME_NULL) > 0);

	ns1 = odp_time_to_ns(t1);
	ns2 = odp_time_to_ns(t2);
	ns = ns2 - ns1;
	nsdiff = odp_time_to_ns(diff);

	upper_limit = ns + 2 * res;
	lower_limit = ns - 2 * res;
	CU_ASSERT((nsdiff <= upper_limit) && (nsdiff >= lower_limit));

	/* test timestamp and interval diff */
	ns1 = 54;
	t1 = time_from_ns(ns1);
	ns = ns2 - ns1;

	diff = odp_time_diff(t2, t1);
	CU_ASSERT(odp_time_cmp(diff, ODP_TIME_NULL) > 0);
	nsdiff = odp_time_to_ns(diff);

	upper_limit = ns + 2 * res;
	lower_limit = ns - 2 * res;
	CU_ASSERT((nsdiff <= upper_limit) && (nsdiff >= lower_limit));

	/* test interval diff */
	ns2 = 60 * 10 * ODP_TIME_SEC_IN_NS;
	ns = ns2 - ns1;

	t2 = time_from_ns(ns2);
	diff = odp_time_diff(t2, t1);
	CU_ASSERT(odp_time_cmp(diff, ODP_TIME_NULL) > 0);
	nsdiff = odp_time_to_ns(diff);

	upper_limit = ns + 2 * res;
	lower_limit = ns - 2 * res;
	CU_ASSERT((nsdiff <= upper_limit) && (nsdiff >= lower_limit));

	/* same time has to diff to 0 */
	diff = odp_time_diff(t2, t2);
	CU_ASSERT(odp_time_cmp(diff, ODP_TIME_NULL) == 0);

	diff = odp_time_diff(t2, ODP_TIME_NULL);
	CU_ASSERT(odp_time_cmp(t2, diff) == 0);
}

void time_test_local_diff(void)
{
	time_test_diff(odp_time_local, odp_time_local_from_ns, local_res);
}

void time_test_global_diff(void)
{
	time_test_diff(odp_time_global, odp_time_global_from_ns, global_res);
}

/* check that a time sum gives a reasonable result */
static void time_test_sum(time_cb time,
			  time_from_ns_cb time_from_ns,
			  uint64_t res)
{
	odp_time_t sum, t1, t2;
	uint64_t nssum, ns1, ns2, ns;
	uint64_t upper_limit, lower_limit;

	/* sum timestamp and interval */
	t1 = time();
	ns2 = 103;
	t2 = time_from_ns(ns2);
	ns1 = odp_time_to_ns(t1);
	ns = ns1 + ns2;

	sum = odp_time_sum(t2, t1);
	CU_ASSERT(odp_time_cmp(sum, ODP_TIME_NULL) > 0);
	nssum = odp_time_to_ns(sum);

	upper_limit = ns + 2 * res;
	lower_limit = ns - 2 * res;
	CU_ASSERT((nssum <= upper_limit) && (nssum >= lower_limit));

	/* sum intervals */
	ns1 = 60 * 13 * ODP_TIME_SEC_IN_NS;
	t1 = time_from_ns(ns1);
	ns = ns1 + ns2;

	sum = odp_time_sum(t2, t1);
	CU_ASSERT(odp_time_cmp(sum, ODP_TIME_NULL) > 0);
	nssum = odp_time_to_ns(sum);

	upper_limit = ns + 2 * res;
	lower_limit = ns - 2 * res;
	CU_ASSERT((nssum <= upper_limit) && (nssum >= lower_limit));

	/* test on 0 */
	sum = odp_time_sum(t2, ODP_TIME_NULL);
	CU_ASSERT(odp_time_cmp(t2, sum) == 0);
}

void time_test_local_sum(void)
{
	time_test_sum(odp_time_local, odp_time_local_from_ns, local_res);
}

void time_test_global_sum(void)
{
	time_test_sum(odp_time_global, odp_time_global_from_ns, global_res);
}

static void time_test_wait_until(time_cb time, time_from_ns_cb time_from_ns)
{
	int i;
	odp_time_t lower_limit, upper_limit;
	odp_time_t start_time, end_time, wait;
	odp_time_t second = time_from_ns(ODP_TIME_SEC_IN_NS);

	start_time = time();
	wait = start_time;
	for (i = 1; i < 6; i++) {
		wait = odp_time_sum(wait, second);
		odp_time_wait_until(wait);
		printf("%d..", i);
	}
	end_time = time();

	wait = odp_time_diff(end_time, start_time);
	lower_limit = time_from_ns(5 * ODP_TIME_SEC_IN_NS - DELAY_TOLERANCE);
	upper_limit = time_from_ns(5 * ODP_TIME_SEC_IN_NS + DELAY_TOLERANCE);

	CU_ASSERT(odp_time_cmp(wait, lower_limit) >= 0);
	CU_ASSERT(odp_time_cmp(wait, upper_limit) <= 0);
}

void time_test_local_wait_until(void)
{
	time_test_wait_until(odp_time_local, odp_time_local_from_ns);
}

void time_test_global_wait_until(void)
{
	time_test_wait_until(odp_time_global, odp_time_global_from_ns);
}

void time_test_wait_ns(void)
{
	int i;
	odp_time_t lower_limit, upper_limit;
	odp_time_t start_time, end_time, diff;

	start_time = odp_time_local();
	for (i = 1; i < 6; i++) {
		odp_time_wait_ns(ODP_TIME_SEC_IN_NS);
		printf("%d..", i);
	}
	end_time = odp_time_local();

	diff = odp_time_diff(end_time, start_time);

	lower_limit = odp_time_local_from_ns(5 * ODP_TIME_SEC_IN_NS -
							DELAY_TOLERANCE);
	upper_limit = odp_time_local_from_ns(5 * ODP_TIME_SEC_IN_NS +
							DELAY_TOLERANCE);

	CU_ASSERT(odp_time_cmp(diff, lower_limit) >= 0);
	CU_ASSERT(odp_time_cmp(diff, upper_limit) <= 0);
}

static void time_test_to_u64(time_cb time)
{
	volatile int count = 0;
	uint64_t val1, val2;
	odp_time_t t1, t2;

	t1 = time();

	val1 = odp_time_to_u64(t1);
	CU_ASSERT(val1 > 0);

	while (count < BUSY_LOOP_CNT) {
		count++;
	};

	t2 = time();
	val2 = odp_time_to_u64(t2);
	CU_ASSERT(val2 > 0);

	CU_ASSERT(val2 > val1);

	val1 = odp_time_to_u64(ODP_TIME_NULL);
	CU_ASSERT(val1 == 0);
}

void time_test_local_to_u64(void)
{
	time_test_to_u64(odp_time_local);
}

void time_test_global_to_u64(void)
{
	time_test_to_u64(odp_time_global);
}

odp_testinfo_t time_suite_time[] = {
	ODP_TEST_INFO(time_test_constants),
	ODP_TEST_INFO(time_test_local_res),
	ODP_TEST_INFO(time_test_local_conversion),
	ODP_TEST_INFO(time_test_local_monotony),
	ODP_TEST_INFO(time_test_local_cmp),
	ODP_TEST_INFO(time_test_local_diff),
	ODP_TEST_INFO(time_test_local_sum),
	ODP_TEST_INFO(time_test_local_wait_until),
	ODP_TEST_INFO(time_test_wait_ns),
	ODP_TEST_INFO(time_test_local_to_u64),
	ODP_TEST_INFO(time_test_global_res),
	ODP_TEST_INFO(time_test_global_conversion),
	ODP_TEST_INFO(time_test_global_monotony),
	ODP_TEST_INFO(time_test_global_cmp),
	ODP_TEST_INFO(time_test_global_diff),
	ODP_TEST_INFO(time_test_global_sum),
	ODP_TEST_INFO(time_test_global_wait_until),
	ODP_TEST_INFO(time_test_global_to_u64),
	ODP_TEST_INFO_NULL
};

odp_suiteinfo_t time_suites[] = {
		{"Time", NULL, NULL, time_suite_time},
		ODP_SUITE_INFO_NULL
};

int time_main(void)
{
	int ret = odp_cunit_register(time_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
