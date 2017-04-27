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

/**
 * @file
 * Holds the structures for the odp internal configuration
 */

#ifndef ODP_INTERNAL_CFG_H
#define ODP_INTERNAL_CFG_H

#include <odp_common.h>
#include <odp_pci_dev_feature_defs.h>

#define MAX_HUGEPAGE_SIZES 3    /**< support up to 3 kinds pf page sizes */

/*
 * internal configuration structure for the number, size and
 * mount points of hugepages
 */
struct odp_hugepage_type {
	uint64_t    hugepage_sz;                   /**< size of a huge page */
	const char *hugedir;                       /**< dir where hugetlbfs is mounted */
	uint32_t    num_pages[ODP_MAX_NUMA_NODES]; /**< huge page num */

	/**< number of hugepages of that size on each socket */
	int lock_descriptor;                       /**< file descriptor for hugepage dir */
};

/**
 * local configuration
 */
struct odp_local_config {
	size_t		     memory;            /**< amount of asked memory */
	unsigned	     force_channel_num; /**< force number of channels */
	unsigned	     force_rank_num;    /**< force number of ranks */
	unsigned	     no_hugetlbfs;      /**< true to disable hugetlbfs */
	unsigned	     no_pci;            /**< true to disable PCI */
	unsigned	     no_shconf;         /**< true if there is no shared config */
	enum odp_proc_type_t process_type;      /**< multi-process proc type */

	/** true to try allocating memory on specific sockets */
	unsigned force_sockets;

	/**< amount of memory per socket */
	uint64_t socket_mem[ODP_MAX_NUMA_NODES];

	/**< base address to try and reserve memory from */
	uintptr_t base_virtaddr;
	int	  syslog_facility; /**< facility passed to openlog() */

	/** default interrupt mode for VFIO */
	enum odp_intr_mode vfio_intr_mode;

	/**< how many page types on this system */
	unsigned		 num_hugepage_types;
	struct odp_hugepage_type odp_hugepage_type[MAX_HUGEPAGE_SIZES];
};

extern struct odp_local_config local_config; /**< local ODP configuration. */

void odp_init_local_config(struct odp_local_config *local_cfg);
#endif /* ODP_INTERNAL_CFG_H */
