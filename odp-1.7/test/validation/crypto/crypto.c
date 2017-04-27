/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "odp_crypto_test_inp.h"
#include "crypto.h"

#define SHM_PKT_POOL_SIZE	(512 * 2048 * 2)
#define SHM_PKT_POOL_BUF_SIZE	(1024 * 32)

#define SHM_COMPL_POOL_SIZE	(128 * 1024)
#define SHM_COMPL_POOL_BUF_SIZE	128

odp_suiteinfo_t crypto_suites[] = {
	{ODP_CRYPTO_SYNC_INP, crypto_suite_sync_init, NULL, crypto_suite},
	{ODP_CRYPTO_ASYNC_INP, crypto_suite_async_init, NULL, crypto_suite},
	ODP_SUITE_INFO_NULL,
};

int crypto_init(void)
{
	odp_pool_param_t params;
	odp_pool_t pool;
	odp_queue_t out_queue;

	if (0 != odp_init_global(NULL, NULL)) {
		fprintf(stderr, "error: odp_init_global() failed.\n");
		return -1;
	}
	if (0 != odp_init_local(ODP_THREAD_CONTROL)) {
		fprintf(stderr, "error: odp_init_local() failed.\n");
		return -1;
	}

	memset(&params, 0, sizeof(params));
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_SIZE / SHM_PKT_POOL_BUF_SIZE;
	params.type        = ODP_POOL_PACKET;

	if (SHM_PKT_POOL_BUF_SIZE > odp_config_packet_buf_len_max())
		params.pkt.len = odp_config_packet_buf_len_max();

	pool = odp_pool_create("packet_pool", &params);

	if (ODP_POOL_INVALID == pool) {
		fprintf(stderr, "Packet pool creation failed.\n");
		return -1;
	}
	out_queue = odp_queue_create("crypto-out", NULL);
	if (ODP_QUEUE_INVALID == out_queue) {
		fprintf(stderr, "Crypto outq creation failed.\n");
		return -1;
	}

	return 0;
}

int crypto_term(void)
{
	odp_pool_t pool;
	odp_queue_t out_queue;

	out_queue = odp_queue_lookup("crypto-out");
	if (ODP_QUEUE_INVALID != out_queue) {
		if (odp_queue_destroy(out_queue))
			fprintf(stderr, "Crypto outq destroy failed.\n");
	} else {
		fprintf(stderr, "Crypto outq not found.\n");
	}

	pool = odp_pool_lookup("packet_pool");
	if (ODP_POOL_INVALID != pool) {
		if (odp_pool_destroy(pool))
			fprintf(stderr, "Packet pool destroy failed.\n");
	} else {
		fprintf(stderr, "Packet pool not found.\n");
	}

	if (0 != odp_term_local()) {
		fprintf(stderr, "error: odp_term_local() failed.\n");
		return -1;
	}

	if (0 != odp_term_global()) {
		fprintf(stderr, "error: odp_term_global() failed.\n");
		return -1;
	}

	return 0;
}

int crypto_main(void)
{
	int ret;

	odp_cunit_register_global_init(crypto_init);
	odp_cunit_register_global_term(crypto_term);

	ret = odp_cunit_register(crypto_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
