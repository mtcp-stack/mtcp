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

#ifndef _ODP_H_
#define _ODP_H_

/**
 * @file
 *
 * ODP Configuration API
 */

#include <stdint.h>
#include <sched.h>

#include <odp/rwlock.h>

#ifdef __cplusplus
extern "C" {
#endif

/**< Magic number written by the main partition when ready. */
#define ODP_MAGIC	   19820526
#define ODP_PATH_MAX	   256
#define ODP_LIB_DESC_LEN   128
#define ODP_BUFF_SIZE	   ODP_PATH_MAX
#define ODP_MAX_CORE	   128 /* max number of cores in one cpu chipset*/
#define ODP_MAX_NUMA_NODES 8   /* max number of numa nodes in one board*/

/**
 * Macro to define a per core variable "var" of type "type", don't
 * use keywords like "static" or "volatile" in type, just prefix the
 * whole macro.
 */
#define ODP_DEFINE_PER_CORE(type, name) \
	__thread __typeof__(type)per_core_ ## name

/**
 * Macro to declare an extern per core variable "var" of type "type"
 */
#define ODP_DECLARE_PER_CORE(type, name) \
	__thread __typeof__(type)per_core_ ## name

/**
 * Read/write the per-core variable value
 */
#define ODP_PER_CORE(name) per_core_ ## name

/**
 * The core role (used in ODP or not).
 */
enum odp_core_role_t {
	ROLE_ODP,
	ROLE_OFF,
};

/**
 * The type of process in a linuxapp, multi-process setup
 */
enum odp_proc_type_t {
	ODP_PROC_AUTO = -1,    /* allow auto-detection of primary/secondary */
	ODP_PROC_PRIMARY = 0,  /* set to zero, so primary is the default */
	ODP_PROC_SECONDARY,

	ODP_PROC_INVALID
};

/**
 * The global ODP configuration structure.
 */
struct odp_config {
	uint32_t	     bsp_core;                /**< Id of the bsp core */
	uint32_t	     core_num;                /**< Number of available cores. */
	enum odp_core_role_t core_role[ODP_MAX_CORE]; /**< State of cores. */

	/** Primary or secondary configuration */
	enum odp_proc_type_t process_type;

	void *global_data;

	/**
	 * Pointer to memory configuration, which may be shared across multiple
	 * Huawei ODP instances
	 */
	struct odp_sys_layout *sys_layout;
} __attribute__((__packed__));

/**
 * Get the global configuration structure.
 *
 * @return
 *   A pointer to the global configuration structure.
 */
struct odp_config *odp_get_configuration(void);

/**
 * Get a core's role.
 *
 * @param core_id
 *   The identifier of the core.
 * @return
 *   The role of the core.
 */
enum odp_core_role_t odp_core_role(unsigned core_id);

/**
 * Get the process type in a multi-process setup
 *
 * @return
 *   The process type
 */
enum odp_proc_type_t odp_process_type(void);

/**
 * Request iopl privilege for all RPL.
 *
 * This function should be called by umds which need access to ioports.

 * @return
 *   - On success, returns 0.
 *   - On failure, returns -1.
 */
int odp_iopl_init(void);

/**
 * Initialize the Environment Abstraction Layer (ODP).
 *
 * This function is to be executed on the MASTER core only, as soon
 * as possible in the application's main() function.
 *
 * The function finishes the initialization process before main() is called.
 * It puts the SLAVE cores in the WAIT state.
 *
 * When the multi-partition feature is supported, depending on the
 * configuration (if CONFIG_ODP_MAIN_PARTITION is disabled), this
 * function waits to ensure that the magic number is set before
 * returning. See also the odp_get_configuration() function. Note:
 * This behavior may change in the future.
 *
 * @param argc
 *   The argc argument that was given to the main() function.
 * @param argv
 *   The argv argument that was given to the main() function.
 * @return
 *   - On success, the number of parsed arguments, which is greater or
 *     equal to zero. After the call to odp_init(),
 *     all arguments argv[x] with x < ret may be modified and should
 *     not be accessed by the application.
 *   - On failure, a negative error value.
 */
int odp_arch_pv660_init(int argc, char **argv);

/**
 * Usage function typedef used by the application usage function.
 *
 * Use this function typedef to define and call odp_set_applcation_usage_hook()
 * routine.
 */
typedef void (*odp_usage_hook_t)(const char *prgname);

/**
 * Add application usage routine callout from the odp_usage() routine.
 *
 * This function allows the application to include its usage message
 * in the ODP system usage message. The routine odp_set_application_usage_hook()
 * needs to be called before the odp_init() routine in the application.
 *
 * This routine is optional for the application and will behave as if the set
 * routine was never called as the default behavior.
 *
 * @param func
 *   The func argument is a function pointer to the application usage routine.
 *   Called function is defined using odp_usage_hook_t typedef, which is of
 *   the form void odp_usage_func(const char * prgname).
 *
 *   Calling this routine with a NULL value will reset the usage
 *   hook routine and
 *   return the current value, which could be NULL.
 * @return
 *   - Returns the current value of the odp_application_usage pointer to allow
 *     the caller to daisy chain the usage routines if needing more then one.
 */
odp_usage_hook_t
odp_set_application_usage_hook(odp_usage_hook_t usage_func);

/**
 * macro to get the lock of tailq in sys_layout
 */
#define ODP_TAILQ_RWLOCK (&(odp_get_configuration()->sys_layout->qlock))

/**
 * macro to get the multiple lock of mempool shared by multiple-instance
 */
#define ODP_MEMPOOL_RWLOCK (&(odp_get_configuration()->sys_layout->mplock))

/**
 * Whether ODP is using huge pages (disabled by --no-huge option).
 * The no-huge mode cannot be used with UIO poll-mode drivers like igb/ixgbe.
 * It is useful for NIC drivers () or
 * crypto drivers (e.g. libodp_crypto_nitrox) provided by third-parties such
 * as 6WIND.
 *
 * @return
 *   Nonzero if hugepages are enabled.
 */
int odp_has_hugepages(void);

/**
 * A wrap API for syscall gettid.
 *
 * @return
 *   On success, returns the thread ID of calling process.
 *   It is always successful.
 */
int odp_sys_gettid(void);

/**
 * Get system unique thread id.
 *
 * @return
 *   On success, returns the thread ID of calling process.
 *   It is always successful.
 */
static inline int odp_gettid(void)
{
	static ODP_DEFINE_PER_CORE(int, _thread_id) = -1;

	if (ODP_PER_CORE(_thread_id) == -1)
		ODP_PER_CORE(_thread_id) = odp_sys_gettid();

	return ODP_PER_CORE(_thread_id);
}

extern ODP_DECLARE_PER_CORE(int, _odp_errno); /**< Per core error number. */

/**
 * Error number value, stored per-thread, which can be queried after
 * calls to certain functions to determine why those functions failed.
 *
 * Uses standard values from errno.h wherever possible, with a small number
 * of additional possible values for ODP-specific conditions.
 */
#define odp_err ODP_PER_CORE(_odp_errno)

unsigned long odp_parse_sysfs_value(const char *filename);

enum odp_proc_type_t odp_get_process_type(void);

enum odp_proc_type_t odp_proc_type_detect(void);

#ifdef __cplusplus
}
#endif
#endif /* _ODP_H_ */
