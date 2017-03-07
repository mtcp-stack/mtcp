/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/msi.h>
#include <linux/version.h>
#include <linux/rtnetlink.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "../kni/ethtool/ixgbe/ixgbe_type.h"
#include "../kni/ethtool/igb/e1000_hw.h"
#ifdef CONFIG_XEN_DOM0
#include <xen/xen.h>
#endif
#include <rte_pci_dev_features.h>
#include "compat.h"
/*----------------------------------------------------------------------------*/
/**
 * struct to hold adapter-specific parameters 
 * it currently supports Intel 1/10 Gbps adapters
 */
enum dev_type {IXGBE, IGB};
/*----------------------------------------------------------------------------*/
/* list of 1 Gbps controllers */
static struct pci_device_id e1000_pci_tbl[] = {
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82540EM)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82545EM_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82545EM_FIBER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82546EB_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82546EB_FIBER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82546EB_QUAD_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_FIBER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_SERDES)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_SERDES_DUAL)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_SERDES_QUAD)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_QUAD_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571PT_QUAD_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_QUAD_FIBER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82571EB_QUAD_COPPER_LP)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82572EI_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82572EI_FIBER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82572EI_SERDES)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82572EI)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82573L)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82574L)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82574LA)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82575EB_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82575EB_FIBER_SERDES)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_82575GB_QUAD_COPPER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_FIBER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_SERDES)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_QUAD_COPPER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_QUAD_COPPER_ET2)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_NS)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_NS_SERDES)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82576_SERDES_QUAD)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_COPPER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_FIBER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_SERDES)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_SGMII)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_COPPER_DUAL)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82580_QUAD_FIBER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_82583V)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_DH89XXCC_SGMII)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_DH89XXCC_SERDES)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_DH89XXCC_BACKPLANE)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_DH89XXCC_SFP)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_COPPER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_COPPER_OEM1)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_COPPER_IT)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_FIBER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_SERDES)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_SGMII)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_COPPER_FLASHLESS)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I210_SERDES_FLASHLESS)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I211_COPPER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_I350_COPPER)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I350_DA4)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_I350_FIBER)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_I350_SERDES)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_I350_SGMII)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I354_BACKPLANE_1GBPS)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I354_SGMII)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_I354_BACKPLANE_2_5GBPS)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_LPT_I217_LM)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_LPT_I217_V)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_LPTLP_I218_LM)},
	{PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_LPTLP_I218_V)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_I218_LM2)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_I218_V2)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_I218_LM3)},
        {PCI_VDEVICE(INTEL, E1000_DEV_ID_PCH_I218_V3)},
	/* required last entry */
	{0,}
};
/*----------------------------------------------------------------------------*/
/* list of 10 Gbps controllers */
static DEFINE_PCI_DEVICE_TABLE(ixgbe_pci_tbl) = {
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_BX)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_SINGLE_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_SFP_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_CX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_CX4_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_DA_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_XF_LR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4_MEZZ)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_COMBO_BACKPLANE)},
	{PCI_VDEVICE(INTEL, IXGBE_SUBDEV_ID_82599_KX4_KR_MEZZ)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_CX4)},
        {PCI_VDEVICE(INTEL, IXGBE_SUBDEV_ID_82599_RNDC)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_SUBDEV_ID_82599_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_SUBDEV_ID_82599_560FLR)},
        {PCI_VDEVICE(INTEL, IXGBE_SUBDEV_ID_82599_ECNA_DP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_BACKPLANE_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_SF2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_QSFP_SF_QP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599EN_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_XAUI_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_T3_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_LS)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540T1)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_SFP)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_10G_T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_1G_T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550T1)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_KR)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_KR_L)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_SFP_N)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_SGMII)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_SGMII_L)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_10G_T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_QSFP)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_QSFP_N)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_SFP)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_1G_T)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_A_1G_T_L)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_KX4)},
        {PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X550EM_X_KR)},
	/* required last entry */
	{0, }
};
/*----------------------------------------------------------------------------*/
/**
 * net adapter private struct 
 */
struct net_adapter {
	struct net_device *netdev;
	struct pci_dev *pdev;
	enum dev_type type;
	union {
		struct ixgbe_hw _ixgbe_hw;
		struct e1000_hw _e1000_hw;
	} hw;
	u16 bd_number;
	bool netdev_registered;
	struct net_device_stats nstats;
};
/*----------------------------------------------------------------------------*/
/**
 * stats struct passed on from user space to the driver
 */
struct stats_struct {
	uint64_t tx_bytes;
	uint64_t tx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_pkts;
	uint8_t qid;
	uint8_t dev;
};
/* max qid */
#define MAX_QID			16
#define MAX_DEVICES		16
/* ioctl# */
#define SEND_STATS		 0
/* major number */
#define MAJOR_NO		1110
/* dev name */
#define DEV_NAME		"dpdk-iface"
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
	int i, ifdx;
	
	adapter = netdev_priv(netdev);
	ifdx = adapter->bd_number;

	if (ifdx >= MAX_DEVICES)
		dev_info(&adapter->pdev->dev, "ifindex value: %d is greater than MAX_DEVICES!\n",
		       ifdx);

	adapter->nstats.rx_packets = adapter->nstats.tx_packets = 0;
	adapter->nstats.rx_bytes = adapter->nstats.tx_bytes = 0;
	
	for (i = 0; i < MAX_QID; i++) {
		adapter->nstats.rx_packets += sarrays[ifdx][i].rx_pkts + old_sarrays[ifdx][i].rx_pkts;
		adapter->nstats.rx_bytes += sarrays[ifdx][i].rx_bytes + old_sarrays[ifdx][i].rx_bytes;
		adapter->nstats.tx_packets += sarrays[ifdx][i].tx_pkts + old_sarrays[ifdx][i].tx_pkts;
		adapter->nstats.tx_bytes += sarrays[ifdx][i].tx_bytes + old_sarrays[ifdx][i].tx_bytes;
	}

#if 0	
	printk(KERN_ALERT "ifdx: %d, rxp: %llu, rxb: %llu, txp: %llu, txb: %llu\n",
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
/**
 *  e1000_translate_register_82542 - Translate the proper register offset
 *  @reg: e1000 register to be read
 *
 *  Registers in 82542 are located in different offsets than other adapters
 *  even though they function in the same manner.  This function takes in
 *  the name of the register to read and returns the correct offset for
 *  82542 silicon.
 **/
u32 
e1000_translate_register_82542(u32 reg)
{
	/*
	 * Some of the 82542 registers are located at different
	 * offsets than they are in newer adapters.
	 * Despite the difference in location, the registers
	 * function in the same manner.
	 */
	switch (reg) {
	case E1000_RA:
		reg = 0x00040;
		break;
	case E1000_RDTR:
		reg = 0x00108;
		break;
	case E1000_RDBAL(0):
		reg = 0x00110;
		break;
	case E1000_RDBAH(0):
		reg = 0x00114;
		break;
	case E1000_RDLEN(0):
		reg = 0x00118;
		break;
	case E1000_RDH(0):
		reg = 0x00120;
		break;
	case E1000_RDT(0):
		reg = 0x00128;
		break;
	case E1000_RDBAL(1):
		reg = 0x00138;
		break;
	case E1000_RDBAH(1):
		reg = 0x0013C;
		break;
	case E1000_RDLEN(1):
		reg = 0x00140;
		break;
	case E1000_RDH(1):
		reg = 0x00148;
		break;
	case E1000_RDT(1):
		reg = 0x00150;
		break;
	case E1000_FCRTH:
		reg = 0x00160;
		break;
	case E1000_FCRTL:
		reg = 0x00168;
		break;
	case E1000_MTA:
		reg = 0x00200;
		break;
	case E1000_TDBAL(0):
		reg = 0x00420;
		break;
	case E1000_TDBAH(0):
		reg = 0x00424;
		break;
	case E1000_TDLEN(0):
		reg = 0x00428;
		break;
	case E1000_TDH(0):
		reg = 0x00430;
		break;
	case E1000_TDT(0):
		reg = 0x00438;
		break;
	case E1000_TIDV:
		reg = 0x00440;
		break;
	case E1000_VFTA:
		reg = 0x00600;
		break;
	case E1000_TDFH:
		reg = 0x08010;
		break;
	case E1000_TDFT:
		reg = 0x08018;
		break;
	default:
		break;
	}

	return reg;
}
/*----------------------------------------------------------------------------*/
/**
 * A device specific function that retrieves mac address from each NIC interface
 */
void
retrieve_dev_addr(struct net_device *netdev, struct net_adapter *adapter)
{
	struct ixgbe_hw *hw_i;
	struct e1000_hw *hw_e;
	u32 rar_high;
	u32 rar_low;
	u16 i;

	switch (adapter->type) {
	case IXGBE:
		hw_i = &adapter->hw._ixgbe_hw;
		rar_high = IXGBE_READ_REG(hw_i, IXGBE_RAH(0));
		rar_low = IXGBE_READ_REG(hw_i, IXGBE_RAL(0));
		
		for (i = 0; i < 4; i++)
			netdev->dev_addr[i] = (u8)(rar_low >> (i*8));
		
		for (i = 0; i < 2; i++)
			netdev->dev_addr[i+4] = (u8)(rar_high >> (i*8));
		break;
	case IGB:
		hw_e = &adapter->hw._e1000_hw;
		rar_high = E1000_READ_REG(hw_e, E1000_RAH(0));
		rar_low = E1000_READ_REG(hw_e, E1000_RAL(0));
		
		for (i = 0; i < E1000_RAL_MAC_ADDR_LEN; i++)
			netdev->dev_addr[i] = (u8)(rar_low >> (i*8));

		for (i = 0; i < E1000_RAH_MAC_ADDR_LEN; i++)
			netdev->dev_addr[i+4] = (u8)(rar_high >> (i*8));
		break;
	}
}
/*----------------------------------------------------------------------------*/
/**
 * function that extracts the device type from the registers
 */
enum dev_type
retrieve_dev_specs(const struct pci_device_id *id)
{
	int i;
	enum dev_type res;
	int no_of_elements;

	res = 0xFF;
	no_of_elements = sizeof(e1000_pci_tbl)/sizeof(struct pci_device_id);
	for (i = 0; i < no_of_elements; i++) {
		if (e1000_pci_tbl[i].vendor == id->vendor &&
		    e1000_pci_tbl[i].device == id->device) {
			return IGB;
		}
			
	}

	no_of_elements = sizeof(ixgbe_pci_tbl)/sizeof(struct pci_device_id);
	for (i = 0; i < no_of_elements; i++) {
		if (ixgbe_pci_tbl[i].vendor == id->vendor &&
		    ixgbe_pci_tbl[i].device == id->device) {
			return IXGBE;
		}
			
	}

	return res;
}
/*----------------------------------------------------------------------------*/
