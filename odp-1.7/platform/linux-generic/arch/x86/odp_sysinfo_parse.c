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
	double ghz = 0.0;
	uint64_t hz;
	int id = 0;

	strcpy(sysinfo->cpu_arch_str, "x86");
	while (fgets(str, sizeof(str), file) != NULL && id < MAX_CPU_NUMBER) {
		pos = strstr(str, "model name");
		if (pos) {
			pos = strchr(str, ':');
			strncpy(sysinfo->model_str[id], pos + 2,
				sizeof(sysinfo->model_str[id]));

			pos = strchr(sysinfo->model_str[id], '@');
			*(pos - 1) = '\0';
			if (sscanf(pos, "@ %lfGHz", &ghz) == 1) {
				hz = (uint64_t)(ghz * 1000000000.0);
				sysinfo->cpu_hz_max[id] = hz;
			}
			id++;
		}
	}

	return 0;
}

uint64_t odp_cpu_hz_current(int id)
{
	char str[1024];
	FILE *file;
	int cpu;
	char *pos;
	double mhz = 0.0;

	file = fopen("/proc/cpuinfo", "rt");

	/* find the correct processor instance */
	while (fgets(str, sizeof(str), file) != NULL) {
		pos = strstr(str, "processor");
		if (pos) {
			if (sscanf(pos, "processor : %d", &cpu) == 1)
				if (cpu == id)
					break;
		}
	}

	/* extract the cpu current speed */
	while (fgets(str, sizeof(str), file) != NULL) {
		pos = strstr(str, "cpu MHz");
		if (pos) {
			if (sscanf(pos, "cpu MHz : %lf", &mhz) == 1)
				break;
		}
	}

	fclose(file);
	if (mhz)
		return (uint64_t)(mhz * 1000000.0);

	return 0;
}
