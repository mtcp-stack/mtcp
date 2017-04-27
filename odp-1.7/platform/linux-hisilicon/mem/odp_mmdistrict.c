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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include <odp/atomic.h>
#include <odp_memory.h>
#include <odp_mmdistrict.h>
#include <odp_base.h>
#include <odp_syslayout.h>
#include <odp/config.h>

#include <odp_common.h>

#include "odp_private.h"
#include "odp_debug_internal.h"

/* internal copy of free memory segments */
static struct odp_mmfrag *free_mmfrag;

static inline const struct odp_mm_district *mm_district_lookup(const char *name)
{
	const struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * zones and this function should be called at init only
	 */
	for (i = 0; i < ODP_MAX_MM_DISTRICT && mcfg->mm_district[i].addr; i++)
		if (!strncmp(name, mcfg->mm_district[i].name,
			     ODP_MEMZONE_NAMESIZE))
			return &mcfg->mm_district[i];

	return NULL;
}

static inline struct odp_mm_district *free_mm_district_lookup(
	const char *orig_name)
{
	struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * districts and this function should be called at init only
	 */
	for (i = 0; i < ODP_MAX_MM_DISTRICT && mcfg->free_mm_district[i].addr;
	     i++)
		if (!strncmp(orig_name, mcfg->free_mm_district[i].orig_name,
			     ODP_MEMZONE_NAMESIZE - 8))
			return &mcfg->free_mm_district[i];

	return NULL;
}

static inline void free_mm_district_fetch(struct odp_mm_district *md)
{
	struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;
	mcfg->mm_district_idx++;
	memcpy(&mcfg->mm_district[mcfg->mm_district_idx], md,
	       sizeof(struct odp_mm_district));

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * districts and this function should be called at init only
	 */
	for (i = 0; i < mcfg->free_district_idx && md->addr; i++) {
		memcpy(md, md + 1, sizeof(struct odp_mm_district));
		md++;
	}

	memset(md, 0, sizeof(*md));
	md->socket_id = SOCKET_ID_ANY;
	mcfg->free_district_idx -= 1;
}

static inline const struct odp_mm_district *get_mm_district_by_vaddress(
	const void *addr)
{
	const struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	/*
	 * the algorithm is not optimal (linear), but there are few
	 * zones and this function should be called at init only
	 */
	for (i = 0; i < ODP_MAX_MM_DISTRICT && mcfg->mm_district[i].addr; i++)
		if ((((unsigned long)addr) >=
		     (unsigned long)(mcfg->mm_district[i].addr)) &&
		    (((unsigned long)addr) <
		     (((unsigned long)(mcfg->mm_district[i].addr)) +
		      mcfg->mm_district[i].len)))
			return &mcfg->mm_district[i];

	return NULL;
}

unsigned long long odp_v2p(const void *virtaddr)
{
	const struct odp_mm_district *mm_district;

	mm_district = get_mm_district_by_vaddress(virtaddr);
	if (mm_district)
		return (mm_district->phys_addr) + ((unsigned long)virtaddr -
						   (unsigned long)(mm_district->
								   addr));

	return -1;
}

static inline const struct odp_mm_district *get_mm_district_by_phyddress(
	const unsigned long long phyaddr)
{
	static unsigned int i;
	const struct odp_sys_layout *mcfg =
		odp_get_configuration()->sys_layout;

	if ((phyaddr >= mcfg->mm_district[i].phys_addr) &&
	    (phyaddr < mcfg->mm_district[i].phys_addr_end))
		return &mcfg->mm_district[i];

	for (i = 0; i < ODP_MAX_MM_DISTRICT && mcfg->mm_district[i].addr; i++)
		if ((phyaddr >= mcfg->mm_district[i].phys_addr) &&
		    (phyaddr < mcfg->mm_district[i].phys_addr_end))
			return &mcfg->mm_district[i];

	return NULL;
}

inline void *odp_p2v(const unsigned long long phyaddr)
{
	static unsigned int i;
	const struct odp_sys_layout *mcfg =
		odp_get_configuration()->sys_layout;

	if ((phyaddr >= mcfg->mm_district[i].phys_addr) &&
	    (phyaddr < mcfg->mm_district[i].phys_addr_end))
#ifdef __arm32__
		return mcfg->mm_district[i].addr +
		       (phyaddr - mcfg->mm_district[i].phys_addr);

#else
		return mcfg->mm_district[i].excursion_addr + phyaddr;
#endif

	for (i = 0; i < ODP_MAX_MM_DISTRICT && mcfg->mm_district[i].addr; i++)
		if ((phyaddr >= mcfg->mm_district[i].phys_addr) &&
		    (phyaddr < mcfg->mm_district[i].phys_addr_end))
#ifdef __arm32__
			return mcfg->mm_district[i].addr +
			       (phyaddr - mcfg->mm_district[i].phys_addr);

#else
			return mcfg->mm_district[i].excursion_addr + phyaddr;
#endif

	return NULL;
}

/*
 * Return a pointer to a correctly filled mm_district descriptor. If the
 * allocation cannot be done, return NULL.
 */
const struct odp_mm_district *odp_mm_district_reserve(const char *name,
						      const char *orig_name,
						      size_t len, int socket_id,
						      unsigned flags)
{
	return odp_mm_district_reserve_aligned(name, orig_name, len,
					       socket_id, flags,
					       ODP_CACHE_LINE_SIZE);
}

/*
 * Helper function for mm_district_reserve_aligned().
 * Calculate address offset from the start of the segment.
 * Align offset in that way that it satisfy istart alignmnet and
 * buffer of the  requested length would not cross specified boundary.
 */
static inline phys_addr_t align_phys_boundary(const struct odp_mmfrag *ms,
					      size_t len, size_t align,
					      size_t bound)
{
	phys_addr_t addr_offset, bmask, end, start;
	size_t step;

	step  = ODP_MAX(align, bound);
	bmask = ~((phys_addr_t)bound - 1);

	/* calculate offset to closest alignment */
	start = ODP_ALIGN_CEIL(ms->phys_addr, align);
	addr_offset = start - ms->phys_addr;

	while (addr_offset + len < ms->len) {
		/* check, do we meet boundary condition */
		end = start + len - (len != 0);
		if ((start & bmask) == (end & bmask))
			break;

		/* calculate next offset */
		start = ODP_ALIGN_CEIL(start + 1, step);
		addr_offset = start - ms->phys_addr;
	}

	return addr_offset;
}

static const struct odp_mm_district *mm_district_reserve_aligned(
	const char *name, const char *orig_name,
	size_t len,
	int socket_id, unsigned flags,
	unsigned align,
	unsigned bound)
{
	struct odp_sys_layout *mcfg;
	unsigned i = 0;
	int mmfrag_idx = -1;
	uint64_t addr_offset, seg_offset = 0;
	size_t	 requested_len;
	size_t	 mmfrag_len = 0;
	phys_addr_t mmfrag_physaddr;
	void *mmfrag_addr;
	struct odp_mm_district *md = NULL;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	/* no more room in config */
	if (mcfg->mm_district_idx >= ODP_MAX_MM_DISTRICT) {
		ODP_ERR("%s: No more room in config\n", name);
		odp_err = ENOSPC;
		return NULL;
	}

	/* zone already exist */
	if (mm_district_lookup(name)) {
		ODP_ERR("mm_district <%s> already exists\n", name);
		odp_err = EEXIST;
		return NULL;
	}

	if (!orig_name) {
		ODP_ERR("Invalid param: orig_name\n");
		odp_err = EINVAL;
		return NULL;
	}

	md = free_mm_district_lookup(orig_name);
	if (md)
		if (len <= md->len) {
			free_mm_district_fetch(md);
			return md;
		}

	/* if alignment is not a power of two */
	if (align && !odp_is_power_of_2(align)) {
		ODP_ERR("Invalid alignment: %u\n", align);
		odp_err = EINVAL;
		return NULL;
	}

	/* alignment less than cache size is not allowed */
	if (align < ODP_CACHE_LINE_SIZE)
		align = ODP_CACHE_LINE_SIZE;

	/* align length on cache boundary. Check for overflow before doing so */
	if (len > MEM_SIZE_MAX - ODP_CACHE_LINE_MASK) {
		odp_err = EINVAL; /* requested size too big */
		return NULL;
	}

	len += ODP_CACHE_LINE_MASK;
	len &= ~((size_t)ODP_CACHE_LINE_MASK);

	/* save minimal requested  length */
	requested_len = ODP_MAX((size_t)ODP_CACHE_LINE_SIZE, len);

	/* check that boundary condition is valid */
	if ((bound != 0) && ((requested_len > bound) ||
			     !odp_is_power_of_2(bound))) {
		odp_err = EINVAL;
		return NULL;
	}

	/* find the smallest segment matching requirements */
	for (i = 0; i < ODP_MAX_MMFRAG; i++) {
		/* last segment */
		if (!free_mmfrag[i].addr)
			break;

		/* empty segment, skip it */
		if (free_mmfrag[i].len == 0)
			continue;

		/* bad socket ID */
		if ((socket_id != SOCKET_ID_ANY) &&
		    (free_mmfrag[i].socket_id != SOCKET_ID_ANY) &&
		    (socket_id != free_mmfrag[i].socket_id))
			continue;

		/*
		 * calculate offset to closest alignment that
		 * meets boundary conditions.
		 */
		addr_offset = align_phys_boundary(free_mmfrag + i,
						  requested_len, align, bound);

		/* check len */
		if ((requested_len + addr_offset) > free_mmfrag[i].len)
			continue;

		/* check flags for hugepage sizes */
		if ((flags & ODP_MEMZONE_2MB) &&
		    (free_mmfrag[i].hugepage_sz == ODP_PGSIZE_1G))
			continue;

		if ((flags & ODP_MEMZONE_1GB) &&
		    (free_mmfrag[i].hugepage_sz == ODP_PGSIZE_2M))
			continue;

		if ((flags & ODP_MEMZONE_16MB) &&
		    (free_mmfrag[i].hugepage_sz == ODP_PGSIZE_16G))
			continue;

		if ((flags & ODP_MEMZONE_16GB) &&
		    (free_mmfrag[i].hugepage_sz == ODP_PGSIZE_16M))
			continue;

		/* this segment is the best until now */
		if (mmfrag_idx == -1) {
			mmfrag_idx = i;
			mmfrag_len = free_mmfrag[i].len;
			seg_offset = addr_offset;
		}

		/* find the biggest contiguous zone */
		else if (len == 0) {
			if (free_mmfrag[i].len > mmfrag_len) {
				mmfrag_idx = i;
				mmfrag_len = free_mmfrag[i].len;
				seg_offset = addr_offset;
			}
		}

		/*
		 * find the smallest (we already checked that current
		 * zone length is > len
		 */
		else if ((free_mmfrag[i].len + align < mmfrag_len) ||
			 ((free_mmfrag[i].len <= mmfrag_len + align) &&
			  (addr_offset < seg_offset))) {
			mmfrag_idx = i;
			mmfrag_len = free_mmfrag[i].len;
			seg_offset = addr_offset;
		}
	}

	/* no segment found */
	if (mmfrag_idx == -1) {
		/*
		 * If ODP_MEMZONE_SIZE_HINT_ONLY flag is specified,
		 * try allocating again without the size parameter
		 * otherwise -fail.
		 */
		if ((flags & ODP_MEMZONE_SIZE_HINT_ONLY) &&
		    ((flags & ODP_MEMZONE_1GB) ||
		     (flags & ODP_MEMZONE_2MB) ||
		     (flags & ODP_MEMZONE_16MB) ||
		     (flags & ODP_MEMZONE_16GB)))
			return mm_district_reserve_aligned(name, orig_name,
							   len, socket_id, 0,
							   align, bound);

		odp_err = ENOMEM;
		return NULL;
	}

	/* save aligned physical and virtual addresses */
	mmfrag_physaddr = free_mmfrag[mmfrag_idx].phys_addr + seg_offset;
	mmfrag_addr = ODP_PTR_ADD(free_mmfrag[mmfrag_idx].addr,
				  (uintptr_t)seg_offset);

	/* if we are looking for a biggest mm_district */
	if (len == 0) {
		if (bound == 0)
			requested_len = mmfrag_len - seg_offset;
		else
			requested_len =
				ODP_ALIGN_CEIL(mmfrag_physaddr + 1, bound)
				- mmfrag_physaddr;
	}

	/* set length to correct value */
	len = (size_t)seg_offset + requested_len;

	/* update our internal state */
	free_mmfrag[mmfrag_idx].len -= len;
	free_mmfrag[mmfrag_idx].phys_addr += len;
	free_mmfrag[mmfrag_idx].addr =
		(char *)free_mmfrag[mmfrag_idx].addr + len;

	/* fill the zone in config */
	struct odp_mm_district *mz =
		&mcfg->mm_district[mcfg->mm_district_idx++];

	snprintf(mz->orig_name, sizeof(mz->orig_name), "%s", orig_name);
	snprintf(mz->name, sizeof(mz->name), "%s", name);
	mz->phys_addr = mmfrag_physaddr;
	mz->phys_addr_end  = mmfrag_physaddr + requested_len;
	mz->excursion_addr = mmfrag_addr - mmfrag_physaddr;
	mz->addr = mmfrag_addr;
	mz->len	 = requested_len;
	mz->hugepage_sz = free_mmfrag[mmfrag_idx].hugepage_sz;
	mz->socket_id = free_mmfrag[mmfrag_idx].socket_id;
	mz->flags = 0;
	mz->mmfrag_id = mmfrag_idx;

	return mz;
}

static const uint32_t mm_district_unreserve(const char *name)
{
	struct odp_sys_layout *mcfg = odp_get_configuration()->sys_layout;
	uint16_t index;
	const struct odp_mm_district *md = NULL;
	int i = 0;
	uint32_t md_id = 0;

	md = mm_district_lookup(name);
	if (!md) {
		ODP_ERR("%s mm district is not exist!!\r\n", name);
		return -1;
	}

	md_id = md->mmfrag_id;

	if ((free_mmfrag[md_id].addr + free_mmfrag[md_id].len) ==
	    md->addr) {
		free_mmfrag[md_id].len += md->len;
	} else if ((md->addr + md->len) == free_mmfrag[md_id].addr) {
		free_mmfrag[md_id].addr = md->addr;
		free_mmfrag[md_id].len += md->len;
	} else {
		if (mcfg->free_district_idx < ODP_MAX_MM_DISTRICT) {
			memcpy(&mcfg->free_mm_district[mcfg->free_district_idx],
			       md, sizeof(struct odp_mm_district));
			mcfg->free_district_idx++;
		} else {
			ODP_ERR("mm_district_unreserve fail!!\r\n");
			return -1;
		}
	}

	for (index = 0; index < ODP_MAX_MM_DISTRICT &&
	     mcfg->mm_district[index].addr; index++)
		if (!strncmp(name, mcfg->mm_district[index].name,
			     ODP_MEMZONE_NAMESIZE)) {
			/* replace the zone in config */
			struct odp_mm_district *md_t =
				&mcfg->mm_district[index];

			for (i = index; i < mcfg->mm_district_idx; i++) {
				memcpy(md_t, md_t + 1,
				       sizeof(struct odp_mm_district));
				md_t++;
			}

			memset(md_t, 0, sizeof(*md_t));
			md_t->socket_id = SOCKET_ID_ANY;
			mcfg->mm_district_idx -= 1;
			return 0;
		}

	return -1;
}

/*
 * Return a pointer to a correctly filled mm_district descriptor (with a
 * specified alignment). If the allocation cannot be done, return NULL.
 */
const struct odp_mm_district
*odp_mm_district_reserve_aligned(const char *name,
				 const char *orig_name, size_t len,
				 int socket_id, unsigned flags,
				 unsigned align)
{
	struct odp_sys_layout *mcfg;
	const struct odp_mm_district *mz = NULL;

	/* both sizes cannot be explicitly called for */
	if (((flags & ODP_MEMZONE_1GB) && (flags & ODP_MEMZONE_2MB)) ||
	    ((flags & ODP_MEMZONE_16MB) && (flags & ODP_MEMZONE_16GB))) {
		odp_err = EINVAL;
		return NULL;
	}

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_write_lock(&mcfg->mlock);

	mz = mm_district_reserve_aligned(name, orig_name,
					 len, socket_id, flags, align, 0);

	odp_rwlock_write_unlock(&mcfg->mlock);

	return mz;
}

/*
 * Return a pointer to a correctly filled mm_district descriptor (with a
 * specified alignment and boundary).
 * If the allocation cannot be done, return NULL.
 */
const struct odp_mm_district
*odp_mm_district_reserve_bounded(const char *name,
				 const char *orig_name,
				 size_t len, int socket_id, unsigned flags,
				 unsigned align,
				 unsigned bound)
{
	struct odp_sys_layout *mcfg;
	const struct odp_mm_district *mz = NULL;

	/* both sizes cannot be explicitly called for */
	if (((flags & ODP_MEMZONE_1GB) && (flags & ODP_MEMZONE_2MB)) ||
	    ((flags & ODP_MEMZONE_16MB) && (flags & ODP_MEMZONE_16GB))) {
		odp_err = EINVAL;
		return NULL;
	}

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_write_lock(&mcfg->mlock);

	mz = mm_district_reserve_aligned(
		name, orig_name, len, socket_id, flags, align, bound);

	odp_rwlock_write_unlock(&mcfg->mlock);

	return mz;
}

const uint32_t odp_mm_district_unreserve(const char *name)
{
	struct odp_sys_layout *mcfg;
	uint32_t ret;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_write_lock(&mcfg->mlock);

	ret = mm_district_unreserve(name);

	odp_rwlock_write_unlock(&mcfg->mlock);

	return ret;
}

/*
 * Lookup for the mm_district identified by the given name
 */
const struct odp_mm_district *odp_mm_district_lookup(const char *name)
{
	struct odp_sys_layout *mcfg;
	const struct odp_mm_district *mm_district = NULL;

	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_read_lock(&mcfg->mlock);

	mm_district = mm_district_lookup(name);

	odp_rwlock_read_unlock(&mcfg->mlock);

	return mm_district;
}

/* Dump all reserved memory zones on console */
void odp_mm_district_dump(FILE *f)
{
	struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_read_lock(&mcfg->mlock);

	/* dump all zones */
	for (i = 0; i < ODP_MAX_MM_DISTRICT; i++) {
		if (!mcfg->mm_district[i].addr)
			break;

		fprintf(f, "Zone %u: name:<%s>, phys:0x%lx, len:0x%zx, "
			"virt:%p, socket_id:%d, flags:%x\n", i,
			mcfg->mm_district[i].name,
			mcfg->mm_district[i].phys_addr,
			mcfg->mm_district[i].len,
			mcfg->mm_district[i].addr,
			mcfg->mm_district[i].socket_id,
			mcfg->mm_district[i].flags);
	}

	odp_rwlock_read_unlock(&mcfg->mlock);
}

/*
 * called by init: modify the free mmfrag list to have cache-aligned
 * addresses and cache-aligned lengths
 */
static int mmfrag_sanitize(struct odp_mmfrag *mmfrag)
{
	unsigned phys_align;
	unsigned virt_align;
	unsigned off;

	phys_align = mmfrag->phys_addr & ODP_CACHE_LINE_MASK;
	virt_align = (unsigned long)mmfrag->addr & ODP_CACHE_LINE_MASK;

	/*
	 * sanity check: phys_addr and addr must have the same
	 * alignment
	 */
	if (phys_align != virt_align)
		return -1;

	/* mmfrag is really too small, don't bother with it */
	if (mmfrag->len < (2 * ODP_CACHE_LINE_SIZE)) {
		mmfrag->len = 0;
		return 0;
	}

	/* align start address */
	off = (ODP_CACHE_LINE_SIZE - phys_align) & ODP_CACHE_LINE_MASK;
	mmfrag->phys_addr += off;
	mmfrag->addr = (char *)mmfrag->addr + off;
	mmfrag->len -= off;

	/* align end address */
	mmfrag->len &= ~((uint64_t)ODP_CACHE_LINE_MASK);

	return 0;
}

/*
 * Init the mm_district subsystem
 */
int odp_mm_district_init(void)
{
	struct odp_sys_layout *mcfg;
	const struct odp_mmfrag *mmfrag;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	/* mirror the runtime mmfrags from config */
	free_mmfrag = mcfg->free_mmfrag;

	/* secondary processes don't need to initialise anything */
	if (odp_process_type() == ODP_PROC_SECONDARY)
		return 0;

	mmfrag = odp_get_physmem_layout();
	if (!mmfrag) {
		ODP_ERR("Cannot get physical layout.\n");
		return -1;
	}

	odp_rwlock_write_lock(&mcfg->mlock);

	/* fill in uninitialized free_mmfrags */
	for (i = 0; i < ODP_MAX_MMFRAG; i++) {
		if (!mmfrag[i].addr)
			break;

		if (free_mmfrag[i].addr)
			continue;

		memcpy(&free_mmfrag[i], &mmfrag[i], sizeof(struct odp_mmfrag));
	}

	mcfg->free_frag_idx = i - 1;

	/* make all fragments cache-aligned */
	for (i = 0; i < ODP_MAX_MMFRAG; i++) {
		if (!free_mmfrag[i].addr)
			break;

		if (mmfrag_sanitize(&free_mmfrag[i]) < 0) {
			ODP_ERR("Sanity check failed\n");
			odp_rwlock_write_unlock(&mcfg->mlock);
			return -1;
		}
	}

	/* delete all fragments */
	mcfg->mm_district_idx = 0;
	memset(mcfg->mm_district, 0, sizeof(mcfg->mm_district));

	odp_rwlock_write_unlock(&mcfg->mlock);

	return 0;
}

/* Walk all reserved memory fragments */
void odp_mm_district_walk(void (*func)(const struct odp_mm_district *, void *),
			  void *arg)
{
	struct odp_sys_layout *mcfg;
	unsigned i;

	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_read_lock(&mcfg->mlock);
	for (i = 0; i < ODP_MAX_MM_DISTRICT; i++)
		if (mcfg->mm_district[i].addr)
			(*func)(&mcfg->mm_district[i], arg);

	odp_rwlock_read_unlock(&mcfg->mlock);
}
