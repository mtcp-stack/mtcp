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

#ifndef _ODP_LCORE_H_
#define _ODP_LCORE_H_

/**
 * @file
 *
 * API for core and socket manipulation
 *
 */

/* #include <odp_per_core.h>
 #include <odp.h> */
#include <odp_common.h>
#include <pthread.h>
#include <odp/cpumask.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ODP_CPU_AFFINITY_STR_LEN 256

#define CORE_ID_ANY UINT32_MAX                    /**< Any core. */

/* ODP_DECLARE_PER_LCORE(unsigned,_socket_id); */
#define PHYS_PKG_FILE "topology/physical_package_id"
#define SYS_CPU_DIR   "/sys/devices/system/cpu/cpu%u"
#define CORE_ID_FILE  "topology/core_id"

typedef int (core_function_t)(void *);

#if defined(__linux__)
typedef	cpu_set_t odp_cpuset_t;

#elif defined(__FreeBSD__)
#include <pthread_np.h>
typedef cpuset_t odp_cpuset_t;
#endif

/**
 * State of an core.
 */
enum odp_core_state_t {
	WAIT,     /**< waiting a new command */
	RUNNING,  /**< executing command */
	FINISHED, /**< command executed */
};

enum odp_core_position_t {
	NOT_ON,  /**< waiting a new command */
	IS_ON,   /**< executing command */
	UNKNOWN, /**< command executed */
};

/**
 * Structure storing internal configuration (per-core)
 */
struct core_config {
	enum odp_core_position_t detected;  /**< true if core was detected */
	pthread_t		 thread_id; /**< pthread identifier */

	/**< communication pipe with master */
	int pipe_master2slave[2];

	/**< communication pipe with master */
	int		      pipe_slave2master[2];
	core_function_t	     *f;          /**< function to call */
	void		     *arg;        /**< argument of function */
	int		      ret;        /**< return value of function */
	enum odp_core_state_t state;      /**< core state */
	unsigned	      socket_id;  /**< physical socket id for this core */
	unsigned	      core_id;    /**< core number on socket for this core */
	int		      core_index; /**< relative index, starting from 0 */
	odp_cpumask_t	      cpuset;     /**< cpu set which the core affinity to */
};

/**
 * Internal configuration (per-core)
 */
extern struct core_config gcore_config[ODP_MAX_CORE];

extern ODP_DECLARE_PER_CORE(unsigned, _core_id); /**< Per thread "core id". */
extern ODP_DECLARE_PER_CORE(cpu_set_t, _cpuset); /**< Per thread "cpuset". */

/**
 * Return the ID of the execution unit we are running on.
 * @return
 *  Logical core ID (in ODP thread) or CORE_ID_ANY (in non-ODP thread)
 */
static inline unsigned odp_core_id(void)
{
	return ODP_PER_CORE(_core_id);
}

/**
 * Get the id of the master core
 *
 * @return
 *   the id of the master core
 */
static inline unsigned odp_get_bsp_core(void)
{
	return odp_get_configuration()->bsp_core;
}

/**
 * Return the number of execution units (cores) on the system.
 *
 * @return
 *   the number of execution units (cores) on the system.
 */
static inline unsigned odp_core_num(void)
{
	const struct odp_config *cfg = odp_get_configuration();

	return cfg->core_num;
}

/**
 * Return the index of the core starting from zero.
 * The order is physical or given by command line (-l option).
 *
 * @param core_id
 *   The targeted core, or -1 for the current one.
 * @return
 *   The relative index, or -1 if not enabled.
 */
static inline int odp_core_index(int core_id)
{
	if (core_id >= ODP_MAX_CORE)
		return -1;

	if (core_id < 0)
		core_id = odp_core_id();

	return gcore_config[core_id].core_index;
}

/**
 * Return the ID of the physical socket of the logical core we are
 * running on.
 * @return
 *   the ID of current coreid's physical socket
 */
unsigned odp_socket_id(void);

int odp_cpu_init(void);

/**
 * Get the ID of the physical socket of the specified core
 *
 * @param core_id
 *   the targeted core, which MUST be between 0 and ODP_MAX_CORE-1.
 * @return
 *   the ID of coreid's physical socket
 */
static inline unsigned odp_core_to_socket_id(unsigned core_id)
{
	return gcore_config[core_id].socket_id;
}

/**
 * Test if an core is enabled.
 *
 * @param core_id
 *   The identifier of the core, which MUST be between 0 and
 *   ODP_MAX_CORE-1.
 * @return
 *   True if the given core is enabled; false otherwise.
 */
static inline int odp_core_is_enabled(unsigned core_id)
{
	struct odp_config *cfg = odp_get_configuration();

	if (core_id >= ODP_MAX_CORE)
		return 0;

	return (cfg->core_role[core_id] != ROLE_OFF);
}

/**
 * Get the next enabled core ID.
 *
 * @param i
 *   The current core (reference).
 * @param skip_master
 *   If true, do not return the ID of the master core.
 * @param wrap
 *   If true, go back to 0 when ODP_MAX_CORE is reached; otherwise,
 *   return ODP_MAX_CORE.
 * @return
 *   The next core_id or ODP_MAX_CORE if not found.
 */
static inline unsigned odp_get_next_core(unsigned i, int skip_master, int wrap)
{
	i++;
	if (wrap)
		i %= ODP_MAX_CORE;

	while (i < ODP_MAX_CORE) {
		if (!odp_core_is_enabled(i) ||
		    (skip_master && (i == odp_get_bsp_core()))) {
			i++;
			if (wrap)
				i %= ODP_MAX_CORE;

			continue;
		}

		break;
	}

	return i;
}

/**
 * Macro to browse all running cores.
 */
#define ODP_LCORE_FOREACH(i)                       \
	for (i = odp_get_next_core(-1, 0, 0);             \
	     i < ODP_MAX_CORE;                       \
	     i = odp_get_next_core(i, 0, 0))

/**
 * Macro to browse all running cores except the master core.
 */
#define ODP_LCORE_FOREACH_SLAVE(i)                 \
	for (i = odp_get_next_core(-1, 1, 0);             \
	     i < ODP_MAX_CORE;                       \
	     i = odp_get_next_core(i, 1, 0))

/**
 * Set core affinity of the current thread.
 * Support both ODP and non-ODP thread and update TLS.
 *
 * @param cpusetp
 *   Point to cpu_set_t for setting current thread affinity.
 * @return
 *   On success, return 0; otherwise return -1;
 */
int odp_thread_set_affinity(odp_cpumask_t *cpusetp);

/**
 * Get core affinity of the current thread.
 *
 * @param cpusetp
 *   Point to cpu_set_t for getting current thread cpu affinity.
 *   It presumes input is not NULL, otherwise it causes panic.
 *
 */
void odp_thread_get_affinity(odp_cpumask_t *cpusetp);

int odp_bind_proc_to_lcore(int lcore_id);
int odp_unbind_proc_to_lcore(int lcore_id);
int odp_get_available_cpu(void);
int odp_check_cpu_status(int cpu_id);
#ifdef __cplusplus
}
#endif
#endif /* _ODP_LCORE_H_ */
