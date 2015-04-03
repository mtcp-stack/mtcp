/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2009 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


/******************************************************************************
 Copyright (c)2006 - 2007 Myricom, Inc. for some LRO specific code
******************************************************************************/
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#include <linux/inetdevice.h>
#ifdef NETIF_F_TSO
#include <net/checksum.h>
#ifdef NETIF_F_TSO6
#include <net/ip6_checksum.h>
#endif
#endif
#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif
#ifdef NETIF_F_HW_VLAN_TX
#include <linux/if_vlan.h>
#endif


#include "ixgbe.h"
#include "../include/ps.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/* disable features we don't need any more... - adaline*/
#define IXGBE_NO_LRO	1
#define IXGBE_NO_HW_RSC 1


char ixgbe_driver_name[] = "ixgbe";
static const char ixgbe_driver_string[] =
	"Intel(R) 10 Gigabit PCI Express Network Driver";
#define DRV_HW_PERF

#ifndef CONFIG_IXGBE_NAPI
#define DRIVERNAPI
#else
#define DRIVERNAPI "-NAPI"
#endif

#define FPGA

#define DRV_VERSION "2.0.38.2" DRIVERNAPI DRV_HW_PERF FPGA
const char ixgbe_driver_version[] = DRV_VERSION;
static char ixgbe_copyright[] = "Copyright (c) 1999-2009 Intel Corporation.";
/* ixgbe_pci_tbl - PCI Device ID Table
 *
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, private data (not used) }
 */
static struct pci_device_id ixgbe_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598_BX)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598AF_DUAL_PORT)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598AF_SINGLE_PORT)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598AT)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598AT2)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598EB_CX4)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598_CX4_DUAL_PORT)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598_DA_DUAL_PORT)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598EB_XF_LR)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82598EB_SFP_LOM)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82599_KX4)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82599_XAUI_LOM)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82599_SFP)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IXGBE_DEV_ID_82599_T3_LOM)},
	/* required last entry */
	{0, }
};
MODULE_DEVICE_TABLE(pci, ixgbe_pci_tbl);

#ifdef IXGBE_DCA
static int ixgbe_notify_dca(struct notifier_block *, unsigned long event,
                            void *p);
static struct notifier_block dca_notifier = {
	.notifier_call = ixgbe_notify_dca,
	.next          = NULL,
	.priority      = 0
};

#endif
MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) 10 Gigabit PCI Express Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#define DEFAULT_DEBUG_LEVEL_SHIFT 3

static void ixgbe_release_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware take over control of h/w */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
	                ctrl_ext & ~IXGBE_CTRL_EXT_DRV_LOAD);
}

static void ixgbe_get_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
	                ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

/*
 * ixgbe_set_ivar - set the IVAR registers, mapping interrupt causes to vectors
 * @adapter: pointer to adapter struct
 * @direction: 0 for Rx, 1 for Tx, -1 for other causes
 * @queue: queue to map the corresponding interrupt to
 * @msix_vector: the vector to map to the corresponding queue
 *
 */
static void ixgbe_set_ivar(struct ixgbe_adapter *adapter, s8 direction,
	                   u8 queue, u8 msix_vector)
{
	u32 ivar, index;
	struct ixgbe_hw *hw = &adapter->hw;
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		msix_vector |= IXGBE_IVAR_ALLOC_VAL;
		if (direction == -1)
			direction = 0;
		index = (((direction * 64) + queue) >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (queue & 0x3)));
		ivar |= (msix_vector << (8 * (queue & 0x3)));
		IXGBE_WRITE_REG(hw, IXGBE_IVAR(index), ivar);
		break;
	case ixgbe_mac_82599EB:
		if (direction == -1) {
			/* other causes */
			msix_vector |= IXGBE_IVAR_ALLOC_VAL;
			index = ((queue & 1) * 8);
			ivar = IXGBE_READ_REG(&adapter->hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (msix_vector << index);
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR_MISC, ivar);
			break;
		} else {
			/* tx or rx causes */
			msix_vector |= IXGBE_IVAR_ALLOC_VAL;
			index = ((16 * (queue & 1)) + (8 * direction));
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(queue >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (msix_vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(queue >> 1), ivar);
			break;
		}
	default:
		break;
	}
}

static inline void ixgbe_irq_rearm_queues(struct ixgbe_adapter *adapter,
                                          u64 qmask)
{
	u32 mask;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(1), mask);
	}
}

static inline bool ixgbe_check_tx_hang(struct ixgbe_adapter *adapter,
                                       struct ixgbe_ring *tx_ring,
                                       unsigned int eop)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 head, tail;

	/* Detect a transmit hang in hardware, this serializes the
	 * check with the clearing of time_stamp and movement of eop */
	head = IXGBE_READ_REG(hw, tx_ring->head);
	tail = IXGBE_READ_REG(hw, tx_ring->tail);
	adapter->detect_tx_hung = false;
	if ((head != tail) &&
	    tx_ring->tx_buffer_info[eop].time_stamp &&
	    time_after(jiffies, tx_ring->tx_buffer_info[eop].time_stamp + HZ) &&
	    !(IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF)) {
		/* detected Tx unit hang */
		union ixgbe_adv_tx_desc *tx_desc;
		tx_desc = IXGBE_TX_DESC_ADV(*tx_ring, eop);
		DPRINTK(DRV, ERR, "Detected Tx Unit Hang\n"
			"  Tx Queue             <%d>\n"
			"  TDH, TDT             <%x>, <%x>\n"
			"  next_to_use          <%x>\n"
			"  next_to_clean        <%x>\n"
			"tx_buffer_info[next_to_clean]\n"
			"  time_stamp           <%lx>\n"
			"  jiffies              <%lx>\n",
			tx_ring->queue_index,
			head, tail,
			tx_ring->next_to_use, eop,
			tx_ring->tx_buffer_info[eop].time_stamp, jiffies);
		return true;
	}

	return false;
}

#define IXGBE_MAX_TXD_PWR	14
#define IXGBE_MAX_DATA_PER_TXD	(1 << IXGBE_MAX_TXD_PWR)

/* Tx Descriptors needed, worst case */
#define TXD_USE_COUNT(S) (((S) >> IXGBE_MAX_TXD_PWR) + \
			 (((S) & (IXGBE_MAX_DATA_PER_TXD - 1)) ? 1 : 0))
#ifdef MAX_SKB_FRAGS
#define DESC_NEEDED (TXD_USE_COUNT(IXGBE_MAX_DATA_PER_TXD) /* skb->data */ + \
	MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE) + 1)      /* for context */
#else
#define DESC_NEEDED TXD_USE_COUNT(IXGBE_MAX_DATA_PER_TXD)
#endif

static void ixgbe_tx_timeout(struct net_device *netdev);

/**
 * ixgbe_clean_tx_irq - Reclaim resources after transmit completes
 * @q_vector: structure containing interrupt and ring information
 * @tx_ring: tx ring to clean
 **/
static bool ixgbe_clean_tx_irq(struct ixgbe_adapter *adapter,
                               struct ixgbe_ring *tx_ring,
                               int *work_done, int work_to_do)
{
	struct net_device *netdev = adapter->netdev;
	unsigned int i, head, count = 0;

	head = IXGBE_READ_REG(&adapter->hw, tx_ring->head);
	i = tx_ring->next_to_clean;

	if (i <= head)
		count = head - i;
	else
		count = tx_ring->count - i + head;

	(*work_done) += count;
	tx_ring->next_to_clean = head;

#define TX_WAKE_THRESHOLD (DESC_NEEDED * 2)
	if (unlikely(count && netif_carrier_ok(netdev) &&
	             (IXGBE_DESC_UNUSED(tx_ring) >= TX_WAKE_THRESHOLD))) {
		/* Make sure that anybody stopping the queue after this
		 * sees the new next_to_clean.
		 */
		smp_mb();
#ifdef HAVE_TX_MQ
		if (__netif_subqueue_stopped(netdev, tx_ring->queue_index) &&
		    !test_bit(__IXGBE_DOWN, &adapter->state)) {
			netif_wake_subqueue(netdev, tx_ring->queue_index);
			++adapter->restart_queue;
		}
#else
		if (netif_queue_stopped(netdev) &&
		    !test_bit(__IXGBE_DOWN, &adapter->state)) {
			netif_wake_queue(netdev);
			++adapter->restart_queue;
		}
#endif
	}

#if 0
	if (adapter->detect_tx_hung) {
		if (ixgbe_check_tx_hang(adapter, tx_ring, i)) {
			/* schedule immediate reset if we believe we hung */
			DPRINTK(PROBE, INFO,
			        "tx hang %d detected, resetting adapter\n",
			        adapter->tx_timeout_count + 1);
			ixgbe_tx_timeout(adapter->netdev);
		}
	}
#endif

	return count > 0;
}

#ifdef IXGBE_DCA
static void ixgbe_update_rx_dca(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *rx_ring)
{
	u32 rxctrl;
	int cpu = get_cpu();
	int q = rx_ring - adapter->rx_ring;
	struct ixgbe_hw *hw = &adapter->hw;

	if (rx_ring->cpu != cpu) {
		rxctrl = IXGBE_READ_REG(hw, IXGBE_DCA_RXCTRL(q));
		if (hw->mac.type == ixgbe_mac_82598EB) {
			rxctrl &= ~IXGBE_DCA_RXCTRL_CPUID_MASK;
			rxctrl |= dca3_get_tag(&adapter->pdev->dev, cpu);
		} else if (hw->mac.type == ixgbe_mac_82599EB) {
			rxctrl &= ~IXGBE_DCA_RXCTRL_CPUID_MASK_82599;
			rxctrl |= (dca3_get_tag(&adapter->pdev->dev, cpu) <<
			                    IXGBE_DCA_RXCTRL_CPUID_SHIFT_82599);
		}
		rxctrl |= IXGBE_DCA_RXCTRL_DESC_DCA_EN;
		rxctrl |= IXGBE_DCA_RXCTRL_HEAD_DCA_EN;
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED_DATA) {
			/* just do the header data when in Packet Split mode */
			if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED)
				rxctrl |= IXGBE_DCA_RXCTRL_HEAD_DCA_EN;
			else
				rxctrl |= IXGBE_DCA_RXCTRL_DATA_DCA_EN;
		}
		rxctrl &= ~(IXGBE_DCA_RXCTRL_DESC_RRO_EN);
		rxctrl &= ~(IXGBE_DCA_RXCTRL_DESC_WRO_EN |
		            IXGBE_DCA_RXCTRL_DESC_HSRO_EN);
		IXGBE_WRITE_REG(hw, IXGBE_DCA_RXCTRL(q), rxctrl);
		rx_ring->cpu = cpu;
	}
	put_cpu();
}

static void ixgbe_update_tx_dca(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *tx_ring)
{
	u32 txctrl;
	int cpu = get_cpu();
	int q = tx_ring - adapter->tx_ring;
	struct ixgbe_hw *hw = &adapter->hw;

	if (tx_ring->cpu != cpu) {
		if (hw->mac.type == ixgbe_mac_82598EB) {
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(q));
			txctrl &= ~IXGBE_DCA_TXCTRL_CPUID_MASK;
			txctrl |= dca3_get_tag(&adapter->pdev->dev, cpu);
			txctrl |= IXGBE_DCA_TXCTRL_DESC_DCA_EN;
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(q), txctrl);
		} else if (hw->mac.type == ixgbe_mac_82599EB) {
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(q));
			txctrl &= ~IXGBE_DCA_TXCTRL_CPUID_MASK_82599;
			txctrl |= (dca3_get_tag(&adapter->pdev->dev, cpu) <<
			                    IXGBE_DCA_TXCTRL_CPUID_SHIFT_82599);
			txctrl |= IXGBE_DCA_TXCTRL_DESC_DCA_EN;
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(q), txctrl);
		}
		tx_ring->cpu = cpu;
	}
	put_cpu();
}

static void ixgbe_setup_dca(struct ixgbe_adapter *adapter)
{
	int i;

	if (!(adapter->flags & IXGBE_FLAG_DCA_ENABLED))
		return;

	/* Always use CB2 mode, difference is masked in the CB driver. */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 2);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		adapter->tx_ring[i].cpu = -1;
		ixgbe_update_tx_dca(adapter, &adapter->tx_ring[i]);
	}
	for (i = 0; i < adapter->num_rx_queues; i++) {
		adapter->rx_ring[i].cpu = -1;
		ixgbe_update_rx_dca(adapter, &adapter->rx_ring[i]);
	}
}

static int __ixgbe_notify_dca(struct device *dev, void *data)
{
	struct net_device *netdev = dev_get_drvdata(dev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	unsigned long event = *(unsigned long *)data;

	switch (event) {
	case DCA_PROVIDER_ADD:
		/* if we're already enabled, don't do it again */
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			break;
		if (dca_add_requester(dev) == 0) {
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_dca(adapter);
			break;
		}
		/* Fall Through since DCA is disabled. */
	case DCA_PROVIDER_REMOVE:
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXGBE_FLAG_DCA_ENABLED;
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 1);
		}
		break;
	}

	return 0;
}

#endif /* IXGBE_DCA */

#if 0

/**
 * ixgbe_receive_skb - Send a completed packet up the stack
 * @q_vector: structure containing interrupt and ring information
 * @skb: packet to send up
 * @vlan_tag: vlan tag for packet
 **/

static void ixgbe_receive_skb(struct ixgbe_q_vector *q_vector,
                              struct sk_buff *skb, u16 vlan_tag)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	int ret;

	dev_kfree_skb(skb);
	return;

#ifdef CONFIG_IXGBE_NAPI
		if (!(adapter->flags & IXGBE_FLAG_IN_NETPOLL)) {
#ifdef NETIF_F_HW_VLAN_TX
			if (adapter->vlgrp && vlan_tag)
				vlan_gro_receive(&q_vector->napi,
						 adapter->vlgrp,
				                 vlan_tag, skb);
			else
				napi_gro_receive(&q_vector->napi, skb);
#else
			napi_gro_receive(&q_vector->napi, skb);
#endif
		} else {
#endif /* CONFIG_IXGBE_NAPI */

#ifdef NETIF_F_HW_VLAN_TX
			if (adapter->vlgrp && vlan_tag)
				ret = vlan_hwaccel_rx(skb, adapter->vlgrp,
				                      vlan_tag);
			else
				ret = netif_rx(skb);
#else
			ret = netif_rx(skb);
#endif
#ifndef CONFIG_IXGBE_NAPI
			if (ret == NET_RX_DROP)
				adapter->rx_dropped_backlog++;
#endif
#ifdef CONFIG_IXGBE_NAPI
		}
#endif /* CONFIG_IXGBE_NAPI */
}

#endif

/**
 * ixgbe_rx_checksum - indicate in skb if hw indicated a good cksum
 * @adapter: address of board private structure
 * @rx_desc: current Rx descriptor being processed
 * @skb: skb currently being received and modified
 **/
static inline void ixgbe_rx_checksum(struct ixgbe_adapter *adapter,
                                     union ixgbe_adv_rx_desc *rx_desc,
                                     struct sk_buff *skb)
{
	u32 status_err = le32_to_cpu(rx_desc->wb.upper.status_error);
	skb->ip_summed = CHECKSUM_NONE;

	/* Rx csum disabled */
	if (!(adapter->flags & IXGBE_FLAG_RX_CSUM_ENABLED))
		return;

	/* if IP and error */
	if ((status_err & IXGBE_RXD_STAT_IPCS) &&
	    (status_err & IXGBE_RXDADV_ERR_IPE)) {
		adapter->hw_csum_rx_error++;
		return;
	}

	if (!(status_err & IXGBE_RXD_STAT_L4CS))
		return;

	if (status_err & IXGBE_RXDADV_ERR_TCPE) {
		u16 pkt_info = rx_desc->wb.lower.lo_dword.hs_rss.pkt_info;
		/*
		 * 82599 errata, UDP frames with a 0 checksum can be marked as
		 * checksum errors.  
		 */
		if ((pkt_info & IXGBE_RXDADV_PKTTYPE_UDP) &&
		    (adapter->hw.mac.type == ixgbe_mac_82599EB)) 
				return;

		adapter->hw_csum_rx_error++;
		return;
	}

	/* It must be a TCP or UDP packet with a valid checksum */
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	adapter->hw_csum_rx_good++;
}

static inline void ixgbe_release_rx_desc(struct ixgbe_hw *hw,
	                                 struct ixgbe_ring *rx_ring, u32 val)
{
	/*
	 * Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64).
	 */
	wmb();
	writel(val, hw->hw_addr + rx_ring->tail);
}

static inline u8 *packet_buf(struct ixgbe_ring *ring, int i)
{
	return ring->window[i >> IXGBE_SUBWINDOW_BITS] +
			(i & IXGBE_SUBWINDOW_MASK) * MAX_PACKET_SIZE;
}

static inline u64 packet_dma(struct ixgbe_ring *ring, int i)
{
	return ring->dma_window[i >> IXGBE_SUBWINDOW_BITS] +
			(i & IXGBE_SUBWINDOW_MASK) * MAX_PACKET_SIZE;
}

/**
 * ixgbe_alloc_rx_buffers - Replace used receive buffers; packet split
 * @adapter: address of board private structure
 **/
static void ixgbe_alloc_rx_buffers(struct ixgbe_adapter *adapter,
                                   struct ixgbe_ring *rx_ring,
                                   int cleaned_count)
{
	union ixgbe_adv_rx_desc *rx_desc;
	unsigned int i = rx_ring->next_to_use;

	while (cleaned_count--) {
		__le64 addr = cpu_to_le64(packet_dma(rx_ring, i));

		rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, i);
		rx_desc->read.pkt_addr = addr;
		rx_desc->read.hdr_addr = addr;

		i = (i + 1) % rx_ring->count;
		if (i == rx_ring->queued)
			printk("ERR: next_to_use == queued?\n");
	}

	if (rx_ring->next_to_use != i) {
		rx_ring->next_to_use = i;
		ixgbe_release_rx_desc(&adapter->hw, rx_ring, i);
	} else
		printk("ERR: descriptor is not updated!\n");
}

static inline u16 ixgbe_get_hdr_info(union ixgbe_adv_rx_desc *rx_desc)
{
	return rx_desc->wb.lower.lo_dword.hs_rss.hdr_info;
}

#if !defined(IXGBE_NO_LRO) || !defined(IXGBE_NO_HW_RSC)
/**
 * ixgbe_transform_rsc_queue - change rsc queue into a full packet
 * @skb: pointer to the last skb in the rsc queue
 *
 * This function changes a queue full of hw rsc buffers into a completed
 * packet.  It uses the ->prev pointers to find the first packet and then
 * turns it into the frag list owner.
 **/
static inline struct sk_buff *ixgbe_transform_rsc_queue(struct sk_buff *skb)
{
	unsigned int frag_list_size = 0;

	while (skb->prev) {
		struct sk_buff *prev = skb->prev;
		frag_list_size += skb->len;
		skb->prev = NULL;
		skb = prev;
	}

	skb_shinfo(skb)->frag_list = skb->next;
	skb->next = NULL;
	skb->len += frag_list_size;
	skb->data_len += frag_list_size;
	skb->truesize += frag_list_size;
	return skb;
}

#endif /* !IXGBE_NO_LRO || !IXGBE_NO_HW_RSC */
#ifndef IXGBE_NO_LRO
/**
 * ixgbe_can_lro - returns true if packet is TCP/IPV4 and LRO is enabled
 * @adapter: board private structure
 * @rx_desc: pointer to the rx descriptor
 *
 **/
static inline bool ixgbe_can_lro(struct ixgbe_adapter *adapter,
                                 union ixgbe_adv_rx_desc *rx_desc)
{
	u16 pkt_info = rx_desc->wb.lower.lo_dword.hs_rss.pkt_info;

	return (adapter->flags2 & IXGBE_FLAG2_SWLRO_ENABLED) &&
		!(adapter->netdev->flags & IFF_PROMISC) &&
	        (pkt_info & IXGBE_RXDADV_PKTTYPE_IPV4) &&
	        (pkt_info & IXGBE_RXDADV_PKTTYPE_TCP);
}

/**
 * ixgbe_lro_flush - Indicate packets to upper layer.
 *
 * Update IP and TCP header part of head skb if more than one
 * skb's chained and indicate packets to upper layer.
 **/
static void ixgbe_lro_flush(struct ixgbe_q_vector *q_vector,
                                 struct ixgbe_lro_desc *lrod)
{
	struct ixgbe_lro_list *lrolist = q_vector->lrolist;
	struct iphdr *iph;
	struct tcphdr *th;
	struct sk_buff *skb;
	u32 *ts_ptr;

	hlist_del(&lrod->lro_node);
	lrolist->active_cnt--;

	skb = lrod->skb;
	lrod->skb = NULL;

	if (lrod->append_cnt) {
		/* take the lro queue and convert to skb format */
		skb = ixgbe_transform_rsc_queue(skb);

		/* incorporate ip header and re-calculate checksum */
		iph = (struct iphdr *)skb->data;
		iph->tot_len = ntohs(skb->len);
		iph->check = 0;
		iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

		/* incorporate the latest ack into the tcp header */
		th = (struct tcphdr *) ((char *)skb->data + sizeof(*iph));
		th->ack_seq = lrod->ack_seq;
		th->psh = lrod->psh;
		th->window = lrod->window;
		th->check = 0;

		/* incorporate latest timestamp into the tcp header */
		if (lrod->opt_bytes) {
			ts_ptr = (u32 *)(th + 1);
			ts_ptr[1] = htonl(lrod->tsval);
			ts_ptr[2] = lrod->tsecr;
		}
	}

#ifdef NETIF_F_TSO
	skb_shinfo(skb)->gso_size = lrod->mss;
#endif
	ixgbe_receive_skb(q_vector, skb, lrod->vlan_tag);
	lrolist->stats.flushed++;

	
	hlist_add_head(&lrod->lro_node, &lrolist->free);
}

static void ixgbe_lro_flush_all(struct ixgbe_q_vector *q_vector)
{
	struct ixgbe_lro_desc *lrod;
	struct hlist_node *node, *node2;
	struct ixgbe_lro_list *lrolist = q_vector->lrolist;

	hlist_for_each_entry_safe(lrod, node, node2, &lrolist->active, lro_node)
		ixgbe_lro_flush(q_vector, lrod);
}

/*
 * ixgbe_lro_header_ok - Main LRO function.
 **/
static u16 ixgbe_lro_header_ok(struct sk_buff *new_skb, struct iphdr *iph,
                               struct tcphdr *th)
{
	int opt_bytes, tcp_data_len;
	u32 *ts_ptr = NULL;

	/* If we see CE codepoint in IP header, packet is not mergeable */
	if (INET_ECN_is_ce(ipv4_get_dsfield(iph)))
		return -1;

	/* ensure there are no options */
	if ((iph->ihl << 2) != sizeof(*iph))
		return -1;

	/* .. and the packet is not fragmented */
	if (iph->frag_off & htons(IP_MF|IP_OFFSET))
		return -1;

	/* ensure no bits set besides ack or psh */
	if (th->fin || th->syn || th->rst ||
	    th->urg || th->ece || th->cwr || !th->ack)
		return -1;

	/* ensure that the checksum is valid */
	if (new_skb->ip_summed != CHECKSUM_UNNECESSARY)
		return -1;

	/*
	 * check for timestamps. Since the only option we handle are timestamps,
	 * we only have to handle the simple case of aligned timestamps
	 */

	opt_bytes = (th->doff << 2) - sizeof(*th);
	if (opt_bytes != 0) {
		ts_ptr = (u32 *)(th + 1);
		if ((opt_bytes != TCPOLEN_TSTAMP_ALIGNED) ||
			(*ts_ptr != ntohl((TCPOPT_NOP << 24) |
			(TCPOPT_NOP << 16) | (TCPOPT_TIMESTAMP << 8) |
			TCPOLEN_TIMESTAMP))) {
			return -1;
		}
	}

	tcp_data_len = ntohs(iph->tot_len) - (th->doff << 2) - sizeof(*iph);

	return tcp_data_len;
}

/**
 * ixgbe_lro_queue - if able, queue skb into lro chain
 * @q_vector: structure containing interrupt and ring information
 * @new_skb: pointer to current skb being checked
 * @tag: vlan tag for skb
 *
 * Checks whether the skb given is eligible for LRO and if that's
 * fine chains it to the existing lro_skb based on flowid. If an LRO for
 * the flow doesn't exist create one.
 **/
static struct sk_buff *ixgbe_lro_queue(struct ixgbe_q_vector *q_vector,
                                       struct sk_buff *new_skb, 
				       u16 tag)
{
	struct sk_buff *lro_skb;
	struct ixgbe_lro_desc *lrod;
	struct hlist_node *node;
	struct skb_shared_info *new_skb_info = skb_shinfo(new_skb);
	struct ixgbe_lro_list *lrolist = q_vector->lrolist;
	struct iphdr *iph = (struct iphdr *)new_skb->data;
	struct tcphdr *th = (struct tcphdr *)(iph + 1);
	int tcp_data_len = ixgbe_lro_header_ok(new_skb, iph, th);
	u16  opt_bytes = (th->doff << 2) - sizeof(*th);
	u32 *ts_ptr = (opt_bytes ? (u32 *)(th + 1) : NULL);
	u32 seq = ntohl(th->seq);

	/*
	 * we have a packet that might be eligible for LRO,
	 * so see if it matches anything we might expect
	 */
	hlist_for_each_entry(lrod, node, &lrolist->active, lro_node) {
		if (lrod->source_port != th->source ||
			lrod->dest_port != th->dest ||
			lrod->source_ip != iph->saddr ||
			lrod->dest_ip != iph->daddr ||
			lrod->vlan_tag != tag)
			continue;

		/* malformed header, or resultant packet would be too large */
		if (tcp_data_len < 0 || (tcp_data_len + lrod->len) > 65535) {
			ixgbe_lro_flush(q_vector, lrod);
			break;
		}

		/* out of order packet */
		if (seq != lrod->next_seq) {
			ixgbe_lro_flush(q_vector, lrod);
			tcp_data_len = -1;
			break;
		}

		if (lrod->opt_bytes || opt_bytes) {
			u32 tsval = ntohl(*(ts_ptr + 1));
			/* make sure timestamp values are increasing */
			if (opt_bytes != lrod->opt_bytes || 
			    lrod->tsval > tsval || *(ts_ptr + 2) == 0) {
				ixgbe_lro_flush(q_vector, lrod);
				tcp_data_len = -1;
				break;
			}
				
			lrod->tsval = tsval;
			lrod->tsecr = *(ts_ptr + 2);
		}

		/* remove any padding from the end of the skb */
		__pskb_trim(new_skb, ntohs(iph->tot_len));
		/* Remove IP and TCP header*/
		skb_pull(new_skb, ntohs(iph->tot_len) - tcp_data_len);

		lrod->next_seq += tcp_data_len;
		lrod->ack_seq = th->ack_seq;
		lrod->window = th->window;
		lrod->len += tcp_data_len;
		lrod->psh |= th->psh;
		lrod->append_cnt++;
		lrolist->stats.coal++;

		if (tcp_data_len > lrod->mss)
			lrod->mss = tcp_data_len;

		lro_skb = lrod->skb;

		/* if header is empty pull pages into current skb */
		if (!skb_headlen(new_skb) &&
		    ((skb_shinfo(lro_skb)->nr_frags +
		      skb_shinfo(new_skb)->nr_frags) <= MAX_SKB_FRAGS )) {
			struct skb_shared_info *lro_skb_info = skb_shinfo(lro_skb);

			/* copy frags into the last skb */
			memcpy(lro_skb_info->frags + lro_skb_info->nr_frags,
			       new_skb_info->frags,
			       new_skb_info->nr_frags * sizeof(skb_frag_t));

			lro_skb_info->nr_frags += new_skb_info->nr_frags;
			lro_skb->len += tcp_data_len;
			lro_skb->data_len += tcp_data_len;
			lro_skb->truesize += tcp_data_len;

			new_skb_info->nr_frags = 0;
			new_skb->truesize -= tcp_data_len;
			new_skb->len = new_skb->data_len = 0;
		} else if (tcp_data_len) {
		/* Chain this new skb in frag_list */
			new_skb->prev = lro_skb;
			lro_skb->next = new_skb;
			lrod->skb = new_skb ;
		}

		if (lrod->psh)
			ixgbe_lro_flush(q_vector, lrod);

		/* return the skb if it is empty for recycling */
		if (!new_skb->len) {
			new_skb->data = skb_mac_header(new_skb);
			__pskb_trim(new_skb, 0);
			new_skb->protocol = 0;
			lrolist->stats.recycled++;
			return new_skb;
		}

		return NULL;
	}

	/* start a new packet */
	if (tcp_data_len > 0 && !hlist_empty(&lrolist->free) && !th->psh) {
		lrod = hlist_entry(lrolist->free.first, struct ixgbe_lro_desc,
		                   lro_node);

		lrod->skb = new_skb;
		lrod->source_ip = iph->saddr;
		lrod->dest_ip = iph->daddr;
		lrod->source_port = th->source;
		lrod->dest_port = th->dest;
		lrod->vlan_tag = tag;
		lrod->len = new_skb->len;
		lrod->next_seq = seq + tcp_data_len;
		lrod->ack_seq = th->ack_seq;
		lrod->window = th->window;
		lrod->mss = tcp_data_len;
		lrod->opt_bytes = opt_bytes;
		lrod->psh = 0;
		lrod->append_cnt = 0;

		/* record timestamp if it is present */
		if (opt_bytes) {
			lrod->tsval = ntohl(*(ts_ptr + 1));
			lrod->tsecr = *(ts_ptr + 2);
		}
		/* remove first packet from freelist.. */
		hlist_del(&lrod->lro_node);
		/* .. and insert at the front of the active list */
		hlist_add_head(&lrod->lro_node, &lrolist->active);
		lrolist->active_cnt++;
		lrolist->stats.coal++;
		return NULL;
	}

	/* packet not handled by any of the above, pass it to the stack */
	ixgbe_receive_skb(q_vector, new_skb, tag);
	return NULL;
}

static void ixgbe_lro_ring_exit(struct ixgbe_lro_list *lrolist)
{
	struct hlist_node *node, *node2;
	struct ixgbe_lro_desc *lrod;

	hlist_for_each_entry_safe(lrod, node, node2, &lrolist->active,
	                          lro_node) {
		hlist_del(&lrod->lro_node);
		kfree(lrod);
	}

	hlist_for_each_entry_safe(lrod, node, node2, &lrolist->free,
	                          lro_node) {
		hlist_del(&lrod->lro_node);
		kfree(lrod);
	}
}

static void ixgbe_lro_ring_init(struct ixgbe_lro_list *lrolist)
{
	int j, bytes;
	struct ixgbe_lro_desc *lrod;

	bytes = sizeof(struct ixgbe_lro_desc);

	INIT_HLIST_HEAD(&lrolist->free);
	INIT_HLIST_HEAD(&lrolist->active);

	for (j = 0; j < IXGBE_LRO_MAX; j++) {
		lrod = kzalloc(bytes, GFP_KERNEL);
		if (lrod != NULL) {
			INIT_HLIST_NODE(&lrod->lro_node);
			hlist_add_head(&lrod->lro_node, &lrolist->free);
		}
	}
}

#endif /* IXGBE_NO_LRO */

#ifndef IXGBE_NO_HW_RSC
static inline u32 ixgbe_get_rsc_count(union ixgbe_adv_rx_desc *rx_desc)
{
	return (le32_to_cpu(rx_desc->wb.lower.lo_dword.data) &
	        IXGBE_RXDADV_RSCCNT_MASK) >>
	        IXGBE_RXDADV_RSCCNT_SHIFT;
}

#endif /* IXGBE_NO_HW_RSC */

#if 0
static void ixgbe_rx_status_indication(u32 staterr,
                                       struct ixgbe_adapter *adapter)
{
	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
		if (staterr & IXGBE_RXD_STAT_FLM)
			adapter->flm++;
#ifndef IXGBE_NO_LLI
		if (staterr & IXGBE_RXD_STAT_DYNINT)
			adapter->lli_int++;
#endif /* IXGBE_NO_LLI */
		break;
	case ixgbe_mac_82598EB:
#ifndef IXGBE_NO_LLI
		if (staterr & IXGBE_RXD_STAT_DYNINT)
			adapter->lli_int++;
#endif /* IXGBE_NO_LLI */
		break;
	default:
		break;
	}
}
#endif

int ixgbe_xmit_batch(struct ixgbe_ring *tx_ring, 
		int num_to_xmit, struct ps_pkt_info *info, char *buf)
{
	struct ixgbe_adapter *adapter = tx_ring->adapter;
	struct ixgbe_tx_buffer *bi;
	union ixgbe_adv_tx_desc *tx_desc = NULL;
	int qidx, next_qidx; 
	int i = 0;
	int left, cnt;
	unsigned int total_bytes = 0;
	u8 *dst;
	
	const u32 cmd_type_len = IXGBE_ADVTXD_DTYP_DATA |
		IXGBE_ADVTXD_DCMD_DEXT |
		IXGBE_TXD_CMD_EOP |
		//IXGBE_TXD_CMD_RS |
		IXGBE_TXD_CMD_IFCS;

	qidx = tx_ring->next_to_use;

	if (tx_ring->next_to_clean <= tx_ring->next_to_use)
		left = tx_ring->count - tx_ring->next_to_use + tx_ring->next_to_clean - 1;
	else
		left = tx_ring->next_to_clean - tx_ring->next_to_use - 1;

	if (left < num_to_xmit) {
		int tmp;
		ixgbe_clean_tx_irq(adapter, tx_ring, &tmp, 0);

		/* second try */
		if (tx_ring->next_to_clean <= tx_ring->next_to_use)
			left = tx_ring->count - tx_ring->next_to_use + tx_ring->next_to_clean - 1;
		else
			left = tx_ring->next_to_clean - tx_ring->next_to_use - 1;
	}
			
	cnt = min(num_to_xmit, left);
	if (cnt == 0)
		return cnt;

	dst = packet_buf(tx_ring, qidx);

	prefetchnta(&tx_ring->tx_buffer_info[qidx + 0]);
	prefetchnta(&tx_ring->tx_buffer_info[qidx + 4]);

	prefetchnta(buf + info[0].offset + 64 * 0);
	prefetchnta(buf + info[0].offset + 64 * 1);
	prefetchnta(buf + info[0].offset + 64 * 2);
	prefetchnta(buf + info[0].offset + 64 * 3);

	prefetchnta(dst + MAX_PACKET_SIZE * 0);
	prefetchnta(dst + MAX_PACKET_SIZE * 1);
	prefetchnta(dst + MAX_PACKET_SIZE * 2);
	prefetchnta(dst + MAX_PACKET_SIZE * 3);
	prefetchnta(dst + MAX_PACKET_SIZE * 4);
	prefetchnta(dst + MAX_PACKET_SIZE * 5);
	prefetchnta(dst + MAX_PACKET_SIZE * 6);
	prefetchnta(dst + MAX_PACKET_SIZE * 7);
	
	for (i = 0; i < cnt; i++) {
		int len = info[i].len;
		
		dst = packet_buf(tx_ring, qidx);

		prefetchnta(&tx_ring->tx_buffer_info[qidx + 8]);
		prefetchnta(dst + MAX_PACKET_SIZE * 8);
		prefetchnta(buf + info[i].offset + 64 * 4);

		next_qidx = (qidx + 1) % tx_ring->count;

		memcpy_aligned(dst, buf + info[i].offset, len);

		tx_desc = IXGBE_TX_DESC_ADV(*tx_ring, qidx);

		tx_desc->read.buffer_addr = 
				cpu_to_le64(packet_dma(tx_ring, qidx));

		tx_desc->read.cmd_type_len = 
				cpu_to_le32(cmd_type_len | len);

		tx_desc->read.olinfo_status = cpu_to_le32(len << IXGBE_ADVTXD_PAYLEN_SHIFT);

		bi = &tx_ring->tx_buffer_info[qidx];
		bi->time_stamp = jiffies;
		bi->length = len;
		bi->next_to_watch = qidx;	/* useless */

		total_bytes += len;

		qidx = next_qidx;

		if (i % 256 == 255) {
			wmb();
			writel(qidx, adapter->hw.hw_addr + tx_ring->tail);
		}
	}

	tx_ring->next_to_use = qidx;

	tx_ring->total_packets += cnt;
	tx_ring->total_bytes += total_bytes;
	tx_ring->stats.packets += cnt;
	tx_ring->stats.bytes += total_bytes;

	/*
	 * Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64).
	 */
	wmb();
	writel(qidx, adapter->hw.hw_addr + tx_ring->tail);

	return cnt;
}

static bool ixgbe_clean_rx_irq(struct ixgbe_q_vector *q_vector,
                               struct ixgbe_ring *rx_ring,
                               int *work_done, int work_to_do)
{
	struct ixgbe_adapter *adapter = rx_ring->adapter;
	union ixgbe_adv_rx_desc *rx_desc;

	int i;
	u32 len, staterr;
	bool cleaned = false;
	int cleaned_count = 0;
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;

	i = rx_ring->next_to_clean;
	work_to_do -= *work_done;

	rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, i);
	staterr = le32_to_cpu(rx_desc->wb.upper.status_error);

	while ((staterr & IXGBE_RXD_STAT_DD) && (cleaned_count < work_to_do)) {
		struct ixgbe_rx_buffer *bi = &rx_ring->rx_buffer_info[i];

		if (i % 4 == 0)
			prefetchnta(IXGBE_RX_DESC_ADV(*rx_ring, i + 8));

		if (likely(staterr & IXGBE_RXD_STAT_EOP)) {
			if (unlikely(staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK))
				len = 0;
		} else {
			printk("found non-EOP packets!\n");
			len = 0;
		}

		len = le16_to_cpu(rx_desc->wb.upper.length);
		//rx_desc->wb.upper.status_error = 0;
		bi->length = len;

		i = (i + 1) % rx_ring->count;
		rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, i);
		staterr = le32_to_cpu(rx_desc->wb.upper.status_error);

		cleaned = true;
		cleaned_count++;

		total_rx_packets++;
		total_rx_bytes += len;
	}

	*work_done += cleaned_count;
	rx_ring->next_to_clean = i;

	if (rx_ring->queued <= rx_ring->next_to_use)
		cleaned_count = rx_ring->count - rx_ring->next_to_use + rx_ring->queued - 1;
	else
		cleaned_count = rx_ring->queued - rx_ring->next_to_use - 1;

	if (cleaned_count > 0)
		ixgbe_alloc_rx_buffers(adapter, rx_ring, cleaned_count);

	rx_ring->stats.packets += total_rx_packets;
	rx_ring->stats.bytes += total_rx_bytes;
	rx_ring->total_packets += total_rx_packets;
	rx_ring->total_bytes += total_rx_bytes;

	return cleaned;
}

/*
 * ixgbe_write_eitr - write EITR register in hardware specific way
 * @q_vector: structure containing interrupt and ring information
 *
 * This function is made to be called by ethtool and by the driver
 * when it needs to update EITR registers at runtime.  Hardware
 * specific quirks/differences are taken care of here.
 */
void ixgbe_write_eitr(struct ixgbe_q_vector *q_vector)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_hw *hw = &adapter->hw;
	int v_idx = q_vector->v_idx;
	u32 itr_reg = EITR_INTS_PER_SEC_TO_REG(q_vector->eitr);

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		/* must write high and low 16 bits to reset counter */
		itr_reg |= (itr_reg << 16);
	} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		/*
		 * set the WDIS bit to not clear the timer bits and cause an
		 * immediate assertion of the interrupt
		 */
		itr_reg |= IXGBE_EITR_CNT_WDIS;
	}
	IXGBE_WRITE_REG(hw, IXGBE_EITR(v_idx), itr_reg);
}

#ifdef CONFIG_IXGBE_NAPI
static int ixgbe_clean_rxonly(struct napi_struct *, int);
#endif
/**
 * ixgbe_configure_msix - Configure MSI-X hardware
 * @adapter: board private structure
 *
 * ixgbe_configure_msix sets up the hardware to properly generate MSI-X
 * interrupts.
 **/
static void ixgbe_configure_msix(struct ixgbe_adapter *adapter)
{
	struct ixgbe_q_vector *q_vector;
	int i, j, q_vectors, v_idx, r_idx;
	u32 mask;

	q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/*
	 * Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	for (v_idx = 0; v_idx < q_vectors; v_idx++) {
		q_vector = adapter->q_vector[v_idx];
		/* XXX for_each_bit(...) */
		r_idx = find_first_bit(q_vector->rxr_idx,
		                       adapter->num_rx_queues);

		for (i = 0; i < q_vector->rxr_count; i++) {
			j = adapter->rx_ring[r_idx].reg_idx;
			ixgbe_set_ivar(adapter, 0, j, v_idx);
			r_idx = find_next_bit(q_vector->rxr_idx,
			                      adapter->num_rx_queues,
			                      r_idx + 1);
		}
		r_idx = find_first_bit(q_vector->txr_idx,
		                       adapter->num_tx_queues);

		for (i = 0; i < q_vector->txr_count; i++) {
			j = adapter->tx_ring[r_idx].reg_idx;
			ixgbe_set_ivar(adapter, 1, j, v_idx);
			r_idx = find_next_bit(q_vector->txr_idx,
			                      adapter->num_tx_queues,
			                      r_idx + 1);
		}

		/* if this is a tx only vector halve the interrupt rate */
		if (q_vector->txr_count && !q_vector->rxr_count)
			q_vector->eitr = (adapter->eitr_param >> 1);
		else if (q_vector->rxr_count)
			/* rx only */
			q_vector->eitr = adapter->eitr_param;

		ixgbe_write_eitr(q_vector);
	}

	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		ixgbe_set_ivar(adapter, -1, IXGBE_IVAR_OTHER_CAUSES_INDEX,
		               v_idx);
	else if (adapter->hw.mac.type == ixgbe_mac_82599EB)
		ixgbe_set_ivar(adapter, -1, 1, v_idx);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(v_idx), 1950);
#ifdef IXGBE_TCP_TIMER
	ixgbe_set_ivar(adapter, -1, 0, ++v_idx);
#endif /* IXGBE_TCP_TIMER */

	/* set up to autoclear timer, and the vectors */
	mask = IXGBE_EIMS_ENABLE_MASK;
	mask &= ~(IXGBE_EIMS_OTHER |
		  IXGBE_EIMS_LSC);

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, mask);
}

enum latency_range {
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};

/**
 * ixgbe_update_itr - update the dynamic ITR value based on statistics
 * @adapter: pointer to adapter
 * @eitr: eitr setting (ints per sec) to give last timeslice
 * @itr_setting: current throttle rate in ints/second
 * @packets: the number of packets during this measurement interval
 * @bytes: the number of bytes during this measurement interval
 *
 *      Stores a new ITR value based on packets and byte
 *      counts during the last interrupt.  The advantage of per interrupt
 *      computation is faster updates and more accurate ITR for the current
 *      traffic pattern.  Constants in this function were computed
 *      based on theoretical maximum wire speed and thresholds were set based
 *      on testing data as well as attempting to minimize response time
 *      while increasing bulk throughput.
 *      this functionality is controlled by the InterruptThrottleRate module
 *      parameter (see ixgbe_param.c)
 **/
static u8 ixgbe_update_itr(struct ixgbe_adapter *adapter,
                           u32 eitr, u8 itr_setting,
                           int packets, int bytes)
{
	unsigned int retval = itr_setting;
	u32 timepassed_us;
	u64 bytes_perint;

	if (packets == 0)
		goto update_itr_done;


	/* simple throttlerate management
	 *    0-20MB/s lowest (100000 ints/s)
	 *   20-100MB/s low   (20000 ints/s)
	 *  100-1249MB/s bulk (8000 ints/s)
	 */
	/* what was last interrupt timeslice? */
	timepassed_us = 1000000/eitr;
	bytes_perint = bytes / timepassed_us; /* bytes/usec */

	switch (itr_setting) {
	case lowest_latency:
		if (bytes_perint > adapter->eitr_low) {
			retval = low_latency;
		}
		break;
	case low_latency:
		if (bytes_perint > adapter->eitr_high) {
			retval = bulk_latency;
		}
		else if (bytes_perint <= adapter->eitr_low) {
			retval = lowest_latency;
		}
		break;
	case bulk_latency:
		if (bytes_perint <= adapter->eitr_high) {
			retval = low_latency;
		}
		break;
	}

update_itr_done:
	return retval;
}

static void ixgbe_set_itr_msix(struct ixgbe_q_vector *q_vector)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	u32 new_itr;
	u8 current_itr, ret_itr;
	int i, r_idx;
	struct ixgbe_ring *rx_ring = NULL, *tx_ring = NULL;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		tx_ring = &(adapter->tx_ring[r_idx]);
		ret_itr = ixgbe_update_itr(adapter, q_vector->eitr,
		                           q_vector->tx_itr,
		                           tx_ring->total_packets,
		                           tx_ring->total_bytes);
		/* if the result for this queue would decrease interrupt
		 * rate for this vector then use that result */
		q_vector->tx_itr = ((q_vector->tx_itr > ret_itr) ?
		                    q_vector->tx_itr - 1 : ret_itr);
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		rx_ring = &(adapter->rx_ring[r_idx]);
		ret_itr = ixgbe_update_itr(adapter, q_vector->eitr,
		                           q_vector->rx_itr,
		                           rx_ring->total_packets,
		                           rx_ring->total_bytes);
		/* if the result for this queue would decrease interrupt
		 * rate for this vector then use that result */
		q_vector->rx_itr = ((q_vector->rx_itr > ret_itr) ?
		                    q_vector->rx_itr - 1 : ret_itr);
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	current_itr = max(q_vector->rx_itr, q_vector->tx_itr);

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 100000;
		break;
	case low_latency:
		new_itr = 20000; /* aka hwitr = ~200 */
		break;
	case bulk_latency:
	default:
		new_itr = 8000;
		break;
	}

	if (new_itr != q_vector->eitr) {

		/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here */
		q_vector->eitr = new_itr;

		ixgbe_write_eitr(q_vector);
	}

	return;
}

static void ixgbe_check_fan_failure(struct ixgbe_adapter *adapter, u32 eicr)
{
	struct ixgbe_hw *hw = &adapter->hw;

	if ((adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) &&
	    (eicr & IXGBE_EICR_GPI_SDP1)) {
		DPRINTK(PROBE, CRIT, "Fan has stopped, replace the adapter\n");
		/* write to clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
	}
}

static void ixgbe_check_sfp_event(struct ixgbe_adapter *adapter, u32 eicr)
{
	struct ixgbe_hw *hw = &adapter->hw;

	if (eicr & IXGBE_EICR_GPI_SDP1) {
		/* Clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			schedule_work(&adapter->multispeed_fiber_task);
	} else if (eicr & IXGBE_EICR_GPI_SDP2) {
		/* Clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP2);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			schedule_work(&adapter->sfp_config_module_task);
	} else {
		/* Interrupt isn't for us... */
		return;
	}
}

static void ixgbe_check_lsc(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	adapter->lsc_int++;
	adapter->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	adapter->link_check_timeout = jiffies;
	if (!test_bit(__IXGBE_DOWN, &adapter->state)) {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_LSC);
		schedule_work(&adapter->watchdog_task);
	}
}

static irqreturn_t ixgbe_msix_lsc(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 eicr;

	/*
	 * Workaround of Silicon errata on 82598.  Use clear-by-write
	 * instead of clear-by-read to clear EICR , reading EICS gives the
	 * value of EICR without read-clear of EICR
	 */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	IXGBE_WRITE_REG(hw, IXGBE_EICR, eicr);

	if (eicr & IXGBE_EICR_LSC)
		ixgbe_check_lsc(adapter);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		if (eicr & IXGBE_EICR_ECC) {
			DPRINTK(LINK, INFO, "Received unrecoverable ECC Err, "
			                    "please reboot\n");
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_ECC);
		}
		/* Handle Flow Director Full threshold interrupt */
		if (eicr & IXGBE_EICR_FLOW_DIR) {
			int i;
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_FLOW_DIR);
			/* Disable transmits before FDIR Re-initialization */
			netif_tx_stop_all_queues(netdev);
			for (i = 0; i < adapter->num_tx_queues; i++) {
				struct ixgbe_ring *tx_ring =
				                           &adapter->tx_ring[i];
				if (test_and_clear_bit(__IXGBE_FDIR_INIT_DONE,
				                       &tx_ring->reinit_state))
					schedule_work(&adapter->fdir_reinit_task);
			}
		}
	}

	ixgbe_check_fan_failure(adapter, eicr);

	if (hw->mac.type == ixgbe_mac_82599EB)
		ixgbe_check_sfp_event(adapter, eicr);

	/* re-enable the original interrupt state, no lsc, no queues */
	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, eicr &
		                ~(IXGBE_EIMS_LSC | IXGBE_EIMS_RTX_QUEUE));

	return IRQ_HANDLED;
}

#ifdef IXGBE_TCP_TIMER
static irqreturn_t ixgbe_msix_pba(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i;

	u32 pba = readl(adapter->msix_addr + IXGBE_MSIXPBA);
	for (i = 0; i < MAX_MSIX_COUNT; i++) {
		if (pba & (1 << i))
			adapter->msix_handlers[i](irq, data, regs);
		else
			adapter->pba_zero[i]++;
	}

	adapter->msix_pba++;
	return IRQ_HANDLED;
}

static irqreturn_t ixgbe_msix_tcp_timer(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	adapter->msix_tcp_timer++;

	return IRQ_HANDLED;
}

#endif /* IXGBE_TCP_TIMER */
static inline void ixgbe_irq_enable_queues(struct ixgbe_adapter *adapter,
					   u64 qmask)
{
	u32 mask;
	struct ixgbe_hw *hw = &adapter->hw;

	if (hw->mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(0), mask);
		mask = (qmask >> 32);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(1), mask);
	}
	/* skip the flush */
}

static inline void ixgbe_irq_disable_queues(struct ixgbe_adapter *adapter,
                                            u64 qmask)
{
	u32 mask;
	struct ixgbe_hw *hw = &adapter->hw;

	if (hw->mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), mask);
		mask = (qmask >> 32);
		if (mask)
			IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), mask);
	}
	/* skip the flush */
}

static irqreturn_t ixgbe_msix_clean_tx(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring     *tx_ring;
	int i, r_idx;

	if (!q_vector->txr_count)
		return IRQ_HANDLED;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		tx_ring = &(adapter->tx_ring[r_idx]);
		tx_ring->total_bytes = 0;
		tx_ring->total_packets = 0;
#ifndef CONFIG_IXGBE_NAPI
		ixgbe_clean_tx_irq(adapter, tx_ring);
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_tx_dca(adapter, tx_ring);
#endif
#endif
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

#ifdef CONFIG_IXGBE_NAPI
	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);
#endif
	/*
	 * possibly later we can enable tx auto-adjustment if necessary
	 *
	if (adapter->itr_setting & 1)
		ixgbe_set_itr_msix(q_vector);
	 */

	return IRQ_HANDLED;
}

/**
 * ixgbe_msix_clean_rx - single unshared vector rx clean (all queues)
 * @irq: unused
 * @data: pointer to our q_vector struct for this interrupt vector
 **/
static irqreturn_t ixgbe_msix_clean_rx(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring  *rx_ring;
	int r_idx;
	int i;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		rx_ring = &(adapter->rx_ring[r_idx]);
		rx_ring->total_bytes = 0;
		rx_ring->total_packets = 0;
#ifndef CONFIG_IXGBE_NAPI
		ixgbe_clean_rx_irq(q_vector, rx_ring);

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_rx_dca(adapter, rx_ring);

#endif
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	if (adapter->itr_setting & 1)
		ixgbe_set_itr_msix(q_vector);
#else
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	if (!q_vector->rxr_count)
		return IRQ_HANDLED;

	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t ixgbe_msix_clean_many(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring  *ring;
	int r_idx;
	int i;

	if (!q_vector->txr_count && !q_vector->rxr_count)
		return IRQ_HANDLED;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		ring = &(adapter->tx_ring[r_idx]);
		ring->total_bytes = 0;
		ring->total_packets = 0;
#ifndef CONFIG_IXGBE_NAPI
		ixgbe_clean_tx_irq(adapter, ring);
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_tx_dca(adapter, ring);
#endif
#endif
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		ring = &(adapter->rx_ring[r_idx]);
		ring->total_bytes = 0;
		ring->total_packets = 0;
#ifndef CONFIG_IXGBE_NAPI
		ixgbe_clean_rx_irq(q_vector, ring);

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_rx_dca(adapter, ring);

#endif
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	if (adapter->itr_setting & 1)
		ixgbe_set_itr_msix(q_vector);
#else
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);
#endif

	return IRQ_HANDLED;
}

int copy_rx_packets(struct ixgbe_ring *rx_ring, 
		int n,
		struct ps_pkt_info *info,
		char *pkt_buf,
		int offset);

/* returns 1 if a packet is processed */
static int rx_kernel(struct net_device *dev, struct ixgbe_ring *rx_ring)
{
	struct sk_buff *skb;
	struct ps_pkt_info info;
	int ret;

	skb = netdev_alloc_skb(dev, rx_ring->rx_buf_len + NET_IP_ALIGN);

	if (skb == NULL) {
		printk(KERN_ERR "netdev_alloc_skb() failed!\n");
		return 1;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	ret = copy_rx_packets(rx_ring, 1, &info, skb->data, 0);
	if (!ret) {
		dev_kfree_skb_any(skb);
		return 0;
	}

	skb_put(skb, info.len);
	skb->protocol = eth_type_trans(skb, dev);
	netif_rx(skb);

	return 1;
}

#ifdef CONFIG_IXGBE_NAPI
/**
 * ixgbe_clean_rxonly - msix (aka one shot) rx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function is optimized for cleaning one queue only on a single
 * q_vector!!!
 **/
static int ixgbe_clean_rxonly(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *rx_ring = NULL;
	int work_done = 0;
	long r_idx;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	rx_ring = &(adapter->rx_ring[r_idx]);
#ifdef IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
		ixgbe_update_rx_dca(adapter, rx_ring);
#endif

	spin_lock(&rx_ring->lock);
	if (rx_ring->wq) {
		wake_up_interruptible(rx_ring->wq);
	} else {
		if (adapter->flags & IXGBE_FLAG_RX_KERNEL_ENABLE) {
			while (work_done < budget) {
				if (!rx_kernel(adapter->netdev, rx_ring))
					break;
				work_done++;
			}
		}
	}

	//ixgbe_clean_rx_irq(q_vector, rx_ring, &work_done, budget);

#ifndef HAVE_NETDEV_NAPI_LIST
	if (!netif_running(adapter->netdev))
		work_done = 0;

#endif
	/* If all Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		
		if (!rx_ring->wq && adapter->flags & IXGBE_FLAG_RX_KERNEL_ENABLE) {
			if (!test_bit(__IXGBE_DOWN, &adapter->state))
				ixgbe_irq_enable_queues(adapter, 
						((u64)1 << q_vector->v_idx));
		}
	}

	spin_unlock(&rx_ring->lock);

	return work_done;
}

/**
 * ixgbe_clean_rxtx_many - msix (aka one shot) rx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function will clean more than one rx queue associated with a
 * q_vector.
 **/
static int ixgbe_clean_rxtx_many(struct napi_struct *napi, int total_budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *ring = NULL;
	int work_done = 0, total_work = 0, i;
	int budget;
	long r_idx;
	bool rx_clean_complete = true, tx_clean_complete = true;

	budget = total_budget / (q_vector->rxr_count + q_vector->txr_count ? : 1);
	budget = max(budget, 1);

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		work_done = 0;
		ring = &(adapter->tx_ring[r_idx]);
#ifdef IXGBE_DCA
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_tx_dca(adapter, ring);
#endif
		ixgbe_clean_tx_irq(adapter, ring, &work_done, budget);
		total_work += work_done;
		tx_clean_complete &= (work_done < budget);
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		work_done = 0;
		ring = &(adapter->rx_ring[r_idx]);
#ifdef IXGBE_DCA
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_rx_dca(adapter, ring);
#endif
		ixgbe_clean_rx_irq(q_vector, ring, &work_done, budget);
		total_work += work_done;
		rx_clean_complete &= (work_done < budget);
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	if (!tx_clean_complete || !rx_clean_complete)
		work_done = total_budget;

	/* If all Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter, ((u64)1 << q_vector->v_idx));
	}

	return total_work;
}

/**
 * ixgbe_clean_txonly - msix (aka one shot) tx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function is optimized for cleaning one queue only on a single
 * q_vector!!!
 **/
static int ixgbe_clean_txonly(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *tx_ring = NULL;
	int work_done = 0;
	long r_idx;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	tx_ring = &(adapter->tx_ring[r_idx]);
#ifdef IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
		ixgbe_update_tx_dca(adapter, tx_ring);
#endif

	ixgbe_clean_tx_irq(adapter, tx_ring, &work_done, budget);

#ifndef HAVE_NETDEV_NAPI_LIST
	if (!netif_running(adapter->netdev))
		work_done = 0;

#endif
	/* If all Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter, ((u64)1 << q_vector->v_idx));
	}

	return work_done;
}

#endif /* CONFIG_IXGBE_NAPI */
static inline void map_vector_to_rxq(struct ixgbe_adapter *a, int v_idx,
                                     int r_idx)
{
	struct ixgbe_q_vector *q_vector = a->q_vector[v_idx];

	set_bit(r_idx, q_vector->rxr_idx);
	q_vector->rxr_count++;
}

static inline void map_vector_to_txq(struct ixgbe_adapter *a, int v_idx,
                                     int t_idx)
{
	struct ixgbe_q_vector *q_vector = a->q_vector[v_idx];

	set_bit(t_idx, q_vector->txr_idx);
	q_vector->txr_count++;
}

/**
 * ixgbe_map_rings_to_vectors - Maps descriptor rings to vectors
 * @adapter: board private structure to initialize
 *
 * This function maps descriptor rings to the queue-specific vectors
 * we were allotted through the MSI-X enabling code.  Ideally, we'd have
 * one vector per ring/queue, but on a constrained vector budget, we
 * group the rings as "efficiently" as possible.  You would add new
 * mapping configurations in here.
 **/
static int ixgbe_map_rings_to_vectors(struct ixgbe_adapter *adapter)
{
	int q_vectors;
	int v_start = 0;
	int rxr_idx = 0, txr_idx = 0;
	int rxr_remaining = adapter->num_rx_queues;

#if 0
	int txr_remaining = adapter->num_tx_queues;
#else
	/* disable all TX interrupts - adaline */
	int txr_remaining = 0;
#endif

	int i, j;
	int rqpv, tqpv;
	int err = 0;

	/* No mapping required if MSI-X is disabled. */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		goto out;

	q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/*
	 * The ideal configuration...
	 * We have enough vectors to map one per queue.
	 */
	if (q_vectors == adapter->num_rx_queues /* + adapter->num_tx_queues */) {
		for (; rxr_idx < rxr_remaining; v_start++, rxr_idx++)
			map_vector_to_rxq(adapter, v_start, rxr_idx);

		for (; txr_idx < txr_remaining; v_start++, txr_idx++)
			map_vector_to_txq(adapter, v_start, txr_idx);

		goto out;
	}

	/*
	 * If we don't have enough vectors for a 1-to-1
	 * mapping, we'll have to group them so there are
	 * multiple queues per vector.
	 */
	/* Re-adjusting *qpv takes care of the remainder. */
	for (i = v_start; i < q_vectors; i++) {
		rqpv = DIV_ROUND_UP(rxr_remaining, q_vectors - i);
		for (j = 0; j < rqpv; j++) {
			map_vector_to_rxq(adapter, i, rxr_idx);
			rxr_idx++;
			rxr_remaining--;
		}
	}

	for (i = v_start; i < q_vectors; i++) {
		tqpv = DIV_ROUND_UP(txr_remaining, q_vectors - i);
		for (j = 0; j < tqpv; j++) {
			map_vector_to_txq(adapter, i, txr_idx);
			txr_idx++;
			txr_remaining--;
		}
	}

out:
	return err;
}

/**
 * ixgbe_request_msix_irqs - Initialize MSI-X interrupts
 * @adapter: board private structure
 *
 * ixgbe_request_msix_irqs allocates MSI-X vectors and requests
 * interrupts from the kernel.
 **/
static int ixgbe_request_msix_irqs(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	irqreturn_t (*handler)(int, void *);
	int i, vector, q_vectors, err;
	int ri = 0, ti = 0;

	/* Decrement for Other and TCP Timer vectors */
	q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

#define SET_HANDLER(_v) (((_v)->rxr_count && (_v)->txr_count)        \
					  ? &ixgbe_msix_clean_many : \
			  (_v)->rxr_count ? &ixgbe_msix_clean_rx   : \
			  (_v)->txr_count ? &ixgbe_msix_clean_tx   : \
			  NULL)
	for (vector = 0; vector < q_vectors; vector++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[vector];
		handler = SET_HANDLER(q_vector);

		if (handler == &ixgbe_msix_clean_rx) {
			sprintf(q_vector->name, "%s-%s-%d",
			        netdev->name, "rx", ri++);
		} else if (handler == &ixgbe_msix_clean_tx) {
			sprintf(q_vector->name, "%s-%s-%d",
			        netdev->name, "tx", ti++);
		} else if (handler == &ixgbe_msix_clean_many) {
			sprintf(q_vector->name, "%s-%s-%d",
			        netdev->name, "TxRx", vector);
		} else {
			/* skip this unused q_vector */
			continue;
		}
		err = request_irq(adapter->msix_entries[vector].vector,
		                  handler, 0, q_vector->name,
		                  q_vector);
		if (err) {
			DPRINTK(PROBE, ERR,
			        "request_irq failed for MSIX interrupt "
			        "Error: %d\n", err);
			goto free_queue_irqs;
		}
	}

	sprintf(adapter->lsc_int_name, "%s:lsc", netdev->name);
	err = request_irq(adapter->msix_entries[vector].vector,
	                  &ixgbe_msix_lsc, 0, adapter->lsc_int_name, netdev);
	if (err) {
		DPRINTK(PROBE, ERR,
		        "request_irq for msix_lsc failed: %d\n", err);
		goto free_queue_irqs;
	}

#ifdef IXGBE_TCP_TIMER
	vector++;
	sprintf(adapter->tcp_timer_name, "%s:timer", netdev->name);
	err = request_irq(adapter->msix_entries[vector].vector,
	                  &ixgbe_msix_tcp_timer, 0, adapter->tcp_timer_name,
	                  netdev);
	if (err) {
		DPRINTK(PROBE, ERR,
		        "request_irq for msix_tcp_timer failed: %d\n", err);
		/* Free "Other" interrupt */
		free_irq(adapter->msix_entries[--vector].vector, netdev);
		goto free_queue_irqs;
	}

#endif
	return 0;

free_queue_irqs:
	for (i = vector - 1; i >= 0; i--)
		free_irq(adapter->msix_entries[--vector].vector,
		         adapter->q_vector[i]);
	adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
	pci_disable_msix(adapter->pdev);
	kfree(adapter->msix_entries);
	adapter->msix_entries = NULL;
	return err;
}

static void ixgbe_set_itr(struct ixgbe_adapter *adapter)
{
	struct ixgbe_q_vector *q_vector = adapter->q_vector[0];
	u8 current_itr;
	u32 new_itr = q_vector->eitr;
	struct ixgbe_ring *rx_ring = &adapter->rx_ring[0];
	struct ixgbe_ring *tx_ring = &adapter->tx_ring[0];

	q_vector->tx_itr = ixgbe_update_itr(adapter, new_itr,
	                                    q_vector->tx_itr,
	                                    tx_ring->total_packets,
	                                    tx_ring->total_bytes);
	q_vector->rx_itr = ixgbe_update_itr(adapter, new_itr,
	                                    q_vector->rx_itr,
	                                    rx_ring->total_packets,
	                                    rx_ring->total_bytes);

	current_itr = max(q_vector->rx_itr, q_vector->tx_itr);

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 100000;
		break;
	case low_latency:
		new_itr = 20000; /* aka hwitr = ~200 */
		break;
	case bulk_latency:
		new_itr = 8000;
		break;
	default:
		break;
	}

	if (new_itr != q_vector->eitr) {

		/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here */
		q_vector->eitr = new_itr;

		ixgbe_write_eitr(q_vector);
	}

	return;
}

/**
 * ixgbe_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/
static inline void ixgbe_irq_enable(struct ixgbe_adapter *adapter, bool queues, bool flush)
{
	u32 mask;
	u64 qmask;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);
	qmask = ~0;

	/* don't reenable LSC while waiting for link */
	if (adapter->flags & IXGBE_FLAG_NEED_LINK_UPDATE)
		mask &= ~IXGBE_EIMS_LSC;
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE)
		mask |= IXGBE_EIMS_GPI_SDP1;
	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		mask |= IXGBE_EIMS_ECC;
		mask |= IXGBE_EIMS_GPI_SDP1;
		mask |= IXGBE_EIMS_GPI_SDP2;
	}
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		mask |= IXGBE_EIMS_FLOW_DIR;

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, mask);
	if (queues)
		ixgbe_irq_enable_queues(adapter, qmask);
	if (flush)
		IXGBE_WRITE_FLUSH(&adapter->hw);
}

/**
 * ixgbe_intr - legacy mode Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 **/
static irqreturn_t ixgbe_intr(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_q_vector *q_vector = adapter->q_vector[0];
	u32 eicr;

	/*
	 * Workaround of Silicon errata on 82598.  Mask the interrupt
	 * before the read of EICR.
	 */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);

	/* for NAPI, using EIAM to auto-mask tx/rx interrupt bits on read
	 * therefore no explict interrupt disable is necessary */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICR);
	if (!eicr) {
		/*
		 * shared interrupt alert!
		 * make sure interrupts are enabled because the read will
		 * have disabled interrupts due to EIAM
		 * finish the workaround of silicon errata on 82598.  Unmask
		 * the interrupt that we masked before the EICR read.
		 */
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable(adapter, true, true);
		return IRQ_NONE;  /* Not our interrupt */
	}

	if (eicr & IXGBE_EICR_LSC)
		ixgbe_check_lsc(adapter);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		if (eicr & IXGBE_EICR_ECC)
			DPRINTK(LINK, INFO, "Received unrecoverable ECC Err, "
			                    "please reboot\n");
		ixgbe_check_sfp_event(adapter, eicr);
	}

	ixgbe_check_fan_failure(adapter, eicr);

#ifdef CONFIG_IXGBE_NAPI
	if (napi_schedule_prep(&(q_vector->napi))) {
		adapter->tx_ring[0].total_packets = 0;
		adapter->tx_ring[0].total_bytes = 0;
		adapter->rx_ring[0].total_packets = 0;
		adapter->rx_ring[0].total_bytes = 0;
		/* would disable interrupts here but EIAM disabled it */
		__napi_schedule(&(q_vector->napi));
	}

	/*
	 * re-enable link(maybe) and non-queue interrupts, no flush.
	 * ixgbe_poll will re-enable the queue interrupts
	 */
	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter, false, false);
#else
	adapter->tx_ring[0].total_packets = 0;
	adapter->tx_ring[0].total_bytes = 0;
	adapter->rx_ring[0].total_packets = 0;
	adapter->rx_ring[0].total_bytes = 0;
	ixgbe_clean_tx_irq(adapter, adapter->tx_ring);
	ixgbe_clean_rx_irq(q_vector, adapter->rx_ring);

	/* dynamically adjust throttle */
	if (adapter->itr_setting & 1)
		ixgbe_set_itr(adapter);

	/*
	 * Workaround of Silicon errata on 82598.  Unmask
	 * the interrupt that we masked before the EICR read
	 * no flush of the re-enable is necessary here
	 */
	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter, true, false);
#endif
	return IRQ_HANDLED;
}

static inline void ixgbe_reset_q_vectors(struct ixgbe_adapter *adapter)
{
	int i, q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	for (i = 0; i < q_vectors; i++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[i];
		bitmap_zero(q_vector->rxr_idx, MAX_RX_QUEUES);
		bitmap_zero(q_vector->txr_idx, MAX_TX_QUEUES);
		q_vector->rxr_count = 0;
		q_vector->txr_count = 0;
		q_vector->eitr = adapter->eitr_param;
	}
}

/**
 * ixgbe_request_irq - initialize interrupts
 * @adapter: board private structure
 *
 * Attempts to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static int ixgbe_request_irq(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		err = ixgbe_request_msix_irqs(adapter);
	} else if (adapter->flags & IXGBE_FLAG_MSI_ENABLED) {
		err = request_irq(adapter->pdev->irq, &ixgbe_intr, 0,
		                  netdev->name, netdev);
	} else {
		err = request_irq(adapter->pdev->irq, &ixgbe_intr, IRQF_SHARED,
		                  netdev->name, netdev);
	}

	if (err)
		DPRINTK(PROBE, ERR, "request_irq failed, Error %d\n", err);

	return err;
}

static void ixgbe_free_irq(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		int i, q_vectors;

		q_vectors = adapter->num_msix_vectors;

		i = q_vectors - 1;
#ifdef IXGBE_TCP_TIMER
		free_irq(adapter->msix_entries[i].vector, netdev);
		i--;
#endif
		free_irq(adapter->msix_entries[i].vector, netdev);
		i--;

		for (; i >= 0; i--) {
			free_irq(adapter->msix_entries[i].vector,
			         adapter->q_vector[i]);
		}

		ixgbe_reset_q_vectors(adapter);
	} else {
		free_irq(adapter->pdev->irq, netdev);
	}
}

/**
 * ixgbe_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/
static inline void ixgbe_irq_disable(struct ixgbe_adapter *adapter)
{
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(&adapter->hw);
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		int i;
		for (i = 0; i < adapter->num_msix_vectors; i++)
			synchronize_irq(adapter->msix_entries[i].vector);
	} else {
		synchronize_irq(adapter->pdev->irq);
	}
}

/**
 * ixgbe_configure_msi_and_legacy - Initialize PIN (INTA...) and MSI interrupts
 *
 **/
static void ixgbe_configure_msi_and_legacy(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	IXGBE_WRITE_REG(hw, IXGBE_EITR(0),
	                EITR_INTS_PER_SEC_TO_REG(adapter->eitr_param));

	ixgbe_set_ivar(adapter, 0, 0, 0);
	ixgbe_set_ivar(adapter, 1, 0, 0);

	map_vector_to_rxq(adapter, 0, 0);
	map_vector_to_txq(adapter, 0, 0);

	DPRINTK(HW, INFO, "Legacy interrupt IVAR setup done\n");
}

/**
 * ixgbe_configure_tx - Configure 8259x Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void ixgbe_configure_tx(struct ixgbe_adapter *adapter)
{
	u64 tdba;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 i, j, tdlen, txctrl;
	u32 mask;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct ixgbe_ring *ring = &adapter->tx_ring[i];
		j = ring->reg_idx;
		tdba = ring->dma;
		tdlen = ring->count * sizeof(union ixgbe_adv_tx_desc);
		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(j),
		                (tdba & DMA_BIT_MASK(32)));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(j), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(j), tdlen);
		IXGBE_WRITE_REG(hw, IXGBE_TDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(j), 0);
		adapter->tx_ring[i].head = IXGBE_TDH(j);
		adapter->tx_ring[i].tail = IXGBE_TDT(j);
		/* Disable Tx Head Writeback RO bit, since this hoses
		 * bookkeeping if things aren't delivered in order.
		 */
		txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(j));
		txctrl &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(j), txctrl);
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		u32 rttdcs;

		/* disable the arbiter while setting MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);

		/* set transmit pool layout */
		mask = IXGBE_FLAG_VMDQ_ENABLED;
		mask |= IXGBE_FLAG_DCB_ENABLED;
		switch (adapter->flags & mask) {

		case (IXGBE_FLAG_VMDQ_ENABLED):
			IXGBE_WRITE_REG(hw, IXGBE_MTQC,
					(IXGBE_MTQC_VT_ENA | IXGBE_MTQC_64VF));
			break;

		case (IXGBE_FLAG_DCB_ENABLED):
			IXGBE_WRITE_REG(hw, IXGBE_MTQC,
				      (IXGBE_MTQC_RT_ENA | IXGBE_MTQC_8TC_8TQ));
			break;

		default:
			IXGBE_WRITE_REG(hw, IXGBE_MTQC, IXGBE_MTQC_64Q_1PB);
			break;
		}

		/* re-eable the arbiter */
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}
}

#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT	2

static void ixgbe_configure_srrctl(struct ixgbe_adapter *adapter, int index)
{
	struct ixgbe_ring *rx_ring;
	u32 srrctl;
	int queue0 = 0;
	unsigned long mask;
	struct ixgbe_ring_feature *feature = adapter->ring_feature;

	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
			int dcb_i = feature[RING_F_DCB].indices;
			if (dcb_i == 8)
				queue0 = index >> 4;
			else if (dcb_i == 4)
				queue0 = index >> 5;
			else
				DPRINTK(PROBE, ERR, "Invalid DCB configuration");
		} else {
			queue0 = index;
		}
	} else {
		mask = (unsigned long) feature[RING_F_RXQ].mask;
		queue0 = index & mask;
		index = index & mask;
	}

	rx_ring = &adapter->rx_ring[queue0];

	srrctl = IXGBE_READ_REG(&adapter->hw, IXGBE_SRRCTL(index));

	srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
	srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;

	srrctl |= (IXGBE_RX_HDR_SIZE << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT) &
		   IXGBE_SRRCTL_BSIZEHDR_MASK;

	if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED) {
#if (PAGE_SIZE / 2) > IXGBE_MAX_RXBUFFER
		srrctl |= IXGBE_MAX_RXBUFFER >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#else
		srrctl |= (PAGE_SIZE / 2) >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#endif
		srrctl |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
	} else {
		srrctl |= ALIGN(rx_ring->rx_buf_len, 1024) >>
		          IXGBE_SRRCTL_BSIZEPKT_SHIFT;
		srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
	}
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_SRRCTL(index), srrctl);
}


static u32 ixgbe_setup_mrqc(struct ixgbe_adapter *adapter)
{
	u32 mrqc = 0;
	int mask;

	if (!(adapter->hw.mac.type == ixgbe_mac_82599EB))
		return mrqc;

	mask = adapter->flags & (IXGBE_FLAG_RSS_ENABLED
				 | IXGBE_FLAG_DCB_ENABLED
				);

	switch (mask) {
	case (IXGBE_FLAG_RSS_ENABLED):
		mrqc = IXGBE_MRQC_RSSEN;
		break;
	case (IXGBE_FLAG_DCB_ENABLED):
		mrqc = IXGBE_MRQC_RT8TCEN;
		break;
	default:
		break;
	}

	return mrqc;
}

/**
 * ixgbe_configure_rx - Configure 8259x Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void ixgbe_configure_rx(struct ixgbe_adapter *adapter)
{
	u64 rdba;
	struct ixgbe_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	int i, j;
	u32 rdlen, rxctrl, rxcsum;
	static const u32 seed[10] = { 0x05050505, 0x05050505, 0x05050505,
	                  0x05050505, 0x05050505, 0x05050505, 0x05050505,
	                  0x05050505, 0x05050505, 0x05050505};
	u32 fctrl, hlreg0;
	u32 reta = 0, mrqc = 0;
	u32 rdrxctl;
#ifndef IXGBE_NO_HW_RSC
	u32 rscctrl;
#endif /* IXGBE_NO_HW_RSC */
	int rx_buf_len;

	/* Decide whether to use packet split mode or not */
	if (netdev->mtu > ETH_DATA_LEN) {
		if (adapter->flags & IXGBE_FLAG_RX_PS_CAPABLE)
			adapter->flags |= IXGBE_FLAG_RX_PS_ENABLED;
		else
			adapter->flags &= ~IXGBE_FLAG_RX_PS_ENABLED;
	} else {
		if (adapter->flags & IXGBE_FLAG_RX_1BUF_CAPABLE) {
			adapter->flags &= ~IXGBE_FLAG_RX_PS_ENABLED;
		} else
			adapter->flags |= IXGBE_FLAG_RX_PS_ENABLED;
	}

	/* Set the RX buffer length according to the mode */
	if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED) {
		rx_buf_len = IXGBE_RX_HDR_SIZE;
		if (hw->mac.type == ixgbe_mac_82599EB) {
			/* PSRTYPE must be initialized in 82599 */
			u32 psrtype = IXGBE_PSRTYPE_TCPHDR |
			              IXGBE_PSRTYPE_UDPHDR |
			              IXGBE_PSRTYPE_IPV4HDR |
			              IXGBE_PSRTYPE_IPV6HDR |
			              IXGBE_PSRTYPE_L2HDR;
			IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0), psrtype);
		}
	} else {
#ifndef IXGBE_NO_HW_RSC
		if (!(adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) &&
		    (netdev->mtu <= ETH_DATA_LEN))
#else
		if (netdev->mtu <= ETH_DATA_LEN)
#endif /* IXGBE_NO_HW_RSC */
			rx_buf_len = MAXIMUM_ETHERNET_VLAN_SIZE;
		else
			rx_buf_len = ALIGN(max_frame, 1024);
	}

	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF; /* discard pause frames when FC enabled */
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (adapter->netdev->mtu <= ETH_DATA_LEN)
		hlreg0 &= ~IXGBE_HLREG0_JUMBOEN;
	else
		hlreg0 |= IXGBE_HLREG0_JUMBOEN;
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);

	/* disable receives while setting up the descriptors */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	rdlen = adapter->rx_ring[0].count * sizeof(union ixgbe_adv_rx_desc);
	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	for (i = 0; i < adapter->num_rx_queues; i++) {
		rdba = adapter->rx_ring[i].dma;
		j = adapter->rx_ring[i].reg_idx;
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(j), (rdba & DMA_BIT_MASK(32)));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(j), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(j), rdlen);
		IXGBE_WRITE_REG(hw, IXGBE_RDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(j), 0);
		adapter->rx_ring[i].head = IXGBE_RDH(j);
		adapter->rx_ring[i].tail = IXGBE_RDT(j);
		adapter->rx_ring[i].rx_buf_len = rx_buf_len;

		ixgbe_configure_srrctl(adapter, j);
	}

	if (hw->mac.type == ixgbe_mac_82598EB) {
		/*
		 * For VMDq support of different descriptor types or
		 * buffer sizes through the use of multiple SRRCTL
		 * registers, RDRXCTL.MVMEN must be set to 1
		 *
		 * also, the manual doesn't mention it clearly but DCA hints
		 * will only use queue 0's tags unless this bit is set.  Side
		 * effects of setting this bit are only that SRRCTL must be
		 * fully programmed [0..15]
		 */
		rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
		rdrxctl |= IXGBE_RDRXCTL_MVMEN;
		IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
	}

	/* Program MRQC for the distribution of queues */
	mrqc = ixgbe_setup_mrqc(adapter);

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		/* Fill out redirection table */
		for (i = 0, j = 0; i < 128; i++, j++) {
			if (j == adapter->ring_feature[RING_F_RXQ].indices)
				j = 0;
			/* reta = 4-byte sliding window of
			 * 0x00..(indices-1)(indices-1)00..etc. */
			reta = (reta << 8) | (j * 0x11);
			if ((i & 3) == 3)
				IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
		}

		/* Fill out hash function seeds */
		for (i = 0; i < 10; i++)
			IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), seed[i]);

		if (hw->mac.type == ixgbe_mac_82598EB)
			mrqc |= IXGBE_MRQC_RSSEN;
		    /* Perform hash on these packet types */
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4
		      | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		      | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		      | IXGBE_MRQC_RSS_FIELD_IPV6
		      | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
		      | IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	}
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED ||
	    adapter->flags & IXGBE_FLAG_RX_CSUM_ENABLED) {
		/* Disable indicating checksum in descriptor, enables
		 * RSS hash */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}
	if (!(rxcsum & IXGBE_RXCSUM_PCSD)) {
		/* Enable IPv4 payload checksum for UDP fragments
		 * if PCSD is not set */
		rxcsum |= IXGBE_RXCSUM_IPPCSE;
	}

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
#ifndef IXGBE_NO_HW_RSC
		if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
			rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
#endif /* IXGBE_NO_HW_RSC */
		rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
		IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
	}

#ifndef IXGBE_NO_HW_RSC
	if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) {
		/* Enable 82599 HW RSC */
		for (i = 0; i < adapter->num_rx_queues; i++) {
			j = adapter->rx_ring[i].reg_idx;
			rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(j));
			rscctrl |= IXGBE_RSCCTL_RSCEN;
			/*
			 * we must limit the number of descriptors so that
			 * the total size of max desc * buf_len is not greater
			 * than 65535
			 */
			if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED) {
#if (MAX_SKB_FRAGS > 16)
				rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
#elif (MAX_SKB_FRAGS > 8)
				rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
#elif (MAX_SKB_FRAGS > 4)
				rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
#else
				rscctrl |= IXGBE_RSCCTL_MAXDESC_1;
#endif
			} else {
				if (rx_buf_len < IXGBE_RXBUFFER_4096)
					rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
				else if (rx_buf_len < IXGBE_RXBUFFER_8192)
					rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
				else
					rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
			}


			IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(j), rscctrl);

		}
		/* Enable TCP header recognition in PSRTYPE */
		IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0),
			(IXGBE_READ_REG(hw, IXGBE_PSRTYPE(0)) |
			 IXGBE_PSRTYPE_TCPHDR));

		/* Disable RSC for ACK packets */
		IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
		   (IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));
	}
#endif /* IXGBE_NO_HW_RSC */
}

#ifdef NETIF_F_HW_VLAN_TX
static void ixgbe_vlan_rx_register(struct net_device *netdev,
                                   struct vlan_group *grp)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u32 ctrl;
	int i, j;

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_disable(adapter);
	adapter->vlgrp = grp;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		/* always enable VLAN tag insert/strip */
		ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
		ctrl |= IXGBE_VLNCTRL_VME | IXGBE_VLNCTRL_VFE;
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
	} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		/* enable VLAN tag insert/strip */
		ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
		ctrl |= IXGBE_VLNCTRL_VFE;
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
		for (i = 0; i < adapter->num_rx_queues; i++) {
			j = adapter->rx_ring[i].reg_idx;
			ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_RXDCTL(j));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXDCTL(j), ctrl);
		}
	}

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter, true, true);
}

static void ixgbe_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int i;
#ifndef HAVE_NETDEV_VLAN_FEATURES
	struct net_device *v_netdev;
#endif /* HAVE_NETDEV_VLAN_FEATURES */

	/* add VID to filter table */
	if (hw->mac.ops.set_vfta) {
		hw->mac.ops.set_vfta(hw, vid, 0, true);
		if ((adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) &&
		    (adapter->hw.mac.type == ixgbe_mac_82599EB)) {
			/* enable vlan id for all pools */
			for (i = 1; i < adapter->num_rx_pools; i++) {
				hw->mac.ops.set_vfta(hw, vid, VMDQ_P(i), true);
			}
		}
	}
#ifndef HAVE_NETDEV_VLAN_FEATURES
	/*
	 * Copy feature flags from netdev to the vlan netdev for this vid.
	 * This allows things like TSO to bubble down to our vlan device.
	 */
	v_netdev = vlan_group_get_device(adapter->vlgrp, vid);
	v_netdev->features |= adapter->netdev->features;
	vlan_group_set_device(adapter->vlgrp, vid, v_netdev);
#endif /* HAVE_NETDEV_VLAN_FEATURES */
}

static void ixgbe_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int i;

	/* User is not allowed to remove vlan ID 0 */
	if (!vid)
		return;

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_disable(adapter);

	vlan_group_set_device(adapter->vlgrp, vid, NULL);

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter, true, true);
	/* remove VID from filter table */
	if (hw->mac.ops.set_vfta) {
		hw->mac.ops.set_vfta(hw, vid, 0, false);
		if ((adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) &&
		    (adapter->hw.mac.type == ixgbe_mac_82599EB)) {
			/* remove vlan id from all pools */
			for (i = 1; i < adapter->num_rx_pools; i++) {
				hw->mac.ops.set_vfta(hw, vid, VMDQ_P(i), false);
			}
		}
	}
}

static void ixgbe_restore_vlan(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	ixgbe_vlan_rx_register(adapter->netdev, adapter->vlgrp);

	/* add vlan ID 0 so we always accept priority-tagged traffic */
	if (hw->mac.ops.set_vfta)
		hw->mac.ops.set_vfta(hw, 0, 0, true);

	if (adapter->vlgrp) {
		u16 vid;
		for (vid = 0; vid < VLAN_N_VID; vid++) {
			if (!vlan_group_get_device(adapter->vlgrp, vid))
				continue;
			ixgbe_vlan_rx_add_vid(adapter->netdev, vid);
		}
	}
}

#endif

static u8 *ixgbe_addr_list_itr(struct ixgbe_hw *hw, u8 **mc_addr_ptr, u32 *vmdq)
{
#ifdef NETDEV_HW_ADDR_T_MULTICAST
	struct netdev_hw_addr *mc_ptr;
#else
	struct dev_mc_list *mc_ptr;
#endif
	u8 *addr = *mc_addr_ptr;

	*vmdq = 0;

#ifdef NETDEV_HW_ADDR_T_MULTICAST
	mc_ptr = container_of(addr, struct netdev_hw_addr, addr[0]);
	if (mc_ptr->list.next) {
		struct netdev_hw_addr *ha;

		ha = list_entry(mc_ptr->list.next, struct netdev_hw_addr, list);
		*mc_addr_ptr = ha->addr;
	}
#else
	mc_ptr = container_of(addr, struct dev_mc_list, dmi_addr[0]);
	if (mc_ptr->next)
		*mc_addr_ptr = mc_ptr->next->dmi_addr;
#endif
	else
		*mc_addr_ptr = NULL;

	return addr;
}

/**
 * ixgbe_set_rx_mode - Unicast, Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_rx_method entry point is called whenever the unicast/multicast
 * address list or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper unicast, multicast and
 * promiscuous mode.
 **/
void ixgbe_set_rx_mode(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
#if defined(HAVE_SET_RX_MODE) && defined(HAVE_NETDEV_HW_ADDR)
	struct netdev_hw_addr *ha;
#endif
	u32 fctrl, vlnctrl;
	u8 *addr_list = NULL;
	int addr_count = 0;

	/* Check for Promiscuous and All Multicast modes */

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = 1;
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		vlnctrl &= ~IXGBE_VLNCTRL_VFE;
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			fctrl |= IXGBE_FCTRL_MPE;
			fctrl &= ~IXGBE_FCTRL_UPE;
		} else {
			fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		}
		vlnctrl |= IXGBE_VLNCTRL_VFE;
		hw->addr_ctrl.user_set_promisc = 0;
		fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	}

	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);

#ifdef HAVE_SET_RX_MODE
	/* reprogram secondary unicast list */
#ifdef HAVE_NETDEV_HW_ADDR
	/*
	 * Zero out addr_count and the addr_list.  We'll program
	 * the RARs by hand after they've been cleared.
	 */
	addr_list = NULL;
	addr_count = 0;
#else
	addr_count = netdev->uc_count;
	if (addr_count)
		addr_list = netdev->uc_list->dmi_addr;
#endif /* HAVE_NETDEV_HW_ADDR */
	if (hw->mac.ops.update_uc_addr_list)
		hw->mac.ops.update_uc_addr_list(hw, addr_list, addr_count,
		                                ixgbe_addr_list_itr);
#ifdef HAVE_NETDEV_HW_ADDR
	if (netdev->uc.count) {
		/* Program the RARs, one by one */
		list_for_each_entry(ha, &netdev->uc.list, list) {
			ixgbe_add_uc_addr(hw, ha->addr, 0);
		}
	}

#endif /* HAVE_NETDEV_HW_ADDR */
#endif /* HAVE_SET_RX_MODE */
	/* reprogram multicast list */
	addr_count = netdev_mc_count(netdev);
	if (addr_count) {
#ifdef NETDEV_HW_ADDR_T_MULTICAST
		ha = list_first_entry(&netdev->mc.list, struct netdev_hw_addr, list);
		addr_list = ha->addr;
#else
		addr_list = netdev->mc_list->dmi_addr;
#endif
	}
	if (hw->mac.ops.update_mc_addr_list)
		hw->mac.ops.update_mc_addr_list(hw, addr_list, addr_count,
		                                ixgbe_addr_list_itr);
}

static void ixgbe_napi_enable_all(struct ixgbe_adapter *adapter)
{
#ifdef CONFIG_IXGBE_NAPI
	int q_idx;
	struct ixgbe_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/* legacy and MSI only use one vector */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		q_vectors = 1;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		struct napi_struct *napi;
		q_vector = adapter->q_vector[q_idx];
		napi = &q_vector->napi;
		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
			if (!q_vector->rxr_count || !q_vector->txr_count) {
				if (q_vector->txr_count == 1)
					napi->poll = &ixgbe_clean_txonly;
				else if (q_vector->rxr_count == 1)
					napi->poll = &ixgbe_clean_rxonly;
			}
		}

		napi_enable(napi);
	}
#endif /* CONFIG_IXGBE_NAPI */
}

static void ixgbe_napi_disable_all(struct ixgbe_adapter *adapter)
{
#ifdef CONFIG_IXGBE_NAPI
	int q_idx;
	struct ixgbe_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/* legacy and MSI only use one vector */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		q_vectors = 1;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		q_vector = adapter->q_vector[q_idx];
		napi_disable(&q_vector->napi);
	}
#endif
}

/*
 * ixgbe_configure_dcb - Configure DCB hardware
 * @adapter: ixgbe adapter struct
 *
 * This is called by the driver on open to configure the DCB hardware.
 * This is also called by the gennetlink interface when reconfiguring
 * the DCB state.
 */
static void ixgbe_configure_dcb(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 txdctl, vlnctrl;
	s32 err;
	int i, j;

	err = ixgbe_dcb_check_config(&adapter->dcb_cfg);
	if (err)
		DPRINTK(DRV, ERR, "err in dcb_check_config\n");
	err = ixgbe_dcb_calculate_tc_credits(&adapter->dcb_cfg, DCB_TX_CONFIG);
	if (err)
		DPRINTK(DRV, ERR, "err in dcb_calculate_tc_credits (TX)\n");
	err = ixgbe_dcb_calculate_tc_credits(&adapter->dcb_cfg, DCB_RX_CONFIG);
	if (err)
		DPRINTK(DRV, ERR, "err in dcb_calculate_tc_credits (RX)\n");

	/* reconfigure the hardware */
	ixgbe_dcb_hw_config(&adapter->hw, &adapter->dcb_cfg);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		/* PThresh workaround for Tx hang with DFP enabled. */
		txdctl |= 32;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);
	}
	/* Enable VLAN tag insert/strip */
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB) {
		vlnctrl |= IXGBE_VLNCTRL_VME | IXGBE_VLNCTRL_VFE;
		vlnctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
	} else if (hw->mac.type == ixgbe_mac_82599EB) {
		vlnctrl |= IXGBE_VLNCTRL_VFE;
		vlnctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
		for (i = 0; i < adapter->num_rx_queues; i++) {
			j = adapter->rx_ring[i].reg_idx;
			vlnctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(j));
			vlnctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(j), vlnctrl);
		}
	}
	if (hw->mac.ops.set_vfta)
		hw->mac.ops.set_vfta(hw, 0, 0, true);
}

#ifndef IXGBE_NO_LLI
static void ixgbe_configure_lli_82599(struct ixgbe_adapter *adapter)
{
	u16 port;

	if (adapter->lli_etype) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_L34T_IMIR(0),
		                (IXGBE_IMIR_LLI_EN_82599 | IXGBE_IMIR_SIZE_BP_82599 |
		                 IXGBE_IMIR_CTRL_BP_82599));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_ETQS(0), IXGBE_ETQS_LLI);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_ETQF(0),
		                (adapter->lli_etype | IXGBE_ETQF_FILTER_EN));
	}

	if (adapter->lli_port) {
		port = ntohs((u16)adapter->lli_port);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_L34T_IMIR(0),
		                (IXGBE_IMIR_LLI_EN_82599 | IXGBE_IMIR_SIZE_BP_82599 |
		                 IXGBE_IMIR_CTRL_BP_82599));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FTQF(0),
		                (IXGBE_FTQF_POOL_MASK_EN |
		                 (IXGBE_FTQF_PRIORITY_MASK <<
		                  IXGBE_FTQF_PRIORITY_SHIFT) |
		                 (IXGBE_FTQF_DEST_PORT_MASK <<
		                  IXGBE_FTQF_5TUPLE_MASK_SHIFT)));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_SDPQF(0), (port << 16));
	}

	if (adapter->flags & IXGBE_FLAG_LLI_PUSH) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_L34T_IMIR(0),
		                (IXGBE_IMIR_LLI_EN_82599 | IXGBE_IMIR_SIZE_BP_82599 |
		                 IXGBE_IMIR_CTRL_PSH_82599 | IXGBE_IMIR_CTRL_SYN_82599 |
		                 IXGBE_IMIR_CTRL_URG_82599 | IXGBE_IMIR_CTRL_ACK_82599 |
		                 IXGBE_IMIR_CTRL_RST_82599 | IXGBE_IMIR_CTRL_FIN_82599));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FTQF(0),
		                (IXGBE_FTQF_POOL_MASK_EN |
		                 (IXGBE_FTQF_PRIORITY_MASK <<
		                  IXGBE_FTQF_PRIORITY_SHIFT) |
		                 (IXGBE_FTQF_5TUPLE_MASK_MASK <<
		                  IXGBE_FTQF_5TUPLE_MASK_SHIFT)));

		IXGBE_WRITE_REG(&adapter->hw, IXGBE_LLITHRESH, 0xfc000000);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_SYNQF, 0x80000100);
	}

	if (adapter->lli_size) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_L34T_IMIR(0),
		                (IXGBE_IMIR_LLI_EN_82599 | IXGBE_IMIR_CTRL_BP_82599));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_LLITHRESH, adapter->lli_size);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FTQF(0),
		                (IXGBE_FTQF_POOL_MASK_EN |
		                 (IXGBE_FTQF_PRIORITY_MASK <<
		                  IXGBE_FTQF_PRIORITY_SHIFT) |
		                 (IXGBE_FTQF_5TUPLE_MASK_MASK <<
		                  IXGBE_FTQF_5TUPLE_MASK_SHIFT)));
	}

	if (adapter->lli_vlan_pri) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIRVP,
		                (IXGBE_IMIRVP_PRIORITY_EN | adapter->lli_vlan_pri));
	}
}

static void ixgbe_configure_lli(struct ixgbe_adapter *adapter)
{
	u16 port;

	if (adapter->lli_port) {
		/* use filter 0 for port */
		port = ntohs((u16)adapter->lli_port);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIR(0),
		                (port | IXGBE_IMIR_PORT_IM_EN));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIREXT(0),
		                (IXGBE_IMIREXT_SIZE_BP |
		                 IXGBE_IMIREXT_CTRL_BP));
	}

	if (adapter->flags & IXGBE_FLAG_LLI_PUSH) {
		/* use filter 1 for push flag */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIR(1),
		                (IXGBE_IMIR_PORT_BP | IXGBE_IMIR_PORT_IM_EN));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIREXT(1),
		                (IXGBE_IMIREXT_SIZE_BP |
		                 IXGBE_IMIREXT_CTRL_PSH));
	}

	if (adapter->lli_size) {
		/* use filter 2 for size */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIR(2),
		                (IXGBE_IMIR_PORT_BP | IXGBE_IMIR_PORT_IM_EN));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IMIREXT(2),
		                (adapter->lli_size | IXGBE_IMIREXT_CTRL_BP));
	}
}

#endif /* IXGBE_NO_LLI */
static void ixgbe_configure(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int i;
	struct ixgbe_hw *hw = &adapter->hw;

	ixgbe_set_rx_mode(netdev);

#ifdef NETIF_F_HW_VLAN_TX
	ixgbe_restore_vlan(adapter);
#endif
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		netif_set_gso_max_size(netdev, 32768);
		ixgbe_configure_dcb(adapter);
	} else {
		netif_set_gso_max_size(netdev, 65536);
	}

	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
		ixgbe_init_fdir_signature_82599(hw, adapter->fdir_pballoc);
	else if (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		ixgbe_init_fdir_perfect_82599(hw, adapter->fdir_pballoc);

	ixgbe_configure_tx(adapter);
	ixgbe_configure_rx(adapter);

	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct ixgbe_ring *ring = &adapter->rx_ring[i];
		ixgbe_alloc_rx_buffers(adapter, ring, ring->count - 1);
	}
}

static inline bool ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_tw_tyco:
	case ixgbe_phy_tw_unknown:
		return true;
	default:
		return false;
	}
}

/**
 * ixgbe_sfp_link_config - set up SFP+ link
 * @adapter: pointer to private adapter struct
 **/
static void ixgbe_sfp_link_config(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

		if (hw->phy.multispeed_fiber) {
			/*
			 * In multispeed fiber setups, the device may not have
			 * had a physical connection when the driver loaded.
			 * If that's the case, the initial link configuration
			 * couldn't get the MAC into 10G or 1G mode, so we'll
			 * never have a link status change interrupt fire.
			 * We need to try and force an autonegotiation
			 * session, then bring up link.
			 */
			hw->mac.ops.setup_sfp(hw);
			if (!(adapter->flags & IXGBE_FLAG_IN_SFP_LINK_TASK))
				schedule_work(&adapter->multispeed_fiber_task);
		} else {
			/*
			 * Direct Attach Cu and non-multispeed fiber modules
			 * still need to be configured properly prior to
			 * attempting link.
			 */
			if (!(adapter->flags & IXGBE_FLAG_IN_SFP_MOD_TASK))
				schedule_work(&adapter->sfp_config_module_task);
		}
}

/**
 * ixgbe_non_sfp_link_config - set up non-SFP+ link
 * @hw: pointer to private hardware struct
 *
 * Returns 0 on success, negative on failure
 **/
static int ixgbe_non_sfp_link_config(struct ixgbe_hw *hw)
{
	u32 autoneg;
	bool link_up = false;
	u32 ret = IXGBE_ERR_LINK_SETUP;

	if (hw->mac.ops.check_link)
		ret = hw->mac.ops.check_link(hw, &autoneg, &link_up, false);

	if (ret)
		goto link_cfg_out;

	autoneg = hw->phy.autoneg_advertised;
	if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
		ret = hw->mac.ops.get_link_capabilities(hw, &autoneg,
		                                        &hw->mac.autoneg);
	if (ret)
		goto link_cfg_out;

	if (hw->mac.ops.setup_link_speed)
		ret = hw->mac.ops.setup_link_speed(hw, autoneg, true, link_up);
link_cfg_out:
	return ret;
}

#define IXGBE_MAX_RX_DESC_POLL 10

static inline void ixgbe_rx_desc_queue_enable(struct ixgbe_adapter *adapter,
					      int rxr)
{
	int j = adapter->rx_ring[rxr].reg_idx;
	int k;

	for (k = 0; k < IXGBE_MAX_RX_DESC_POLL; k++) {
		if (IXGBE_READ_REG(&adapter->hw,
		                   IXGBE_RXDCTL(j)) & IXGBE_RXDCTL_ENABLE)
			break;
		else
			msleep(1);
	}
	if (k >= IXGBE_MAX_RX_DESC_POLL) {
		DPRINTK(DRV, ERR, "RXDCTL.ENABLE on Rx queue %d "
		        "not set within the polling period\n", rxr);
	}
	ixgbe_release_rx_desc(&adapter->hw, &adapter->rx_ring[rxr],
	                      (adapter->rx_ring[rxr].count - 1));
}

static int ixgbe_up_complete(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	int i, j = 0;
	int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	int err;
#ifdef IXGBE_TCP_TIMER
	u32 tcp_timer;
#endif
	u32 txdctl, rxdctl, mhadd;
	u32 dmatxctl;
	u32 gpie;

	ixgbe_get_hw_control(adapter);

#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	if (adapter->num_tx_queues > 1)
		netdev->features |= NETIF_F_MULTI_QUEUE;

#endif
	if ((adapter->flags & IXGBE_FLAG_MSIX_ENABLED) ||
	    (adapter->flags & IXGBE_FLAG_MSI_ENABLED)) {
		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
			gpie = (IXGBE_GPIE_MSIX_MODE | IXGBE_GPIE_EIAME |
			        IXGBE_GPIE_PBA_SUPPORT | IXGBE_GPIE_OCD);
		} else {
			/* MSI only */
			gpie = 0;
		}
		/* XXX: to interrupt immediately for EICS writes, enable this */
		/* gpie |= IXGBE_GPIE_EIMEN; */
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
#ifdef IXGBE_TCP_TIMER

		tcp_timer = IXGBE_READ_REG(hw, IXGBE_TCPTIMER);
		tcp_timer |= IXGBE_TCPTIMER_DURATION_MASK;
		tcp_timer |= (IXGBE_TCPTIMER_KS |
		              IXGBE_TCPTIMER_COUNT_ENABLE |
		              IXGBE_TCPTIMER_LOOP);
		IXGBE_WRITE_REG(hw, IXGBE_TCPTIMER, tcp_timer);
		tcp_timer = IXGBE_READ_REG(hw, IXGBE_TCPTIMER);
#endif
	}

#ifdef CONFIG_IXGBE_NAPI
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED)) {
		/* legacy interrupts, use EIAM to auto-mask when reading EICR,
		 * specifically only auto mask tx and rx interrupts */
		IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}

#endif
	/* Enable fan failure interrupt */
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) {
		gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
		gpie |= IXGBE_SDP1_GPIEN;
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
		gpie |= IXGBE_SDP1_GPIEN;
		gpie |= IXGBE_SDP2_GPIEN;
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
	}

	mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
	if (max_frame != (mhadd >> IXGBE_MHADD_MFS_SHIFT)) {
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= max_frame << IXGBE_MHADD_MFS_SHIFT;

		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}

	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		/*
		 * tuned. - adaline
		 * WTHRESH: 8 -> 16
		 * HTHRESH: 0
		 * PTHRESH: 0 -> 32
		 */
		txdctl &= ~0x3FFFFF;
		txdctl |=  0x100020;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		/* DMATXCTL.EN must be set after all Tx queue config is done */
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
	}

	for (i = 0; i < adapter->num_tx_queues; i++) {
		int wait_loop = 10;
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);

		if (hw->mac.type == ixgbe_mac_82599EB) {
			/* poll for Tx Enable ready */
			do {
				msleep(1);
				txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
			} while (--wait_loop && 
			         !(txdctl & IXGBE_TXDCTL_ENABLE));
			if (!wait_loop)
				DPRINTK(DRV, ERR, "Could not enable "
				        "Tx Queue %d\n", j);
		}
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		j = adapter->rx_ring[i].reg_idx;
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(j));
		if (hw->mac.type == ixgbe_mac_82598EB) {
			/*
			 * enable cache line friendly hardware writes:
			 * PTHRESH=32 descriptors (half the internal cache),
			 * this also removes ugly rx_no_buffer_count increment
			 * HTHRESH=4 descriptors (to minimize latency on fetch)
			 * WTHRESH=8 burst writeback up to two cachelines
			 * tuned. - adaline
			 * WTHRESH: 8 -> 16
			 * HTHRESH: 4 -> 8
			 * PTHRESH: 32 -> 32
			 */
			rxdctl &= ~0x3FFFFF;
			rxdctl |=  0x100820;
		}
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(j), rxdctl);
		if (hw->mac.type == ixgbe_mac_82599EB)
			ixgbe_rx_desc_queue_enable(adapter, i);
	}
	/* enable all receives */
	rxdctl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxdctl |= (IXGBE_RXCTRL_DMBYPS | IXGBE_RXCTRL_RXEN);
	else
		rxdctl |= IXGBE_RXCTRL_RXEN;
	ixgbe_enable_rx_dma(hw, rxdctl);

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
		ixgbe_configure_msix(adapter);
	else
		ixgbe_configure_msi_and_legacy(adapter);
#ifndef IXGBE_NO_LLI
	/* lli should only be enabled with MSI-X and MSI */
	if (adapter->flags & IXGBE_FLAG_MSI_ENABLED ||
	    adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		if (adapter->hw.mac.type == ixgbe_mac_82599EB)
			ixgbe_configure_lli_82599(adapter);
		else
			ixgbe_configure_lli(adapter);
	}

#endif
	clear_bit(__IXGBE_DOWN, &adapter->state);
	ixgbe_napi_enable_all(adapter);

	/*
	 * For hot-pluggable SFP+ devices, a SFP+ module may have arrived
	 * before interrupts were enabled but after probe.  Such devices
	 * wouldn't have their type indentified yet.  We need to kick off
	 * the SFP+ module setup first, then try to bring up link.  If we're
	 * not hot-pluggable SFP+, we just need to configure link and bring 
	 * it up.
	 */
	if (hw->phy.type == ixgbe_phy_none) {
		err = hw->phy.ops.identify_sfp(hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			/*
			 * Take the device down and set schedule sfp tasklet 
			 * which will unregister_netdev it and log it.
			 */
			ixgbe_down(adapter);
			schedule_work(&adapter->sfp_config_module_task);
			return err;
		}
	}

	if (ixgbe_is_sfp(hw)) {
		ixgbe_sfp_link_config(adapter);
	} else {
		err = ixgbe_non_sfp_link_config(hw);
		if (err)
			DPRINTK(PROBE, ERR, "link_config FAILED %d\n", err);
	}

	/* enable transmits */
	netif_tx_start_all_queues(netdev);

	/* bring the link up in the watchdog, this could race with our first
	 * link up interrupt but shouldn't be a problem */
	adapter->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	adapter->link_check_timeout = jiffies;
	mod_timer(&adapter->watchdog_timer, jiffies);
	for (i = 0; i < adapter->num_tx_queues; i++)
		set_bit(__IXGBE_FDIR_INIT_DONE,
		        &(adapter->tx_ring[i].reinit_state));
	return 0;
}

void ixgbe_reinit_locked(struct ixgbe_adapter *adapter)
{
	WARN_ON(in_interrupt());
	while (test_and_set_bit(__IXGBE_RESETTING, &adapter->state))
		msleep(1);
	ixgbe_down(adapter);
	ixgbe_up(adapter);
	clear_bit(__IXGBE_RESETTING, &adapter->state);
}

int ixgbe_up(struct ixgbe_adapter *adapter)
{
	int err;
	struct ixgbe_hw *hw = &adapter->hw;

	ixgbe_configure(adapter);

	err = ixgbe_up_complete(adapter);

	/* clear any pending interrupts, may auto mask */
	IXGBE_READ_REG(hw, IXGBE_EICR);
	ixgbe_irq_enable(adapter, true, true);

	return err;
}

void ixgbe_reset(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int err;

	err = hw->mac.ops.init_hw(hw);
	switch (err) {
	case 0:
	case IXGBE_ERR_SFP_NOT_PRESENT:
		break;
	case IXGBE_ERR_MASTER_REQUESTS_PENDING:
		DPRINTK(HW, INFO, "master disable timed out\n");
		break;
	case IXGBE_ERR_EEPROM_VERSION:
		/* We are running on a pre-production device, log a warning */
		DPRINTK(PROBE, INFO, "This device is a pre-production adapter/"
		        "LOM.  Please be aware there may be issues associated "
		        "with your hardware.  If you are experiencing problems "
		        "please contact your Intel or hardware representative "
		        "who provided you with this hardware.\n");
		break;
	default:
		DPRINTK(PROBE, ERR, "Hardware Error: %d\n", err);
	}

	/* reprogram the RAR[0] in case user changed it. */
	if (hw->mac.ops.set_rar)
		hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
}

/**
 * ixgbe_clean_rx_ring - Free Rx Buffers per Queue
 * @adapter: board private structure
 * @rx_ring: ring to free buffers from
 **/
static void ixgbe_clean_rx_ring(struct ixgbe_adapter *adapter,
                         struct ixgbe_ring *rx_ring)
{
	unsigned long size;
	unsigned int i;

	spin_lock(&rx_ring->lock);

	/* Free all the Rx ring sk_buffs */

	for (i = 0; i < rx_ring->count; i++) {
		struct ixgbe_rx_buffer *rx_buffer_info;

		rx_buffer_info = &rx_ring->rx_buffer_info[i];
	}

	size = sizeof(struct ixgbe_rx_buffer) * rx_ring->count;
	memset(rx_ring->rx_buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->queued = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	if (rx_ring->head)
		writel(0, adapter->hw.hw_addr + rx_ring->head);
	if (rx_ring->tail)
		writel(0, adapter->hw.hw_addr + rx_ring->tail);

	spin_unlock(&rx_ring->lock);
}

/**
 * ixgbe_clean_tx_ring - Free Tx Buffers
 * @adapter: board private structure
 * @tx_ring: ring to be cleaned
 **/
static void ixgbe_clean_tx_ring(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *tx_ring)
{
	unsigned long size;

	spin_lock(&tx_ring->lock);

	size = sizeof(struct ixgbe_tx_buffer) * tx_ring->count;
	memset(tx_ring->tx_buffer_info, 0, size);

	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	if (tx_ring->head)
		writel(0, adapter->hw.hw_addr + tx_ring->head);
	if (tx_ring->tail)
		writel(0, adapter->hw.hw_addr + tx_ring->tail);

	spin_unlock(&tx_ring->lock);
}

/**
 * ixgbe_clean_all_rx_rings - Free Rx Buffers for all queues
 * @adapter: board private structure
 **/
static void ixgbe_clean_all_rx_rings(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		ixgbe_clean_rx_ring(adapter, &adapter->rx_ring[i]);
}

/**
 * ixgbe_clean_all_tx_rings - Free Tx Buffers for all queues
 * @adapter: board private structure
 **/
static void ixgbe_clean_all_tx_rings(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		ixgbe_clean_tx_ring(adapter, &adapter->tx_ring[i]);
}

void ixgbe_down(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 rxctrl;
	u32 txdctl;
	int i, j;

	/* signal that we are down to the interrupt handler */
	set_bit(__IXGBE_DOWN, &adapter->state);

	/* disable receives */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	netif_tx_disable(netdev);

	IXGBE_WRITE_FLUSH(hw);
	msleep(10);

	netif_tx_stop_all_queues(netdev);

	ixgbe_irq_disable(adapter);

	ixgbe_napi_disable_all(adapter);

	del_timer_sync(&adapter->watchdog_timer);
	/* can't call flush scheduled work here because it can deadlock
	 * if linkwatch_event tries to acquire the rtnl_lock which we are
	 * holding */
	while (adapter->flags & IXGBE_FLAG_IN_WATCHDOG_TASK)
		msleep(1);
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		cancel_work_sync(&adapter->fdir_reinit_task);

	/* disable transmits in the hardware now that interrupts are off */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j),
		                (txdctl & ~IXGBE_TXDCTL_ENABLE));
	}
	/* Disable the Tx DMA engine on 82599 */
	if (hw->mac.type == ixgbe_mac_82599EB)
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL,
				(IXGBE_READ_REG(hw, IXGBE_DMATXCTL) &
				 ~IXGBE_DMATXCTL_TE));

	netif_carrier_off(netdev);

#ifdef HAVE_PCI_ERS
	if (!pci_channel_offline(adapter->pdev))
#endif
		ixgbe_reset(adapter);
	ixgbe_clean_all_tx_rings(adapter);
	ixgbe_clean_all_rx_rings(adapter);

#ifdef IXGBE_DCA
	/* since we reset the hardware DCA settings were cleared */
	ixgbe_setup_dca(adapter);
#endif
}

#ifdef CONFIG_IXGBE_NAPI
/**
 * ixgbe_poll - NAPI Rx polling callback
 * @napi: structure for representing this polling device
 * @budget: how many packets driver is allowed to clean
 *
 * This function is used for legacy and MSI, NAPI mode
 **/
static int ixgbe_poll(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                        container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	int tx_clean_complete, work_done = 0;

#ifdef IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
		ixgbe_update_tx_dca(adapter, adapter->tx_ring);
		ixgbe_update_rx_dca(adapter, adapter->rx_ring);
	}
#endif

	tx_clean_complete = ixgbe_clean_tx_irq(adapter, adapter->tx_ring, &work_done, budget);
	ixgbe_clean_rx_irq(q_vector, adapter->rx_ring, &work_done, budget);

	if (!tx_clean_complete)
		work_done = budget;

#ifndef HAVE_NETDEV_NAPI_LIST
	if (!netif_running(adapter->netdev))
		work_done = 0;

#endif
	/* If no Tx and not enough Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->itr_setting & 1)
			ixgbe_set_itr(adapter);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter, IXGBE_EIMS_RTX_QUEUE);
	}
	return work_done;
}

#endif /* CONFIG_IXGBE_NAPI */
/**
 * ixgbe_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/
static void ixgbe_tx_timeout(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	/* Do the reset outside of interrupt context */
	schedule_work(&adapter->reset_task);
}

static void ixgbe_reset_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter;
	adapter = container_of(work, struct ixgbe_adapter, reset_task);

	/* If we're already down or resetting, just bail */
	if (test_bit(__IXGBE_DOWN, &adapter->state) ||
	    test_bit(__IXGBE_RESETTING, &adapter->state))
		return;

	adapter->tx_timeout_count++;

	ixgbe_reinit_locked(adapter);
}


/**
 * ixgbe_set_dcb_queues: Allocate queues for a DCB-enabled device
 * @adapter: board private structure to initialize
 *
 * When DCB (Data Center Bridging) is enabled, allocate queues for
 * each traffic class.  If multiqueue isn't availabe, then abort DCB
 * initialization.
 *
 **/
static inline bool ixgbe_set_dcb_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_DCB];

	if (!(adapter->flags & IXGBE_FLAG_DCB_ENABLED))
		return ret;

#ifdef HAVE_TX_MQ
	f->mask = 0x7 << 3;
	adapter->num_rx_queues = f->indices;
	adapter->num_tx_queues = f->indices;
	ret = true;
#else
	DPRINTK(DRV, INFO, "Kernel has no multiqueue support, disabling DCB\n");
	f->mask = 0;
	f->indices = 0;
#endif

	return ret;
}

/**
 * ixgbe_set_rss_queues: Allocate queues for RSS
 * @adapter: board private structure to initialize
 *
 * This is our "base" multiqueue mode.  RSS (Receive Side Scaling) will try
 * to allocate one Rx queue per CPU, and if available, one Tx queue per CPU.
 *
 **/
static inline bool ixgbe_set_rss_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		struct ixgbe_ring_feature *f;
		
		f = &adapter->ring_feature[RING_F_RXQ];
		f->mask = 0xF;
		adapter->num_rx_queues = f->indices;
		
		f = &adapter->ring_feature[RING_F_TXQ];
		adapter->num_tx_queues = f->indices;

		ret = true;
	}

	return ret;
}

/**
 * ixgbe_set_fdir_queues: Allocate queues for Flow Director
 * @adapter: board private structure to initialize
 *
 * Flow Director is an advanced Rx filter, attempting to get Rx flows back
 * to the original CPU that initiated the Tx session.  This runs in addition
 * to RSS, so if a packet doesn't match an FDIR filter, we can still spread the
 * Rx load across CPUs using RSS.
 *
 **/
static bool inline ixgbe_set_fdir_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f_fdir = &adapter->ring_feature[RING_F_FDIR];

	f_fdir->indices = min((int)num_online_cpus(), f_fdir->indices);
	f_fdir->mask = 0;

	/* Flow Director must have RSS enabled */
	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED &&
	    ((adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	     (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)))) {
		adapter->num_rx_queues = f_fdir->indices;
#ifdef HAVE_TX_MQ
		adapter->num_tx_queues = f_fdir->indices;
#endif
		ret = true;
	} else {
		adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
		adapter->flags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
	}
	return ret;
}

/*
 * ixgbe_set_num_queues: Allocate queues for device, feature dependant
 * @adapter: board private structure to initialize
 *
 * This is the top level queue allocation routine.  The order here is very
 * important, starting with the "most" number of features turned on at once,
 * and ending with the smallest set of features.  This way large combinations
 * can be allocated if they're turned on, and smaller combinations are the
 * fallthrough conditions.
 *
 **/
static void ixgbe_set_num_queues(struct ixgbe_adapter *adapter)
{
	/* Start with base case */
	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;
	adapter->num_rx_pools = adapter->num_rx_queues;
	adapter->num_rx_queues_per_pool = 1;

	if (ixgbe_set_dcb_queues(adapter))
		return;

	if (ixgbe_set_fdir_queues(adapter))
		return;


	if (ixgbe_set_rss_queues(adapter))
		return;
}

static void ixgbe_acquire_msix_vectors(struct ixgbe_adapter *adapter,
                                       int vectors)
{
	int err, vector_threshold;

	/* We'll want at least 3 (vector_threshold):
	 * 1) TxQ[0] Cleanup
	 * 2) RxQ[0] Cleanup
	 * 3) Other (Link Status Change, etc.)
	 * 4) TCP Timer (optional)
	 */
	vector_threshold = MIN_MSIX_COUNT;

	/* The more we get, the more we will assign to Tx/Rx Cleanup
	 * for the separate queues...where Rx Cleanup >= Tx Cleanup.
	 * Right now, we simply care about how many we'll get; we'll
	 * set them up later while requesting irq's.
	 */
	while (vectors >= vector_threshold) {
		err = pci_enable_msix(adapter->pdev, adapter->msix_entries,
		                      vectors);
		if (!err) /* Success in acquiring all requested vectors. */
			break;
		else if (err < 0)
			vectors = 0; /* Nasty failure, quit now */
		else /* err == number of vectors we should try again with */
			vectors = err;
	}

	if (vectors < vector_threshold) {
		/* Can't allocate enough MSI-X interrupts?  Oh well.
		 * This just means we'll go with either a single MSI
		 * vector or fall back to legacy interrupts.
		 */
		DPRINTK(HW, DEBUG, "Unable to allocate MSI-X interrupts\n");
		adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else {
		adapter->flags |= IXGBE_FLAG_MSIX_ENABLED; /* Woot! */
		/*
		 * Adjust for only the vectors we'll use, which is minimum
		 * of max_msix_q_vectors + NON_Q_VECTORS, or the number of
		 * vectors we were allocated.
		 */
		adapter->num_msix_vectors = min(vectors,
		                   adapter->max_msix_q_vectors + NON_Q_VECTORS);
	}
}

/**
 * ixgbe_cache_ring_rss - Descriptor ring to register mapping for RSS
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for RSS to the assigned rings.
 *
 **/
static inline bool ixgbe_cache_ring_rss(struct ixgbe_adapter *adapter)
{
	int i;

	if (!(adapter->flags & IXGBE_FLAG_RSS_ENABLED))
		return false;

	for (i = 0; i < adapter->num_rx_queues; i++)
		adapter->rx_ring[i].reg_idx = i;
	for (i = 0; i < adapter->num_tx_queues; i++)
		adapter->tx_ring[i].reg_idx = i;

	return true;
}

/**
 * ixgbe_cache_ring_dcb - Descriptor ring to register mapping for DCB
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for DCB to the assigned rings.
 *
 **/
static inline bool ixgbe_cache_ring_dcb(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;
	int dcb_i = adapter->ring_feature[RING_F_DCB].indices;

	if (!(adapter->flags & IXGBE_FLAG_DCB_ENABLED))
		return false;

	/* the number of queues is assumed to be symmetric */
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		for (i = 0; i < dcb_i; i++) {
			adapter->rx_ring[i].reg_idx = i << 3;
			adapter->tx_ring[i].reg_idx = i << 2;
		}
		ret = true;
	} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		if (dcb_i == 8) {
			/*
			 * Tx TC0 starts at: descriptor queue 0
			 * Tx TC1 starts at: descriptor queue 32
			 * Tx TC2 starts at: descriptor queue 64
			 * Tx TC3 starts at: descriptor queue 80
			 * Tx TC4 starts at: descriptor queue 96
			 * Tx TC5 starts at: descriptor queue 104
			 * Tx TC6 starts at: descriptor queue 112
			 * Tx TC7 starts at: descriptor queue 120
			 *
			 * Rx TC0-TC7 are offset by 16 queues each
			 */
			for (i = 0; i < 3; i++) {
				adapter->tx_ring[i].reg_idx = i << 5;
				adapter->rx_ring[i].reg_idx = i << 4;
			}
			for ( ; i < 5; i++) {
				adapter->tx_ring[i].reg_idx = ((i + 2) << 4);
				adapter->rx_ring[i].reg_idx = i << 4;
			}
			for ( ; i < dcb_i; i++) {
				adapter->tx_ring[i].reg_idx = ((i + 8) << 3);
				adapter->rx_ring[i].reg_idx = i << 4;
			}
			ret = true;
		} else if (dcb_i == 4) {
			/*
			 * Tx TC0 starts at: descriptor queue 0
			 * Tx TC1 starts at: descriptor queue 64
			 * Tx TC2 starts at: descriptor queue 96
			 * Tx TC3 starts at: descriptor queue 112
			 *
			 * Rx TC0-TC3 are offset by 32 queues each
			 */
			adapter->tx_ring[0].reg_idx = 0;
			adapter->tx_ring[1].reg_idx = 64;
			adapter->tx_ring[2].reg_idx = 96;
			adapter->tx_ring[3].reg_idx = 112;
			for (i = 0 ; i < dcb_i; i++)
				adapter->rx_ring[i].reg_idx = i << 5;
			ret = true;
		}
	}

	return ret;
}

/**
 * ixgbe_cache_ring_fdir - Descriptor ring to register mapping for Flow Director
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for Flow Director to the assigned rings.
 *
 **/
static bool inline ixgbe_cache_ring_fdir(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED &&
	    ((adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) ||
	     (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE))) {
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i].reg_idx = i;
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i].reg_idx = i;
		ret = true;
	}

	return ret;
}

/**
 * ixgbe_cache_ring_register - Descriptor ring to register mapping
 * @adapter: board private structure to initialize
 *
 * Once we know the feature-set enabled for the device, we'll cache
 * the register offset the descriptor ring is assigned to.
 *
 * Note, the order the various feature calls is important.  It must start with
 * the "most" features enabled at the same time, then trickle down to the
 * least amount of features turned on at once.
 **/
static void ixgbe_cache_ring_register(struct ixgbe_adapter *adapter)
{
	/* start with default case */
	adapter->rx_ring[0].reg_idx = 0;
	adapter->tx_ring[0].reg_idx = 0;

	if (ixgbe_cache_ring_dcb(adapter))
		return;

	if (ixgbe_cache_ring_fdir(adapter))
		return;

	if (ixgbe_cache_ring_rss(adapter))
		return;

}

/**
 * ixgbe_alloc_queues - Allocate memory for all rings
 * @adapter: board private structure to initialize
 *
 * We allocate one ring per queue at run-time since we don't know the
 * number of queues at compile-time.  The polling_netdev array is
 * intended for Multiqueue, but should work fine with a single queue.
 **/
static int ixgbe_alloc_queues(struct ixgbe_adapter *adapter)
{
	int i;

	adapter->tx_ring = kcalloc(adapter->num_tx_queues,
	                           sizeof(struct ixgbe_ring), GFP_KERNEL);
	if (!adapter->tx_ring)
		goto err_tx_ring_allocation;

	adapter->rx_ring = kcalloc(adapter->num_rx_queues,
	                           sizeof(struct ixgbe_ring), GFP_KERNEL);

	if (!adapter->rx_ring)
		goto err_rx_ring_allocation;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		adapter->tx_ring[i].count = adapter->tx_ring_count;
		adapter->tx_ring[i].queue_index = i;
		if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
			adapter->tx_ring[i].atr_sample_rate =
				adapter->atr_sample_rate;
		adapter->tx_ring[i].atr_count = 0;
		adapter->tx_ring[i].adapter = adapter;
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		adapter->rx_ring[i].count = adapter->rx_ring_count;
		adapter->rx_ring[i].queue_index = i;
		adapter->rx_ring[i].adapter = adapter;
	}

	ixgbe_cache_ring_register(adapter);

	return 0;

err_rx_ring_allocation:
	kfree(adapter->tx_ring);
	adapter->tx_ring = NULL;
err_tx_ring_allocation:
	return -ENOMEM;
}

/**
 * ixgbe_set_interrupt_capability - set MSI-X or MSI if supported
 * @adapter: board private structure to initialize
 *
 * Attempt to configure the interrupts using the best available
 * capabilities of the hardware and the kernel.
 **/
static int ixgbe_set_interrupt_capability(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int err = 0;
	int vector, v_budget;

	if (!(adapter->flags & IXGBE_FLAG_MSIX_CAPABLE))
		goto try_msi;

	/*
	 * It's easy to be greedy for MSI-X vectors, but it really
	 * doesn't do us much good if we have a lot more vectors
	 * than CPU's.  So let's be conservative and only ask for
	 * (roughly) twice the number of vectors as there are CPU's.
	 */
	v_budget = min(adapter->num_rx_queues 
			/* disable all TX interrupts - adaline
			   + adapter->num_tx_queues */,
	               	(int)(num_online_cpus()
			/* disable all TX interrupts - adaline
			  * 2 */
			)) + NON_Q_VECTORS;

	/*
	 * At the same time, hardware can only support a maximum of
	 * hw.mac->max_msix_vectors vectors.  With features
	 * such as RSS and VMDq, we can easily surpass the number of Rx and Tx
	 * descriptor queues supported by our device.  Thus, we cap it off in
	 * those rare cases where the cpu count also exceeds our vector limit.
	 */
	v_budget = min(v_budget, (int)hw->mac.max_msix_vectors);

	/* A failure in MSI-X entry allocation isn't fatal, but it does
	 * mean we disable MSI-X capabilities of the adapter. */
	adapter->msix_entries = kcalloc(v_budget,
	                                sizeof(struct msix_entry), GFP_KERNEL);
	if (adapter->msix_entries) {
		for (vector = 0; vector < v_budget; vector++)
			adapter->msix_entries[vector].entry = vector;

		ixgbe_acquire_msix_vectors(adapter, v_budget);

		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
			goto out;

	}

	adapter->flags &= ~IXGBE_FLAG_DCB_ENABLED;
	adapter->flags &= ~IXGBE_FLAG_DCB_CAPABLE;
	adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
	adapter->flags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
	adapter->atr_sample_rate = 0;
	adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	ixgbe_set_num_queues(adapter);

try_msi:
	if (!(adapter->flags & IXGBE_FLAG_MSI_CAPABLE))
		goto out;

	err = pci_enable_msi(adapter->pdev);
	if (!err) {
		adapter->flags |= IXGBE_FLAG_MSI_ENABLED;
	} else {
		DPRINTK(HW, DEBUG, "Unable to allocate MSI interrupt, "
		                   "falling back to legacy.  Error: %d\n", err);
		/* reset err */
		err = 0;
	}

out:
#ifdef HAVE_TX_MQ
	/* Notify the stack of the (possibly) reduced Tx Queue count. */
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	adapter->netdev->egress_subqueue_count = adapter->num_tx_queues;
#else
	adapter->netdev->real_num_tx_queues = adapter->num_tx_queues;
#endif
#endif /* HAVE_TX_MQ */
	return err;
}

/**
 * ixgbe_alloc_q_vectors - Allocate memory for interrupt vectors
 * @adapter: board private structure to initialize
 *
 * We allocate one q_vector per queue interrupt.  If allocation fails we
 * return -ENOMEM.
 **/
static int ixgbe_alloc_q_vectors(struct ixgbe_adapter *adapter)
{
	int v_idx, num_q_vectors;
	struct ixgbe_q_vector *q_vector;
	int rx_vectors;
#ifdef CONFIG_IXGBE_NAPI
	int (*poll)(struct napi_struct *, int);
#endif

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
		rx_vectors = adapter->num_rx_queues;
#ifdef CONFIG_IXGBE_NAPI
		poll = &ixgbe_clean_rxtx_many;
#endif
	} else {
		num_q_vectors = 1;
		rx_vectors = 1;
#ifdef CONFIG_IXGBE_NAPI
		poll = &ixgbe_poll;
#endif
	}

	for (v_idx = 0; v_idx < num_q_vectors; v_idx++) {
		q_vector = kzalloc(sizeof(struct ixgbe_q_vector), GFP_KERNEL);
		if (!q_vector)
			goto err_out;
		q_vector->adapter = adapter;
		q_vector->eitr = adapter->eitr_param;
		q_vector->v_idx = v_idx;
#ifndef IXGBE_NO_LRO
		if (v_idx < rx_vectors) {
			int size = sizeof(struct ixgbe_lro_list);
			q_vector->lrolist = vmalloc(size);
			if (!q_vector->lrolist) {
				kfree(q_vector);
				goto err_out;
			}
			memset(q_vector->lrolist, 0, size);
			ixgbe_lro_ring_init(q_vector->lrolist);
		}
#endif
#ifdef CONFIG_IXGBE_NAPI
		netif_napi_add(adapter->netdev, &q_vector->napi, (*poll), 128);
#endif
		adapter->q_vector[v_idx] = q_vector;
	}

	return 0;

err_out:
	while (v_idx) {
		v_idx--;
		q_vector = adapter->q_vector[v_idx];
#ifdef CONFIG_IXGBE_NAPI
		netif_napi_del(&q_vector->napi);
#endif
#ifndef IXGBE_NO_LRO
		if (q_vector->lrolist) {
			ixgbe_lro_ring_exit(q_vector->lrolist);
			vfree(q_vector->lrolist);
			q_vector->lrolist = NULL;
		}
#endif
		kfree(q_vector);
		adapter->q_vector[v_idx] = NULL;
	}
	return -ENOMEM;
}

/**
 * ixgbe_free_q_vectors - Free memory allocated for interrupt vectors
 * @adapter: board private structure to initialize
 *
 * This function frees the memory allocated to the q_vectors.  In addition if
 * NAPI is enabled it will delete any references to the NAPI struct prior
 * to freeing the q_vector.
 **/
static void ixgbe_free_q_vectors(struct ixgbe_adapter *adapter)
{
	int v_idx, num_q_vectors;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
	} else {
		num_q_vectors = 1;
	}

	for (v_idx = 0; v_idx < num_q_vectors; v_idx++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[v_idx];

		adapter->q_vector[v_idx] = NULL;
#ifdef CONFIG_IXGBE_NAPI
		netif_napi_del(&q_vector->napi);
#endif
#ifndef IXGBE_NO_LRO
		if (q_vector->lrolist) {
			ixgbe_lro_ring_exit(q_vector->lrolist);
			vfree(q_vector->lrolist);
			q_vector->lrolist = NULL;
		}
#endif
		kfree(q_vector);
	}
}

static void ixgbe_reset_interrupt_capability(struct ixgbe_adapter *adapter)
{
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
		pci_disable_msix(adapter->pdev);
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else if (adapter->flags & IXGBE_FLAG_MSI_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_MSI_ENABLED;
		pci_disable_msi(adapter->pdev);
	}
	return;
}

/**
 * ixgbe_init_interrupt_scheme - Determine proper interrupt scheme
 * @adapter: board private structure to initialize
 *
 * We determine which interrupt scheme to use based on...
 * - Kernel support (MSI, MSI-X)
 *   - which can be user-defined (via MODULE_PARAM)
 * - Hardware queue count (num_*_queues)
 *   - defined by miscellaneous hardware support/features (RSS, etc.)
 **/
int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter)
{
	int err;

	/* Number of supported queues */
	ixgbe_set_num_queues(adapter);

	err = ixgbe_set_interrupt_capability(adapter);
	if (err) {
		DPRINTK(PROBE, ERR, "Unable to setup interrupt capabilities\n");
		goto err_set_interrupt;
	}

	err = ixgbe_alloc_q_vectors(adapter);
	if (err) {
		DPRINTK(PROBE, ERR, "Unable to allocate memory for queue "
		        "vectors\n");
		goto err_alloc_q_vectors;
	}

	err = ixgbe_alloc_queues(adapter);
	if (err) {
		DPRINTK(PROBE, ERR, "Unable to allocate memory for queues\n");
		goto err_alloc_queues;
	}

	DPRINTK(DRV, INFO, "Multiqueue %s: Rx Queue count = %u, "
	                   "Tx Queue count = %u\n",
	        (adapter->num_rx_queues > 1) ? "Enabled" :
	        "Disabled", adapter->num_rx_queues, adapter->num_tx_queues);

	set_bit(__IXGBE_DOWN, &adapter->state);

	return 0;
err_alloc_queues:
	ixgbe_free_q_vectors(adapter);
err_alloc_q_vectors:
	ixgbe_reset_interrupt_capability(adapter);
err_set_interrupt:
	return err;
}

/**
 * ixgbe_clear_interrupt_scheme - Clear the current interrupt scheme settings
 * @adapter: board private structure to clear interrupt scheme on
 *
 * We go through and clear interrupt specific resources and reset the structure
 * to pre-load conditions
 **/
void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter)
{
	kfree(adapter->tx_ring);
	kfree(adapter->rx_ring);
	adapter->tx_ring = NULL;
	adapter->rx_ring = NULL;

	ixgbe_free_q_vectors(adapter);
	ixgbe_reset_interrupt_capability(adapter);
}

/**
 * ixgbe_sfp_timer - worker thread to find a missing module
 * @data: pointer to our adapter struct
 **/
static void ixgbe_sfp_timer(unsigned long data)
{
	struct ixgbe_adapter *adapter = (struct ixgbe_adapter *)data;

	/* Do the sfp_timer outside of interrupt context due to the
	 * delays that sfp+ detection requires */
	schedule_work(&adapter->sfp_task);
}

/**
 * ixgbe_sfp_task - worker thread to find a missing module
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_sfp_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter = container_of(work,
	                                             struct ixgbe_adapter,
	                                             sfp_task);
	struct ixgbe_hw *hw = &adapter->hw;

	if ((hw->phy.type == ixgbe_phy_nl) &&
	    (hw->phy.sfp_type == ixgbe_sfp_type_not_present)) {
		s32 ret = hw->phy.ops.identify_sfp(hw);
		if (ret && ret != IXGBE_ERR_SFP_NOT_SUPPORTED)
			goto reschedule;
		ret = hw->phy.ops.reset(hw);
		if (ret == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			DPRINTK(PROBE, ERR, "failed to initialize because an "
			        "unsupported SFP+ module type was detected.\n"
			        "Reload the driver after installing a "
			        "supported module.\n");
			unregister_netdev(adapter->netdev);
			adapter->netdev_registered = false;
		} else {
			DPRINTK(PROBE, INFO, "detected SFP+: %d\n",
			        hw->phy.sfp_type);
		}
		/* don't need this routine any more */
		clear_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state);
	}
	return;
reschedule:
	if (test_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state))
		mod_timer(&adapter->sfp_timer,
		          round_jiffies(jiffies + (2 * HZ)));
}

/**
 * ixgbe_sw_init - Initialize general software structures (struct ixgbe_adapter)
 * @adapter: board private structure to initialize
 *
 * ixgbe_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int __devinit ixgbe_sw_init(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	int err;

	/* PCI config space info */

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	err = ixgbe_init_shared_code(hw);
	if (err) {
		DPRINTK(PROBE, ERR, "init_shared_code failed: %d\n", err);
		goto out;
	}

	/* Set capability flags */
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		if (hw->device_id == IXGBE_DEV_ID_82598AT)
			adapter->flags |= IXGBE_FLAG_FAN_FAIL_CAPABLE;
		adapter->flags |= IXGBE_FLAG_DCA_CAPABLE;
		adapter->flags |= IXGBE_FLAG_MSI_CAPABLE;
		adapter->flags |= IXGBE_FLAG_MSIX_CAPABLE;
		if (adapter->flags & IXGBE_FLAG_MSIX_CAPABLE)
			adapter->flags |= IXGBE_FLAG_MQ_CAPABLE;
		if (adapter->flags & IXGBE_FLAG_MQ_CAPABLE)
			adapter->flags |= IXGBE_FLAG_DCB_CAPABLE;
#ifdef IXGBE_RSS
		if (adapter->flags & IXGBE_FLAG_MQ_CAPABLE)
			adapter->flags |= IXGBE_FLAG_RSS_CAPABLE;
#endif
#ifndef IXGBE_NO_HW_RSC
		adapter->flags2 &= ~IXGBE_FLAG2_RSC_CAPABLE;
#endif
		adapter->max_msix_q_vectors = IXGBE_MAX_MSIX_Q_VECTORS_82598;
		break;
	case ixgbe_mac_82599EB:
#ifndef IXGBE_NO_HW_RSC
		adapter->flags2 |= IXGBE_FLAG2_RSC_CAPABLE;
#endif
		adapter->flags |= IXGBE_FLAG_DCA_CAPABLE;
		adapter->flags |= IXGBE_FLAG_MSI_CAPABLE;
		adapter->flags |= IXGBE_FLAG_MSIX_CAPABLE;
		if (adapter->flags & IXGBE_FLAG_MSIX_CAPABLE)
			adapter->flags |= IXGBE_FLAG_MQ_CAPABLE;
		if (adapter->flags & IXGBE_FLAG_MQ_CAPABLE)
			adapter->flags |= IXGBE_FLAG_DCB_CAPABLE;
#ifdef IXGBE_RSS
		if (adapter->flags & IXGBE_FLAG_MQ_CAPABLE)
			adapter->flags |= IXGBE_FLAG_RSS_CAPABLE;
#endif
		adapter->max_msix_q_vectors = IXGBE_MAX_MSIX_Q_VECTORS_82599;
		break;
	default:
		break;
	}

	/* Default DCB settings, if applicable */
	adapter->ring_feature[RING_F_DCB].indices = 8;
	if (adapter->flags & IXGBE_FLAG_DCB_CAPABLE) {
		int j;
		struct tc_configuration *tc;
		for (j = 0; j < MAX_TRAFFIC_CLASS; j++) {
			tc = &adapter->dcb_cfg.tc_config[j];
			tc->path[DCB_TX_CONFIG].bwg_id = 0;
			tc->path[DCB_TX_CONFIG].bwg_percent = 12 + (j & 1);
			tc->path[DCB_RX_CONFIG].bwg_id = 0;
			tc->path[DCB_RX_CONFIG].bwg_percent = 12 + (j & 1);
			tc->dcb_pfc = pfc_disabled;
		}
		adapter->dcb_cfg.bw_percentage[DCB_TX_CONFIG][0] = 100;
		adapter->dcb_cfg.bw_percentage[DCB_RX_CONFIG][0] = 100;
		adapter->dcb_cfg.rx_pba_cfg = pba_equal;
		adapter->dcb_cfg.pfc_mode_enable = false;
		adapter->dcb_cfg.round_robin_enable = false;
		adapter->dcb_set_bitmap = 0x00;
	}
#ifdef CONFIG_DCB
	ixgbe_copy_dcb_cfg(&adapter->dcb_cfg, &adapter->temp_dcb_cfg,
			   adapter->ring_feature[RING_F_DCB].indices);
#endif

 

	/* default flow control settings */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.current_mode = ixgbe_fc_full;	/* init for ethtool output */
	adapter->last_lfc_mode = hw->fc.current_mode;
	hw->fc.high_water = IXGBE_DEFAULT_FCRTH;
	hw->fc.low_water = IXGBE_DEFAULT_FCRTL;
	hw->fc.pause_time = IXGBE_DEFAULT_FCPAUSE;
	hw->fc.send_xon = true;
	hw->fc.disable_fc_autoneg = false;

	/* set defaults for eitr in MegaBytes */
	adapter->eitr_low = 10;
	adapter->eitr_high = 20;

	/* set default ring sizes */
	adapter->tx_ring_count = IXGBE_DEFAULT_TXD;
	adapter->rx_ring_count = IXGBE_DEFAULT_RXD;

	/* enable rx csum by default */
	adapter->flags |= IXGBE_FLAG_RX_CSUM_ENABLED;

	set_bit(__IXGBE_DOWN, &adapter->state);
out:
	return err;
}

/**
 * ixgbe_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 * @tx_ring:    tx descriptor ring (for a specific queue) to setup
 *
 * Return 0 on success, negative on failure
 **/
int ixgbe_setup_tx_resources(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *tx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int size;
	int i;

	size = sizeof(struct ixgbe_tx_buffer) * tx_ring->count;
	tx_ring->tx_buffer_info = vmalloc_node(size, adapter->numa_node);
	if (!tx_ring->tx_buffer_info) {
		DPRINTK(PROBE, ERR,
				"Unable to vmalloc buffer memory for "
				"the transmit descriptor ring\n");
		goto alloc_bi_failed;
	}
	memset(tx_ring->tx_buffer_info, 0, size);

	/* round up to nearest 4K */
	tx_ring->size = tx_ring->count * sizeof(union ixgbe_adv_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);

	tx_ring->desc = pci_alloc_consistent(pdev, tx_ring->size,
	                                     &tx_ring->dma);
	if (!tx_ring->desc) {
		DPRINTK(PROBE, ERR, "Unable to allocate memory for the "
				"transmit descriptor ring\n");
		goto alloc_desc_failed;
	}

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
#ifndef CONFIG_IXGBE_NAPI
	tx_ring->work_limit = tx_ring->count;
#endif

	size = ALIGN(IXGBE_SUBWINDOW_SIZE * MAX_PACKET_SIZE, 4096);
	tx_ring->window_size = size;

	for (i = 0; i <= (tx_ring->count - 1) / IXGBE_SUBWINDOW_SIZE; i++) {
		tx_ring->window[i] = pci_alloc_consistent(pdev, 
				size, 
				&tx_ring->dma_window[i]);
		
		/*
		printk("TX %s:%d %d = %lx\n", adapter->netdev->name, 
				tx_ring->queue_index, 
				i, 
				(uint64_t) tx_ring->dma_window[i]);
		*/

		if (!tx_ring->window[i]) {
			DPRINTK(PROBE, ERR,
				"Unable to allocate memory for "
				"the transmit buffer window\n");
			goto alloc_window_failed;
		}
	}

	spin_lock_init(&tx_ring->lock);

	return 0;

alloc_window_failed:
	for (i = 0; i < IXGBE_MAX_SUBWINDOWS; i++) {
		if (tx_ring->window[i]) {
			pci_free_consistent(pdev, 
					tx_ring->size, 
					tx_ring->window[i],
					tx_ring->dma_window[i]);
			tx_ring->window[i] = NULL;
			tx_ring->dma_window[i] = 0;
		}
	}

	pci_free_consistent(pdev, tx_ring->size, tx_ring->desc, tx_ring->dma);
	tx_ring->desc = NULL;

alloc_desc_failed:
	vfree(tx_ring->tx_buffer_info);
	tx_ring->tx_buffer_info = NULL;

alloc_bi_failed:
	return -ENOMEM;
}

/**
 * ixgbe_setup_all_tx_resources - allocate all queues Tx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int ixgbe_setup_all_tx_resources(struct ixgbe_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		err = ixgbe_setup_tx_resources(adapter, &adapter->tx_ring[i]);
		if (!err)
			continue;
		DPRINTK(PROBE, ERR, "Allocation for Tx Queue %u failed\n", i);
		break;
	}
	return err;
}

/**
 * ixgbe_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 * @rx_ring:    rx descriptor ring (for a specific queue) to setup
 *
 * Returns 0 on success, negative on failure
 **/
int ixgbe_setup_rx_resources(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *rx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int size;
	int i;

	size = sizeof(struct ixgbe_rx_buffer) * rx_ring->count;
	rx_ring->rx_buffer_info = vmalloc_node(size, adapter->numa_node);
	if (!rx_ring->rx_buffer_info) {
		DPRINTK(PROBE, ERR,
		        "Unable to vmalloc buffer memory for "
		        "the receive descriptor ring\n");
		goto alloc_bi_failed;
	}
	memset(rx_ring->rx_buffer_info, 0, size);

	/* Round up to nearest 4K */
	rx_ring->size = rx_ring->count * sizeof(union ixgbe_adv_rx_desc);
	rx_ring->size = ALIGN(rx_ring->size, 4096);

	rx_ring->desc = pci_alloc_consistent(pdev, rx_ring->size, &rx_ring->dma);

	if (!rx_ring->desc) {
		DPRINTK(PROBE, ERR,
		        "Unable to allocate memory for "
		        "the receive descriptor ring\n");
		goto alloc_desc_failed;
	}

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
#ifndef CONFIG_IXGBE_NAPI
	rx_ring->work_limit = rx_ring->count / 2;
#endif

	rx_ring->queued = 0;

	size = ALIGN(IXGBE_SUBWINDOW_SIZE * MAX_PACKET_SIZE, 4096);
	rx_ring->window_size = size;

	for (i = 0; i <= (rx_ring->count - 1) / IXGBE_SUBWINDOW_SIZE; i++) {
		rx_ring->window[i] = pci_alloc_consistent(pdev, 
				size, 
				&rx_ring->dma_window[i]);

		/*
		printk("RX %s:%d %d = %lx\n", adapter->netdev->name, 
				rx_ring->queue_index, 
				i, 
				(uint64_t) rx_ring->dma_window[i]);
		*/

		if (!rx_ring->window[i]) {
			DPRINTK(PROBE, ERR,
				"Unable to allocate memory for "
				"the receive buffer window\n");
			goto alloc_window_failed;
		}
	}

	spin_lock_init(&rx_ring->lock);

	return 0;

alloc_window_failed:
	for (i = 0; i < IXGBE_MAX_SUBWINDOWS; i++) {
		if (rx_ring->window[i]) {
			pci_free_consistent(pdev, 
					rx_ring->size, 
					rx_ring->window[i],
					rx_ring->dma_window[i]);
			rx_ring->window[i] = NULL;
			rx_ring->dma_window[i] = 0;
		}
	}

	pci_free_consistent(pdev, rx_ring->size, rx_ring->desc, rx_ring->dma);
	rx_ring->desc = NULL;

alloc_desc_failed:
	vfree(rx_ring->rx_buffer_info);
	rx_ring->rx_buffer_info = NULL;

alloc_bi_failed:
	return -ENOMEM;
}

/**
 * ixgbe_setup_all_rx_resources - allocate all queues Rx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int ixgbe_setup_all_rx_resources(struct ixgbe_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		err = ixgbe_setup_rx_resources(adapter, &adapter->rx_ring[i]);
		if (!err)
			continue;
		DPRINTK(PROBE, ERR, "Allocation for Rx Queue %u failed\n", i);
		break;
	}
	return err;
}

/**
 * ixgbe_free_tx_resources - Free Tx Resources per Queue
 * @adapter: board private structure
 * @tx_ring: Tx descriptor ring for a specific queue
 *
 * Free all transmit software resources
 **/
void ixgbe_free_tx_resources(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *tx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int i;

	ixgbe_clean_tx_ring(adapter, tx_ring);

	vfree(tx_ring->tx_buffer_info);
	tx_ring->tx_buffer_info = NULL;

	pci_free_consistent(pdev, tx_ring->size, tx_ring->desc, tx_ring->dma);
	tx_ring->desc = NULL;
	tx_ring->dma = 0;

	for (i = 0; i < IXGBE_MAX_SUBWINDOWS; i++) {
		if (tx_ring->window[i]) {
			pci_free_consistent(pdev, 
					tx_ring->window_size, 
					tx_ring->window[i], 
					tx_ring->dma_window[i]);
			tx_ring->window[i] = NULL;
			tx_ring->dma_window[i] = 0;
		}
	}
}

/**
 * ixgbe_free_all_tx_resources - Free Tx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
static void ixgbe_free_all_tx_resources(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		if (adapter->tx_ring[i].desc)
			ixgbe_free_tx_resources(adapter, &adapter->tx_ring[i]);
}

/**
 * ixgbe_free_rx_resources - Free Rx Resources
 * @adapter: board private structure
 * @rx_ring: ring to clean the resources from
 *
 * Free all receive software resources
 **/
void ixgbe_free_rx_resources(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *rx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	int i;

	ixgbe_clean_rx_ring(adapter, rx_ring);

	vfree(rx_ring->rx_buffer_info);
	rx_ring->rx_buffer_info = NULL;

	pci_free_consistent(pdev, rx_ring->size, rx_ring->desc, rx_ring->dma);
	rx_ring->desc = NULL;
	rx_ring->dma = 0;

	for (i = 0; i < IXGBE_MAX_SUBWINDOWS; i++) {
		if (rx_ring->window[i]) {
			pci_free_consistent(pdev, 
					rx_ring->window_size, 
					rx_ring->window[i], 
					rx_ring->dma_window[i]);
			rx_ring->window[i] = NULL;
			rx_ring->dma_window[i] = 0;
		}
	}
}

/**
 * ixgbe_free_all_rx_resources - Free Rx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/
static void ixgbe_free_all_rx_resources(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		if (adapter->rx_ring[i].desc)
			ixgbe_free_rx_resources(adapter, &adapter->rx_ring[i]);
}

/**
 * ixgbe_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 *
 * Returns 0 on success, negative on failure
 **/
static int ixgbe_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;

	/* MTU < 68 is an error and causes problems on some kernels */
	if ((new_mtu < 68) || (max_frame > IXGBE_MAX_JUMBO_FRAME_SIZE))
		return -EINVAL;

	/* sangjin: jumbo frame is not supported yet. */
	if (max_frame > ETH_DATA_LEN)
		return -EINVAL;

	DPRINTK(PROBE, INFO, "changing MTU from %d to %d\n",
	        netdev->mtu, new_mtu);
	/* must set new MTU before calling down or up */
	netdev->mtu = new_mtu;

	if (netif_running(netdev))
		ixgbe_reinit_locked(adapter);

	return 0;
}

/**
 * ixgbe_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
static int ixgbe_open(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int err;

	/* disallow open during test */
	if (test_bit(__IXGBE_TESTING, &adapter->state))
		return -EBUSY;

	netif_carrier_off(netdev);

	/* allocate transmit descriptors */
	err = ixgbe_setup_all_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = ixgbe_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	ixgbe_configure(adapter);

	/*
	 * Map the Tx/Rx rings to the vectors we were allotted.
	 * if request_irq will be called in this function map_rings
	 * must be called *before* up_complete
	 */
	ixgbe_map_rings_to_vectors(adapter);

	err = ixgbe_up_complete(adapter);
	if (err)
		goto err_setup_rx;

	/* clear any pending interrupts, may auto mask */
	IXGBE_READ_REG(hw, IXGBE_EICR);

	err = ixgbe_request_irq(adapter);
	if (err)
		goto err_req_irq;

	ixgbe_irq_enable(adapter, true, true);

	/*
	 * If this adapter has a fan, check to see if we had a failure
	 * before we enabled the interrupt.
	 */
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) {
		u32 esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);
		if (esdp & IXGBE_ESDP_SDP1)
			DPRINTK(DRV, CRIT,
				"Fan has stopped, replace the adapter\n");
	}
	
	dev_set_promiscuity(netdev, 1);

	return 0;

err_req_irq:
	ixgbe_down(adapter);
	ixgbe_release_hw_control(adapter);
	ixgbe_free_irq(adapter);
err_setup_rx:
	ixgbe_free_all_rx_resources(adapter);
err_setup_tx:
	ixgbe_free_all_tx_resources(adapter);
	ixgbe_reset(adapter);

	return err;
}

/**
 * ixgbe_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
static int ixgbe_close(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	ixgbe_down(adapter);
	ixgbe_free_irq(adapter);

	ixgbe_free_all_tx_resources(adapter);
	ixgbe_free_all_rx_resources(adapter);

	ixgbe_release_hw_control(adapter);

	return 0;
}

#ifdef CONFIG_PM
static int ixgbe_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u32 err;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	err = pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR "ixgbe: Cannot enable PCI device from "
		       "suspend\n");
		return err;
	}
	pci_set_master(pdev);

	pci_wake_from_d3(pdev, false);

	err = ixgbe_init_interrupt_scheme(adapter);
	if (err) {
		printk(KERN_ERR "ixgbe: Cannot initialize interrupts for "
		       "device\n");
		return err;
	}

	ixgbe_reset(adapter);

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_WUS, ~0);

	if (netif_running(netdev)) {
		err = ixgbe_open(adapter->netdev);
		if (err)
			return err;
	}

	netif_device_attach(netdev);

	return 0;
}
#endif /* CONFIG_PM */
static int __ixgbe_shutdown(struct pci_dev *pdev, bool *enable_wake)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ctrl, fctrl;
	u32 wufc = adapter->wol;
#ifdef CONFIG_PM
	int retval = 0;
#endif

	netif_device_detach(netdev);

	if (netif_running(netdev)) {
		ixgbe_down(adapter);
		ixgbe_free_irq(adapter);
		ixgbe_free_all_tx_resources(adapter);
		ixgbe_free_all_rx_resources(adapter);
	}

	ixgbe_clear_interrupt_scheme(adapter);

#ifdef CONFIG_PM
	retval = pci_save_state(pdev);
	if (retval)
		return retval;

#endif
	if (wufc) {
		ixgbe_set_rx_mode(netdev);

		/* turn on all-multi mode if wake on multicast is enabled */
		if (wufc & IXGBE_WUFC_MC) {
			fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
			fctrl |= IXGBE_FCTRL_MPE;
			IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
		}

		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		ctrl |= IXGBE_CTRL_GIO_DIS;
		IXGBE_WRITE_REG(hw, IXGBE_CTRL, ctrl);

		IXGBE_WRITE_REG(hw, IXGBE_WUFC, wufc);
	} else {
		IXGBE_WRITE_REG(hw, IXGBE_WUC, 0);
		IXGBE_WRITE_REG(hw, IXGBE_WUFC, 0);
	}

	if (wufc && hw->mac.type == ixgbe_mac_82599EB)
		pci_wake_from_d3(pdev, true);
	else
		pci_wake_from_d3(pdev, false);

	*enable_wake = !!wufc;

	ixgbe_release_hw_control(adapter);

	pci_disable_device(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int ixgbe_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int retval;
	bool wake;

	retval = __ixgbe_shutdown(pdev, &wake);
	if (retval)
		return retval;

	if (wake) {
		pci_prepare_to_sleep(pdev);
	} else {
		pci_wake_from_d3(pdev, false);
		pci_set_power_state(pdev, PCI_D3hot);
	}

	return 0;
}
#endif /* CONFIG_PM */

#ifndef USE_REBOOT_NOTIFIER
static void ixgbe_shutdown(struct pci_dev *pdev)
{
	bool wake;

	__ixgbe_shutdown(pdev, &wake);

	if (system_state == SYSTEM_POWER_OFF) {
		pci_wake_from_d3(pdev, wake);
		pci_set_power_state(pdev, PCI_D3hot);
	}
}

#endif

static void ixgbe_update_rxtx_stats(struct ixgbe_adapter *adapter)
{
	struct ixgbe_ring *ring;
	int i;

	adapter->net_stats.rx_packets = 0;
	adapter->net_stats.rx_bytes = 0;
	for (i = 0; i < adapter->num_rx_queues; i++) {
		ring = &adapter->rx_ring[i];

		adapter->net_stats.rx_packets += ring->stats.packets;
		adapter->net_stats.rx_bytes += ring->stats.bytes;
	}

	adapter->net_stats.tx_packets = 0;
	adapter->net_stats.tx_bytes = 0;
	for (i = 0; i < adapter->num_tx_queues; i++) {
		ring = &adapter->tx_ring[i];

		adapter->net_stats.tx_packets += ring->stats.packets;
		adapter->net_stats.tx_bytes += ring->stats.bytes;
	}
}

/**
 * ixgbe_update_stats - Update the board statistics counters.
 * @adapter: board private structure
 **/
void ixgbe_update_stats(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64 total_mpc = 0;
	u32 i, missed_rx = 0, mpc, bprc, lxon, lxoff, xon_off_tot;
#ifndef IXGBE_NO_LRO
	u32 flushed = 0, coal = 0, recycled = 0;
	int num_q_vectors = 1;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
		num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
#endif

	if (hw->mac.type == ixgbe_mac_82599EB) {
		u64 rsc_count = 0;
		for (i = 0; i < 16; i++)
			adapter->hw_rx_no_dma_resources += IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
		for (i = 0; i < adapter->num_rx_queues; i++)
			rsc_count += adapter->rx_ring[i].rsc_count;
		adapter->rsc_count = rsc_count;
	}

#ifndef IXGBE_NO_LRO
	for (i = 0; i < num_q_vectors; i++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[i];
		if (!q_vector || !q_vector->lrolist)
			continue;
		flushed += q_vector->lrolist->stats.flushed;
		coal += q_vector->lrolist->stats.coal;
		recycled += q_vector->lrolist->stats.recycled;
	}
	adapter->lro_stats.flushed = flushed;
	adapter->lro_stats.coal = coal;
	adapter->lro_stats.recycled = recycled;

#endif
	adapter->stats.crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	for (i = 0; i < 8; i++) {
		/* for packet buffers not used, the register should read 0 */
		mpc = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		missed_rx += mpc;
		adapter->stats.mpc[i] += mpc;
		total_mpc += adapter->stats.mpc[i];
		if (hw->mac.type == ixgbe_mac_82598EB)
			adapter->stats.rnbc[i] += IXGBE_READ_REG(hw, IXGBE_RNBC(i));
		adapter->stats.qptc[i] += IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		adapter->stats.qbtc[i] += IXGBE_READ_REG(hw, IXGBE_QBTC(i));
		adapter->stats.qprc[i] += IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		adapter->stats.qbrc[i] += IXGBE_READ_REG(hw, IXGBE_QBRC(i));
		if (hw->mac.type == ixgbe_mac_82599EB) {
			adapter->stats.pxonrxc[i] += IXGBE_READ_REG(hw,
			                                    IXGBE_PXONRXCNT(i));
			adapter->stats.pxoffrxc[i] += IXGBE_READ_REG(hw,
			                                   IXGBE_PXOFFRXCNT(i));
		} else {
			adapter->stats.pxonrxc[i] += IXGBE_READ_REG(hw,
			                                      IXGBE_PXONRXC(i));
			adapter->stats.pxoffrxc[i] += IXGBE_READ_REG(hw,
			                                     IXGBE_PXOFFRXC(i));
		}
	}
	adapter->stats.gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	/* work around hardware counting issue */
	adapter->stats.gprc -= missed_rx;

	/* 82598 hardware only has a 32 bit counter in the high register */
	if (hw->mac.type == ixgbe_mac_82599EB) {
		adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCL);
		IXGBE_READ_REG(hw, IXGBE_GORCH); /* to clear */
		adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCL);
		IXGBE_READ_REG(hw, IXGBE_GOTCH); /* to clear */
		adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORL);
		IXGBE_READ_REG(hw, IXGBE_TORH); /* to clear */
		adapter->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		adapter->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
		adapter->stats.fdirmatch += IXGBE_READ_REG(hw, IXGBE_FDIRMATCH);
		adapter->stats.fdirmiss += IXGBE_READ_REG(hw, IXGBE_FDIRMISS);
	} else {
		adapter->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		adapter->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
		adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
		adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORH);
	}
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	adapter->stats.bprc += bprc;
	adapter->stats.mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	if (hw->mac.type == ixgbe_mac_82598EB)
		adapter->stats.mprc -= bprc;
	adapter->stats.roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	adapter->stats.prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	adapter->stats.prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	adapter->stats.prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	adapter->stats.prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	adapter->stats.prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	adapter->stats.prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);
	adapter->stats.rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);
	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	adapter->stats.lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	adapter->stats.lxofftxc += lxoff;
	adapter->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	adapter->stats.mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	/*
	 * 82598 errata - tx of flow control packets is included in tx counters
	 */
	xon_off_tot = lxon + lxoff;
	adapter->stats.gptc -= xon_off_tot;
	adapter->stats.mptc -= xon_off_tot;
	adapter->stats.gotc -= (xon_off_tot * (ETH_ZLEN + ETH_FCS_LEN));
	adapter->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	adapter->stats.rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	adapter->stats.tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	adapter->stats.tpt += IXGBE_READ_REG(hw, IXGBE_TPT);
	adapter->stats.ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	adapter->stats.ptc64 -= xon_off_tot;
	adapter->stats.ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	adapter->stats.ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	adapter->stats.ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	adapter->stats.ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	adapter->stats.ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	adapter->stats.bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);

	/* Fill out the OS statistics structure */
	adapter->net_stats.multicast = adapter->stats.mprc;

	/* Rx Errors */
	adapter->net_stats.rx_errors = adapter->stats.crcerrs +
	                               adapter->stats.rlec;
	adapter->net_stats.rx_dropped = 0;
	adapter->net_stats.rx_length_errors = adapter->stats.rlec;
	adapter->net_stats.rx_crc_errors = adapter->stats.crcerrs;
	adapter->net_stats.rx_missed_errors = total_mpc;

	ixgbe_update_rxtx_stats(adapter);
}

/**
 * ixgbe_watchdog - Timer Call-back
 * @data: pointer to adapter cast into an unsigned long
 **/
static void ixgbe_watchdog(unsigned long data)
{
	struct ixgbe_adapter *adapter = (struct ixgbe_adapter *)data;
	struct ixgbe_hw *hw = &adapter->hw;
	u64 eics = 0;
	int i;

	/*
	 * Do the watchdog outside of interrupt context due to the lovely
	 * delays that some of the newer hardware requires
	 */

	if (test_bit(__IXGBE_DOWN, &adapter->state))
		goto watchdog_short_circuit;


	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED)) {
		/*
		 * for legacy and MSI interrupts don't set any bits
		 * that are enabled for EIAM, because this operation
		 * would set *both* EIMS and EICS for any bit in EIAM
		 */
		IXGBE_WRITE_REG(hw, IXGBE_EICS,
			(IXGBE_EICS_TCP_TIMER | IXGBE_EICS_OTHER));
		goto watchdog_reschedule;
	}

	/* get one bit for every active tx/rx interrupt vector */
	for (i = 0; i < adapter->num_msix_vectors - NON_Q_VECTORS; i++) {
		struct ixgbe_q_vector *qv = adapter->q_vector[i];
		if (qv->rxr_count || qv->txr_count)
			eics |= ((u64)1 << i);
	}

	/* Cause software interrupt to ensure rings are cleaned */
	ixgbe_irq_rearm_queues(adapter, eics);
	
watchdog_reschedule:
	/* Reset the timer */
	mod_timer(&adapter->watchdog_timer, round_jiffies(jiffies + 2 * HZ));

watchdog_short_circuit:
	schedule_work(&adapter->watchdog_task);
}

/**
 * ixgbe_multispeed_fiber_task - worker thread to configure multispeed fiber
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_multispeed_fiber_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter = container_of(work,
	                                             struct ixgbe_adapter,
	                                             multispeed_fiber_task);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 autoneg;

	adapter->flags |= IXGBE_FLAG_IN_SFP_LINK_TASK;
	autoneg = hw->phy.autoneg_advertised;
	if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
		hw->mac.ops.get_link_capabilities(hw, &autoneg,
		                                  &hw->mac.autoneg);
	if (hw->mac.ops.setup_link_speed)
		hw->mac.ops.setup_link_speed(hw, autoneg, true, true);
	adapter->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	adapter->flags &= ~IXGBE_FLAG_IN_SFP_LINK_TASK;
}

/**
 * ixgbe_sfp_config_module_task - worker thread to configure a new SFP+ module
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_sfp_config_module_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter = container_of(work,
	                                             struct ixgbe_adapter,
	                                             sfp_config_module_task);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 err;

	adapter->flags |= IXGBE_FLAG_IN_SFP_MOD_TASK;
	err = hw->phy.ops.identify_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		DPRINTK(PROBE, ERR, "failed to load because an "
		        "unsupported SFP+ module type was detected.\n");
		unregister_netdev(adapter->netdev);
		adapter->netdev_registered = false;
		return;
	}
	/*
	 * A module may be identified correctly, but the EEPROM may not have
	 * support for that module.  setup_sfp() will fail in that case, so
	 * we should not allow that module to load.
	 */
	err = hw->mac.ops.setup_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		DPRINTK(PROBE, ERR, "failed to load because an "
		        "unsupported SFP+ module type was detected.\n");
		unregister_netdev(adapter->netdev);
		adapter->netdev_registered = false;
		return;
	}

	if (!(adapter->flags & IXGBE_FLAG_IN_SFP_LINK_TASK))
		/* This will also work for DA Twinax connections */
		schedule_work(&adapter->multispeed_fiber_task);
	adapter->flags &= ~IXGBE_FLAG_IN_SFP_MOD_TASK;
}

/**
 * ixgbe_fdir_reinit_task - worker thread to reinit FDIR filter table
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_fdir_reinit_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter = container_of(work,
	                                             struct ixgbe_adapter,
	                                             fdir_reinit_task);
	struct ixgbe_hw *hw = &adapter->hw;
	int i;

	if (ixgbe_reinit_fdir_tables_82599(hw) == 0) {
		for (i = 0; i < adapter->num_tx_queues; i++)
			set_bit(__IXGBE_FDIR_INIT_DONE,
			        &(adapter->tx_ring[i].reinit_state));
	} else {
		DPRINTK(PROBE, ERR, "failed to finish FDIR re-initialization, "
		        "ignored adding FDIR ATR filters \n");
	}
	/* Done FDIR Re-initialization, enable transmits */
	netif_tx_start_all_queues(adapter->netdev);
}

/**
 * ixgbe_watchdog_task - worker thread to bring link up
 * @work: pointer to work_struct containing our data
 **/
static void ixgbe_watchdog_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter = container_of(work,
	                                             struct ixgbe_adapter,
	                                             watchdog_task);
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 link_speed = adapter->link_speed;
	bool link_up = adapter->link_up;
	int i;
	struct ixgbe_ring *tx_ring;
	int some_tx_pending = 0;

	adapter->flags |= IXGBE_FLAG_IN_WATCHDOG_TASK;

	if (adapter->flags & IXGBE_FLAG_NEED_LINK_UPDATE) {
		if (hw->mac.ops.check_link) {
			hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
		} else {
			/* always assume link is up, if no check link function */
			link_speed = IXGBE_LINK_SPEED_10GB_FULL;
			link_up = true;
		}
		if (link_up) {
			if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
				for (i = 0; i < MAX_TRAFFIC_CLASS; i++)
					hw->mac.ops.fc_enable(hw, i);
			} else {
				hw->mac.ops.fc_enable(hw, 0);
			}
		}

		if (link_up ||
		    time_after(jiffies, (adapter->link_check_timeout +
		                         IXGBE_TRY_LINK_TIMEOUT))) {
			adapter->flags &= ~IXGBE_FLAG_NEED_LINK_UPDATE;
			IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EIMC_LSC);
		}
		adapter->link_up = link_up;
		adapter->link_speed = link_speed;
	}

	if (link_up) {
		if (!netif_carrier_ok(netdev)) {
			bool flow_rx, flow_tx;

			if (hw->mac.type == ixgbe_mac_82599EB) {
				u32 mflcn = IXGBE_READ_REG(hw, IXGBE_MFLCN);
				u32 fccfg = IXGBE_READ_REG(hw, IXGBE_FCCFG);
				flow_rx = !!(mflcn & IXGBE_MFLCN_RFCE);
				flow_tx = !!(fccfg & IXGBE_FCCFG_TFCE_802_3X);
			} else {
				u32 frctl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
				u32 rmcs = IXGBE_READ_REG(hw, IXGBE_RMCS);
				flow_rx = !!(frctl & IXGBE_FCTRL_RFCE);
				flow_tx = !!(rmcs & IXGBE_RMCS_TFCE_802_3X);
			}
			DPRINTK(LINK, INFO, "NIC Link is Up %s, "
			        "Flow Control: %s\n",
			        (link_speed == IXGBE_LINK_SPEED_10GB_FULL ?
			         "10 Gbps" :
			         (link_speed == IXGBE_LINK_SPEED_1GB_FULL ?
			          "1 Gbps" : "unknown speed")),
			        ((flow_rx && flow_tx) ? "RX/TX" :
			         (flow_rx ? "RX" :
			         (flow_tx ? "TX" : "None"))));

			netif_carrier_on(netdev);
			netif_tx_wake_all_queues(netdev);
		} else {
			/* Force detection of hung controller */
			adapter->detect_tx_hung = true;
		}
	} else {
		adapter->link_up = false;
		adapter->link_speed = 0;
		if (netif_carrier_ok(netdev)) {
			DPRINTK(LINK, INFO, "NIC Link is Down\n");
			netif_carrier_off(netdev);
			netif_tx_stop_all_queues(netdev);
		}
	}

	if (!netif_carrier_ok(netdev)) {
		for (i = 0; i < adapter->num_tx_queues; i++) {
			tx_ring = &adapter->tx_ring[i];
			if (tx_ring->next_to_use != tx_ring->next_to_clean) {
				some_tx_pending = 1;
				break;
			}
		}

		if (some_tx_pending) {
			/* We've lost link, so the controller stops DMA,
			 * but we've got queued Tx work that's never going
			 * to get done, so reset controller to flush Tx.
			 * (Do the reset outside of interrupt context).
			 */
			 schedule_work(&adapter->reset_task);
		}
	}

	ixgbe_update_stats(adapter);
	adapter->flags &= ~IXGBE_FLAG_IN_WATCHDOG_TASK;

	if (adapter->flags & IXGBE_FLAG_NEED_LINK_UPDATE) {
		/* poll faster when waiting for link */
		mod_timer(&adapter->watchdog_timer, jiffies + (HZ/10));
	}

}

static int ixgbe_xmit_frame_ps(struct sk_buff *skb, struct net_device *netdev)
{
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *tx_ring;
	int r_idx = 0;

	struct ps_pkt_info info;

#ifdef HAVE_TX_MQ
	r_idx = skb->queue_mapping;
#endif
	adapter = netdev_priv(netdev);
	tx_ring = &adapter->tx_ring[r_idx];

	info.offset = 0;
	info.len = skb->len;

	spin_lock(&tx_ring->lock);
	ixgbe_xmit_batch(tx_ring, 1, &info, skb->data);
	spin_unlock(&tx_ring->lock);

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

#if 0
static int ixgbe_tso(struct ixgbe_adapter *adapter, struct ixgbe_ring *tx_ring,
                     struct sk_buff *skb, u32 tx_flags, u8 *hdr_len)
{
#ifdef NETIF_F_TSO
	struct ixgbe_adv_tx_context_desc *context_desc;
	unsigned int i;
	int err;
	struct ixgbe_tx_buffer *tx_buffer_info;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl;
	u32 mss_l4len_idx, l4len;

	if (skb_is_gso(skb)) {
		if (skb_header_cloned(skb)) {
			err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (err)
				return err;
		}
		l4len = tcp_hdrlen(skb);
		*hdr_len += l4len;

		if (skb->protocol == htons(ETH_P_IP)) {
			struct iphdr *iph = ip_hdr(skb);
			iph->tot_len = 0;
			iph->check = 0;
			tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
			                                         iph->daddr, 0,
			                                         IPPROTO_TCP,
			                                         0);
			adapter->hw_tso_ctxt++;
#ifdef NETIF_F_TSO6
		} else if (skb_shinfo(skb)->gso_type == SKB_GSO_TCPV6) {
			ipv6_hdr(skb)->payload_len = 0;
			tcp_hdr(skb)->check =
			    ~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
			                     &ipv6_hdr(skb)->daddr,
			                     0, IPPROTO_TCP, 0);
			adapter->hw_tso6_ctxt++;
#endif
		}

		i = tx_ring->next_to_use;

		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		context_desc = IXGBE_TX_CTXTDESC_ADV(*tx_ring, i);

		/* VLAN MACLEN IPLEN */
		if (tx_flags & IXGBE_TX_FLAGS_VLAN)
			vlan_macip_lens |=
			                  (tx_flags & IXGBE_TX_FLAGS_VLAN_MASK);
		vlan_macip_lens |= ((skb_network_offset(skb)) <<
		                    IXGBE_ADVTXD_MACLEN_SHIFT);
		*hdr_len += skb_network_offset(skb);
		vlan_macip_lens |=
		          (skb_transport_header(skb) - skb_network_header(skb));
		*hdr_len +=
		          (skb_transport_header(skb) - skb_network_header(skb));
		context_desc->vlan_macip_lens = cpu_to_le32(vlan_macip_lens);
		context_desc->seqnum_seed = 0;

		/* ADV DTYP TUCMD MKRLOC/ISCSIHEDLEN */
		type_tucmd_mlhl = (IXGBE_TXD_CMD_DEXT |
				    IXGBE_ADVTXD_DTYP_CTXT);

		if (skb->protocol == htons(ETH_P_IP))
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
		context_desc->type_tucmd_mlhl = cpu_to_le32(type_tucmd_mlhl);

		/* MSS L4LEN IDX */
		mss_l4len_idx =
		          (skb_shinfo(skb)->gso_size << IXGBE_ADVTXD_MSS_SHIFT);
		mss_l4len_idx |= (l4len << IXGBE_ADVTXD_L4LEN_SHIFT);
		/* use index 1 for TSO */
		mss_l4len_idx |= (1 << IXGBE_ADVTXD_IDX_SHIFT);
		context_desc->mss_l4len_idx = cpu_to_le32(mss_l4len_idx);

		tx_buffer_info->time_stamp = jiffies;
		tx_buffer_info->next_to_watch = i;

		i++;
		if (i == tx_ring->count)
			i = 0;
		tx_ring->next_to_use = i;

		return true;
	}

#endif
	return false;
}

static bool ixgbe_tx_csum(struct ixgbe_adapter *adapter,
                          struct ixgbe_ring *tx_ring,
                          struct sk_buff *skb, u32 tx_flags)
{
	struct ixgbe_adv_tx_context_desc *context_desc;
	unsigned int i;
	struct ixgbe_tx_buffer *tx_buffer_info;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;

	if (skb->ip_summed == CHECKSUM_PARTIAL ||
	    (tx_flags & IXGBE_TX_FLAGS_VLAN)) {
		i = tx_ring->next_to_use;
		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		context_desc = IXGBE_TX_CTXTDESC_ADV(*tx_ring, i);

		if (tx_flags & IXGBE_TX_FLAGS_VLAN)
			vlan_macip_lens |= (tx_flags &
			                    IXGBE_TX_FLAGS_VLAN_MASK);
		vlan_macip_lens |= (skb_network_offset(skb) <<
		                    IXGBE_ADVTXD_MACLEN_SHIFT);
		if (skb->ip_summed == CHECKSUM_PARTIAL)
			vlan_macip_lens |= (skb_transport_header(skb) -
			                    skb_network_header(skb));

		context_desc->vlan_macip_lens = cpu_to_le32(vlan_macip_lens);
		context_desc->seqnum_seed = 0;

		type_tucmd_mlhl |= (IXGBE_TXD_CMD_DEXT |
		                    IXGBE_ADVTXD_DTYP_CTXT);

		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			switch (skb->protocol) {
			case __constant_htons(ETH_P_IP):
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
				if (ip_hdr(skb)->protocol == IPPROTO_TCP)
					type_tucmd_mlhl |=
					    IXGBE_ADVTXD_TUCMD_L4T_TCP;
				break;
#ifdef NETIF_F_IPV6_CSUM
			case __constant_htons(ETH_P_IPV6):
				/* XXX what about other V6 headers?? */
				if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
					type_tucmd_mlhl |=
					         IXGBE_ADVTXD_TUCMD_L4T_TCP;
				break;
#endif
			default:
				if (unlikely(net_ratelimit())) {
					DPRINTK(PROBE, WARNING,
					 "partial checksum but proto=%x!\n",
					 skb->protocol);
				}
				break;
			}
		}

		context_desc->type_tucmd_mlhl = cpu_to_le32(type_tucmd_mlhl);
		/* use index zero for tx checksum offload */
		context_desc->mss_l4len_idx = 0;

		tx_buffer_info->time_stamp = jiffies;
		tx_buffer_info->next_to_watch = i;

		adapter->hw_csum_tx_good++;
		i++;
		if (i == tx_ring->count)
			i = 0;
		tx_ring->next_to_use = i;

		return true;
	}

	return false;
}

static int ixgbe_tx_map(struct ixgbe_adapter *adapter,
                        struct ixgbe_ring *tx_ring,
                        struct sk_buff *skb, u32 tx_flags,
                        unsigned int first)
{
	struct ixgbe_tx_buffer *tx_buffer_info;
	unsigned int len;
	unsigned int total = skb->len;
	unsigned int offset = 0, size, count = 0, i;
#ifdef MAX_SKB_FRAGS
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	unsigned int f;
#endif

	i = tx_ring->next_to_use;

	len = min(skb_headlen(skb), total);
	while (len) {
		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		size = min(len, (unsigned int)IXGBE_MAX_DATA_PER_TXD);

		tx_buffer_info->length = size;
		/*
		tx_buffer_info->dma = pci_map_single(adapter->pdev,
		                                     skb->data + offset,
		                                     size, PCI_DMA_TODEVICE);
		*/
		tx_buffer_info->time_stamp = jiffies;
		tx_buffer_info->next_to_watch = i;

		len -= size;
		total -= size;
		offset += size;
		count++;
		i++;
		if (i == tx_ring->count)
			i = 0;
	}

#ifdef MAX_SKB_FRAGS
	for (f = 0; f < nr_frags; f++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[f];
		len = min( (unsigned int)frag->size, total);
		offset = frag->page_offset;

		while (len) {
			tx_buffer_info = &tx_ring->tx_buffer_info[i];
			size = min(len, (unsigned int)IXGBE_MAX_DATA_PER_TXD);

			tx_buffer_info->length = size;
			/*
			tx_buffer_info->dma = pci_map_page(adapter->pdev,
			                                   frag->page,
			                                   offset,
			                                   size,
			                                   PCI_DMA_TODEVICE);
			*/
			tx_buffer_info->time_stamp = jiffies;
			tx_buffer_info->next_to_watch = i;

			len -= size;
			total -= size;
			offset += size;
			count++;
			i++;
			if (i == tx_ring->count)
				i = 0;
		}
		if (total == 0)
			break;
	}
#endif
	if (i == 0)
		i = tx_ring->count - 1;
	else
		i = i - 1;
	/* tx_ring->tx_buffer_info[i].skb = skb; */
	tx_ring->tx_buffer_info[first].next_to_watch = i;

	return count;
}

static void ixgbe_tx_queue(struct ixgbe_adapter *adapter,
                           struct ixgbe_ring *tx_ring, int tx_flags,
                           int count, u32 paylen, u8 hdr_len)
{
	union ixgbe_adv_tx_desc *tx_desc = NULL;
	struct ixgbe_tx_buffer *tx_buffer_info;
	u32 olinfo_status = 0, cmd_type_len = 0;
	unsigned int i;

	u32 txd_cmd = IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS | IXGBE_TXD_CMD_IFCS;

	cmd_type_len |= IXGBE_ADVTXD_DTYP_DATA;

	cmd_type_len |= IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT;

	if (tx_flags & IXGBE_TX_FLAGS_VLAN)
		cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;

	if (tx_flags & IXGBE_TX_FLAGS_TSO) {
		cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;

		olinfo_status |= IXGBE_TXD_POPTS_TXSM <<
		                 IXGBE_ADVTXD_POPTS_SHIFT;

		/* use index 1 context for tso */
		olinfo_status |= (1 << IXGBE_ADVTXD_IDX_SHIFT);
		if (tx_flags & IXGBE_TX_FLAGS_IPV4)
			olinfo_status |= IXGBE_TXD_POPTS_IXSM <<
			                 IXGBE_ADVTXD_POPTS_SHIFT;

	} else if (tx_flags & IXGBE_TX_FLAGS_CSUM)
		olinfo_status |= IXGBE_TXD_POPTS_TXSM <<
		                 IXGBE_ADVTXD_POPTS_SHIFT;
	olinfo_status |= ((paylen - hdr_len) << IXGBE_ADVTXD_PAYLEN_SHIFT);

	i = tx_ring->next_to_use;
	while (count--) {
		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		tx_desc = IXGBE_TX_DESC_ADV(*tx_ring, i);
		/* tx_desc->read.buffer_addr = cpu_to_le64(tx_buffer_info->dma); */
		tx_desc->read.cmd_type_len =
			cpu_to_le32(cmd_type_len | tx_buffer_info->length);
		tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);
		i++;
		if (i == tx_ring->count)
			i = 0;
	}

	tx_desc->read.cmd_type_len |= cpu_to_le32(txd_cmd);

	/*
	 * Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64).
	 */
	wmb();

	tx_ring->next_to_use = i;
	writel(i, adapter->hw.hw_addr + tx_ring->tail);
}

static void ixgbe_atr(struct ixgbe_adapter *adapter, struct sk_buff *skb,
	              int queue, u32 tx_flags)
{
	/* Right now, we support IPv4 only */
	struct ixgbe_atr_input atr_input;
	struct tcphdr *th;
	struct udphdr *uh;
	struct iphdr *iph = ip_hdr(skb);
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	u16 vlan_id, src_port, dst_port, flex_bytes;
	u32 src_ipv4_addr, dst_ipv4_addr;
	u8 l4type = 0;

	/* check if we're UDP or TCP */
	if (iph->protocol == IPPROTO_TCP) {
		th = tcp_hdr(skb);
		src_port = th->source;
		dst_port = th->dest;
		l4type |= IXGBE_ATR_L4TYPE_TCP;
		/* l4type IPv4 type is 0, no need to assign */
	} else if(iph->protocol == IPPROTO_UDP) {
		uh = udp_hdr(skb);
		src_port = uh->source;
		dst_port = uh->dest;
		l4type |= IXGBE_ATR_L4TYPE_UDP;
		/* l4type IPv4 type is 0, no need to assign */
	} else {
		/* Unsupported L4 header, just bail here */
		return;
	}

	memset(&atr_input, 0, sizeof(struct ixgbe_atr_input));

	vlan_id = (tx_flags & IXGBE_TX_FLAGS_VLAN_MASK) >>
	           IXGBE_TX_FLAGS_VLAN_SHIFT;
	src_ipv4_addr = iph->saddr;
	dst_ipv4_addr = iph->daddr;
	flex_bytes = eth->h_proto;

	ixgbe_atr_set_vlan_id_82599(&atr_input, vlan_id);
	ixgbe_atr_set_src_port_82599(&atr_input, dst_port);
	ixgbe_atr_set_dst_port_82599(&atr_input, src_port);
	ixgbe_atr_set_flex_byte_82599(&atr_input, flex_bytes);
	ixgbe_atr_set_l4type_82599(&atr_input, l4type);
	/* src and dst are inverted, think how the receiver sees them */
	ixgbe_atr_set_src_ipv4_82599(&atr_input, dst_ipv4_addr);
	ixgbe_atr_set_dst_ipv4_82599(&atr_input, src_ipv4_addr);

	/* This assumes the Rx queue and Tx queue are bound to the same CPU */
	ixgbe_fdir_add_signature_filter_82599(&adapter->hw, &atr_input, queue);
}

static int __ixgbe_maybe_stop_tx(struct net_device *netdev,
                                 struct ixgbe_ring *tx_ring, int size)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	netif_stop_subqueue(netdev, tx_ring->queue_index);
	/* Herbert's original patch had:
	 *  smp_mb__after_netif_stop_queue();
	 * but since that doesn't exist yet, just open code it. */
	smp_mb();

	/* We need to check again in a case another CPU has just
	 * made room available. */
	if (likely(IXGBE_DESC_UNUSED(tx_ring) < size))
		return -EBUSY;

	/* A reprieve! - use start_queue because it doesn't call schedule */
	netif_start_subqueue(netdev, tx_ring->queue_index);
	++adapter->restart_queue;
	return 0;
}

static int ixgbe_maybe_stop_tx(struct net_device *netdev,
                               struct ixgbe_ring *tx_ring, int size)
{
	if (likely(IXGBE_DESC_UNUSED(tx_ring) >= size))
		return 0;
	return __ixgbe_maybe_stop_tx(netdev, tx_ring, size);
}

static int ixgbe_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_ring *tx_ring;
	unsigned int first;
	unsigned int tx_flags = 0;
	u8 hdr_len = 0;
	int r_idx = 0, tso;
	int count = 0;

#ifdef MAX_SKB_FRAGS
	unsigned int f;
#endif

#ifdef NETIF_F_HW_VLAN_TX
	if (adapter->vlgrp && vlan_tx_tag_present(skb)) {
		tx_flags |= vlan_tx_tag_get(skb);
#ifdef HAVE_TX_MQ
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
			if (skb->queue_mapping) {
				tx_flags &= ~IXGBE_TX_FLAGS_VLAN_PRIO_MASK;
				tx_flags |= (skb->queue_mapping << 13);
			} else {
				skb->queue_mapping = (tx_flags &
					IXGBE_TX_FLAGS_VLAN_PRIO_MASK) >> 13;
			}
		}
#endif
		tx_flags <<= IXGBE_TX_FLAGS_VLAN_SHIFT;
		tx_flags |= IXGBE_TX_FLAGS_VLAN;
#ifdef HAVE_TX_MQ
	} else if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		if (skb->priority != TC_PRIO_CONTROL) {
			tx_flags |= (skb->queue_mapping << 13);
			tx_flags <<= IXGBE_TX_FLAGS_VLAN_SHIFT;
			tx_flags |= IXGBE_TX_FLAGS_VLAN;
		} else {
			skb->queue_mapping =
				adapter->ring_feature[RING_F_DCB].indices-1;
		}
#endif
	}
#endif

#ifdef HAVE_TX_MQ
	r_idx = skb->queue_mapping;
#endif
	tx_ring = &adapter->tx_ring[r_idx];

	/* four things can cause us to need a context descriptor */
	if (skb_is_gso(skb) ||
	    (skb->ip_summed == CHECKSUM_PARTIAL) ||
	    (tx_flags & IXGBE_TX_FLAGS_VLAN))
		count++;
	count += TXD_USE_COUNT(skb_headlen(skb));
#ifdef MAX_SKB_FRAGS
	for (f = 0; f < skb_shinfo(skb)->nr_frags; f++)
		count += TXD_USE_COUNT(skb_shinfo(skb)->frags[f].size);

#endif
	if (ixgbe_maybe_stop_tx(netdev, tx_ring, count)) {
		adapter->tx_busy++;
		return NETDEV_TX_BUSY;
	}

	first = tx_ring->next_to_use;
	if (skb->protocol == htons(ETH_P_IP))
		tx_flags |= IXGBE_TX_FLAGS_IPV4;
	tso = ixgbe_tso(adapter, tx_ring, skb, tx_flags, &hdr_len);
	if (tso < 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (tso)
		tx_flags |= IXGBE_TX_FLAGS_TSO;
	else if (ixgbe_tx_csum(adapter, tx_ring, skb, tx_flags) &&
		 (skb->ip_summed == CHECKSUM_PARTIAL))
		tx_flags |= IXGBE_TX_FLAGS_CSUM;

	/* add the ATR filter if ATR is on */
	if (tx_ring->atr_sample_rate) {
		++tx_ring->atr_count;
		if ((tx_ring->atr_count >= tx_ring->atr_sample_rate) &&
		    test_bit(__IXGBE_FDIR_INIT_DONE, &tx_ring->reinit_state)) {
			ixgbe_atr(adapter, skb, tx_ring->queue_index, tx_flags);
			tx_ring->atr_count = 0;
		}
	}
	ixgbe_tx_queue(adapter, tx_ring, tx_flags,
		       ixgbe_tx_map(adapter, tx_ring, skb, tx_flags, first),
		       skb->len, hdr_len);

	netdev->trans_start = jiffies;

	ixgbe_maybe_stop_tx(netdev, tx_ring, DESC_NEEDED);

	return NETDEV_TX_OK;
}
#endif

/**
 * ixgbe_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/
static struct net_device_stats *ixgbe_get_stats(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	ixgbe_update_rxtx_stats(adapter);

	/* only return the current stats */
	return &adapter->net_stats;
}

/**
 * ixgbe_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int ixgbe_set_mac(struct net_device *netdev, void *p)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac.addr, addr->sa_data, netdev->addr_len);

	if (hw->mac.ops.set_rar)
		hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	return 0;
}

#if defined(HAVE_NETDEV_STORAGE_ADDRESS) && defined(NETDEV_HW_ADDR_T_SAN)
/**
 * ixgbe_add_sanmac_netdev - Add the SAN MAC address to the corresponding
 * netdev->dev_addr_list
 * @netdev: network interface device structure
 *
 * Returns non-zero on failure
 **/
static int ixgbe_add_sanmac_netdev(struct net_device *dev)
{
	int err = 0;
	struct ixgbe_adapter *adapter = netdev_priv(dev);
	struct ixgbe_mac_info *mac = &adapter->hw.mac;

	if (is_valid_ether_addr(mac->san_addr)) {
		rtnl_lock();
		err = dev_addr_add(dev, mac->san_addr, NETDEV_HW_ADDR_T_SAN);
		rtnl_unlock();
	}
	return err;
}

/**
 * ixgbe_del_sanmac_netdev - Removes the SAN MAC address to the corresponding
 * netdev->dev_addr_list
 * @netdev: network interface device structure
 *
 * Returns non-zero on failure
 **/
static int ixgbe_del_sanmac_netdev(struct net_device *dev)
{
	int err = 0;
	struct ixgbe_adapter *adapter = netdev_priv(dev);
	struct ixgbe_mac_info *mac = &adapter->hw.mac;

	if (is_valid_ether_addr(mac->san_addr)) {
		rtnl_lock();
		err = dev_addr_del(dev, mac->san_addr, NETDEV_HW_ADDR_T_SAN);
		rtnl_unlock();
	}
	return err;
}

#endif /* (HAVE_NETDEV_STORAGE_ADDRESS) && defined(NETDEV_HW_ADDR_T_SAN) */
#ifdef ETHTOOL_OPS_COMPAT
/**
 * ixgbe_ioctl -
 * @netdev:
 * @ifreq:
 * @cmd:
 **/
static int ixgbe_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCETHTOOL:
		return ethtool_ioctl(ifr);
	default:
		return -EOPNOTSUPP;
	}
}

#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void ixgbe_netpoll(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i;

#ifndef CONFIG_IXGBE_NAPI
	ixgbe_irq_disable(adapter);
#endif
	adapter->flags |= IXGBE_FLAG_IN_NETPOLL;
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		int num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
		for (i = 0; i < num_q_vectors; i++) {
			struct ixgbe_q_vector *q_vector = adapter->q_vector[i];
			ixgbe_msix_clean_many(0, q_vector);
		}
	} else {
		ixgbe_intr(adapter->pdev->irq, netdev);
	}
	adapter->flags &= ~IXGBE_FLAG_IN_NETPOLL;
#ifndef CONFIG_IXGBE_NAPI
	ixgbe_irq_enable(adapter, true, true);
#endif
}

#endif
#ifdef HAVE_NETDEV_SELECT_QUEUE
static u16 ixgbe_select_queue(struct net_device *dev, struct sk_buff *skb)
{
	struct ixgbe_adapter *adapter = netdev_priv(dev);

	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
		return smp_processor_id();

	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
		return 0; /* all untagged traffic should default to TC 0 */

	return skb_tx_hash(dev, skb);
}

#endif /* HAVE_NETDEV_SELECT_QUEUE */
#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops ixgbe_netdev_ops = {
	.ndo_open		= &ixgbe_open,
	.ndo_stop		= &ixgbe_close,
	.ndo_start_xmit		= &ixgbe_xmit_frame_ps,
	.ndo_get_stats		= &ixgbe_get_stats,
	.ndo_set_rx_mode	= &ixgbe_set_rx_mode,
	.ndo_set_multicast_list	= &ixgbe_set_rx_mode,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= &ixgbe_set_mac,
	.ndo_change_mtu		= &ixgbe_change_mtu,
#ifdef ETHTOOL_OPS_COMPAT
	.ndo_do_ioctl		= &ixgbe_ioctl,
#endif
	.ndo_tx_timeout		= &ixgbe_tx_timeout,
	.ndo_vlan_rx_register	= &ixgbe_vlan_rx_register,
	.ndo_vlan_rx_add_vid	= &ixgbe_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= &ixgbe_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= &ixgbe_netpoll,
#endif
	.ndo_select_queue	= &ixgbe_select_queue,
};

#endif /* HAVE_NET_DEVICE_OPS */

void ixgbe_assign_netdev_ops(struct net_device *dev)
{
	struct ixgbe_adapter *adapter;
	adapter = netdev_priv(dev);
#ifdef HAVE_NET_DEVICE_OPS
	dev->netdev_ops = &ixgbe_netdev_ops;
#else /* HAVE_NET_DEVICE_OPS */
	dev->open = &ixgbe_open;
	dev->stop = &ixgbe_close;
	dev->hard_start_xmit = &ixgbe_xmit_frame_ps;
	dev->get_stats = &ixgbe_get_stats;
#ifdef HAVE_SET_RX_MODE
	dev->set_rx_mode = &ixgbe_set_rx_mode;
#endif
	dev->set_multicast_list = &ixgbe_set_rx_mode;
	dev->set_mac_address = &ixgbe_set_mac;
	dev->change_mtu = &ixgbe_change_mtu;
#ifdef ETHTOOL_OPS_COMPAT
	dev->do_ioctl = &ixgbe_ioctl;
#endif
#ifdef HAVE_TX_TIMEOUT
	dev->tx_timeout = &ixgbe_tx_timeout;
#endif
#ifdef NETIF_F_HW_VLAN_TX
	dev->vlan_rx_register = &ixgbe_vlan_rx_register;
	dev->vlan_rx_add_vid = &ixgbe_vlan_rx_add_vid;
	dev->vlan_rx_kill_vid = &ixgbe_vlan_rx_kill_vid;
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = &ixgbe_netpoll;
#endif
#ifdef HAVE_NETDEV_SELECT_QUEUE
	dev->select_queue = &ixgbe_select_queue;
#endif /* HAVE_NETDEV_SELECT_QUEUE */
#endif /* HAVE_NET_DEVICE_OPS */
	ixgbe_set_ethtool_ops(dev);
	dev->watchdog_timeo = 5 * HZ;
}

#define MAX_ADAPTERS 64

static int adapters_found;
struct ixgbe_adapter *adapters[MAX_ADAPTERS];

/**
 * ixgbe_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in ixgbe_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * ixgbe_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
static int __devinit ixgbe_probe(struct pci_dev *pdev,
                                 const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct ixgbe_adapter *adapter = NULL;
	struct ixgbe_hw *hw = NULL;
	int i, err, pci_using_dac;
	u32 part_num;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) &&
	    !pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64))) {
		pci_using_dac = 1;
	} else {
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (err) {
				dev_err(&pdev->dev, "No usable DMA "
				        "configuration, aborting\n");
				goto err_dma;
			}
		}
		pci_using_dac = 0;
	}

	err = pci_request_regions(pdev, ixgbe_driver_name);
	if (err) {
		dev_err(&pdev->dev, "pci_request_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}

	/*
	 * Workaround of Silicon errata on 82598. Disable LOs in the PCI switch
	 * port to which the 82598 is connected to prevent duplicate
	 * completions caused by LOs.  We need the mac type so that we only
	 * do this on 82598 devices, ixgbe_set_mac_type does this for us if
	 * we set it's device ID.
	 */
	hw = vmalloc(sizeof(struct ixgbe_hw));
	if (!hw) {
		printk(KERN_INFO "Unable to allocate memory for LOs fix "
			"- not checked\n");
	} else {
		hw->vendor_id = pdev->vendor;
		hw->device_id = pdev->device;
		ixgbe_set_mac_type(hw);
		if (hw->mac.type == ixgbe_mac_82598EB)
			pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S);
		vfree(hw);
	}

	pci_enable_pcie_error_reporting(pdev);

	pci_set_master(pdev);

#ifdef HAVE_TX_MQ
	netdev = alloc_etherdev_mq(sizeof(struct ixgbe_adapter), MAX_TX_QUEUES);
#else
	netdev = alloc_etherdev(sizeof(struct ixgbe_adapter));
#endif
	if (!netdev) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	pci_set_drvdata(pdev, netdev);
	adapter = netdev_priv(netdev);

	adapter->netdev = netdev;
	adapter->pdev = pdev;
	hw = &adapter->hw;
	hw->back = adapter;
	adapter->msg_enable = (1 << DEFAULT_DEBUG_LEVEL_SHIFT) - 1;

#ifdef HAVE_PCI_ERS
	/*
	 * call save state here in standalone driver because it relies on
	 * adapter struct to exist, and needs to call netdev_priv
	 */
	pci_save_state(pdev);

#endif
	hw->hw_addr = ioremap(pci_resource_start(pdev, 0),
	                      pci_resource_len(pdev, 0));
	if (!hw->hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}

	for (i = 1; i <= 5; i++) {
		if (pci_resource_len(pdev, i) == 0)
			continue;
	}

	ixgbe_assign_netdev_ops(netdev);

	strcpy(netdev->name, pci_name(pdev));

	adapter->bd_number = adapters_found;

#ifdef IXGBE_TCP_TIMER
	adapter->msix_addr = ioremap(pci_resource_start(pdev, 3),
	                             pci_resource_len(pdev, 3));
	if (!adapter->msix_addr) {
		err = -EIO;
		printk("Error in ioremap of BAR3\n");
		goto err_map_msix;
	}

#endif
	/* set up this timer and work struct before calling sw_init which
	 * might start the timer */
	init_timer(&adapter->sfp_timer);
	adapter->sfp_timer.function = &ixgbe_sfp_timer;
	adapter->sfp_timer.data = (unsigned long) adapter;

	INIT_WORK(&adapter->sfp_task, ixgbe_sfp_task);

	/* multispeed fiber has its own tasklet, called from GPI SDP1 context */
	INIT_WORK(&adapter->multispeed_fiber_task, ixgbe_multispeed_fiber_task);

	/* a new SFP+ module arrival, called from GPI SDP2 context */
	INIT_WORK(&adapter->sfp_config_module_task,
	          ixgbe_sfp_config_module_task);

	/* setup the private structure */
	err = ixgbe_sw_init(adapter);
	if (err)
		goto err_sw_init;

	/*
	 * If we have a fan, this is as early we know, warn if we
	 * have had a failure.
	 */
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) {
		u32 esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);
		if (esdp & IXGBE_ESDP_SDP1)
			DPRINTK(PROBE, CRIT,
				"Fan has stopped, replace the adapter\n");
	}

	/* reset_hw fills in the perm_addr as well */
	err = hw->mac.ops.reset_hw(hw);
	if (err == IXGBE_ERR_SFP_NOT_PRESENT &&
	    hw->mac.type == ixgbe_mac_82598EB) {
		/*
		 * Start a kernel thread to watch for a module to arrive.
		 * Only do this for 82598, since 82599 will generate interrupts
		 * on module arrival.
		 */
		set_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state);
		mod_timer(&adapter->sfp_timer,
		          round_jiffies(jiffies + (2 * HZ)));
		err = 0;
	} else if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		DPRINTK(PROBE, ERR, "failed to load because an "
		        "unsupported SFP+ module type was detected.\n");
		goto err_sw_init;
	} else if (err) {
		DPRINTK(PROBE, ERR, "HW Init failed: %d\n", err);
		goto err_sw_init;
	}

	/* check_options must be called before setup_link_speed to set up
	 * hw->fc completely
	 */
	ixgbe_check_options(adapter);

#ifdef MAX_SKB_FRAGS
#ifdef NETIF_F_HW_VLAN_TX
	netdev->features = NETIF_F_SG |
			   NETIF_F_IP_CSUM |
			   NETIF_F_HW_VLAN_TX |
			   NETIF_F_HW_VLAN_RX |
			   NETIF_F_HW_VLAN_FILTER;

#else
	netdev->features = NETIF_F_SG | NETIF_F_IP_CSUM;

#endif
#ifdef NETIF_F_IPV6_CSUM
	netdev->features |= NETIF_F_IPV6_CSUM;
#endif
#ifdef NETIF_F_TSO
	netdev->features |= NETIF_F_TSO;
#ifdef NETIF_F_TSO6
	netdev->features |= NETIF_F_TSO6;
#endif /* NETIF_F_TSO6 */
#endif /* NETIF_F_TSO */
#ifdef NETIF_F_GRO
	netdev->features |= NETIF_F_GRO;
#endif /* NETIF_F_GRO */
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
		adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED)
		adapter->flags &= ~(IXGBE_FLAG_FDIR_HASH_CAPABLE
				    | IXGBE_FLAG_FDIR_PERFECT_CAPABLE);
#ifndef IXGBE_NO_HW_RSC
	if (adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE) {
#ifdef NETIF_F_LRO
		netdev->features |= NETIF_F_LRO;
#endif
#ifndef IXGBE_NO_LRO
		adapter->flags2 &= ~IXGBE_FLAG2_SWLRO_ENABLED;
#endif
		adapter->flags2 |= IXGBE_FLAG2_RSC_ENABLED;
	} else {
#endif
#ifndef IXGBE_NO_LRO
#ifdef NETIF_F_LRO
		netdev->features |= NETIF_F_LRO;
#endif
		adapter->flags2 |= IXGBE_FLAG2_SWLRO_ENABLED;
#endif
#ifndef IXGBE_NO_HW_RSC
		adapter->flags2 &= ~IXGBE_FLAG2_RSC_ENABLED;
	}
#endif
#ifdef HAVE_NETDEV_VLAN_FEATURES
#ifdef NETIF_F_TSO
	netdev->vlan_features |= NETIF_F_TSO;
#ifdef NETIF_F_TSO6
	netdev->vlan_features |= NETIF_F_TSO6;
#endif /* NETIF_F_TSO6 */
#endif /* NETIF_F_TSO */
	netdev->vlan_features |= NETIF_F_IP_CSUM;
	netdev->vlan_features |= NETIF_F_SG;

#endif /* HAVE_NETDEV_VLAN_FEATURES */
#ifdef CONFIG_DCB
	netdev->dcbnl_ops = &dcbnl_ops;
#endif

	if (pci_using_dac)
		netdev->features |= NETIF_F_HIGHDMA;

#endif /* MAX_SKB_FRAGS */
	/* make sure the EEPROM is good */
	if (hw->eeprom.ops.validate_checksum &&
	    (hw->eeprom.ops.validate_checksum(hw, NULL) < 0)) {
		DPRINTK(PROBE, ERR, "The EEPROM Checksum Is Not Valid\n");
		err = -EIO;
		goto err_sw_init;
	}

	memcpy(netdev->dev_addr, hw->mac.perm_addr, netdev->addr_len);
#ifdef ETHTOOL_GPERMADDR
	memcpy(netdev->perm_addr, hw->mac.perm_addr, netdev->addr_len);

	if (ixgbe_validate_mac_addr(netdev->perm_addr)) {
		DPRINTK(PROBE, INFO, "invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}
#else
	if (ixgbe_validate_mac_addr(netdev->dev_addr)) {
		DPRINTK(PROBE, INFO, "invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}
#endif

	init_timer(&adapter->watchdog_timer);
	adapter->watchdog_timer.function = &ixgbe_watchdog;
	adapter->watchdog_timer.data = (unsigned long)adapter;

	INIT_WORK(&adapter->reset_task, ixgbe_reset_task);
	INIT_WORK(&adapter->watchdog_task, ixgbe_watchdog_task);

	err = ixgbe_init_interrupt_scheme(adapter);
	if (err)
		goto err_sw_init;

	switch (pdev->device) {
	case IXGBE_DEV_ID_82599_KX4:
		adapter->wol = (IXGBE_WUFC_MAG | IXGBE_WUFC_EX |
		                IXGBE_WUFC_MC | IXGBE_WUFC_BC);
		/* Enable ACPI wakeup in GRC */
		IXGBE_WRITE_REG(hw, IXGBE_GRC,
		             (IXGBE_READ_REG(hw, IXGBE_GRC) & ~IXGBE_GRC_APME));
		break;
	default:
		adapter->wol = 0;
		break;
	}
	device_init_wakeup(&adapter->pdev->dev, true);
	device_set_wakeup_enable(&adapter->pdev->dev, adapter->wol);

	/* save off EEPROM version number */
	ixgbe_read_eeprom(hw, 0x29, &adapter->eeprom_version);

	/* reset the hardware with the new settings */
	err = hw->mac.ops.start_hw(hw);
	if (err == IXGBE_ERR_EEPROM_VERSION) {
		/* We are running on a pre-production device, log a warning */
		DPRINTK(PROBE, INFO, "This device is a pre-production adapter/"
		        "LOM.  Please be aware there may be issues associated "
		        "with your hardware.  If you are experiencing problems "
		        "please contact your Intel or hardware representative "
		        "who provided you with this hardware.\n");
	}
	/* pick up the PCI bus settings for reporting later */
	if (hw->mac.ops.get_bus_info)
		hw->mac.ops.get_bus_info(hw);


	strcpy(netdev->name, "xge%d");
	err = register_netdev(netdev);
	if (err)
		goto err_register;

	adapter->netdev_registered = true;

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);
	/* keep stopping all the transmit queues for older kernels */
	netif_tx_stop_all_queues(netdev);

	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		INIT_WORK(&adapter->fdir_reinit_task, ixgbe_fdir_reinit_task);

#ifdef IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_CAPABLE) {
		err = dca_add_requester(&pdev->dev);
		switch (err) {
		case 0:
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_dca(adapter);
			break;
		/* -19 is returned from the kernel when no provider is found */
		case -19:
			DPRINTK(PROBE, INFO, "No DCA provider found.  Please "
			        "start ioatdma for DCA functionality.\n");
			break;
		default:
			DPRINTK(PROBE, INFO, "DCA registration failed: %d\n",
			        err);
			break;
		}
	}

#endif
	/* print all messages at the end so that we use our eth%d name */
	/* print bus type/speed/width info */
	DPRINTK(PROBE, INFO, "(PCI Express:%s:%s) ",
		((hw->bus.speed == ixgbe_bus_speed_5000) ? "5.0Gb/s":
		 (hw->bus.speed == ixgbe_bus_speed_2500) ? "2.5Gb/s":"Unknown"),
		 (hw->bus.width == ixgbe_bus_width_pcie_x8) ? "Width x8" :
		 (hw->bus.width == ixgbe_bus_width_pcie_x4) ? "Width x4" :
		 (hw->bus.width == ixgbe_bus_width_pcie_x1) ? "Width x1" :
		 ("Unknown"));

	/* print the MAC address */
	for (i = 0; i < 6; i++)
		printk("%2.2x%c", netdev->dev_addr[i], i == 5 ? '\n' : ':');

	ixgbe_read_pba_num(hw, &part_num);
	if (ixgbe_is_sfp(hw) && hw->phy.sfp_type != ixgbe_sfp_type_not_present)
		DPRINTK(PROBE, INFO, "MAC: %d, PHY: %d, SFP+: %d, PBA No: %06x-%03x\n",
		        hw->mac.type, hw->phy.type, hw->phy.sfp_type,
		        (part_num >> 8), (part_num & 0xff));
	else
		DPRINTK(PROBE, INFO, "MAC: %d, PHY: %d, PBA No: %06x-%03x\n",
		        hw->mac.type, hw->phy.type,
		        (part_num >> 8), (part_num & 0xff));

	if (hw->bus.width <= ixgbe_bus_width_pcie_x4) {
		DPRINTK(PROBE, WARNING, "PCI-Express bandwidth available for "
			"this card is not sufficient for optimal "
			"performance.\n");
		DPRINTK(PROBE, WARNING, "For optimal performance a x8 "
			"PCI-Express slot is required.\n");
	}

#ifndef IXGBE_NO_LRO
	if (adapter->flags2 & IXGBE_FLAG2_SWLRO_ENABLED)
		DPRINTK(PROBE, INFO, "Internal LRO is enabled \n");
	else
		DPRINTK(PROBE, INFO, "LRO is disabled \n");
#endif
#ifndef IXGBE_NO_HW_RSC
	if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
		DPRINTK(PROBE, INFO, "HW RSC is enabled \n");
#endif
#if defined(HAVE_NETDEV_STORAGE_ADDRESS) && defined(NETDEV_HW_ADDR_T_SAN)
	/* add san mac addr to netdev */
	ixgbe_add_sanmac_netdev(netdev);

#endif /* (HAVE_NETDEV_STORAGE_ADDRESS) && (NETDEV_HW_ADDR_T_SAN) */

	/* do not use any advanced features :) - adaline */
	netdev->features = 0;
	netdev->vlan_features = 0;

	/* XXX: workaround for invalid numa node allocation */
	adapter->numa_node = (pdev->bus->number & 0x80) ? 1 : 0;
	set_dev_node(&pdev->dev, adapter->numa_node);

	DPRINTK(PROBE, INFO, "Intel(R) 10 Gigabit Network Connection\n");
	DPRINTK(PROBE, INFO, "NUMA node = %d, flags = 0x%x\n", 
			adapter->numa_node, adapter->flags);
	
	if (adapter->flags & IXGBE_FLAG_RX_KERNEL_ENABLE) {
		DPRINTK(PROBE, INFO, "Received packets will be passed to "
			"Linux TCP/IP stack if the RX ring is detached\n");
	} else {
		DPRINTK(PROBE, INFO, "Received packets will be discarded "
			"if the RX ring is detached\n");
	}
	adapters[adapters_found] = adapter;
	adapters_found++;
	return 0;

err_register:
	ixgbe_clear_interrupt_scheme(adapter);
	ixgbe_release_hw_control(adapter);
err_sw_init:
	clear_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state);
	del_timer_sync(&adapter->sfp_timer);
	cancel_work_sync(&adapter->sfp_task);
	cancel_work_sync(&adapter->multispeed_fiber_task);
	cancel_work_sync(&adapter->sfp_config_module_task);
#ifdef IXGBE_TCP_TIMER
	iounmap(adapter->msix_addr);
err_map_msix:
#endif
	iounmap(hw->hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	pci_release_regions(pdev);
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

/**
 * ixgbe_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * ixgbe_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/
static void __devexit ixgbe_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	set_bit(__IXGBE_DOWN, &adapter->state);
	/*
	 * clear the module not found bit to make sure the worker won't
	 * reschedule
	 */
	clear_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state);
	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->sfp_timer);
	cancel_work_sync(&adapter->watchdog_task);
	cancel_work_sync(&adapter->sfp_task);
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		cancel_work_sync(&adapter->fdir_reinit_task);
	cancel_work_sync(&adapter->multispeed_fiber_task);
	cancel_work_sync(&adapter->sfp_config_module_task);
	flush_scheduled_work();

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_DCA_ENABLED;
		dca_remove_requester(&pdev->dev);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 1);
	}

#endif
#if defined(HAVE_NETDEV_STORAGE_ADDRESS) && defined(NETDEV_HW_ADDR_T_SAN)
	/* remove the added san mac */
	ixgbe_del_sanmac_netdev(netdev);

#endif /* (HAVE_NETDEV_STORAGE_ADDRESS) && (NETDEV_HW_ADDR_T_SAN) */
	if (adapter->netdev_registered) {
		unregister_netdev(netdev);
		adapter->netdev_registered = false;
	}

	ixgbe_clear_interrupt_scheme(adapter);
	ixgbe_release_hw_control(adapter);

#ifdef IXGBE_TCP_TIMER
	iounmap(adapter->msix_addr);
#endif
	iounmap(adapter->hw.hw_addr);
	pci_release_regions(pdev);

	DPRINTK(PROBE, INFO, "complete\n");
	free_netdev(netdev);

	pci_disable_pcie_error_reporting(pdev);

	pci_disable_device(pdev);
}

u16 ixgbe_read_pci_cfg_word(struct ixgbe_hw *hw, u32 reg)
{
	u16 value;
	struct ixgbe_adapter *adapter = hw->back;

	pci_read_config_word(adapter->pdev, reg, &value);
	return value;
}

void ixgbe_write_pci_cfg_word(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	struct ixgbe_adapter *adapter = hw->back;

	pci_write_config_word(adapter->pdev, reg, value);
}

#ifdef HAVE_PCI_ERS
/**
 * ixgbe_io_error_detected - called when PCI error is detected
 * @pdev: Pointer to PCI device
 * @state: The current pci connection state
 *
 * This function is called after a PCI bus error affecting
 * this device has been detected.
 */
static pci_ers_result_t ixgbe_io_error_detected(struct pci_dev *pdev,
                                                pci_channel_state_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	netif_device_detach(netdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (netif_running(netdev))
		ixgbe_down(adapter);
	pci_disable_device(pdev);

	/* Request a slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

/**
 * ixgbe_io_slot_reset - called after the pci bus has been reset.
 * @pdev: Pointer to PCI device
 *
 * Restart the card from scratch, as if from a cold-boot.
 */
static pci_ers_result_t ixgbe_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	pci_ers_result_t result;

	if (pci_enable_device(pdev)) {
		DPRINTK(PROBE, ERR,
			"Cannot re-enable PCI device after reset.\n");
		result = PCI_ERS_RESULT_DISCONNECT;
	} else {
		pci_set_master(pdev);
		pci_restore_state(pdev);

		pci_wake_from_d3(pdev, false);

		ixgbe_reset(adapter);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_WUS, ~0);
		result = PCI_ERS_RESULT_RECOVERED;
	}

	pci_cleanup_aer_uncorrect_error_status(pdev);

	return result;
}

/**
 * ixgbe_io_resume - called when traffic can start flowing again.
 * @pdev: Pointer to PCI device
 *
 * This callback is called when the error recovery driver tells us that
 * its OK to resume normal operation.
 */
static void ixgbe_io_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (netif_running(netdev)) {
		if (ixgbe_up(adapter)) {
			DPRINTK(PROBE, INFO, "ixgbe_up failed after reset\n");
			return;
		}
	}

	netif_device_attach(netdev);
}

static struct pci_error_handlers ixgbe_err_handler = {
	.error_detected = ixgbe_io_error_detected,
	.slot_reset = ixgbe_io_slot_reset,
	.resume = ixgbe_io_resume,
};

#endif
static struct pci_driver ixgbe_driver = {
	.name     = ixgbe_driver_name,
	.id_table = ixgbe_pci_tbl,
	.probe    = ixgbe_probe,
	.remove   = __devexit_p(ixgbe_remove),
#ifdef CONFIG_PM
	.suspend  = ixgbe_suspend,
	.resume   = ixgbe_resume,
#endif
#ifndef USE_REBOOT_NOTIFIER
	.shutdown = ixgbe_shutdown,
#endif
#ifdef HAVE_PCI_ERS
	.err_handler = &ixgbe_err_handler
#endif
};

bool ixgbe_is_ixgbe(struct pci_dev *pcidev)
{
	if (pci_dev_driver(pcidev) != &ixgbe_driver)
		return false;
	else
		return true;
}

#if MAX_PACKET_SIZE * MAX_CHUNK_SIZE % PAGE_SIZE != 0
#error must be a multiple of PAGE_SIZE!
#endif

int ps_open(struct inode *inode, struct file *filp)
{
	struct ps_context *context;

	context = kmalloc_node(sizeof(struct ps_context), 
			GFP_USER, numa_node_id());
	if (!context) {
		printk(KERN_ERR "Allocation of ps_context failed\n");
		return -ENOMEM;
	}

	memset(context, 0, sizeof(struct ps_context));

	context->info = kmalloc_node(sizeof(struct ps_pkt_info) * MAX_CHUNK_SIZE, 
			GFP_USER, numa_node_id());
	if (!context->info) {
		printk(KERN_ERR "Allocation of ps_context->info failed\n");
		return -ENOMEM;
	}

	init_MUTEX(&context->sem);
	init_waitqueue_head(&context->wq);

	context->num_attached = 0;
	context->next_ring = 0;

	context->num_bufs = 0;

	filp->private_data = context;

	return 0;
}

int ps_release(struct inode *inode, struct file *filp)
{
	struct ps_context *context = filp->private_data;
	int i;

	if (!context)
		return 0;

	for (i = 0; i < context->num_attached; i++) {
		struct ixgbe_ring *rx_ring;
		
		rx_ring = context->rx_rings[i];
		spin_lock_bh(&rx_ring->lock);

		rx_ring->wq = NULL;
		
		/* Enable the RX interrupt
		 * XXX: it assumes 1-to-1 queue-vector matching */
		ixgbe_irq_enable_queues(rx_ring->adapter, (u64)1 << rx_ring->reg_idx);

		spin_unlock_bh(&rx_ring->lock);
	}

	kfree(context->info);
	kfree(context);
	filp->private_data = NULL;

	return 0;
}

void ps_vma_open(struct vm_area_struct *vma)
{
	struct file *filp = vma->vm_file;
	struct ps_context *context;
	int i;

	if (!filp) {
		printk(KERN_ERR "vma->vm_file == NULL\n");
		return;
	}

	context = filp->private_data;

	if (!context) {
		printk(KERN_ERR "context == NULL\n");
		return;
	}

	for (i = 0; i < context->num_bufs; i++)
		if (context->ubufs[i] == (char *)vma->vm_start)
			break;

	if (i == context->num_bufs) {
		printk(KERN_ERR "unknown address: %p\n", (void *)vma->vm_start);
		return;
	}

	context->buf_refcnt[i]++;
}

void ps_vma_close(struct vm_area_struct *vma)
{
	struct file *filp = vma->vm_file;
	struct ps_context *context;
	int i;

	if (!filp) {
		printk(KERN_ERR "vma->vm_file == NULL\n");
		return;
	}

	context = filp->private_data;

	if (!context) {
		printk(KERN_ERR "context == NULL\n");
		return;
	}

	for (i = 0; i < context->num_bufs; i++)
		if (context->ubufs[i] == (char *)vma->vm_start)
			break;

	if (i == context->num_bufs) {
		printk(KERN_ERR "unknown address: %p\n", (void *)vma->vm_start);
		return;
	}

	if (--context->buf_refcnt[i] <= 0) {
		char *addr;
		int size;

		size = PAGE_ALIGN(MAX_PACKET_SIZE * MAX_CHUNK_SIZE);
		addr = context->kbufs[i];

		while (size > 0) {
			ClearPageReserved(vmalloc_to_page(addr));
			addr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}

		vfree(context->kbufs[i]);

		while (i < context->num_bufs - 1) {
			context->kbufs[i] = context->kbufs[i + 1];
			context->ubufs[i] = context->ubufs[i + 1];
			context->buf_refcnt[i] = context->buf_refcnt[i + 1];
			i++;
		}

		context->num_bufs--;		
	}
}

static struct vm_operations_struct ps_vm_ops = {
	.open = ps_vma_open,
	.close = ps_vma_close,
};

int ps_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ps_context *context = filp->private_data;
	unsigned long up;
	char *buf;
	char *kp;
	int err = 0;
	int size;
	
	if (!context) {
		printk("ps_mmap() failed: context == NULL\n");
		return -EINVAL;
	}

	if (context->num_bufs >= MAX_BUFS) {
		printk("ps_mmap() failed: exceeding %d bufs\n", MAX_BUFS);
		return -ENOMEM;
	}
	
	size = vma->vm_end - vma->vm_start;
	if (PAGE_ALIGN(size) != size) {
		 printk("ps_mmap() failed: invalid size\n");
		 return -EINVAL;
	}

	buf = vmalloc_node(size, numa_node_id());

	if (!buf) {
		printk(KERN_ERR "Allocation of a mmaped buffer failed"
				"(size = %d)\n",
				size);
		return -ENOMEM;
	}

	memset(buf, 0, size);

	if (vma->vm_pgoff) {

		return -EINVAL;
	}

	kp = buf;
	for (up = vma->vm_start; up < vma->vm_end; up += PAGE_SIZE) {
		struct page *page = vmalloc_to_page(kp);
		SetPageReserved(page);
		err = vm_insert_page(vma, up, page);
		if (err)
			break;
		kp += PAGE_SIZE;
	}

	if (err) {
		for (kp = buf; size > 0; kp += PAGE_SIZE, size -= PAGE_SIZE)
			ClearPageReserved(vmalloc_to_page(kp));
		vfree(buf);
		printk(KERN_ERR "ps_mmap() failed! err=%d\n", err);
	} else  {
		int i = context->num_bufs;

		context->kbufs[i] = buf;
		context->ubufs[i] = (char *)vma->vm_start;
		context->num_bufs++;
		
		vma->vm_ops = &ps_vm_ops;
		ps_vma_open(vma);
	}
		
	return err;
}

int ps_list_devices(struct ps_device __user *devices_usr)
{
	struct ps_device *devices;
	int i;
	int n;

	devices = kmalloc(sizeof(struct ps_device) * MAX_DEVICES, GFP_USER);
	if (!devices) {
		printk(KERN_ERR "Allocation of devices failed\n");
		return -ENOMEM;
	}

	n = min(MAX_DEVICES, adapters_found);

	for (i = 0; i < n; i++) {
		struct ixgbe_adapter *adapter = adapters[i];
		struct net_device *netdev = adapter->netdev;
		struct in_device *indev;

		strcpy(devices[i].name, netdev->name);
		devices[i].ifindex = i;
		devices[i].kifindex = netdev->ifindex;
		devices[i].num_rx_queues = adapter->num_rx_queues;
		devices[i].num_tx_queues = adapter->num_tx_queues;
		memcpy(devices[i].dev_addr, netdev->dev_addr, ETH_ALEN);

		devices[i].ip_addr = 0;

		indev = in_dev_get(netdev);
		if (!indev)
			continue;

		if (indev->ifa_list)
			devices[i].ip_addr = indev->ifa_list->ifa_local;

		in_dev_put(indev);
	}

	if (copy_to_user(devices_usr, devices, 
				n * sizeof(struct ps_device)))
		return -EFAULT;

	kfree(devices);

	return n;
}

int ps_attach_rx_device(struct ps_context *context,
		struct ps_queue __user *queue_usr)
{
	struct ps_queue queue;
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *rx_ring;

	if (copy_from_user(&queue, queue_usr, sizeof(struct ps_queue)))
		return -EFAULT;

	if (queue.ifindex < 0 || queue.ifindex >= adapters_found) {
		printk(KERN_ERR "ps_attach_rx_device(): invalid ifindex\n");
		return -EINVAL;
	}

	adapter = adapters[queue.ifindex];
	if (queue.qidx < 0 || queue.qidx >= adapter->num_rx_queues) {
		printk(KERN_ERR "ps_attach_rx_device(): invalid qidx\n");
		return -EINVAL;
	}

	rx_ring = &adapter->rx_ring[queue.qidx];
	if (!rx_ring) {
		printk(KERN_ERR "ps_attach_rx_device(): NULL rx_ring[i]?\n");
		return -EINVAL;
	}

	spin_lock_bh(&rx_ring->lock);

	if (rx_ring->wq) {
		printk(KERN_ERR "ps_attach_rx_device(): already held\n");
		spin_unlock_bh(&rx_ring->lock);
		return -EBUSY;
	}

	rx_ring->wq = &context->wq;

	/* Disable the RX interrupt
	 * XXX: it assumes 1-to-1 queue-vector matching */
	ixgbe_irq_disable_queues(rx_ring->adapter, (u64)1 << rx_ring->reg_idx);

	spin_unlock_bh(&rx_ring->lock);

	context->rx_rings[context->num_attached] = rx_ring;
	context->num_attached++;

	return 0;
}

int ps_detach_rx_device(struct ps_context *context,
		struct ps_queue __user *queue_usr)
{
	struct ps_queue queue;
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *rx_ring;
	int i;

	if (copy_from_user(&queue, queue_usr, sizeof(struct ps_queue)))
		return -EFAULT;

	if (queue.ifindex < 0 || queue.ifindex >= adapters_found) {
		printk(KERN_ERR "ps_detach_rx_device(): invalid ifindex\n");
		return -EINVAL;
	}

	adapter = adapters[queue.ifindex];
	if (queue.qidx < 0 || queue.qidx >= adapter->num_rx_queues) {
		printk(KERN_ERR "ps_detach_rx_device(): invalid qidx\n");
		return -EINVAL;
	}

	rx_ring = &adapter->rx_ring[queue.qidx];
	if (!rx_ring) {
		printk(KERN_ERR "ps_detach_rx_device(): NULL rx_ring[i]?\n");
		return -EINVAL;
	}

	spin_lock_bh(&rx_ring->lock);
	
	if (!rx_ring->wq) {
		printk(KERN_ERR "ps_detach_rx_device(): not held\n");
		spin_unlock_bh(&rx_ring->lock);
		return -EBUSY;
	}

	if (rx_ring->wq != &context->wq) {
		printk(KERN_ERR "ps_detach_rx_device(): owned by another\n");
		spin_unlock_bh(&rx_ring->lock);
		return -EBUSY;
	}

	rx_ring->wq = NULL;

	/* Enable the RX interrupt
	 * XXX: it assumes 1-to-1 queue-vector matching */
	ixgbe_irq_enable_queues(rx_ring->adapter, (u64)1 << rx_ring->reg_idx);

	spin_unlock_bh(&rx_ring->lock);

	for (i = 0; i < context->num_attached; i++)
		if (context->rx_rings[i] == rx_ring) {
			if (context->next_ring > i)
				context->next_ring--;

			while (i + 1 < context->num_attached) {
				context->rx_rings[i] = context->rx_rings[i + 1];
				i++;
			}

			context->num_attached--;
			return 0;
		}

	/* BUG */
	printk(KERN_CRIT "rx_ring is not found in context->rx_rings\n");
	return 0;
}

static inline void memcpy_aligned_rx(void *to, const void *from, size_t len)
{
	if (len <= 64) {
		memcpy(to, from, 64);
	} else if (len <= 128) {
		memcpy(to, from, 64);
		memcpy((uint8_t *)to + 64, (uint8_t *)from + 64, 64);
	} else {
		int offset;

		for (offset = 0; offset < len; offset += 64) {
			if (offset + 128 < len) {
				prefetchnta((uint8_t *)from + offset + 128);
				prefetcht0((uint8_t *)to + offset + 128);
			}

			memcpy((uint8_t *)to + offset, 
					(uint8_t *)from + offset, 
					64);
		}
	}
}

int copy_rx_packets(struct ixgbe_ring *rx_ring, 
		int n,
		struct ps_pkt_info *info,
		char *pkt_buf,
		int offset)
{
	struct ixgbe_adapter *adapter = rx_ring->adapter;
	union ixgbe_adv_rx_desc *rx_desc;

	u32 len = 64;
	u32 staterr;

	int qidx = rx_ring->next_to_clean;
	int next_qidx = rx_ring->next_to_clean;
	int cnt = 0;
	u8 *src;

	unsigned int total_rx_packets = 0;
	unsigned int total_rx_bytes = 0;

	src = packet_buf(rx_ring, qidx);

	prefetcht0(pkt_buf + offset + 64 * 0);
	prefetcht0(pkt_buf + offset + 64 * 1);

	prefetchnta(IXGBE_RX_DESC_ADV(*rx_ring, qidx + 0));
	prefetchnta(IXGBE_RX_DESC_ADV(*rx_ring, qidx + 1));
	
	prefetchnta(src + MAX_PACKET_SIZE * 0);
	prefetchnta(src + MAX_PACKET_SIZE * 1);
	prefetchnta(src + MAX_PACKET_SIZE * 2);
	prefetchnta(src + MAX_PACKET_SIZE * 3);

	while (cnt < n) {
		rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, qidx);
		staterr = le32_to_cpu(rx_desc->wb.upper.status_error);

		src = packet_buf(rx_ring, qidx);

		prefetchnta(src + MAX_PACKET_SIZE * 4);
		if (len > 64)
			prefetchnta(src + MAX_PACKET_SIZE * 4 + 64);

		prefetchnta(rx_desc + 2);
		prefetcht0(pkt_buf + offset + 64 * 2);

		next_qidx = (qidx + 1) % rx_ring->count;

		if (!(staterr & IXGBE_RXD_STAT_DD))
			break;
		
		if (unlikely(!(staterr & IXGBE_RXD_STAT_EOP))) {
			printk("found non-EOP packets!\n");
			goto next;
		}

		if (unlikely(staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK)) {
			printk("found error frames\n");
			goto next;
		}

		len = le16_to_cpu(rx_desc->wb.upper.length);

		if (unlikely(len > MAX_PACKET_SIZE)) {
			printk("invalid packet length!\n");
			goto next;
		}

		memcpy_aligned_rx(pkt_buf + offset, src, len);

		info[cnt].offset = offset;
		info[cnt].len = len;

		if (staterr & IXGBE_RXD_STAT_IPCS) {
			if (unlikely(staterr & IXGBE_RXDADV_ERR_IPE))
				info[cnt].checksum_rx = PS_CHECKSUM_RX_BAD;
			else
				info[cnt].checksum_rx = PS_CHECKSUM_RX_GOOD;
		} else
			info[cnt].checksum_rx = PS_CHECKSUM_RX_UNKNOWN;

		cnt++;
		
		total_rx_packets++;
		total_rx_bytes += len;
		offset = ALIGN(offset + len, 64);

next:
		rx_desc->read.pkt_addr = rx_desc->read.hdr_addr =
				cpu_to_le64(packet_dma(rx_ring, qidx));

		qidx = next_qidx;
	}

	if (cnt > 0) {
		rx_ring->queued = qidx;
		rx_ring->next_to_clean = qidx;
		rx_ring->next_to_use = 
				(qidx == 0) ? (rx_ring->count - 1) : (qidx - 1);

		rx_ring->stats.packets += total_rx_packets;
		rx_ring->stats.bytes += total_rx_bytes;
		rx_ring->total_packets += total_rx_packets;
		rx_ring->total_bytes += total_rx_bytes;

		ixgbe_release_rx_desc(&adapter->hw, rx_ring, rx_ring->next_to_use);
	}

	return cnt;
}

int ps_recv_chunk(struct ps_context *context, struct ps_chunk __user *chunk_usr)
{
	struct ps_chunk chunk;
	struct ixgbe_ring *rx_ring;

	char *kbuf = NULL;

	int i, j;
	int ret;
	int tmp;

	int offset;

	DEFINE_WAIT(wait);

	if (copy_from_user(&chunk, chunk_usr, sizeof(chunk)))
		return -EFAULT;

	/*
	printk("ps_recv_chunk() called %d %p %p\n",
			chunk.cnt, chunk.info, chunk.buf);
	*/

	if (chunk.cnt <= 0 || chunk.cnt > MAX_CHUNK_SIZE)
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, chunk.info, 
			chunk.cnt * sizeof(struct ps_pkt_info)))
		return -EFAULT;

	for (i = 0; i < context->num_bufs; i++) {
		if (context->ubufs[i] == chunk.buf) {
			kbuf = context->kbufs[i];
			break;
		}
	}
	
	if (!kbuf)
		return -EFAULT;

	if (context->num_attached == 0)
		return -EINVAL;

	offset = ALIGN((u64)kbuf, 64) - (u64)kbuf;

retry:
	if (chunk.recv_blocking)
		prepare_to_wait(&context->wq, &wait, TASK_INTERRUPTIBLE);

	for (i = 0; i < context->num_attached; i++) {
		j = (context->next_ring + i) % context->num_attached;
		rx_ring = context->rx_rings[j];

		if (unlikely(!rx_ring || !rx_ring->desc))
			continue;
		
		spin_lock_bh(&rx_ring->lock);
		ret = copy_rx_packets(rx_ring, 
				chunk.cnt, 
				context->info,
				kbuf, 
				offset);
		spin_unlock_bh(&rx_ring->lock);

		if (ret) {
			if (chunk.recv_blocking)
				finish_wait(&context->wq, &wait);
			goto found;
		}

		if (chunk.recv_blocking) {
			/* 
			 * Enable the RX interrupt temporarily
			 * XXX: it assumes 1-to-1 queue-vector matching 
			 */
			ixgbe_irq_enable_queues(rx_ring->adapter, 
					(u64)1 << rx_ring->reg_idx);
		}
	}

	if (chunk.recv_blocking) {
		schedule();
		finish_wait(&context->wq, &wait);

		if (signal_pending(current)) {
			/* printk("signal pending!!!\n"); */
			return -EINTR;
		}
		goto retry;
	} else
		return -EWOULDBLOCK;

found:
	tmp = copy_to_user(chunk.info, context->info, 
			ret * sizeof(struct ps_pkt_info));
	if (tmp) {
		printk("copy_to_user failed - %ld requested %d failed\n",
				ret * sizeof(struct ps_pkt_info),
				tmp);
		ret = -EFAULT;
	}

	/*
	dump(context->info, context->buf, ret);
	*/

	context->next_ring = (context->next_ring + 1) % context->num_attached;

	put_user(rx_ring->adapter->bd_number, &chunk_usr->queue.ifindex);
	put_user(rx_ring->queue_index, &chunk_usr->queue.qidx);
	return ret;
}

int ps_recv_chunk_ifidx(struct ps_context *context, struct ps_chunk __user *chunk_usr)
{
	struct ps_chunk chunk;
	struct ixgbe_ring *rx_ring;

	char *kbuf = NULL;

	int i;
	int ret;
	int tmp;

	int offset;

	if (copy_from_user(&chunk, chunk_usr, sizeof(chunk)))
		return -EFAULT;
	
	if (chunk.cnt <= 0 || chunk.cnt > MAX_CHUNK_SIZE)
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, chunk.info, 
			chunk.cnt * sizeof(struct ps_pkt_info)))
		return -EFAULT;

	for (i = 0; i < context->num_bufs; i++)
		if (context->ubufs[i] == chunk.buf) {
			kbuf = context->kbufs[i];
			break;
		}

	if (!kbuf)
		return -EFAULT;

	if (context->num_attached == 0)
		return -EINVAL;

	offset = ALIGN((u64)kbuf, 64) - (u64)kbuf;

	rx_ring = context->rx_rings[chunk.queue.ifindex];

	if (unlikely(!rx_ring || !rx_ring->desc))
		return -EINVAL;
	
	spin_lock_bh(&rx_ring->lock);
	ret = copy_rx_packets(rx_ring, 
			chunk.cnt, 
			context->info,
			kbuf, 
			offset);
	spin_unlock_bh(&rx_ring->lock);
	
	if (ret <= 0)
		return -EWOULDBLOCK;

	tmp = copy_to_user(chunk.info, context->info, 
			ret * sizeof(struct ps_pkt_info));
	if (tmp) {
		printk("copy_to_user failed - %ld requested %d failed\n",
				ret * sizeof(struct ps_pkt_info),
				tmp);
		ret = -EFAULT;
	}

	put_user(rx_ring->adapter->bd_number, &chunk_usr->queue.ifindex);
	
	return ret;
}

int ps_send_chunk(struct ps_context *context, struct ps_chunk __user *chunk_usr)
{
	struct ps_chunk chunk;
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *tx_ring;

	char *kbuf = NULL;

	int ret;
	int i;

	if (copy_from_user(&chunk, chunk_usr, sizeof(chunk)))
		return -EFAULT;
	
	if (chunk.cnt <= 0 || chunk.cnt > MAX_CHUNK_SIZE)
		return -EINVAL;

	if (chunk.queue.ifindex < 0 || chunk.queue.ifindex >= adapters_found)
		return -EINVAL;

	adapter = adapters[chunk.queue.ifindex];

	if (chunk.queue.qidx < 0 || chunk.queue.qidx >= adapter->num_tx_queues)
		return -EINVAL;

	tx_ring = &adapter->tx_ring[chunk.queue.qidx];

	ret = copy_from_user(context->info, chunk.info,
			chunk.cnt * sizeof(struct ps_pkt_info));
	if (ret) {
		printk("copy_from_user(1) failed - %ld requested %d failed\n",
				chunk.cnt * sizeof(struct ps_pkt_info),
				ret);
		return -EFAULT;
	}

	for (i = 0; i < context->num_bufs; i++)
		if (context->ubufs[i] == chunk.buf) {
			kbuf = context->kbufs[i];
			break;
		}

	if (!kbuf)
		return -EFAULT;

	spin_lock_bh(&tx_ring->lock);
	ret = ixgbe_xmit_batch(tx_ring, chunk.cnt, context->info, kbuf);
	spin_unlock_bh(&tx_ring->lock);

	return ret;
}

int ps_send_chunk_buf(struct ps_context *context, struct ps_chunk_buf __user *chunk_usr)
{
	struct ps_chunk_buf chunk;
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *tx_ring;

	char *kbuf = NULL;

	int ret, i, send_cnt;
	
	if (copy_from_user(&chunk, chunk_usr, sizeof(chunk)))
		return -EFAULT;
	
	if (chunk.cnt <= 0)
		return -EINVAL;

	if (chunk.queue.ifindex < 0 || chunk.queue.ifindex >= adapters_found)
		return -EINVAL;

	adapter = adapters[chunk.queue.ifindex];

	if (chunk.queue.qidx < 0 || chunk.queue.qidx >= adapter->num_tx_queues)
		return -EINVAL;
	
	tx_ring = &adapter->tx_ring[chunk.queue.qidx];

	/* copy information from chunk */
	if (chunk.next_to_send + chunk.cnt > ENTRY_CNT)
		send_cnt = ENTRY_CNT - chunk.next_to_send;
	else
		send_cnt = chunk.cnt;
	send_cnt = MIN(PS_SEND_MIN, send_cnt);

	ret = copy_from_user(context->info, chunk.info + chunk.next_to_send,
			send_cnt * sizeof(struct ps_pkt_info));
	if (ret) {
		printk("copy_from_user(1) failed - %ld requested %d failed\n",
				send_cnt * sizeof(struct ps_pkt_info),
				ret);
		return -EFAULT;
	}
	/* find an mapped kernel buffer */
	for (i = 0; i < context->num_bufs; i++)
		if (context->ubufs[i] == chunk.buf) {
			kbuf = context->kbufs[i];
			break;
		}
	if (!kbuf)
		return -EFAULT;
		
	spin_lock_bh(&tx_ring->lock);
	ret = ixgbe_xmit_batch(tx_ring, send_cnt, context->info, kbuf);
	spin_unlock_bh(&tx_ring->lock);
	return ret;
}

//#define MAX_SLACK       (100 * NSEC_PER_MSEC)
#define MAX_SLACK       (NSEC_PER_MSEC)
static long __estimate_accuracy(struct timespec *tv) {
	long slack;
	int divfactor = 1000;

	if(tv->tv_sec < 0)
		return 0;

	if (task_nice(current) > 0)
		divfactor = divfactor / 5;

	if (tv->tv_sec > MAX_SLACK / (NSEC_PER_SEC/divfactor))
		return MAX_SLACK;

	slack = tv->tv_nsec / divfactor;
	slack += tv->tv_sec * (NSEC_PER_SEC/divfactor);

	if (slack > MAX_SLACK)
		return MAX_SLACK;

	return slack;
}
static long ps_select_estimate_accuracy(struct timespec *tv)
{
	 unsigned long ret;
	 struct timespec now;   

	 if (rt_task(current))
		 return 0;

	 ktime_get_ts(&now);
	 now = timespec_sub(*tv, now);
	 ret = __estimate_accuracy(&now);
	 if (ret < current->timer_slack_ns)
		 return current->timer_slack_ns;
	 return ret;
}
void set_normalized_timespec(struct timespec *ts, time_t sec, s64 nsec) 
{
	while (nsec >= NSEC_PER_SEC) {
		asm("" : "+rm"(nsec));
		nsec -= NSEC_PER_SEC;
		++sec;
	}
	while (nsec < 0) {
		asm("" : "+rm"(nsec));
		nsec += NSEC_PER_SEC;
		--sec;
	}
	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}
struct timespec timespec_add_safe(const struct timespec lhs, 
								const struct timespec rhs) {
	struct timespec res;

	set_normalized_timespec(&res, lhs.tv_sec + rhs.tv_sec,
			lhs.tv_nsec + rhs.tv_nsec);
	if (res.tv_sec < lhs.tv_sec || res.tv_sec < rhs.tv_sec)
		res.tv_sec = TIME_T_MAX;
	return res;
}
static inline struct timespec ps_set_mstimeout(long ms)
{
	struct timespec now, ts = {
		.tv_sec = ms / MSEC_PER_SEC,
		.tv_nsec = NSEC_PER_MSEC * (ms % MSEC_PER_SEC),
	};

	ktime_get_ts(&now);
	return timespec_add_safe(now, ts);
}
static inline struct timespec ps_set_ustimeout(long us)
{
	struct timespec now, ts = {
		.tv_sec = us / USEC_PER_SEC,
		.tv_nsec = NSEC_PER_USEC * (us % USEC_PER_SEC),
	};

	ktime_get_ts(&now);
	return timespec_add_safe(now, ts);
}
int ps_select(struct ps_context *context,
		struct ps_event __user *evt_usr) {

	struct ps_event event;
	struct ixgbe_ring *tx_ring = NULL, *rx_ring = NULL;
	int tx_count = 0;
	int max_rx_nid = -1, max_tx_nid = -1;
	int qidx;
	union ixgbe_adv_rx_desc *rx_desc;
	u32 staterr;
	int i;
	long slack = 0;
	ktime_t expires, *to = NULL;
	struct ixgbe_adapter *adapter;
	nids_set rx_set, tx_set;

	DEFINE_WAIT(wait);

	if (copy_from_user(&event, evt_usr, sizeof(event)))
		return -EFAULT;
	
	if (context->num_attached == 0)
		return -EINVAL;
	
	// no event is set
	if (event.rx_nids == 0 && event.tx_nids == 0)
		return 1;

	if (event.rx_nids != 0) {
		i = sizeof(nids_set)*8 - 1;
		while (!(event.rx_nids >> i--));
		max_rx_nid = i + 1;
	}
	
	if (event.tx_nids != 0) {
		i = sizeof(nids_set)*8 - 1;
		while (!(event.tx_nids >> i--));
		max_tx_nid = i + 1;
	}

	if (max_rx_nid >= adapters_found || max_tx_nid >= adapters_found) {
		return -EINVAL;
	}
	
	/* DANGER */
	// every interface should have same number of queues
	// in the current (& default) PSIO install script 
	adapter = adapters[0]; 
	if (event.qidx < 0 || 
			((event.qidx >= adapter->num_rx_queues) && event.rx_nids) || 
			((event.qidx >= adapter->num_tx_queues) && event.tx_nids) ) {
		return -EINVAL;	
	}

	if (event.timeout > 0) {
		/* calculate what time event should be ended up */
		struct timespec end_time = ps_set_ustimeout(event.timeout);
		
		/* no slack calculation: let slack be zero for us control */
		slack = ps_select_estimate_accuracy(&end_time);
		to = &expires;
		*to = timespec_to_ktime(end_time);
	}

	if (event.timeout != 0)
		prepare_to_wait(&context->wq, &wait, TASK_INTERRUPTIBLE);

rx:
	NID_ZERO(rx_set);
	
	/* check rx queue */
	if (event.rx_nids != 0) {
		for (i = 0; i <= max_rx_nid; i++) {
			if (NID_ISSET(i, event.rx_nids)) {
				rx_ring = &adapters[i]->rx_ring[event.qidx];

				if (unlikely(!rx_ring || !rx_ring->desc))
					goto tx;

				qidx = rx_ring->next_to_clean;
				prefetchnta(IXGBE_RX_DESC_ADV(*rx_ring, qidx));

				rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, qidx);
				staterr = le32_to_cpu(rx_desc->wb.upper.status_error);

				// rx_queue event condition
				if ((staterr & IXGBE_RXD_STAT_DD)) {
					NID_SET(i, rx_set); // setting event for rx queue i
				}
			}
		}
	}

tx:
	NID_ZERO(tx_set);

	/* check tx queue */
	if (event.tx_nids >= 0) {
		for (i = 0; i <= max_tx_nid; i++) {
			if (NID_ISSET(i, event.tx_nids)) {
				tx_ring = &adapters[i]->tx_ring[event.qidx];

				if (unlikely(!tx_ring || !tx_ring->desc))
					goto check;

				if (tx_ring->next_to_clean <= tx_ring->next_to_use)
					tx_count = tx_ring->count - tx_ring->next_to_use + tx_ring->next_to_clean - 1;
				else
					tx_count = tx_ring->next_to_clean - tx_ring->next_to_use - 1;

				// tx_queue event condition
				if (tx_count >= PS_SEND_MIN) {
					NID_SET(i, tx_set); // setting event for tx queue i
				} else if (event.timeout != 0) {
					int tmp;
					ixgbe_clean_tx_irq(adapters[i], tx_ring, &tmp, PS_SEND_MIN);
				} 
			}
		}
	}

check:

	// wake up conditions - if any events is set
	if (tx_set != 0 || rx_set != 0) {
		if (event.timeout != 0)
			finish_wait(&context->wq, &wait);
		goto found;
	}
	
	/*
	 * Enable the TX/RX interrupt temporarily
	 * XXX: it assumes 1-to-1 queue-vector matching
	 */
	if (event.timeout != 0) {
		if (event.tx_nids != 0)
			ixgbe_irq_enable_queues(tx_ring->adapter, (u64)1 << tx_ring->reg_idx);
		if (event.rx_nids != 0)
			ixgbe_irq_enable_queues(rx_ring->adapter, (u64)1 << rx_ring->reg_idx);
	}

	if (event.timeout > 0) {
		/* wake up beween to and to + slack
		 * delta gives the freedom upto delta and it makes power & performance friendly
		 */
		if (!schedule_hrtimeout_range(to, slack, HRTIMER_MODE_ABS)) {
			finish_wait(&context->wq, &wait);
			goto found;
		}
	}
	if (event.timeout != 0) {
		schedule();
		finish_wait(&context->wq, &wait);
		
		if (signal_pending(current))
			return -EINTR;
	} else {
		return -EAGAIN;
	}

	goto rx;

found:
	event.rx_nids = rx_set;
	event.tx_nids = tx_set;

	if (copy_to_user(evt_usr, &event, sizeof(struct ps_event)))
		return -EFAULT;

	return 1;
}

int ps_slowpath_packet(struct ps_context *context, 
		struct ps_packet __user *pkt_usr)
{
	struct ps_packet pkt;
	struct ixgbe_adapter *adapter;

	struct sk_buff *skb;
	int ret;

	if (copy_from_user(&pkt, pkt_usr, sizeof(pkt))) {
		return -EFAULT;
	}

	if (pkt.len < ETH_HLEN || pkt.len > ETH_FRAME_LEN) {
		return -EINVAL;
	}

	if (pkt.ifindex < 0 || pkt.ifindex >= adapters_found) {
		return -EINVAL;
	}

	adapter = adapters[pkt.ifindex];
	skb = netdev_alloc_skb(adapter->netdev, pkt.len + NET_IP_ALIGN);

	if (skb == NULL) {
		printk(KERN_ERR "netdev_alloc_skb() failed!\n");
		return -ENOMEM;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	ret = copy_from_user(skb->data, pkt.buf, pkt.len);
	if (ret) {
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	skb_put(skb, pkt.len);
	skb->protocol = eth_type_trans(skb, adapter->netdev);

	local_bh_disable();
	netif_rx(skb);
	local_bh_enable();

	return 0;
}
		
int ps_get_txentry(struct ps_context *context, 
		struct ps_queue __user *queue_usr)
{
	struct ps_queue queue;
	struct ixgbe_adapter *adapter;
	struct ixgbe_ring *tx_ring;
	int left;

	if (copy_from_user(&queue, queue_usr, sizeof(struct ps_queue)))
		return -EFAULT;
	
	if (queue.ifindex < 0 || queue.ifindex >= adapters_found) {
		printk(KERN_ERR "ps_get_txentry(): invalid ifindex\n");
		return -EINVAL;
	}

	adapter = adapters[queue.ifindex];
	if (queue.qidx < 0 || queue.qidx >= adapter->num_tx_queues) {
		printk(KERN_ERR "ps_get_txentry(): invalid qidx\n");
		return -EINVAL;
	}

	tx_ring = &adapter->tx_ring[queue.qidx];
	if (!tx_ring) {
		printk(KERN_ERR "ps_get_txentry(): invalid qidx\n");
		return -EINVAL;
	}

//	spin_lock_bh(&tx_ring->lock);	

	if (tx_ring->next_to_clean <= tx_ring->next_to_use)
		left = tx_ring->count - tx_ring->next_to_use + tx_ring->next_to_clean - 1;
	else 
		left = tx_ring->next_to_clean - tx_ring->next_to_use - 1;
	
//	spin_unlock_bh(&tx_ring->lock);

	return left;
}

long ps_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ps_context *context;

	/*
	printk("ps_ioctl called() cmd=%u arg=%lu\n", cmd, arg);
	*/

	context = filp->private_data;
	if (!context)
		goto out;

	/*if (down_interruptible(&context->sem)) {
		ret = -ERESTARTSYS;
		goto out;
	}
	*/

	switch (cmd) {
	case PS_IOC_LIST_DEVICES:
		ret = ps_list_devices((struct ps_device __user *)arg);
		break;

	case PS_IOC_ATTACH_RX_DEVICE:
		ret = ps_attach_rx_device(context, (struct ps_queue __user *)arg);
		break;

	case PS_IOC_DETACH_RX_DEVICE:
		ret = ps_detach_rx_device(context, (struct ps_queue __user *)arg);
		break;

	case PS_IOC_RECV_CHUNK:
		ret = ps_recv_chunk(context, (struct ps_chunk __user *)arg);
		break;

	case PS_IOC_RECV_CHUNK_IFIDX:
		ret = ps_recv_chunk_ifidx(context, (struct ps_chunk __user *) arg);
		break;

	case PS_IOC_SEND_CHUNK:
		ret = ps_send_chunk(context, (struct ps_chunk __user *)arg);
		break;
	
	case PS_IOC_SEND_CHUNK_BUF:
		ret = ps_send_chunk_buf(context, (struct ps_chunk_buf __user *)arg);
		break;

	case PS_IOC_SLOWPATH_PACKET:
		ret = ps_slowpath_packet(context, (struct ps_packet __user *)arg);
		break;
	
	case PS_IOC_GET_TXENTRY:
		ret = ps_get_txentry(context, (struct ps_queue __user *)arg);
		break;
	
	case PS_IOC_SELECT:
		ret = ps_select(context, (struct ps_event __user *)arg);
		break;

	default:
		ret = -ENOTTY;
	};

	//up(&context->sem);

out:
	/*
	printk("ps_ioctl returns %d\n", ret);
	*/
	return ret;
}

static struct file_operations ps_fops = {
	.open = ps_open,
	.release = ps_release,
	.mmap = ps_mmap,
	.unlocked_ioctl = ps_ioctl,
};

/**
 * ixgbe_init_module - Driver Registration Routine
 *
 * ixgbe_init_module is the first routine called when the driver is
 * loaded. 
 **/
static int __init ixgbe_init_module(void)
{
	int ret;

	printk(KERN_INFO "ixgbe: %s - version %s (PacketShader)\n", ixgbe_driver_string,
	       ixgbe_driver_version);

	printk(KERN_INFO "%s\n", ixgbe_copyright);

#ifndef CONFIG_DCB
	ixgbe_dcb_netlink_register();
#endif
#ifdef IXGBE_DCA
	dca_register_notify(&dca_notifier);
#endif

	ret = pci_register_driver(&ixgbe_driver);
	if (ret < 0) {
		printk(KERN_ERR "pci_register_driver() failed\n");
		return ret;
	}

	ret = register_chrdev(PS_MAJOR, PS_NAME, &ps_fops);
	if (ret < 0)
		printk(KERN_ERR "register_chrdev() failed\n");

	return ret;
}

module_init(ixgbe_init_module);

/**
 * ixgbe_exit_module - Driver Exit Cleanup Routine
 *
 * ixgbe_exit_module is called just before the driver is removed
 * from memory.
 **/
static void __exit ixgbe_exit_module(void)
{
	unregister_chrdev(PS_MAJOR, PS_NAME);

#ifdef IXGBE_DCA
	dca_unregister_notify(&dca_notifier);
#endif
#ifndef CONFIG_DCB
	ixgbe_dcb_netlink_unregister();
#endif
	pci_unregister_driver(&ixgbe_driver);
}

#ifdef IXGBE_DCA
static int ixgbe_notify_dca(struct notifier_block *nb, unsigned long event,
                            void *p)
{
	int ret_val;

	ret_val = driver_for_each_device(&ixgbe_driver.driver, NULL, &event,
	                                 __ixgbe_notify_dca);

	return ret_val ? NOTIFY_BAD : NOTIFY_DONE;
}
#endif /* CONFIG_DCA or CONFIG_DCA_MODULE */
module_exit(ixgbe_exit_module);

/* ixgbe_main.c */

