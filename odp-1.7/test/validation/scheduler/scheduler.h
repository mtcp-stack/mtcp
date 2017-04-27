/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_SCHEDULER_H_
#define _ODP_TEST_SCHEDULER_H_

#include <odp_cunit_common.h>

/* test functions: */
void scheduler_test_wait_time(void);
void scheduler_test_num_prio(void);
void scheduler_test_queue_destroy(void);
void scheduler_test_groups(void);
void scheduler_test_chaos(void);
void scheduler_test_parallel(void);
void scheduler_test_atomic(void);
void scheduler_test_ordered(void);
void scheduler_test_1q_1t_n(void);
void scheduler_test_1q_1t_a(void);
void scheduler_test_1q_1t_o(void);
void scheduler_test_mq_1t_n(void);
void scheduler_test_mq_1t_a(void);
void scheduler_test_mq_1t_o(void);
void scheduler_test_mq_1t_prio_n(void);
void scheduler_test_mq_1t_prio_a(void);
void scheduler_test_mq_1t_prio_o(void);
void scheduler_test_mq_mt_prio_n(void);
void scheduler_test_mq_mt_prio_a(void);
void scheduler_test_mq_mt_prio_o(void);
void scheduler_test_1q_mt_a_excl(void);
void scheduler_test_multi_1q_1t_n(void);
void scheduler_test_multi_1q_1t_a(void);
void scheduler_test_multi_1q_1t_o(void);
void scheduler_test_multi_mq_1t_n(void);
void scheduler_test_multi_mq_1t_a(void);
void scheduler_test_multi_mq_1t_o(void);
void scheduler_test_multi_mq_1t_prio_n(void);
void scheduler_test_multi_mq_1t_prio_a(void);
void scheduler_test_multi_mq_1t_prio_o(void);
void scheduler_test_multi_mq_mt_prio_n(void);
void scheduler_test_multi_mq_mt_prio_a(void);
void scheduler_test_multi_mq_mt_prio_o(void);
void scheduler_test_multi_1q_mt_a_excl(void);
void scheduler_test_pause_resume(void);

/* test arrays: */
extern odp_testinfo_t scheduler_suite[];

/* test array init/term functions: */
int scheduler_suite_init(void);
int scheduler_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t scheduler_suites[];

/* main test program: */
int scheduler_main(void);

#endif
