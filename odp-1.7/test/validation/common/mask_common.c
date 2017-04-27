/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>

#include "odp_cunit_common.h"
#include "mask_common.h"

/*
 * The following strings are used to build masks with odp_*mask_from_str().
 * Both 0x prefixed and non prefixed hex values are supported.
 */
#define TEST_MASK_NONE    "0x0"
#define TEST_MASK_0       "0x1"
#define TEST_MASK_1       "0x2"
#define TEST_MASK_2       "0x4"
#define TEST_MASK_0_2     "0x5"
#define TEST_MASK_0_3     "0x9"
#define TEST_MASK_1_2     "0x6"
#define TEST_MASK_1_3     "0xA"
#define TEST_MASK_0_1_2   "0x7"
#define TEST_MASK_0_2_4_6 "0x55"
#define TEST_MASK_1_2_4_6 "0x56"

#define TEST_MASK_0_NO_PREFIX       "1"

/* padding pattern used to check buffer overflow: */
#define FILLING_PATTERN 0x55

/*
 * returns the length of a string, excluding terminating NULL.
 * As its C lib strlen equivalent. Just rewritten here to avoid C lib
 * dependency in ODP tests (for platform independent / bare metal testing)
 */
static unsigned int stringlen(const char *str)
{
	unsigned int i = 0;

	while (str[i] != 0)
		i++;
	return i;
}

/*
 * builds a string containing a 0x prefixed hex number where a single bit
 * (corresponding to a cpu or thread) is set.
 * The string is null terminated.
 * bit_set_str(0) returns "0x1".
 * bit_set_str(10) returns "0x400".
 * The buffer should be at least ceil(offs/4)+3 bytes long,
 * to accommodate with 4 bits per nibble + "0x" prefix + null.
 */
#define BITS_PER_NIBBLE 4
static void bit_set_str(char *buff, int offs)
{
	const char *hex_nibble = "1248";
	int i = 0;

	buff[i++] = '0';
	buff[i++] = 'x';
	buff[i++] = hex_nibble[offs % BITS_PER_NIBBLE];
	while (offs > 3) {
		buff[i++] = '0';
		offs -= BITS_PER_NIBBLE;
	}
	buff[i++] = 0; /* null */
}

/*
 * Returns the maximum number of CPUs that a mask can contain.
 */
unsigned mask_capacity(void)
{
	_odp_mask_t mask;

	_odp_mask_setall(&mask);

	return _odp_mask_count(&mask);
}

MASK_TESTFUNC(to_from_str)
{
	_odp_mask_t mask;
	int32_t str_sz;
	unsigned int buf_sz; /* buf size for the 2 following bufs */
	char *buf_in;
	char *buf_out;
	unsigned int cpu;
	unsigned int i;

	/* makes sure the mask has room for at least 1 CPU...: */
	CU_ASSERT_FATAL(mask_capacity() > 0);

	/* allocate memory for the buffers containing the mask strings:
	   1 char per nibble, i.e. 1 char per 4 cpus +extra for "0x" and null:*/
	buf_sz = (mask_capacity() >> 2) + 20;
	buf_in  = malloc(buf_sz);
	buf_out = malloc(buf_sz);
	CU_ASSERT_FATAL(buf_in && buf_out);

	/* test 1 CPU at a time for all possible cpu positions in the mask */
	for (cpu = 0; cpu < mask_capacity(); cpu++) {
		/* init buffer for overwrite check: */
		for (i = 0; i < buf_sz; i++)
			buf_out[i] = FILLING_PATTERN;

		/* generate a hex string with that cpu set: */
		bit_set_str(buf_in, cpu);

		/* generate mask: */
		_odp_mask_from_str(&mask, buf_in);

		/* reverse cpu mask computation to get string back: */
		str_sz = _odp_mask_to_str(&mask, buf_out,
					  stringlen(buf_in) + 1);

		/* check that returned size matches original (with NULL): */
		CU_ASSERT(str_sz == (int32_t)stringlen(buf_in) + 1);

		/* check that returned string matches original (with NULL): */
		CU_ASSERT_NSTRING_EQUAL(buf_out, buf_in, stringlen(buf_in) + 1);

		/* check that no extra buffer writes occurred: */
		CU_ASSERT(buf_out[stringlen(buf_in) + 2] == FILLING_PATTERN);
	}

	/* re-init buffer for overwrite check: */
	for (i = 0; i < buf_sz; i++)
		buf_out[i] = FILLING_PATTERN;

	/* check for buffer overflow when too small buffer given: */
	_odp_mask_from_str(&mask, TEST_MASK_0);
	str_sz = _odp_mask_to_str(&mask, buf_out, stringlen(TEST_MASK_0));

	CU_ASSERT(str_sz == -1);

	for (i = 0; i < buf_sz; i++)
		CU_ASSERT(buf_out[i] == FILLING_PATTERN);

	/* check for handling of missing "0x" prefix: */
	_odp_mask_from_str(&mask, TEST_MASK_0_NO_PREFIX);

	str_sz = _odp_mask_to_str(&mask, buf_out,
				  stringlen(TEST_MASK_0) + 1);
	CU_ASSERT(str_sz  == (int32_t)stringlen(TEST_MASK_0) + 1);

	CU_ASSERT_NSTRING_EQUAL(buf_out, TEST_MASK_0,
				stringlen(TEST_MASK_0) + 1);

	free(buf_out);
	free(buf_in);
}

MASK_TESTFUNC(equal)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;
	_odp_mask_t mask3;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask3, TEST_MASK_NONE);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
	CU_ASSERT_FALSE(_odp_mask_equal(&mask1, &mask3));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	_odp_mask_from_str(&mask2, TEST_MASK_0_2);
	_odp_mask_from_str(&mask3, TEST_MASK_1_2);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
	CU_ASSERT_FALSE(_odp_mask_equal(&mask1, &mask3));

	if (mask_capacity() < 8)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2_4_6);
	_odp_mask_from_str(&mask2, TEST_MASK_0_2_4_6);
	_odp_mask_from_str(&mask3, TEST_MASK_1_2_4_6);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
	CU_ASSERT_FALSE(_odp_mask_equal(&mask1, &mask3));
}

MASK_TESTFUNC(zero)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_zero(&mask2);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
}

MASK_TESTFUNC(set)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_set(&mask1, 0);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask2, TEST_MASK_0_3);
	_odp_mask_set(&mask1, 3);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));

	/* make sure that re-asserting a cpu has no impact: */
	_odp_mask_set(&mask1, 3);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
}

MASK_TESTFUNC(clr)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_from_str(&mask2, TEST_MASK_NONE);
	_odp_mask_clr(&mask1, 0);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_clr(&mask1, 2);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));

	_odp_mask_from_str(&mask2, TEST_MASK_NONE);
	_odp_mask_clr(&mask1, 0);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));

	/* make sure that re-clearing a cpu has no impact: */
	_odp_mask_clr(&mask1, 0);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
}

MASK_TESTFUNC(isset)
{
	_odp_mask_t mask1;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	CU_ASSERT(_odp_mask_isset(&mask1, 0));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	CU_ASSERT_FALSE(_odp_mask_isset(&mask1, 0));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	CU_ASSERT(_odp_mask_isset(&mask1, 0));
	CU_ASSERT_FALSE(_odp_mask_isset(&mask1, 1));
	CU_ASSERT(_odp_mask_isset(&mask1, 2));
	CU_ASSERT_FALSE(_odp_mask_isset(&mask1, 3));
}

MASK_TESTFUNC(count)
{
	_odp_mask_t mask1;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	CU_ASSERT(_odp_mask_count(&mask1) == 1);

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	CU_ASSERT(_odp_mask_count(&mask1) == 0);

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	CU_ASSERT(_odp_mask_count(&mask1) == 2);
}

MASK_TESTFUNC(and)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;
	_odp_mask_t mask3;
	_odp_mask_t mask4;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_0);
	_odp_mask_and(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_NONE);
	_odp_mask_and(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_NONE);
	_odp_mask_from_str(&mask4, TEST_MASK_NONE);
	_odp_mask_and(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	_odp_mask_from_str(&mask2, TEST_MASK_1_2);
	_odp_mask_from_str(&mask4, TEST_MASK_2);
	_odp_mask_and(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));
}

MASK_TESTFUNC(or)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;
	_odp_mask_t mask3;
	_odp_mask_t mask4;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_0);
	_odp_mask_or(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_0);
	_odp_mask_or(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_NONE);
	_odp_mask_from_str(&mask4, TEST_MASK_NONE);
	_odp_mask_or(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_0_2);
	_odp_mask_from_str(&mask2, TEST_MASK_1);
	_odp_mask_from_str(&mask4, TEST_MASK_0_1_2);
	_odp_mask_or(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));
}

MASK_TESTFUNC(xor)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;
	_odp_mask_t mask3;
	_odp_mask_t mask4;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_NONE);
	_odp_mask_xor(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_0);
	_odp_mask_from_str(&mask4, TEST_MASK_0);
	_odp_mask_xor(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	_odp_mask_from_str(&mask2, TEST_MASK_NONE);
	_odp_mask_from_str(&mask4, TEST_MASK_NONE);
	_odp_mask_xor(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_2);
	_odp_mask_from_str(&mask2, TEST_MASK_1_2);
	_odp_mask_from_str(&mask4, TEST_MASK_1);
	_odp_mask_xor(&mask3, &mask1, &mask2);
	CU_ASSERT(_odp_mask_equal(&mask3, &mask4));
}

MASK_TESTFUNC(copy)
{
	_odp_mask_t mask1;
	_odp_mask_t mask2;

	_odp_mask_from_str(&mask1, TEST_MASK_0);
	_odp_mask_copy(&mask2, &mask1);
	CU_ASSERT(_odp_mask_equal(&mask1, &mask2));
}

MASK_TESTFUNC(first)
{
	_odp_mask_t mask1;

	/* check when there is no first */
	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	CU_ASSERT(_odp_mask_first(&mask1) == -1);

	/* single CPU case: */
	_odp_mask_from_str(&mask1, TEST_MASK_0);
	CU_ASSERT(_odp_mask_first(&mask1) == 0);

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_1_3);
	CU_ASSERT(_odp_mask_first(&mask1) == 1);
}

MASK_TESTFUNC(last)
{
	_odp_mask_t mask1;

	/* check when there is no last: */
	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	CU_ASSERT(_odp_mask_last(&mask1) == -1);

	/* single CPU case: */
	_odp_mask_from_str(&mask1, TEST_MASK_0);
	CU_ASSERT(_odp_mask_last(&mask1) == 0);

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_1_3);
	CU_ASSERT(_odp_mask_last(&mask1) == 3);
}

MASK_TESTFUNC(next)
{
	unsigned int i;
	int expected[] = {1, 3, 3, -1};
	_odp_mask_t mask1;

	/* case when the mask does not contain any CPU: */
	_odp_mask_from_str(&mask1, TEST_MASK_NONE);
	CU_ASSERT(_odp_mask_next(&mask1, -1) == -1);

	/* case when the mask just contain CPU 0: */
	_odp_mask_from_str(&mask1, TEST_MASK_0);
	CU_ASSERT(_odp_mask_next(&mask1, -1) == 0);
	CU_ASSERT(_odp_mask_next(&mask1, 0)  == -1);

	if (mask_capacity() < 4)
		return;

	_odp_mask_from_str(&mask1, TEST_MASK_1_3);

	for (i = 0; i < sizeof(expected) / sizeof(int); i++)
		CU_ASSERT(_odp_mask_next(&mask1, i) == expected[i]);
}

MASK_TESTFUNC(setall)
{
	int num;
	int max = mask_capacity();
	_odp_mask_t mask;

	_odp_mask_setall(&mask);
	num = _odp_mask_count(&mask);

	CU_ASSERT(num > 0);
	CU_ASSERT(num <= max);
}
