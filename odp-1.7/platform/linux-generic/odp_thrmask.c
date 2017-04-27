/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/thrmask.h>
#include <odp/cpumask.h>

void odp_thrmask_from_str(odp_thrmask_t *mask, const char *str)
{
	odp_cpumask_from_str(&mask->m, str);
}

int32_t odp_thrmask_to_str(const odp_thrmask_t *mask, char *str, int32_t size)
{
	return odp_cpumask_to_str(&mask->m, str, size);
}

void odp_thrmask_zero(odp_thrmask_t *mask)
{
	odp_cpumask_zero(&mask->m);
}

void odp_thrmask_set(odp_thrmask_t *mask, int thr)
{
	odp_cpumask_set(&mask->m, thr);
}

void odp_thrmask_setall(odp_thrmask_t *mask)
{
	odp_cpumask_setall(&mask->m);
}

void odp_thrmask_clr(odp_thrmask_t *mask, int thr)
{
	odp_cpumask_clr(&mask->m, thr);
}

int odp_thrmask_isset(const odp_thrmask_t *mask, int thr)
{
	return odp_cpumask_isset(&mask->m, thr);
}

int odp_thrmask_count(const odp_thrmask_t *mask)
{
	return odp_cpumask_count(&mask->m);
}

void odp_thrmask_and(odp_thrmask_t *dest, const odp_thrmask_t *src1,
		     const odp_thrmask_t *src2)
{
	odp_cpumask_and(&dest->m, &src1->m, &src2->m);
}

void odp_thrmask_or(odp_thrmask_t *dest, const odp_thrmask_t *src1,
		    const odp_thrmask_t *src2)
{
	odp_cpumask_or(&dest->m, &src1->m, &src2->m);
}

void odp_thrmask_xor(odp_thrmask_t *dest, const odp_thrmask_t *src1,
		     const odp_thrmask_t *src2)
{
	odp_cpumask_xor(&dest->m, &src1->m, &src2->m);
}

int odp_thrmask_equal(const odp_thrmask_t *mask1,
		      const odp_thrmask_t *mask2)
{
	return odp_cpumask_equal(&mask1->m, &mask2->m);
}

void odp_thrmask_copy(odp_thrmask_t *dest, const odp_thrmask_t *src)
{
	odp_cpumask_copy(&dest->m, &src->m);
}

int odp_thrmask_first(const odp_thrmask_t *mask)
{
	return odp_cpumask_first(&mask->m);
}

int odp_thrmask_last(const odp_thrmask_t *mask)
{
	return odp_cpumask_last(&mask->m);
}

int odp_thrmask_next(const odp_thrmask_t *mask, int thr)
{
	return odp_cpumask_next(&mask->m, thr);
}
