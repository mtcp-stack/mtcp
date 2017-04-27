/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/classification.h>
#include <odp/align.h>
#include <odp/queue.h>
#include <odp/debug.h>
#include <odp_internal.h>
#include <odp_debug_internal.h>
#include <odp_packet_internal.h>
#include <odp/packet_io.h>
#include <odp_packet_io_internal.h>
#include <odp_classification_datamodel.h>
#include <odp_classification_inlines.h>
#include <odp_classification_internal.h>
#include <odp_pool_internal.h>
#include <odp/shared_memory.h>
#include <odp/helper/eth.h>
#include <string.h>
#include <odp/spinlock.h>

#define LOCK(a)      odp_spinlock_lock(a)
#define UNLOCK(a)    odp_spinlock_unlock(a)
#define LOCK_INIT(a)	odp_spinlock_init(a)

static cos_tbl_t *cos_tbl;
static pmr_set_tbl_t	*pmr_set_tbl;
static pmr_tbl_t	*pmr_tbl;

cos_t *get_cos_entry_internal(odp_cos_t cos_id)
{
	return &cos_tbl->cos_entry[_odp_typeval(cos_id)];
}

pmr_set_t *get_pmr_set_entry_internal(odp_pmr_set_t pmr_set_id)
{
	return &pmr_set_tbl->pmr_set[_odp_typeval(pmr_set_id)];
}

pmr_t *get_pmr_entry_internal(odp_pmr_t pmr_id)
{
	return &pmr_tbl->pmr[_odp_typeval(pmr_id)];
}

int odp_classification_init_global(void)
{
	odp_shm_t cos_shm;
	odp_shm_t pmr_shm;
	odp_shm_t pmr_set_shm;
	int i;

	cos_shm = odp_shm_reserve("shm_odp_cos_tbl",
			sizeof(cos_tbl_t),
			sizeof(cos_t), 0);

	if (cos_shm == ODP_SHM_INVALID) {
		ODP_ERR("shm allocation failed for shm_odp_cos_tbl");
		goto error;
	}

	cos_tbl = odp_shm_addr(cos_shm);
	if (cos_tbl == NULL)
		goto error_cos;

	memset(cos_tbl, 0, sizeof(cos_tbl_t));
	for (i = 0; i < ODP_COS_MAX_ENTRY; i++) {
		/* init locks */
		cos_t *cos =
			get_cos_entry_internal(_odp_cast_scalar(odp_cos_t, i));
		LOCK_INIT(&cos->s.lock);
	}

	pmr_shm = odp_shm_reserve("shm_odp_pmr_tbl",
				  sizeof(pmr_tbl_t),
				  sizeof(pmr_t), 0);

	if (pmr_shm == ODP_SHM_INVALID) {
		ODP_ERR("shm allocation failed for shm_odp_pmr_tbl");
		goto error_cos;
	}

	pmr_tbl = odp_shm_addr(pmr_shm);
	if (pmr_tbl == NULL)
		goto error_pmr;

	memset(pmr_tbl, 0, sizeof(pmr_tbl_t));
	for (i = 0; i < ODP_PMR_MAX_ENTRY; i++) {
		/* init locks */
		pmr_t *pmr =
			get_pmr_entry_internal(_odp_cast_scalar(odp_pmr_t, i));
		LOCK_INIT(&pmr->s.lock);
	}

	pmr_set_shm = odp_shm_reserve("shm_odp_pmr_set_tbl",
				      sizeof(pmr_set_tbl_t),
				      sizeof(pmr_set_t), 0);

	if (pmr_set_shm == ODP_SHM_INVALID) {
		ODP_ERR("shm allocation failed for shm_odp_pmr_set_tbl");
		goto error_pmr;
	}

	pmr_set_tbl = odp_shm_addr(pmr_set_shm);
	if (pmr_set_tbl == NULL)
		goto error_pmrset;

	memset(pmr_set_tbl, 0, sizeof(pmr_set_tbl_t));
	for (i = 0; i < ODP_PMRSET_MAX_ENTRY; i++) {
		/* init locks */
		pmr_set_t *pmr =
			get_pmr_set_entry_internal
			(_odp_cast_scalar(odp_pmr_set_t, i));
		LOCK_INIT(&pmr->s.pmr.s.lock);
	}

	return 0;

error_pmrset:
	odp_shm_free(pmr_set_shm);
error_pmr:
	odp_shm_free(pmr_shm);
error_cos:
	odp_shm_free(cos_shm);
error:
	return -1;
}

int odp_classification_term_global(void)
{
	int ret = 0;
	int rc = 0;

	ret = odp_shm_free(odp_shm_lookup("shm_odp_cos_tbl"));
	if (ret < 0) {
		ODP_ERR("shm free failed for shm_odp_cos_tbl");
		rc = -1;
	}

	ret = odp_shm_free(odp_shm_lookup("shm_odp_pmr_tbl"));
	if (ret < 0) {
		ODP_ERR("shm free failed for shm_odp_pmr_tbl");
		rc = -1;
	}

	ret = odp_shm_free(odp_shm_lookup("shm_odp_pmr_set_tbl"));
	if (ret < 0) {
		ODP_ERR("shm free failed for shm_odp_pmr_tbl");
		rc = -1;
	}

	return rc;
}

void odp_cls_cos_param_init(odp_cls_cos_param_t *param)
{
	param->queue = ODP_QUEUE_INVALID;
	param->pool = ODP_POOL_INVALID;
	param->drop_policy = ODP_COS_DROP_NEVER;
}

odp_cos_t odp_cls_cos_create(const char *name, odp_cls_cos_param_t *param)
{
	int i;
	queue_entry_t *queue;
	pool_entry_t *pool;
	odp_cls_drop_t drop_policy;

	/* Packets are dropped if Queue or Pool is invalid*/
	if (param->queue == ODP_QUEUE_INVALID)
		queue = NULL;
	else
		queue = queue_to_qentry(param->queue);

	if (param->pool == ODP_POOL_INVALID)
		pool = NULL;
	else
		pool = odp_pool_to_entry(param->pool);

	drop_policy = param->drop_policy;

	for (i = 0; i < ODP_COS_MAX_ENTRY; i++) {
		LOCK(&cos_tbl->cos_entry[i].s.lock);
		if (0 == cos_tbl->cos_entry[i].s.valid) {
			strncpy(cos_tbl->cos_entry[i].s.name, name,
				ODP_COS_NAME_LEN - 1);
			cos_tbl->cos_entry[i].s.name[ODP_COS_NAME_LEN - 1] = 0;
			cos_tbl->cos_entry[i].s.pmr = NULL;
			cos_tbl->cos_entry[i].s.linked_cos = NULL;
			cos_tbl->cos_entry[i].s.queue = queue;
			cos_tbl->cos_entry[i].s.pool = pool;
			cos_tbl->cos_entry[i].s.flow_set = 0;
			cos_tbl->cos_entry[i].s.headroom = 0;
			cos_tbl->cos_entry[i].s.valid = 1;
			cos_tbl->cos_entry[i].s.drop_policy = drop_policy;
			UNLOCK(&cos_tbl->cos_entry[i].s.lock);
			return _odp_cast_scalar(odp_cos_t, i);
		}
		UNLOCK(&cos_tbl->cos_entry[i].s.lock);
	}

	ODP_ERR("ODP_COS_MAX_ENTRY reached");
	return ODP_COS_INVALID;
}

odp_pmr_set_t alloc_pmr_set(pmr_t **pmr)
{
	int i;

	for (i = 0; i < ODP_PMRSET_MAX_ENTRY; i++) {
		LOCK(&pmr_set_tbl->pmr_set[i].s.pmr.s.lock);
		if (0 == pmr_set_tbl->pmr_set[i].s.pmr.s.valid) {
			pmr_set_tbl->pmr_set[i].s.pmr.s.valid = 1;
			pmr_set_tbl->pmr_set[i].s.pmr.s.num_pmr = 0;
			*pmr = (pmr_t *)&pmr_set_tbl->pmr_set[i];
			odp_atomic_init_u32(&pmr_set_tbl->pmr_set[i]
					    .s.pmr.s.count, 0);
			/* return as locked */
			return _odp_cast_scalar(odp_pmr_set_t, i);
		}
		UNLOCK(&pmr_set_tbl->pmr_set[i].s.pmr.s.lock);
	}
	ODP_ERR("ODP_PMRSET_MAX_ENTRY reached");
	return ODP_PMR_SET_INVAL;
}

odp_pmr_t alloc_pmr(pmr_t **pmr)
{
	int i;

	for (i = 0; i < ODP_PMR_MAX_ENTRY; i++) {
		LOCK(&pmr_tbl->pmr[i].s.lock);
		if (0 == pmr_tbl->pmr[i].s.valid) {
			pmr_tbl->pmr[i].s.valid = 1;
			odp_atomic_init_u32(&pmr_tbl->pmr[i].s.count, 0);
			pmr_tbl->pmr[i].s.num_pmr = 0;
			*pmr = &pmr_tbl->pmr[i];
			/* return as locked */
			return _odp_cast_scalar(odp_pmr_t, i);
		}
		UNLOCK(&pmr_tbl->pmr[i].s.lock);
	}
	ODP_ERR("ODP_PMR_MAX_ENTRY reached");
	return ODP_PMR_INVAL;
}

cos_t *get_cos_entry(odp_cos_t cos_id)
{
	if (_odp_typeval(cos_id) >= ODP_COS_MAX_ENTRY ||
	    cos_id == ODP_COS_INVALID)
		return NULL;
	if (cos_tbl->cos_entry[_odp_typeval(cos_id)].s.valid == 0)
		return NULL;
	return &cos_tbl->cos_entry[_odp_typeval(cos_id)];
}


pmr_set_t *get_pmr_set_entry(odp_pmr_set_t pmr_set_id)
{
	if (_odp_typeval(pmr_set_id) >= ODP_PMRSET_MAX_ENTRY ||
	    pmr_set_id == ODP_PMR_SET_INVAL)
		return NULL;
	if (pmr_set_tbl->pmr_set[_odp_typeval(pmr_set_id)].s.pmr.s.valid == 0)
		return NULL;
	return &pmr_set_tbl->pmr_set[_odp_typeval(pmr_set_id)];
}

pmr_t *get_pmr_entry(odp_pmr_t pmr_id)
{
	if (_odp_typeval(pmr_id) >= ODP_PMR_MAX_ENTRY ||
	    pmr_id == ODP_PMR_INVAL)
		return NULL;
	if (pmr_tbl->pmr[_odp_typeval(pmr_id)].s.valid == 0)
		return NULL;
	return &pmr_tbl->pmr[_odp_typeval(pmr_id)];
}

int odp_cos_destroy(odp_cos_t cos_id)
{
	cos_t *cos = get_cos_entry(cos_id);

	if (NULL == cos) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	cos->s.valid = 0;
	return 0;
}

int odp_cos_queue_set(odp_cos_t cos_id, odp_queue_t queue_id)
{
	cos_t *cos = get_cos_entry(cos_id);

	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	/* Locking is not required as intermittent stale
	data during CoS modification is acceptable*/
	if (queue_id == ODP_QUEUE_INVALID)
		cos->s.queue = NULL;
	else
		cos->s.queue = queue_to_qentry(queue_id);

	return 0;
}

odp_queue_t odp_cos_queue(odp_cos_t cos_id)
{
	cos_t *cos = get_cos_entry(cos_id);

	if (!cos) {
		ODP_ERR("Invalid odp_cos_t handle");
		return ODP_QUEUE_INVALID;
	}

	if (!cos->s.queue)
		return ODP_QUEUE_INVALID;

	return cos->s.queue->s.handle;
}

int odp_cos_drop_set(odp_cos_t cos_id, odp_cls_drop_t drop_policy)
{
	cos_t *cos = get_cos_entry(cos_id);

	if (!cos) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	/*Drop policy is not supported in v1.0*/
	cos->s.drop_policy = drop_policy;
	return 0;
}

odp_cls_drop_t odp_cos_drop(odp_cos_t cos_id)
{
	cos_t *cos = get_cos_entry(cos_id);

	if (!cos) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	return cos->s.drop_policy;
}

int odp_pktio_default_cos_set(odp_pktio_t pktio_in, odp_cos_t default_cos)
{
	pktio_entry_t *entry;
	cos_t *cos;
	pktio_entry_t *pktio_entry;
	int id = 0;

	entry = get_pktio_entry(pktio_in);
	if (entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}
	cos = get_cos_entry(default_cos);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	/* temporary Solutions */
	pktio_entry = get_pktio_entry(pktio_in);
	id = pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index;

	entry->s.cls[id].default_cos = cos;
	pktio_cls_enabled_set(entry, id);
	return 0;
}

int odp_pktio_error_cos_set(odp_pktio_t pktio_in, odp_cos_t error_cos)
{
	pktio_entry_t *entry;
	cos_t *cos;
	int id = 0;

	entry = get_pktio_entry(pktio_in);
	if (entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}

	cos = get_cos_entry(error_cos);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	entry->s.cls[id].error_cos = cos;
	return 0;
}

int odp_pktio_skip_set(odp_pktio_t pktio_in, uint32_t offset)
{
	int id = 0;
	pktio_entry_t *entry = get_pktio_entry(pktio_in);

	if (entry == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	entry->s.cls[id].skip = offset;
	return 0;
}

int odp_pktio_headroom_set(odp_pktio_t pktio_in, uint32_t headroom)
{
	int id = 0;
	pktio_entry_t *entry = get_pktio_entry(pktio_in);

	if (entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}
	entry->s.cls[id].headroom = headroom;
	return 0;
}

int odp_cos_with_l2_priority(odp_pktio_t pktio_in,
			     uint8_t num_qos,
			     uint8_t qos_table[],
			     odp_cos_t cos_table[])
{
	pmr_l2_cos_t *l2_cos;
	uint32_t i;
	cos_t *cos;
	int id = 0;
	pktio_entry_t *entry = get_pktio_entry(pktio_in);

	if (entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}
	l2_cos = &entry->s.cls[id].l2_cos_table;

	LOCK(&l2_cos->lock);
	/* Update the L2 QoS table*/
	for (i = 0; i < num_qos; i++) {
		cos = get_cos_entry(cos_table[i]);
		if (cos != NULL) {
			if (ODP_COS_MAX_L2_QOS > qos_table[i])
				l2_cos->cos[qos_table[i]] = cos;
		}
	}
	pktio_cls_enabled_set(entry, id);
	UNLOCK(&l2_cos->lock);
	return 0;
}

int odp_cos_with_l3_qos(odp_pktio_t pktio_in,
			uint32_t num_qos,
			uint8_t qos_table[],
			odp_cos_t cos_table[],
			odp_bool_t l3_preference)
{
	pmr_l3_cos_t *l3_cos;
	uint32_t i;
	pktio_entry_t *entry = get_pktio_entry(pktio_in);
	cos_t *cos;
	int id = 0;

	if (entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}

	entry->s.cls[id].l3_precedence = l3_preference;
	l3_cos = &entry->s.cls[id].l3_cos_table;

	LOCK(&l3_cos->lock);
	/* Update the L3 QoS table*/
	for (i = 0; i < num_qos; i++) {
		cos = get_cos_entry(cos_table[i]);
		if (cos != NULL) {
			if (ODP_COS_MAX_L3_QOS > qos_table[i])
				l3_cos->cos[qos_table[i]] = cos;
		}
	}
	pktio_cls_enabled_set(entry, id);
	UNLOCK(&l3_cos->lock);
	return 0;
}

static void odp_pmr_create_term(pmr_term_value_t *value,
				const odp_pmr_match_t *match)
{
	value->term = match->term;
	value->offset = match->offset;
	value->val_sz = match->val_sz;
	value->val = 0;
	value->mask = 0;
	memcpy(&value->val, match->val, match->val_sz);
	memcpy(&value->mask, match->mask, match->val_sz);
	value->val &= value->mask;
}

odp_pmr_t odp_pmr_create(const odp_pmr_match_t *match)
{
	pmr_t *pmr;
	odp_pmr_t id;

	if (match->val_sz > ODP_PMR_TERM_BYTES_MAX) {
		ODP_ERR("val_sz greater than max supported limit");
		return ODP_PMR_INVAL;
	}

	id = alloc_pmr(&pmr);
	/*if alloc_pmr() is successful it returns with lock acquired*/
	if (id == ODP_PMR_INVAL)
		return ODP_PMR_INVAL;

	pmr->s.num_pmr = 1;
	odp_pmr_create_term(&pmr->s.pmr_term_value[0], match);
	UNLOCK(&pmr->s.lock);
	return id;
}

int odp_pmr_destroy(odp_pmr_t pmr_id)
{
	pmr_t *pmr = get_pmr_entry(pmr_id);

	if (pmr == NULL)
		return -1;
	pmr->s.valid = 0;
	return 0;
}

int odp_pktio_pmr_cos(odp_pmr_t pmr_id,
		      odp_pktio_t src_pktio,
		      odp_cos_t dst_cos)
{
	uint8_t num_pmr;
	pktio_entry_t *pktio_entry;
	pmr_t *pmr;
	cos_t *cos;
	int id = 0;

	pktio_entry = get_pktio_entry(src_pktio);
	if (pktio_entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}

	pmr = get_pmr_entry(pmr_id);
	if (pmr == NULL) {
		ODP_ERR("Invalid odp_pmr_t handle");
		return -1;
	}

	cos = get_cos_entry(dst_cos);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	LOCK(&pktio_entry->s.cls[id].lock);
	num_pmr = pktio_entry->s.cls[id].num_pmr;
	if (num_pmr >= ODP_PKTIO_MAX_PMR) {
		ODP_ERR("ODP_PKTIO_MAX_PMR reached");
		UNLOCK(&pktio_entry->s.cls[id].lock);
		return -1;
	}

	pktio_entry->s.cls[id].pmr[num_pmr] = pmr;
	pktio_entry->s.cls[id].cos[num_pmr] = cos;
	pktio_entry->s.cls[id].num_pmr++;
	pktio_cls_enabled_set(pktio_entry, id);
	UNLOCK(&pktio_entry->s.cls[id].lock);

	return 0;
}

int odp_cos_pmr_cos(odp_pmr_t pmr_id, odp_cos_t src_cos, odp_cos_t dst_cos)
{
	cos_t *cos_src = get_cos_entry(src_cos);
	cos_t *cos_dst = get_cos_entry(dst_cos);
	pmr_t *pmr = get_pmr_entry(pmr_id);

	if (NULL == cos_src || NULL == cos_dst || NULL == pmr) {
		ODP_ERR("Invalid input handle");
		return -1;
	}

	/*Locking is not required as intermittent stale data is acceptable*/
	cos_src->s.pmr = pmr;
	cos_src->s.linked_cos = cos_dst;

	return 0;
}

unsigned long long odp_pmr_terms_cap(void)
{
	unsigned long long term_cap = 0;

	term_cap |= (1 << ODP_PMR_LEN);
	term_cap |= (1 << ODP_PMR_IPPROTO);
	term_cap |= (1 << ODP_PMR_UDP_DPORT);
	term_cap |= (1 << ODP_PMR_TCP_DPORT);
	term_cap |= (1 << ODP_PMR_UDP_SPORT);
	term_cap |= (1 << ODP_PMR_TCP_SPORT);
	term_cap |= (1 << ODP_PMR_SIP_ADDR);
	term_cap |= (1 << ODP_PMR_DIP_ADDR);
	return term_cap;
}

unsigned odp_pmr_terms_avail(void)
{
	unsigned count = 0;
	int i;

	for (i = 0; i < ODP_PMR_MAX_ENTRY; i++)
		if (!pmr_tbl->pmr[i].s.valid)
			count++;
	return count;
}

int odp_pmr_match_set_create(int num_terms, const odp_pmr_match_t *terms,
			     odp_pmr_set_t *pmr_set_id)
{
	pmr_t *pmr;
	int i;
	odp_pmr_set_t id;
	int val_sz;
	int count = 0;

	if (num_terms > ODP_PMRTERM_MAX) {
		ODP_ERR("no of terms greater than supported ODP_PMRTERM_MAX");
		return -1;
	}

	id = alloc_pmr_set(&pmr);
	/*if alloc_pmr_set is successful it returns with the acquired lock*/
	if (id == ODP_PMR_SET_INVAL) {
		*pmr_set_id = id;
		return -1;
	}

	pmr->s.num_pmr = num_terms;
	for (i = 0; i < num_terms; i++) {
		val_sz = terms[i].val_sz;
		if (val_sz > ODP_PMR_TERM_BYTES_MAX)
			continue;
		odp_pmr_create_term(&pmr->s.pmr_term_value[i], &terms[i]);
		count++;
	}
	*pmr_set_id = id;
	UNLOCK(&pmr->s.lock);
	return count;
}

int odp_pmr_match_set_destroy(odp_pmr_set_t pmr_set_id)
{
	pmr_set_t *pmr_set = get_pmr_set_entry(pmr_set_id);

	if (pmr_set == NULL)
		return -1;

	pmr_set->s.pmr.s.valid = 0;
	return 0;
}

int odp_pktio_pmr_match_set_cos(odp_pmr_set_t pmr_set_id,
				odp_pktio_t src_pktio,
				odp_cos_t dst_cos)
{
	uint8_t num_pmr;
	pktio_entry_t *pktio_entry;
	pmr_t *pmr;
	cos_t *cos;
	int phyq_id;

	pktio_entry = get_pktio_entry(src_pktio);
	if (pktio_entry == NULL) {
		ODP_ERR("Invalid odp_pktio_t handle");
		return -1;
	}

	pmr = (pmr_t *)get_pmr_set_entry(pmr_set_id);
	if (pmr == NULL) {
		ODP_ERR("Invalid odp_pmr_set_t handle");
		return -1;
	}

	cos = get_cos_entry(dst_cos);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	phyq_id = pktio_entry->s.in_queue[PKTIO_MAX_QUEUES - 1].pktin.index;

	LOCK(&pktio_entry->s.cls[phyq_id].lock);
	num_pmr = pktio_entry->s.cls[phyq_id].num_pmr;
	if (num_pmr >= ODP_PKTIO_MAX_PMR) {
		ODP_ERR("ODP_PKTIO_MAX_PMR reached");
		UNLOCK(&pktio_entry->s.cls[phyq_id].lock);
		return -1;
	}

	pktio_entry->s.cls[phyq_id].pmr[num_pmr] = pmr;
	pktio_entry->s.cls[phyq_id].cos[num_pmr] = cos;
	pktio_entry->s.cls[phyq_id].num_pmr++;

	pktio_cls_enabled_set(pktio_entry, phyq_id);
	UNLOCK(&pktio_entry->s.cls[phyq_id].lock);

	return 0;
}

int odp_cls_cos_pool_set(odp_cos_t cos_id, odp_pool_t pool_id)
{
	cos_t *cos;

	cos = get_cos_entry(cos_id);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return -1;
	}

	if (pool_id == ODP_POOL_INVALID)
		cos->s.pool = NULL;
	else
		cos->s.pool =  odp_pool_to_entry(pool_id);

	return 0;
}

odp_pool_t odp_cls_cos_pool(odp_cos_t cos_id)
{
	cos_t *cos;

	cos = get_cos_entry(cos_id);
	if (cos == NULL) {
		ODP_ERR("Invalid odp_cos_t handle");
		return ODP_POOL_INVALID;
	}

	if (!cos->s.pool)
		return ODP_POOL_INVALID;

	return cos->s.pool->s.pool_hdl;
}

int verify_pmr(pmr_t *pmr, const uint8_t *pkt_addr, odp_packet_hdr_t *pkt_hdr)
{
	int pmr_failure = 0;
	int num_pmr;
	int i;
	pmr_term_value_t *term_value;

	/* Locking is not required as PMR rules for in-flight packets
	delivery during a PMR change is indeterminate*/

	if (!pmr->s.valid)
		return 0;
	num_pmr = pmr->s.num_pmr;

	/* Iterate through list of PMR Term values in a pmr_t */
	for (i = 0; i < num_pmr; i++) {
		term_value = &pmr->s.pmr_term_value[i];
		switch (term_value->term) {
		case ODP_PMR_LEN:
			if (!verify_pmr_packet_len(pkt_hdr, term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_ETHTYPE_0:
			if (!verify_pmr_eth_type_0(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_ETHTYPE_X:
			if (!verify_pmr_eth_type_x(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_VLAN_ID_0:
			if (!verify_pmr_vlan_id_0(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_VLAN_ID_X:
			if (!verify_pmr_vlan_id_x(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_DMAC:
			if (!verify_pmr_dmac(pkt_addr, pkt_hdr,
					     term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_IPPROTO:
			if (!verify_pmr_ip_proto(pkt_addr, pkt_hdr,
						 term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_UDP_DPORT:
			if (!verify_pmr_udp_dport(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_TCP_DPORT:
			if (!verify_pmr_tcp_dport(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_UDP_SPORT:
			if (!verify_pmr_udp_sport(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_TCP_SPORT:
			if (!verify_pmr_tcp_sport(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_SIP_ADDR:
			if (!verify_pmr_ipv4_saddr(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_DIP_ADDR:
			if (!verify_pmr_ipv4_daddr(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_SIP6_ADDR:
			if (!verify_pmr_ipv6_saddr(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_DIP6_ADDR:
			if (!verify_pmr_ipv6_daddr(pkt_addr, pkt_hdr,
						   term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_IPSEC_SPI:
			if (!verify_pmr_ipsec_spi(pkt_addr, pkt_hdr,
						  term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_LD_VNI:
			if (!verify_pmr_ld_vni(pkt_addr, pkt_hdr,
					       term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_CUSTOM_FRAME:
			if (!verify_pmr_custom_frame(pkt_addr, pkt_hdr,
						     term_value))
				pmr_failure = 1;
			break;
		case ODP_PMR_INNER_HDR_OFF:
			break;
		}

		if (pmr_failure)
			return false;
	}
	odp_atomic_inc_u32(&pmr->s.count);
	return true;
}

cos_t *match_pmr_cos(cos_t *cos, const uint8_t *pkt_addr, pmr_t *pmr,
		     odp_packet_hdr_t *hdr)
{
	cos_t *retcos = NULL;

	if (cos == NULL || pmr == NULL)
		return NULL;

	if (!cos->s.valid)
		return NULL;

	if (verify_pmr(pmr, pkt_addr, hdr)) {
		/** This gets called recursively to check all the PMRs in
		 * a PMR chain */
		retcos = match_pmr_cos(cos->s.linked_cos, pkt_addr,
				       cos->s.pmr, hdr);
		if (!retcos)
			return cos;
	}
	return retcos;
}

int pktio_classifier_init(pktio_entry_t *entry)
{
	classifier_t *cls;
	int i;
	int id;

	/* classifier lock should be acquired by the calling function */
	if (entry == NULL)
		return -1;

	for (id = 0; id < MAX_CLS_SUPPORT; id++) {
		cls = &entry->s.cls[id];
		cls->num_pmr = 0;
		cls->flow_set = 0;
		cls->error_cos = NULL;
		cls->default_cos = NULL;
		cls->headroom = 0;
		cls->skip = 0;

		for (i = 0; i < ODP_PKTIO_MAX_PMR; i++) {
			cls->pmr[i] = NULL;
			cls->cos[i] = NULL;
		}
	}

	return 0;
}

int _odp_packet_classifier(pktio_entry_t *entry, int queue_id, odp_packet_t pkt)
{
	queue_entry_t *queue;
	cos_t *cos;
	odp_packet_hdr_t *pkt_hdr;
	odp_packet_t new_pkt;
	uint8_t *pkt_addr;

	if (entry == NULL)
		return -1;

	pkt_hdr = odp_packet_hdr(pkt);
	pkt_addr = odp_packet_data(pkt);

	/* Matching PMR and selecting the CoS for the packet*/
	cos = pktio_select_cos(entry, queue_id, pkt_addr, pkt_hdr);
	if (cos == NULL)
		return -1;

	if (cos->s.pool == NULL) {
		odp_packet_free(pkt);
		return -1;
	}

	if (cos->s.queue == NULL) {
		odp_packet_free(pkt);
		return -1;
	}

	if (odp_packet_pool(pkt) != cos->s.pool->s.pool_hdl) {
		new_pkt = odp_packet_copy(pkt, cos->s.pool->s.pool_hdl);
		if (new_pkt == ODP_PACKET_INVALID)
			return -1;
		odp_packet_free(pkt);
	} else {
		new_pkt = pkt;
	}

	/* Enqueuing the Packet based on the CoS */
	queue = cos->s.queue;
	return queue_enq(queue, odp_buf_to_hdr((odp_buffer_t)new_pkt), 0);
}

int packet_classifier(odp_pktio_t pktio, odp_packet_t pkt)
{
	pktio_entry_t *entry;
	int id = 0;

	entry = get_pktio_entry(pktio);
	if (entry == NULL)
		return -1;
	return _odp_packet_classifier(entry, id, pkt);
}

cos_t *pktio_select_cos(pktio_entry_t *entry, int queue_id,
			const uint8_t *pkt_addr,
			odp_packet_hdr_t *pkt_hdr)
{
	pmr_t *pmr;
	cos_t *cos;
	uint32_t i;
	classifier_t *cls;

	/* Check for lazy parse needed */
	if (packet_parse_not_complete(pkt_hdr))
		packet_parse_full(pkt_hdr);

	/* Return error cos for error packet */
	if (pkt_hdr->error_flags.all)
		return NULL;

	/* global rule check */
	cls = &entry->s.cls[GLOBAL_CLS_ID];
	/* Calls all the PMRs attached at the PKTIO level*/
	for (i = 0; i < cls->num_pmr; i++) {
		pmr = cls->pmr[i];
		cos = cls->cos[i];
		cos = match_pmr_cos(cos, pkt_addr, pmr, pkt_hdr);
		if (cos)
			return cos;
	}

	/* phy queue rule check */
	cls = &entry->s.cls[queue_id];
	/* Calls all the PMRs attached at the PKTIO level*/
	for (i = 0; i < cls->num_pmr; i++) {
		pmr = cls->pmr[i];
		cos = cls->cos[i];
		cos = match_pmr_cos(cos, pkt_addr, pmr, pkt_hdr);
		if (cos)
			return cos;
	}

	cos = match_qos_cos(entry, pkt_addr, pkt_hdr);
	if (cos)
		return cos;

	return cls->default_cos;
}

cos_t *match_qos_l3_cos(pmr_l3_cos_t *l3_cos, const uint8_t *pkt_addr,
			odp_packet_hdr_t *hdr)
{
	uint8_t dscp;
	cos_t *cos = NULL;
	const odph_ipv4hdr_t *ipv4;
	const odph_ipv6hdr_t *ipv6;

	if (hdr->input_flags.l3 && hdr->input_flags.ipv4) {
		ipv4 = (const odph_ipv4hdr_t *)(pkt_addr + hdr->l3_offset);
		dscp = ODPH_IPV4HDR_DSCP(ipv4->tos);
		cos = l3_cos->cos[dscp];
	} else if (hdr->input_flags.l3 && hdr->input_flags.ipv6) {
		ipv6 = (const odph_ipv6hdr_t *)(pkt_addr + hdr->l3_offset);
		dscp = ODPH_IPV6HDR_DSCP(ipv6->ver_tc_flow);
		cos = l3_cos->cos[dscp];
	}

	return cos;
}

cos_t *match_qos_l2_cos(pmr_l2_cos_t *l2_cos, const uint8_t *pkt_addr,
			odp_packet_hdr_t *hdr)
{
	cos_t *cos = NULL;
	const odph_ethhdr_t *eth;
	const odph_vlanhdr_t *vlan;
	uint16_t qos;

	if (hdr->input_flags.l2 && hdr->input_flags.vlan &&
	    hdr->input_flags.eth) {
		eth = (const odph_ethhdr_t *)(pkt_addr + hdr->l2_offset);
		vlan = (const odph_vlanhdr_t *)(&eth->type);
		qos = odp_be_to_cpu_16(vlan->tci);
		qos = ((qos >> 13) & 0x07);
		cos = l2_cos->cos[qos];
	}
	return cos;
}

cos_t *match_qos_cos(pktio_entry_t *entry, const uint8_t *pkt_addr,
		     odp_packet_hdr_t *hdr)
{
	int id = 0;
	classifier_t *cls = &entry->s.cls[id];
	pmr_l2_cos_t *l2_cos;
	pmr_l3_cos_t *l3_cos;
	cos_t *cos;

	l2_cos = &cls->l2_cos_table;
	l3_cos = &cls->l3_cos_table;

	if (cls->l3_precedence) {
		cos =  match_qos_l3_cos(l3_cos, pkt_addr, hdr);
		if (cos)
			return cos;
		cos = match_qos_l2_cos(l2_cos, pkt_addr, hdr);
		if (cos)
			return cos;
	} else {
		cos = match_qos_l2_cos(l2_cos, pkt_addr, hdr);
		if (cos)
			return cos;
		cos = match_qos_l3_cos(l3_cos, pkt_addr, hdr);
		if (cos)
			return cos;
	}
	return NULL;
}
