#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#include "rss.h"

/*-------------------------------------------------------------*/ 
static void 
BuildKeyCache(uint32_t *cache, int cache_len)
{
#define NBBY 8 /* number of bits per byte */

	/* Keys for system testing */
	static const uint8_t key[] = {
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
	};

	uint32_t result = (((uint32_t)key[0]) << 24) | 
		(((uint32_t)key[1]) << 16) | 
		(((uint32_t)key[2]) << 8)  | 
		((uint32_t)key[3]);

	uint32_t idx = 32;
	int i;

	for (i = 0; i < cache_len; i++, idx++) {
		uint8_t shift = (idx % NBBY);
		uint32_t bit;

		cache[i] = result;
		bit = ((key[idx/NBBY] << shift) & 0x80) ? 1 : 0;
		result = ((result << 1) | bit);
	}
}
/*-------------------------------------------------------------*/ 
static uint32_t 
GetRSSHash(in_addr_t sip, in_addr_t dip, in_port_t sp, in_port_t dp)
{
#define MSB32 0x80000000
#define MSB16 0x8000
#define KEY_CACHE_LEN 96

	uint32_t res = 0;
	int i;
	static int first = 1;
	static uint32_t key_cache[KEY_CACHE_LEN] = {0};
	
	if (first) {
		BuildKeyCache(key_cache, KEY_CACHE_LEN);
		first = 0;
	}

	for (i = 0; i < 32; i++) {
		if (sip & MSB32)
			res ^= key_cache[i];
		sip <<= 1;
	}
	for (i = 0; i < 32; i++) {
		if (dip & MSB32)
			res ^= key_cache[32+i];
		dip <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (sp & MSB16)
			res ^= key_cache[64+i];
		sp <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (dp & MSB16)
			res ^= key_cache[80+i];
		dp <<= 1;
	}
	return res;
}
/*-------------------------------------------------------------------*/ 
/* RSS redirection table is in the little endian byte order (intel)  */
/*                                                                   */
/* idx: 0 1 2 3 | 4 5 6 7 | 8 9 10 11 | 12 13 14 15 | 16 17 18 19 ...*/
/* val: 3 2 1 0 | 7 6 5 4 | 11 10 9 8 | 15 14 13 12 | 19 18 17 16 ...*/
/* qid = val % num_queues */
/*-------------------------------------------------------------------*/
/*
 * IXGBE (Intel X520 NIC) : (Rx queue #) = (7 LS bits of RSS hash) mod N
 * I40E (Intel XL710 NIC) : (Rx queue #) = (9 LS bits of RSS hash) mod N
 */
#define RSS_BIT_MASK_IXGBE		0x0000007F
#define RSS_BIT_MASK_I40E		0x000001FF

int
GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
	      in_port_t sp, in_port_t dp, int num_queues, uint8_t endian_check)
{
	uint32_t masked;

	if (endian_check) {
		/* i40e */
		static const uint32_t off[] = {3, 1, -1, -3};
		masked = GetRSSHash(sip, dip, sp, dp) & RSS_BIT_MASK_I40E; 
		masked += off[masked & 0x3];
	} else {
		/* ixgbe or mlx* */
		masked = GetRSSHash(sip, dip, sp, dp) & RSS_BIT_MASK_IXGBE;
	}

	return (masked % num_queues);
}
/*-------------------------------------------------------------------*/ 
