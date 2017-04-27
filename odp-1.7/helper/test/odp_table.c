/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:BSD-3-Clause
 */

#include <test_debug.h>
#include <../odph_hashtable.h>
#include <../odph_lineartable.h>
#include <odp.h>

/**
 * Address Resolution Protocol (ARP)
 * Description: Once a route has been identified for an IP packet (so the
 * output interface and the IP address of the next hop station are known),
 * the MAC address of the next hop station is needed in order to send this
 * packet onto the next leg of the journey towards its destination
 * (as identified by its destination IP address). The MAC address of the next
 * hop station becomes the destination MAC address of the outgoing
 * Ethernet frame.
 * Hash table name: ARP table
 * Number of keys: Thousands
 * Key format: The pair of (Output interface, Next Hop IP address),
 *        which is typically 5 bytes for IPv4 and 17 bytes for IPv6.
 * value (data): MAC address of the next hop station (6 bytes).
 */

int main(int argc TEST_UNUSED, char *argv[] TEST_UNUSED)
{
	int ret = 0;
	odph_table_t table;
	odph_table_t tmp_tbl;
	struct odp_table_ops *test_ops;
	char tmp[32];
	char ip_addr1[] = "12345678";
	char ip_addr2[] = "11223344";
	char ip_addr3[] = "55667788";
	char mac_addr1[] = "0A1122334401";
	char mac_addr2[] = "0A1122334402";
	char mac_addr3[] = "0B4433221101";
	char mac_addr4[] = "0B4433221102";

	ret = odp_init_global(NULL, NULL);
	if (ret != 0) {
		LOG_ERR("odp_shm_init_global fail\n");
		exit(EXIT_FAILURE);
	}
	ret = odp_init_local(ODP_THREAD_WORKER);
	if (ret != 0) {
		LOG_ERR("odp_shm_init_local fail\n");
		exit(EXIT_FAILURE);
	}

	printf("test hash table:\n");
	test_ops = &odph_hash_table_ops;

	table = test_ops->f_create("test", 2, 4, 16);
	if (table == NULL) {
		printf("table create fail\n");
		return -1;
	}
	ret += test_ops->f_put(table, &ip_addr1, mac_addr1);

	ret += test_ops->f_put(table, &ip_addr2, mac_addr2);

	ret += test_ops->f_put(table, &ip_addr3, mac_addr3);

	if (ret != 0) {
		printf("put value fail\n");
		return -1;
	}

	ret = test_ops->f_get(table, &ip_addr1, &tmp, 32);
	if (ret != 0) {
		printf("get value fail\n");
		return -1;
	}
	printf("\t1  get '123' tmp = %s,\n", tmp);

	ret = test_ops->f_put(table, &ip_addr1, mac_addr4);
	if (ret != 0) {
		printf("repeat put value fail\n");
		return -1;
	}

	ret = test_ops->f_get(table, &ip_addr1, &tmp, 32);
	if (ret != 0 || strcmp(tmp, mac_addr4) != 0) {
		printf("get value fail\n");
		return -1;
	}

	printf("\t2  repeat get '123' value = %s\n", tmp);

	ret = test_ops->f_remove(table, &ip_addr1);
	if (ret != 0) {
		printf("remove value fail\n");
		return -1;
	}
	ret = test_ops->f_get(table, &ip_addr1, tmp, 32);
	if (ret == 0) {
		printf("remove value fail actually\n");
		return -1;
	}
	printf("\t3  remove success!\n");

	tmp_tbl = test_ops->f_lookup("test");
	if (tmp_tbl != table) {
		printf("lookup table fail!!!\n");
		return -1;
	}
	printf("\t4  lookup table success!\n");

	ret = test_ops->f_des(table);
	if (ret != 0) {
		printf("destroy table fail!!!\n");
		exit(EXIT_FAILURE);
	}
	printf("\t5  destroy table success!\n");

	printf("all test finished success!!\n");

	if (odp_term_local()) {
		LOG_ERR("Error: ODP local term failed.\n");
		exit(EXIT_FAILURE);
	}

	if (odp_term_global()) {
		LOG_ERR("Error: ODP global term failed.\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

