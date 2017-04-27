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

#include <sys/queue.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include <odp_memory.h>
#include <odp_mmdistrict.h>

/* #include <odp_launch.h> */
#include <odp_base.h>
#include <odp_syslayout.h>

/* #include <odp_per_core.h> */
#include <odp_core.h>

/* #include <odp_memory.h> */
#include <odp/config.h>
#include <odp/atomic.h>

/* #include <odp_branch_prediction.h>
   #include <odp_log.h>
   #include <odp_string_fns.h>
 #include <odp_debug.h> */

#include "odp_private.h"
#include "odp_debug_internal.h"
TAILQ_HEAD(odp_tailq_elem_head, odp_tailq_elem);

/* local tailq list */
static struct odp_tailq_elem_head odp_tailq_elem_head =
	TAILQ_HEAD_INITIALIZER(odp_tailq_elem_head);

/* number of tailqs registered, -1 before call to odp_tailqs_init */
static int odp_tailqs_count = -1;

struct odp_tailq_head *odp_tailq_lookup(const char *name)
{
	unsigned i;
	struct odp_sys_layout *mcfg = odp_get_configuration()->sys_layout;

	if (!name)
		return NULL;

	for (i = 0; i < ODP_MAX_TAILQ; i++)
		if (!strncmp(name, mcfg->tailq_head[i].name,
			     ODP_TAILQ_NAMESIZE - 1))
			return &mcfg->tailq_head[i];

	return NULL;
}

void odp_dump_tailq(FILE *f)
{
	struct odp_sys_layout *mcfg;
	unsigned i = 0;

	mcfg = odp_get_configuration()->sys_layout;

	odp_rwlock_read_lock(&mcfg->qlock);
	for (i = 0; i < ODP_MAX_TAILQ; i++) {
		const struct odp_tailq_head *tailq = &mcfg->tailq_head[i];
		const struct odp_tailq_entry_head *head = &tailq->tailq_head;

		fprintf(f, "Tailq %u: qname:<%s>, tqh_first:%p, tqh_last:%p\n",
			i, tailq->name, head->tqh_first, head->tqh_last);
	}

	odp_rwlock_read_lock(&mcfg->qlock);
}

static struct odp_tailq_head *odp_tailq_create(const char *name)
{
	struct odp_tailq_head *head = NULL;

	if (!odp_tailq_lookup(name) &&
	    (odp_tailqs_count + 1 < ODP_MAX_TAILQ)) {
		struct odp_sys_layout *mcfg;

		mcfg = odp_get_configuration()->sys_layout;
		head = &mcfg->tailq_head[odp_tailqs_count];
		snprintf(head->name, sizeof(head->name) - 1, "%s", name);
		TAILQ_INIT(&head->tailq_head);
		odp_tailqs_count++;
	}

	return head;
}

/* local register, used to store "early" tailqs before odp_init() and to
 * ensure secondary process only registers tailqs once. */
static int odp_tailq_local_register(struct odp_tailq_elem *t)
{
	struct odp_tailq_elem *temp;

	TAILQ_FOREACH(temp, &odp_tailq_elem_head, next)
	{
		if (!strncmp(t->name, temp->name, sizeof(temp->name)))
			return -1;
	}

	TAILQ_INSERT_TAIL(&odp_tailq_elem_head, t, next);
	return 0;
}

static void odp_tailq_update(struct odp_tailq_elem *t)
{
	if (odp_process_type() == ODP_PROC_PRIMARY)
		/* primary process is the only one that creates */
		t->head = odp_tailq_create(t->name);
	else
		t->head = odp_tailq_lookup(t->name);
}

int odp_tailq_register(struct odp_tailq_elem *t)
{
	if (odp_tailq_local_register(t) < 0) {
		ODP_PRINT("%s tailq is already registered\n", t->name);
		goto error;
	}

	/* if a register happens after odp_tailqs_init(), then we can update
	 * tailq head */
	if (odp_tailqs_count >= 0) {
		odp_tailq_update(t);
		if (!t->head) {
			ODP_ERR("Cannot initialize tailq: %s\n", t->name);
			TAILQ_REMOVE(&odp_tailq_elem_head, t, next);
			goto error;
		}
	}

	return 0;

error:
	t->head = NULL;
	return -1;
}

int odp_tailqs_init(void)
{
	struct odp_tailq_elem *t;

	odp_tailqs_count = 0;

	TAILQ_FOREACH(t, &odp_tailq_elem_head, next)
	{
		/* second part of register job for "early" tailqs, see
		 * odp_tailq_register and HODP_REGISTER_TAILQ */
		odp_tailq_update(t);
		if (!t->head) {
			ODP_ERR("Cannot initialize tailq: %s\n", t->name);

			/* no need to TAILQ_REMOVE, we are going to panic in
			 * odp_init() */
			goto fail;
		}
	}

	return 0;

fail:
	odp_dump_tailq(stderr);
	return -1;
}
