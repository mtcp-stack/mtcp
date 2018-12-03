#ifndef __DPDK_IFACE_H__
#define __DPDK_IFACE_H__
/*--------------------------------------------------------------------------*/
#include <linux/netdevice.h>
#include "dpdk_iface_common.h"
/*--------------------------------------------------------------------------*/
#define IFACE_PREFIX		"dpdk"
/*--------------------------------------------------------------------------*/
/**
 * net adapter private struct 
 */
struct net_adapter {
	struct net_device *netdev;
	unsigned char mac_addr[ETH_ALEN];	
	u16 bd_number;
	bool netdev_registered;
	int numa_socket;
	struct net_device_stats nstats;
	struct PciAddress pa;
};
/*--------------------------------------------------------------------------*/
/**
 * stats struct passed on from user space to the driver
 */
struct stats_struct {
	uint64_t tx_bytes;
	uint64_t tx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_pkts;
	uint64_t rmiss;
	uint64_t rerr;
	uint64_t terr;	
	uint8_t qid;
	uint8_t dev;
};
/*--------------------------------------------------------------------------*/
/* sarray declaration */
extern struct stats_struct sarrays[MAX_DEVICES][MAX_QID];
extern struct stats_struct old_sarrays[MAX_DEVICES][MAX_QID];
/*----------------------------------------------------------------------------*/
/**
 * dummy function whenever a device is `opened'
 */
static int 
netdev_open(struct net_device *netdev) 
{
	(void)netdev;
	return 0;
}
/*----------------------------------------------------------------------------*/
/**
 * dummy function for retrieving net stats
 */
static struct net_device_stats *
netdev_stats(struct net_device *netdev) 
{
	struct net_adapter *adapter;
	struct stats_struct *old_sarray = NULL;
	struct stats_struct *sarray = NULL;
	int i, ifdx;
	
	adapter = netdev_priv(netdev);
	ifdx = adapter->bd_number;
	
	if (ifdx >= MAX_DEVICES)
		printk(KERN_ERR "ifindex value: %d is greater than MAX_DEVICES!\n",
		       ifdx);
	
	adapter->nstats.rx_packets = adapter->nstats.tx_packets = 0;
	adapter->nstats.rx_bytes = adapter->nstats.tx_bytes = 0;
	
	for (i = 0; i < MAX_QID; i++) {
		sarray = &sarrays[ifdx][i];
		old_sarray = &old_sarrays[ifdx][i];
		
		adapter->nstats.rx_packets += sarray->rx_pkts + old_sarray->rx_pkts;
		adapter->nstats.rx_bytes += sarray->rx_bytes + old_sarray->rx_bytes;
		adapter->nstats.tx_packets += sarray->tx_pkts + old_sarray->tx_pkts;
		adapter->nstats.tx_bytes += sarray->tx_bytes + old_sarray->tx_bytes;
		adapter->nstats.rx_missed_errors += sarray->rmiss + old_sarray->rmiss;
		adapter->nstats.rx_frame_errors += sarray->rerr + old_sarray->rerr;
		adapter->nstats.tx_errors += sarray->terr + old_sarray->terr;
	}
	
#if 0	
	printk(KERN_INFO "ifdx: %d, rxp: %llu, rxb: %llu, txp: %llu, txb: %llu\n",
	       ifdx,
	       (long long unsigned int)adapter->nstats.rx_packets,
	       (long long unsigned int)adapter->nstats.rx_bytes,
	       (long long unsigned int)adapter->nstats.tx_packets,
	       (long long unsigned int)adapter->nstats.tx_bytes);
#endif
	return &adapter->nstats;
}
/*----------------------------------------------------------------------------*/
/** 
 * dummy function for setting features
 */
static int 
netdev_set_features(struct net_device *netdev, netdev_features_t features) 
{
	(void)netdev;
	(void)features;
	return 0;
}
/*----------------------------------------------------------------------------*/
/**
 * dummy function for fixing features
 */
static netdev_features_t 
netdev_fix_features(struct net_device *netdev, netdev_features_t features) 
{
	(void)netdev;
	(void)features;
	return 0;
}
/*----------------------------------------------------------------------------*/
/**
 * dummy function that returns void
 */
static void 
netdev_no_ret(struct net_device *netdev) 
{
	(void)netdev;
	return;
}
/*----------------------------------------------------------------------------*/
/**
 * dummy tx function
 */
static int 
netdev_xmit(struct sk_buff *skb, struct net_device *netdev) {
	(void)netdev;
	(void)skb;
	return 0;
}
/*----------------------------------------------------------------------------*/
/**
 * A naive net_device_ops struct to get the interface visible to the OS
 */
static const struct net_device_ops netdev_ops = {
        .ndo_open               = netdev_open,
        .ndo_stop               = netdev_open,
        .ndo_start_xmit         = netdev_xmit,
        .ndo_set_rx_mode        = netdev_no_ret,
        .ndo_validate_addr      = netdev_open,
        .ndo_set_mac_address    = NULL,
        .ndo_change_mtu         = NULL,
        .ndo_tx_timeout         = netdev_no_ret,
        .ndo_vlan_rx_add_vid    = NULL,
        .ndo_vlan_rx_kill_vid   = NULL,
        .ndo_do_ioctl           = NULL,
        .ndo_set_vf_mac         = NULL,
        .ndo_set_vf_vlan        = NULL,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
        .ndo_set_vf_tx_rate     = NULL,
#else
        .ndo_set_vf_rate        = NULL,
#endif
        .ndo_set_vf_spoofchk    = NULL,
        .ndo_get_vf_config      = NULL,
        .ndo_get_stats          = netdev_stats,
        .ndo_setup_tc           = NULL,
#ifdef CONFIG_NET_POLL_CONTROLLER
        .ndo_poll_controller    = netdev_no_ret,
	.ndo_netpoll_setup	= NULL,
	.ndo_netpoll_cleanup	= NULL,
#endif
        .ndo_set_features 	= netdev_set_features,
        .ndo_fix_features 	= netdev_fix_features,
        .ndo_fdb_add            = NULL,
};
/*----------------------------------------------------------------------------*/
/**
 * assignment function
 */
void
netdev_assign_netdev_ops(struct net_device *dev)
{
	dev->netdev_ops = &netdev_ops;
}
/*----------------------------------------------------------------------------*/
#endif /* __DPDK_IFACE_H__ */
