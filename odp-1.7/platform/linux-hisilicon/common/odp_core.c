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
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>

#include <sys/queue.h>
#include <sys/syscall.h>

#include <odp/cpu.h>
#include <odp_base.h>
#include <odp/atomic.h>
#include <odp/config.h>
#include <odp/cpumask.h>
#include <odp_common.h>
#include <odp_core.h>
#include <odp_memory.h>

#include <odp/sync.h>
#include <odp/rwlock.h>

#include "odp_private.h"
#include "odp_filesystem.h"
#include "odp_core.h"
#include "odp_internal.h"
#include "odp_debug_internal.h"
#include "odp_syslayout.h"

ODP_DEFINE_PER_CORE(unsigned, _core_id) = CORE_ID_ANY;
ODP_DEFINE_PER_CORE(unsigned, _socket_id) = (unsigned)SOCKET_ID_ANY;
ODP_DEFINE_PER_CORE(cpu_set_t, _cpuset);

#define ODP_CPU_MANAGE_WLOCK \
	odp_rwlock_write_lock(&config->sys_layout->rw_lock)
#define ODP_CPU_MANAGE_WUNLOCK \
	odp_rwlock_write_unlock(&config->sys_layout->rw_lock)
#define ODP_CPU_MANAGE_RLOCK \
	odp_rwlock_read_lock(&config->sys_layout->rw_lock)
#define ODP_CPU_MANAGE_RUNLOCK \
	odp_rwlock_read_unlock(&config->sys_layout->rw_lock)

/* Get CPU socket id (NUMA node) by reading directory
 * /sys/devices/system/cpu/cpuX looking for symlink "nodeY"
 * which gives the NUMA topology information.
 * Note: physical package id != NUMA node, but we use it as a
 * fallback for kernels which don't create a nodeY link
 */
unsigned odp_cpu_socket_id(unsigned core_id)
{
	const char node_pref[] = "node";
	const size_t pref_len  = sizeof(node_pref) - 1;
	char path[ODP_PATH_MAX];
	DIR *d = NULL;
	unsigned long id = 0;
	struct dirent *e = NULL;
	char *endptr = NULL;

	int   len = snprintf(path, sizeof(path),
			     SYS_CPU_DIR, core_id);

	if (len <= 0)
		return 0;

	if ((unsigned)len >= sizeof(path))
		return 0;

	d = opendir(path);
	if (!d)
		return 0;

	while ((e = readdir(d)) != NULL)
		if (strncmp(e->d_name, node_pref, pref_len) == 0) {
			id = strtoul(e->d_name + pref_len, &endptr, 0);
			break;
		}

	if ((!endptr) || (*endptr != '\0') ||
	    (endptr == e->d_name + pref_len)) {
		/* ODP_PRINT("Cannot read numa node link for
		 * core %u - using physical package id instead\n", core_id); */
		len = snprintf(path, sizeof(path), SYS_CPU_DIR "/%s", core_id,
			       PHYS_PKG_FILE);
		if ((len <= 0) || ((unsigned)len >= sizeof(path))) {
			closedir(d);
			return 0;
		}

		id = odp_parse_sysfs_value(path);
		if (id == -1) {
			closedir(d);
			return 0;
		}
	}

	closedir(d);
	return (unsigned)id;
}

/* Get the cpu core id value from the /sys/.../cpuX core_id value */
static unsigned odp_cpu_core_id(unsigned core_id)
{
	char path[ODP_PATH_MAX];
	unsigned long id;

	int len = snprintf(path, sizeof(path), SYS_CPU_DIR "/%s",
			   core_id, CORE_ID_FILE);

	if ((len <= 0) || ((unsigned)len >= sizeof(path)))
		return 0;

	id = odp_parse_sysfs_value(path);
	if (id == -1)
		return 0;

	return (unsigned)id;
}

int odp_bind_proc_to_lcore(int lcore_id)
{
	int i;
	struct odp_config *config = odp_get_configuration();

	ODP_CPU_MANAGE_WLOCK;
	if (gcore_config[lcore_id].detected)
		for (i = 0; i < (ODP_MAX_CORE >> 6); i++)
			if (lcore_id < (sizeof(uint64_t) * 8 * (i + 1)) &&
			    lcore_id >= (sizeof(uint64_t) * 8 * i)) {
				if (config->sys_layout->
				    odp_lcore_status[i] &
				    (1 << (lcore_id & 63))) {
					ODP_CPU_MANAGE_WUNLOCK;
					return -1;
				}

				config->sys_layout->odp_lcore_status[i]
					|= (1 << (lcore_id & 63));
				ODP_CPU_MANAGE_WUNLOCK;

				return 0;
			}

	ODP_CPU_MANAGE_WUNLOCK;

	return -1;
}

int odp_unbind_proc_to_lcore(int lcore_id)
{
	int i;
	uint64_t lcore_mask = 0xffffffffffffffffull;
	struct odp_config *config = odp_get_configuration();

	ODP_CPU_MANAGE_WLOCK;
	if (gcore_config[lcore_id].detected)
		for (i = 0; i < (ODP_MAX_CORE >> 6); i++)
			if (lcore_id < (sizeof(uint64_t) * 8 * (i + 1)) &&
			    lcore_id >= (sizeof(uint64_t) * 8 * i)) {
				lcore_mask ^=
					(uint64_t)(1 << (lcore_id & 63));
				config->sys_layout->odp_lcore_status[i]
					&= lcore_mask;
				ODP_CPU_MANAGE_WUNLOCK;

				return 0;
			}

	ODP_CPU_MANAGE_WUNLOCK;

	return -1;
}

int odp_get_available_cpu(void)
{
	int i;
	int cpu_id;
	uint64_t lcore_mask = 0xffffffffffffffffull;
	struct odp_config *config = odp_get_configuration();

	ODP_CPU_MANAGE_WLOCK;
	for (cpu_id = 0; cpu_id < ODP_MAX_CORE; cpu_id++) {
		for (i = 0; i < (ODP_MAX_CORE >> 6); i++)
			if (cpu_id < (sizeof(uint64_t) * 8 * (i + 1)) &&
			    cpu_id >= (sizeof(uint64_t) * 8 * i)) {
				lcore_mask =
					config->sys_layout->
					odp_lcore_status[i] &
					(uint64_t)(1 << (cpu_id & 63));
				if (!lcore_mask &&
				    gcore_config[cpu_id].detected) {
					config->sys_layout->odp_lcore_status[i]
						|= (uint64_t)(1 <<
							      (cpu_id & 63));
					ODP_CPU_MANAGE_WUNLOCK;
					return cpu_id;
				}
			}
	}

	ODP_CPU_MANAGE_WUNLOCK;

	return -1;
}

int odp_check_cpu_status(int cpu_id)
{
	struct odp_config *config = odp_get_configuration();
	int i;
	uint64_t lcore_mask = 0;

	ODP_CPU_MANAGE_RLOCK;
	if (gcore_config[cpu_id].detected)
		for (i = 0; i < (ODP_MAX_CORE >> 6); i++)
			if (cpu_id < (sizeof(uint64_t) * 8 * (i + 1)) &&
			    cpu_id >= (sizeof(uint64_t) * 8 * i)) {
				lcore_mask =
					(uint64_t)(1 << (cpu_id & 63)) &
					config->sys_layout->odp_lcore_status[i];
				ODP_CPU_MANAGE_RUNLOCK;
				if (lcore_mask)
					return 1;
				else
					return 0;
			}

	ODP_CPU_MANAGE_RUNLOCK;

	return -1;
}

/*****************************************************************************
   Function     : odp_cpu_init
   Description  : cpu information init
   Input        : None
   Output       : None
   Return       : 0:successed;other,failed
   Create By    : x00180405
   Modification :
   1.created: 2015/7/2
   Restriction  :
*****************************************************************************/
int odp_cpu_init(void)
{
	/* pointer to global configuration */
	struct odp_config *config = odp_get_configuration();
	unsigned core_id;
	unsigned count = 0;
	enum odp_proc_type_t proc;

	proc = odp_proc_type_detect();
	if (proc == ODP_PROC_PRIMARY) {
		memset(config->sys_layout->odp_lcore_status, 0,
		       (sizeof(uint64_t) * (ODP_MAX_CORE >> 6)));

		odp_rwlock_init(&config->sys_layout->rw_lock);
	}

	config->global_data = odp_get_global_data();

	odp_rwlock_write_lock(&config->sys_layout->rw_lock);

	/*
	 * Parse the maximum set of logical cores, detect the subset of running
	 * ones and enable them by default.
	 */
	for (core_id = 0; core_id < odp_cpu_count(); core_id++) {
		/* init cpuset for per core config */
		odp_cpumask_zero(&gcore_config[core_id].cpuset);

		/* in 1:1 mapping, record related cpu detected state */
		gcore_config[core_id].detected = odp_cpu_detected(core_id);
		if (gcore_config[core_id].detected == NOT_ON) {
			config->core_role[core_id] = ROLE_OFF;
			continue;
		}

		/* By default, core 1:1 map to cpu id */
		odp_cpumask_set(&gcore_config[core_id].cpuset, (int)core_id);

		/* By default, each detected core is enabled */
		config->core_role[core_id] = ROLE_ODP;
		gcore_config[core_id].core_id = odp_cpu_core_id(core_id);
		gcore_config[core_id].socket_id = odp_cpu_socket_id(core_id);

		count++;
	}

	/* Set the count of enabled logical cores of the ODP configuration */
	config->core_num = count;
	odp_rwlock_write_unlock(&config->sys_layout->rw_lock);

	return 0;
}

unsigned odp_socket_id(void)
{
	return ODP_PER_CORE(_socket_id);
}

int odp_cpuset_socket_id(odp_cpumask_t *cpusetp)
{
	int cpu = 0;
	int socket_id = SOCKET_ID_ANY;
	int sid;

	if (!cpusetp)
		return SOCKET_ID_ANY;

	do {
		if (!odp_cpumask_isset(cpusetp, cpu))
			continue;

		if (socket_id == SOCKET_ID_ANY)
			socket_id = odp_cpu_socket_id((unsigned)cpu);

		sid = odp_cpu_socket_id((unsigned)cpu);
		if (socket_id != sid) {
			socket_id = SOCKET_ID_ANY;
			break;
		}
	} while (++cpu < ODP_MAX_CORE);

	return socket_id;
}

int odp_thread_set_affinity(odp_cpumask_t *cpusetp)
{
	int s;
	unsigned core_id;
	pthread_t tid;

	tid = pthread_self();

	s = pthread_setaffinity_np(tid, sizeof(odp_cpumask_t),
				   (cpu_set_t *)cpusetp);
	if (s != 0) {
		ODP_PRINT("pthread_setaffinity_np failed\n");
		return -1;
	}

	/* store socket_id in TLS for quick access */
	ODP_PER_CORE(_socket_id) =
		odp_cpuset_socket_id(cpusetp);

	/* store cpuset in TLS for quick access */
	memmove(&ODP_PER_CORE(_cpuset), cpusetp,
		sizeof(odp_cpumask_t));

	core_id = odp_core_id();
	if (core_id != (unsigned)CORE_ID_ANY) {
		/* HODP thread will update core_config */
		gcore_config[core_id].socket_id = ODP_PER_CORE(_socket_id);
		memmove(&gcore_config[core_id].cpuset, cpusetp,
			sizeof(odp_cpumask_t));
	}

	return 0;
}

void odp_thread_get_affinity(odp_cpumask_t *cpusetp)
{
	/* assert(cpusetp); */
	memmove(cpusetp, &ODP_PER_CORE(_cpuset),
		sizeof(odp_cpumask_t));
}

int odp_thread_dump_affinity(char *str, unsigned size)
{
	odp_cpumask_t cpuset;
	int cpu;
	int ret;
	unsigned int out = 0;

	odp_thread_get_affinity(&cpuset);

	for (cpu = 0; cpu < ODP_MAX_CORE; cpu++) {
		if (!odp_cpumask_isset(&cpuset, cpu))
			continue;

		ret = snprintf(str + out,
			       size - out, "%u,", cpu);
		if ((ret < 0) || ((unsigned)ret >= size - out)) {
			/* string will be truncated */
			ret = -1;
			goto exit;
		}

		out += ret;
	}

	ret = 0;
exit:

	/* remove the last separator */
	if (out > 0)
		str[out - 1] = '\0';

	return ret;
}

int odp_remote_launch(int (*f)(void *), void *arg, unsigned slave_id)
{
	int n;
	char c	= 0;
	int m2s = gcore_config[slave_id].pipe_master2slave[1];
	int s2m = gcore_config[slave_id].pipe_slave2master[0];

	if (gcore_config[slave_id].state != WAIT)
		return -EBUSY;

	gcore_config[slave_id].f = f;
	gcore_config[slave_id].arg = arg;

	/* send message */
	n = 0;
	while (n == 0 || (n < 0 && errno == EINTR))
		n = write(m2s, &c, 1);

	if (n < 0)
		ODP_PRINT("cannot write on configuration pipe\n");

	/* wait ack */
	do
		n = read(s2m, &c, 1);
	while (n < 0 && errno == EINTR);

	if (n <= 0)
		ODP_PRINT("cannot read on configuration pipe\n");

	return 0;
}

/* set affinity for current HODP thread */
static int odp_hisi_thread_set_affinity(void)
{
	unsigned core_id = odp_core_id();

	/* acquire system unique id  */
	odp_gettid();

	/* update HODP thread core affinity */
	return odp_thread_set_affinity(&gcore_config[core_id].cpuset);
}

void odp_thread_init_master(unsigned core_id)
{
	/* set the core ID in per-core memory area */
	ODP_PER_CORE(_core_id) = core_id;

	/* set CPU affinity */
	if (odp_hisi_thread_set_affinity() < 0)
		ODP_PRINT("cannot set affinity\n");
}

/* require calling thread tid by gettid() */
int odp_sys_gettid(void)
{
	return (int)syscall(SYS_gettid);
}
