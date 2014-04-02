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

#ifndef _IXGBE_H_
#define _IXGBE_H_

#ifndef IXGBE_NO_LRO
#include <net/tcp.h>
#endif

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>

#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif
#ifdef NETIF_F_HW_VLAN_TX
#include <linux/if_vlan.h>
#endif

#if 0
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_DCA
#include <linux/dca.h>
#endif
#endif

#include "ixgbe_dcb.h"


#include "kcompat.h"

#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#define IXGBE_FCOE
#include "ixgbe_fcoe.h"
#endif /* CONFIG_FCOE or CONFIG_FCOE_MODULE */

#include "ixgbe_api.h"

#define PFX "ixgbe: "
#define DPRINTK(nlevel, klevel, fmt, args...) \
	((void)((NETIF_MSG_##nlevel & adapter->msg_enable) && \
	printk(KERN_##klevel PFX "%s: %s: " fmt, adapter->netdev->name, \
		__FUNCTION__ , ## args)))


/* TX/RX descriptor defines */
#define IXGBE_DEFAULT_TXD	4096
#define IXGBE_MAX_TXD		4096
#define IXGBE_MIN_TXD		64

#define IXGBE_DEFAULT_RXD	4096
#define IXGBE_MAX_RXD		4096
#define IXGBE_MIN_RXD		64

#define IXGBE_SUBWINDOW_BITS	10
#define IXGBE_SUBWINDOW_SIZE	(1 << IXGBE_SUBWINDOW_BITS)
#define IXGBE_SUBWINDOW_MASK	(IXGBE_SUBWINDOW_SIZE - 1)
#define IXGBE_MAX_SUBWINDOWS	(IXGBE_MAX_TXD / IXGBE_SUBWINDOW_SIZE)

/* flow control */
#define IXGBE_DEFAULT_FCRTL		0x10000
#define IXGBE_MIN_FCRTL			   0x40
#define IXGBE_MAX_FCRTL			0x7FF80
#define IXGBE_DEFAULT_FCRTH		0x20000
#define IXGBE_MIN_FCRTH			  0x600
#define IXGBE_MAX_FCRTH			0x7FFF0
#define IXGBE_DEFAULT_FCPAUSE		 0xFFFF
#define IXGBE_MIN_FCPAUSE		      0
#define IXGBE_MAX_FCPAUSE		 0xFFFF

/* Supported Rx Buffer Sizes */
#define IXGBE_RXBUFFER_64    64     /* Used for packet split */
#define IXGBE_RXBUFFER_128   128    /* Used for packet split */
#define IXGBE_RXBUFFER_256   256    /* Used for packet split */
#define IXGBE_RXBUFFER_2048  2048
#define IXGBE_RXBUFFER_4096  4096
#define IXGBE_RXBUFFER_8192  8192
#define IXGBE_MAX_RXBUFFER   16384  /* largest size for single descriptor */

#define IXGBE_RX_HDR_SIZE IXGBE_RXBUFFER_256

#define MAXIMUM_ETHERNET_VLAN_SIZE (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)

#if defined(IXGBE_DCB) || defined(IXGBE_RSS) || \
    defined(IXGBE_VMDQ)
#define IXGBE_MQ
#endif

/* How many Rx Buffers do we bundle into one write to the hardware ? */
#define IXGBE_RX_BUFFER_WRITE	16	/* Must be power of 2 */

#define IXGBE_TX_FLAGS_CSUM		(u32)(1)
#define IXGBE_TX_FLAGS_VLAN		(u32)(1 << 1)
#define IXGBE_TX_FLAGS_TSO		(u32)(1 << 2)
#define IXGBE_TX_FLAGS_IPV4		(u32)(1 << 3)
#define IXGBE_TX_FLAGS_FCOE		(u32)(1 << 4)
#define IXGBE_TX_FLAGS_FSO		(u32)(1 << 5)
#define IXGBE_TX_FLAGS_VLAN_MASK	0xffff0000
#define IXGBE_TX_FLAGS_VLAN_PRIO_MASK	0x0000e000
#define IXGBE_TX_FLAGS_VLAN_SHIFT	16

#define IXGBE_MAX_RSC_INT_RATE          162760

#ifndef IXGBE_NO_LRO
#define IXGBE_LRO_MAX 32	/*Maximum number of LRO descriptors*/
#define IXGBE_LRO_GLOBAL 10

struct ixgbe_lro_stats {
	u32 flushed;
	u32 coal;
	u32 recycled;
};

struct ixgbe_lro_desc {
	struct  hlist_node lro_node;
	struct  sk_buff *skb;
	u32   source_ip;
	u32   dest_ip;
	u16   source_port;
	u16   dest_port;
	u16   vlan_tag;
	u16   len;
	u32   next_seq;
	u32   ack_seq;
	u16   window;
	u16   mss;
	u16   opt_bytes;
	u16   psh:1;
	u32   tsval;
	u32   tsecr;
	u32   append_cnt;
};

struct ixgbe_lro_list {
	struct hlist_head active;
	struct hlist_head free;
	int active_cnt;
	struct ixgbe_lro_stats stats;
};

#endif /* IXGBE_NO_LRO */
/* wrapper around a pointer to a socket buffer,
 * so a DMA handle can be stored along with the buffer */
struct ixgbe_tx_buffer {
	unsigned long time_stamp;
	u16 length;
	u16 next_to_watch;
};

struct ixgbe_rx_buffer {
	u16 length;
};

struct ixgbe_queue_stats {
	u64 packets;
	u64 bytes;
};

struct ____cacheline_aligned ixgbe_ring {
	void *desc;			/* descriptor ring memory */
	union {
		struct ixgbe_tx_buffer *tx_buffer_info;
		struct ixgbe_rx_buffer *rx_buffer_info;
	};

	struct ixgbe_adapter *adapter;

	u8 atr_sample_rate;
	u8 atr_count;
	u16 count;			/* amount of descriptors */
	u16 rx_buf_len;
	u16 next_to_use;
	u16 next_to_clean;

	u8 queue_index; /* needed for multiqueue queue management */

	u16 head;
	u16 tail;

	unsigned int total_bytes;
	unsigned int total_packets;

#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
	/* cpu for tx queue */
	int cpu;
#endif
	u16 reg_idx;			/* holds the special value that gets the
					 * hardware register offset associated
					 * with this ring, which is different
					 * for DCB and RSS modes */

	struct ixgbe_queue_stats stats;
	unsigned long reinit_state;
	u64 rsc_count;                 /* stat for coalesced packets */
	unsigned int size;		/* length in bytes */
	dma_addr_t dma;			/* phys. address of descriptor ring */

	/* [queued, next_to_clean): packets waiting to be pulled */
	u16 queued; /* only used for RX */

	u8 *window[IXGBE_MAX_SUBWINDOWS];
	dma_addr_t dma_window[IXGBE_MAX_SUBWINDOWS];
	unsigned int window_size;

	spinlock_t lock;
	wait_queue_head_t *wq;
};

enum ixgbe_ring_f_enum {
	RING_F_NONE = 0,
	RING_F_DCB,
	RING_F_VMDQ,
	RING_F_RXQ,
	RING_F_TXQ,
	RING_F_FDIR,
	RING_F_ARRAY_SIZE      /* must be last in enum set */
};

#define IXGBE_MAX_DCB_INDICES   8
#define IXGBE_MAX_RSS_INDICES  16
#define IXGBE_MAX_VMDQ_INDICES 64
#define IXGBE_MAX_FDIR_INDICES 64
struct ixgbe_ring_feature {
	int indices;
	int mask;
};

#define MAX_RX_QUEUES 128
#define MAX_TX_QUEUES 128

#define MAX_RX_PACKET_BUFFERS ((adapter->flags & IXGBE_FLAG_DCB_ENABLED) \
                               ? 8 : 1)
#define MAX_TX_PACKET_BUFFERS MAX_RX_PACKET_BUFFERS

/* MAX_MSIX_Q_VECTORS of these are allocated,
 * but we only use one per queue-specific vector.
 */
struct ixgbe_q_vector {
	struct ixgbe_adapter *adapter;
	unsigned int v_idx; /* index of q_vector within array, also used for
	                     * finding the bit in EICR and friends that
	                     * represents the vector for this ring */
#ifdef CONFIG_IXGBE_NAPI
	struct napi_struct napi;
#endif
	DECLARE_BITMAP(rxr_idx, MAX_RX_QUEUES); /* Rx ring indices */
	DECLARE_BITMAP(txr_idx, MAX_TX_QUEUES); /* Tx ring indices */
	u8 rxr_count;     /* Rx ring count assigned to this vector */
	u8 txr_count;     /* Tx ring count assigned to this vector */
	u8 tx_itr;
	u8 rx_itr;
	u32 eitr;
#ifndef IXGBE_NO_LRO
	struct ixgbe_lro_list *lrolist;   /* LRO list for queue vector*/
#endif
	char name[IFNAMSIZ + 9];
#ifndef HAVE_NETDEV_NAPI_LIST
	struct net_device poll_dev;
#endif
};


/* Helper macros to switch between ints/sec and what the register uses.
 * And yes, it's the same math going both ways.  The lowest value
 * supported by all of the ixgbe hardware is 8.
 */
#define EITR_INTS_PER_SEC_TO_REG(_eitr) \
	((_eitr) ? (1000000000 / ((_eitr) * 256)) : 8)
#define EITR_REG_TO_INTS_PER_SEC EITR_INTS_PER_SEC_TO_REG

#define IXGBE_DESC_UNUSED(R) \
	((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	(R)->next_to_clean - (R)->next_to_use - 1)

#define IXGBE_RX_DESC_ADV(R, i)	    \
	(&(((union ixgbe_adv_rx_desc *)((R).desc))[i]))
#define IXGBE_TX_DESC_ADV(R, i)	    \
	(&(((union ixgbe_adv_tx_desc *)((R).desc))[i]))
#define IXGBE_TX_CTXTDESC_ADV(R, i)	    \
	(&(((struct ixgbe_adv_tx_context_desc *)((R).desc))[i]))

#define IXGBE_MAX_JUMBO_FRAME_SIZE        16128

#ifdef IXGBE_TCP_TIMER
#define TCP_TIMER_VECTOR 1
#else
#define TCP_TIMER_VECTOR 0
#endif
#define OTHER_VECTOR 1
#define NON_Q_VECTORS (OTHER_VECTOR + TCP_TIMER_VECTOR)

#define IXGBE_MAX_MSIX_VECTORS_82599 64
#define IXGBE_MAX_MSIX_Q_VECTORS_82599 64
#define IXGBE_MAX_MSIX_Q_VECTORS_82598 16
#define IXGBE_MAX_MSIX_VECTORS_82598 18

/*
 * Only for array allocations in our adapter struct.  On 82598, there will be
 * unused entries in the array, but that's not a big deal.  Also, in 82599,
 * we can actually assign 64 queue vectors based on our extended-extended
 * interrupt registers.  This is different than 82598, which is limited to 16.
 */
#define MAX_MSIX_Q_VECTORS IXGBE_MAX_MSIX_Q_VECTORS_82599
#define MAX_MSIX_COUNT IXGBE_MAX_MSIX_VECTORS_82599

#if 0
#define MIN_MSIX_Q_VECTORS 2
#else
/* no TX interrupt - Sangjin */
#define MIN_MSIX_Q_VECTORS 1
#endif
#define MIN_MSIX_COUNT (MIN_MSIX_Q_VECTORS + NON_Q_VECTORS)

/* board specific private data structure */
struct ixgbe_adapter {
	struct timer_list watchdog_timer;
#ifdef NETIF_F_HW_VLAN_TX
	struct vlan_group *vlgrp;
#endif
	int bd_number;
	struct work_struct reset_task;
	struct ixgbe_q_vector *q_vector[MAX_MSIX_Q_VECTORS];
	struct ixgbe_dcb_config dcb_cfg;
	struct ixgbe_dcb_config temp_dcb_cfg;
	u8 dcb_set_bitmap;
	enum ixgbe_fc_mode last_lfc_mode;
	
	int numa_node;

	/* Interrupt Throttle Rate */
	u32 itr_setting;
	u16 eitr_low;
	u16 eitr_high;

	/* TX */
	struct ixgbe_ring *tx_ring;	/* One per active queue */
	int num_tx_queues;
	u64 restart_queue;
	u64 hw_csum_tx_good;
	u64 lsc_int;
	u64 hw_tso_ctxt;
	u64 hw_tso6_ctxt;
	u32 tx_timeout_count;
	bool detect_tx_hung;

	/* RX */
	struct ixgbe_ring *rx_ring;	/* One per active queue */
	int num_rx_queues;
	int num_rx_pools;               /* == num_rx_queues in 82598 */
	int num_rx_queues_per_pool;	/* 1 if 82598, can be many if 82599 */
	u64 hw_csum_rx_error;
	u64 hw_rx_no_dma_resources;
	u64 hw_csum_rx_good;
	u64 non_eop_descs;
#ifndef CONFIG_IXGBE_NAPI
	u64 rx_dropped_backlog;		/* count drops from rx intr handler */
#endif
	int num_msix_vectors;
	int max_msix_q_vectors;         /* true count of q_vectors for device */
	struct ixgbe_ring_feature ring_feature[RING_F_ARRAY_SIZE];
	struct msix_entry *msix_entries;
#ifdef IXGBE_TCP_TIMER
	irqreturn_t (*msix_handlers[MAX_MSIX_COUNT])(int irq, void *data,
	                                             struct pt_regs *regs);
#endif

	u32 alloc_rx_page_failed;
	u32 alloc_rx_buff_failed;

	/* Some features need tri-state capability,
	 * thus the additional *_CAPABLE flags.
	 */
	u32 flags;
#define IXGBE_FLAG_RX_CSUM_ENABLED              (u32)(1)
#define IXGBE_FLAG_MSI_CAPABLE                  (u32)(1 << 1)
#define IXGBE_FLAG_MSI_ENABLED                  (u32)(1 << 2)
#define IXGBE_FLAG_MSIX_CAPABLE                 (u32)(1 << 3)
#define IXGBE_FLAG_MSIX_ENABLED                 (u32)(1 << 4)
#ifndef IXGBE_NO_LLI
#define IXGBE_FLAG_LLI_PUSH                     (u32)(1 << 5)
#endif
#define IXGBE_FLAG_RX_1BUF_CAPABLE              (u32)(1 << 6)
#define IXGBE_FLAG_RX_PS_CAPABLE                (u32)(1 << 7)
#define IXGBE_FLAG_RX_PS_ENABLED                (u32)(1 << 8)
#define IXGBE_FLAG_IN_NETPOLL                   (u32)(1 << 9)
#define IXGBE_FLAG_DCA_ENABLED                  (u32)(1 << 10)
#define IXGBE_FLAG_DCA_CAPABLE                  (u32)(1 << 11)
#define IXGBE_FLAG_DCA_ENABLED_DATA             (u32)(1 << 12)
#define IXGBE_FLAG_MQ_CAPABLE                   (u32)(1 << 13)
#define IXGBE_FLAG_DCB_ENABLED                  (u32)(1 << 14)
#define IXGBE_FLAG_DCB_CAPABLE                  (u32)(1 << 15)
#define IXGBE_FLAG_RSS_ENABLED                  (u32)(1 << 16)
#define IXGBE_FLAG_RSS_CAPABLE                  (u32)(1 << 17)
#define IXGBE_FLAG_VMDQ_CAPABLE                 (u32)(1 << 18)
#define IXGBE_FLAG_VMDQ_ENABLED                 (u32)(1 << 19)
#define IXGBE_FLAG_FAN_FAIL_CAPABLE             (u32)(1 << 20)
#define IXGBE_FLAG_NEED_LINK_UPDATE             (u32)(1 << 22)
#define IXGBE_FLAG_IN_WATCHDOG_TASK             (u32)(1 << 23)
#define IXGBE_FLAG_IN_SFP_LINK_TASK             (u32)(1 << 24)
#define IXGBE_FLAG_IN_SFP_MOD_TASK              (u32)(1 << 25)
#define IXGBE_FLAG_FDIR_HASH_CAPABLE            (u32)(1 << 26)
#define IXGBE_FLAG_FDIR_PERFECT_CAPABLE         (u32)(1 << 27)

/* added - Sangjin */
#define IXGBE_FLAG_RX_KERNEL_ENABLE		(u32)(1 << 28)

	u32 flags2;
#ifndef IXGBE_NO_HW_RSC
#define IXGBE_FLAG2_RSC_CAPABLE                  (u32)(1)
#define IXGBE_FLAG2_RSC_ENABLED                  (u32)(1 << 1)
#endif /* IXGBE_NO_HW_RSC */
#ifndef IXGBE_NO_LRO
#define IXGBE_FLAG2_SWLRO_ENABLED                (u32)(1 << 2)
#endif /* IXGBE_NO_LRO */
#define IXGBE_FLAG2_VMDQ_DEFAULT_OVERRIDE        (u32)(1 << 3)

/* default to trying for four seconds */
#define IXGBE_TRY_LINK_TIMEOUT (4 * HZ)

	/* OS defined structs */
	struct net_device *netdev;
	struct pci_dev *pdev;
	struct net_device_stats net_stats;
#ifndef IXGBE_NO_LRO
	struct ixgbe_lro_stats lro_stats;
#endif

#ifdef ETHTOOL_TEST
	u32 test_icr;
	struct ixgbe_ring test_tx_ring;
	struct ixgbe_ring test_rx_ring;
#endif

	/* structs defined in ixgbe_hw.h */
	struct ixgbe_hw hw;
	u16 msg_enable;
	struct ixgbe_hw_stats stats;
#ifndef IXGBE_NO_LLI
	u32 lli_port;
	u32 lli_size;
	u64 lli_int;
	u32 lli_etype;
	u32 lli_vlan_pri;
#endif /* IXGBE_NO_LLI */
	/* Interrupt Throttle Rate */
	u32 eitr_param;

	unsigned long state;
	u32 *config_space;
	u64 tx_busy;
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;

	u32 link_speed;
	bool link_up;
	unsigned long link_check_timeout;

	struct work_struct watchdog_task;
	struct work_struct sfp_task;
	struct timer_list sfp_timer;
	struct work_struct multispeed_fiber_task;
	struct work_struct sfp_config_module_task;
	u64 flm;
	u32 fdir_pballoc;
	u32 atr_sample_rate;
	spinlock_t fdir_perfect_lock;
	struct work_struct fdir_reinit_task;
	u64 rsc_count;
	u32 wol;
	u16 eeprom_version;
	bool netdev_registered;
	char lsc_int_name[IFNAMSIZ + 9];
#ifdef IXGBE_TCP_TIMER
	char tcp_timer_name[IFNAMSIZ + 9];
#endif
};

enum ixbge_state_t {
	__IXGBE_TESTING,
	__IXGBE_RESETTING,
	__IXGBE_DOWN,
	__IXGBE_FDIR_INIT_DONE,
	__IXGBE_SFP_MODULE_NOT_FOUND
};

#ifdef CONFIG_DCB
extern struct dcbnl_rtnl_ops dcbnl_ops;
extern int ixgbe_copy_dcb_cfg(struct ixgbe_dcb_config *src_dcb_cfg,
			      struct ixgbe_dcb_config *dst_dcb_cfg, int tc_max);
#endif

/* needed by ixgbe_main.c */
extern int ixgbe_validate_mac_addr(u8 *mc_addr);
extern void ixgbe_check_options(struct ixgbe_adapter *adapter);
extern void ixgbe_assign_netdev_ops(struct net_device *netdev);

/* needed by ixgbe_ethtool.c */
extern char ixgbe_driver_name[];
extern const char ixgbe_driver_version[];

extern int ixgbe_up(struct ixgbe_adapter *adapter);
extern void ixgbe_down(struct ixgbe_adapter *adapter);
extern void ixgbe_reinit_locked(struct ixgbe_adapter *adapter);
extern void ixgbe_reset(struct ixgbe_adapter *adapter);
extern void ixgbe_set_ethtool_ops(struct net_device *netdev);
extern int ixgbe_setup_rx_resources(struct ixgbe_adapter *,struct ixgbe_ring *);
extern int ixgbe_setup_tx_resources(struct ixgbe_adapter *,struct ixgbe_ring *);
extern void ixgbe_free_rx_resources(struct ixgbe_adapter *,struct ixgbe_ring *);
extern void ixgbe_free_tx_resources(struct ixgbe_adapter *,struct ixgbe_ring *);
extern void ixgbe_update_stats(struct ixgbe_adapter *adapter);
extern int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter);
extern void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter);
extern bool ixgbe_is_ixgbe(struct pci_dev *pcidev);


void ixgbe_set_rx_mode(struct net_device *netdev);

#ifdef ETHTOOL_OPS_COMPAT
extern int ethtool_ioctl(struct ifreq *ifr);

#endif
extern int ixgbe_dcb_netlink_register(void);
extern int ixgbe_dcb_netlink_unregister(void);


#endif /* _IXGBE_H_ */
