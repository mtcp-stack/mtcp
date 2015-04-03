#ifndef __IXGBE_H__
#define __IXGBE_H__
/*-------------------------------------------------------------------------*/
#include <linux/if_vlan.h>
#include "ixgbe_dcb.h"
/*-------------------------------------------------------------------------*/
/* VLAN info */
#define IXGBE_TX_FLAGS_VLAN_MASK        0xffff0000
#define IXGBE_TX_FLAGS_VLAN_PRIO_MASK   0xe0000000
#define IXGBE_TX_FLAGS_VLAN_PRIO_SHIFT  29
#define IXGBE_TX_FLAGS_VLAN_SHIFT       16

#define IXGBE_MAX_RX_DESC_POLL          10

#define IXGBE_MAX_VF_MC_ENTRIES         30
#define IXGBE_MAX_VF_FUNCTIONS          64
#define IXGBE_MAX_VFTA_ENTRIES          128
#define MAX_EMULATION_MAC_ADDRS         16
#define IXGBE_MAX_PF_MACVLANS           15
#define IXGBE_82599_VF_DEVICE_ID        0x10ED
#define IXGBE_X540_VF_DEVICE_ID         0x1515

#define IXGBE_MAX_DCB_INDICES           8
#define IXGBE_MAX_RSS_INDICES           16
#define IXGBE_MAX_VMDQ_INDICES          64
#define IXGBE_MAX_FDIR_INDICES          63
#ifdef IXGBE_FCOE
#define IXGBE_MAX_FCOE_INDICES  8
#define MAX_RX_QUEUES   (IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#define MAX_TX_QUEUES   (IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#else
#define MAX_RX_QUEUES   (IXGBE_MAX_FDIR_INDICES + 1)
#define MAX_TX_QUEUES   (IXGBE_MAX_FDIR_INDICES + 1)
#endif /* IXGBE_FCOE */

#define TCP_TIMER_VECTOR        0
#define OTHER_VECTOR    1
#define NON_Q_VECTORS   (OTHER_VECTOR + TCP_TIMER_VECTOR)

#define IXGBE_MAX_MSIX_Q_VECTORS_82599  64
#define IXGBE_MAX_MSIX_Q_VECTORS_82598  16


/*
 * Only for array allocations in our adapter struct.  On 82598, there will be
 * unused entries in the array, but that's not a big deal.  Also, in 82599,
 * we can actually assign 64 queue vectors based on our extended-extended
 * interrupt registers.  This is different than 82598, which is limited to 16.
 */
#define MAX_MSIX_Q_VECTORS      IXGBE_MAX_MSIX_Q_VECTORS_82599
#define MAX_MSIX_COUNT          IXGBE_MAX_MSIX_VECTORS_82599

#define MIN_MSIX_Q_VECTORS      1
#define MIN_MSIX_COUNT          (MIN_MSIX_Q_VECTORS + NON_Q_VECTORS)

enum ixgbe_ring_f_enum {
        RING_F_NONE = 0,
        RING_F_VMDQ,  /* SR-IOV uses the same ring feature */
        RING_F_RSS,
        RING_F_FDIR,
#ifdef IXGBE_FCOE
	RING_F_FCOE,
#endif /* IXGBE_FCOE */
        RING_F_ARRAY_SIZE  /* must be last in enum set */
};

struct ixgbe_queue_stats {
	u64 packets;
        u64 bytes;
};

struct ixgbe_tx_queue_stats {
        u64 restart_queue;
	u64 tx_busy;
        u64 tx_done_old;
};

struct ixgbe_rx_queue_stats {
        u64 rsc_count;
        u64 rsc_flush;
        u64 non_eop_descs;
        u64 alloc_rx_page_failed;
        u64 alloc_rx_buff_failed;
        u64 csum_err;
};

struct ixgbe_ring_feature {
        u16 limit;      /* upper limit on feature indices */
	u16 indices;    /* current value of indices */
        u16 mask;       /* Mask used for feature to ring mapping */
        u16 offset;     /* offset to start of feature */
};

struct ixgbe_lro_stats {
        u32 flushed;
        u32 coal;
};

struct ixgbe_ring {
        struct ixgbe_ring *next;        /* pointer to next ring in q_vector */
        struct ixgbe_q_vector *q_vector; /* backpointer to host q_vector */
        struct net_device *netdev;      /* netdev ring belongs to */
        struct device *dev;             /* device for DMA mapping */
        void *desc;                     /* descriptor ring memory */
        union {
                struct ixgbe_tx_buffer *tx_buffer_info;
                struct ixgbe_rx_buffer *rx_buffer_info;
        };
        unsigned long state;
        u8 __iomem *tail;
        dma_addr_t dma;                 /* phys. address of descriptor ring */
        unsigned int size;              /* length in bytes */

        u16 count;                      /* amount of descriptors */

        u8 queue_index; /* needed for multiqueue queue management */
        u8 reg_idx;                     /* holds the special value that gets
                                         * the hardware register offset
                                         * associated with this ring, which is
                                         * different for DCB and RSS modes
                                         */
        u16 next_to_use;
        u16 next_to_clean;

#ifdef HAVE_PTP_1588_CLOCK
        unsigned long last_rx_timestamp;

#endif
        union {
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
                u16 rx_buf_len;
#else
                u16 next_to_alloc;
#endif
                struct {
                        u8 atr_sample_rate;
                        u8 atr_count;
                };
        };

        u8 dcb_tc;
        struct ixgbe_queue_stats stats;
        union {
                struct ixgbe_tx_queue_stats tx_stats;
                struct ixgbe_rx_queue_stats rx_stats;
        };
} ____cacheline_internodealigned_in_smp;

struct vf_macvlans {
	struct list_head l;
        int vf;
        bool free;
        bool is_macvlan;
        u8 vf_macvlan[ETH_ALEN];
};

#ifdef IXGBE_HWMON

#define IXGBE_HWMON_TYPE_LOC            0
#define IXGBE_HWMON_TYPE_TEMP           1
#define IXGBE_HWMON_TYPE_CAUTION        2
#define IXGBE_HWMON_TYPE_MAX            3

struct hwmon_attr {
        struct device_attribute dev_attr;
        struct ixgbe_hw *hw;
        struct ixgbe_thermal_diode_data *sensor;
        char name[12];
};

struct hwmon_buff {
        struct device *device;
        struct hwmon_attr *hwmon_list;
        unsigned int n_hwmon;
};
#endif /* IXGBE_HWMON */

/*-------------------------------------------------------------------------*/
struct ixgbe_adapter {
#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
#ifdef HAVE_VLAN_RX_REGISTER
	struct vlan_group *vlgrp; /* must be first, see ixgbe_receive_skb */
#else
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
#endif
#endif /* NETIF_F_HW_VLAN_TX || NETIF_F_HW_VLAN_CTAG_TX */
	/* OS defined structs */
	struct net_device *netdev;
	struct pci_dev *pdev;

	unsigned long state;

	/* Some features need tri-state capability,
	 * thus the additional *_CAPABLE flags.
	 */
	u32 flags;
#define IXGBE_FLAG_MSI_CAPABLE			(u32)(1 << 0)
#define IXGBE_FLAG_MSI_ENABLED			(u32)(1 << 1)
#define IXGBE_FLAG_MSIX_CAPABLE			(u32)(1 << 2)
#define IXGBE_FLAG_MSIX_ENABLED			(u32)(1 << 3)
#ifndef IXGBE_NO_LLI
#define IXGBE_FLAG_LLI_PUSH			(u32)(1 << 4)
#endif
#define IXGBE_FLAG_IN_NETPOLL                   (u32)(1 << 5)
#if defined(CONFIG_DCA) || defined(CONFIG_DCA_MODULE)
#define IXGBE_FLAG_DCA_ENABLED			(u32)(1 << 6)
#define IXGBE_FLAG_DCA_CAPABLE			(u32)(1 << 7)
#define IXGBE_FLAG_DCA_ENABLED_DATA		(u32)(1 << 8)
#else
#define IXGBE_FLAG_DCA_ENABLED			(u32)0
#define IXGBE_FLAG_DCA_CAPABLE			(u32)0
#define IXGBE_FLAG_DCA_ENABLED_DATA             (u32)0
#endif
#define IXGBE_FLAG_MQ_CAPABLE			(u32)(1 << 9)
#define IXGBE_FLAG_DCB_ENABLED			(u32)(1 << 10)
#define IXGBE_FLAG_VMDQ_ENABLED			(u32)(1 << 11)
#define IXGBE_FLAG_FAN_FAIL_CAPABLE		(u32)(1 << 12)
#define IXGBE_FLAG_NEED_LINK_UPDATE		(u32)(1 << 13)
#define IXGBE_FLAG_NEED_LINK_CONFIG		(u32)(1 << 14)
#define IXGBE_FLAG_FDIR_HASH_CAPABLE		(u32)(1 << 15)
#define IXGBE_FLAG_FDIR_PERFECT_CAPABLE		(u32)(1 << 16)
#ifdef IXGBE_FCOE
#define IXGBE_FLAG_FCOE_CAPABLE			(u32)(1 << 17)
#define IXGBE_FLAG_FCOE_ENABLED			(u32)(1 << 18)
#endif /* IXGBE_FCOE */
#define IXGBE_FLAG_SRIOV_CAPABLE		(u32)(1 << 19)
#define IXGBE_FLAG_SRIOV_ENABLED		(u32)(1 << 20)
#define IXGBE_FLAG_SRIOV_REPLICATION_ENABLE	(u32)(1 << 21)
#define IXGBE_FLAG_SRIOV_L2SWITCH_ENABLE	(u32)(1 << 22)
#define IXGBE_FLAG_SRIOV_L2LOOPBACK_ENABLE	(u32)(1 << 23)
#define IXGBE_FLAG_RX_HWTSTAMP_ENABLED          (u32)(1 << 24)

/* preset defaults */
#define IXGBE_FLAGS_82598_INIT		(IXGBE_FLAG_MSI_CAPABLE |	\
					 IXGBE_FLAG_MSIX_CAPABLE |	\
					 IXGBE_FLAG_MQ_CAPABLE)

#define IXGBE_FLAGS_82599_INIT		(IXGBE_FLAGS_82598_INIT |	\
					 IXGBE_FLAG_SRIOV_CAPABLE)

#define IXGBE_FLAGS_X540_INIT		IXGBE_FLAGS_82599_INIT


	u32 flags2;
#ifndef IXGBE_NO_HW_RSC
#define IXGBE_FLAG2_RSC_CAPABLE			(u32)(1 << 0)
#define IXGBE_FLAG2_RSC_ENABLED			(u32)(1 << 1)
#else
#define IXGBE_FLAG2_RSC_CAPABLE			0
#define IXGBE_FLAG2_RSC_ENABLED			0
#endif
#define IXGBE_FLAG2_TEMP_SENSOR_CAPABLE		(u32)(1 << 3)
#define IXGBE_FLAG2_TEMP_SENSOR_EVENT		(u32)(1 << 4)
#define IXGBE_FLAG2_SEARCH_FOR_SFP		(u32)(1 << 5)
#define IXGBE_FLAG2_SFP_NEEDS_RESET		(u32)(1 << 6)
#define IXGBE_FLAG2_RESET_REQUESTED		(u32)(1 << 7)
#define IXGBE_FLAG2_FDIR_REQUIRES_REINIT	(u32)(1 << 8)
#define IXGBE_FLAG2_RSS_FIELD_IPV4_UDP		(u32)(1 << 9)
#define IXGBE_FLAG2_RSS_FIELD_IPV6_UDP		(u32)(1 << 10)
#define IXGBE_FLAG2_PTP_ENABLED                 (u32)(1 << 11)
#define IXGBE_FLAG2_PTP_PPS_ENABLED		(u32)(1 << 12)

	/* Tx fast path data */
	int num_tx_queues;
	u16 tx_itr_setting;
	u16 tx_work_limit;

	/* Rx fast path data */
	int num_rx_queues;
	u16 rx_itr_setting;
	u16 rx_work_limit;

	/* TX */
	struct ixgbe_ring *tx_ring[MAX_TX_QUEUES] ____cacheline_aligned_in_smp;

	u64 restart_queue;
	u64 lsc_int;
	u32 tx_timeout_count;

	/* RX */
	struct ixgbe_ring *rx_ring[MAX_RX_QUEUES];
	int num_rx_pools;		/* == num_rx_queues in 82598 */
	int num_rx_queues_per_pool;	/* 1 if 82598, can be many if 82599 */
	u64 hw_csum_rx_error;
	u64 hw_rx_no_dma_resources;
	u64 rsc_total_count;
	u64 rsc_total_flush;
	u64 non_eop_descs;
	u32 alloc_rx_page_failed;
	u32 alloc_rx_buff_failed;

	struct ixgbe_q_vector *q_vector[MAX_MSIX_Q_VECTORS];

#ifdef HAVE_DCBNL_IEEE
	struct ieee_pfc *ixgbe_ieee_pfc;
	struct ieee_ets *ixgbe_ieee_ets;
#endif
	struct ixgbe_dcb_config dcb_cfg;
	struct ixgbe_dcb_config temp_dcb_cfg;
	u8 dcb_set_bitmap;
	u8 dcbx_cap;
#ifndef HAVE_MQPRIO
	u8 dcb_tc;
#endif
	enum ixgbe_fc_mode last_lfc_mode;

	int num_q_vectors;	/* current number of q_vectors for device */
	int max_q_vectors;	/* upper limit of q_vectors for device */
	struct ixgbe_ring_feature ring_feature[RING_F_ARRAY_SIZE];
	struct msix_entry *msix_entries;

#ifndef HAVE_NETDEV_STATS_IN_NETDEV
	struct net_device_stats net_stats;
#endif
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
	u32 lli_etype;
	u32 lli_vlan_pri;
#endif /* IXGBE_NO_LLI */

	u32 *config_space;
	u64 tx_busy;
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;

	u32 link_speed;
	bool link_up;
	unsigned long link_check_timeout;

	struct timer_list service_timer;
	struct work_struct service_task;

	struct hlist_head fdir_filter_list;
	unsigned long fdir_overflow; /* number of times ATR was backed off */
	union ixgbe_atr_input fdir_mask;
	int fdir_filter_count;
	u32 fdir_pballoc;
	u32 atr_sample_rate;
	spinlock_t fdir_perfect_lock;

#ifdef IXGBE_FCOE
	struct ixgbe_fcoe fcoe;
#endif /* IXGBE_FCOE */
	u32 wol;

	u16 bd_number;

	char eeprom_id[32];
	u16 eeprom_cap;
	bool netdev_registered;
	u32 interrupt_event;
#ifdef HAVE_ETHTOOL_SET_PHYS_ID
	u32 led_reg;
#endif

#ifdef HAVE_PTP_1588_CLOCK
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_caps;
	struct work_struct ptp_tx_work;
	struct sk_buff *ptp_tx_skb;
	unsigned long ptp_tx_start;
	unsigned long last_overflow_check;
	unsigned long last_rx_ptp_check;
	spinlock_t tmreg_lock;
	struct cyclecounter hw_cc;
	struct timecounter hw_tc;
	u32 base_incval;
	u32 tx_hwtstamp_timeouts;
	u32 rx_hwtstamp_cleared;
#endif /* HAVE_PTP_1588_CLOCK */

	DECLARE_BITMAP(active_vfs, IXGBE_MAX_VF_FUNCTIONS);
	unsigned int num_vfs;
	struct vf_data_storage *vfinfo;
	int vf_rate_link_speed;
	struct vf_macvlans vf_mvs;
	struct vf_macvlans *mv_list;
#ifdef CONFIG_PCI_IOV
	u32 timer_event_accumulator;
	u32 vferr_refcount;
#endif
	struct ixgbe_mac_addr *mac_table;
#ifdef IXGBE_SYSFS
#ifdef IXGBE_HWMON
        struct hwmon_buff ixgbe_hwmon_buff;
#endif /* IXGBE_HWMON */
#else /* IXGBE_SYSFS */
#ifdef IXGBE_PROCFS
        struct proc_dir_entry *eth_dir;
	struct proc_dir_entry *info_dir;
        struct proc_dir_entry *therm_dir[IXGBE_MAX_SENSORS];
        struct ixgbe_therm_proc_data therm_data[IXGBE_MAX_SENSORS];
#endif /* IXGBE_PROCFS */
#endif /* IXGBE_SYSFS */

#ifdef HAVE_IXGBE_DEBUG_FS
        struct dentry *ixgbe_dbg_adapter;
#endif /*HAVE_IXGBE_DEBUG_FS*/
        u8 default_up;
};
/*-------------------------------------------------------------------------*/
#endif /* __IXGBE_H__*/
