/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_CLASSIFICATION_TESTSUITES_H_
#define ODP_CLASSIFICATION_TESTSUITES_H_

#include <odp.h>
#include <odp_cunit_common.h>

extern odp_testinfo_t classification_suite[];
extern odp_testinfo_t classification_suite_basic[];
extern odp_testinfo_t classification_suite_pmr[];

int classification_suite_init(void);
int classification_suite_term(void);

int classification_suite_pmr_term(void);
int classification_suite_pmr_init(void);

odp_packet_t create_packet(odp_pool_t pool, bool vlan,
			   odp_atomic_u32_t *seq, bool udp);
odp_packet_t create_packet_len(odp_pool_t pool, bool vlan,
			       odp_atomic_u32_t *seq, bool flag_udp,
			       uint16_t len);
int cls_pkt_set_seq(odp_packet_t pkt);
uint32_t cls_pkt_get_seq(odp_packet_t pkt);
odp_pktio_t create_pktio(odp_queue_type_t q_type);
int create_default_inq(odp_pktio_t pktio, odp_queue_type_t qtype);
void configure_default_cos(odp_pktio_t pktio, odp_cos_t *cos,
			   odp_queue_t *queue, odp_pool_t *pool);
int parse_ipv4_string(const char *ipaddress, uint32_t *addr, uint32_t *mask);
void enqueue_pktio_interface(odp_packet_t pkt, odp_pktio_t pktio);
odp_packet_t receive_packet(odp_queue_t *queue, uint64_t ns);
odp_pool_t pool_create(const char *poolname);
odp_queue_t queue_create(const char *queuename, bool sched);
void configure_pktio_default_cos(void);
void test_pktio_default_cos(void);
void configure_pktio_error_cos(void);
void test_pktio_error_cos(void);
void configure_cls_pmr_chain(void);
void test_cls_pmr_chain(void);
void configure_cos_with_l2_priority(void);
void test_cos_with_l2_priority(void);
void configure_pmr_cos(void);
void test_pmr_cos(void);
void configure_pktio_pmr_match_set_cos(void);
void test_pktio_pmr_match_set_cos(void);
int destroy_inq(odp_pktio_t pktio);
odp_pmr_term_t find_first_supported_l3_pmr(void);
int set_first_supported_pmr_port(odp_packet_t pkt, uint16_t port);

#endif /* ODP_BUFFER_TESTSUITES_H_ */
