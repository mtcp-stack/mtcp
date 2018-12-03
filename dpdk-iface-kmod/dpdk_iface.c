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
/*--------------------------------------------------------------------------*/
#include <linux/device.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include "dpdk_iface.h"
/*--------------------------------------------------------------------------*/
struct stats_struct sarrays[MAX_DEVICES][MAX_QID] = {{{0, 0, 0, 0, 0, 0, 0, 0, 0}}};
struct stats_struct old_sarrays[MAX_DEVICES][MAX_QID] = {{{0, 0, 0, 0, 0, 0, 0, 0, 0}}};
static int major_no = -1;
/*--------------------------------------------------------------------------*/
static int
update_stats(struct stats_struct *stats)
{
	uint8_t qid = stats->qid;
	uint8_t device = stats->dev;
	struct stats_struct *old_sarray = &old_sarrays[device][qid];
	struct stats_struct *sarray = &sarrays[device][qid];
	
	if (unlikely(sarrays[device][qid].rx_bytes > stats->rx_bytes ||
		     sarrays[device][qid].tx_bytes > stats->tx_bytes)) {
		/* mTCP app restarted?? */
		old_sarray->rx_bytes += sarray->rx_bytes;
		old_sarray->rx_pkts += sarray->rx_pkts;
		old_sarray->tx_bytes += sarray->tx_bytes;
		old_sarray->tx_pkts += sarray->tx_pkts;
		old_sarray->rmiss += sarray->rmiss;
		old_sarray->rerr += sarray->rerr;
		old_sarray->terr += sarray->terr;		
	}

	sarray->rx_bytes = stats->rx_bytes;
	sarray->rx_pkts = stats->rx_pkts;
	sarray->tx_bytes = stats->tx_bytes;
	sarray->tx_pkts = stats->tx_pkts;
	sarray->rmiss = stats->rmiss;
	sarray->rerr = stats->rerr;
	sarray->terr = stats->terr;	

#if 0
	printk(KERN_ALERT "%s: Dev: %d, Qid: %d, RXP: %llu, "
	       "RXB: %llu, TXP: %llu, TXB: %llu\n",
	       device, qid,
	       THIS_MODULE->name,
	       (long long unsigned int)sarray->rx_pkts,
	       (long long unsigned int)sarray->rx_bytes,
	       (long long unsigned int)sarray->tx_pkts,
	       (long long unsigned int)sarray->tx_bytes);
#endif
	return 0;
}
/*--------------------------------------------------------------------------*/
static void
clear_all_netdevices(void)
{
	struct net_device *netdev, *dpdk_netdev;
	uint8_t freed;
	
	do {
		dpdk_netdev = NULL;
		freed = 0;
		write_lock(&dev_base_lock);
		netdev = first_net_device(&init_net);
		while (netdev) {
			if (strncmp(netdev->name, IFACE_PREFIX,
				    strlen(IFACE_PREFIX)) == 0) {
				dpdk_netdev = netdev;
				break;
			}
			netdev = next_net_device(netdev);
		}
		write_unlock(&dev_base_lock);
		if (dpdk_netdev) {
			unregister_netdev(dpdk_netdev);
			free_netdev(dpdk_netdev);
			freed = 1;
		}
	} while (freed);
}
/*--------------------------------------------------------------------------*/
int
igb_net_open(struct inode *inode, struct file *filp)
{
	return 0;
}
/*--------------------------------------------------------------------------*/
int
igb_net_release(struct inode *inode, struct file *filp)
{
	return 0;
}
/*--------------------------------------------------------------------------*/
long
igb_net_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned char mac_addr[ETH_ALEN];
	struct net_device *netdev;
	struct stats_struct ss;	
	struct net_adapter *adapter = NULL;
	struct PciDevice pd;

	switch (cmd) {
	case SEND_STATS:
		ret = copy_from_user(&ss,
				     (struct stats_struct __user *)arg,
				     sizeof(struct stats_struct));
		if (ret)
			return -EFAULT;		
		ret = update_stats(&ss);
		break;
	case CREATE_IFACE:
		ret = copy_from_user(&pd,
				     (PciDevice __user *)arg,
				     sizeof(PciDevice));
		ret = copy_from_user(mac_addr,
				     (unsigned char __user *)pd.ports_eth_addr,
				     ETH_ALEN);
		if (!ret) {
			/* first check whether the entry does not exist */
			read_lock(&dev_base_lock);
			netdev = first_net_device(&init_net);
			while (netdev) {
				if (memcmp(netdev->dev_addr, mac_addr, ETH_ALEN) == 0) {
					read_unlock(&dev_base_lock);
					printk(KERN_ERR "%s: port already registered!\n", THIS_MODULE->name);
					return -EINVAL;
				}
				netdev = next_net_device(netdev);
			}
			read_unlock(&dev_base_lock);
			
			/* initialize the corresponding netdev */
			netdev = alloc_etherdev(sizeof(struct net_adapter));
			if (!netdev) {
				ret = -ENOMEM;
			} else {
				SET_NETDEV_DEV(netdev, NULL);
				adapter = netdev_priv(netdev);
				adapter->netdev = netdev;
				netdev_assign_netdev_ops(netdev);
				memcpy(netdev->dev_addr, mac_addr, ETH_ALEN);
				strcpy(netdev->name, IFACE_PREFIX"%d");
				ret = register_netdev(netdev);
				if (ret)
					goto fail_ioremap;
				adapter->netdev_registered = true;
				
				if ((ret=sscanf(netdev->name, IFACE_PREFIX"%hu", &adapter->bd_number)) <= 0)
					goto fail_bdnumber;
				
				printk(KERN_INFO "%s: ifindex picked: %hu\n",
				       THIS_MODULE->name, adapter->bd_number);
				/* reset nstats */
				memset(&adapter->nstats, 0, sizeof(struct net_device_stats));
				/* set 'fake' pci address */
				memcpy(&adapter->pa, &pd.pa, sizeof(struct PciAddress));
				ret = copy_to_user((unsigned char __user *)arg,
						   netdev->name,
						   IFNAMSIZ);
				if (ret) {
					printk(KERN_INFO "%s: Interface %s copy to user failed!\n",
					       THIS_MODULE->name, netdev->name);
					ret = -1;
					goto fail_pciaddr;
				}
				/* set numa locality */
				adapter->numa_socket = pd.numa_socket;
			}
		}
		break;
	case CLEAR_IFACE:
		clear_all_netdevices();
		break;

	case FETCH_PCI_ADDRESS:
		ret = copy_from_user(&pd,
				     (PciDevice __user *)arg,
				     sizeof(PciDevice));
		if (!ret) {
			read_lock(&dev_base_lock);
			netdev = first_net_device(&init_net);
			while (netdev) {
				if (strcmp(netdev->name, pd.ifname) == 0) {
					read_unlock(&dev_base_lock);
					printk(KERN_INFO "%s: Passing PCI info of %s to user\n",
					       THIS_MODULE->name, pd.ifname);
					adapter = netdev_priv(netdev);
					ret = copy_to_user(&((PciDevice __user *)arg)->pa,
							   &adapter->pa,
							   sizeof(struct PciAddress));
					if (ret) return -1;
					ret = copy_to_user(&((PciDevice __user *)arg)->numa_socket,
							   &adapter->numa_socket,
							   sizeof(adapter->numa_socket));
					if (ret) return -1;
					return 0;
				}
				netdev = next_net_device(netdev);
			}
			read_unlock(&dev_base_lock);
			ret = -1;
		}
		break;
	default:
		ret = -ENOTTY;
		break;
	}


	return ret;
 fail_pciaddr:
 fail_bdnumber:
	unregister_netdev(netdev);
 fail_ioremap:
	free_netdev(netdev);
	return ret;
}
/*--------------------------------------------------------------------------*/
static struct file_operations igb_net_fops = {
	.open = 		igb_net_open,
	.release = 		igb_net_release,
	.unlocked_ioctl = 	igb_net_ioctl,
};
/*--------------------------------------------------------------------------*/
static int __init
iface_pci_init_module(void)
{
	int ret;

	ret = register_chrdev(0 /* MAJOR */,
			      DEV_NAME /*NAME*/,
			      &igb_net_fops);
	if (ret < 0) {
		printk(KERN_ERR "%s: register_chrdev failed\n",
		       THIS_MODULE->name);
		return ret;
	}

	printk(KERN_INFO "%s: Loaded\n",
	       THIS_MODULE->name);

	/* record major number */
	major_no = ret;
	
	return 0;
}
/*--------------------------------------------------------------------------*/
static void __exit
iface_pci_exit_module(void)
{
	clear_all_netdevices();
	unregister_chrdev(major_no, DEV_NAME);
}
/*--------------------------------------------------------------------------*/
module_init(iface_pci_init_module);
module_exit(iface_pci_exit_module);

MODULE_DESCRIPTION("Interface driver for DPDK devices");
MODULE_LICENSE("BSD");
MODULE_AUTHOR("mtcp@list.ndsl.kaist.edu");
/*--------------------------------------------------------------------------*/
