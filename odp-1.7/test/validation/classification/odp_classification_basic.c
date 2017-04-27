/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp_cunit_common.h>
#include "odp_classification_testsuites.h"
#include "classification.h"

#define PMR_SET_NUM	5

void classification_test_create_cos(void)
{
	odp_cos_t cos;
	odp_cls_cos_param_t cls_param;
	odp_pool_t pool;
	odp_queue_t queue;
	char cosname[ODP_COS_NAME_LEN];

	pool = pool_create("cls_basic_pool");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	queue = queue_create("cls_basic_queue", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	sprintf(cosname, "ClassOfService");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT(odp_cos_to_u64(cos) != odp_cos_to_u64(ODP_COS_INVALID));
	odp_cos_destroy(cos);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
}

void classification_test_destroy_cos(void)
{
	odp_cos_t cos;
	char name[ODP_COS_NAME_LEN];
	odp_pool_t pool;
	odp_queue_t queue;
	odp_cls_cos_param_t cls_param;
	int retval;

	pool = pool_create("cls_basic_pool");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	queue = queue_create("cls_basic_queue", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	sprintf(name, "ClassOfService");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;

	cos = odp_cls_cos_create(name, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);
	retval = odp_cos_destroy(cos);
	CU_ASSERT(retval == 0);
	retval = odp_cos_destroy(ODP_COS_INVALID);
	CU_ASSERT(retval < 0);

	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
}

void classification_test_create_pmr_match(void)
{
	odp_pmr_t pmr;
	uint16_t val;
	uint16_t mask;
	odp_pmr_match_t match;

	val = 1024;
	mask = 0xffff;
	match.term = find_first_supported_l3_pmr();
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	CU_ASSERT(pmr != ODP_PMR_INVAL);
	CU_ASSERT(odp_pmr_to_u64(pmr) != odp_pmr_to_u64(ODP_PMR_INVAL));
	odp_pmr_destroy(pmr);
}

void classification_test_destroy_pmr(void)
{
	odp_pmr_t pmr;
	uint16_t val;
	uint16_t mask;
	int retval;
	odp_pmr_match_t match;

	val = 1024;
	mask = 0xffff;
	match.term = find_first_supported_l3_pmr();
	match.val = &val;
	match.mask = &mask;
	match.val_sz = sizeof(val);

	pmr = odp_pmr_create(&match);
	retval = odp_pmr_destroy(pmr);
	CU_ASSERT(retval == 0);
	retval = odp_pmr_destroy(ODP_PMR_INVAL);
	retval = odp_pmr_destroy(ODP_PMR_INVAL);
	CU_ASSERT(retval < 0);
}

void classification_test_cos_set_queue(void)
{
	int retval;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pool_t pool;
	odp_queue_t queue;
	odp_queue_t queue_cos;
	odp_cos_t cos_queue;
	odp_queue_t recvqueue;

	pool = pool_create("cls_basic_pool");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	queue = queue_create("cls_basic_queue", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	sprintf(cosname, "CoSQueue");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_queue = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_queue != ODP_COS_INVALID);

	queue_cos = queue_create("QueueCoS", true);
	CU_ASSERT_FATAL(queue_cos != ODP_QUEUE_INVALID);

	retval = odp_cos_queue_set(cos_queue, queue_cos);
	CU_ASSERT(retval == 0);
	recvqueue = odp_cos_queue(cos_queue);
	CU_ASSERT(recvqueue == queue_cos);

	odp_cos_destroy(cos_queue);
	odp_queue_destroy(queue_cos);
	odp_queue_destroy(queue);
	odp_pool_destroy(pool);
}

void classification_test_cos_set_pool(void)
{
	int retval;
	char cosname[ODP_COS_NAME_LEN];
	odp_cls_cos_param_t cls_param;
	odp_pool_t pool;
	odp_queue_t queue;
	odp_pool_t cos_pool;
	odp_cos_t cos;
	odp_pool_t recvpool;

	pool = pool_create("cls_basic_pool");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	queue = queue_create("cls_basic_queue", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	sprintf(cosname, "CoSQueue");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos != ODP_COS_INVALID);

	cos_pool = pool_create("PoolCoS");
	CU_ASSERT_FATAL(cos_pool != ODP_POOL_INVALID);

	retval = odp_cls_cos_pool_set(cos, cos_pool);
	CU_ASSERT(retval == 0);
	recvpool = odp_cls_cos_pool(cos);
	CU_ASSERT(recvpool == cos_pool);

	odp_cos_destroy(cos);
	odp_queue_destroy(queue);
	odp_pool_destroy(pool);
	odp_pool_destroy(cos_pool);
}

void classification_test_cos_set_drop(void)
{
	int retval;
	char cosname[ODP_COS_NAME_LEN];
	odp_cos_t cos_drop;
	odp_queue_t queue;
	odp_pool_t pool;
	odp_cls_cos_param_t cls_param;

	pool = pool_create("cls_basic_pool");
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);

	queue = queue_create("cls_basic_queue", true);
	CU_ASSERT_FATAL(queue != ODP_QUEUE_INVALID);

	sprintf(cosname, "CoSDrop");
	odp_cls_cos_param_init(&cls_param);
	cls_param.pool = pool;
	cls_param.queue = queue;
	cls_param.drop_policy = ODP_COS_DROP_POOL;
	cos_drop = odp_cls_cos_create(cosname, &cls_param);
	CU_ASSERT_FATAL(cos_drop != ODP_COS_INVALID);

	retval = odp_cos_drop_set(cos_drop, ODP_COS_DROP_POOL);
	CU_ASSERT(retval == 0);
	retval = odp_cos_drop_set(cos_drop, ODP_COS_DROP_NEVER);
	CU_ASSERT(retval == 0);
	odp_cos_destroy(cos_drop);
	odp_pool_destroy(pool);
	odp_queue_destroy(queue);
}

void classification_test_pmr_match_set_create(void)
{
	odp_pmr_set_t pmr_set;
	int retval;
	odp_pmr_match_t pmr_terms[PMR_SET_NUM];
	uint16_t val = 1024;
	uint16_t mask = 0xffff;
	int i;
	for (i = 0; i < PMR_SET_NUM; i++) {
		pmr_terms[i].term = ODP_PMR_TCP_DPORT;
		pmr_terms[i].val = &val;
		pmr_terms[i].mask = &mask;
		pmr_terms[i].val_sz = sizeof(val);
	}

	retval = odp_pmr_match_set_create(PMR_SET_NUM, pmr_terms, &pmr_set);
	CU_ASSERT(retval > 0);
	CU_ASSERT(odp_pmr_set_to_u64(pmr_set) !=
		  odp_pmr_set_to_u64(ODP_PMR_SET_INVAL));

	retval = odp_pmr_match_set_destroy(pmr_set);
	CU_ASSERT(retval == 0);
}

void classification_test_pmr_match_set_destroy(void)
{
	odp_pmr_set_t pmr_set;
	int retval;
	odp_pmr_match_t pmr_terms[PMR_SET_NUM];
	uint16_t val = 1024;
	uint16_t mask = 0xffff;
	int i;

	retval = odp_pmr_match_set_destroy(ODP_PMR_SET_INVAL);
	CU_ASSERT(retval < 0);

	for (i = 0; i < PMR_SET_NUM; i++) {
		pmr_terms[i].term = ODP_PMR_TCP_DPORT;
		pmr_terms[i].val = &val;
		pmr_terms[i].mask = &mask;
		pmr_terms[i].val_sz = sizeof(val);
	}

	retval = odp_pmr_match_set_create(PMR_SET_NUM, pmr_terms, &pmr_set);
	CU_ASSERT(retval > 0);

	retval = odp_pmr_match_set_destroy(pmr_set);
	CU_ASSERT(retval == 0);
}

odp_testinfo_t classification_suite_basic[] = {
	ODP_TEST_INFO(classification_test_create_cos),
	ODP_TEST_INFO(classification_test_destroy_cos),
	ODP_TEST_INFO(classification_test_create_pmr_match),
	ODP_TEST_INFO(classification_test_destroy_pmr),
	ODP_TEST_INFO(classification_test_cos_set_queue),
	ODP_TEST_INFO(classification_test_cos_set_drop),
	ODP_TEST_INFO(classification_test_cos_set_pool),
	ODP_TEST_INFO(classification_test_pmr_match_set_create),
	ODP_TEST_INFO(classification_test_pmr_match_set_destroy),
	ODP_TEST_INFO_NULL,
};
