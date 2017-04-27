/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <test_debug.h>
#include <odp.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/udp.h>

#define PACKET_BUF_LEN ODP_CONFIG_PACKET_SEG_LEN_MIN
/* Reserve some tailroom for tests */
#define PACKET_TAILROOM_RESERVE 4

struct udata_struct {
	uint64_t u64;
	uint32_t u32;
	char str[10];
} test_packet_udata = {
	123456,
	789912,
	"abcdefg",
};

static	const uint32_t packet_len =	PACKET_BUF_LEN -
					ODP_CONFIG_PACKET_HEADROOM -
					ODP_CONFIG_PACKET_TAILROOM -
					PACKET_TAILROOM_RESERVE;

/**
 * Scan ip
 * Parse ip address.
 *
 * @param buf ip address string xxx.xxx.xxx.xx
 * @param paddr ip address for odp_packet
 * @return 1 success, 0 failed
*/
static int scan_ip(const char *buf, unsigned int *paddr)
{
	int part1, part2, part3, part4;
	char tail = 0;
	int field;

	if (!buf)
		return 0;

	field = sscanf(buf, "%d . %d . %d . %d %c",
		       &part1, &part2, &part3, &part4, &tail);

	if (field < 4 || field > 5) {
		printf("expect 4 field,get %d/n", field);
		return 0;
	}

	if (tail != 0) {
		printf("ip address mixed with non number/n");
		return 0;
	}

	if ((part1 >= 0 && part1 <= 255) && (part2 >= 0 && part2 <= 255) &&
	    (part3 >= 0 && part3 <= 255) && (part4 >= 0 && part4 <= 255)) {
		if (paddr)
			*paddr = part1 << 24 | part2 << 16 | part3 << 8 | part4;
		return 1;
	}

	printf("not good ip %d:%d:%d:%d/n", part1, part2, part3, part4);

	return 0;
}

/**
 * Scan mac addr form string
 *
 * @param  in mac string
 * @param  des mac for odp_packet
 * @return 1 success, 0 failed
 */
static int scan_mac(const char *in, odph_ethaddr_t *des)
{
	int field;
	int i;
	unsigned int mac[7];

	field = sscanf(in, "%2x:%2x:%2x:%2x:%2x:%2x",
		       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	for (i = 0; i < 6; i++)
		des->addr[i] = mac[i];

	if (field != 6)
		return 0;
	return 1;
}

/* Create additional dataplane threads */
int main(int argc TEST_UNUSED, char *argv[] TEST_UNUSED)
{
	int status = 0;
	odp_pool_t packet_pool;
	odp_packet_t test_packet;
	struct udata_struct *udat;
	uint32_t udat_size;
	char *buf;
	odph_ethhdr_t *eth;
	odph_ipv4hdr_t *ip;
	odph_udphdr_t *udp;
	odph_ethaddr_t des;
	odph_ethaddr_t src;
	unsigned int srcip;
	unsigned int dstip;
	odp_pool_param_t params = {
		.pkt = {
				.seg_len = PACKET_BUF_LEN,
				.len     = PACKET_BUF_LEN,
				.num     = 100,
				.uarea_size = sizeof(struct udata_struct),
			},
			.type  = ODP_POOL_PACKET,
	};

	if (odp_init_global(NULL, NULL)) {
		LOG_ERR("Error: ODP global init failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_init_local(ODP_THREAD_WORKER)) {
		LOG_ERR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	packet_pool = odp_pool_create("packet_pool", &params);
	if (packet_pool == ODP_POOL_INVALID)
		return -1;

	test_packet = odp_packet_alloc(packet_pool, packet_len);
	if (odp_packet_is_valid(test_packet) == 0)
		return -1;

	udat = odp_packet_user_area(test_packet);
	udat_size = odp_packet_user_area_size(test_packet);
	if (!udat || udat_size != sizeof(struct udata_struct))
		return -1;

	memcpy(udat, &test_packet_udata, sizeof(struct udata_struct));

	buf = odp_packet_data(test_packet);
	scan_mac("fe:0f:97:c9:e0:44", &des);
	scan_mac("fe:0f:97:c9:e0:44", &src);

	/* ether */
	odp_packet_l2_offset_set(test_packet, 0);
	eth = (odph_ethhdr_t *)buf;
	memcpy((char *)eth->src.addr, &src, ODPH_ETHADDR_LEN);
	memcpy((char *)eth->dst.addr, &des, ODPH_ETHADDR_LEN);
	eth->type = odp_cpu_to_be_16(ODPH_ETHTYPE_IPV4);

	if (!scan_ip("192.168.0.1", &dstip)) {
		LOG_ERR("Error: scan_ip\n");
		return -1;
	}

	if (!scan_ip("192.168.0.2", &srcip)) {
		LOG_ERR("Error: scan_ip\n");
		return -1;
	}

	/* ip */
	odp_packet_l3_offset_set(test_packet, ODPH_ETHHDR_LEN);
	ip = (odph_ipv4hdr_t *)(buf + ODPH_ETHHDR_LEN);
	ip->dst_addr = odp_cpu_to_be_32(srcip);
	ip->src_addr = odp_cpu_to_be_32(dstip);
	ip->ver_ihl = ODPH_IPV4 << 4 | ODPH_IPV4HDR_IHL_MIN;
	ip->tot_len = odp_cpu_to_be_16(udat_size + ODPH_UDPHDR_LEN +
				       ODPH_IPV4HDR_LEN);
	ip->proto = ODPH_IPPROTO_UDP;
	ip->id = odp_cpu_to_be_16(1);
	ip->chksum = 0;
	odph_ipv4_csum_update(test_packet);

	/* udp */
	odp_packet_l4_offset_set(test_packet, ODPH_ETHHDR_LEN
				 + ODPH_IPV4HDR_LEN);
	udp = (odph_udphdr_t *)(buf + ODPH_ETHHDR_LEN
				+ ODPH_IPV4HDR_LEN);
	udp->src_port = 0;
	udp->dst_port = 0;
	udp->length = odp_cpu_to_be_16(udat_size + ODPH_UDPHDR_LEN);
	udp->chksum = 0;
	udp->chksum = odph_ipv4_udp_chksum(test_packet);

	if (udp->chksum == 0)
		return -1;

	printf("chksum = 0x%x\n", odp_be_to_cpu_16(udp->chksum));

	if (odp_be_to_cpu_16(udp->chksum) != 0x7e5a)
		status = -1;

	odp_packet_free(test_packet);
	if (odp_pool_destroy(packet_pool) != 0)
		return -1;

	if (odp_term_local()) {
		LOG_ERR("Error: ODP local term failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_global()) {
		LOG_ERR("Error: ODP global term failed.\n");
		exit(EXIT_FAILURE);
	}

	return status;
}
