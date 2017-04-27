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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <odp_memory.h>
#include <odp_mmdistrict.h>
#include <odp_base.h>
#include <odp_syslayout.h>

/* #include <odp_log.h> */

#include "odp_private.h"

/*
 * Return a pointer to a read-only table of struct odp_physmem_desc
 * elements, containing the layout of all addressable physical
 * memory. The last element of the table contains a NULL address.
 */
const struct odp_mmfrag *odp_get_physmem_layout(void)
{
	return odp_get_configuration()->sys_layout->mmfrag;
}

/* get the total size of memory */
uint64_t odp_get_physmem_size(void)
{
	const struct odp_sys_layout *mcfg;
	unsigned i = 0;
	uint64_t total_len = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	for (i = 0; i < ODP_MAX_MMFRAG; i++) {
		if (!mcfg->mmfrag[i].addr)
			break;

		total_len += mcfg->mmfrag[i].len;
	}

	return total_len;
}

/* Dump the physical memory layout on console */
void odp_dump_physmem_layout(FILE *f)
{
	const struct odp_sys_layout *mcfg;
	unsigned i = 0;

	/* get pointer to global configuration */
	mcfg = odp_get_configuration()->sys_layout;

	for (i = 0; i < ODP_MAX_MMFRAG; i++) {
		if (!mcfg->mmfrag[i].addr)
			break;

		fprintf(f, "Segment %u: phys:0x%lx, len:%zu, virt:%p, "
			"socket_id:%d, hugepage_sz:%lu, "
			"channel_num:%x, rank_num:%x\n", i,
			mcfg->mmfrag[i].phys_addr,
			mcfg->mmfrag[i].len,
			mcfg->mmfrag[i].addr,
			mcfg->mmfrag[i].socket_id,
			mcfg->mmfrag[i].hugepage_sz,
			mcfg->mmfrag[i].channel_num,
			mcfg->mmfrag[i].rank_num);
	}
}

/* return the number of memory channels */
unsigned int odp_memory_get_channel_num(void)
{
	return odp_get_configuration()->sys_layout->channel_num;
}

/* return the number of memory rank */
unsigned int odp_memory_get_rank_num(void)
{
	return odp_get_configuration()->sys_layout->rank_num;
}
