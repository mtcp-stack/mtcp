/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_CLASSIFICATION_H_
#define _ODP_TEST_CLASSIFICATION_H_

#include <odp_cunit_common.h>

#define SHM_PKT_NUM_BUFS        32
#define SHM_PKT_BUF_SIZE        1024

/* Config values for Default CoS */
#define TEST_DEFAULT		1
#define	CLS_DEFAULT		0
#define CLS_DEFAULT_SADDR	"10.0.0.1/32"
#define CLS_DEFAULT_DADDR	"10.0.0.100/32"
#define CLS_DEFAULT_SPORT	1024
#define CLS_DEFAULT_DPORT	2048
#define CLS_DEFAULT_DMAC	0x010203040506
#define CLS_DEFAULT_SMAC	0x060504030201

/* Config values for Error CoS */
#define TEST_ERROR		1
#define CLS_ERROR		1

/* Config values for PMR_CHAIN */
#define TEST_PMR_CHAIN		1
#define CLS_PMR_CHAIN_SRC	2
#define CLS_PMR_CHAIN_DST	3
#define CLS_PMR_CHAIN_SADDR	"10.0.0.5/32"
#define CLS_PMR_CHAIN_PORT	3000

/* Config values for PMR */
#define TEST_PMR		1
#define CLS_PMR			4
#define CLS_PMR_PORT		4000

/* Config values for PMR SET */
#define TEST_PMR_SET		1
#define CLS_PMR_SET		5
#define CLS_PMR_SET_SADDR	"10.0.0.6/32"
#define CLS_PMR_SET_PORT	5000

/* Config values for CoS L2 Priority */
#define TEST_L2_QOS		1
#define CLS_L2_QOS_0		6
#define CLS_L2_QOS_MAX		5

#define CLS_ENTRIES		(CLS_L2_QOS_0 + CLS_L2_QOS_MAX)

/* Test Packet values */
#define DATA_MAGIC		0x01020304
#define TEST_SEQ_INVALID	((uint32_t)~0)

/* test functions: */
void classification_test_create_cos(void);
void classification_test_destroy_cos(void);
void classification_test_create_pmr_match(void);
void classification_test_destroy_pmr(void);
void classification_test_cos_set_queue(void);
void classification_test_cos_set_pool(void);
void classification_test_cos_set_drop(void);
void classification_test_pmr_match_set_create(void);
void classification_test_pmr_match_set_destroy(void);

void classification_test_pktio_set_skip(void);
void classification_test_pktio_set_headroom(void);
void classification_test_pmr_terms_avail(void);
void classification_test_pmr_terms_cap(void);
void classification_test_pktio_configure(void);
void classification_test_pktio_test(void);

void classification_test_pmr_term_tcp_dport(void);
void classification_test_pmr_term_tcp_sport(void);
void classification_test_pmr_term_udp_dport(void);
void classification_test_pmr_term_udp_sport(void);
void classification_test_pmr_term_ipproto(void);
void classification_test_pmr_term_dmac(void);
void classification_test_pmr_term_packet_len(void);

/* test arrays: */
extern odp_testinfo_t classification_suite_basic[];
extern odp_testinfo_t classification_suite[];

/* test array init/term functions: */
int classification_suite_init(void);
int classification_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t classification_suites[];

/* main test program: */
int classification_main(void);

#endif
