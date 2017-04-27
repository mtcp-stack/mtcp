/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @file
 *
 * ODP test ring
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <odp.h>
#include <odp_common.h>
#include <odp/helper/ring.h>
#include <test_debug.h>

#define RING_SIZE 4096
#define MAX_BULK 32

#define RING_TEST_BASIC

static int test_ring_basic(odph_ring_t *r)
{
	void **src = NULL, **cur_src = NULL, **dst = NULL, **cur_dst = NULL;
	int ret;
	unsigned i, num_elems;

	/* alloc dummy object pointers */
	src = malloc(RING_SIZE*2*sizeof(void *));
	if (src == NULL) {
		LOG_ERR("failed to allocate test ring src memory\n");
		goto fail;
	}
	for (i = 0; i < RING_SIZE*2; i++)
		src[i] = (void *)(unsigned long)i;

	cur_src = src;

	/* alloc some room for copied objects */
	dst = malloc(RING_SIZE*2*sizeof(void *));
	if (dst == NULL) {
		LOG_ERR("failed to allocate test ring dst memory\n");
		goto fail;
	}

	memset(dst, 0, RING_SIZE*2*sizeof(void *));
	cur_dst = dst;

	printf("Test SP & SC basic functions\n");
	printf("enqueue 1 obj\n");
	ret = odph_ring_sp_enqueue_burst(r, cur_src, 1);
	cur_src += 1;
	if ((ret & ODPH_RING_SZ_MASK) != 1) {
		LOG_ERR("sp_enq for 1 obj failed\n");
		goto fail;
	}

	printf("enqueue 2 objs\n");
	ret = odph_ring_sp_enqueue_burst(r, cur_src, 2);
	cur_src += 2;
	if ((ret & ODPH_RING_SZ_MASK) != 2) {
		LOG_ERR("sp_enq for 2 obj failed\n");
		goto fail;
	}

	printf("enqueue MAX_BULK objs\n");
	ret = odph_ring_sp_enqueue_burst(r, cur_src, MAX_BULK);
	if ((ret & ODPH_RING_SZ_MASK) != MAX_BULK) {
		LOG_ERR("sp_enq for %d obj failed\n", MAX_BULK);
		goto fail;
	}

	printf("dequeue 1 obj\n");
	ret = odph_ring_sc_dequeue_burst(r, cur_dst, 1);
	cur_dst += 1;
	if ((ret & ODPH_RING_SZ_MASK) != 1) {
		LOG_ERR("sc_deq for 1 obj failed\n");
		goto fail;
	}

	printf("dequeue 2 objs\n");
	ret = odph_ring_sc_dequeue_burst(r, cur_dst, 2);
	cur_dst += 2;
	if ((ret & ODPH_RING_SZ_MASK) != 2) {
		LOG_ERR("sc_deq for 2 obj failed\n");
		goto fail;
	}

	printf("dequeue MAX_BULK objs\n");
	ret = odph_ring_sc_dequeue_burst(r, cur_dst, MAX_BULK);
	cur_dst += MAX_BULK;
	if ((ret & ODPH_RING_SZ_MASK) != MAX_BULK) {
		LOG_ERR("sc_deq for %d obj failed\n", MAX_BULK);
		goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		LOG_ERR("data after dequeue is not the same\n");
		goto fail;
	}

	cur_src = src;
	cur_dst = dst;

	printf("Test MP & MC basic functions\n");

	printf("enqueue 1 obj\n");
	ret = odph_ring_mp_enqueue_bulk(r, cur_src, 1);
	cur_src += 1;
	if (ret != 0) {
		LOG_ERR("mp_enq for 1 obj failed\n");
		goto fail;
	}
	printf("enqueue 2 objs\n");
	ret = odph_ring_mp_enqueue_bulk(r, cur_src, 2);
	cur_src += 2;
	if (ret != 0) {
		LOG_ERR("mp_enq for 2 obj failed\n");
		goto fail;
	}
	printf("enqueue MAX_BULK objs\n");
	ret = odph_ring_mp_enqueue_bulk(r, cur_src, MAX_BULK);
	if (ret != 0) {
		LOG_ERR("mp_enq for %d obj failed\n", MAX_BULK);
		goto fail;
	}
	printf("dequeue 1 obj\n");
	ret = odph_ring_mc_dequeue_bulk(r, cur_dst, 1);
	cur_dst += 1;
	if (ret != 0) {
		LOG_ERR("mc_deq for 1 obj failed\n");
		goto fail;
	}
	printf("dequeue 2 objs\n");
	ret = odph_ring_mc_dequeue_bulk(r, cur_dst, 2);
	cur_dst += 2;
	if (ret != 0) {
		LOG_ERR("mc_deq for 2 obj failed\n");
		goto fail;
	}
	printf("dequeue MAX_BULK objs\n");
	ret = odph_ring_mc_dequeue_bulk(r, cur_dst, MAX_BULK);
	cur_dst += MAX_BULK;
	if (ret != 0) {
		LOG_ERR("mc_deq for %d obj failed\n", MAX_BULK);
		goto fail;
	}
	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		LOG_ERR("data after dequeue is not the same\n");
		goto fail;
	}

	printf("test watermark and default bulk enqueue / dequeue\n");
	odph_ring_set_water_mark(r, 20);
	num_elems = 16;

	cur_src = src;
	cur_dst = dst;

	ret = odph_ring_mp_enqueue_bulk(r, cur_src, num_elems);
	cur_src += num_elems;
	if (ret != 0) {
		LOG_ERR("Cannot enqueue\n");
		goto fail;
	}
	ret = odph_ring_mp_enqueue_bulk(r, cur_src, num_elems);
	if (ret != -EDQUOT) {
		LOG_ERR("Watermark not exceeded\n");
		goto fail;
	}
	ret = odph_ring_mc_dequeue_bulk(r, cur_dst, num_elems);
	cur_dst += num_elems;
	if (ret != 0) {
		LOG_ERR("Cannot dequeue\n");
		goto fail;
	}
	ret = odph_ring_mc_dequeue_bulk(r, cur_dst, num_elems);
	cur_dst += num_elems;
	if (ret != 0) {
		LOG_ERR("Cannot dequeue2\n");
		goto fail;
	}

	/* check data */
	if (memcmp(src, dst, cur_dst - dst)) {
		LOG_ERR("data after dequeue is not the same\n");
		goto fail;
	}

	printf("basic enqueu, dequeue test for ring <%s>@%p passed\n",
	       r->name, r);

	free(src);
	free(dst);
	return 0;

fail:
	free(src);
	free(dst);
	return -1;
}

/* global shared ring used for stress testing */
static odph_ring_t *r_stress;

/* Stress func for Multi producer only */
static int producer_fn(void)
{
	unsigned i;

	void **src = NULL;

	/* alloc dummy object pointers */
	src = malloc(MAX_BULK*2*sizeof(void *));
	if (src == NULL) {
		LOG_ERR("failed to allocate producer memory.\n");
		return -1;
	}
	for (i = 0; i < MAX_BULK; i++)
		src[i] = (void *)(unsigned long)i;

	do {
		i = odph_ring_mp_enqueue_bulk(r_stress, src, MAX_BULK);
		if (i == 0) {
			free(src);
			return 0;
		}
	} while (1);
}

/* Stress func for Multi consumer only */
static int consumer_fn(void)
{
	unsigned i;
	void **src = NULL;

	/* alloc dummy object pointers */
	src = malloc(MAX_BULK*2*sizeof(void *));
	if (src == NULL) {
		LOG_ERR("failed to allocate consumer memory.\n");
		return -1;
	}

	do {
		i = odph_ring_mc_dequeue_bulk(r_stress, src, MAX_BULK);
		if (i == 0) {
			for (i = 0; i < MAX_BULK; i++) {
				if (src[i] != (void *)(unsigned long)i) {
					free(src);
					printf("data mismatch.. lockless ops fail\n");
					return -1;
				}
			}
			free(src);
			printf("\n Test OK !\n");
			return 0;
		}
	} while (1);
}


/*
 * Note : make sure that both enqueue and dequeue
 * operation starts at same time so to avoid data corruption
 * Its because atomic lock will protect only indexes, but if order of
 * read or write operation incorrect then data mismatch will happen
 * So its resposibility of application develop to take care of order of
 * data read or write.
*/
typedef enum {
	one_enq_one_deq,	/* One thread to enqueue one to
				   dequeu at same time */
	one_enq_rest_deq,	/* one thread to enq rest to
				   dequeue at same time */
	one_deq_rest_enq,	/* one to deq and rest enq at very same time */
	multi_enq_multi_deq     /* multiple enq,deq */
} stress_type_t;

static void test_ring_stress(stress_type_t type)
{
	int thr;
	thr = odp_thread_id();

	switch (type) {
	case one_enq_one_deq:

		if (thr == 1)
			producer_fn();
		if (thr == 2)
			consumer_fn();
		break;

	case multi_enq_multi_deq:
		if (thr%2 == 0)
			producer_fn();
		else
			consumer_fn();
		break;

	case one_deq_rest_enq:
	case one_enq_rest_deq:/*TBD*/
	default:
		LOG_ERR("Invalid stress type or test case yet not supported\n");
	}
}

/* local struct for ring_thread argument */
typedef struct {
	pthrd_arg thrdarg;
	int stress_type;
} ring_arg_t;


static void *test_ring(void *arg)
{
	ring_arg_t *parg = (ring_arg_t *)arg;
	int thr;
	char ring_name[ODPH_RING_NAMESIZE];
	odph_ring_t *r;
	int result = 0;

	thr = odp_thread_id();

	printf("Thread %i starts\n", thr);

	switch (parg->thrdarg.testcase) {
	case ODP_RING_TEST_BASIC:
		snprintf(ring_name, sizeof(ring_name), "test_ring_%i", thr);

		r = odph_ring_create(ring_name, RING_SIZE,
				    0 /* not used, alignement
					 taken care inside func : todo */);
		if (r == NULL) {
			LOG_ERR("ring create failed\n");
			result = -1;
			break;
		}
		/* lookup ring from its name */
		if (odph_ring_lookup(ring_name) != r) {
			LOG_ERR("ring lookup failed\n");
			result = -1;
			break;
		}

		/* basic operations */
		if (test_ring_basic(r) < 0) {
			LOG_ERR("ring basic enqueue/dequeu ops failed\n");
			result = -1;
		}

		/* dump ring stats */
		odph_ring_list_dump();

		break;

	case ODP_RING_TEST_STRESS:
		test_ring_stress(parg->stress_type);

		/* dump ring stats */
		odph_ring_list_dump();
		break;

	default:
		LOG_ERR("Invalid test case [%d]\n", parg->thrdarg.testcase);
		result = -1;
		break;
	}

	LOG_DBG("result = %d\n", result);
	if (result == 0)
		printf("test_ring Result:pass\n");
	else
		printf("test_ring Result:fail\n");

	fflush(stdout);

	return parg;
}


int main(int argc __attribute__((__unused__)),
	 char *argv[] __attribute__((__unused__)))
{
	ring_arg_t rarg;

	if (odp_test_global_init() != 0)
		return -1;

	odp_print_system_info();

	odph_ring_tailq_init();

	rarg.thrdarg.numthrds = odp_cpu_count();

#ifdef RING_TEST_BASIC
	rarg.thrdarg.testcase = ODP_RING_TEST_BASIC;
#else
	rarg.thrdarg.testcase = ODP_RING_TEST_STRESS;
	rarg.stress_type = one_enq_one_deq;
/*	rarg.stress_type = multi_enq_multi_deq;*/
	char ring_name[ODPH_RING_NAMESIZE];

	printf("starting stess test type : %d..\n", rarg.stress_type);
	/* create a ring */
	snprintf(ring_name, sizeof(ring_name), "test_ring_stress");

	r_stress = odph_ring_create(ring_name, RING_SIZE,
				0 /* not used, alignement
				 taken care inside func : todo */);
	if (r_stress == NULL) {
		LOG_ERR("ring create failed\n");
		goto fail;
	}
	/* lookup ring from its name */
	if (odph_ring_lookup(ring_name) != r_stress) {
		LOG_ERR("ring lookup failed\n");
		goto fail;
	}
#endif
	odp_test_thread_create(test_ring, (pthrd_arg *)&rarg);

#ifndef RING_TEST_BASIC
fail:
#endif

	odp_test_thread_exit(&rarg.thrdarg);

	return 0;
}

