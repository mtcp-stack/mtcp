/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Checksum
 */
#ifndef ODP_CHKSUM_H_
#define ODP_CHKSUM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>

/**
 * Checksum
 *
 * @param buffer calculate chksum for buffer
 * @param len    buffer length
 *
 * @return checksum value in host cpu order
 */
static inline odp_u16sum_t odp_chksum(void *buffer, int len)
{
	uint16_t *buf = buffer;
	uint32_t sum = 0;
	uint16_t result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;

	if (len == 1)
		sum += *(unsigned char *)buf;

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;

	return  (__odp_force odp_u16sum_t) result;
}

#ifdef __cplusplus
}
#endif

#endif
