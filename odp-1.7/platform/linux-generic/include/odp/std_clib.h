/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PLAT_STD_CLIB_H_
#define ODP_PLAT_STD_CLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/std_types.h>

/**
 * Copy bytes from one location to another. The locations must not overlap.
 *
 * @note This is implemented as a macro, so it's address should not be taken
 * and care is needed as parameter expressions may be evaluated multiple times.
 *
 * @param dst
 *   Pointer to the destination of the data.
 * @param src
 *   Pointer to the source data.
 * @param n
 *   Number of bytes to copy.
 * @return
 *   Pointer to the destination data.
 */
static inline void *
odp_memcpy_a64(void *dst,
const void *src, size_t n) __attribute__((always_inline));

static inline void *
odp_memset_a64(void *ptr, int value_t, size_t n) __attribute__((always_inline));

/**
 * Copy 16 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov16_a64(uint8_t *dst, const uint8_t *src)
{
	asm volatile (
		"ld4 {v1.s, v2.s, v3.s, v4.s}[0], [%0]\n\t"
		"st4 {v1.s, v2.s, v3.s, v4.s}[0], [%1]\n\t"

		: "+r" (src), "+r" (dst)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * Copy 32 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov32_a64(uint8_t *dst, const uint8_t *src)
{
	asm volatile (
		"ld4 {v1.d, v2.d, v3.d, v4.d}[0], [%0]\n\t"
		"st4 {v1.d, v2.d, v3.d, v4.d}[0], [%1]\n\t"

		: "+r" (src), "+r" (dst)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * Copy 64 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov64_a64(uint8_t *dst, const uint8_t *src)
{
	asm volatile (
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0]\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1]\n\t"

		: "+r" (src), "+r" (dst)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * Copy 128 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov128_a64(uint8_t *dst, const uint8_t *src)
{
	asm volatile (
		/* "PRFM PLDL1STRM, [%0, #64]\n\t"
		"PRFM PSTL1STRM, [%1, #64]\n\t" */
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0], #64\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1], #64\n\t"
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0]\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1]\n\t"

		: "+r" (src), "+r" (dst)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * Copy 256 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov256_a64(uint8_t *dst, const uint8_t *src)
{
	asm volatile (
		/* "PRFM PLDL1STRM, [%0, #64]\n\t"
		"PRFM PSTL1STRM, [%1, #64]\n\t" */
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0], #64\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1], #64\n\t"
		/* "PRFM PLDL1STRM, [%0, #128]\n\t"
		"PRFM PSTL1STRM, [%1, #128]\n\t" */
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0], #64\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1], #64\n\t"
		/* "PRFM PLDL1STRM, [%0, #192]\n\t"
		"PRFM PSTL1STRM, [%1, #192]\n\t" */
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0], #64\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1], #64\n\t"
		"ld4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%0]\n\t"
		"st4 {v1.2d, v2.2d, v3.2d, v4.2d}, [%1]\n\t"

		: "+r" (src), "+r" (dst)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * Copy 64-byte blocks from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov64blocks_a64(uint8_t *dst, const uint8_t *src, size_t n)
{
	while (n >= 64) {
		n -= 64;
		odp_mov64_a64(dst, src);
		src = (const uint8_t *)src + 64;
		dst = (uint8_t *)dst + 64;
	}
}

/**
 * Copy 256-byte blocks from one location to another,
 * locations should not overlap.
 */
static inline void
odp_mov256blocks_a64(uint8_t *dst, const uint8_t *src, size_t n)
{
	while (n >= 256) {
		n -= 256;
		odp_mov256_a64(dst, src);
		src = (const uint8_t *)src + 256;
		dst = (uint8_t *)dst + 256;
	}
}

static inline void *
odp_memcpy_a64(void *dst, const void *src, size_t n)
{
	uintptr_t dstu = (uintptr_t)dst;
	uintptr_t srcu = (uintptr_t)src;
	void *ret = dst;
	size_t dstofss;
	size_t bits;

	/**
	 * Copy less than 16 bytes
	 */
	if (n < 16) {
		if (n & 0x01) {
			*(uint8_t *)dstu = *(const uint8_t *)srcu;
			srcu = (uintptr_t)((const uint8_t *)srcu + 1);
			dstu = (uintptr_t)((uint8_t *)dstu + 1);
		}
		if (n & 0x02) {
			*(uint16_t *)dstu = *(const uint16_t *)srcu;
			srcu = (uintptr_t)((const uint16_t *)srcu + 1);
			dstu = (uintptr_t)((uint16_t *)dstu + 1);
		}
		if (n & 0x04) {
			*(uint32_t *)dstu = *(const uint32_t *)srcu;
			srcu = (uintptr_t)((const uint32_t *)srcu + 1);
			dstu = (uintptr_t)((uint32_t *)dstu + 1);
		}
		if (n & 0x08)
			*(uint64_t *)dstu = *(const uint64_t *)srcu;

		return ret;
	}

	/**
	 * Fast way when copy size doesn't exceed 512 bytes
	 */
	if (n <= 32) {
		odp_mov16_a64((uint8_t *)dst, (const uint8_t *)src);
		odp_mov16_a64((uint8_t *)dst - 16 + n,
			(const uint8_t *)src - 16 + n);
		return ret;
	}
	if (n <= 64) {
		odp_mov32_a64((uint8_t *)dst, (const uint8_t *)src);
		odp_mov32_a64((uint8_t *)dst - 32 + n,
			(const uint8_t *)src - 32 + n);
		return ret;
	}
	if (n <= 512) {
		if (n >= 256) {
			n -= 256;
			odp_mov256_a64((uint8_t *)dst, (const uint8_t *)src);
			src = (const uint8_t *)src + 256;
			dst = (uint8_t *)dst + 256;
		}
		if (n >= 128) {
			n -= 128;
			odp_mov128_a64((uint8_t *)dst, (const uint8_t *)src);
			src = (const uint8_t *)src + 128;
			dst = (uint8_t *)dst + 128;
		}
		if (n >= 64) {
			n -= 64;
			odp_mov64_a64((uint8_t *)dst, (const uint8_t *)src);
			src = (const uint8_t *)src + 64;
			dst = (uint8_t *)dst + 64;
		}
COPY_BLOCK_64_BACK31:
		if (n > 32) {
			odp_mov32_a64((uint8_t *)dst, (const uint8_t *)src);
			odp_mov32_a64((uint8_t *)dst - 32 + n,
				(const uint8_t *)src - 32 + n);
			return ret;
		}
		if (n > 0) {
			odp_mov32_a64((uint8_t *)dst - 32 + n,
				(const uint8_t *)src - 32 + n);
		}
		return ret;
	}

	/**
	 * Make store aligned when copy size exceeds 512 bytes
	 */
	dstofss = 32 - ((uintptr_t)dst & 0x1F);
	n -= dstofss;
	odp_mov32_a64((uint8_t *)dst, (const uint8_t *)src);
	src = (const uint8_t *)src + dstofss;
	dst = (uint8_t *)dst + dstofss;

	/**
	 * Copy 256-byte blocks.
	 * Use copy block function for better instruction order control,
	 * which is important when load is unaligned.
	 */
	odp_mov256blocks_a64((uint8_t *)dst, (const uint8_t *)src, n);
	bits = n;
	n = n & 255;
	bits -= n;
	src = (const uint8_t *)src + bits;
	dst = (uint8_t *)dst + bits;

	/**
	 * Copy 64-byte blocks.
	 * Use copy block function for better instruction order control,
	 * which is important when load is unaligned.
	 */
	if (n >= 64) {
		odp_mov64blocks_a64((uint8_t *)dst, (const uint8_t *)src, n);
		bits = n;
		n = n & 63;
		bits -= n;
		src = (const uint8_t *)src + bits;
		dst = (uint8_t *)dst + bits;
	}

	/**
	 * Copy whatever left
	 */
	goto COPY_BLOCK_64_BACK31;
}
/**
 * set 16 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set16_a64(void *ptr, int *value)
{
	asm volatile (
		"ld1r {v1.16b}, [%0]\n\t"
		"st1 {v1.4s}, [%1]\n\t"
		: "+r" (value), "+r" (ptr)
		: : "memory", "v1");
}

/**
 * set 32 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set32_a64(void *ptr, int *value)
{
	asm volatile (
		"ld1r {v1.16b}, [%0]\n\t"
		"mov v2.16b, v1.16b\n\t"
		"st2 {v1.16b, v2.16b}, [%1]\n\t"
		: "+r" (value), "+r" (ptr)
		: : "memory", "v1", "v2");
}

/**
 * set 64 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set64_a64(void *ptr, int *value)
{
	asm volatile (
		"ld1r {v1.16b}, [%0]\n\t"
		"st1 {v1.s}[0], [%0]\n\t"
		"ld4r {v1.16b, v2.16b, v3.16b, v4.16b}, [%0]\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1]\n\t"
		: "+r" (value), "+r" (ptr)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * set 128 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set128_a64(void *ptr, int *value)
{
	asm volatile (
		"ld1r {v1.16b}, [%0]\n\t"
		"st1 {v1.s}[0], [%0]\n\t"
		"ld4r {v1.16b, v2.16b, v3.16b, v4.16b}, [%0]\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1], #64\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1]\n\t"
		: "+r" (value), "+r" (ptr)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * set 256 bytes from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set256_a64(void *ptr, int *value)
{
	asm volatile (
		"ld1r {v1.16b}, [%0]\n\t"
		"st1 {v1.s}[0], [%0]\n\t"
		"ld4r {v1.16b, v2.16b, v3.16b, v4.16b}, [%0]\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1], #64\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1], #64\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1], #64\n\t"
		"st4 {v1.16b, v2.16b, v3.16b, v4.16b}, [%1]\n\t"
		: "+r" (value), "+r" (ptr)
		: : "memory", "v1", "v2", "v3", "v4");
}

/**
 * set 64-byte blocks from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set64blocks_a64(void *ptr, int *value, size_t n)
{
	while (n >= 64) {
		n -= 64;
		odp_set64_a64(ptr, value);
		ptr = (uint8_t *)ptr + 64;
	}
}

/**
 * set 256-byte blocks from one location to another,
 * locations should not overlap.
 */
static inline void
odp_set256blocks_a64(void *ptr, int *value, size_t n)
{
	while (n >= 256) {
		n -= 256;
		odp_set256_a64(ptr, value);
		ptr = (uint8_t *)ptr + 256;
	}
}
static inline void *odp_memset_a64(void *ptr, int value_t, size_t n)
{
	uintptr_t dstu = (uintptr_t)ptr;
	void *ret = ptr;
	size_t dstofss;
	size_t bits;
	void *value = (void *)&value_t;

	/**
	 * set less than 16 bytes
	 */
	if (n < 16) {
		if (n & 0x01) {
			*(uint8_t *)dstu = (uint8_t)value_t;
			dstu = (uintptr_t)((uint8_t *)dstu + 1);
		}
		if (n & 0x02) {
			*(uint16_t *)dstu = (uint16_t)value_t;
			dstu = (uintptr_t)((uint16_t *)dstu + 1);
		}
		if (n & 0x04) {
			*(uint32_t *)dstu = (uint32_t)value_t;
			dstu = (uintptr_t)((uint32_t *)dstu + 1);
		}
		if (n & 0x08) {
			*(uint32_t *)dstu = (uint32_t)value_t;
			dstu = (uintptr_t)((uint32_t *)dstu + 1);
			*(uint32_t *)dstu = (uint32_t)value_t;
			dstu = (uintptr_t)((uint32_t *)dstu + 1);
		}
		return ret;
	}

	/**
	 * Fast way when set size doesn't exceed 512 bytes
	 */
	if (n <= 32) {
		odp_set16_a64(ptr, value);
		odp_set16_a64(ptr - 16 + n, value);
		return ret;
	}
	if (n <= 64) {
		odp_set32_a64(ptr, value);
		odp_set32_a64(ptr - 32 + n, value);
		return ret;
	}
	if (n <= 512) {
		if (n >= 256) {
			n -= 256;
			odp_set256_a64(ptr, value);
			ptr = ptr + 256;
		}
		if (n >= 128) {
			n -= 128;
			odp_set128_a64(ptr, value);
			ptr = ptr + 128;
		}
		if (n >= 64) {
			n -= 64;
			odp_set64_a64(ptr, value);
			ptr = ptr + 64;
		}
SET_BLOCK_64_BACK31:
		if (n > 32) {
			odp_set32_a64(ptr, value);
			odp_set32_a64(ptr - 32 + n, value);
			return ret;
		}
		if (n > 0)
			odp_set32_a64(ptr - 32 + n, value);

		return ret;
	}

	/**
	 * Make store aligned when set size exceeds 512 bytes
	 */
	dstofss = 32 - ((uintptr_t)ptr & 0x1F);
	n -= dstofss;
	odp_set32_a64(ptr, value);
	ptr = ptr + dstofss;

	/**
	 * Set 256-byte blocks.
	 * Use copy block function for better instruction order control,
	 * which is important when load is unaligned.
	 */
	odp_set256blocks_a64(ptr, value, n);
	bits = n;
	n = n & 255;
	bits -= n;
	ptr = ptr + bits;

	/**
	 * Set 64-byte blocks.
	 * Use set block function for better instruction order control,
	 * which is important when load is unaligned.
	 */
	if (n >= 64) {
		odp_set64blocks_a64(ptr, value, n);
		bits = n;
		n = n & 63;
		bits -= n;
		ptr = ptr + bits;
	}

	/**
	 * set whatever left
	 */
	goto SET_BLOCK_64_BACK31;
}
static inline void *odp_memcpy(void *dst, const void *src, size_t num)
{
	return odp_memcpy_a64(dst, src, num);
}

static inline void *odp_memset(void *ptr, int value, size_t num)
{
	return odp_memset_a64(ptr, value, num);
}

static inline int odp_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	return memcmp(ptr1, ptr2, num);
}

#ifdef __cplusplus
}
#endif

#endif
