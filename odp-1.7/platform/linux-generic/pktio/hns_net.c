/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include <linux/ethtool.h>
#include <linux/sockios.h>

#include <odp_pkt_uio.h>
#include <net/if.h>
#include <odp_debug_internal.h>
#include <odp_packet_io_internal.h>
#include <odp_ether.h>
#include <odp_ethdev.h>
#include <odp_classification_internal.h>
#include "odp_common.h"

#define PKTIO_DEV_NAME "pktio_"

static uint16_t nb_rxd = ODP_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = ODP_TEST_TX_DESC_DEFAULT;

static const struct odp_eth_conf port_conf = {
	.rxmode			 = {
		.mq_mode	 = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ODP_ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split	 = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0,  /**< IP checksum offload disabled */
		.hw_vlan_filter = 0,  /**< VLAN filtering disabled */
		.jumbo_frame  = 0,    /**< Jumbo Frame Support disabled */
		.hw_strip_crc = 0,    /**< CRC stripped by hardware */
	},
	.rx_adv_conf		 = {
		.rss_conf	 = {
			.rss_key = NULL,
			.rss_hf	 = ETH_RSS_IPV4 | ETH_RSS_IPV6,
		},
	},
	.txmode			 = {
		.mq_mode	 = ETH_MQ_TX_NONE,
	},
};

static const struct odp_eth_rxconf rx_conf = {
	.rx_thresh	 = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
};

static const struct odp_eth_txconf tx_conf = {
	.tx_thresh	 = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh	 = 0, /* Use UMD default values */
	.tx_rs_thresh	 = 0, /* Use UMD default values */

	/*
	 * As the example won't handle mult-segments and offload cases,
	 * set the flag by default.
	 */
	.txq_flags	 = ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOOFFLOADS,
};

/* Test if s has only digits or not.*/
static int str_is_digital_number(const char *s)
{
	while (*s) {
		if (!isdigit(*s))
			return 0;

		s++;
	}

	return 1;
}

static int _odp_netdev_is_valid(const char *netdev)
{
	if ((strncmp(netdev, PKTIO_DEV_NAME, strlen(PKTIO_DEV_NAME)) != 0) ||
	    (strlen(netdev) <= strlen(PKTIO_DEV_NAME)) ||
	    !str_is_digital_number(netdev + strlen(PKTIO_DEV_NAME)))
		return 0;

	return 1;
}

int setup_pkt_odp(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		  const char *netdev, odp_pool_t pool)
{
	uint8_t	portid = 0;
	uint16_t nbrxq = 0;
	uint16_t nbtxq = 0;
	int ret, i;
	struct odp_eth_dev *dev;

	pkt_odp_t *const pkt_odp = &pktio_entry->s.pkt_odp;

	if (!_odp_netdev_is_valid(netdev)) {
		ODP_ERR("netdev %s, format err! should be "
			"pktio_x! x is digital number\n", netdev);
		return -1;
	}

	portid = atoi(netdev + strlen(PKTIO_DEV_NAME));
	pkt_odp->portid = portid;
	pkt_odp->pool = pool;
	pkt_odp->queueid = 0;

	/* On init set it up only to 1 rx and tx queue.*/

	dev = odp_eth_dev_allocated_id(portid);
	if (!dev) {
		ODP_ERR("netdev %s, odp_eth_dev_allocated_id err!\n", netdev);
		return -1;
	}

	nbtxq = dev->q_num;
	nbrxq = dev->q_num;

	ret = odp_eth_dev_configure(portid, nbrxq, nbtxq, &port_conf);
	if (ret < 0) {
		ODP_ERR("Cannot configure device: err=%d, port=%u\n",
			ret, (unsigned int)portid);
		return -1;
	}

	/* init one RX queue on each port */
	for (i = 0; i < nbrxq; i++) {
		ret = odp_eth_rx_queue_setup(portid, i, nb_rxd,
					     0, NULL/*&rx_conf*/, (void *)pool);
		if (ret < 0) {
			ODP_ERR("rxq:err=%d, port=%u\n",
				ret, (unsigned int)portid);
			return -1;
		}
	}

	/* init one TX queue on each port */
	for (i = 0; i < nbtxq; i++) {
		ret = odp_eth_tx_queue_setup(portid, i, nb_txd,
					     0, NULL/*&tx_conf*/, (void *)pool);
		if (ret < 0) {
			ODP_ERR("txq:err=%d, port=%u\n",
				ret, (unsigned int)portid);
			return -1;
		}
	}

	/* Start device */
	ret = odp_eth_dev_start(portid);
	if (ret < 0) {
		ODP_ERR("odp_eth_dev_start:err=%d, port=%u\n",
			ret, (unsigned int)portid);
		return -1;
	}

	return 0;
}

int close_pkt_odp(pktio_entry_t *pktio_entry)
{
	ODP_DBG("close pkt_odp, %u\n", pktio_entry->s.pkt_odp.portid);

	return 0;
}

int odp_eth_rx_burst(pktio_entry_t *pktio_entry, odp_packet_t *rx_pkts,
		     unsigned int nb_pkts)
{
	int nb_rx;
	struct odp_eth_dev *dev;
	uint8_t	port_id	= pktio_entry->s.pkt_odp.portid;
	uint16_t queue_id = pktio_entry->s.pkt_odp.queueid;

	dev = &odp_eth_devices[port_id];

	if (pktio_cls_enabled(pktio_entry, queue_id)) {
		odp_packet_t tmpbuf[64];
		odp_packet_t onepkt;
		odp_packet_hdr_t *pkt_hdr;
		odp_pktio_t id;
		int i, j;

		if (nb_pkts > 64)
			nb_pkts = 64;

		nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id],
					     (void **)tmpbuf, nb_pkts);
		if (odp_unlikely(nb_rx <= 0))
			return nb_rx;

		id = pktio_entry->s.handle;
		for (i = 0, j = 0; i < nb_rx; i++) {
			onepkt = tmpbuf[i];
			pkt_hdr = odp_packet_hdr(onepkt);
			pkt_hdr->input = id;
			packet_parse_reset(pkt_hdr);
			packet_parse_l2(pkt_hdr);
			if (0 > _odp_packet_classifier(pktio_entry,
						       queue_id, onepkt)) {
				rx_pkts[j++] = onepkt;
			}
		}

		nb_rx = j;
	} else {
		nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id],
					     (void **)rx_pkts, nb_pkts);
	}

#ifdef ODP_ETHDEV_RXTX_CALLBACKS
	struct odp_eth_rxtx_callback *cb = dev->post_rx_burst_cbs[queue_id];

	if (odp_unlikely(cb)) {
		do {
			nb_rx = cb->fn.rx(port_id, queue_id,
					  (void **)rx_pkts, nb_rx, nb_pkts,
					  cb->param);
			cb = cb->next;
		} while (cb);
	}
#endif

	return nb_rx;
}

int odp_eth_tx_burst(pktio_entry_t *pktio_entry,
		     odp_packet_t *tx_pkts, unsigned nb_pkts)
{
	struct odp_eth_dev *dev;
	uint8_t	port_id	= pktio_entry->s.pkt_odp.portid;
	uint16_t queue_id = pktio_entry->s.pkt_odp.queueid;

	dev = &odp_eth_devices[port_id];

#ifdef ODP_ETHDEV_RXTX_CALLBACKS
	struct odp_eth_rxtx_callback *cb = dev->pre_tx_burst_cbs[queue_id];

	if (odp_unlikely(cb)) {
		do {
			nb_pkts = cb->fn.tx(port_id, queue_id,
					    (void **)tx_pkts, nb_pkts,
					    cb->param);
			cb = cb->next;
		} while (cb);
	}
#endif
	return (*dev->tx_pkt_burst)(dev->data->tx_queues[queue_id],
				    (void **)tx_pkts, nb_pkts);
}

int odp_queue_rx_burst(pktio_entry_t *pktio_entry, int q_index,
		       odp_packet_t *rx_pkts, int nb_pkts)
{
	int nb_rx;
	struct odp_eth_dev *dev;
	uint8_t	port_id	= pktio_entry->s.pkt_odp.portid;

	dev = &odp_eth_devices[port_id];

	if (pktio_cls_enabled(pktio_entry, q_index)) {
		odp_packet_t tmpbuf[64];
		odp_packet_t onepkt;
		odp_packet_hdr_t *pkt_hdr;
		odp_pktio_t id;
		int i, j;

		if (nb_pkts > 64)
			nb_pkts = 64;

		nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[q_index],
						(void **)tmpbuf, nb_pkts);
		if (odp_unlikely(nb_rx <= 0))
			return nb_rx;

		id = pktio_entry->s.handle;
		for (i = 0, j = 0; i < nb_rx; i++) {
			onepkt = tmpbuf[i];
			pkt_hdr = odp_packet_hdr(onepkt);
			pkt_hdr->input = id;
			packet_parse_reset(pkt_hdr);
			packet_parse_l2(pkt_hdr);
			if (0 > _odp_packet_classifier(pktio_entry,
						       q_index, onepkt)) {
				rx_pkts[j++] = onepkt;
			}
		}

		nb_rx = j;
	} else {
		nb_rx = (*dev->rx_pkt_burst)(dev->data->rx_queues[q_index],
					     (void **)rx_pkts, nb_pkts);
	}

#ifdef ODP_ETHDEV_RXTX_CALLBACKS
	struct odp_eth_rxtx_callback *cb = dev->post_rx_burst_cbs[q_index];

	if (odp_unlikely(cb)) {
		do {
			nb_rx = cb->fn.rx(port_id, q_index,
					  (void **)rx_pkts, nb_rx,
					  nb_pkts, cb->param);
			cb = cb->next;
		} while (cb);
	}
#endif

	return nb_rx;
}

int odp_queue_tx_burst(pktio_entry_t *pktio_entry, int q_index,
		       odp_packet_t *tx_pkts, int nb_pkts)
{
	struct odp_eth_dev *dev;
	uint8_t	port_id = pktio_entry->s.pkt_odp.portid;

	dev = &odp_eth_devices[port_id];

#ifdef ODP_ETHDEV_RXTX_CALLBACKS
	struct odp_eth_rxtx_callback *cb = dev->pre_tx_burst_cbs[q_index];

	if (odp_unlikely(cb)) {
		do {
			nb_pkts = cb->fn.tx(port_id, q_index,
					    (void **)tx_pkts, nb_pkts,
					    cb->param);
			cb = cb->next;
		} while (cb);
	}
#endif
	return (*dev->tx_pkt_burst)(dev->data->tx_queues[q_index],
				    (void **)tx_pkts, nb_pkts);
}

int odp_eth_mtu_get(pktio_entry_t *pktio_entry)
{
	uint8_t port_id = pktio_entry->s.pkt_odp.portid;
	uint16_t mtu;
	int ret;

	if (!odp_eth_dev_is_valid_port(port_id))
		return -1;

	ret = odp_eth_dev_get_mtu(port_id, &mtu);
	if (ret) {
		ODP_DBG("port_id %d get mtu failed!\n", port_id);
		return -1;
	}

	return (int)mtu;
}

int odp_eth_mac_get(pktio_entry_t *pktio_entry, void *mac_addr)
{
	uint8_t port_id = pktio_entry->s.pkt_odp.portid;

	if (!odp_eth_dev_is_valid_port(port_id))
		return -1;

	(void)odp_eth_get_macaddr(port_id, mac_addr);
	return ODPH_ETHADDR_LEN;
}

int odp_eth_promisc_mode_set(pktio_entry_t *pktio_entry, int enable)
{
	uint8_t port_id = pktio_entry->s.pkt_odp.portid;

	if (!odp_eth_dev_is_valid_port(port_id))
		return -1;

	if (enable)
		(void)odp_eth_promiscuous_enable(port_id);
	else
		(void)odp_eth_promiscuous_disable(port_id);

	return 0;
}

int odp_eth_promisc_mode_get(pktio_entry_t *pktio_entry)
{
	uint8_t port_id = pktio_entry->s.pkt_odp.portid;

	if (!odp_eth_dev_is_valid_port(port_id))
		return -1;

	return odp_eth_promiscuous_get(port_id);
}

const pktio_if_ops_t hns_eth_pktio_ops = {
	.init  = NULL,
	.term  = NULL,
	.open  = setup_pkt_odp,
	.close = close_pkt_odp,
	.recv  = odp_eth_rx_burst,
	.send  = odp_eth_tx_burst,
	.mtu_get	  = odp_eth_mtu_get,
	.promisc_mode_set = odp_eth_promisc_mode_set,
	.promisc_mode_get = odp_eth_promisc_mode_get,
	.mac_get	  = odp_eth_mac_get,
	.recv_queue = odp_queue_rx_burst,
	.send_queue = odp_queue_tx_burst,
};

int odp_pktio_dev_get(struct odp_pktio_info *pktio_info, uint8_t *num)
{
	uint8_t port_num = 0;
	uint8_t i;
	struct odp_eth_dev_info dev_info;

	if (!pktio_info || !num) {
		ODP_DBG("parameter pktio_info or num is NULL!\n");
		return -1;
	}

	for (i = 0; i < ODP_MAX_ETHPORTS; i++) {
		if (odp_eth_dev_is_valid_port(i)) {
			memset(pktio_info[i].name, 0,
			       sizeof(PACKET_IO_NAME_LENGTH_MAX));
			(void)sprintf(pktio_info[i].name, "pktio_%d", i);

			odp_eth_dev_info_get(i, &dev_info);
			if (dev_info.pci_dev) { /* pci netif */
				pktio_info[port_num].if_type
					= ODP_PKITIO_DEV_TYPE_PCI;
				pktio_info[port_num].info.pci_info.addr.domain
					= dev_info.pci_dev->addr.domain;
				pktio_info[port_num].info.pci_info.addr.bus
					= dev_info.pci_dev->addr.bus;
				pktio_info[port_num].info.pci_info.addr.devid
					= dev_info.pci_dev->addr.devid;
				pktio_info[port_num].info.pci_info.addr.function
					= dev_info.pci_dev->addr.function;
				pktio_info[port_num].info.pci_info.id.vendor_id
					= dev_info.pci_dev->id.vendor_id;
				pktio_info[port_num].info.pci_info.id.device_id
					= dev_info.pci_dev->id.device_id;
				pktio_info[port_num].info.pci_info.id
					.subsystem_vendor_id = dev_info
					.pci_dev->id.subsystem_vendor_id;
				pktio_info[port_num].info.pci_info.id
					.subsystem_device_id = dev_info
					.pci_dev->id.subsystem_device_id;
				pktio_info[port_num].info.pci_info.numa_node
					= dev_info.pci_dev->numa_node;
			} else { /* soc netif */
				pktio_info[port_num].if_type
					= ODP_PKITIO_DEV_TYPE_SOC;
				pktio_info[port_num].info.soc_info.if_idx
					= dev_info.if_index;
				/* checkpatch ERROR: do not initialise
				globals to 0 or NULL */
				/* pktio_info[port_num].info.soc_info.numa_node
					= 0;*/ /* not used now  */
			}
			port_num++;
		}
	}

	*num = port_num;
	return 0;
}

static int odp_pktio_is_not_hns_eth(pktio_entry_t *entry)
{
	return ((entry)->s.ops != &hns_eth_pktio_ops);
}

int odp_pktio_restart(odp_pktio_t id)
{
	pktio_entry_t *entry;
	uint8_t port_id;
	int ret;

	entry = get_pktio_entry(id);
	if (entry == NULL) {
		ODP_DBG("pktio entry %d does not exist\n",
			id->unused_dummy_var);
		return -1;
	}

	if (odp_unlikely(is_free(entry))) {
		ODP_DBG("already freed pktio\n");
		return -1;
	}

	if (odp_pktio_is_not_hns_eth(entry)) {
		ODP_DBG("pktio entry %d is not ODP UMD pktio\n",
			id->unused_dummy_var);
		return -1;
	}

	port_id = entry->s.pkt_odp.portid;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_DBG("pktio entry %d ODP UMD Invalid port_id=%d\n",
			id->unused_dummy_var, port_id);
		return -1;
	}

	/* Stop device */
	odp_eth_dev_stop(port_id);

	/* Start device */
	ret = odp_eth_dev_start(port_id);
	if (ret < 0) {
		ODP_ERR("odp_eth_dev_start:err=%d, port=%u\n",
			ret, (unsigned)port_id);
		return -1;
	}

	ODP_DBG("odp pmd restart done\n\n");

	return 0;
}
