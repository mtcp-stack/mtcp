/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ODP_COMMON_H_
#define _ODP_COMMON_H_

/**
 * @file
 *
 * Generic, commonly-used macro and inline function definitions
 * for Huawei ODP.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#ifndef typeof
#define typeof __typeof__
#endif

#ifndef asm
#define asm __asm__
#endif

#ifndef __ELASTERROR

/**
 * Check if we have a defined value for the max system-defined errno values.
 * if no max defined, start from 1000 to prevent overlap with standard values
 */
#define __ELASTERROR 1000
#endif

/** Error types */
enum {
	/**< Start numbering above std errno vals */
	ODP_MIN_ERRNO = __ELASTERROR,

	/**< Operation not allowed in secondary processes */
	E_ODP_SECONDARY,

	E_ODP_NO_CONFIG, /**< Missing odp_config */

	ODP_MAX_ERRNO    /**< Max ODP error number */
};

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#define __odp_unused __attribute__((__unused__))

/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define ODP_SET_USED(x) (void)(x)

/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset from a pointer
 */
#define ODP_PTR_ADD(ptr, x) ((void *)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define ODP_PTR_SUB(ptr, x) ((void *)((uintptr_t)ptr - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define ODP_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/*********** Macros/static functions for doing alignment ********/

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define ODP_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))ODP_ALIGN_FLOOR((uintptr_t)ptr, align))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define ODP_ALIGN_FLOOR(val, align) \
	(typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define ODP_PTR_ALIGN_CEIL(ptr, align) \
	ODP_PTR_ALIGN_FLOOR((typeof(ptr))ODP_PTR_ADD(ptr, (align) - 1), align)

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define ODP_ALIGN_CEIL(val, align) \
	ODP_ALIGN_FLOOR(((val) + ((typeof(val))(align) - 1)), align)

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as ODP_PTR_ALIGN_CEIL
 */
#define ODP_PTR_ALIGN(ptr, align) ODP_PTR_ALIGN_CEIL(ptr, align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as ODP_ALIGN_CEIL
 */
#define ODP_ALIGN(val, align) ODP_ALIGN_CEIL(val, align)

/**
 * Checks if a pointer is aligned to a given power-of-two value
 *
 * @param ptr
 *   The pointer whose alignment is to be checked
 * @param align
 *   The power-of-two value to which the ptr should be aligned
 *
 * @return
 *   True(1) where the pointer is correctly aligned, false(0) otherwise
 */
static inline int odp_is_aligned(void *ptr, unsigned align)
{
	return ODP_PTR_ALIGN(ptr, align) == ptr;
}

/*********** Macros for compile type checks ********/

/**
 * Triggers an error at compilation time if the condition is true.
 */
#ifndef __OPTIMIZE__
#define ODP_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))
#else
extern int ODP_BUILD_BUG_ON_detected_error;

#define ODP_BUILD_BUG_ON(condition) do {             \
		((void)sizeof(char[1 - 2 * !!(condition)]));   \
		if (condition)                               \
			ODP_BUILD_BUG_ON_detected_error = 1; \
} while (0)
#endif

/*********** Macros to work with powers of 2 ********/

/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int odp_is_power_of_2(uint32_t n)
{
	return n && !(n & (n - 1));
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to algin
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint32_t odp_align32pow2(uint32_t x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return x + 1;
}

/**
 * Aligns 64b input parameter to the next power of 2
 *
 * @param x
 *   The 64b value to algin
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint64_t odp_align64pow2(uint64_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	return v + 1;
}

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define ODP_MIN(a, b) ({ \
		typeof(a)_a = (a); \
		typeof(b)_b = (b); \
		_a < _b ? _a : _b; \
	})

/**
 * Macro to return the maximum of two numbers
 */
#define ODP_MAX(a, b) ({ \
		typeof(a)_a = (a); \
		typeof(b)_b = (b); \
		_a > _b ? _a : _b; \
	})

/*********** Other general functions / macros ********/

#ifdef __SSE2__
#include <emmintrin.h>

/**
 * PAUSE instruction for tight loops (avoid busy waiting)
 */
static inline void odp_pause(void)
{
	_mm_pause();
}

#else
static inline void odp_pause(void)
{
}
#endif

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t odp_bsf32(uint32_t v)
{
	return __builtin_ctz(v);
}

#ifndef offsetof

/** Return the offset of a field in a structure. */
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif

#define _ODP_STR(x) # x

/** Take a macro value and get a string version of it */
#define ODP_STR(x) _ODP_STR(x)

/** Mask value of type <tp> for the first <ln> bit set. */
#define	ODP_LEN2MASK(ln, tp)   \
	((tp)((uint64_t)-1 >> (sizeof(uint64_t) * CHAR_BIT - (ln))))

/** Number of elements in the array. */
#define	ODP_DIM(a) (sizeof(a) / sizeof((a)[0]))

/**
 * Converts a numeric string to the equivalent uint64_t value.
 * As well as straight number conversion, also recognises the suffixes
 * k, m and g for kilobytes, megabytes and gigabytes respectively.
 *
 * If a negative number is passed in  i.e. a string with the first non-black
 * character being "-", zero is returned. Zero is also returned in the case of
 * an error with the strtoull call in the function.
 *
 * @param str
 *     String containing number to convert.
 * @return
 *     Number.
 */
static inline uint64_t odp_str_to_size(const char *str)
{
	char *endptr;
	unsigned long long size;

	while (isspace((int)*str))
		str++;

	if (*str == '-')
		return 0;

	errno = 0;
	size  = strtoull(str, &endptr, 0);
	if (errno)
		return 0;

	if (*endptr == ' ')
		endptr++; /* allow 1 space gap */

	switch (*endptr) {
	case 'G':
	case 'g':
		size *= 1024;           /* fall-through */
	case 'M':
	case 'm':
		size *= 1024;           /* fall-through */
	case 'K':
	case 'k':
		size *= 1024;           /* fall-through */
	default:
		break;
	}

	return size;
}

/* split string into tokens */
static inline int odp_strsplit(char *string, int stringlen,
			       char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if ((!string) || (!tokens))
		goto einval_error;

	for (i = 0; i < stringlen; i++) {
		if ((string[i] == '\0') || (tok >= maxtokens))
			break;

		if (tokstart) {
			tokstart = 0;
			tokens[tok++] = &string[i];
		}

		if (string[i] == delim) {
			string[i] = '\0';
			tokstart  = 1;
		}
	}

	return tok;

einval_error:
	errno = EINVAL;
	return -1;
}

#ifdef __cplusplus
}
#endif
#endif
