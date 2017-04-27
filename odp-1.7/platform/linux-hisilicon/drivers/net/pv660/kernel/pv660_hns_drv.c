/*
 * Copyright (c) 2014-2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/cacheflush.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_net.h>
#include <linux/fs.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/rtnetlink.h>
#include <linux/acpi.h>
#include <linux/uio_driver.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/iommu.h>

#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include "pv660_hns_drv.h"
#include "hns_dsaf_reg.h"
#include "hns_dsaf_main.h"
#include "hns_dsaf_rcb.h"

#define MODE_IDX_IN_NAME 8
#define HNS_UIO_DEV_MAX	 129

/* module_param(num, int, S_IRUGO); */

#define UIO_OK	  0
#define UIO_ERROR -1

#ifndef PRINT
#define PRINT(LOGLEVEL, fmt, ...) printk(LOGLEVEL "[Func: %s. Line: %d] " fmt, \
					 __func__, __LINE__, ## __VA_ARGS__)
#endif

struct hns_uio_ioctrl_para {
	unsigned long long index;
	unsigned long long cmd;
	unsigned long long value;
	unsigned char	   data[40];
};

/*#define hns_setbit(x, y) (x) |= (1 << (y))
 #define hns_clrbit(x, y) (x) &= ~(1 << (y))*/

static int char_dev_flag;
static int uio_index;
struct nic_uio_device *uio_dev_info[HNS_UIO_DEV_MAX] = {
	0
};

struct char_device char_dev;

struct task_struct *ring_task;
unsigned int kthread_stop_flag;

static int   port_vf[] = {
	0, 0, 0, 8, 16, 32, 128, 0, 0, 8, 16, 64, 1, 2, 4, 16
};

/**
 * dummy function whenever a device is `opened'
 */
static int netdev_open(struct net_device *netdev)
{
	(void)netdev;
	return 0;
}

/**
 * dummy function for retrieving net stats
 */
static struct net_device_stats *netdev_stats(struct net_device *netdev)
{
	struct nic_uio_device *adapter;
	int ifdx;

	adapter = netdev_priv(netdev);
	ifdx = adapter->bd_number;

	adapter->nstats.rx_packets = 0;
	adapter->nstats.tx_packets = 0;
	adapter->nstats.rx_bytes = 0;
	adapter->nstats.tx_bytes = 0;

	return &adapter->nstats;
}

/**
 * dummy function for setting features
 */
static int netdev_set_features(struct net_device *netdev,
			       netdev_features_t  features)
{
	(void)netdev;
	(void)features;
	return 0;
}

/**
 * dummy function for fixing features
 */
static netdev_features_t netdev_fix_features(struct net_device *netdev,
					     netdev_features_t	features)
{
	(void)netdev;
	(void)features;
	return 0;
}

/**
 * dummy function that returns void
 */
static void netdev_no_ret(struct net_device *netdev)
{
	(void)netdev;
}

/**
 * dummy tx function
 */
static int netdev_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	(void)netdev;
	(void)skb;
	return 0;
}

/**
 * A naive net_device_ops struct to get the interface visible to the OS
 */
static const struct net_device_ops netdev_ops = {
	.ndo_open = netdev_open,
	.ndo_stop = netdev_open,
	.ndo_start_xmit	 = netdev_xmit,
	.ndo_set_rx_mode = netdev_no_ret,
	.ndo_validate_addr    = netdev_open,
	.ndo_set_mac_address  = NULL,
	.ndo_change_mtu = NULL,
	.ndo_tx_timeout = netdev_no_ret,
	.ndo_vlan_rx_add_vid  = NULL,
	.ndo_vlan_rx_kill_vid = NULL,
	.ndo_do_ioctl	     = NULL,
	.ndo_set_vf_mac	     = NULL,
	.ndo_set_vf_vlan     = NULL,
	.ndo_set_vf_rate     = NULL,
	.ndo_set_vf_spoofchk = NULL,
	.ndo_get_vf_config = NULL,
	.ndo_get_stats = netdev_stats,
	.ndo_setup_tc  = NULL,

	/* .ndo_poll_controller    = netdev_no_ret, */
	.ndo_set_features = netdev_set_features,
	.ndo_fix_features = netdev_fix_features,
	.ndo_fdb_add	      = NULL,
};

/**
 * assignment function
 */
void netdev_assign_netdev_ops(struct net_device *dev)
{
	dev->netdev_ops = &netdev_ops;
}

static ssize_t hns_cdev_read(
	struct file *file,
	char __user *buffer,
	size_t	     length,
	loff_t	    *offset)
{
	return UIO_OK;
}

static ssize_t hns_cdev_write(struct file *file,
			      const char __user *buffer, size_t length,
			      loff_t *offset)
{
	return UIO_OK;
}

static int hns_uio_change_mtu(struct nic_uio_device *priv, int new_mtu)
{
	struct hnae_handle *h = priv->ae_handle;
	int ret;
	int state = 1;

	if (new_mtu < 68)
		return UIO_ERROR;

	if (!h->dev->ops->set_mtu)
		return UIO_ERROR;

	state = h->dev->ops->get_status(h);

	if (state) {
		if (h->dev->ops->stop)
			h->dev->ops->stop(h);

		ret = h->dev->ops->set_mtu(h, new_mtu);
		ret = h->dev->ops->start ? h->dev->ops->start(h) : 0;
	} else {
		ret = h->dev->ops->set_mtu(h, new_mtu);
	}

	if (!ret)
		priv->netdev->mtu = new_mtu;
	else
		netdev_err(priv->netdev, "set mtu net fail. ret = %d.\n", ret);

	return ret;
}

void hns_uio_get_stats(struct nic_uio_device *priv,
		       unsigned long long    *data)
{
	unsigned long long *p = data;
	struct hnae_handle *h = priv->ae_handle;
	const struct rtnl_link_stats64 *net_stats;
	struct rtnl_link_stats64 temp;

	if (!h->dev->ops->get_stats || !h->dev->ops->update_stats) {
		netdev_err(priv->netdev,
			   "get_stats or update_stats is null!\n");
		return;
	}

	h->dev->ops->update_stats(h, &priv->netdev->stats);

	net_stats = dev_get_stats(priv->netdev, &temp);

	/* get netdev statistics */
	p[0]  = net_stats->rx_packets;
	p[1]  = net_stats->tx_packets;
	p[2]  = net_stats->rx_bytes;
	p[3]  = net_stats->tx_bytes;
	p[4]  = net_stats->rx_errors;
	p[5]  = net_stats->tx_errors;
	p[6]  = net_stats->rx_dropped;
	p[7]  = net_stats->tx_dropped;
	p[8]  = net_stats->multicast;
	p[9]  = net_stats->collisions;
	p[10] = net_stats->rx_over_errors;
	p[11] = net_stats->rx_crc_errors;
	p[12] = net_stats->rx_frame_errors;
	p[13] = net_stats->rx_fifo_errors;
	p[14] = net_stats->rx_missed_errors;
	p[15] = net_stats->tx_aborted_errors;
	p[16] = net_stats->tx_carrier_errors;
	p[17] = net_stats->tx_fifo_errors;
	p[18] = net_stats->tx_heartbeat_errors;
	p[19] = net_stats->rx_length_errors;
	p[20] = net_stats->tx_window_errors;
	p[21] = net_stats->rx_compressed;
	p[22] = net_stats->tx_compressed;

	p[23] = priv->netdev->rx_dropped.counter;
	p[24] = priv->netdev->tx_dropped.counter;

	/* get driver statistics */
	h->dev->ops->get_stats(h, &p[25]);
}

void hns_uio_pausefrm_cfg(void *mac_drv, u32 rx_en, u32 tx_en)
{
	struct hns_mac_cb *mac_cb = (struct hns_mac_cb *)mac_drv;
	u8 __iomem *base = (u8 *)mac_cb->vaddr + XGMAC_MAC_PAUSE_CTRL_REG;
	u32 origin = readl(base);

	dsaf_set_bit(origin, XGMAC_PAUSE_CTL_TX_B, !!tx_en);
	dsaf_set_bit(origin, XGMAC_PAUSE_CTL_RX_B, !!rx_en);
	writel(origin, base);
}

void hns_uio_set_iommu(struct nic_uio_device *priv, unsigned long iova,
		       unsigned long paddr, int gfp_order)
{
	struct iommu_domain *domain;
	int ret = 0;

	domain = iommu_domain_alloc(priv->dev->bus);

	if (!domain)
		PRINT(KERN_ERR, "domain is null\n");

	ret = iommu_attach_device(domain, priv->dev);
	PRINT(KERN_ERR, "domain is null = %d\n", ret);

	ret =
		iommu_map(domain, iova, (phys_addr_t)paddr, gfp_order,
			  (IOMMU_WRITE | IOMMU_READ | IOMMU_CACHE));
	PRINT(KERN_ERR, "domain is null = %d\n", ret);
}

long hns_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int index = 0;
	void __user *parg;
	struct hns_uio_ioctrl_para uio_para;
	struct nic_uio_device	  *priv = NULL;
	struct hnae_handle *handle;

	/* unsigned long long data[128] = {0}; */

	parg = (void __user *)arg;

	if (copy_from_user(&uio_para, parg,
			   sizeof(struct hns_uio_ioctrl_para))) {
		PRINT(KERN_ERR, "copy_from_user error.\n");
		return UIO_ERROR;
	}

	if (uio_para.index >= uio_index) {
		PRINT(KERN_ERR, "Device index is out of range (%d).\n",
		      uio_index);
		return UIO_ERROR;
	}

	priv = uio_dev_info[uio_para.index];
	if (!priv) {
		PRINT(KERN_ERR, "nic_uio_dev is null!\n");
		return UIO_ERROR;
	}

	handle = priv->ae_handle;
	index  = uio_para.index;

	switch (cmd) {
	case HNS_UIO_IOCTL_MAC:
	{
		memcpy((void *)priv->netdev->dev_addr,
		       (void *)&uio_para.data[0], 6);
		ret = handle->dev->ops->set_mac_addr(handle,
						     priv->netdev->dev_addr);
		if (ret) {
			PRINT(KERN_ERR, "set_mac_addr fail, ret = %d\n", ret);
			return UIO_ERROR;
		}

		break;
	}
	case HNS_UIO_IOCTL_UP:
	{
		ret = handle->dev->ops->start ? handle->dev->ops->start(handle)
		      : 0;
		if (ret) {
			PRINT(KERN_ERR, "set_mac_addr fail, ret = %d.\n", ret);
			return UIO_ERROR;
		}

		break;
	}
	case HNS_UIO_IOCTL_DOWN:
	{
		if (handle->dev->ops->stop)
			handle->dev->ops->stop(priv->ae_handle);

		break;
	}
	case HNS_UIO_IOCTL_PORT:
	{
		uio_para.value = priv->port;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_VF_MAX:
	{
		uio_para.value = priv->vf_sum;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_VF_ID:
	{
		uio_para.value = priv->vf_id;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_QNUM:
	{
		uio_para.value = priv->q_num;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_VF_START:
	{
		uio_para.value = priv->uio_start;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_MTU:
	{
		ret = hns_uio_change_mtu(priv, (int)uio_para.value);
		break;
	}
	case HNS_UIO_IOCTL_GET_STAT:
	{
		unsigned long long *data = kzalloc(
			sizeof(unsigned long long) * 256, GFP_KERNEL);

		hns_uio_get_stats(priv, data);
		if (copy_to_user((void __user *)arg, data, sizeof(data)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_GET_LINK:
		uio_para.value =
			handle->dev->ops->get_status ? handle->dev->ops->
			get_status(handle) : 0;
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;

	case HNS_UIO_IOCTL_REG_READ:
	{
		struct hnae_queue *queue;

		queue = handle->qs[0];
		uio_para.value = dsaf_read_reg(queue->io_base, uio_para.cmd);
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_REG_WRITE:
	{
		struct hnae_queue *queue;

		queue = handle->qs[0];
		dsaf_write_reg(queue->io_base, uio_para.cmd, uio_para.value);
		uio_para.value = dsaf_read_reg(queue->io_base, uio_para.cmd);
		if (copy_to_user((void __user *)arg, &uio_para,
				 sizeof(struct hns_uio_ioctrl_para)) != 0)
			return UIO_ERROR;

		break;
	}
	case HNS_UIO_IOCTL_SET_PAUSE:
	{
		hns_uio_pausefrm_cfg(priv->vf_cb->mac_cb, 0, uio_para.value);
		break;
	}

	default:
		PRINT(KERN_ERR, "uio ioctl cmd(%d) illegal! range:0-%d.\n", cmd,
		      HNS_UIO_IOCTL_NUM - 1);
		return UIO_ERROR;
	}

	return ret;
}

const struct file_operations hns_uio_fops = {
	.owner = THIS_MODULE,
	.read  = hns_cdev_read,
	.write = hns_cdev_write,
	.unlocked_ioctl = hns_cdev_ioctl,
	.compat_ioctl	= hns_cdev_ioctl,
};

int hns_uio_register_cdev(void)
{
	struct device *aeclassdev;
	struct char_device *priv = &char_dev;

	if (char_dev_flag++ != 0)
		return UIO_OK;

	(void)strncpy(priv->name, "nic_uio", strlen("nic_uio"));
	priv->major = register_chrdev(0, priv->name, &hns_uio_fops);
	(void)strncpy(priv->class_name, "nic_uio", strlen("nic_uio"));
	priv->dev_class = class_create(THIS_MODULE, priv->class_name);
	if (IS_ERR(priv->dev_class)) {
		PRINT(KERN_ERR, "Class_create device %s failed!\n",
		      priv->class_name);
		(void)unregister_chrdev(priv->major, priv->name);
		return PTR_ERR(priv->dev_class);
	}

	aeclassdev = device_create(priv->dev_class, NULL, MKDEV(priv->major,
								0), NULL,
				   priv->name);
	if (IS_ERR(aeclassdev)) {
		PRINT(KERN_ERR, "Class_device_create device %s failed!\n",
		      priv->class_name);
		(void)unregister_chrdev(priv->major, priv->name);
		class_destroy((void *)priv->dev_class);
		return PTR_ERR(aeclassdev);
	}

	return UIO_OK;
}

void hns_uio_unregister_cdev(void)
{
	struct char_device *priv = &char_dev;

	if (char_dev_flag == 0)
		return;

	if (char_dev_flag == 1) {
		unregister_chrdev(priv->major, priv->name);
		device_destroy(priv->dev_class, MKDEV(priv->major, 0));
		class_destroy(priv->dev_class);
	}

	char_dev_flag--;
}

static int hns_uio_nic_open(struct uio_info *dev_info, struct inode *node)
{
	/* PRINT("hns_uio_nic_open = 0x%llx\n", dev_info->mem[0].addr); */
	return UIO_OK;
}

static int hns_uio_nic_release(struct uio_info *dev_info,
			       struct inode    *inode)
{
	return UIO_OK;
}

static int hns_uio_nic_irqcontrol(struct uio_info *dev_info, s32 irq_state)
{
	PRINT(KERN_ERR, "hns_uio_nic_open = %d\n", irq_state);
	return UIO_OK;
}

static irqreturn_t hns_uio_nic_irqhandler(int		   irq,
					  struct uio_info *dev_info)
{
	struct nic_uio_device *priv = NULL;

	priv = uio_dev_info[dev_info->mem[3].addr];
	uio_event_notify(&priv->uinfo);
	PRINT(KERN_ERR, "hns_uio_nic_open = %d\n", irq);
	return IRQ_HANDLED;
}

static int hns_uio_alloc(struct hnae_ring *ring, struct hnae_desc_cb *cb)
{
	return UIO_OK;
}

static void hns_uio_free(struct hnae_ring *ring, struct hnae_desc_cb *cb)
{
}

static int hns_uio_map(struct hnae_ring *ring, struct hnae_desc_cb *cb)
{
	return UIO_OK;
}

static void hns_uio_unmap(struct hnae_ring *ring, struct hnae_desc_cb *cb)
{
}

static struct hnae_buf_ops hns_uio_nic_bops = {
	.alloc_buffer = hns_uio_alloc,
	.free_buffer  = hns_uio_free,
	.map_buffer   = hns_uio_map,
	.unmap_buffer = hns_uio_unmap,
};

void hns_free_buffers(struct hnae_ring *ring)
{
	int i;

	for (i = 0; i < ring->desc_num; i++)
		hnae_free_buffer_detach(ring, i);
}

/* free desc along with its attached buffer */
void hns_free_desc(struct hnae_ring *ring)
{
	hns_free_buffers(ring);
	dma_unmap_single(ring_to_dev(ring), ring->desc_dma_addr,
			 ring->desc_num * sizeof(ring->desc[0]),
			 ring_to_dma_dir(ring));
	ring->desc_dma_addr = 0;
	kfree(ring->desc);
	ring->desc = NULL;
}

/* fini ring, also free the buffer for the ring */
void hns_fini_ring(struct hnae_ring *ring)
{
	hns_free_desc(ring);
	kfree(ring->desc_cb);
	ring->desc_cb = NULL;
	ring->next_to_clean = 0;
	ring->next_to_use = 0;
}

void hns_kernel_queue_free(struct hnae_handle *h)
{
	int i;
	struct hnae_queue *q;

	for (i = 0; i < h->q_num; i++) {
		q = h->qs[i];

		if (q->dev->ops->fini_queue)
			q->dev->ops->fini_queue(q);

		hns_fini_ring(&q->tx_ring);
		hns_fini_ring(&q->rx_ring);
	}
}

int hns_user_queue_malloc(struct hnae_handle *handle)
{
	int i;
	struct hnae_ring *tx_ring;
	struct hnae_ring *rx_ring;
	unsigned char	 *base_tx_cb;
	unsigned char	 *base_rx_cb;
	unsigned char	 *base_tx_desc;
	unsigned char	 *base_rx_desc;
	dma_addr_t base_tx_dma;
	dma_addr_t base_rx_dma;
	int cb_size;
	int desc_size;

	tx_ring = (struct hnae_ring *)&handle->qs[0]->tx_ring;
	rx_ring = (struct hnae_ring *)&handle->qs[0]->rx_ring;

	cb_size = tx_ring->desc_num * sizeof(tx_ring->desc_cb[0]);
	desc_size = tx_ring->desc_num * sizeof(tx_ring->desc[0]);

	base_tx_cb = kcalloc(handle->q_num, cb_size, GFP_KERNEL);
	if (!base_tx_cb)
		return UIO_ERROR;

	base_rx_cb = kcalloc(handle->q_num, cb_size, GFP_KERNEL);
	if (!base_rx_cb) {
		kfree(base_tx_cb);
		return UIO_ERROR;
	}

	base_tx_desc = kzalloc(desc_size * handle->q_num, GFP_KERNEL);
	if (!base_tx_desc) {
		kfree(base_tx_cb);
		kfree(base_rx_cb);
		return UIO_ERROR;
	}

	base_tx_dma = dma_map_single(ring_to_dev(tx_ring), base_tx_desc,
				     desc_size * handle->q_num,
				     ring_to_dma_dir(tx_ring));
	if (dma_mapping_error(ring_to_dev(tx_ring), base_tx_dma)) {
		kfree(base_tx_cb);
		kfree(base_rx_cb);
		kfree(base_tx_desc);
		PRINT(KERN_ERR, "dma_mapping_error is fail!\n");
		return UIO_ERROR;
	}

	base_rx_desc = kzalloc(desc_size * handle->q_num, GFP_KERNEL);
	if (!base_rx_desc) {
		kfree(base_tx_cb);
		kfree(base_rx_cb);
		kfree(base_tx_desc);

		return UIO_ERROR;
	}

	base_rx_dma = dma_map_single(ring_to_dev(rx_ring), base_rx_desc,
				     desc_size * handle->q_num,
				     ring_to_dma_dir(rx_ring));
	if (dma_mapping_error(ring_to_dev(rx_ring), base_rx_dma)) {
		kfree(base_tx_cb);
		kfree(base_rx_cb);
		kfree(base_tx_desc);
		kfree(base_rx_desc);
		dma_unmap_single(ring_to_dev(tx_ring), base_tx_dma,
				 desc_size * handle->q_num,
				 ring_to_dma_dir(tx_ring));
		PRINT(KERN_ERR, "dma_mapping_error is fail!\n");
		return UIO_ERROR;
	}

	for (i = 0; i < handle->q_num; i++) {
		tx_ring = (struct hnae_ring *)&handle->qs[i]->tx_ring;
		rx_ring = (struct hnae_ring *)&handle->qs[i]->rx_ring;
		tx_ring->q = handle->qs[i];
		tx_ring->flags = 1;
		rx_ring->q = handle->qs[i];
		rx_ring->flags = 0;

		tx_ring->desc_cb =
			(struct hnae_desc_cb *)(base_tx_cb + cb_size * i);
		rx_ring->desc_cb =
			(struct hnae_desc_cb *)(base_rx_cb + cb_size * i);
		tx_ring->desc =
			(struct hnae_desc *)(base_tx_desc + desc_size * i);
		rx_ring->desc =
			(struct hnae_desc *)(base_rx_desc + desc_size * i);
		tx_ring->desc_dma_addr = base_tx_dma + desc_size * i;
		rx_ring->desc_dma_addr = base_rx_dma + desc_size * i;

		if (handle->dev->ops->init_queue)
			handle->dev->ops->init_queue(handle->qs[i]);
	}

	return UIO_OK;
}

int hns_user_queue_free(struct hnae_handle *handle)
{
	int i;
	struct hnae_ring *tx_ring;
	struct hnae_ring *rx_ring;

	tx_ring = (struct hnae_ring *)&handle->qs[0]->tx_ring;
	rx_ring = (struct hnae_ring *)&handle->qs[0]->rx_ring;

	kfree(rx_ring->desc);
	kfree(tx_ring->desc);
	kfree(rx_ring->desc_cb);
	kfree(tx_ring->desc_cb);

	dma_unmap_single(ring_to_dev(tx_ring), tx_ring->desc_dma_addr,
			 tx_ring->desc_num *
			 sizeof(tx_ring->desc[0]) * handle->q_num,
			 ring_to_dma_dir(tx_ring));

	dma_unmap_single(ring_to_dev(rx_ring), rx_ring->desc_dma_addr,
			 rx_ring->desc_num *
			 sizeof(rx_ring->desc[0]) * handle->q_num,
			 ring_to_dma_dir(rx_ring));

	for (i = 0; i < handle->q_num; i++) {
		tx_ring = (struct hnae_ring *)&handle->qs[i]->tx_ring;
		rx_ring = (struct hnae_ring *)&handle->qs[i]->rx_ring;
		tx_ring->desc_cb = NULL;
		rx_ring->desc_cb = NULL;
		tx_ring->desc = NULL;
		rx_ring->desc = NULL;
		tx_ring->desc_dma_addr = 0;
		rx_ring->desc_dma_addr = 0;
	}

	return UIO_OK;
}

void hnae_list_del(spinlock_t	    *lock,
		   struct list_head *node)
{
	unsigned long flags;

	spin_lock_irqsave(lock, flags);
	list_del_rcu(node);
	spin_unlock_irqrestore(lock, flags);
}

void hns_user_put_handle(struct hnae_handle *h)
{
	struct hnae_ae_dev *dev = h->dev;

	hns_user_queue_free(h);

	if (h->dev->ops->reset)
		h->dev->ops->reset(h);

	hnae_list_del(&dev->lock, &h->node);

	if (dev->ops->put_handle)
		dev->ops->put_handle(h);

	module_put(dev->owner);
}

int hns_uio_probe(struct platform_device *pdev)
{
	struct nic_uio_device *priv = NULL;
	struct hnae_handle    *handle;
	struct device *dev = &pdev->dev;
	struct device_node    *node = dev->of_node;
	struct net_device     *netdev;
	struct hnae_queue     *queue;
	struct hnae_vf_cb     *vf_cb;
	int ret;
	int port = 0;
	int uio_start = 0;
	int i = 0;
	static int cards_found;
	const char *ae_name;

	ret = of_property_read_string(node, "ae-name", &ae_name);
	if (ret)
		return UIO_ERROR;

	ret = of_property_read_u32(node, "port-id", &port);
	if (ret)
		return UIO_ERROR;

	uio_start = uio_index;
	do {
		handle = hnae_get_handle(dev, ae_name, port, &hns_uio_nic_bops);
		if (IS_ERR_OR_NULL(handle)) {
			PRINT(KERN_ERR, "hnae_get_handle fail. port=%d\n",
			      port);
			goto err_uio_dev_free;
		}

		hns_kernel_queue_free(handle);
		hns_user_queue_malloc(handle);

		vf_cb = (struct hnae_vf_cb *)container_of(
			handle, struct hnae_vf_cb, ae_handle);

		netdev = alloc_etherdev_mq(sizeof(struct nic_uio_device),
					   handle->q_num);
		if (!netdev) {
			PRINT(KERN_ERR, "alloc_etherdev_mq fail. port=%d\n",
			      port);
			goto err_get_handle;
		}

		priv = netdev_priv(netdev);
		priv->dev = dev;
		priv->netdev = netdev;
		priv->ae_handle = handle;
		priv->vf_cb  = vf_cb;
		priv->port   = port;
		priv->vf_sum = port_vf[vf_cb->dsaf_dev->dsaf_mode];
		priv->vf_id  = handle->vf_id;
		priv->q_num  = handle->q_num;
		priv->uio_start = uio_start;

		queue = handle->qs[0];
		priv->uinfo.name = DRIVER_UIO_NAME;
		priv->uinfo.version = "1";
		priv->uinfo.priv = (void *)priv;
		priv->uinfo.mem[0].name = "rcb ring";
		priv->uinfo.mem[0].addr = (unsigned long)queue->phy_base;
		priv->uinfo.mem[0].size = NIC_UIO_SIZE * handle->q_num;
		priv->uinfo.mem[0].memtype = UIO_MEM_PHYS;

		priv->uinfo.mem[1].name = "tx_bd";
		priv->uinfo.mem[1].addr = (unsigned long)queue->tx_ring.desc;
		priv->uinfo.mem[1].size = queue->tx_ring.desc_num *
					  sizeof(queue->tx_ring.desc[0]) *
					  handle->q_num;
		priv->uinfo.mem[1].memtype = UIO_MEM_LOGICAL;

		priv->uinfo.mem[2].name = "rx_bd";
		priv->uinfo.mem[2].addr = (unsigned long)queue->rx_ring.desc;
		priv->uinfo.mem[2].size = queue->rx_ring.desc_num *
					  sizeof(queue->rx_ring.desc[0]) *
					  handle->q_num;
		priv->uinfo.mem[2].memtype = UIO_MEM_LOGICAL;

		priv->uinfo.mem[3].name = "nic_uio_device";
		priv->uinfo.mem[3].addr = (unsigned long)(uio_index);
		priv->uinfo.mem[3].size = sizeof(unsigned long);
		priv->uinfo.mem[3].memtype = UIO_MEM_LOGICAL;

		/* priv->uinfo.irq = queue->rx_ring->irq; */
		priv->uinfo.irq_flags = UIO_IRQ_CUSTOM;
		priv->uinfo.handler = hns_uio_nic_irqhandler;
		priv->uinfo.irqcontrol = hns_uio_nic_irqcontrol;
		priv->uinfo.open = hns_uio_nic_open;
		priv->uinfo.release = hns_uio_nic_release;

		ret = uio_register_device(dev, &priv->uinfo);
		if (ret) {
			PRINT(KERN_ERR, "uio_register_device failed!\n");
			goto err_unregister_uio;
		}

		platform_set_drvdata(pdev, netdev);
		uio_dev_info[uio_index] = priv;

		netdev_assign_netdev_ops(netdev);
		hns_ethtool_set_ops(netdev);
		SET_NETDEV_DEV(netdev, dev);

		strcpy(netdev->name, "odp%d");
		priv->bd_number = cards_found;
		netdev->ifindex = cards_found;
		ret = register_netdev(netdev);
		if (ret)
			goto err_unregister_uio;

		/* reset nstats */
		memset(&priv->nstats, 0, sizeof(struct net_device_stats));
		priv->netdev_registered = true;

		uio_index++;
	} while (handle->vf_id < (port_vf[vf_cb->dsaf_dev->dsaf_mode] - 1));

	PRINT(KERN_ERR, "uio_start = %d, port = %d, vf = %d\n",
	      uio_start, port, port_vf[vf_cb->dsaf_dev->dsaf_mode]);

	ret = hns_uio_register_cdev();

	if (ret) {
		PRINT(KERN_ERR,
		      "registering the character device failed! ret=%d\n", ret);
		goto err_uio_dev_free;
	}

	return UIO_OK;

err_unregister_uio:
	free_netdev(priv->netdev);
err_get_handle:
	hns_user_put_handle(handle);
err_uio_dev_free:
	for (i = 0; i < uio_index; i++) {
		priv = uio_dev_info[i];
		if (!priv) {
			uio_unregister_device(&priv->uinfo);
			free_netdev(priv->netdev);
			hns_user_put_handle(priv->ae_handle);
			uio_dev_info[i] = NULL;
		}
	}

	return ret;
}

/**
 * hns_uio_nic_remove - remove nic_uio_device
 * @pdev: platform device
 *
 * Return 0 on success, negative on failure
 */
int hns_uio_remove(struct platform_device *pdev)
{
	int i = 0;
	int vf_max = 0;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct nic_uio_device *nic_uio_dev = netdev_priv(ndev);
	struct nic_uio_device *priv;

	hns_uio_unregister_cdev();

	PRINT(KERN_ERR, "vf_sum = %d, uio_start = %d, q_num = %d\n",
	      nic_uio_dev->vf_sum, nic_uio_dev->uio_start, nic_uio_dev->q_num);

	vf_max = nic_uio_dev->uio_start + nic_uio_dev->vf_sum;
	for (i = nic_uio_dev->uio_start; i < vf_max; i++) {
		priv = uio_dev_info[i];
		if (!priv)
			continue;

		uio_unregister_device(&priv->uinfo);

		if (priv->netdev_registered) {
			unregister_netdev(priv->netdev);
			priv->netdev_registered = false;
		}

		if (priv->ae_handle->dev->ops->stop)
			priv->ae_handle->dev->ops->stop(priv->ae_handle);

		free_netdev(priv->netdev);
		hns_user_put_handle(priv->ae_handle);
		uio_dev_info[i] = NULL;
	}

	PRINT(KERN_ERR, "Uninstall UIO driver successfully.\n\n");
	return UIO_OK;
}

/**
 * hns_uio_nic_suspend - netdev suspend
 * @pdev: platform device
 * @state: power manage message
 *
 * Return 0 on success, negative on failure
 */
int hns_uio_suspend(struct platform_device *pdev,
		    pm_message_t	    state)
{
	return UIO_OK;
}

/**
 * hns_uio_nic_resume - netdev resume
 * @pdev: platform device
 *
 * Return 0 on success, negative on failure
 */
int hns_uio_resume(struct platform_device *pdev)
{
	return UIO_OK;
}

/*for dts*/
static const struct of_device_id hns_uio_enet_match[]
	= {
	{.compatible = "hisilicon,hns-nic-v1"},
	{}
	};

MODULE_DEVICE_TABLE(of, hns_uio_enet_match);

static struct platform_driver hns_uio_driver = {
	.probe			     = hns_uio_probe,
	.remove	 = hns_uio_remove,
	.suspend = hns_uio_suspend,
	.resume	 = hns_uio_resume,
	.driver	 = {
		.name  = DRIVER_UIO_NAME,
		.owner = THIS_MODULE,
		.of_match_table	     = hns_uio_enet_match,
		.suppress_bind_attrs = false,
	},
};

int __init hns_uio_module_init(void)
{
	int ret;

	ret = platform_driver_register(&hns_uio_driver);
	if (ret) {
		PRINT(KERN_ERR, "platform_driver_register fail, ret = %d\n",
		      ret);
		return ret;
	}

	return UIO_OK;
}

void __exit hns_uio_module_exit(void)
{
	platform_driver_unregister(&hns_uio_driver);
}

module_init(hns_uio_module_init);

module_exit(hns_uio_module_exit);
MODULE_DESCRIPTION("Hisilicon HNS uio Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_VERSION(NIC_MOD_VERSION);
