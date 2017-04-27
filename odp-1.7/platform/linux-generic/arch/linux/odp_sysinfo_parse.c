/* Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_internal.h>
#include <string.h>

int odp_cpuinfo_parser(FILE *file ODP_UNUSED,
		       odp_system_info_t *sysinfo ODP_UNUSED)
{
	return 0;
}

uint64_t odp_cpu_hz_current(int id ODP_UNUSED)
{
	return 0;
}
