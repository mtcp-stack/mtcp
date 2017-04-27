/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <stdlib.h>

#include <odp.h>
#include <odp_cunit_common.h>
#include "packet.h"

#define PACKET_BUF_LEN	ODP_CONFIG_PACKET_SEG_LEN_MIN
/* Reserve some tailroom for tests */
#define PACKET_TAILROOM_RESERVE  4

static odp_pool_t packet_pool;
static const uint32_t packet_len = PACKET_BUF_LEN -
				PACKET_TAILROOM_RESERVE;

static uint32_t   segmented_packet_len = ODP_CONFIG_PACKET_BUF_LEN_MAX;
static odp_bool_t segmentation_supported = true;

odp_packet_t test_packet, segmented_test_packet;

static struct udata_struct {
	uint64_t u64;
	uint32_t u32;
	char str[10];
} test_packet_udata = {
	123456,
	789912,
	"abcdefg",
};

int packet_suite_init(void)
{
	odp_pool_param_t params = {
		.pkt = {
			.seg_len = PACKET_BUF_LEN,
			.len     = PACKET_BUF_LEN,
			.num     = 100,
			.uarea_size = sizeof(struct udata_struct),
		},
		.type  = ODP_POOL_PACKET,
	};
	struct udata_struct *udat;
	uint32_t udat_size;

	packet_pool = odp_pool_create("packet_pool", &params);
	if (packet_pool == ODP_POOL_INVALID)
		return -1;

	test_packet = odp_packet_alloc(packet_pool, packet_len);

	/* Try to allocate the largest possible packet to see
	 * if segmentation is supported  */
	do {
		segmented_test_packet = odp_packet_alloc(packet_pool,
							 segmented_packet_len);
		if (segmented_test_packet == ODP_PACKET_INVALID)
			segmented_packet_len -= ODP_CONFIG_BUFFER_ALIGN_MIN;
	} while (segmented_test_packet == ODP_PACKET_INVALID);

	if (odp_packet_is_valid(test_packet) == 0 ||
	    odp_packet_is_valid(segmented_test_packet) == 0)
		return -1;

	segmentation_supported = odp_packet_is_segmented(segmented_test_packet);

	udat = odp_packet_user_area(test_packet);
	udat_size = odp_packet_user_area_size(test_packet);
	if (!udat || udat_size != sizeof(struct udata_struct))
		return -1;
	odp_pool_print(packet_pool);
	memcpy(udat, &test_packet_udata, sizeof(struct udata_struct));

	udat = odp_packet_user_area(segmented_test_packet);
	udat_size = odp_packet_user_area_size(segmented_test_packet);
	if (udat == NULL || udat_size != sizeof(struct udata_struct))
		return -1;
	memcpy(udat, &test_packet_udata, sizeof(struct udata_struct));

	return 0;
}

int packet_suite_term(void)
{
	odp_packet_free(test_packet);
	odp_packet_free(segmented_test_packet);
	if (odp_pool_destroy(packet_pool) != 0)
		return -1;
	return 0;
}

void packet_test_alloc_free(void)
{
	odp_pool_t pool;
	odp_packet_t packet;
	odp_pool_param_t params = {
		.pkt = {
			.seg_len = PACKET_BUF_LEN,
			.len     = PACKET_BUF_LEN,
			.num     = 1,
		},
		.type  = ODP_POOL_PACKET,
	};

	pool = odp_pool_create("packet_pool_alloc", &params);

	/* Allocate the only buffer from the pool */
	packet = odp_packet_alloc(pool, packet_len);
	CU_ASSERT_FATAL(packet != ODP_PACKET_INVALID);
	CU_ASSERT(odp_packet_len(packet) == packet_len);
	CU_ASSERT(odp_event_type(odp_packet_to_event(packet)) ==
			ODP_EVENT_PACKET);
	CU_ASSERT(odp_packet_to_u64(packet) !=
		  odp_packet_to_u64(ODP_PACKET_INVALID));

	/* Pool should have only one packet */
	CU_ASSERT_FATAL(odp_packet_alloc(pool, packet_len)
			== ODP_PACKET_INVALID);

	odp_packet_free(packet);

	/* Check that the buffer was returned back to the pool */
	packet = odp_packet_alloc(pool, packet_len);
	CU_ASSERT_FATAL(packet != ODP_PACKET_INVALID);
	CU_ASSERT(odp_packet_len(packet) == packet_len);

	odp_packet_free(packet);
	CU_ASSERT(odp_pool_destroy(pool) == 0);
}

/* Wrapper to call odp_packet_alloc_multi multiple times until
 * either no mure buffers are returned, or num buffers were alloced */
static int packet_alloc_multi(odp_pool_t pool, uint32_t pkt_len,
			      odp_packet_t pkt[], int num)
{
	int ret, total = 0;

	do {
		ret = odp_packet_alloc_multi(pool, pkt_len, pkt + total,
					     num - total);
		CU_ASSERT(ret >= 0);
		CU_ASSERT(ret <= num - total);
		total += ret;
	} while (total < num && ret);

	return total;
}

void packet_test_alloc_free_multi(void)
{
	const int num_pkt = 2;
	odp_pool_t pool[2];
	int i, ret;
	odp_packet_t packet[2 * num_pkt + 1];
	odp_packet_t inval_pkt[num_pkt];
	odp_pool_param_t params = {
		.pkt = {
			.seg_len = PACKET_BUF_LEN,
			.len     = PACKET_BUF_LEN,
			.num     = num_pkt,
		},
		.type  = ODP_POOL_PACKET,
	};

	pool[0] = odp_pool_create("packet_pool_alloc_multi_0", &params);
	pool[1] = odp_pool_create("packet_pool_alloc_multi_1", &params);
	CU_ASSERT_FATAL(pool[0] != ODP_POOL_INVALID);
	CU_ASSERT_FATAL(pool[1] != ODP_POOL_INVALID);

	/* Allocate all the packets from the pools */

	ret = packet_alloc_multi(pool[0], packet_len, &packet[0], num_pkt + 1);
	CU_ASSERT_FATAL(ret == num_pkt);
	ret = packet_alloc_multi(pool[1], packet_len,
				 &packet[num_pkt], num_pkt + 1);
	CU_ASSERT_FATAL(ret == num_pkt);

	for (i = 0; i < 2 * num_pkt; ++i) {
		CU_ASSERT(odp_packet_len(packet[i]) == packet_len);
		CU_ASSERT(odp_event_type(odp_packet_to_event(packet[i])) ==
			  ODP_EVENT_PACKET);
		CU_ASSERT(odp_packet_to_u64(packet[i]) !=
			  odp_packet_to_u64(ODP_PACKET_INVALID));
	}

	/* Pools should have no more packets */
	ret = odp_packet_alloc_multi(pool[0], packet_len, inval_pkt, num_pkt);
	CU_ASSERT(ret == 0);
	ret = odp_packet_alloc_multi(pool[1], packet_len, inval_pkt, num_pkt);
	CU_ASSERT(ret == 0);

	/* Free all packets from all pools at once */
	odp_packet_free_multi(packet, 2 * num_pkt);

	/* Check that all the packets were returned back to their pools */
	ret = packet_alloc_multi(pool[0], packet_len, &packet[0], num_pkt);
	CU_ASSERT(ret);
	ret  = packet_alloc_multi(pool[1], packet_len,
				  &packet[num_pkt], num_pkt);
	CU_ASSERT(ret);

	for (i = 0; i < 2 * num_pkt; ++i) {
		CU_ASSERT_FATAL(packet[i] != ODP_PACKET_INVALID);
		CU_ASSERT(odp_packet_len(packet[i]) == packet_len);
	}
	odp_packet_free_multi(packet, 2 * num_pkt);
	CU_ASSERT(odp_pool_destroy(pool[0]) == 0);
	CU_ASSERT(odp_pool_destroy(pool[1]) == 0);
}

void packet_test_alloc_segmented(void)
{
	odp_packet_t pkt;
	const uint32_t len = ODP_CONFIG_PACKET_BUF_LEN_MAX -
			ODP_CONFIG_PACKET_HEADROOM -
			ODP_CONFIG_PACKET_TAILROOM;

	pkt = odp_packet_alloc(packet_pool, len);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);
	CU_ASSERT(odp_packet_len(pkt) == len);
	if (segmentation_supported)
		CU_ASSERT(odp_packet_is_segmented(pkt) == 1);
	odp_packet_free(pkt);
}

void packet_test_event_conversion(void)
{
	odp_packet_t pkt = test_packet;
	odp_packet_t tmp_pkt;
	odp_event_t ev;

	ev = odp_packet_to_event(pkt);
	CU_ASSERT_FATAL(ev != ODP_EVENT_INVALID);
	CU_ASSERT(odp_event_type(ev) == ODP_EVENT_PACKET);

	tmp_pkt = odp_packet_from_event(ev);
	CU_ASSERT_FATAL(tmp_pkt != ODP_PACKET_INVALID);
	/** @todo: Need an API to compare packets */
}

void packet_test_basic_metadata(void)
{
	odp_packet_t pkt = test_packet;

	CU_ASSERT_PTR_NOT_NULL(odp_packet_head(pkt));
	CU_ASSERT_PTR_NOT_NULL(odp_packet_data(pkt));

	CU_ASSERT(odp_packet_pool(pkt) != ODP_POOL_INVALID);
	/* Packet was allocated by application so shouldn't have valid pktio. */
	CU_ASSERT(odp_packet_input(pkt) == ODP_PKTIO_INVALID);

	odp_packet_flow_hash_set(pkt, UINT32_MAX);
	CU_ASSERT(odp_packet_has_flow_hash(pkt));
	CU_ASSERT(odp_packet_flow_hash(pkt) == UINT32_MAX);
	odp_packet_has_flow_hash_clr(pkt);
	CU_ASSERT(!odp_packet_has_flow_hash(pkt));
}

void packet_test_length(void)
{
	odp_packet_t pkt = test_packet;
	uint32_t buf_len, headroom, tailroom;

	buf_len = odp_packet_buf_len(pkt);
	headroom = odp_packet_headroom(pkt);
	tailroom = odp_packet_tailroom(pkt);

	CU_ASSERT(odp_packet_len(pkt) == packet_len);
#if ODP_CONFIG_PACKET_HEADROOM != 0 /* Avoid 'always true' warning */
	CU_ASSERT(headroom >= ODP_CONFIG_PACKET_HEADROOM);
#endif
#if ODP_CONFIG_PACKET_TAILROOM != 0 /* Avoid 'always true' warning */
	CU_ASSERT(tailroom >= ODP_CONFIG_PACKET_TAILROOM);
#endif
	CU_ASSERT(buf_len >= packet_len + headroom + tailroom);
}

void packet_test_debug(void)
{
	CU_ASSERT(odp_packet_is_valid(test_packet) == 1);
	odp_packet_print(test_packet);
}

void packet_test_context(void)
{
	odp_packet_t pkt = test_packet;
	char ptr_test_value = 2;
	void *prev_ptr;
	struct udata_struct *udat;

	prev_ptr = odp_packet_user_ptr(pkt);
	odp_packet_user_ptr_set(pkt, &ptr_test_value);
	CU_ASSERT(odp_packet_user_ptr(pkt) == &ptr_test_value);
	odp_packet_user_ptr_set(pkt, prev_ptr);

	udat = odp_packet_user_area(pkt);
	CU_ASSERT_PTR_NOT_NULL(udat);
	CU_ASSERT(odp_packet_user_area_size(pkt) ==
		  sizeof(struct udata_struct));
	CU_ASSERT(memcmp(udat, &test_packet_udata, sizeof(struct udata_struct))
		  == 0);

	odp_packet_reset(pkt, packet_len);
}

void packet_test_layer_offsets(void)
{
	odp_packet_t pkt = test_packet;
	uint8_t *l2_addr, *l3_addr, *l4_addr;
	uint32_t seg_len;
	const uint32_t l2_off = 2;
	const uint32_t l3_off = l2_off + 14;
	const uint32_t l4_off = l3_off + 14;
	int ret;

	/* Set offsets to the same value */
	ret = odp_packet_l2_offset_set(pkt, l2_off);
	CU_ASSERT(ret == 0);
	ret = odp_packet_l3_offset_set(pkt, l2_off);
	CU_ASSERT(ret == 0);
	ret = odp_packet_l4_offset_set(pkt, l2_off);
	CU_ASSERT(ret == 0);

	/* Addresses should be the same */
	l2_addr = odp_packet_l2_ptr(pkt, &seg_len);
	CU_ASSERT(seg_len != 0);
	l3_addr = odp_packet_l3_ptr(pkt, &seg_len);
	CU_ASSERT(seg_len != 0);
	l4_addr = odp_packet_l4_ptr(pkt, &seg_len);
	CU_ASSERT(seg_len != 0);
	CU_ASSERT_PTR_NOT_NULL(l2_addr);
	CU_ASSERT(l2_addr == l3_addr);
	CU_ASSERT(l2_addr == l4_addr);

	/* Set offsets to the different values */
	odp_packet_l2_offset_set(pkt, l2_off);
	CU_ASSERT(odp_packet_l2_offset(pkt) == l2_off);
	odp_packet_l3_offset_set(pkt, l3_off);
	CU_ASSERT(odp_packet_l3_offset(pkt) == l3_off);
	odp_packet_l4_offset_set(pkt, l4_off);
	CU_ASSERT(odp_packet_l4_offset(pkt) == l4_off);

	/* Addresses should not be the same */
	l2_addr = odp_packet_l2_ptr(pkt, NULL);
	CU_ASSERT_PTR_NOT_NULL(l2_addr);
	l3_addr = odp_packet_l3_ptr(pkt, NULL);
	CU_ASSERT_PTR_NOT_NULL(l3_addr);
	l4_addr = odp_packet_l4_ptr(pkt, NULL);
	CU_ASSERT_PTR_NOT_NULL(l4_addr);

	CU_ASSERT(l2_addr != l3_addr);
	CU_ASSERT(l2_addr != l4_addr);
	CU_ASSERT(l3_addr != l4_addr);
}

static void _verify_headroom_shift(odp_packet_t packet,
				   int shift)
{
	uint32_t room = odp_packet_headroom(packet);
	uint32_t seg_data_len = odp_packet_seg_len(packet);
	uint32_t pkt_data_len = odp_packet_len(packet);
	void *data;
	char *data_orig = odp_packet_data(packet);
	char *head_orig = odp_packet_head(packet);

	if (shift >= 0)
		data = odp_packet_push_head(packet, shift);
	else
		data = odp_packet_pull_head(packet, -shift);

	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT(odp_packet_headroom(packet) == room - shift);
	CU_ASSERT(odp_packet_seg_len(packet) == seg_data_len + shift);
	CU_ASSERT(odp_packet_len(packet) == pkt_data_len + shift);
	CU_ASSERT(odp_packet_data(packet) == data);
	CU_ASSERT(odp_packet_head(packet) == head_orig);
	CU_ASSERT(data == data_orig - shift);
}

void packet_test_headroom(void)
{
	odp_packet_t pkt = test_packet;
	uint32_t room;
	uint32_t seg_data_len;
	uint32_t push_val, pull_val;

	room = odp_packet_headroom(pkt);

#if ODP_CONFIG_PACKET_HEADROOM != 0 /* Avoid 'always true' warning */
	CU_ASSERT(room >= ODP_CONFIG_PACKET_HEADROOM);
#endif
	seg_data_len = odp_packet_seg_len(pkt);
	CU_ASSERT(seg_data_len >= 1);
	/** @todo: should be len - 1 */
	pull_val = seg_data_len / 2;
	push_val = room;

	_verify_headroom_shift(pkt, -pull_val);
	_verify_headroom_shift(pkt, push_val + pull_val);
	_verify_headroom_shift(pkt, -push_val);
	_verify_headroom_shift(pkt, 0);
}

static void _verify_tailroom_shift(odp_packet_t pkt,
				   int shift)
{
	odp_packet_seg_t seg;
	uint32_t room;
	uint32_t seg_data_len, pkt_data_len;
	void *tail;
	char *tail_orig;

	room = odp_packet_tailroom(pkt);
	pkt_data_len = odp_packet_len(pkt);
	tail_orig = odp_packet_tail(pkt);

	seg = odp_packet_last_seg(pkt);
	CU_ASSERT(seg != ODP_PACKET_SEG_INVALID);
	seg_data_len = odp_packet_seg_data_len(pkt, seg);

	if (shift >= 0) {
		uint32_t l2_off, l3_off, l4_off;

		l2_off = odp_packet_l2_offset(pkt);
		l3_off = odp_packet_l3_offset(pkt);
		l4_off = odp_packet_l4_offset(pkt);

		tail = odp_packet_push_tail(pkt, shift);

		CU_ASSERT(l2_off == odp_packet_l2_offset(pkt));
		CU_ASSERT(l3_off == odp_packet_l3_offset(pkt));
		CU_ASSERT(l4_off == odp_packet_l4_offset(pkt));
	} else {
		tail = odp_packet_pull_tail(pkt, -shift);
	}

	CU_ASSERT_PTR_NOT_NULL(tail);
	CU_ASSERT(odp_packet_seg_data_len(pkt, seg) == seg_data_len + shift);
	CU_ASSERT(odp_packet_len(pkt) == pkt_data_len + shift);
	CU_ASSERT(odp_packet_tailroom(pkt) == room - shift);
	if (room == 0 || (room - shift) == 0)
		return;
	if (shift >= 0) {
		CU_ASSERT(odp_packet_tail(pkt) == tail_orig + shift);
		CU_ASSERT(tail == tail_orig);
	} else {
		CU_ASSERT(odp_packet_tail(pkt) == tail);
		CU_ASSERT(tail == tail_orig + shift);
	}
}

void packet_test_tailroom(void)
{
	odp_packet_t pkt = test_packet;
	odp_packet_seg_t segment;
	uint32_t room;
	uint32_t seg_data_len;
	uint32_t push_val, pull_val;

	segment = odp_packet_last_seg(pkt);
	CU_ASSERT(segment != ODP_PACKET_SEG_INVALID);
	room = odp_packet_tailroom(pkt);
#if ODP_CONFIG_PACKET_TAILROOM != 0 /* Avoid 'always true' warning */
	CU_ASSERT(room >= ODP_CONFIG_PACKET_TAILROOM);
#endif
	seg_data_len = odp_packet_seg_data_len(pkt, segment);
	CU_ASSERT(seg_data_len >= 1);
	/** @todo: should be len - 1 */
	pull_val = seg_data_len / 2;
	/* Leave one byte in a tailroom for odp_packet_tail() to succeed */
	push_val = (room > 0) ? room - 1 : room;

	_verify_tailroom_shift(pkt, -pull_val);
	_verify_tailroom_shift(pkt, push_val + pull_val);
	_verify_tailroom_shift(pkt, -push_val);
	_verify_tailroom_shift(pkt, 0);
}

void packet_test_segments(void)
{
	int num_segs, seg_index;
	uint32_t data_len, buf_len;
	odp_packet_seg_t seg;
	odp_packet_t pkt = test_packet;
	odp_packet_t seg_pkt = segmented_test_packet;

	CU_ASSERT(odp_packet_is_valid(pkt) == 1);

	num_segs = odp_packet_num_segs(pkt);
	CU_ASSERT(num_segs != 0);

	if (odp_packet_is_segmented(pkt)) {
		CU_ASSERT(num_segs > 1);
	} else {
		CU_ASSERT(num_segs == 1);
	}

	CU_ASSERT(odp_packet_is_segmented(pkt) == 0);
	if (segmentation_supported)
		CU_ASSERT(odp_packet_is_segmented(seg_pkt) == 1);

	seg = odp_packet_first_seg(pkt);
	buf_len = 0;
	data_len = 0;
	seg_index = 0;
	while (seg_index < num_segs && seg != ODP_PACKET_SEG_INVALID) {
		uint32_t seg_data_len, seg_buf_len;
		void *seg_buf_addr, *seg_data;

		seg_buf_addr = odp_packet_seg_buf_addr(pkt, seg);
		seg_buf_len  = odp_packet_seg_buf_len(pkt, seg);
		seg_data_len = odp_packet_seg_data_len(pkt, seg);
		seg_data     = odp_packet_seg_data(pkt, seg);

		CU_ASSERT(seg_buf_len > 0);
		CU_ASSERT(seg_data_len > 0);
		CU_ASSERT(seg_buf_len >= seg_data_len);
		CU_ASSERT_PTR_NOT_NULL(seg_data);
		CU_ASSERT_PTR_NOT_NULL(seg_buf_addr);
		CU_ASSERT(seg_data >= seg_buf_addr);
		CU_ASSERT(odp_packet_seg_to_u64(seg) !=
			  odp_packet_seg_to_u64(ODP_PACKET_SEG_INVALID));

		buf_len += seg_buf_len;
		data_len += seg_data_len;

		/** @todo: touch memory in a segment */
		seg_index++;
		seg = odp_packet_next_seg(pkt, seg);
	}

	CU_ASSERT(seg_index == num_segs);
	CU_ASSERT(buf_len == odp_packet_buf_len(pkt));
	CU_ASSERT(data_len == odp_packet_len(pkt));

	if (seg_index == num_segs)
		CU_ASSERT(seg == ODP_PACKET_SEG_INVALID);

	seg = odp_packet_first_seg(seg_pkt);
	num_segs = odp_packet_num_segs(seg_pkt);

	buf_len = 0;
	data_len = 0;
	seg_index = 0;
	while (seg_index < num_segs && seg != ODP_PACKET_SEG_INVALID) {
		uint32_t seg_data_len, seg_buf_len;
		void *seg_buf_addr, *seg_data;

		seg_buf_addr = odp_packet_seg_buf_addr(seg_pkt, seg);
		seg_buf_len  = odp_packet_seg_buf_len(seg_pkt, seg);
		seg_data_len = odp_packet_seg_data_len(seg_pkt, seg);
		seg_data     = odp_packet_seg_data(seg_pkt, seg);

		CU_ASSERT(seg_buf_len > 0);
		CU_ASSERT(seg_data_len > 0);
		CU_ASSERT(seg_buf_len >= seg_data_len);
		CU_ASSERT(seg_data != NULL);
		CU_ASSERT(seg_buf_addr != NULL);
		CU_ASSERT(seg_data >= seg_buf_addr);
		CU_ASSERT(odp_packet_seg_to_u64(seg) !=
			  odp_packet_seg_to_u64(ODP_PACKET_SEG_INVALID));

		buf_len += seg_buf_len;
		data_len += seg_data_len;

		/** @todo: touch memory in a segment */
		seg_index++;
		seg = odp_packet_next_seg(seg_pkt, seg);
	}

	CU_ASSERT(seg_index == num_segs);
	CU_ASSERT(buf_len == odp_packet_buf_len(seg_pkt));
	CU_ASSERT(data_len == odp_packet_len(seg_pkt));

	if (seg_index == num_segs)
		CU_ASSERT(seg == ODP_PACKET_SEG_INVALID);
}

void packet_test_segment_last(void)
{
	odp_packet_t pkt = test_packet;
	odp_packet_seg_t seg;

	seg = odp_packet_last_seg(pkt);
	CU_ASSERT_FATAL(seg != ODP_PACKET_SEG_INVALID);

	seg = odp_packet_next_seg(pkt, seg);
	CU_ASSERT(seg == ODP_PACKET_SEG_INVALID);
}

#define TEST_INFLAG(packet, flag) \
do { \
	odp_packet_has_##flag##_set(packet, 0);           \
	CU_ASSERT(odp_packet_has_##flag(packet) == 0);    \
	odp_packet_has_##flag##_set(packet, 1);           \
	CU_ASSERT(odp_packet_has_##flag(packet) != 0);    \
} while (0)

void packet_test_in_flags(void)
{
	odp_packet_t pkt = test_packet;

	TEST_INFLAG(pkt, l2);
	TEST_INFLAG(pkt, l3);
	TEST_INFLAG(pkt, l4);
	TEST_INFLAG(pkt, eth);
	TEST_INFLAG(pkt, jumbo);
	TEST_INFLAG(pkt, vlan);
	TEST_INFLAG(pkt, vlan_qinq);
	TEST_INFLAG(pkt, arp);
	TEST_INFLAG(pkt, ipv4);
	TEST_INFLAG(pkt, ipv6);
	TEST_INFLAG(pkt, ipfrag);
	TEST_INFLAG(pkt, ipopt);
	TEST_INFLAG(pkt, ipsec);
	TEST_INFLAG(pkt, udp);
	TEST_INFLAG(pkt, tcp);
	TEST_INFLAG(pkt, sctp);
	TEST_INFLAG(pkt, icmp);
}

void packet_test_error_flags(void)
{
	odp_packet_t pkt = test_packet;
	int err;

	/**
	 * The packet have not been classified so it doesn't have error flag
	 * properly set. Just check that function return one of allowed values.
	 * @todo: check classified packet when classifier is added in place.
	 */
	err = odp_packet_has_error(pkt);
	CU_ASSERT(err == 0 || err == 1);
}

struct packet_metadata {
	uint32_t l2_off;
	uint32_t l3_off;
	uint32_t l4_off;
	void *usr_ptr;
	uint64_t usr_u64;
};

void packet_test_add_rem_data(void)
{
	odp_packet_t pkt, new_pkt;
	uint32_t pkt_len, offset, add_len;
	void *usr_ptr;
	struct udata_struct *udat, *new_udat;

	pkt = odp_packet_alloc(packet_pool, packet_len);
	CU_ASSERT_FATAL(pkt != ODP_PACKET_INVALID);

	pkt_len = odp_packet_len(pkt);
	usr_ptr = odp_packet_user_ptr(pkt);
	udat    = odp_packet_user_area(pkt);
	CU_ASSERT(odp_packet_user_area_size(pkt) ==
		  sizeof(struct udata_struct));
	memcpy(udat, &test_packet_udata, sizeof(struct udata_struct));

	offset = pkt_len / 2;

	if (segmentation_supported) {
		/* Insert one more packet length in the middle of a packet */
		add_len = PACKET_BUF_LEN;
	} else {
		/* Add diff between largest and smaller packets
		 * which is at least tailroom */
		add_len = segmented_packet_len - packet_len;
	}

	new_pkt = odp_packet_add_data(pkt, offset, add_len);
	CU_ASSERT(new_pkt != ODP_PACKET_INVALID);
	if (new_pkt == ODP_PACKET_INVALID)
		goto free_packet;
	CU_ASSERT(odp_packet_len(new_pkt) == pkt_len + add_len);
	/* Verify that user metadata is preserved */
	CU_ASSERT(odp_packet_user_ptr(new_pkt) == usr_ptr);

	/* Verify that user metadata has been preserved */
	new_udat = odp_packet_user_area(new_pkt);
	CU_ASSERT_PTR_NOT_NULL(new_udat);
	CU_ASSERT(odp_packet_user_area_size(new_pkt) ==
		  sizeof(struct udata_struct));
	CU_ASSERT(memcmp(new_udat, &test_packet_udata,
			 sizeof(struct udata_struct)) == 0);

	pkt = new_pkt;

	pkt_len = odp_packet_len(pkt);
	usr_ptr = odp_packet_user_ptr(pkt);
	new_pkt = odp_packet_rem_data(pkt, offset, add_len);
	CU_ASSERT(new_pkt != ODP_PACKET_INVALID);
	if (new_pkt == ODP_PACKET_INVALID)
		goto free_packet;
	CU_ASSERT(odp_packet_len(new_pkt) == pkt_len - add_len);
	CU_ASSERT(odp_packet_user_ptr(new_pkt) == usr_ptr);

	/* Verify that user metadata has been preserved */
	new_udat = odp_packet_user_area(new_pkt);
	CU_ASSERT_PTR_NOT_NULL(new_udat);
	CU_ASSERT(odp_packet_user_area_size(new_pkt) ==
		  sizeof(struct udata_struct));
	CU_ASSERT(memcmp(new_udat, &test_packet_udata,
			 sizeof(struct udata_struct)) == 0);

	pkt = new_pkt;

free_packet:
	odp_packet_free(pkt);
}

#define COMPARE_INFLAG(p1, p2, flag) \
	CU_ASSERT(odp_packet_has_##flag(p1) == odp_packet_has_##flag(p2))

static void _packet_compare_inflags(odp_packet_t pkt1, odp_packet_t pkt2)
{
	COMPARE_INFLAG(pkt1, pkt2, l2);
	COMPARE_INFLAG(pkt1, pkt2, l3);
	COMPARE_INFLAG(pkt1, pkt2, l4);
	COMPARE_INFLAG(pkt1, pkt2, eth);
	COMPARE_INFLAG(pkt1, pkt2, jumbo);
	COMPARE_INFLAG(pkt1, pkt2, eth);
	COMPARE_INFLAG(pkt1, pkt2, vlan);
	COMPARE_INFLAG(pkt1, pkt2, vlan_qinq);
	COMPARE_INFLAG(pkt1, pkt2, arp);
	COMPARE_INFLAG(pkt1, pkt2, ipv4);
	COMPARE_INFLAG(pkt1, pkt2, ipv6);
	COMPARE_INFLAG(pkt1, pkt2, ipfrag);
	COMPARE_INFLAG(pkt1, pkt2, ipopt);
	COMPARE_INFLAG(pkt1, pkt2, ipsec);
	COMPARE_INFLAG(pkt1, pkt2, udp);
	COMPARE_INFLAG(pkt1, pkt2, tcp);
	COMPARE_INFLAG(pkt1, pkt2, sctp);
	COMPARE_INFLAG(pkt1, pkt2, icmp);
}

static void _packet_compare_data(odp_packet_t pkt1, odp_packet_t pkt2)
{
	uint32_t len = odp_packet_len(pkt1);
	uint32_t offset = 0;
	uint32_t seglen1, seglen2, cmplen;

	CU_ASSERT_FATAL(len == odp_packet_len(pkt2));

	while (len > 0) {
		void *pkt1map = odp_packet_offset(pkt1, offset, &seglen1, NULL);
		void *pkt2map = odp_packet_offset(pkt2, offset, &seglen2, NULL);

		CU_ASSERT_PTR_NOT_NULL_FATAL(pkt1map);
		CU_ASSERT_PTR_NOT_NULL_FATAL(pkt2map);
		cmplen = seglen1 < seglen2 ? seglen1 : seglen2;
		CU_ASSERT(!memcmp(pkt1map, pkt2map, cmplen));

		offset += cmplen;
		len    -= cmplen;
	}
}

void packet_test_copy(void)
{
	odp_packet_t pkt = test_packet;
	odp_packet_t pkt_copy;
	odp_pool_t pool;

	/** @todo: fill original packet with some data */
	pool = odp_packet_pool(pkt);
	CU_ASSERT_FATAL(pool != ODP_POOL_INVALID);
	pkt_copy = odp_packet_copy(pkt, odp_packet_pool(pkt));
	CU_ASSERT_FATAL(pkt_copy != ODP_PACKET_INVALID);

	CU_ASSERT(odp_packet_len(pkt) == odp_packet_len(pkt_copy));

	_packet_compare_inflags(pkt, pkt_copy);
	_packet_compare_data(pkt, pkt_copy);
	odp_packet_free(pkt_copy);
}

void packet_test_copydata(void)
{
	odp_packet_t pkt = test_packet;
	uint32_t pkt_len = odp_packet_len(pkt);
	uint8_t *data_buf;
	uint32_t i;
	int correct_memory;

	CU_ASSERT_FATAL(pkt_len > 0);

	data_buf = malloc(pkt_len);
	CU_ASSERT_PTR_NOT_NULL_FATAL(data_buf);

	for (i = 0; i < pkt_len; i++)
		data_buf[i] = (uint8_t)i;

	CU_ASSERT(!odp_packet_copydata_in(pkt, 0, pkt_len, data_buf));
	memset(data_buf, 0, pkt_len);
	CU_ASSERT(!odp_packet_copydata_out(pkt, 0, pkt_len, data_buf));

	correct_memory = 1;
	for (i = 0; i < pkt_len; i++)
		if (data_buf[i] != (uint8_t)i) {
			correct_memory = 0;
			break;
		}
	CU_ASSERT(correct_memory);

	free(data_buf);
}

void packet_test_offset(void)
{
	odp_packet_t pkt = test_packet;
	uint32_t seg_len, full_seg_len;
	odp_packet_seg_t seg;
	uint8_t *ptr, *start_ptr;
	uint32_t offset;

	ptr = odp_packet_offset(pkt, 0, &seg_len, &seg);
	CU_ASSERT(seg != ODP_PACKET_SEG_INVALID);
	CU_ASSERT(seg_len > 1);
	CU_ASSERT(seg_len == odp_packet_seg_len(pkt));
	CU_ASSERT(seg_len == odp_packet_seg_data_len(pkt, seg));
	CU_ASSERT_PTR_NOT_NULL(ptr);
	CU_ASSERT(ptr == odp_packet_data(pkt));
	CU_ASSERT(ptr == odp_packet_seg_data(pkt, seg));

	/* Query a second byte */
	start_ptr = ptr;
	full_seg_len = seg_len;
	offset = 1;

	ptr = odp_packet_offset(pkt, offset, &seg_len, NULL);
	CU_ASSERT_PTR_NOT_NULL(ptr);
	CU_ASSERT(ptr == start_ptr + offset);
	CU_ASSERT(seg_len == full_seg_len - offset);

	/* Query the last byte in a segment */
	offset = full_seg_len - 1;

	ptr = odp_packet_offset(pkt, offset, &seg_len, NULL);
	CU_ASSERT_PTR_NOT_NULL(ptr);
	CU_ASSERT(ptr == start_ptr + offset);
	CU_ASSERT(seg_len == full_seg_len - offset);

	/* Query the last byte in a packet */
	offset = odp_packet_len(pkt) - 1;
	ptr = odp_packet_offset(pkt, offset, &seg_len, NULL);
	CU_ASSERT_PTR_NOT_NULL(ptr);
	CU_ASSERT(seg_len == 1);

	/* Pass NULL to [out] arguments */
	ptr = odp_packet_offset(pkt, 0, NULL, NULL);
	CU_ASSERT_PTR_NOT_NULL(ptr);
}

odp_testinfo_t packet_suite[] = {
	ODP_TEST_INFO(packet_test_alloc_free),
	ODP_TEST_INFO(packet_test_alloc_free_multi),
	ODP_TEST_INFO(packet_test_alloc_segmented),
	ODP_TEST_INFO(packet_test_basic_metadata),
	ODP_TEST_INFO(packet_test_debug),
	ODP_TEST_INFO(packet_test_length),
	ODP_TEST_INFO(packet_test_headroom),
	ODP_TEST_INFO(packet_test_tailroom),
	ODP_TEST_INFO(packet_test_context),
	ODP_TEST_INFO(packet_test_event_conversion),
	ODP_TEST_INFO(packet_test_layer_offsets),
	ODP_TEST_INFO(packet_test_segments),
	ODP_TEST_INFO(packet_test_segment_last),
	ODP_TEST_INFO(packet_test_in_flags),
	ODP_TEST_INFO(packet_test_error_flags),
	ODP_TEST_INFO(packet_test_add_rem_data),
	ODP_TEST_INFO(packet_test_copy),
	ODP_TEST_INFO(packet_test_copydata),
	ODP_TEST_INFO(packet_test_offset),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t packet_suites[] = {
	{ .pName = "packet tests",
			.pTests = packet_suite,
			.pInitFunc = packet_suite_init,
			.pCleanupFunc = packet_suite_term,
	},
	ODP_SUITE_INFO_NULL,
};

int packet_main(void)
{
	int ret = odp_cunit_register(packet_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
