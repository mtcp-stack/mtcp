/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_posix_extensions.h>

#include <time.h>
#include <odp/time.h>
#include <odp/hints.h>
#include <odp_debug_internal.h>

typedef union {
	odp_time_t      ex;
	struct timespec in;
} _odp_time_t;

static odp_time_t start_time;

static inline
uint64_t time_to_ns(odp_time_t time)
{
	uint64_t ns;

	ns = time.tv_sec * ODP_TIME_SEC_IN_NS;
	ns += time.tv_nsec;

	return ns;
}

static inline odp_time_t time_diff(odp_time_t t2, odp_time_t t1)
{
	odp_time_t time;

	time.tv_sec = t2.tv_sec - t1.tv_sec;
	time.tv_nsec = t2.tv_nsec - t1.tv_nsec;

	if (time.tv_nsec < 0) {
		time.tv_nsec += ODP_TIME_SEC_IN_NS;
		--time.tv_sec;
	}

	return time;
}

static inline odp_time_t time_local(void)
{
	int ret;
	_odp_time_t time;

	ret = clock_gettime(CLOCK_MONOTONIC_RAW, &time.in);
	if (odp_unlikely(ret != 0))
		ODP_ABORT("clock_gettime failed\n");

	return time_diff(time.ex, start_time);
}

static inline int time_cmp(odp_time_t t2, odp_time_t t1)
{
	if (t2.tv_sec < t1.tv_sec)
		return -1;

	if (t2.tv_sec > t1.tv_sec)
		return 1;

	return t2.tv_nsec - t1.tv_nsec;
}

static inline odp_time_t time_sum(odp_time_t t1, odp_time_t t2)
{
	odp_time_t time;

	time.tv_sec = t2.tv_sec + t1.tv_sec;
	time.tv_nsec = t2.tv_nsec + t1.tv_nsec;

	if (time.tv_nsec >= (long)ODP_TIME_SEC_IN_NS) {
		time.tv_nsec -= ODP_TIME_SEC_IN_NS;
		++time.tv_sec;
	}

	return time;
}

static inline odp_time_t time_local_from_ns(uint64_t ns)
{
	odp_time_t time;

	time.tv_sec = ns / ODP_TIME_SEC_IN_NS;
	time.tv_nsec = ns - time.tv_sec * ODP_TIME_SEC_IN_NS;

	return time;
}

static inline void time_wait_until(odp_time_t time)
{
	odp_time_t cur;

	do {
		cur = time_local();
	} while (time_cmp(time, cur) > 0);
}

static inline uint64_t time_local_res(void)
{
	int ret;
	struct timespec tres;

	ret = clock_getres(CLOCK_MONOTONIC_RAW, &tres);
	if (odp_unlikely(ret != 0))
		ODP_ABORT("clock_getres failed\n");

	return ODP_TIME_SEC_IN_NS / (uint64_t)tres.tv_nsec;
}

odp_time_t odp_time_local(void)
{
	return time_local();
}

odp_time_t odp_time_global(void)
{
	return time_local();
}

odp_time_t odp_time_diff(odp_time_t t2, odp_time_t t1)
{
	return time_diff(t2, t1);
}

uint64_t odp_time_to_ns(odp_time_t time)
{
	return time_to_ns(time);
}

odp_time_t odp_time_local_from_ns(uint64_t ns)
{
	return time_local_from_ns(ns);
}

odp_time_t odp_time_global_from_ns(uint64_t ns)
{
	return time_local_from_ns(ns);
}

int odp_time_cmp(odp_time_t t2, odp_time_t t1)
{
	return time_cmp(t2, t1);
}

odp_time_t odp_time_sum(odp_time_t t1, odp_time_t t2)
{
	return time_sum(t1, t2);
}

uint64_t odp_time_local_res(void)
{
	return time_local_res();
}

uint64_t odp_time_global_res(void)
{
	return time_local_res();
}

void odp_time_wait_ns(uint64_t ns)
{
	odp_time_t cur = time_local();
	odp_time_t wait = time_local_from_ns(ns);
	odp_time_t end_time = time_sum(cur, wait);

	time_wait_until(end_time);
}

void odp_time_wait_until(odp_time_t time)
{
	return time_wait_until(time);
}

uint64_t odp_time_to_u64(odp_time_t time)
{
	int ret;
	struct timespec tres;
	uint64_t resolution;

	ret = clock_getres(CLOCK_MONOTONIC_RAW, &tres);
	if (odp_unlikely(ret != 0))
		ODP_ABORT("clock_getres failed\n");

	resolution = (uint64_t)tres.tv_nsec;

	return time_to_ns(time) / resolution;
}

int odp_time_init_global(void)
{
	int ret;
	_odp_time_t time;

	ret = clock_gettime(CLOCK_MONOTONIC_RAW, &time.in);
	start_time = ret ? ODP_TIME_NULL : time.ex;

	return ret;
}

int odp_time_term_global(void)
{
	return 0;
}
