/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp/errno.h>
#include <odp_internal.h>
#include <string.h>
#include <stdio.h>
#include <odp_debug_internal.h>

__thread int __odp_errno;

int odp_errno(void)
{
	return __odp_errno;
}

void odp_errno_zero(void)
{
	__odp_errno = 0;
}

void odp_errno_print(const char *str)
{
	if (str != NULL)
		printf("%s ", str);

	ODP_PRINT("%s\n", strerror(__odp_errno));
}

const char *odp_errno_str(int errnum)
{
	return strerror(errnum);
}
