/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "queue.h"

#define MAX_BUFFER_QUEUE        (8)
#define MSG_POOL_SIZE           (4 * 1024 * 1024)
#define CONFIG_MAX_ITERATION    (100)

static int queue_contest = 0xff;
static odp_pool_t pool;

int queue_suite_init(void)
{
	odp_pool_param_t params;

	params.buf.size  = 0;
	params.buf.align = ODP_CACHE_LINE_SIZE;
	params.buf.num   = 1024 * 10;
	params.type      = ODP_POOL_BUFFER;

	pool = odp_pool_create("msg_pool", &params);
	if (ODP_POOL_INVALID == pool) {
		printf("Pool create failed.\n");
		return -1;
	}
	return 0;
}

int queue_suite_term(void)
{
	return odp_pool_destroy(pool);
}

void queue_test_sunnydays(void)
{
	odp_queue_t queue_creat_id, queue_id;
	odp_event_t enev[MAX_BUFFER_QUEUE];
	odp_event_t deev[MAX_BUFFER_QUEUE];
	odp_buffer_t buf;
	odp_event_t ev;
	odp_pool_t msg_pool;
	odp_event_t *pev_tmp;
	int i, deq_ret, ret;
	int nr_deq_entries = 0;
	int max_iteration = CONFIG_MAX_ITERATION;
	void *prtn = NULL;
	odp_queue_param_t qparams;

	odp_queue_param_init(&qparams);
	qparams.type       = ODP_QUEUE_TYPE_SCHED;
	qparams.sched.prio = ODP_SCHED_PRIO_LOWEST;
	qparams.sched.sync = ODP_SCHED_SYNC_PARALLEL;
	qparams.sched.group = ODP_SCHED_GROUP_WORKER;

	queue_creat_id = odp_queue_create("test_queue", &qparams);
	CU_ASSERT(ODP_QUEUE_INVALID != queue_creat_id);
	CU_ASSERT(odp_queue_to_u64(queue_creat_id) !=
		  odp_queue_to_u64(ODP_QUEUE_INVALID));

	CU_ASSERT_EQUAL(ODP_QUEUE_TYPE_SCHED,
			odp_queue_type(queue_creat_id));

	queue_id = odp_queue_lookup("test_queue");
	CU_ASSERT_EQUAL(queue_creat_id, queue_id);

	CU_ASSERT_EQUAL(ODP_SCHED_GROUP_WORKER,
			odp_queue_sched_group(queue_id));
	CU_ASSERT_EQUAL(ODP_SCHED_PRIO_LOWEST, odp_queue_sched_prio(queue_id));
	CU_ASSERT_EQUAL(ODP_SCHED_SYNC_PARALLEL,
			odp_queue_sched_type(queue_id));

	CU_ASSERT(0 == odp_queue_context_set(queue_id, &queue_contest));

	prtn = odp_queue_context(queue_id);
	CU_ASSERT(&queue_contest == (int *)prtn);

	msg_pool = odp_pool_lookup("msg_pool");
	buf = odp_buffer_alloc(msg_pool);
	CU_ASSERT_FATAL(buf != ODP_BUFFER_INVALID);
	ev  = odp_buffer_to_event(buf);

	if (!(CU_ASSERT(odp_queue_enq(queue_id, ev) == 0))) {
		odp_buffer_free(buf);
	} else {
		CU_ASSERT_EQUAL(ev, odp_queue_deq(queue_id));
		odp_buffer_free(buf);
	}

	for (i = 0; i < MAX_BUFFER_QUEUE; i++) {
		odp_buffer_t buf = odp_buffer_alloc(msg_pool);
		enev[i] = odp_buffer_to_event(buf);
	}

	/*
	 * odp_queue_enq_multi may return 0..n buffers due to the resource
	 * constraints in the implementation at that given point of time.
	 * But here we assume that we succeed in enqueuing all buffers.
	 */
	ret = odp_queue_enq_multi(queue_id, enev, MAX_BUFFER_QUEUE);
	CU_ASSERT(MAX_BUFFER_QUEUE == ret);
	i = ret < 0 ? 0 : ret;
	for ( ; i < MAX_BUFFER_QUEUE; i++)
		odp_event_free(enev[i]);

	pev_tmp = deev;
	do {
		deq_ret  = odp_queue_deq_multi(queue_id, pev_tmp,
					       MAX_BUFFER_QUEUE);
		nr_deq_entries += deq_ret;
		max_iteration--;
		pev_tmp += deq_ret;
		CU_ASSERT(max_iteration >= 0);
	} while (nr_deq_entries < MAX_BUFFER_QUEUE);

	for (i = 0; i < MAX_BUFFER_QUEUE; i++) {
		odp_buffer_t enbuf = odp_buffer_from_event(enev[i]);
		CU_ASSERT_EQUAL(enev[i], deev[i]);
		odp_buffer_free(enbuf);
	}

	CU_ASSERT(odp_queue_destroy(queue_id) == 0);
}

void queue_test_info(void)
{
	odp_queue_t q_plain, q_order;
	const char *const nq_plain = "test_q_plain";
	const char *const nq_order = "test_q_order";
	odp_queue_info_t info;
	odp_queue_param_t param;
	char q_plain_ctx[] = "test_q_plain context data";
	char q_order_ctx[] = "test_q_order context data";
	unsigned lock_count;
	char *ctx;
	int ret;

	/* Create a plain queue and set context */
	q_plain = odp_queue_create(nq_plain, NULL);
	CU_ASSERT(ODP_QUEUE_INVALID != q_plain);
	CU_ASSERT(odp_queue_context_set(q_plain, q_plain_ctx) == 0);

	/* Create a scheduled ordered queue with explicitly set params */
	odp_queue_param_init(&param);
	param.type       = ODP_QUEUE_TYPE_SCHED;
	param.sched.prio = ODP_SCHED_PRIO_NORMAL;
	param.sched.sync = ODP_SCHED_SYNC_ORDERED;
	param.sched.group = ODP_SCHED_GROUP_ALL;
	param.sched.lock_count = 1;
	param.context = q_order_ctx;
	q_order = odp_queue_create(nq_order, &param);
	CU_ASSERT(ODP_QUEUE_INVALID != q_order);

	/* Check info for the plain queue */
	CU_ASSERT(odp_queue_info(q_plain, &info) == 0);
	CU_ASSERT(strcmp(nq_plain, info.name) == 0);
	CU_ASSERT(info.param.type == ODP_QUEUE_TYPE_PLAIN);
	CU_ASSERT(info.param.type == odp_queue_type(q_plain));
	ctx = info.param.context; /* 'char' context ptr */
	CU_ASSERT(ctx == q_plain_ctx);
	CU_ASSERT(info.param.context == odp_queue_context(q_plain));

	/* Check info for the scheduled ordered queue */
	CU_ASSERT(odp_queue_info(q_order, &info) == 0);
	CU_ASSERT(strcmp(nq_order, info.name) == 0);
	CU_ASSERT(info.param.type == ODP_QUEUE_TYPE_SCHED);
	CU_ASSERT(info.param.type == odp_queue_type(q_order));
	ctx = info.param.context; /* 'char' context ptr */
	CU_ASSERT(ctx == q_order_ctx);
	CU_ASSERT(info.param.context == odp_queue_context(q_order));
	CU_ASSERT(info.param.sched.prio == odp_queue_sched_prio(q_order));
	CU_ASSERT(info.param.sched.sync == odp_queue_sched_type(q_order));
	CU_ASSERT(info.param.sched.group == odp_queue_sched_group(q_order));
	ret = odp_queue_lock_count(q_order);
	CU_ASSERT(ret >= 0);
	lock_count = (unsigned) ret;
	CU_ASSERT(info.param.sched.lock_count == lock_count);

	CU_ASSERT(odp_queue_destroy(q_plain) == 0);
	CU_ASSERT(odp_queue_destroy(q_order) == 0);
}

odp_testinfo_t queue_suite[] = {
	ODP_TEST_INFO(queue_test_sunnydays),
	ODP_TEST_INFO(queue_test_info),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t queue_suites[] = {
	{"Queue", queue_suite_init, queue_suite_term, queue_suite},
	ODP_SUITE_INFO_NULL,
};

int queue_main(void)
{
	int ret = odp_cunit_register(queue_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
