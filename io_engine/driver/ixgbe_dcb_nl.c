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

#include "ixgbe.h"

#ifdef CONFIG_DCB
#include <linux/dcbnl.h>
#include "ixgbe_dcb_82598.h"
#include "ixgbe_dcb_82599.h"
#else
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#endif

/* Callbacks for DCB netlink in the kernel */
#define BIT_DCB_MODE    0x01
#define BIT_PFC         0x02
#define BIT_PG_RX       0x04
#define BIT_PG_TX       0x08
#define BIT_RESETLINK   0x40
#define BIT_LINKSPEED   0x80

/* Responses for the DCB_C_SET_ALL command */
#define DCB_HW_CHG_RST  0  /* DCB configuration changed with reset */
#define DCB_NO_HW_CHG   1  /* DCB configuration did not change */
#define DCB_HW_CHG      2  /* DCB configuration changed, no reset */

#ifndef CONFIG_DCB
/* DCB configuration commands */
enum {
	DCB_C_UNDEFINED,
	DCB_C_GSTATE,
	DCB_C_SSTATE,
	DCB_C_PG_STATS,
	DCB_C_PGTX_GCFG,
	DCB_C_PGTX_SCFG,
	DCB_C_PGRX_GCFG,
	DCB_C_PGRX_SCFG,
	DCB_C_PFC_GCFG,
	DCB_C_PFC_SCFG,
	DCB_C_PFC_STATS,
	DCB_C_GLINK_SPD,
	DCB_C_SLINK_SPD,
	DCB_C_SET_ALL,
	DCB_C_GPERM_HWADDR,
	__DCB_C_ENUM_MAX,
};

#define IXGBE_DCB_C_MAX               (__DCB_C_ENUM_MAX - 1)

/* DCB configuration attributes */
enum {
	DCB_A_UNDEFINED = 0,
	DCB_A_IFNAME,
	DCB_A_STATE,
	DCB_A_PFC_STATS,
	DCB_A_PFC_CFG,
	DCB_A_PG_STATS,
	DCB_A_PG_CFG,
	DCB_A_LINK_SPD,
	DCB_A_SET_ALL,
	DCB_A_PERM_HWADDR,
	__DCB_A_ENUM_MAX,
};

#define IXGBE_DCB_A_MAX               (__DCB_A_ENUM_MAX - 1)

/* PERM HWADDR attributes */
enum {
	PERM_HW_A_UNDEFINED,
	PERM_HW_A_0,
	PERM_HW_A_1,
	PERM_HW_A_2,
	PERM_HW_A_3,
	PERM_HW_A_4,
	PERM_HW_A_5,
	PERM_HW_A_ALL,
	__PERM_HW_A_ENUM_MAX,
};

#define IXGBE_DCB_PERM_HW_A_MAX        (__PERM_HW_A_ENUM_MAX - 1)

/* PFC configuration attributes */
enum {
	PFC_A_UP_UNDEFINED,
	PFC_A_UP_0,
	PFC_A_UP_1,
	PFC_A_UP_2,
	PFC_A_UP_3,
	PFC_A_UP_4,
	PFC_A_UP_5,
	PFC_A_UP_6,
	PFC_A_UP_7,
	PFC_A_UP_MAX, /* Used as an iterator cap */
	PFC_A_UP_ALL,
	__PFC_A_UP_ENUM_MAX,
};

#define IXGBE_DCB_PFC_A_UP_MAX        (__PFC_A_UP_ENUM_MAX - 1)

/* Priority Group Traffic Class and Bandwidth Group
 * configuration attributes
 */
enum {
	PG_A_UNDEFINED,
	PG_A_TC_0,
	PG_A_TC_1,
	PG_A_TC_2,
	PG_A_TC_3,
	PG_A_TC_4,
	PG_A_TC_5,
	PG_A_TC_6,
	PG_A_TC_7,
	PG_A_TC_MAX, /* Used as an iterator cap */
	PG_A_TC_ALL,
	PG_A_BWG_0,
	PG_A_BWG_1,
	PG_A_BWG_2,
	PG_A_BWG_3,
	PG_A_BWG_4,
	PG_A_BWG_5,
	PG_A_BWG_6,
	PG_A_BWG_7,
	PG_A_BWG_MAX, /* Used as an iterator cap */
	PG_A_BWG_ALL,
	__PG_A_ENUM_MAX,
};

#define IXGBE_DCB_PG_A_MAX     (__PG_A_ENUM_MAX - 1)

enum {
	TC_A_PARAM_UNDEFINED,
	TC_A_PARAM_STRICT_PRIO,
	TC_A_PARAM_BW_GROUP_ID,
	TC_A_PARAM_BW_PCT_IN_GROUP,
	TC_A_PARAM_UP_MAPPING,
	TC_A_PARAM_MAX, /* Used as an iterator cap */
	TC_A_PARAM_ALL,
	__TC_A_PARAM_ENUM_MAX,
};

#define IXGBE_DCB_TC_A_PARAM_MAX      (__TC_A_PARAM_ENUM_MAX - 1)

#define DCB_PROTO_VERSION             0x1
#define is_pci_device(dev) ((dev)->bus == &pci_bus_type)

static struct genl_family dcb_family = {
    .id = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name = "IXGBE_DCB",
    .version = DCB_PROTO_VERSION,
    .maxattr = IXGBE_DCB_A_MAX,
};

/* DCB NETLINK attributes policy */
static struct nla_policy dcb_genl_policy[IXGBE_DCB_A_MAX + 1] = {
	[DCB_A_IFNAME]    = {.type = NLA_STRING, .len = IFNAMSIZ - 1},
	[DCB_A_STATE]     = {.type = NLA_U8},
	[DCB_A_PG_CFG]    = {.type = NLA_NESTED},
	[DCB_A_PFC_CFG]   = {.type = NLA_NESTED},
	[DCB_A_PFC_STATS] = {.type = NLA_NESTED},
	[DCB_A_PG_STATS]  = {.type = NLA_NESTED},
	[DCB_A_LINK_SPD]  = {.type = NLA_U8},
	[DCB_A_SET_ALL]   = {.type = NLA_U8},
	[DCB_A_PERM_HWADDR] = {.type = NLA_NESTED},
};

/* DCB_A_PERM_HWADDR nested attributes... an array. */
static struct nla_policy dcb_perm_hwaddr_nest[IXGBE_DCB_PERM_HW_A_MAX + 1] = {
	[PERM_HW_A_0] = {.type = NLA_U8},
	[PERM_HW_A_1] = {.type = NLA_U8},
	[PERM_HW_A_2] = {.type = NLA_U8},
	[PERM_HW_A_3] = {.type = NLA_U8},
	[PERM_HW_A_4] = {.type = NLA_U8},
	[PERM_HW_A_5] = {.type = NLA_U8},
	[PERM_HW_A_ALL] = {.type = NLA_FLAG},
};

/* DCB_A_PFC_CFG nested attributes...like an array. */
static struct nla_policy dcb_pfc_up_nest[IXGBE_DCB_PFC_A_UP_MAX + 1] = {
	[PFC_A_UP_0]   = {.type = NLA_U8},
	[PFC_A_UP_1]   = {.type = NLA_U8},
	[PFC_A_UP_2]   = {.type = NLA_U8},
	[PFC_A_UP_3]   = {.type = NLA_U8},
	[PFC_A_UP_4]   = {.type = NLA_U8},
	[PFC_A_UP_5]   = {.type = NLA_U8},
	[PFC_A_UP_6]   = {.type = NLA_U8},
	[PFC_A_UP_7]   = {.type = NLA_U8},
	[PFC_A_UP_ALL] = {.type = NLA_FLAG},
};

/* DCB_A_PG_CFG nested attributes...like a struct. */
static struct nla_policy dcb_pg_nest[IXGBE_DCB_PG_A_MAX + 1] = {
	[PG_A_TC_0]   = {.type = NLA_NESTED},
	[PG_A_TC_1]   = {.type = NLA_NESTED},
	[PG_A_TC_2]   = {.type = NLA_NESTED},
	[PG_A_TC_3]   = {.type = NLA_NESTED},
	[PG_A_TC_4]   = {.type = NLA_NESTED},
	[PG_A_TC_5]   = {.type = NLA_NESTED},
	[PG_A_TC_6]   = {.type = NLA_NESTED},
	[PG_A_TC_7]   = {.type = NLA_NESTED},
	[PG_A_TC_ALL] = {.type = NLA_NESTED},
	[PG_A_BWG_0]  = {.type = NLA_U8},
	[PG_A_BWG_1]  = {.type = NLA_U8},
	[PG_A_BWG_2]  = {.type = NLA_U8},
	[PG_A_BWG_3]  = {.type = NLA_U8},
	[PG_A_BWG_4]  = {.type = NLA_U8},
	[PG_A_BWG_5]  = {.type = NLA_U8},
	[PG_A_BWG_6]  = {.type = NLA_U8},
	[PG_A_BWG_7]  = {.type = NLA_U8},
	[PG_A_BWG_ALL]= {.type = NLA_FLAG},
};

/* TC_A_CLASS_X nested attributes. */
static struct nla_policy dcb_tc_param_nest[IXGBE_DCB_TC_A_PARAM_MAX + 1] = {
	[TC_A_PARAM_STRICT_PRIO]     = {.type = NLA_U8},
	[TC_A_PARAM_BW_GROUP_ID]     = {.type = NLA_U8},
	[TC_A_PARAM_BW_PCT_IN_GROUP] = {.type = NLA_U8},
	[TC_A_PARAM_UP_MAPPING]      = {.type = NLA_U8},
	[TC_A_PARAM_ALL]             = {.type = NLA_FLAG},
};

static int ixgbe_dcb_check_adapter(struct net_device *netdev)
{
	struct device *busdev;
	struct pci_dev *pcidev;

	busdev = netdev->dev.parent;
	if (!busdev)
		return -EINVAL;

	if (!is_pci_device(busdev))
		return -EINVAL;

	pcidev = to_pci_dev(busdev);
	if (!pcidev)
		return -EINVAL;

	if (ixgbe_is_ixgbe(pcidev))
		return 0;
	else
		return -EINVAL;
}
#endif

#ifdef CONFIG_DCB
int ixgbe_copy_dcb_cfg(struct ixgbe_dcb_config *src_dcb_cfg,
		       struct ixgbe_dcb_config *dst_dcb_cfg, int tc_max)
{
	struct tc_configuration *src_tc_cfg = NULL;
	struct tc_configuration *dst_tc_cfg = NULL;
	int i;

	if (!src_dcb_cfg || !dst_dcb_cfg)
		return -EINVAL;

	for (i = DCB_PG_ATTR_TC_0; i < tc_max + DCB_PG_ATTR_TC_0; i++) {
		src_tc_cfg = &src_dcb_cfg->tc_config[i - DCB_PG_ATTR_TC_0];
		dst_tc_cfg = &dst_dcb_cfg->tc_config[i - DCB_PG_ATTR_TC_0];

		dst_tc_cfg->path[DCB_TX_CONFIG].prio_type =
				src_tc_cfg->path[DCB_TX_CONFIG].prio_type;

		dst_tc_cfg->path[DCB_TX_CONFIG].bwg_id =
				src_tc_cfg->path[DCB_TX_CONFIG].bwg_id;

		dst_tc_cfg->path[DCB_TX_CONFIG].bwg_percent =
				src_tc_cfg->path[DCB_TX_CONFIG].bwg_percent;

		dst_tc_cfg->path[DCB_TX_CONFIG].up_to_tc_bitmap =
				src_tc_cfg->path[DCB_TX_CONFIG].up_to_tc_bitmap;

		dst_tc_cfg->path[DCB_RX_CONFIG].prio_type =
				src_tc_cfg->path[DCB_RX_CONFIG].prio_type;

		dst_tc_cfg->path[DCB_RX_CONFIG].bwg_id =
				src_tc_cfg->path[DCB_RX_CONFIG].bwg_id;

		dst_tc_cfg->path[DCB_RX_CONFIG].bwg_percent =
				src_tc_cfg->path[DCB_RX_CONFIG].bwg_percent;

		dst_tc_cfg->path[DCB_RX_CONFIG].up_to_tc_bitmap =
				src_tc_cfg->path[DCB_RX_CONFIG].up_to_tc_bitmap;
	}

	for (i = DCB_PG_ATTR_BW_ID_0; i < DCB_PG_ATTR_BW_ID_MAX; i++) {
		dst_dcb_cfg->bw_percentage[DCB_TX_CONFIG]
			[i-DCB_PG_ATTR_BW_ID_0] = src_dcb_cfg->bw_percentage
				[DCB_TX_CONFIG][i-DCB_PG_ATTR_BW_ID_0];
		dst_dcb_cfg->bw_percentage[DCB_RX_CONFIG]
			[i-DCB_PG_ATTR_BW_ID_0] = src_dcb_cfg->bw_percentage
				[DCB_RX_CONFIG][i-DCB_PG_ATTR_BW_ID_0];
	}

	for (i = DCB_PFC_UP_ATTR_0; i < DCB_PFC_UP_ATTR_MAX; i++) {
		dst_dcb_cfg->tc_config[i - DCB_PFC_UP_ATTR_0].dcb_pfc =
			src_dcb_cfg->tc_config[i - DCB_PFC_UP_ATTR_0].dcb_pfc;
	}
	dst_dcb_cfg->pfc_mode_enable = src_dcb_cfg->pfc_mode_enable;

	return 0;
}
#else
static int ixgbe_copy_dcb_cfg(struct ixgbe_dcb_config *src_dcb_cfg,
			      struct ixgbe_dcb_config *dst_dcb_cfg, int tc_max)
{
	struct tc_configuration *src_tc_cfg = NULL;
	struct tc_configuration *dst_tc_cfg = NULL;
	int i;

	if (!src_dcb_cfg || !dst_dcb_cfg)
		return -EINVAL;

	dst_dcb_cfg->link_speed = src_dcb_cfg->link_speed;

	for (i = PG_A_TC_0; i < tc_max + PG_A_TC_0; i++) {
		src_tc_cfg = &src_dcb_cfg->tc_config[i - PG_A_TC_0];
		dst_tc_cfg = &dst_dcb_cfg->tc_config[i - PG_A_TC_0];

		dst_tc_cfg->path[DCB_TX_CONFIG].prio_type =
				src_tc_cfg->path[DCB_TX_CONFIG].prio_type;

		dst_tc_cfg->path[DCB_TX_CONFIG].bwg_id =
				src_tc_cfg->path[DCB_TX_CONFIG].bwg_id;

		dst_tc_cfg->path[DCB_TX_CONFIG].bwg_percent =
				src_tc_cfg->path[DCB_TX_CONFIG].bwg_percent;

		dst_tc_cfg->path[DCB_TX_CONFIG].up_to_tc_bitmap =
				src_tc_cfg->path[DCB_TX_CONFIG].up_to_tc_bitmap;

		dst_tc_cfg->path[DCB_RX_CONFIG].prio_type =
				src_tc_cfg->path[DCB_RX_CONFIG].prio_type;

		dst_tc_cfg->path[DCB_RX_CONFIG].bwg_id =
				src_tc_cfg->path[DCB_RX_CONFIG].bwg_id;

		dst_tc_cfg->path[DCB_RX_CONFIG].bwg_percent =
				src_tc_cfg->path[DCB_RX_CONFIG].bwg_percent;

		dst_tc_cfg->path[DCB_RX_CONFIG].up_to_tc_bitmap =
				src_tc_cfg->path[DCB_RX_CONFIG].up_to_tc_bitmap;
	}

	for (i = PG_A_BWG_0; i < PG_A_BWG_MAX; i++) {
		dst_dcb_cfg->bw_percentage[DCB_TX_CONFIG][i - PG_A_BWG_0] =
		    src_dcb_cfg->bw_percentage[DCB_TX_CONFIG][i - PG_A_BWG_0];
		dst_dcb_cfg->bw_percentage[DCB_RX_CONFIG][i - PG_A_BWG_0] =
	            src_dcb_cfg->bw_percentage[DCB_RX_CONFIG][i - PG_A_BWG_0];
	}

	for (i = PFC_A_UP_0; i < PFC_A_UP_MAX; i++) {
		dst_dcb_cfg->tc_config[i - PFC_A_UP_0].dcb_pfc =
			src_dcb_cfg->tc_config[i - PFC_A_UP_0].dcb_pfc;
	}

	return 0;
}

static int ixgbe_nl_reply(u8 value, u8 cmd, u8 attr, struct genl_info *info)
{
	struct sk_buff *dcb_skb = NULL;
	void *data;
	int ret;

	dcb_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!dcb_skb)
		return -EINVAL;

	data =  genlmsg_put_reply(dcb_skb, info, &dcb_family, 0, cmd);
	if (!data)
		goto err;

	ret = nla_put_u8(dcb_skb, attr, value);
	if (ret)
        	goto err;

	/* end the message, assign the nlmsg_len. */
	genlmsg_end(dcb_skb, data);
	ret = genlmsg_reply(dcb_skb, info);
	if (ret)
        	goto err;

	return 0;

err:
	kfree(dcb_skb);
	return -EINVAL;
}
#endif

#ifdef CONFIG_DCB
static u8 ixgbe_dcbnl_get_state(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	return !!(adapter->flags & IXGBE_FLAG_DCB_ENABLED);
}

static u8 ixgbe_dcbnl_set_state(struct net_device *netdev, u8 state)
{
	u8 err = 0;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (state > 0) {
		/* Turn on DCB */
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
			goto out;

		if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED)) {
			DPRINTK(DRV, ERR, "Enable failed, needs MSI-X\n");
			err = 1;
			goto out;
		}

		if (netif_running(netdev))
#ifdef HAVE_NET_DEVICE_OPS
			netdev->netdev_ops->ndo_stop(netdev);
#else
			netdev->stop(netdev);
#endif
		ixgbe_clear_interrupt_scheme(adapter);
		if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
			adapter->last_lfc_mode = adapter->hw.fc.current_mode;
			adapter->hw.fc.requested_mode = ixgbe_fc_none;
		}
		adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
		if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
			DPRINTK(DRV, INFO, "DCB enabled, "
			        "disabling Flow Director\n");
			adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
			adapter->flags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
		}
		adapter->flags |= IXGBE_FLAG_DCB_ENABLED;
		ixgbe_init_interrupt_scheme(adapter);
		if (netif_running(netdev))
#ifdef HAVE_NET_DEVICE_OPS
			netdev->netdev_ops->ndo_open(netdev);
#else
			netdev->open(netdev);
#endif
	} else {
		/* Turn off DCB */
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
			if (netif_running(netdev))
#ifdef HAVE_NET_DEVICE_OPS
				netdev->netdev_ops->ndo_stop(netdev);
#else
				netdev->stop(netdev);
#endif
			ixgbe_clear_interrupt_scheme(adapter);
			adapter->hw.fc.requested_mode = adapter->last_lfc_mode;
			adapter->temp_dcb_cfg.pfc_mode_enable = false;
			adapter->dcb_cfg.pfc_mode_enable = false;
			adapter->flags &= ~IXGBE_FLAG_DCB_ENABLED;
			adapter->flags |= IXGBE_FLAG_RSS_ENABLED;
			if (adapter->hw.mac.type == ixgbe_mac_82599EB)
				adapter->flags |= IXGBE_FLAG_FDIR_HASH_CAPABLE;
			ixgbe_init_interrupt_scheme(adapter);
			if (netif_running(netdev))
#ifdef HAVE_NET_DEVICE_OPS
				netdev->netdev_ops->ndo_open(netdev);
#else
				netdev->open(netdev);
#endif
		}
	}
out:
	return err;
}
#else
static int ixgbe_dcb_gstate(struct sk_buff *skb, struct genl_info *info)
{
	int ret = -ENOMEM;
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;

	if (!info->attrs[DCB_A_IFNAME])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	ret = ixgbe_nl_reply(!!(adapter->flags & IXGBE_FLAG_DCB_ENABLED),
				DCB_C_GSTATE, DCB_A_STATE, info);
	if (ret)
		goto err_out;

err_out:
	dev_put(netdev);
	return ret;
}

static int ixgbe_dcb_sstate(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	int ret = -EINVAL;
	u8 value;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_STATE])
		goto err;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		goto err;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	value = nla_get_u8(info->attrs[DCB_A_STATE]);
	if ((value & 1) != value) {
		DPRINTK(DRV, ERR, "Value is not 1 or 0, it is %d.\n", value);
	} else {
		switch (value) {
		case 0:
			if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
				if (netdev->flags & IFF_UP)
#ifdef HAVE_NET_DEVICE_OPS
					netdev->netdev_ops->ndo_stop(netdev);
#else
					netdev->stop(netdev);
#endif
				ixgbe_clear_interrupt_scheme(adapter);

				adapter->flags &= ~IXGBE_FLAG_DCB_ENABLED;
				if (adapter->flags & IXGBE_FLAG_RSS_CAPABLE)
					adapter->flags |=
					                 IXGBE_FLAG_RSS_ENABLED;
				ixgbe_init_interrupt_scheme(adapter);
				ixgbe_reset(adapter);
				if (netdev->flags & IFF_UP)
#ifdef HAVE_NET_DEVICE_OPS
					netdev->netdev_ops->ndo_open(netdev);
#else
					netdev->open(netdev);
#endif
				break;
			} else {
				/* Nothing to do, already off */
				goto out;
			}
		case 1:
			if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
				/* Nothing to do, already on */
				goto out;
			} else if (!(adapter->flags & IXGBE_FLAG_DCB_CAPABLE)) {
				DPRINTK(DRV, ERR, "Enable failed.  Make sure "
				        "the driver can enable MSI-X.\n");
				ret = -EINVAL;
				goto err_out;
			} else {
				if (netdev->flags & IFF_UP)
#ifdef HAVE_NET_DEVICE_OPS
					netdev->netdev_ops->ndo_stop(netdev);
#else
					netdev->stop(netdev);
#endif
				ixgbe_clear_interrupt_scheme(adapter);

				adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
				adapter->flags |= IXGBE_FLAG_DCB_ENABLED;
				adapter->dcb_cfg.support.capabilities =
				 (IXGBE_DCB_PG_SUPPORT | IXGBE_DCB_PFC_SUPPORT |
				  IXGBE_DCB_GSP_SUPPORT);
				if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
					DPRINTK(DRV, INFO, "DCB enabled, "
					        "disabling Flow Director\n");
					adapter->flags &=
					          ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
					adapter->flags &=
					       ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
					adapter->dcb_cfg.support.capabilities |=
					                IXGBE_DCB_UP2TC_SUPPORT;
				}
				adapter->ring_feature[RING_F_DCB].indices = 8;
				ixgbe_init_interrupt_scheme(adapter);
				ixgbe_reset(adapter);
				if (netdev->flags & IFF_UP)
#ifdef HAVE_NET_DEVICE_OPS
					netdev->netdev_ops->ndo_open(netdev);
#else
					netdev->open(netdev);
#endif
				break;
			}
		}
	}

out:
	ret = ixgbe_nl_reply(0, DCB_C_SSTATE, DCB_A_STATE, info);
	if (ret)
		goto err_out;

err_out:
	dev_put(netdev);
err:
	return ret;
}

static int ixgbe_dcb_glink_spd(struct sk_buff *skb, struct genl_info *info)
{
	int ret = -ENOMEM;
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;

	if (!info->attrs[DCB_A_IFNAME])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	ret = ixgbe_nl_reply(adapter->dcb_cfg.link_speed & 0xff,
				DCB_C_GLINK_SPD, DCB_A_LINK_SPD, info);
	if (ret)
		goto err_out;

err_out:
	dev_put(netdev);
	return ret;
}

static int ixgbe_dcb_slink_spd(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	int ret = -EINVAL;
	u8 value;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_LINK_SPD])
		goto err;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		goto err;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	value = nla_get_u8(info->attrs[DCB_A_LINK_SPD]);
	if (value > 9) {
		DPRINTK(DRV, ERR, "Value is not 0 thru 9, it is %d.\n", value);
	} else {
		if (!adapter->dcb_set_bitmap &&
		   ixgbe_copy_dcb_cfg(&adapter->dcb_cfg, &adapter->temp_dcb_cfg,
				adapter->ring_feature[RING_F_DCB].indices)) {
			ret = -EINVAL;
			goto err_out;
		}

		adapter->temp_dcb_cfg.link_speed = value;
		adapter->dcb_set_bitmap |= BIT_LINKSPEED;
	}

	ret = ixgbe_nl_reply(0, DCB_C_SLINK_SPD, DCB_A_LINK_SPD, info);
	if (ret)
		goto err_out;

err_out:
	dev_put(netdev);
err:
	return ret;
}
#endif

#ifdef CONFIG_DCB
static void ixgbe_dcbnl_get_perm_hw_addr(struct net_device *netdev,
					 u8 *perm_addr)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i, j;

	memset(perm_addr, 0xff, MAX_ADDR_LEN);

	for (i = 0; i < netdev->addr_len; i++)
		perm_addr[i] = adapter->hw.mac.perm_addr[i];

	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		for (j = 0; j < netdev->addr_len; j++, i++)
			perm_addr[i] = adapter->hw.mac.san_addr[j];
	}
}
#else
static int ixgbe_dcb_gperm_hwaddr(struct sk_buff *skb, struct genl_info *info)
{
	void *data;
	struct sk_buff *dcb_skb = NULL;
	struct nlattr *tb[IXGBE_DCB_PERM_HW_A_MAX + 1], *nest;
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	struct ixgbe_hw *hw = NULL;
	int ret = -ENOMEM;
	int i;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_PERM_HWADDR])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	hw = &adapter->hw;

	ret = nla_parse_nested(tb, IXGBE_DCB_PERM_HW_A_MAX,
				info->attrs[DCB_A_PERM_HWADDR],
				dcb_perm_hwaddr_nest);
	if (ret)
		goto err;

	dcb_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!dcb_skb)
		goto err;

	data =  genlmsg_put_reply(dcb_skb, info, &dcb_family, 0,
				  DCB_C_GPERM_HWADDR);
	if (!data)
		goto err;

	nest = nla_nest_start(dcb_skb, DCB_A_PERM_HWADDR);
	if (!nest)
		goto err;

	for (i = 0; i < netdev->addr_len; i++) {
		if (!tb[i+PERM_HW_A_0] && !tb[PERM_HW_A_ALL])
			goto err;

		ret = nla_put_u8(dcb_skb, DCB_A_PERM_HWADDR,
				 hw->mac.perm_addr[i]);

		if (ret) {
			nla_nest_cancel(dcb_skb, nest);
			goto err;
		}
	}

	nla_nest_end(dcb_skb, nest);

	genlmsg_end(dcb_skb, data);

	ret = genlmsg_reply(dcb_skb, info);
	if (ret)
		goto err;

	dev_put(netdev);
	return 0;

err:
	DPRINTK(DRV, ERR, "Error in get permanent hwaddr.\n");
	kfree(dcb_skb);
err_out:
	dev_put(netdev);
	return ret;
}
#endif

#ifdef CONFIG_DCB
static void ixgbe_dcbnl_set_pg_tc_cfg_tx(struct net_device *netdev, int tc,
					 u8 prio, u8 bwg_id, u8 bw_pct,
					 u8 up_map)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (prio != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[0].prio_type = prio;
	if (bwg_id != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[0].bwg_id = bwg_id;
	if (bw_pct != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[0].bwg_percent =
			bw_pct;
	if (up_map != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[0].up_to_tc_bitmap =
			up_map;

	if ((adapter->temp_dcb_cfg.tc_config[tc].path[0].prio_type !=
	     adapter->dcb_cfg.tc_config[tc].path[0].prio_type) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[0].bwg_id !=
	     adapter->dcb_cfg.tc_config[tc].path[0].bwg_id) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[0].bwg_percent !=
	     adapter->dcb_cfg.tc_config[tc].path[0].bwg_percent) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[0].up_to_tc_bitmap !=
	     adapter->dcb_cfg.tc_config[tc].path[0].up_to_tc_bitmap)) {
		adapter->dcb_set_bitmap |= BIT_PG_TX;
		adapter->dcb_set_bitmap |= BIT_RESETLINK;
	}
}

static void ixgbe_dcbnl_set_pg_bwg_cfg_tx(struct net_device *netdev, int bwg_id,
					  u8 bw_pct)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	adapter->temp_dcb_cfg.bw_percentage[0][bwg_id] = bw_pct;

	if (adapter->temp_dcb_cfg.bw_percentage[0][bwg_id] !=
	    adapter->dcb_cfg.bw_percentage[0][bwg_id]) {
		adapter->dcb_set_bitmap |= BIT_PG_RX;
		adapter->dcb_set_bitmap |= BIT_RESETLINK;
	}
}

static void ixgbe_dcbnl_set_pg_tc_cfg_rx(struct net_device *netdev, int tc,
					 u8 prio, u8 bwg_id, u8 bw_pct,
					 u8 up_map)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (prio != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[1].prio_type = prio;
	if (bwg_id != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[1].bwg_id = bwg_id;
	if (bw_pct != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[1].bwg_percent =
			bw_pct;
	if (up_map != DCB_ATTR_VALUE_UNDEFINED)
		adapter->temp_dcb_cfg.tc_config[tc].path[1].up_to_tc_bitmap =
			up_map;

	if ((adapter->temp_dcb_cfg.tc_config[tc].path[1].prio_type !=
	     adapter->dcb_cfg.tc_config[tc].path[1].prio_type) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[1].bwg_id !=
	     adapter->dcb_cfg.tc_config[tc].path[1].bwg_id) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[1].bwg_percent !=
	     adapter->dcb_cfg.tc_config[tc].path[1].bwg_percent) ||
	    (adapter->temp_dcb_cfg.tc_config[tc].path[1].up_to_tc_bitmap !=
	     adapter->dcb_cfg.tc_config[tc].path[1].up_to_tc_bitmap)) {
		adapter->dcb_set_bitmap |= BIT_PG_RX;
		adapter->dcb_set_bitmap |= BIT_RESETLINK;
	}
}

static void ixgbe_dcbnl_set_pg_bwg_cfg_rx(struct net_device *netdev, int bwg_id,
					  u8 bw_pct)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	adapter->temp_dcb_cfg.bw_percentage[1][bwg_id] = bw_pct;

	if (adapter->temp_dcb_cfg.bw_percentage[1][bwg_id] !=
	    adapter->dcb_cfg.bw_percentage[1][bwg_id]) {
		adapter->dcb_set_bitmap |= BIT_PG_RX;
		adapter->dcb_set_bitmap |= BIT_RESETLINK;
	}
}

static void ixgbe_dcbnl_get_pg_tc_cfg_tx(struct net_device *netdev, int tc,
					 u8 *prio, u8 *bwg_id, u8 *bw_pct,
					 u8 *up_map)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	*prio = adapter->dcb_cfg.tc_config[tc].path[0].prio_type;
	*bwg_id = adapter->dcb_cfg.tc_config[tc].path[0].bwg_id;
	*bw_pct = adapter->dcb_cfg.tc_config[tc].path[0].bwg_percent;
	*up_map = adapter->dcb_cfg.tc_config[tc].path[0].up_to_tc_bitmap;
}

static void ixgbe_dcbnl_get_pg_bwg_cfg_tx(struct net_device *netdev, int bwg_id,
					  u8 *bw_pct)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	*bw_pct = adapter->dcb_cfg.bw_percentage[0][bwg_id];
}

static void ixgbe_dcbnl_get_pg_tc_cfg_rx(struct net_device *netdev, int tc,
					 u8 *prio, u8 *bwg_id, u8 *bw_pct,
					 u8 *up_map)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	*prio = adapter->dcb_cfg.tc_config[tc].path[1].prio_type;
	*bwg_id = adapter->dcb_cfg.tc_config[tc].path[1].bwg_id;
	*bw_pct = adapter->dcb_cfg.tc_config[tc].path[1].bwg_percent;
	*up_map = adapter->dcb_cfg.tc_config[tc].path[1].up_to_tc_bitmap;
}

static void ixgbe_dcbnl_get_pg_bwg_cfg_rx(struct net_device *netdev, int bwg_id,
					  u8 *bw_pct)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	*bw_pct = adapter->dcb_cfg.bw_percentage[1][bwg_id];
}
#else
static int ixgbe_dcb_pg_scfg(struct sk_buff *skb, struct genl_info *info,
				int dir)
{
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	struct tc_configuration *tc_config = NULL;
	struct tc_configuration *tc_tmpcfg = NULL;
	struct nlattr *pg_tb[IXGBE_DCB_PG_A_MAX + 1];
	struct nlattr *param_tb[IXGBE_DCB_TC_A_PARAM_MAX + 1];
	int i, ret, tc_max;
	u8 value;
	u8 changed = 0;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_PG_CFG])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err;
	else
		adapter = netdev_priv(netdev);

	ret = nla_parse_nested(pg_tb, IXGBE_DCB_PG_A_MAX,
			       info->attrs[DCB_A_PG_CFG], dcb_pg_nest);
	if (ret)
		goto err;

	if (!adapter->dcb_set_bitmap &&
	    ixgbe_copy_dcb_cfg(&adapter->dcb_cfg, &adapter->temp_dcb_cfg,
			       adapter->ring_feature[RING_F_DCB].indices))
		goto err;

	tc_max = adapter->ring_feature[RING_F_DCB].indices;
	for (i = PG_A_TC_0; i < tc_max + PG_A_TC_0; i++) {
		if (!pg_tb[i])
			continue;

		ret = nla_parse_nested(param_tb, IXGBE_DCB_TC_A_PARAM_MAX,
				       pg_tb[i], dcb_tc_param_nest);
		if (ret)
			goto err;

		tc_config = &adapter->dcb_cfg.tc_config[i - PG_A_TC_0];
		tc_tmpcfg = &adapter->temp_dcb_cfg.tc_config[i - PG_A_TC_0];
		if (param_tb[TC_A_PARAM_STRICT_PRIO]) {
			value = nla_get_u8(param_tb[TC_A_PARAM_STRICT_PRIO]);
			tc_tmpcfg->path[dir].prio_type = value;
			if (tc_tmpcfg->path[dir].prio_type !=
				tc_config->path[dir].prio_type)
				changed = 1;
		}
		if (param_tb[TC_A_PARAM_BW_GROUP_ID]) {
			value = nla_get_u8(param_tb[TC_A_PARAM_BW_GROUP_ID]);
			tc_tmpcfg->path[dir].bwg_id = value;
			if (tc_tmpcfg->path[dir].bwg_id !=
				tc_config->path[dir].bwg_id)
				changed = 1;
		}
		if (param_tb[TC_A_PARAM_BW_PCT_IN_GROUP]) {
			value = nla_get_u8(param_tb[TC_A_PARAM_BW_PCT_IN_GROUP]);
			tc_tmpcfg->path[dir].bwg_percent = value;
			if (tc_tmpcfg->path[dir].bwg_percent !=
				tc_config->path[dir].bwg_percent)
				changed = 1;
		}
		if (param_tb[TC_A_PARAM_UP_MAPPING]) {
			value = nla_get_u8(param_tb[TC_A_PARAM_UP_MAPPING]);
			tc_tmpcfg->path[dir].up_to_tc_bitmap = value;
			if (tc_tmpcfg->path[dir].up_to_tc_bitmap !=
				tc_config->path[dir].up_to_tc_bitmap)
				changed = 1;
		}
	}

	for (i = PG_A_BWG_0; i < PG_A_BWG_MAX; i++) {
		if (!pg_tb[i])
			continue;

		value = nla_get_u8(pg_tb[i]);
		adapter->temp_dcb_cfg.bw_percentage[dir][i-PG_A_BWG_0] = value;

		if (adapter->temp_dcb_cfg.bw_percentage[dir][i-PG_A_BWG_0] !=
			adapter->dcb_cfg.bw_percentage[dir][i-PG_A_BWG_0])
			changed = 1;
	}

	adapter->temp_dcb_cfg.round_robin_enable = false;

	if (changed) {
		if (dir == DCB_TX_CONFIG)
			adapter->dcb_set_bitmap |= BIT_PG_TX;
		else
			adapter->dcb_set_bitmap |= BIT_PG_RX;

		adapter->dcb_set_bitmap |= BIT_RESETLINK;
	}

	ret = ixgbe_nl_reply(0, (dir? DCB_C_PGRX_SCFG : DCB_C_PGTX_SCFG),
			     DCB_A_PG_CFG, info);
	if (ret)
		goto err;

err:
	dev_put(netdev);
	return ret;
}

static int ixgbe_dcb_pgtx_scfg(struct sk_buff *skb, struct genl_info *info)
{
	return ixgbe_dcb_pg_scfg(skb, info, DCB_TX_CONFIG);
}

static int ixgbe_dcb_pgrx_scfg(struct sk_buff *skb, struct genl_info *info)
{
	return ixgbe_dcb_pg_scfg(skb, info, DCB_RX_CONFIG);
}

static int ixgbe_dcb_pg_gcfg(struct sk_buff *skb, struct genl_info *info,
				int dir)
{
	void *data;
	struct sk_buff *dcb_skb = NULL;
	struct nlattr *pg_nest, *param_nest, *tb;
	struct nlattr *pg_tb[IXGBE_DCB_PG_A_MAX + 1];
	struct nlattr *param_tb[IXGBE_DCB_TC_A_PARAM_MAX + 1];
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	struct tc_configuration *tc_config = NULL;
	struct tc_bw_alloc *tc = NULL;
	int ret  = -ENOMEM;
	int i, tc_max;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_PG_CFG])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	ret = nla_parse_nested(pg_tb, IXGBE_DCB_PG_A_MAX,
			       info->attrs[DCB_A_PG_CFG], dcb_pg_nest);
	if (ret)
		goto err;

	dcb_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!dcb_skb)
		goto err;

	data =  genlmsg_put_reply(dcb_skb, info, &dcb_family, 0,
				 (dir) ? DCB_C_PGRX_GCFG : DCB_C_PGTX_GCFG);

	if (!data)
		goto err;

	pg_nest = nla_nest_start(dcb_skb, DCB_A_PG_CFG);
	if (!pg_nest)
		goto err;

	tc_max = adapter->ring_feature[RING_F_DCB].indices;
	for (i = PG_A_TC_0; i < tc_max + PG_A_TC_0; i++) {
		if (!pg_tb[i] && !pg_tb[PG_A_TC_ALL])
			continue;

		if (pg_tb[PG_A_TC_ALL])
			tb = pg_tb[PG_A_TC_ALL];
		else
			tb = pg_tb[i];
		ret = nla_parse_nested(param_tb, IXGBE_DCB_TC_A_PARAM_MAX,
				       tb, dcb_tc_param_nest);
		if (ret)
			goto err_pg;

		param_nest = nla_nest_start(dcb_skb, i);
		if (!param_nest)
			goto err_pg;

		tc_config = &adapter->dcb_cfg.tc_config[i - PG_A_TC_0];
		tc = &adapter->dcb_cfg.tc_config[i - PG_A_TC_0].path[dir];

		if (param_tb[TC_A_PARAM_STRICT_PRIO] ||
		    param_tb[TC_A_PARAM_ALL]) {
			ret = nla_put_u8(dcb_skb, TC_A_PARAM_STRICT_PRIO,
					 tc->prio_type);
			if (ret)
				goto err_param;
		}
		if (param_tb[TC_A_PARAM_BW_GROUP_ID] ||
		    param_tb[TC_A_PARAM_ALL]) {
			ret = nla_put_u8(dcb_skb, TC_A_PARAM_BW_GROUP_ID,
					 tc->bwg_id);
			if (ret)
				goto err_param;
		}
		if (param_tb[TC_A_PARAM_BW_PCT_IN_GROUP] ||
		    param_tb[TC_A_PARAM_ALL]) {
			ret = nla_put_u8(dcb_skb, TC_A_PARAM_BW_PCT_IN_GROUP,
					 tc->bwg_percent);
			if (ret)
				goto err_param;
		}
		if (param_tb[TC_A_PARAM_UP_MAPPING] ||
		    param_tb[TC_A_PARAM_ALL]) {
			ret = nla_put_u8(dcb_skb, TC_A_PARAM_UP_MAPPING,
					 tc->up_to_tc_bitmap);
			if (ret)
				goto err_param;
		}
		nla_nest_end(dcb_skb, param_nest);
	}

	for (i = PG_A_BWG_0; i < PG_A_BWG_MAX; i++) {
		if (!pg_tb[i] && !pg_tb[PG_A_BWG_ALL])
			continue;

		ret = nla_put_u8(dcb_skb, i,
		            adapter->dcb_cfg.bw_percentage[dir][i-PG_A_BWG_0]);

		if (ret)
			goto err_pg;
	}

	nla_nest_end(dcb_skb, pg_nest);

	genlmsg_end(dcb_skb, data);
	ret = genlmsg_reply(dcb_skb, info);
	if (ret)
		goto err;

	dev_put(netdev);
	return 0;

err_param:
	DPRINTK(DRV, ERR, "Error in get pg %s.\n", dir?"rx":"tx");
	nla_nest_cancel(dcb_skb, param_nest);
err_pg:
	nla_nest_cancel(dcb_skb, pg_nest);
err:
	kfree(dcb_skb);
err_out:
	dev_put(netdev);
	return ret;
}

static int ixgbe_dcb_pgtx_gcfg(struct sk_buff *skb, struct genl_info *info)
{
	return ixgbe_dcb_pg_gcfg(skb, info, DCB_TX_CONFIG);
}

static int ixgbe_dcb_pgrx_gcfg(struct sk_buff *skb, struct genl_info *info)
{
	return ixgbe_dcb_pg_gcfg(skb, info, DCB_RX_CONFIG);
}
#endif

#ifdef CONFIG_DCB
static void ixgbe_dcbnl_set_pfc_cfg(struct net_device *netdev, int priority,
				    u8 setting)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	adapter->temp_dcb_cfg.tc_config[priority].dcb_pfc = setting;
	if (adapter->temp_dcb_cfg.tc_config[priority].dcb_pfc !=
	    adapter->dcb_cfg.tc_config[priority].dcb_pfc) {
		adapter->dcb_set_bitmap |= BIT_PFC;
	}
}

static void ixgbe_dcbnl_get_pfc_cfg(struct net_device *netdev, int priority,
				    u8 *setting)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	*setting = adapter->dcb_cfg.tc_config[priority].dcb_pfc;
}
#else
static int ixgbe_dcb_spfccfg(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr *tb[IXGBE_DCB_PFC_A_UP_MAX + 1];
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	int i, ret = -ENOMEM;
	u8 setting;
	u8 changed = 0;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	adapter = netdev_priv(netdev);

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_PFC_CFG])
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err;
	else
		adapter = netdev_priv(netdev);

	ret = nla_parse_nested(tb, IXGBE_DCB_PFC_A_UP_MAX,
		               info->attrs[DCB_A_PFC_CFG],
		               dcb_pfc_up_nest);
	if (ret)
		goto err;

	if (!adapter->dcb_set_bitmap &&
	    ixgbe_copy_dcb_cfg(&adapter->dcb_cfg, &adapter->temp_dcb_cfg,
			       adapter->ring_feature[RING_F_DCB].indices)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = PFC_A_UP_0; i < PFC_A_UP_MAX; i++) {
		if (!tb[i])
			continue;

		setting = nla_get_u8(tb[i]);
		adapter->temp_dcb_cfg.tc_config[i-PFC_A_UP_0].dcb_pfc = setting;

		if (adapter->temp_dcb_cfg.tc_config[i-PFC_A_UP_0].dcb_pfc !=
			adapter->dcb_cfg.tc_config[i-PFC_A_UP_0].dcb_pfc)
			changed = 1;
	}

	if (changed)
		adapter->dcb_set_bitmap |= BIT_PFC;

	ret = ixgbe_nl_reply(0, DCB_C_PFC_SCFG, DCB_A_PFC_CFG, info);
	if (ret)
		goto err;

err:
	dev_put(netdev);
	return ret;
}

static int ixgbe_dcb_gpfccfg(struct sk_buff *skb, struct genl_info *info)
{
	void *data;
	struct sk_buff *dcb_skb = NULL;
	struct nlattr *tb[IXGBE_DCB_PFC_A_UP_MAX + 1], *nest;
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	int ret = -ENOMEM;
	int i;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_PFC_CFG])
		return -EINVAL;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		return -EINVAL;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	ret = nla_parse_nested(tb, IXGBE_DCB_PFC_A_UP_MAX,
			       info->attrs[DCB_A_PFC_CFG], dcb_pfc_up_nest);
	if (ret)
		goto err;

	dcb_skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!dcb_skb)
		goto err;

	data =  genlmsg_put_reply(dcb_skb, info, &dcb_family, 0,
				  DCB_C_PFC_GCFG);
	if (!data)
		goto err;

	nest = nla_nest_start(dcb_skb, DCB_A_PFC_CFG);
	if (!nest)
		goto err;

	for (i = PFC_A_UP_0; i < PFC_A_UP_MAX; i++) {
		if (!tb[i] && !tb[PFC_A_UP_ALL])
			continue;

		ret = nla_put_u8(dcb_skb, i,
			      adapter->dcb_cfg.tc_config[i-PFC_A_UP_0].dcb_pfc);
		if (ret) {
			nla_nest_cancel(dcb_skb, nest);
			goto err;
		}
	}

	nla_nest_end(dcb_skb, nest);

	genlmsg_end(dcb_skb, data);

	ret = genlmsg_reply(dcb_skb, info);
	if (ret)
		goto err;

	dev_put(netdev);
	return 0;

err:
	DPRINTK(DRV, ERR, "Error in get pfc stats.\n");
	kfree(dcb_skb);
err_out:
	dev_put(netdev);
	return ret;
}
#endif

#ifdef CONFIG_DCB
static u8 ixgbe_dcbnl_set_all(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int ret;

	if (!adapter->dcb_set_bitmap)
		return DCB_NO_HW_CHG;

	/* Only take down the adapter if the configuration change
	 * requires a reset.
	*/
	if (adapter->dcb_set_bitmap & BIT_RESETLINK) {
		while (test_and_set_bit(__IXGBE_RESETTING, &adapter->state))
			msleep(1);

		if (netif_running(netdev))
			ixgbe_down(adapter);
	}

	ret = ixgbe_copy_dcb_cfg(&adapter->temp_dcb_cfg, &adapter->dcb_cfg,
				 adapter->ring_feature[RING_F_DCB].indices);
	if (ret) {
		if (adapter->dcb_set_bitmap & BIT_RESETLINK)
			clear_bit(__IXGBE_RESETTING, &adapter->state);
		return DCB_NO_HW_CHG;
	}

	if (adapter->dcb_cfg.pfc_mode_enable) {
		if ((adapter->hw.mac.type != ixgbe_mac_82598EB) &&
			(adapter->hw.fc.current_mode != ixgbe_fc_pfc))
			adapter->last_lfc_mode = adapter->hw.fc.current_mode;
		adapter->hw.fc.requested_mode = ixgbe_fc_pfc;
	} else {
		if (adapter->hw.mac.type != ixgbe_mac_82598EB)
			adapter->hw.fc.requested_mode = adapter->last_lfc_mode;
		else
			adapter->hw.fc.requested_mode = ixgbe_fc_none;
	}

	if (adapter->dcb_set_bitmap & BIT_RESETLINK) {
		if (netif_running(netdev))
			ixgbe_up(adapter);
		ret = DCB_HW_CHG_RST;
	} else if (adapter->dcb_set_bitmap & BIT_PFC) {
		if (adapter->hw.mac.type == ixgbe_mac_82598EB)
			ixgbe_dcb_config_pfc_82598(&adapter->hw,
				&adapter->dcb_cfg);
		else if (adapter->hw.mac.type == ixgbe_mac_82599EB)
			ixgbe_dcb_config_pfc_82599(&adapter->hw,
				&adapter->dcb_cfg);
		ret = DCB_HW_CHG;
	}
	if (adapter->dcb_cfg.pfc_mode_enable)
		adapter->hw.fc.current_mode = ixgbe_fc_pfc;

	if (adapter->dcb_set_bitmap & BIT_RESETLINK)
		clear_bit(__IXGBE_RESETTING, &adapter->state);
	adapter->dcb_set_bitmap = 0x00;
	return ret;
}
#else
static int ixgbe_dcb_set_all(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct ixgbe_adapter *adapter = NULL;
	int ret = -ENOMEM;
	u8 value;
	u8 retval = 0;

	if (!info->attrs[DCB_A_IFNAME] || !info->attrs[DCB_A_SET_ALL])
		goto err;

	netdev = dev_get_by_name(&init_net,
				 nla_data(info->attrs[DCB_A_IFNAME]));
	if (!netdev)
		goto err;

	ret = ixgbe_dcb_check_adapter(netdev);
	if (ret)
		goto err_out;
	else
		adapter = netdev_priv(netdev);

	if (!(adapter->flags & IXGBE_FLAG_DCA_CAPABLE)) {
		ret = -EINVAL;
		goto err_out;
	}

	value = nla_get_u8(info->attrs[DCB_A_SET_ALL]);
	if ((value & 1) != value) {
		DPRINTK(DRV, ERR, "Value is not 1 or 0, it is %d.\n", value);
	} else {
		if (!adapter->dcb_set_bitmap) {
			retval = 1;
			goto out;
		}

		while (test_and_set_bit(__IXGBE_RESETTING, &adapter->state))
			msleep(1);

		ret = ixgbe_copy_dcb_cfg(&adapter->temp_dcb_cfg,
				&adapter->dcb_cfg,
				adapter->ring_feature[RING_F_DCB].indices);
		if (ret) {
			clear_bit(__IXGBE_RESETTING, &adapter->state);
			goto err_out;
		}

		ixgbe_down(adapter);
		ixgbe_up(adapter);
		adapter->dcb_set_bitmap = 0x00;
		clear_bit(__IXGBE_RESETTING, &adapter->state);
	}

out:
	ret = ixgbe_nl_reply(retval, DCB_C_SET_ALL, DCB_A_SET_ALL, info);
	if (ret)
		goto err_out;

err_out:
	dev_put(netdev);
err:
	return ret;
}
#endif

#ifdef CONFIG_DCB
static u8 ixgbe_dcbnl_getcap(struct net_device *netdev, int capid, u8 *cap)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u8 rval = 0;

	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		switch (capid) {
		case DCB_CAP_ATTR_PG:
			*cap = true;
			break;
		case DCB_CAP_ATTR_PFC:
			*cap = true;
			break;
		case DCB_CAP_ATTR_UP2TC:
			*cap = false;
			break;
		case DCB_CAP_ATTR_PG_TCS:
			*cap = 0x80;
			break;
		case DCB_CAP_ATTR_PFC_TCS:
			*cap = 0x80;
			break;
		case DCB_CAP_ATTR_GSP:
			*cap = true;
			break;
		default:
			rval = -EINVAL;
			break;
		}
	} else {
		rval = -EINVAL;
	}

	return rval;
}

static u8 ixgbe_dcbnl_getnumtcs(struct net_device *netdev, int tcid, u8 *num)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u8 rval = 0;

	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		switch (tcid) {
		case DCB_NUMTCS_ATTR_PG:
			*num = MAX_TRAFFIC_CLASS;
			break;
		case DCB_NUMTCS_ATTR_PFC:
			*num = MAX_TRAFFIC_CLASS;
			break;
		default:
			rval = -EINVAL;
			break;
		}
	} else {
		rval = -EINVAL;
	}

	return rval;
}

static u8 ixgbe_dcbnl_setnumtcs(struct net_device *netdev, int tcid, u8 num)
{
	return -EINVAL;
}

static u8 ixgbe_dcbnl_getpfcstate(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	return adapter->dcb_cfg.pfc_mode_enable;
}

static void ixgbe_dcbnl_setpfcstate(struct net_device *netdev, u8 state)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	adapter->temp_dcb_cfg.pfc_mode_enable = state;
	if (adapter->temp_dcb_cfg.pfc_mode_enable != 
		adapter->dcb_cfg.pfc_mode_enable)
		adapter->dcb_set_bitmap |= BIT_PFC;
	return;
}

#else
#endif

#ifdef CONFIG_DCB
struct dcbnl_rtnl_ops dcbnl_ops = {
	.getstate	= ixgbe_dcbnl_get_state,
	.setstate	= ixgbe_dcbnl_set_state,
	.getpermhwaddr	= ixgbe_dcbnl_get_perm_hw_addr,
	.setpgtccfgtx	= ixgbe_dcbnl_set_pg_tc_cfg_tx,
	.setpgbwgcfgtx	= ixgbe_dcbnl_set_pg_bwg_cfg_tx,
	.setpgtccfgrx	= ixgbe_dcbnl_set_pg_tc_cfg_rx,
	.setpgbwgcfgrx	= ixgbe_dcbnl_set_pg_bwg_cfg_rx,
	.getpgtccfgtx	= ixgbe_dcbnl_get_pg_tc_cfg_tx,
	.getpgbwgcfgtx	= ixgbe_dcbnl_get_pg_bwg_cfg_tx,
	.getpgtccfgrx	= ixgbe_dcbnl_get_pg_tc_cfg_rx,
	.getpgbwgcfgrx	= ixgbe_dcbnl_get_pg_bwg_cfg_rx,
	.setpfccfg	= ixgbe_dcbnl_set_pfc_cfg,
	.getpfccfg	= ixgbe_dcbnl_get_pfc_cfg,
	.setall		= ixgbe_dcbnl_set_all,
	.getcap		= ixgbe_dcbnl_getcap,
	.getnumtcs	= ixgbe_dcbnl_getnumtcs,
	.setnumtcs	= ixgbe_dcbnl_setnumtcs,
	.getpfcstate	= ixgbe_dcbnl_getpfcstate,
	.setpfcstate	= ixgbe_dcbnl_setpfcstate,
};
#else
/* DCB Generic NETLINK command Definitions */
/* Get DCB Admin Mode */
static struct genl_ops ixgbe_dcb_genl_c_gstate = {
    .cmd = DCB_C_GSTATE,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_gstate,
    .dumpit =  NULL,
};

/* Set DCB Admin Mode */
static struct genl_ops ixgbe_dcb_genl_c_sstate = {
    .cmd = DCB_C_SSTATE,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_sstate,
    .dumpit =  NULL,
};

/* Set TX Traffic Attributes */
static struct genl_ops ixgbe_dcb_genl_c_spgtx = {
    .cmd = DCB_C_PGTX_SCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_pgtx_scfg,
    .dumpit =  NULL,
};

/* Set RX Traffic Attributes */
static struct genl_ops ixgbe_dcb_genl_c_spgrx = {
    .cmd = DCB_C_PGRX_SCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_pgrx_scfg,
    .dumpit =  NULL,
};

/* Set PFC CFG */
static struct genl_ops ixgbe_dcb_genl_c_spfc = {
    .cmd = DCB_C_PFC_SCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_spfccfg,
    .dumpit =  NULL,
};

/* Get TX Traffic Attributes */
static struct genl_ops ixgbe_dcb_genl_c_gpgtx = {
    .cmd = DCB_C_PGTX_GCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_pgtx_gcfg,
    .dumpit =  NULL,
};

/* Get RX Traffic Attributes */
static struct genl_ops ixgbe_dcb_genl_c_gpgrx = {
    .cmd = DCB_C_PGRX_GCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_pgrx_gcfg,
    .dumpit =  NULL,
};

/* Get PFC CFG */
static struct genl_ops ixgbe_dcb_genl_c_gpfc = {
    .cmd = DCB_C_PFC_GCFG,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_gpfccfg,
    .dumpit =  NULL,
};


/* Get Link Speed setting */
static struct genl_ops ixgbe_dcb_genl_c_glink_spd = {
    .cmd = DCB_C_GLINK_SPD,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_glink_spd,
    .dumpit =  NULL,
};

/* Set Link Speed setting */
static struct genl_ops ixgbe_dcb_genl_c_slink_spd = {
    .cmd = DCB_C_SLINK_SPD,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_slink_spd,
    .dumpit =  NULL,
};

/* Set all "set" feature */
static struct genl_ops ixgbe_dcb_genl_c_set_all= {
    .cmd = DCB_C_SET_ALL,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_set_all,
    .dumpit =  NULL,
};

/* Get permanent HW address */
static struct genl_ops ixgbe_dcb_genl_c_gperm_hwaddr = {
    .cmd = DCB_C_GPERM_HWADDR,
    .flags = GENL_ADMIN_PERM,
    .policy = dcb_genl_policy,
    .doit = ixgbe_dcb_gperm_hwaddr,
    .dumpit =  NULL,
};

/**
 * ixgbe_dcb_netlink_register - Initialize the NETLINK communication channel
 *
 * Description:
 * Call out to the DCB components so they can register their families and
 * commands with Generic NETLINK mechanism.  Return zero on success and
 * non-zero on failure.
 *
 */
int ixgbe_dcb_netlink_register(void)
{
	int ret = 1;

	/* consider writing as:
	 * ret = genl_register_family(aaa)
	 *	|| genl_register_ops(bbb, bbb)
	 *	|| genl_register_ops(ccc, ccc);
	 * if (ret)
	 *	goto err;
	 */
	ret = genl_register_family(&dcb_family);
	if (ret)
		return ret;

	ret =  genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_gstate);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_sstate);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_spgtx);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_spgrx);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_spfc);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_gpfc);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_gpgtx);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_gpgrx);
	if (ret)
		goto err;


	ret =  genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_glink_spd);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_slink_spd);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_set_all);
	if (ret)
		goto err;

	ret = genl_register_ops(&dcb_family, &ixgbe_dcb_genl_c_gperm_hwaddr);
	if (ret)
		goto err;

	return 0;

err:
	genl_unregister_family(&dcb_family);
	return ret;
}

int ixgbe_dcb_netlink_unregister(void)
{
	return genl_unregister_family(&dcb_family);
}
#endif
