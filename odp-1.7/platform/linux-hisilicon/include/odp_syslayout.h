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

#ifndef _ODP_MEMCONFIG_H_
#define _ODP_MEMCONFIG_H_
#include <odp_base.h>
#include <odp_config.h>
#include <odp_tailq.h>
#include <odp_memory.h>
#include <odp_mmdistrict.h>
#include <odp/rwlock.h>
#include <odp_common.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MEM_SIZE_MAX 0x100000000000ull

/**
 * the structure for the memory configuration for the HODP.
 * Used by the odp_config structure. It is separated out, as for multi-process
 * support, the memory details should be shared across instances
 */
struct odp_sys_layout {
	uint64_t mem_cfg_addr;
	uint32_t magic;           /**< Magic number - Sanity check. */

	/* memory topology */
	uint32_t channel_num;     /**< Number of channels (0 if unknown). */
	uint32_t rank_num;        /**< Number of ranks (0 if unknown). */

	uint32_t mm_district_idx; /**< Index of mm_district */
	uint32_t frag_idx;
	uint32_t free_frag_idx;
	uint32_t free_district_idx;

	/**
	 * current lock nest order
	 *  - qlock->mlock (ring/hash/lpm)
	 *  - mplock->qlock->mlock (mempool)
	 * Notice:
	 *  *ALWAYS* obtain qlock first if having to obtain both qlock and mlock
	 */

	/**< only used by mm_district LIB for thread-safe. */
	odp_rwlock_t mlock;
	odp_rwlock_t qlock;   /**< used for tailq operation for thread safe. */
	odp_rwlock_t mplock;  /**< only used by mempool LIB for thread-safe. */
	odp_rwlock_t rw_lock; /**< only used by cpu management. */
	uint64_t     odp_lcore_status[ODP_MAX_CORE >> 6]; /*process of cores.*/

	/* memory segments and zones */
	struct odp_mmfrag mmfrag[ODP_MAX_MMFRAG];

	/**< Memzone descriptors. */
	struct odp_mm_district mm_district[ODP_MAX_MM_DISTRICT];
	struct odp_mm_district free_mm_district[ODP_MAX_MM_DISTRICT];

	/* Runtime Physmem descriptors. */
	struct odp_mmfrag free_mmfrag[ODP_MAX_MMFRAG];

	/**< Tailqs for objects */
	struct odp_tailq_head tailq_head[ODP_MAX_TAILQ];

	/* address of sys_layout in primary process. used to map
	 * shared config into
	 * exact same address the primary process maps it.
	 */
} __attribute__((__packed__));

static inline void odp_mcfg_wait_complete(struct odp_sys_layout *mcfg)
{
	/* wait until shared sys_layout finish initialising */
	while (mcfg->magic != ODP_MAGIC)
		odp_pause();
}

#ifdef __cplusplus
}
#endif
#endif /*__ODP_MEMCONFIG_H_*/
