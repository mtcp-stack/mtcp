/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#ifndef _ODP_TEST_PACKET_H_
#define _ODP_TEST_PACKET_H_

#include <odp_cunit_common.h>

/* test functions: */
void packet_test_alloc_free(void);
void packet_test_alloc_free_multi(void);
void packet_test_alloc_segmented(void);
void packet_test_event_conversion(void);
void packet_test_basic_metadata(void);
void packet_test_length(void);
void packet_test_debug(void);
void packet_test_context(void);
void packet_test_layer_offsets(void);
void packet_test_headroom(void);
void packet_test_tailroom(void);
void packet_test_segments(void);
void packet_test_segment_last(void);
void packet_test_in_flags(void);
void packet_test_error_flags(void);
void packet_test_add_rem_data(void);
void packet_test_copy(void);
void packet_test_copydata(void);
void packet_test_offset(void);

/* test arrays: */
extern odp_testinfo_t packet_suite[];

/* test array init/term functions: */
int packet_suite_init(void);
int packet_suite_term(void);

/* test registry: */
extern odp_suiteinfo_t packet_suites[];

/* main test program: */
int packet_main(void);

#endif
