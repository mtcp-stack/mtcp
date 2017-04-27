/* Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_internal.h>
#include <string.h>

int odp_cpuinfo_parser(FILE *file, odp_system_info_t *sysinfo)
{
	char str[1024];
	char *pos;
	double mhz = 0.0;
	uint64_t hz;
	int model = 0;
	int count = 2;
	int id = 0;

	strcpy(sysinfo->cpu_arch_str, "powerpc");
	while (fgets(str, sizeof(str), file) != NULL && id < MAX_CPU_NUMBER) {
		if (!mhz) {
			pos = strstr(str, "clock");

			if (pos)
				if (sscanf(pos, "clock : %lf", &mhz) == 1) {
					hz = (uint64_t)(mhz * 1000000.0);
					sysinfo->cpu_hz_max[id] = hz;
					count--;
				}
		}

		if (!model) {
			pos = strstr(str, "cpu");

			if (pos) {
				int len;

				pos = strchr(str, ':');
				strncpy(sysinfo->model_str[id], pos + 2,
					sizeof(sysinfo->model_str[id]));
				len = strlen(sysinfo->model_str[id]);
				sysinfo->model_str[id][len - 1] = 0;
				model = 1;
				count--;
			}
		}

		if (count == 0) {
			mhz = 0.0;
			model = 0;
			count = 2;
			id++;
		}
	}

	return 0;
}

uint64_t odp_cpu_hz_current(int id ODP_UNUSED)
{
	return 0;
}
