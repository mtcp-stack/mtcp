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

#ifndef _ODP_MEMZONE_H_
#define _ODP_MEMZONE_H_

/**
 * @file
 * ODP Memzone
 *
 * The goal of the mm_district allocator is to reserve contiguous
 * portions of physical memory. These zones are identified by a name.
 *
 * The mm_district descriptors are shared by all partitions and are
 * located in a known place of physical memory. This zone is accessed
 * using odp_get_configuration(). The lookup (by name) of a
 * memory zone can be done in any partition and returns the same
 * physical address.
 *
 * A reserved memory zone cannot be unreserved. The reservation shall
 * be done at initialization time only.
 */

#include <stdio.h>
#include "odp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ODP_MEMZONE_2MB		   0x00000001 /**< Use 2MB pages. */
#define ODP_MEMZONE_1GB		   0x00000002 /**< Use 1GB pages. */
#define ODP_MEMZONE_16MB	   0x00000100 /**< Use 16MB pages. */
#define ODP_MEMZONE_16GB	   0x00000200 /**< Use 16GB pages. */
#define ODP_MEMZONE_SIZE_HINT_ONLY 0x00000004 /**< Use available page size */
#define ODP_MEMZONE_NAMESIZE	   32

/**
 * A structure describing a mm_district, which is a contiguous portion of
 * physical memory identified by a name.
 */
struct odp_mm_district {
#define ODP_MEMZONE_NAMESIZE 32  /**< Maximum length of memory district name.*/
	/**< Name of the memory district. */
	char	    name[ODP_MEMZONE_NAMESIZE];
	/**< original name of the memory district. */
	char	    orig_name[ODP_MEMZONE_NAMESIZE - 8];
	/**< Start physical address. */
	phys_addr_t phys_addr;
	phys_addr_t phys_addr_end;
	void	   *excursion_addr;

	union {
		void	*addr;    /**< Start virtual address. */
		uint64_t addr_64; /**< Makes sure addr is always 64-bits */
	};

	size_t len;               /**< Length of the mm_district. */

	uint64_t hugepage_sz;     /**< The page size of underlying memory */

	int32_t socket_id;        /**< NUMA socket ID. */

	uint32_t flags;           /**< Characteristics of this mm_district. */
	uint32_t mmfrag_id;       /** <store the mm_district is from which mmfrag. */
} __attribute__((__packed__));

/**
 * Reserve a portion of physical memory.
 *
 * This function reserves some memory and returns a pointer to a
 * correctly filled mm_district descriptor. If the allocation cannot be
 * done, return NULL. Note: A reserved zone cannot be freed.
 *
 * @param name
 *   The name of the mm_district. If it already exists, the function will
 *   fail and return NULL.
 * @param len
 *   The size of the memory to be reserved. If it
 *   is 0, the biggest contiguous zone will be reserved.
 * @param socket_id
 *   The socket identifier in the case of
 *   NUMA. The value can be SOCKET_ID_ANY if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The flags parameter is used to request mm_districts to be
 *   taken from 1GB or 2MB hugepages.
 *   - ODP_MEMZONE_2MB - Reserve from 2MB pages
 *   - ODP_MEMZONE_1GB - Reserve from 1GB pages
 *   - ODP_MEMZONE_16MB - Reserve from 16MB pages
 *   - ODP_MEMZONE_16GB - Reserve from 16GB pages
 *   - ODP_MEMZONE_SIZE_HINT_ONLY - Allow alternative page size to be used if
 *                                  the requested page size is unavailable.
 *                                  If this flag is not set, the function
 *                                  will return error on an unavailable size
 *                                  request.
 * @return
 *   A pointer to a correctly-filled read-only mm_district descriptor, or NULL
 *   on error.
 *   On error case, odp_err will be set appropriately:
 *    - E_HODP_NO_CONFIG - function could not get pointer to odp_config
 *       structure
 *    - E_HODP_SECONDARY - function was called from a secondary process instance
 *    - ENOSPC - the maximum number of mm_districts has already been allocated
 *    - EEXIST - a mm_district with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create
 *       mm_district
 *    - EINVAL - invalid parameters
 */
const struct odp_mm_district
*odp_mm_district_reserve(const char *name,
			 const char *orig_name,
			 size_t len, int socket_id,
			 unsigned flags);
const uint32_t odp_mm_district_unreserve(const char *name);

/**
 * Reserve a portion of physical memory with alignment on a specified
 * boundary.
 *
 * This function reserves some memory with alignment on a specified
 * boundary, and returns a pointer to a correctly filled mm_district
 * descriptor. If the allocation cannot be done or if the alignment
 * is not a power of 2, returns NULL.
 * Note: A reserved zone cannot be freed.
 *
 * @param name
 *   The name of the mm_district. If it already exists, the function will
 *   fail and return NULL.
 * @param len
 *   The size of the memory to be reserved. If it
 *   is 0, the biggest contiguous zone will be reserved.
 * @param socket_id
 *   The socket identifier in the case of
 *   NUMA. The value can be SOCKET_ID_ANY if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The flags parameter is used to request mm_districts to be
 *   taken from 1GB or 2MB hugepages.
 *   - ODP_MEMZONE_2MB - Reserve from 2MB pages
 *   - ODP_MEMZONE_1GB - Reserve from 1GB pages
 *   - ODP_MEMZONE_16MB - Reserve from 16MB pages
 *   - ODP_MEMZONE_16GB - Reserve from 16GB pages
 *   - ODP_MEMZONE_SIZE_HINT_ONLY - Allow alternative page size to be used if
 *                                  the requested page size is unavailable.
 *                                  If this flag is not set, the function
 *                                  will return error on an unavailable size
 *                                  request.
 * @param align
 *   Alignment for resulting mm_district. Must be a power of 2.
 * @return
 *   A pointer to a correctly-filled read-only mm_district descriptor, or NULL
 *   on error.
 *   On error case, odp_err will be set appropriately:
 *    - E_HODP_NO_CONFIG - function could not get pointer to odp_config
 *       structure
 *    - E_HODP_SECONDARY - function was called from a secondary process
 *       instance
 *    - ENOSPC - the maximum number of mm_districts has already been allocated
 *    - EEXIST - a mm_district with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create
 *       mm_district
 *    - EINVAL - invalid parameters
 */
const struct odp_mm_district
*odp_mm_district_reserve_aligned(const char *name,
				 const char *orig_name,
				 size_t len, int socket_id,
				 unsigned flags,
				 unsigned align);

/**
 * Reserve a portion of physical memory with specified alignment and
 * boundary.
 *
 * This function reserves some memory with specified alignment and
 * boundary, and returns a pointer to a correctly filled mm_district
 * descriptor. If the allocation cannot be done or if the alignment
 * or boundary are not a power of 2, returns NULL.
 * Memory buffer is reserved in a way, that it wouldn't cross specified
 * boundary. That implies that requested length should be less or equal
 * then boundary.
 * Note: A reserved zone cannot be freed.
 *
 * @param name
 *   The name of the mm_district. If it already exists, the function will
 *   fail and return NULL.
 * @param len
 *   The size of the memory to be reserved. If it
 *   is 0, the biggest contiguous zone will be reserved.
 * @param socket_id
 *   The socket identifier in the case of
 *   NUMA. The value can be SOCKET_ID_ANY if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   The flags parameter is used to request mm_districts to be
 *   taken from 1GB or 2MB hugepages.
 *   - ODP_MEMZONE_2MB - Reserve from 2MB pages
 *   - ODP_MEMZONE_1GB - Reserve from 1GB pages
 *   - ODP_MEMZONE_16MB - Reserve from 16MB pages
 *   - ODP_MEMZONE_16GB - Reserve from 16GB pages
 *   - ODP_MEMZONE_SIZE_HINT_ONLY - Allow alternative page size to be used if
 *                                  the requested page size is unavailable.
 *                                  If this flag is not set, the function
 *                                  will return error on an unavailable size
 *                                  request.
 * @param align
 *   Alignment for resulting mm_district. Must be a power of 2.
 * @param bound
 *   Boundary for resulting mm_district. Must be a power of 2 or zero.
 *   Zero value implies no boundary condition.
 * @return
 *   A pointer to a correctly-filled read-only mm_district descriptor, or NULL
 *   on error.
 *   On error case, odp_err will be set appropriately:
 *    - E_HODP_NO_CONFIG - function could not get pointer to odp_config
 *       structure
 *    - E_HODP_SECONDARY - function was called from a secondary process
 *       instance
 *    - ENOSPC - the maximum number of mm_districts has already been allocated
 *    - EEXIST - a mm_district with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create
 *       mm_district
 *    - EINVAL - invalid parameters
 */
const struct odp_mm_district
*odp_mm_district_reserve_bounded(const char *name,
				 const char *orig_name,
				 size_t len, int socket_id,
				 unsigned flags, unsigned align,
				 unsigned bound);

/**
 * Lookup for a mm_district.
 *
 * Get a pointer to a descriptor of an already reserved memory
 * zone identified by the name given as an argument.
 *
 * @param name
 *   The name of the mm_district.
 * @return
 *   A pointer to a read-only mm_district descriptor.
 */
const struct odp_mm_district *odp_mm_district_lookup(const char *name);

/**
 * Dump all reserved mm_districts to the console.
 *
 * @param f
 *   A pointer to a file for output
 */
void odp_mm_district_dump(FILE *);

/**
 * Walk list of all mm_districts
 *
 * @param func
 *   Iterator function
 * @param arg
 *   Argument passed to iterator
 */
void
odp_mm_district_walk(void (*func)(const struct odp_mm_district *, void *arg),
		     void *arg);

unsigned long long odp_v2p(const void *virtaddr);
inline void *odp_p2v(const unsigned long long phyaddr);

#ifdef __cplusplus
}
#endif
#endif /* _ODP_MEMZONE_H_ */
