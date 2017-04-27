/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/packet_io.h>
#include <odp_packet_io_internal.h>
#include <odp_packet_io_queue.h>
#include <odp/packet.h>
#include <odp_packet_internal.h>
#include <odp_internal.h>
#include <odp/spinlock.h>
#include <odp/ticketlock.h>
#include <odp/shared_memory.h>
#include <odp_packet_socket.h>
#include <odp/config.h>
#include <odp_queue_internal.h>
#include <odp_schedule_internal.h>
#include <odp_classification_internal.h>
#include <odp_debug_internal.h>

#include <string.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <errno.h>

pktio_table_t *pktio_tbl;

/* pktio pointer entries ( for inlines) */
void *pktio_entry_ptr[ODP_CONFIG_PKTIO_ENTRIES];

int odp_pktio_init_global(void)
{
	char name[ODP_QUEUE_NAME_LEN];
	pktio_entry_t *pktio_entry;
	queue_entry_t *queue_entry;
	odp_queue_t qid;
	int id;
	int phyqid;
	odp_shm_t shm;
	int pktio_if;

	shm = odp_shm_reserve("odp_pktio_entries",
			      sizeof(pktio_table_t),
			      sizeof(pktio_entry_t), 0);
	pktio_tbl = odp_shm_addr(shm);

	if (pktio_tbl == NULL)
		return -1;

	memset(pktio_tbl, 0, sizeof(pktio_table_t));

	odp_spinlock_init(&pktio_tbl->lock);

	for (id = 1; id <= ODP_CONFIG_PKTIO_ENTRIES; ++id) {
		odp_queue_param_t param;

		pktio_entry = &pktio_tbl->entries[id - 1];

		odp_ticketlock_init(&pktio_entry->s.rxl);
		odp_ticketlock_init(&pktio_entry->s.txl);

		for (phyqid = 0; phyqid < MAX_CLS_SUPPORT; phyqid++) {
			odp_spinlock_init(&pktio_entry->s.cls[phyqid].lock);
			odp_spinlock_init(
				&pktio_entry->s.cls[phyqid].l2_cos_table.lock);
			odp_spinlock_init(
				&pktio_entry->s.cls[phyqid].l3_cos_table.lock);
		}

		pktio_entry_ptr[id - 1] = pktio_entry;
		/* Create a default output queue for each pktio resource */
		snprintf(name, sizeof(name), "%i-pktio_outq_default", (int)id);
		name[ODP_QUEUE_NAME_LEN-1] = '\0';

		odp_queue_param_init(&param);
		param.type = ODP_QUEUE_TYPE_PKTOUT;

		qid = odp_queue_create(name, &param);
		if (qid == ODP_QUEUE_INVALID)
			return -1;
		pktio_entry->s.outq_default = qid;

		queue_entry = queue_to_qentry(qid);
		queue_entry->s.pktout = _odp_cast_scalar(odp_pktio_t, id);
	}

	for (pktio_if = 0; pktio_if_ops[pktio_if]; ++pktio_if) {
		if (pktio_if_ops[pktio_if]->init)
			if (pktio_if_ops[pktio_if]->init())
				ODP_ERR("failed to initialized pktio type %d",
					pktio_if);
	}

	return 0;
}

int odp_pktio_init_local(void)
{
	return 0;
}

int is_free(pktio_entry_t *entry)
{
	return (entry->s.taken == 0);
}

static void set_free(pktio_entry_t *entry)
{
	entry->s.taken = 0;
}

static void set_taken(pktio_entry_t *entry)
{
	entry->s.taken = 1;
}

static void lock_entry(pktio_entry_t *entry)
{
	odp_ticketlock_lock(&entry->s.rxl);
	odp_ticketlock_lock(&entry->s.txl);
}

static void unlock_entry(pktio_entry_t *entry)
{
	odp_ticketlock_unlock(&entry->s.txl);
	odp_ticketlock_unlock(&entry->s.rxl);
}

static void lock_entry_classifier(pktio_entry_t *entry)
{
	int id = 0;

	odp_ticketlock_lock(&entry->s.rxl);
	odp_ticketlock_lock(&entry->s.txl);
	odp_spinlock_lock(&entry->s.cls[id].lock);
}

static void unlock_entry_classifier(pktio_entry_t *entry)
{
	int id = 0;

	odp_spinlock_unlock(&entry->s.cls[id].lock);
	odp_ticketlock_unlock(&entry->s.txl);
	odp_ticketlock_unlock(&entry->s.rxl);
}

static void init_pktio_entry(pktio_entry_t *entry)
{
	int i;

	set_taken(entry);
	pktio_cls_enabled_init(entry);

	for (i = 0; i < PKTIO_MAX_QUEUES; i++) {
		entry->s.in_queue[i].queue   = ODP_QUEUE_INVALID;
		entry->s.in_queue[i].pktin   = PKTIN_INVALID;
		entry->s.out_queue[i].pktout = PKTOUT_INVALID;
	}

	pktio_classifier_init(entry);
}

static odp_pktio_t alloc_lock_pktio_entry(void)
{
	odp_pktio_t id;
	pktio_entry_t *entry;
	int i;

	for (i = 0; i < ODP_CONFIG_PKTIO_ENTRIES; ++i) {
		entry = &pktio_tbl->entries[i];
		if (is_free(entry)) {
			lock_entry_classifier(entry);
			if (is_free(entry)) {
				init_pktio_entry(entry);
				id = _odp_cast_scalar(odp_pktio_t, i + 1);
				return id; /* return with entry locked! */
			}
			unlock_entry_classifier(entry);
		}
	}

	return ODP_PKTIO_INVALID;
}

static int free_pktio_entry(odp_pktio_t id)
{
	pktio_entry_t *entry = get_pktio_entry(id);

	if (entry == NULL)
		return -1;

	set_free(entry);

	return 0;
}

static odp_pktio_t setup_pktio_entry(const char *dev, odp_pool_t pool,
				     const odp_pktio_param_t *param)
{
	odp_pktio_t id;
	pktio_entry_t *pktio_entry;
	int ret = -1;
	int pktio_if;

	if (strlen(dev) >= PKTIO_NAME_LEN - 1) {
		/* ioctl names limitation */
		ODP_ERR("pktio name %s is too big, limit is %d bytes\n",
			dev, PKTIO_NAME_LEN - 1);
		return ODP_PKTIO_INVALID;
	}

	id = alloc_lock_pktio_entry();
	if (id == ODP_PKTIO_INVALID) {
		ODP_ERR("No resources available.\n");
		return ODP_PKTIO_INVALID;
	}
	/* if successful, alloc_pktio_entry() returns with the entry locked */

	pktio_entry = get_pktio_entry(id);
	if (!pktio_entry)
		return ODP_PKTIO_INVALID;

	memcpy(&pktio_entry->s.param, param, sizeof(odp_pktio_param_t));
	pktio_entry->s.id = id;

	for (pktio_if = 0; pktio_if_ops[pktio_if]; ++pktio_if) {
		ret = pktio_if_ops[pktio_if]->open(id, pktio_entry, dev, pool);

		if (!ret) {
			pktio_entry->s.ops = pktio_if_ops[pktio_if];
			break;
		}
	}

	if (ret != 0) {
		unlock_entry_classifier(pktio_entry);
		free_pktio_entry(id);
		id = ODP_PKTIO_INVALID;
		ODP_ERR("Unable to init any I/O type.\n");
	} else {
		snprintf(pktio_entry->s.name,
			 sizeof(pktio_entry->s.name), "%s", dev);
		pktio_entry->s.state = STATE_STOP;
		unlock_entry_classifier(pktio_entry);
	}

	pktio_entry->s.handle = id;

	return id;
}

int pool_type_is_packet(odp_pool_t pool)
{
	odp_pool_info_t pool_info;

	if (pool == ODP_POOL_INVALID)
		return 0;

	if (odp_pool_info(pool, &pool_info) != 0)
		return 0;

	return pool_info.params.type == ODP_POOL_PACKET;
}

odp_pktio_t odp_pktio_open(const char *dev, odp_pool_t pool,
			   const odp_pktio_param_t *param)
{
	odp_pktio_t id;

	ODP_ASSERT(pool_type_is_packet(pool));

	id = odp_pktio_lookup(dev);
	if (id != ODP_PKTIO_INVALID) {
		/* interface is already open */
		__odp_errno = EEXIST;
		return ODP_PKTIO_INVALID;
	}

	odp_spinlock_lock(&pktio_tbl->lock);
	id = setup_pktio_entry(dev, pool, param);
	odp_spinlock_unlock(&pktio_tbl->lock);

	return id;
}

static int _pktio_close(pktio_entry_t *entry)
{
	int ret;

	ret = entry->s.ops->close(entry);
	if (ret)
		return -1;

	set_free(entry);
	return 0;
}

static void destroy_in_queues(pktio_entry_t *entry, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		if (entry->s.in_queue[i].queue != ODP_QUEUE_INVALID) {
			odp_queue_destroy(entry->s.in_queue[i].queue);
			entry->s.in_queue[i].queue = ODP_QUEUE_INVALID;
		}
	}
}

int odp_pktio_close(odp_pktio_t id)
{
	pktio_entry_t *entry;
	int res;

	entry = get_pktio_entry(id);
	if (entry == NULL)
		return -1;

	lock_entry(entry);

	destroy_in_queues(entry, entry->s.num_in_queue);

	if (!is_free(entry)) {
		res = _pktio_close(entry);
		if (res)
			ODP_ABORT("unable to close pktio\n");
	}
	unlock_entry(entry);

	return 0;
}

int odp_pktio_start(odp_pktio_t id)
{
	pktio_entry_t *entry;
	odp_pktin_mode_t mode;
	int res = 0;

	entry = get_pktio_entry(id);
	if (!entry)
		return -1;

	lock_entry(entry);
	if (entry->s.state == STATE_START) {
		unlock_entry(entry);
		return -1;
	}
	if (entry->s.ops->start)
		res = entry->s.ops->start(entry);
	if (!res)
		entry->s.state = STATE_START;

	unlock_entry(entry);

	mode = entry->s.param.in_mode;

	if (mode == ODP_PKTIN_MODE_SCHED) {
		unsigned i;

		for (i = 0; i < entry->s.num_in_queue; i++) {
			int index = i;

			if (entry->s.in_queue[i].queue == ODP_QUEUE_INVALID) {
				ODP_ERR("No input queue\n");
				return -1;
			}

			schedule_pktio_start(id, 1, &index);
		}
	}

	return res;
}

static int _pktio_stop(pktio_entry_t *entry)
{
	int res = 0;

	if (entry->s.state == STATE_STOP)
		return -1;

	if (entry->s.ops->stop)
		res = entry->s.ops->stop(entry);
	if (!res)
		entry->s.state = STATE_STOP;

	return res;
}

int odp_pktio_stop(odp_pktio_t id)
{
	pktio_entry_t *entry;
	int res;

	entry = get_pktio_entry(id);
	if (!entry)
		return -1;

	lock_entry(entry);
	res = _pktio_stop(entry);
	unlock_entry(entry);

	return res;
}

odp_pktio_t odp_pktio_lookup(const char *dev)
{
	odp_pktio_t id = ODP_PKTIO_INVALID;
	pktio_entry_t *entry;
	int i;

	odp_spinlock_lock(&pktio_tbl->lock);

	for (i = 1; i <= ODP_CONFIG_PKTIO_ENTRIES; ++i) {
		entry = get_pktio_entry(_odp_cast_scalar(odp_pktio_t, i));
		if (!entry || is_free(entry))
			continue;

		lock_entry(entry);

		if (!is_free(entry) &&
		    strncmp(entry->s.name, dev, sizeof(entry->s.name)) == 0)
			id = _odp_cast_scalar(odp_pktio_t, i);

		unlock_entry(entry);

		if (id != ODP_PKTIO_INVALID)
			break;
	}

	odp_spinlock_unlock(&pktio_tbl->lock);

	return id;
}

int odp_pktio_recv(odp_pktio_t id, odp_packet_t pkt_table[], int len)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);
	int pkts;
	int i;

	ODP_ASSERT(pktio_entry != NULL);

	odp_ticketlock_lock(&pktio_entry->s.rxl);
	if (odp_unlikely(pktio_entry->s.state == STATE_STOP ||
	    pktio_entry->s.param.in_mode == ODP_PKTIN_MODE_DISABLED)) {
		odp_ticketlock_unlock(&pktio_entry->s.rxl);
		__odp_errno = EPERM;
		return -1;
	}

	pkts = pktio_entry->s.ops->recv(pktio_entry, pkt_table, len);
	odp_ticketlock_unlock(&pktio_entry->s.rxl);
	for (i = 0; i < pkts; ++i)
		odp_packet_hdr(pkt_table[i])->input = id;

	return pkts;
}

int odp_pktio_send(odp_pktio_t id, odp_packet_t pkt_table[], int len)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);
	int pkts;

	ODP_ASSERT(pktio_entry != NULL);

	odp_ticketlock_lock(&pktio_entry->s.txl);
	if (odp_unlikely(pktio_entry->s.state == STATE_STOP ||
	    pktio_entry->s.param.out_mode == ODP_PKTOUT_MODE_DISABLED)) {
		odp_ticketlock_unlock(&pktio_entry->s.txl);
		__odp_errno = EPERM;
		return -1;
	}
	pkts = pktio_entry->s.ops->send(pktio_entry, pkt_table, len);
	odp_ticketlock_unlock(&pktio_entry->s.txl);

	return pkts;
}

int odp_pktio_inq_setdef(odp_pktio_t id, odp_queue_t queue)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);
	queue_entry_t *qentry;

	if (pktio_entry == NULL || queue == ODP_QUEUE_INVALID)
		return -1;

	qentry = queue_to_qentry(queue);

	if (qentry->s.type != ODP_QUEUE_TYPE_PKTIN)
		return -1;

	lock_entry(pktio_entry);
	if (pktio_entry->s.state != STATE_STOP) {
		unlock_entry(pktio_entry);
		return -1;
	}

	/* Temporary support for default input queue */
	pktio_entry->s.in_queue[0].queue = queue;
	pktio_entry->s.in_queue[0].pktin.pktio = id;
	pktio_entry->s.in_queue[0].pktin.index = 0;
	pktio_entry->s.num_in_queue = 1;
	unlock_entry(pktio_entry);

	/* User polls the input queue */
	queue_lock(qentry);
	qentry->s.pktin.pktio = id;
	qentry->s.pktin.index = 0;
	queue_unlock(qentry);

	return 0;
}

int odp_pktio_inq_remdef(odp_pktio_t id)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);
	odp_queue_t queue;
	queue_entry_t *qentry;

	if (pktio_entry == NULL)
		return -1;

	lock_entry(pktio_entry);
	if (pktio_entry->s.state != STATE_STOP) {
		unlock_entry(pktio_entry);
		return -1;
	}

	/* Temporary support for default input queue */
	queue = pktio_entry->s.in_queue[0].queue;
	qentry = queue_to_qentry(queue);

	queue_lock(qentry);
	qentry->s.pktin = PKTIN_INVALID;
	queue_unlock(qentry);

	pktio_entry->s.in_queue[0].queue = ODP_QUEUE_INVALID;
	pktio_entry->s.in_queue[0].pktin.pktio = ODP_PKTIO_INVALID;
	pktio_entry->s.in_queue[0].pktin.index = 0;
	pktio_entry->s.num_in_queue = 0;
	unlock_entry(pktio_entry);

	return 0;
}

odp_queue_t odp_pktio_inq_getdef(odp_pktio_t id)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);

	if (pktio_entry == NULL)
		return ODP_QUEUE_INVALID;

	/* Temporary support for default input queue */
	return pktio_entry->s.in_queue[0].queue;
}

odp_queue_t odp_pktio_outq_getdef(odp_pktio_t id)
{
	pktio_entry_t *pktio_entry = get_pktio_entry(id);

	if (pktio_entry == NULL)
		return ODP_QUEUE_INVALID;

	return pktio_entry->s.outq_default;
}

int pktout_enqueue(queue_entry_t *qentry, odp_buffer_hdr_t *buf_hdr)
{
	odp_packet_t pkt = (odp_packet_t)buf_hdr;
	int len = 1;
	int nbr;

	nbr = odp_pktio_send(qentry->s.pktout, &pkt, len);
	return (nbr == len ? 0 : -1);
}

odp_buffer_hdr_t *pktout_dequeue(queue_entry_t *qentry ODP_UNUSED)
{
	ODP_ABORT("attempted dequeue from a pktout queue");
	return NULL;
}

int pktout_enq_multi(queue_entry_t *qentry, odp_buffer_hdr_t *buf_hdr[],
		     int num)
{
	return odp_pktio_send(qentry->s.pktout, (odp_packet_t *)buf_hdr, num);
}

int pktout_deq_multi(queue_entry_t *qentry ODP_UNUSED,
		     odp_buffer_hdr_t *buf_hdr[] ODP_UNUSED,
		     int num ODP_UNUSED)
{
	ODP_ABORT("attempted dequeue from a pktout queue");
	return 0;
}

int pktin_enqueue(queue_entry_t *qentry ODP_UNUSED,
		  odp_buffer_hdr_t *buf_hdr ODP_UNUSED, int sustain ODP_UNUSED)
{
	ODP_ABORT("attempted enqueue to a pktin queue");
	return -1;
}

odp_buffer_hdr_t *pktin_dequeue(queue_entry_t *qentry)
{
	odp_packet_t pkt_tbl[QUEUE_MULTI_MAX];
	odp_buffer_hdr_t *buf_hdr;
	int pkts;

	buf_hdr = queue_deq(qentry);
	if (buf_hdr != NULL)
		return buf_hdr;

	pkts = odp_pktio_recv_queue(qentry->s.pktin, pkt_tbl, QUEUE_MULTI_MAX);

	if (pkts <= 0)
		return NULL;
	if (pkts > 1)
		queue_enq_multi(qentry,
			(odp_buffer_hdr_t **)&pkt_tbl[1], pkts - 1, 0);
	buf_hdr = (odp_buffer_hdr_t *)pkt_tbl[0];

	return buf_hdr;
}

int pktin_enq_multi(queue_entry_t *qentry ODP_UNUSED,
		    odp_buffer_hdr_t *buf_hdr[] ODP_UNUSED,
		    int num ODP_UNUSED, int sustain ODP_UNUSED)
{
	ODP_ABORT("attempted enqueue to a pktin queue");
	return 0;
}

int pktin_deq_multi(queue_entry_t *qentry, odp_buffer_hdr_t *buf_hdr[], int num)
{
	int nbr;
	odp_packet_t pkt_tbl[QUEUE_MULTI_MAX];
	int pkts, i;

	nbr = queue_deq_multi(qentry, buf_hdr, num);
	if (odp_unlikely(nbr > num))
		ODP_ABORT("queue_deq_multi req: %d, returned %d\n",
			num, nbr);

	/** queue already has number of requsted buffers,
	 *  do not do receive in that case.
	 */
	if (nbr == num)
		return nbr;

	pkts = odp_pktio_recv_queue(qentry->s.pktin, pkt_tbl, QUEUE_MULTI_MAX);
	if (pkts <= 0)
		return nbr;

	/* Fill in buf_hdr first */
	for (i = 0; i < pkts && nbr < num; i++, nbr++)
		buf_hdr[nbr] = (odp_buffer_hdr_t *)pkt_tbl[i];


	if (pkts - i)
		queue_enq_multi(qentry,
			(odp_buffer_hdr_t **)(pkt_tbl + i), pkts - i, 0);
	return nbr;
}

int pktin_poll(pktio_entry_t *entry, int num_queue, int index[])
{
	odp_packet_t pkt_tbl[QUEUE_MULTI_MAX];
	int num, idx;

	if (odp_unlikely(is_free(entry))) {
		ODP_ERR("Bad pktio entry\n");
		return -1;
	}

	/* Temporarely needed for odp_pktio_inq_remdef() */
	if (odp_unlikely(entry->s.num_in_queue == 0))
		return -1;

	if (entry->s.state == STATE_STOP)
		return 0;

	for (idx = 0; idx < num_queue; idx++) {
		queue_entry_t *qentry;
		odp_queue_t queue;
		odp_pktin_queue_t pktin = entry->s.in_queue[index[idx]].pktin;

		num = odp_pktio_recv_queue(pktin, pkt_tbl, QUEUE_MULTI_MAX);

		if (num == 0)
			continue;

		if (num < 0) {
			ODP_ERR("Packet recv error\n");
			return -1;
		}

		queue = entry->s.in_queue[index[idx]].queue;
		qentry = queue_to_qentry(queue);
		queue_enq_multi(qentry, (odp_buffer_hdr_t **)pkt_tbl, num, 0);
	}

	return 0;
}

int odp_pktio_mtu(odp_pktio_t id)
{
	pktio_entry_t *entry;
	int ret;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}
	ret = entry->s.ops->mtu_get(entry);

	unlock_entry(entry);
	return ret;
}

int odp_pktio_promisc_mode_set(odp_pktio_t id, odp_bool_t enable)
{
	pktio_entry_t *entry;
	int ret;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}
	if (entry->s.state != STATE_STOP) {
		unlock_entry(entry);
		return -1;
	}

	ret = entry->s.ops->promisc_mode_set(entry, enable);

	unlock_entry(entry);
	return ret;
}

int odp_pktio_promisc_mode(odp_pktio_t id)
{
	pktio_entry_t *entry;
	int ret;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	ret = entry->s.ops->promisc_mode_get(entry);
	unlock_entry(entry);

	return ret;
}

int odp_pktio_mac_addr(odp_pktio_t id, void *mac_addr, int addr_size)
{
	pktio_entry_t *entry;
	int ret = ETH_ALEN;

	if (addr_size < ETH_ALEN) {
		/* Output buffer too small */
		return -1;
	}

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	ret = entry->s.ops->mac_get(entry, mac_addr);
	unlock_entry(entry);

	return ret;
}

int odp_pktio_link_status(odp_pktio_t id)
{
	pktio_entry_t *entry;
	int ret = -1;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	if (entry->s.ops->link_status)
		ret = entry->s.ops->link_status(entry);
	unlock_entry(entry);

	return ret;
}

void odp_pktio_param_init(odp_pktio_param_t *params)
{
	memset(params, 0, sizeof(odp_pktio_param_t));
}

void odp_pktin_queue_param_init(odp_pktin_queue_param_t *param)
{
	memset(param, 0, sizeof(odp_pktin_queue_param_t));
	param->op_mode = ODP_PKTIO_OP_MT;
}

void odp_pktout_queue_param_init(odp_pktout_queue_param_t *param)
{
	memset(param, 0, sizeof(odp_pktout_queue_param_t));
	param->op_mode = ODP_PKTIO_OP_MT;
}

void odp_pktio_print(odp_pktio_t id)
{
	pktio_entry_t *entry;
	uint8_t addr[ETH_ALEN];
	int max_len = 512;
	char str[max_len];
	int len = 0;
	int n = max_len - 1;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %u does not exist\n",
				id->unused_dummy_var);
		return;
	}

	len += snprintf(&str[len], n - len,
			"pktio\n");
	len += snprintf(&str[len], n - len,
			"  handle       %" PRIu64 "\n", odp_pktio_to_u64(id));
	len += snprintf(&str[len], n - len,
			"  name         %s\n", entry->s.name);
	len += snprintf(&str[len], n - len,
			"  type         %s\n", entry->s.ops->name);
	len += snprintf(&str[len], n - len,
			"  state        %s\n",
			entry->s.state ==  STATE_START ? "start" :
		       (entry->s.state ==  STATE_STOP ? "stop" : "unknown"));
	memset(addr, 0, sizeof(addr));
	odp_pktio_mac_addr(id, addr, ETH_ALEN);
	len += snprintf(&str[len], n - len,
			"  mac          %02x:%02x:%02x:%02x:%02x:%02x\n",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	len += snprintf(&str[len], n - len,
			"  mtu          %d\n", odp_pktio_mtu(id));
	len += snprintf(&str[len], n - len,
			"  promisc      %s\n",
			odp_pktio_promisc_mode(id) ? "yes" : "no");
	str[len] = '\0';

	ODP_PRINT("\n%s\n", str);
}

int odp_pktio_term_global(void)
{
	int ret;
	int id;
	int pktio_if;

	for (id = 0; id < ODP_CONFIG_PKTIO_ENTRIES; ++id) {
		pktio_entry_t *pktio_entry;

		pktio_entry = &pktio_tbl->entries[id];

		ret = odp_queue_destroy(pktio_entry->s.outq_default);
		if (ret)
			ODP_ABORT("unable to destroy outq %s\n",
				  pktio_entry->s.name);

		if (is_free(pktio_entry))
			continue;

		lock_entry(pktio_entry);
		if (pktio_entry->s.state != STATE_STOP) {
			ret = _pktio_stop(pktio_entry);
			if (ret)
				ODP_ABORT("unable to stop pktio %s\n",
					  pktio_entry->s.name);
		}
		ret = _pktio_close(pktio_entry);
		if (ret)
			ODP_ABORT("unable to close pktio %s\n",
				  pktio_entry->s.name);
		unlock_entry(pktio_entry);
	}

	for (pktio_if = 0; pktio_if_ops[pktio_if]; ++pktio_if) {
		if (pktio_if_ops[pktio_if]->term)
			if (pktio_if_ops[pktio_if]->term())
				ODP_ABORT("failed to terminate pktio type %d",
					  pktio_if);
	}

	ret = odp_shm_free(odp_shm_lookup("odp_pktio_entries"));
	if (ret != 0)
		ODP_ERR("shm free failed for odp_pktio_entries");

	return ret;
}

int odp_pktio_capability(odp_pktio_t pktio,
			 odp_pktio_capability_t *capa)
{
	pktio_entry_t *entry;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
			pktio->unused_dummy_var);
		return -1;
	}

	if (entry->s.ops->capability)
		return entry->s.ops->capability(entry, capa);

	return single_capability(capa);
}

int odp_pktio_stats(odp_pktio_t pktio,
		    odp_pktio_stats_t *stats)
{
	pktio_entry_t *entry;
	int ret = -1;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	if (entry->s.ops->stats)
		ret = entry->s.ops->stats(entry, stats);
	unlock_entry(entry);

	return ret;
}

int odp_pktio_stats_reset(odp_pktio_t pktio)
{
	pktio_entry_t *entry;
	int ret = -1;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	lock_entry(entry);

	if (odp_unlikely(is_free(entry))) {
		unlock_entry(entry);
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	if (entry->s.ops->stats)
		ret = entry->s.ops->stats_reset(entry);
	unlock_entry(entry);

	return ret;
}

int odp_pktin_queue_config(odp_pktio_t pktio,
			   const odp_pktin_queue_param_t *param)
{
	pktio_entry_t *entry;
	odp_pktin_mode_t mode;
	odp_pktio_capability_t capa;
	unsigned num_queues;
	unsigned i;
	odp_queue_t queue;

	if (param == NULL) {
		ODP_DBG("no parameters\n");
		return -1;
	}

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	if (entry->s.state != STATE_STOP) {
		ODP_DBG("pktio %s: not stopped\n", entry->s.name);
		return -1;
	}

	mode = entry->s.param.in_mode;

	if (mode == ODP_PKTIN_MODE_DISABLED) {
		ODP_DBG("pktio %s: packet input is disabled\n",
				entry->s.name);
		return -1;
	}

	num_queues = param->num_queues;

	if (num_queues == 0) {
		ODP_DBG("pktio %s: zero input queues\n", entry->s.name);
		return -1;
	}

	odp_pktio_capability(pktio, &capa);

	if (num_queues > capa.max_input_queues) {
		ODP_DBG("pktio %s: too many input queues\n", entry->s.name);
		return -1;
	}

	/* If re-configuring, destroy old queues */
	if (entry->s.num_in_queue)
		destroy_in_queues(entry, entry->s.num_in_queue);

	for (i = 0; i < num_queues; i++) {
		if (mode == ODP_PKTIN_MODE_QUEUE ||
		    mode == ODP_PKTIN_MODE_SCHED) {
			odp_queue_param_t queue_param;

			memcpy(&queue_param, &param->queue_param,
			       sizeof(odp_queue_param_t));

			queue_param.type = ODP_QUEUE_TYPE_PLAIN;

			if (mode == ODP_PKTIN_MODE_SCHED)
				queue_param.type = ODP_QUEUE_TYPE_SCHED;

			queue = odp_queue_create("pktio_in",
						 &queue_param);

			if (queue == ODP_QUEUE_INVALID) {
				destroy_in_queues(entry, i + 1);
				return -1;
			}

			if (mode == ODP_PKTIN_MODE_QUEUE) {
				queue_entry_t *qentry;

				qentry = queue_to_qentry(queue);
				qentry->s.pktin.index  = i;
				qentry->s.pktin.pktio  = pktio;

				qentry->s.enqueue = pktin_enqueue;
				qentry->s.dequeue = pktin_dequeue;
				qentry->s.enqueue_multi = pktin_enq_multi;
				qentry->s.dequeue_multi = pktin_deq_multi;
			}

			entry->s.in_queue[i].queue = queue;
		} else {
			entry->s.in_queue[i].queue = ODP_QUEUE_INVALID;
		}

		entry->s.in_queue[i].pktin.index = i;
		entry->s.in_queue[i].pktin.pktio = entry->s.handle;
	}

	entry->s.num_in_queue = num_queues;

	if (entry->s.ops->input_queues_config)
		return entry->s.ops->input_queues_config(entry, param);

	return 0;
}

int odp_pktout_queue_config(odp_pktio_t pktio,
			    const odp_pktout_queue_param_t *param)
{
	pktio_entry_t *entry;
	odp_pktout_mode_t mode;
	odp_pktio_capability_t capa;
	unsigned num_queues;
	unsigned i;

	if (param == NULL) {
		ODP_DBG("no parameters\n");
		return -1;
	}

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	if (entry->s.state != STATE_STOP) {
		ODP_DBG("pktio %s: not stopped\n", entry->s.name);
		return -1;
	}

	mode = entry->s.param.out_mode;

	if (mode == ODP_PKTOUT_MODE_DISABLED) {
		ODP_DBG("pktio %s: packet output is disabled\n",
				entry->s.name);
		return -1;
	}

	if (mode != ODP_PKTOUT_MODE_DIRECT) {
		ODP_DBG("pktio %s: bad packet output mode\n",
			entry->s.name);
		return -1;
	}

	num_queues = param->num_queues;

	if (num_queues == 0) {
		ODP_DBG("pktio %s: zero output queues\n",
			entry->s.name);
		return -1;
	}

	odp_pktio_capability(pktio, &capa);

	if (num_queues > capa.max_output_queues) {
		ODP_DBG("pktio %s: too many output queues\n",
			entry->s.name);
		return -1;
	}

	for (i = 0; i < num_queues; i++) {
		entry->s.out_queue[i].pktout.index = i;
		entry->s.out_queue[i].pktout.pktio = entry->s.handle;
	}

	entry->s.num_out_queue = num_queues;

	if (entry->s.ops->output_queues_config)
		return entry->s.ops->output_queues_config(entry, param);

	return 0;
}

int odp_pktin_event_queue(odp_pktio_t pktio,
			odp_queue_t queues[], int num)
{
	pktio_entry_t *entry;
	odp_pktin_mode_t mode;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	mode = entry->s.param.in_mode;

	if (mode != ODP_PKTIN_MODE_QUEUE &&
	    mode != ODP_PKTIN_MODE_SCHED)
		return -1;

	if (entry->s.ops->in_queues)
		return entry->s.ops->in_queues(entry, queues, num);

	return single_in_queues(entry, queues, num);
}

int odp_pktin_queue(odp_pktio_t pktio, odp_pktin_queue_t queues[], int num)
{
	pktio_entry_t *entry;
	odp_pktin_mode_t mode;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
				pktio->unused_dummy_var);
		return -1;
	}

	mode = entry->s.param.in_mode;

	if (mode != ODP_PKTIN_MODE_DIRECT)
		return -1;

	if (entry->s.ops->pktin_queues)
		return entry->s.ops->pktin_queues(entry, queues, num);

	return single_pktin_queues(entry, queues, num);
}

int odp_pktout_queue(odp_pktio_t pktio, odp_pktout_queue_t queues[], int num)
{
	pktio_entry_t *entry;
	odp_pktout_mode_t mode;

	entry = get_pktio_entry(pktio);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
			pktio->unused_dummy_var);
		return -1;
	}

	mode = entry->s.param.out_mode;

	if (mode != ODP_PKTOUT_MODE_DIRECT)
		return -1;

	if (entry->s.ops->pktout_queues)
		return entry->s.ops->pktout_queues(entry, queues, num);

	return single_pktout_queues(entry, queues, num);
}

int odp_pktio_recv_queue(odp_pktin_queue_t queue, odp_packet_t packets[],
			 int num)
{
	pktio_entry_t *entry;
	odp_pktio_t pktio = queue.pktio;

	entry = get_pktio_entry(pktio);

	ODP_ASSERT(entry != NULL);

	if (entry->s.ops->recv_queue)
		return entry->s.ops->recv_queue(entry, queue.index,
						packets, num);

	return single_recv_queue(entry, queue.index, packets, num);
}

int odp_pktio_send_queue(odp_pktout_queue_t queue, odp_packet_t packets[],
			 int num)
{
	pktio_entry_t *entry;
	odp_pktio_t pktio = queue.pktio;

	entry = get_pktio_entry(pktio);

	ODP_ASSERT(entry != NULL);

	if (entry->s.ops->send_queue)
		return entry->s.ops->send_queue(entry, queue.index,
						packets, num);

	return single_send_queue(entry, queue.index, packets, num);
}

int single_capability(odp_pktio_capability_t *capa)
{
	memset(capa, 0, sizeof(odp_pktio_capability_t));
	capa->max_input_queues  = 1;
	capa->max_output_queues = 1;

	return 0;
}

int single_in_queues(pktio_entry_t *entry, odp_queue_t queues[], int num)
{
	if (queues && num > 0)
		queues[0] = entry->s.in_queue[0].queue;

	return 1;
}

int single_pktin_queues(pktio_entry_t *entry, odp_pktin_queue_t queues[],
			int num)
{
	if (queues && num > 0)
		queues[0] = entry->s.in_queue[0].pktin;

	return 1;
}

int single_pktout_queues(pktio_entry_t *entry, odp_pktout_queue_t queues[],
			 int num)
{
	if (queues && num > 0)
		queues[0] = entry->s.out_queue[0].pktout;

	return 1;
}

int single_recv_queue(pktio_entry_t *entry, int index, odp_packet_t packets[],
		      int num)
{
	(void)index;
	return odp_pktio_recv(entry->s.handle, packets, num);
}

int single_send_queue(pktio_entry_t *entry, int index, odp_packet_t packets[],
		      int num)
{
	(void)index;
	return odp_pktio_send(entry->s.handle, packets, num);
}
