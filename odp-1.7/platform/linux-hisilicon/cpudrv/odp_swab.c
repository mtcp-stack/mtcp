/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <inttypes.h>

uint16_t  __arch_swab16(uint16_t x)
{
	__asm__("rev16 %0, %1" : "=r" (x) : "r" (x));
	return x;
}

#if (!defined(__arm32__))
uint32_t __arch_swab32(uint32_t x)
{
	__asm__("rev32 %0, %1" : "=r" (x) : "r" (x));
	return x;
}
#endif
