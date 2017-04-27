/*
 * Copyright (c) 2014-2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uio_driver.h>

#include "pv660_hns_drv.h"

#define HNS_PHY_PAGE_MDIX   0
#define HNS_PHY_PAGE_LED    3
#define HNS_PHY_PAGE_COPPER 0

#define HNS_PHY_PAGE_REG 22             /* Page Selection Reg. */
#define HNS_PHY_CSC_REG	 16             /* Copper Specific Control Register */
#define HNS_PHY_CSS_REG	 17             /* Copper Specific Status Register */
#define HNS_LED_FC_REG	 16             /* LED Function Control Reg. */
#define HNS_LED_PC_REG	 17             /* LED Polarity Control Reg. */

#define HNS_LED_FORCE_ON  9
#define HNS_LED_FORCE_OFF 8

#define HNS_CHIP_VERSION  660
#define HNS_NET_STATS_CNT 26

#define PHY_MDIX_CTRL_S	(5)
#define PHY_MDIX_CTRL_M	(3 << PHY_MDIX_CTRL_S)

#define PHY_MDIX_STATUS_B	(6)
#define PHY_SPEED_DUP_RESOLVE_B	(11)
#define SOC_NET			(1)

/**
 * hns_nic_get_drvinfo - get net driver info
 * @dev: net device
 * @drvinfo: driver info
 */
static void hns_nic_get_drvinfo(struct net_device      *net_dev,
				struct ethtool_drvinfo *drvinfo)
{
	struct nic_uio_device *priv = netdev_priv(net_dev);

	assert(priv);

	strncpy(drvinfo->version, HNAE_DRIVER_VERSION,
		sizeof(drvinfo->version));
	drvinfo->version[sizeof(drvinfo->version) - 1] = '\0';

	strncpy(drvinfo->driver, DRIVER_UIO_NAME, sizeof(drvinfo->driver));
	drvinfo->driver[sizeof(drvinfo->driver) - 1] = '\0';

	strncpy(drvinfo->bus_info, priv->dev->bus->name,
		sizeof(drvinfo->bus_info));
	drvinfo->bus_info[ETHTOOL_BUSINFO_LEN - 1] = '\0';

	strncpy(drvinfo->fw_version, "N/A", ETHTOOL_FWVERS_LEN);
	drvinfo->eedump_len = 0;
	drvinfo->reserved2[0] = SOC_NET;
}

/**
 * get_ethtool_stats - get detail statistics.
 * @dev: net device
 * @stats: statistics info.
 * @data: statistics data.
 */
void hns_get_ethtool_stats(struct net_device *netdev,
			   struct ethtool_stats *stats, u64 *data)
{
	u64 *p = data;

	p[0] = 0;
	p[1] = 2;
}

/**
 * get_strings: Return a set of strings that describe the requested objects
 * @dev: net device
 * @stats: string set ID.
 * @data: objects data.
 */
void hns_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	char *buff;

	(void)netdev;

	buff = (char *)data;

	snprintf(buff, ETH_GSTRING_LEN, "rx_packets");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_packets");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_bytes");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_bytes");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_dropped");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_dropped");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "multicast");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "collisions");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_over_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_crc_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_frame_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_fifo_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_missed_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_aborted_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_carrier_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_fifo_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_heartbeat_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_length_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_window_errors");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "rx_compressed");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "tx_compressed");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "netdev_rx_dropped");
	buff = buff + ETH_GSTRING_LEN;
	snprintf(buff, ETH_GSTRING_LEN, "netdev_tx_dropped");
	buff = buff + ETH_GSTRING_LEN;

	snprintf(buff, ETH_GSTRING_LEN, "netdev_tx_timeout");
	buff = buff + ETH_GSTRING_LEN;

	/* h->dev->ops->get_strings(h, stringset, (u8 *)buff); */
}

/**
 * nic_get_sset_count - get string set count witch returned by
   nic_get_strings.
 * @dev: net device
 * @stringset: string set index, 0: self test string; 1: statistics string.
 *
 * Return string set count.
 */
int hns_get_sset_count(struct net_device *netdev, int stringset)
{
	(void)netdev;
	(void)stringset;

	return HNS_NET_STATS_CNT;
}

static struct ethtool_ops hns_ethtool_ops = {
	.get_drvinfo	   = hns_nic_get_drvinfo,
	.get_link	   = NULL, /* hns_nic_get_link, */
	.get_settings	= NULL,    /* hns_nic_get_settings, */
	.set_settings	= NULL,    /* hns_nic_set_settings, */
	.get_ringparam	= NULL,    /* hns_get_ringparam, */
	.get_pauseparam = NULL,    /* hns_get_pauseparam, */
	.set_pauseparam = NULL,    /* hns_set_pauseparam, */
	.get_coalesce	= NULL,    /* hns_get_coalesce, */
	.set_coalesce	= NULL,    /* hns_set_coalesce, */
	.get_channels	= NULL,    /* hns_get_channels, */
	.self_test	   = NULL, /* hns_nic_self_test, */
	.get_strings	   = hns_get_strings,
	.get_sset_count	   = hns_get_sset_count,
	.get_ethtool_stats = hns_get_ethtool_stats,
	.set_phys_id  = NULL,      /* hns_set_phys_id, */
	.get_regs_len = NULL,      /* hns_get_regs_len, */
	.get_regs	   = NULL, /* hns_get_regs, */
	.nway_reset	   = NULL, /* hns_nic_nway_reset, */
};

void hns_ethtool_set_ops(struct net_device *ndev)
{
	ndev->ethtool_ops = &hns_ethtool_ops;
}
