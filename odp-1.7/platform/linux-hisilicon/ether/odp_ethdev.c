/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <odp/config.h>

#include <odp_pci.h>
#include <odp_memory.h>
#include <odp_memcpy.h>
#include <odp_mmdistrict.h>
#include <odp_base.h>
#include <odp_core.h>
#include <odp/atomic.h>
#include <odp_common.h>
#include <odp_ring.h>
#include <odp/spinlock.h>

#include "odp_ether.h"
#include "odp_ethdev.h"
#include "odp_dev.h"
#include "odp_debug_internal.h"
#include "odp_hisi_atomic.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef ODP_LIBODP_ETHDEV_DEBUG
#define UMD_DEBUG_TRACE ODP_PRINT
#else
#define UMD_DEBUG_TRACE ODP_PRINT
#endif

static const char *MZ_ODP_ETH_DEV_DATA = "odp_eth_dev_data";
struct odp_eth_dev odp_eth_devices[ODP_MAX_ETHPORTS];
static struct odp_eth_dev_data *odp_eth_dev_data;
static uint8_t nb_ports;

/* spinlock for eth device callbacks */
static odp_spinlock_t odp_eth_dev_cb_lock = {
	0
}; /* initiated unlock */

/* store statistics names and its offset in stats structure  */
struct odp_eth_xstats_name_off {
	char name[ODP_ETH_XSTATS_NAME_SIZE];

	unsigned offset;
};

static struct odp_eth_xstats_name_off odp_stats_strings[] = {
	{"rx_packets",
	 offsetof(struct odp_eth_stats, ipackets)	},
	{"tx_packets",
	 offsetof(struct odp_eth_stats, opackets)	},
	{"rx_bytes",
	 offsetof(struct odp_eth_stats, ibytes)		},
	{"tx_bytes",
	 offsetof(struct odp_eth_stats, obytes)		},
	{"tx_errors",
	 offsetof(struct odp_eth_stats, oerrors)	},
	{"rx_missed_errors",
	 offsetof(struct odp_eth_stats, imissed)	},
	{"rx_crc_errors",
	 offsetof(struct odp_eth_stats, ibadcrc)	},
	{"rx_bad_length_errors",
	 offsetof(struct odp_eth_stats, ibadlen)	},
	{"rx_errors",
	 offsetof(struct odp_eth_stats, ierrors)	},
	{"alloc_rx_buff_failed",
	 offsetof(struct odp_eth_stats, rx_nombuf)	},
	{"fdir_match",
	 offsetof(struct odp_eth_stats, fdirmatch)	},
	{"fdir_miss",
	 offsetof(struct odp_eth_stats, fdirmiss)	},
	{"tx_flow_control_xon",
	 offsetof(struct odp_eth_stats, tx_pause_xon)	},
	{"rx_flow_control_xon",
	 offsetof(struct odp_eth_stats, rx_pause_xon)	},
	{"tx_flow_control_xoff",
	 offsetof(struct odp_eth_stats, tx_pause_xoff)	},
	{"rx_flow_control_xoff",
	 offsetof(struct odp_eth_stats, rx_pause_xoff)	},
};

#define ODP_NB_STATS \
	(sizeof(odp_stats_strings) / sizeof(struct odp_eth_xstats_name_off))

static struct odp_eth_xstats_name_off odp_rxq_stats_strings[] = {
	{"rx_packets", offsetof(struct odp_eth_stats, q_ipackets)    },
	{"rx_bytes",   offsetof(struct odp_eth_stats, q_ibytes)	     },
};

#define ODP_NB_RXQ_STATS (sizeof(odp_rxq_stats_strings) / \
			  sizeof(odp_rxq_stats_strings[0]))

static struct odp_eth_xstats_name_off odp_txq_stats_strings[] = {
	{"tx_packets", offsetof(struct odp_eth_stats, q_opackets)    },
	{"tx_bytes",   offsetof(struct odp_eth_stats, q_obytes)	     },
	{"tx_errors",  offsetof(struct odp_eth_stats, q_errors)	     },
};

#define ODP_NB_TXQ_STATS (sizeof(odp_txq_stats_strings) / \
			  sizeof(odp_txq_stats_strings[0]))

/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct odp_eth_dev_callback {
	TAILQ_ENTRY(odp_eth_dev_callback) next; /**< Callbacks list */

	odp_eth_dev_cb_fn	cb_fn;          /**< Callback address */
	void		       *cb_arg;         /**< Parameter for callback */
	enum odp_eth_event_type event;          /**< Interrupt event type */
	uint32_t		active;         /**< Callback is executing */
};

enum {
	STAT_QMAP_TX = 0,
	STAT_QMAP_RX
};

enum {
	DEV_DETACHED = 0,
	DEV_ATTACHED
};

#define ETHTOOL_GDRVINFO	0x00000003 /* Get driver info. */
#define SIOCETHTOOL     0x8946

/* these strings are set to whatever the driver author decides... */
struct ethtool_drvinfo {
	uint32_t	cmd;
	/* driver short name, "tulip", "eepro100" */
	char	driver[32];
	/* driver version string */
	char	version[32];
	/* firmware version string, if applicable */
	char	fw_version[32];
	/* Bus info for this IF. */
	/* For PCI devices, use pci_dev->slot_name. */
	char	bus_info[32];
	char	erom_version[32];
	char	reserved2[16];
	/* number of u64's from ETHTOOL_GSTATS */
	uint32_t	n_stats;
	/* Size of data from ETHTOOL_GEEPROM (bytes) */
	uint32_t	testinfo_len;
	uint32_t	eedump_len;
	/* Size of data from ETHTOOL_GREGS (bytes) */
	uint32_t	regdump_len;
};

static inline void odp_eth_dev_data_alloc(void)
{
	const unsigned flags = 0;
	const struct odp_mm_district *mz;

	if (odp_process_type() == ODP_PROC_PRIMARY)
		mz = odp_mm_district_reserve(MZ_ODP_ETH_DEV_DATA,
					     MZ_ODP_ETH_DEV_DATA,
					     ODP_MAX_ETHPORTS *
					     sizeof(*odp_eth_dev_data),
					     odp_socket_id(), flags);
	else
		mz = odp_mm_district_lookup(MZ_ODP_ETH_DEV_DATA);

	if (mz == NULL) {
		ODP_PRINT("Cannot allocate mm_district for ethernet port data\n");
		return;
	}

	odp_eth_dev_data = mz->addr;

	if (odp_process_type() == ODP_PROC_PRIMARY)
		memset(odp_eth_dev_data, 0, ODP_MAX_ETHPORTS *
		       sizeof(*odp_eth_dev_data));
}

struct odp_eth_dev *odp_eth_dev_allocated(const char *name)
{
	unsigned int i;

	for (i = 0; i < ODP_MAX_ETHPORTS; i++)
		if ((odp_eth_devices[i].attached == DEV_ATTACHED) &&
		    (strcmp(odp_eth_devices[i].data->name, name) == 0))
			return &odp_eth_devices[i];

	return NULL;
}

struct odp_eth_dev *odp_eth_dev_allocated_id(int portid)
{
	if (portid < ODP_MAX_ETHPORTS &&
		odp_eth_devices[portid].attached == DEV_ATTACHED)
			return &odp_eth_devices[portid];

	return NULL;
}

static uint8_t odp_eth_dev_find_free_port(enum odp_eth_nic_type type)
{
	int ret;
	char driver_type = ODP_ETH_NIC_UNKNOWN;
	unsigned int idx;
	static int eth_dev_init_flag;
	struct ifaddrs *ifap;
	struct ifaddrs *iter_if;
	struct ethtool_drvinfo drvinfo;

	if (!eth_dev_init_flag) {
		memset(odp_eth_devices, 0, sizeof(struct odp_eth_dev) *
		       ODP_MAX_ETHPORTS);
		eth_dev_init_flag = 1;
	}

	if (getifaddrs(&ifap) != 0)
		perror("getifaddrs: ");

	iter_if = ifap;
	do {
		if ((iter_if->ifa_addr->sa_family == AF_PACKET) &&
		    (strncmp("odp", iter_if->ifa_name, strlen("odp")) == 0)) {
			struct ifreq ifr;

			strcpy(ifr.ifr_name, iter_if->ifa_name);
			/* Create socket */
			int sock = socket(AF_INET, SOCK_DGRAM, 0);

			if (sock == -1)
				ODP_PRINT("\nsocket\n");

			drvinfo.cmd = ETHTOOL_GDRVINFO;
			ifr.ifr_data = (caddr_t)&drvinfo;
			ret = ioctl(sock, SIOCETHTOOL, &ifr);
			if (ret < 0) {
				ODP_PRINT("\nCannot get driver information\n");
				break;
			}
			driver_type = drvinfo.reserved2[0];
			if (driver_type != type)
				goto next_if;

			idx = atoi(iter_if->ifa_name + strlen("odp"));
			if (idx == -1) {
				ODP_PRINT("\n alloc port_id failed!\n");
				return ODP_MAX_ETHPORTS;
			}
			if (odp_eth_devices[idx].attached == DEV_DETACHED)
				return idx;
		}
next_if:
		iter_if = iter_if->ifa_next;
	} while (iter_if != NULL);

	freeifaddrs(ifap);

	return ODP_MAX_ETHPORTS;
}

struct odp_eth_dev *odp_eth_dev_allocate(const char
					 *name,
					 enum
					 odp_eth_nic_type
					 type)
{
	uint8_t port_id;
	struct odp_eth_dev *eth_dev;

	port_id = odp_eth_dev_find_free_port(type);
	if (port_id == ODP_MAX_ETHPORTS) {
		ODP_ERR("Reached maximum number "
			"of Ethernet ports\n");
		return NULL;
	}

	if (odp_eth_dev_allocated(name) != NULL) {
		ODP_ERR("Ethernet Device with name "
			"%s already allocated!\n", name);
		return NULL;
	}

	if (odp_eth_dev_data == NULL)
		odp_eth_dev_data_alloc();

	eth_dev = &odp_eth_devices[port_id];
	eth_dev->data = &odp_eth_dev_data[port_id];
	snprintf(eth_dev->data->name,
		 sizeof(eth_dev->data->name), "%s", name);
	eth_dev->attached = DEV_ATTACHED;
	switch (type) {
	case	ODP_ETH_NIC_SOC:
		eth_dev->dev_type = ODP_ETH_DEV_SOC;
	break;
	case	ODP_ETH_DEV_UNKNOWN:
		eth_dev->dev_type = ODP_ETH_DEV_UNKNOWN;
	break;
	default:
		eth_dev->dev_type = ODP_ETH_DEV_PCI;
	break;
	}
		eth_dev->data->port_id = port_id;
		nb_ports++;

	return eth_dev;
}

static inline int odp_eth_dev_create_unique_device_name(
	char		      *name,
	size_t		       size,
	struct odp_pci_device *pci_dev)
{
	int ret;

	if ((name == NULL) || (pci_dev == NULL))
		return -EINVAL;

	ret = snprintf(name, size, "%d:%d.%d",
		       pci_dev->addr.bus, pci_dev->addr.devid,
		       pci_dev->addr.function);
	if (ret < 0)
		return ret;

	return 0;
}

int odp_eth_dev_release_port(struct odp_eth_dev *eth_dev)
{
	if (eth_dev == NULL)
		return -EINVAL;

	eth_dev->attached = DEV_DETACHED;
	nb_ports--;
	return 0;
}

static int odp_eth_dev_init(struct odp_pci_driver *pci_drv,
			    struct odp_pci_device *pci_dev)
{
	int diag;
	struct eth_driver *eth_drv;
	struct odp_eth_dev *eth_dev;
	char ethdev_name[ODP_ETH_NAME_MAX_LEN];

	eth_drv = (struct eth_driver *)pci_drv;

	/* Create unique Ethernet device name using PCI address */
	odp_eth_dev_create_unique_device_name(ethdev_name,
					      sizeof(ethdev_name),
					      pci_dev);

	eth_dev =
		odp_eth_dev_allocate(ethdev_name,
				     ODP_ETH_NIC_IXGBE);
	if (eth_dev == NULL)
		return -ENOMEM;

	if (odp_process_type() == ODP_PROC_PRIMARY) {
		eth_dev->data->dev_private =
			odp_zmalloc("dev_priv",
				    eth_drv->dev_private_size,
				    0);
		if (eth_dev->data->dev_private == NULL)
			ODP_PRINT("Cannot allocate "
				  "mm_district for private port data\n");
	}

	eth_dev->pci_dev = pci_dev;
	eth_dev->driver	 = eth_drv;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	/* init user callbacks */
	TAILQ_INIT(&eth_dev->link_intr_cbs);

	/*
	 * Set the default MTU.
	 */
	eth_dev->data->mtu = ODP_ETHER_MTU;

	/* Invoke UMD device initialization function */
	diag = (*eth_drv->eth_dev_init)(eth_dev);
	if (diag == 0)
		return 0;

	ODP_ERR("driver %s: eth_dev_init("
		"vendor_id=0x%u device_id=0x%x)"
		" failed\n", pci_drv->name,
		(unsigned)pci_dev->id.vendor_id,
		(unsigned)pci_dev->id.device_id);
	if (odp_process_type() == ODP_PROC_PRIMARY)
		free(eth_dev->data->dev_private);

	eth_dev->attached = DEV_DETACHED;
	nb_ports--;
	return diag;
}

static int odp_eth_dev_uninit(struct odp_pci_device *pci_dev)
{
	int ret;
	const struct eth_driver *eth_drv;
	struct odp_eth_dev *eth_dev;
	char ethdev_name[ODP_ETH_NAME_MAX_LEN];

	if (pci_dev == NULL)
		return -EINVAL;

	/* Create unique Ethernet device name using PCI address */
	odp_eth_dev_create_unique_device_name(ethdev_name,
					      sizeof(ethdev_name),
					      pci_dev);

	eth_dev = odp_eth_dev_allocated(ethdev_name);
	if (eth_dev == NULL)
		return -ENODEV;

	eth_drv = (const struct eth_driver *)pci_dev->driver;

	/* Invoke UMD device uninit function */
	if (*eth_drv->eth_dev_uninit) {
		ret = (*eth_drv->eth_dev_uninit)(eth_dev);
		if (ret)
			return ret;
	}

	/* free ether device */
	odp_eth_dev_release_port(eth_dev);

	if (odp_process_type() == ODP_PROC_PRIMARY)
		free(eth_dev->data->dev_private);

	eth_dev->pci_dev = NULL;
	eth_dev->driver	 = NULL;
	eth_dev->data = NULL;

	return 0;
}

/**
 * Register an Ethernet [Poll Mode] driver.
 *
 * Function invoked by the initialization function of an Ethernet driver
 * to simultaneously register itself as a PCI driver and as an Ethernet
 * Poll Mode Driver.
 * Invokes the odp_pci_register() function to register the *pci_drv*
 * structure embedded in the *eth_drv* structure, after having stored the
 * address of the odp_eth_dev_init() function in the *devinit* field of
 * the *pci_drv* structure.
 * During the PCI probing phase, the odp_eth_dev_init() function is
 * invoked for each PCI [Ethernet device] matching the embedded PCI
 * identifiers provided by the driver.
 */
void odp_eth_driver_register(struct eth_driver *eth_drv)
{
	eth_drv->pci_drv.devinit = odp_eth_dev_init;
	eth_drv->pci_drv.devuninit = odp_eth_dev_uninit;
	odp_pci_register(&eth_drv->pci_drv);
}

int odp_eth_dev_is_valid_port(uint8_t port_id)
{
	if ((port_id >= ODP_MAX_ETHPORTS) ||
	    (odp_eth_devices[port_id].attached != DEV_ATTACHED))
		return 0;
	else
		return 1;
}

int odp_eth_dev_socket_id(uint8_t port_id)
{
	if (!odp_eth_dev_is_valid_port(port_id))
		return -1;

	return odp_eth_devices[port_id].pci_dev->numa_node;
}

uint8_t odp_eth_dev_count(void)
{
	return nb_ports;
}

/* So far, ODP hotplug function only supports linux */
#ifdef ODP_LIBODP_HOTPLUG
static enum odp_eth_dev_type odp_eth_dev_get_device_type(
	uint8_t port_id)
{
	if (!odp_eth_dev_is_valid_port(port_id))
		return ODP_ETH_DEV_UNKNOWN;

	return odp_eth_devices[port_id].dev_type;
}

static int odp_eth_dev_save(struct odp_eth_dev *devs,
			    size_t		size)
{
	if (!devs ||
	    (size !=
	     sizeof(struct odp_eth_dev) * ODP_MAX_ETHPORTS))
		return -EINVAL;

	/* save current odp_eth_devices */
	memcpy(devs, odp_eth_devices, size);
	return 0;
}

static int odp_eth_dev_get_changed_port(struct odp_eth_dev *
					devs,
					uint8_t            *
					port_id)
{
	if ((!devs) || (!port_id))
		return -EINVAL;

	/* check which port was attached or detached */
	for (*port_id = 0; *port_id < ODP_MAX_ETHPORTS;
	     (*port_id)++, devs++)
		if (odp_eth_devices[*port_id].attached ^
		    devs->attached)
			return 0;

	return -ENODEV;
}

static int odp_eth_dev_get_addr_by_port(uint8_t port_id,
					struct odp_pci_addr
					       *addr)
{
	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (addr == NULL) {
		ODP_ERR("Null pointer is specified\n");
		return -EINVAL;
	}

	*addr = odp_eth_devices[port_id].pci_dev->addr;
	return 0;
}

static int odp_eth_dev_get_name_by_port(uint8_t port_id,
					char   *name)
{
	char *tmp;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (name == NULL) {
		ODP_ERR("Null pointer is specified\n");
		return -EINVAL;
	}

	/* shouldn't check 'odp_eth_devices[i].data',
	 * because it might be overwritten by VDEV UMD */
	tmp = odp_eth_dev_data[port_id].name;
	strcpy(name, tmp);
	return 0;
}

static int odp_eth_dev_is_detachable(uint8_t port_id)
{
	uint32_t drv_flags;

	if (port_id >= ODP_MAX_ETHPORTS) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (odp_eth_devices[port_id].dev_type ==
	    ODP_ETH_DEV_PCI)
		switch (odp_eth_devices[port_id].pci_dev->kdrv)	{
		case ODP_KDRV_IGB_UIO:
		case ODP_KDRV_UIO_GENERIC:
			break;
		case ODP_KDRV_VFIO:
		default:
			return -ENOTSUP;
		}

	drv_flags =
		odp_eth_devices[port_id].driver->pci_drv.
		drv_flags;
	return !(drv_flags & ODP_PCI_DRV_DETACHABLE);
}

/* attach the new physical device, then store port_id of the device */
static int odp_eth_dev_attach_pdev(struct odp_pci_addr *addr,
				   uint8_t             *
				   port_id)
{
	uint8_t new_port_id;
	struct odp_eth_dev devs[ODP_MAX_ETHPORTS];

	if ((addr == NULL) || (port_id == NULL))
		goto err;

	/* save current port status */
	if (odp_eth_dev_save(devs, sizeof(devs)))
		goto err;

	/* re-construct pci_device_list */
	if (odp_pci_scan())
		goto err;

	/* invoke probe func of the driver can handle the new device.
	 * TODO:
	 * odp_pci_probe_one() should return port_id.
	 * And odp_eth_dev_save() and odp_eth_dev_get_changed_port()
	 * should be removed. */
	if (odp_pci_probe_one(addr))
		goto err;

	/* get port_id enabled by above procedures */
	if (odp_eth_dev_get_changed_port(devs, &new_port_id))
		goto err;

	*port_id = new_port_id;
	return 0;
err:
	ODP_PRINT("Driver, cannot attach the device\n");
	return -1;
}

/* detach the new physical device, then store pci_addr of the device */
static int odp_eth_dev_detach_pdev(uint8_t		port_id,
				   struct odp_pci_addr *addr)
{
	struct odp_pci_addr freed_addr;
	struct odp_pci_addr vp;

	if (addr == NULL)
		goto err;

	/* check whether the driver supports detach feature, or not */
	if (odp_eth_dev_is_detachable(port_id))
		goto err;

	/* get pci address by port id */
	if (odp_eth_dev_get_addr_by_port(port_id, &freed_addr))
		goto err;

	/* Zerod pci addr means the port comes from virtual device */
	vp.domain = 0;
	vp.bus = 0;
	vp.devid = 0;
	vp.function = 0;
	if (odp_compare_pci_addr(&vp, &freed_addr) == 0)
		goto err;

	/* invoke close func of the driver,
	 * also remove the device from pci_device_list */
	if (odp_pci_close_one(&freed_addr))
		goto err;

	*addr = freed_addr;
	return 0;
err:
	ODP_PRINT("Driver, cannot detach the device\n");
	return -1;
}

/* attach the new virtual device, then store port_id of the device */
static int odp_eth_dev_attach_vdev(const char *vdevargs,
				   uint8_t    *port_id)
{
	int ret = -1;
	uint8_t new_port_id;
	char   *name = NULL, *args = NULL;
	struct odp_eth_dev devs[ODP_MAX_ETHPORTS];

	if ((vdevargs == NULL) || (port_id == NULL))
		goto end;

	/* parse vdevargs, then retrieve device name and args */
	if (odp_parse_devargs_str(vdevargs, &name, &args))
		goto end;

	/* save current port status */
	if (odp_eth_dev_save(devs, sizeof(devs)))
		goto end;

	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke probe function o the driver.
	 * TODO:
	 * odp_vdev_init() should return port_id,
	 * And odp_eth_dev_save() and odp_eth_dev_get_changed_port()
	 * should be removed. */
	if (odp_vdev_init(name, args))
		goto end;

	/* get port_id enabled by above procedures */
	if (odp_eth_dev_get_changed_port(devs, &new_port_id))
		goto end;

	ret = 0;
	*port_id = new_port_id;
end:
	if (args)
		free(args);

	if (name)
		free(name);

	if (ret < 0)
		ODP_PRINT("Driver, cannot attach the device\n");

	return ret;
}

/* detach the new virtual device, then store the name of the device */
static int odp_eth_dev_detach_vdev(uint8_t port_id,
				   char	  *vdevname)
{
	char name[ODP_ETH_NAME_MAX_LEN];

	if (vdevname == NULL)
		goto err;

	/* check whether the driver supports detach feature, or not */
	if (odp_eth_dev_is_detachable(port_id))
		goto err;

	/* get device name by port id */
	if (odp_eth_dev_get_name_by_port(port_id, name))
		goto err;

	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke close function o the driver */
	if (odp_vdev_uninit(name))
		goto err;

	strncpy(vdevname, name, sizeof(name));
	return 0;
err:
	ODP_PRINT("Driver, cannot detach the device\n");
	return -1;
}

/* attach the new device, then store port_id of the device */
int odp_eth_dev_attach(const char *devargs,
		       uint8_t	  *port_id)
{
	struct odp_pci_addr addr;

	if ((devargs == NULL) || (port_id == NULL))
		return -EINVAL;

	if (odp_parse_pci_dombdf(devargs, &addr) == 0)
		return odp_eth_dev_attach_pdev(&addr, port_id);
	else
		return odp_eth_dev_attach_vdev(devargs,
					       port_id);
}

/* detach the device, then store the name of the device */
int odp_eth_dev_detach(uint8_t port_id, char *name)
{
	struct odp_pci_addr addr;
	int ret;

	if (name == NULL)
		return -EINVAL;

	if (odp_eth_dev_get_device_type(port_id) ==
	    ODP_ETH_DEV_PCI) {
		ret =
			odp_eth_dev_get_addr_by_port(port_id,
						     &addr);
		if (ret < 0)
			return ret;

		ret = odp_eth_dev_detach_pdev(port_id, &addr);
		if (ret == 0)
			snprintf(name, ODP_ETH_NAME_MAX_LEN,
				 "%04x:%02x:%02x.%d",
				 addr.domain, addr.bus,
				 addr.devid, addr.function);

		return ret;
	} else {
		return odp_eth_dev_detach_vdev(port_id, name);
	}
}

#else           /* ODP_LIBODP_HOTPLUG */
int odp_eth_dev_attach(const char *devargs __odp_unused,
		       uint8_t *port_id	   __odp_unused)
{
	ODP_PRINT("Hotplug support isn't enabled\n");
	return -1;
}

/* detach the device, then store the name of the device */
int odp_eth_dev_detach(uint8_t port_id __odp_unused,
		       char *name      __odp_unused)
{
	ODP_PRINT("Hotplug support isn't enabled\n");
	return -1;
}
#endif          /* ODP_LIBODP_HOTPLUG */

static int odp_eth_dev_rx_queue_config(struct odp_eth_dev *
				       dev,
				       uint16_t nb_queues)
{
	void **rxq;
	unsigned i;
	uint16_t old_nb_queues = dev->data->nb_rx_queues;

	if (dev->data->rx_queues == NULL) { /* first time configuration */
		dev->data->rx_queues =
			odp_zmalloc("rx_que",
				    sizeof(dev->data->rx_queues[
						   0]) *
				    nb_queues, 0);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else {                 /* re-configure */
		if (*dev->dev_ops->rx_queue_release == NULL) {
			printf( "%s:%d function not supported",
				    __func__, __LINE__);
			return -ENOTSUP;
		}

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);

		rxq =
			odp_zmalloc("rxq",
				    sizeof(rxq[0]) * nb_queues,
				    0);
		if (rxq == NULL)
			return -(ENOMEM);

		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues -
					  old_nb_queues;

			memset(rxq + old_nb_queues, 0,
			       sizeof(rxq[0]) * new_qs);
		}

		dev->data->rx_queues = rxq;
	}

	dev->data->nb_rx_queues = nb_queues;

	return 0;
}

int odp_eth_dev_rx_queue_start(uint8_t	port_id,
			       uint16_t rx_queue_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY);*/

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		ODP_ERR("Invalid RX queue_id=%d\n",
			rx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->rx_queue_start == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return dev->dev_ops->rx_queue_start(dev, rx_queue_id);
}

int odp_eth_dev_rx_queue_stop(uint8_t  port_id,
			      uint16_t rx_queue_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		ODP_ERR("Invalid RX queue_id=%d\n",
			rx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->rx_queue_stop == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return dev->dev_ops->rx_queue_stop(dev, rx_queue_id);
}

int odp_eth_dev_tx_queue_start(uint8_t	port_id,
			       uint16_t tx_queue_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		ODP_ERR("Invalid TX queue_id=%d\n",
			tx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->tx_queue_start == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return dev->dev_ops->tx_queue_start(dev, tx_queue_id);
}

int odp_eth_dev_tx_queue_stop(uint8_t  port_id,
			      uint16_t tx_queue_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		ODP_ERR("Invalid TX queue_id=%d\n",
			tx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->tx_queue_stop == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return dev->dev_ops->tx_queue_stop(dev, tx_queue_id);
}

static int odp_eth_dev_tx_queue_config(struct odp_eth_dev *
				       dev,
				       uint16_t nb_queues)
{
	void **txq;
	unsigned i;
	uint16_t old_nb_queues = dev->data->nb_tx_queues;

	if (dev->data->tx_queues == NULL) { /* first time configuration */
		dev->data->tx_queues =
			odp_zmalloc("tx_que",
				    sizeof(dev->data->tx_queues[
						   0]) *
				    nb_queues, 0);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else {                 /* re-configure */
		if (*dev->dev_ops->tx_queue_release == NULL) {
			printf( "%s:%d function not supported",
				    __func__, __LINE__);
			return -ENOTSUP;
		}

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);

		txq =
			odp_zmalloc("txq",
				    sizeof(txq[0]) * nb_queues,
				    0);
		if (txq == NULL)
			return -ENOMEM;

		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues -
					  old_nb_queues;

			memset(txq + old_nb_queues, 0,
			       sizeof(txq[0]) * new_qs);
		}

		dev->data->tx_queues = txq;
	}

	dev->data->nb_tx_queues = nb_queues;
	return 0;
}

static int odp_eth_dev_check_vf_rss_rxq_num(uint8_t  port_id,
					    uint16_t nb_rx_q)
{
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];

	switch (nb_rx_q) {
	case 1:
	case 2:
		ODP_ETH_DEV_SRIOV(dev).active =
			ETH_64_POOLS;
		break;
	case 4:
		ODP_ETH_DEV_SRIOV(dev).active =
			ETH_32_POOLS;
		break;
	default:
		return -EINVAL;
	}

	ODP_ETH_DEV_SRIOV(dev).nb_q_per_pool  = nb_rx_q;
	ODP_ETH_DEV_SRIOV(dev).def_pool_q_idx =
		dev->pci_dev->max_vfs * nb_rx_q;

	return 0;
}

static int odp_eth_dev_check_mq_mode(uint8_t	   port_id,
				     uint16_t	   nb_rx_q,
				     uint16_t	   nb_tx_q,
				const struct odp_eth_conf *dev_conf)
{
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];

	if (ODP_ETH_DEV_SRIOV(dev).active != 0) {
		/* check multi-queue mode */
		if ((dev_conf->rxmode.mq_mode ==
		     ETH_MQ_RX_DCB) ||
		    (dev_conf->rxmode.mq_mode ==
		     ETH_MQ_RX_DCB_RSS) ||
		    (dev_conf->txmode.mq_mode ==
		     ETH_MQ_TX_DCB)) {
			/* SRIOV only works in VMDq enable mode */
			ODP_ERR("ethdev port_id=%u"
				" SRIOV active, "
				"wrong VMDQ mq_mode rx %u tx %u\n",
				port_id,
				dev_conf->rxmode.mq_mode,
				dev_conf->txmode.mq_mode);
			return (-EINVAL);
		}

		switch (dev_conf->rxmode.mq_mode) {
		case ETH_MQ_RX_VMDQ_DCB:
		case ETH_MQ_RX_VMDQ_DCB_RSS:

			/* DCB/RSS VMDQ in SRIOV mode, not implement yet */
			ODP_ERR("ethdev port_id=%u"
				" SRIOV active, "
				"unsupported VMDQ mq_mode rx %u\n",
				port_id,
				dev_conf->rxmode.mq_mode);
			return (-EINVAL);
		case ETH_MQ_RX_VMDQ_RSS:
			dev->data->dev_conf.rxmode.mq_mode =
				ETH_MQ_RX_VMDQ_RSS;
			if (nb_rx_q <=
			    ODP_ETH_DEV_SRIOV(dev).nb_q_per_pool)
				if (
					odp_eth_dev_check_vf_rss_rxq_num(
						port_id,
						nb_rx_q)
					!=
					0) {
					ODP_ERR(
						"ethdev port_id=%d"
						" SRIOV active, invalid queue"
						" number for VMDQ RSS, allowed"
						" value are 1, 2 or 4\n",
						port_id);
					return -EINVAL;
				}

			break;
		case ETH_MQ_RX_RSS:
			ODP_ERR("ethdev port_id=%u"
				" SRIOV active, "
				"Rx mq mode is changed from:"
				"mq_mode %u into VMDQ mq_mode %u\n",
				port_id,
				dev_conf->rxmode.mq_mode,
				dev->data->dev_conf.rxmode.
				mq_mode);

		default:  /* ETH_MQ_RX_VMDQ_ONLY or ETH_MQ_RX_NONE */
			  /* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.rxmode.mq_mode =
				ETH_MQ_RX_VMDQ_ONLY;
			if (ODP_ETH_DEV_SRIOV(dev).nb_q_per_pool
			    > 1)
				ODP_ETH_DEV_SRIOV(dev).
				nb_q_per_pool = 1;

			break;
		}

		switch (dev_conf->txmode.mq_mode) {
		case ETH_MQ_TX_VMDQ_DCB:

			/* DCB VMDQ in SRIOV mode, not implement yet */
			ODP_ERR("ethdev port_id=%u"
				" SRIOV active, "
				"unsupported VMDQ mq_mode tx %u\n",
				port_id,
				dev_conf->txmode.mq_mode);
			return (-EINVAL);
		default: /* ETH_MQ_TX_VMDQ_ONLY or ETH_MQ_TX_NONE */
			/* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.txmode.mq_mode =
				ETH_MQ_TX_VMDQ_ONLY;
			break;
		}

		/* check valid queue number */
		if ((nb_rx_q >
		     ODP_ETH_DEV_SRIOV(dev).nb_q_per_pool) ||
		    (nb_tx_q >
		     ODP_ETH_DEV_SRIOV(dev).nb_q_per_pool)) {
			ODP_ERR(
				"ethdev port_id=%d SRIOV active, "
				"queue number must less equal to %d\n",
				port_id,
				ODP_ETH_DEV_SRIOV(dev).
				nb_q_per_pool);
			return (-EINVAL);
		}
	} else {
		/* For vmdb+dcb mode check our configuration
		 * before we go further */
		if (dev_conf->rxmode.mq_mode ==
		    ETH_MQ_RX_VMDQ_DCB) {
			const struct odp_eth_vmdq_dcb_conf *conf;

			if (nb_rx_q !=
			    ETH_VMDQ_DCB_NUM_QUEUES) {
				ODP_ERR("ethdev port_id=%d "
					"VMDQ+DCB, nb_rx_q "
					"!= %d\n",
					port_id,
					ETH_VMDQ_DCB_NUM_QUEUES);
				return (-EINVAL);
			}

			conf =
				&dev_conf->rx_adv_conf.
				vmdq_dcb_conf;
			if (!((conf->nb_queue_pools ==
			       ETH_16_POOLS) ||
			      (conf->nb_queue_pools ==
			       ETH_32_POOLS))) {
				ODP_ERR("ethdev port_id=%d "
					"VMDQ+DCB selected, "
					"nb_queue_pools must be %d or %d\n",
					port_id,
					ETH_16_POOLS,
					ETH_32_POOLS);
				return (-EINVAL);
			}
		}

		if (dev_conf->txmode.mq_mode ==
		    ETH_MQ_TX_VMDQ_DCB) {
			const struct odp_eth_vmdq_dcb_tx_conf *
				conf;

			if (nb_tx_q !=
			    ETH_VMDQ_DCB_NUM_QUEUES) {
				ODP_ERR("ethdev port_id=%d "
					"VMDQ+DCB, nb_tx_q "
					"!= %d\n",
					port_id,
					ETH_VMDQ_DCB_NUM_QUEUES);
				return (-EINVAL);
			}

			conf =
				&dev_conf->tx_adv_conf.
				vmdq_dcb_tx_conf;
			if (!((conf->nb_queue_pools ==
			       ETH_16_POOLS) ||
			      (conf->nb_queue_pools ==
			       ETH_32_POOLS))) {
				ODP_ERR("ethdev port_id=%d "
					"VMDQ+DCB selected, "
					"nb_queue_pools != %d "
					"or nb_queue_pools "
					"!= %d\n",
					port_id, ETH_16_POOLS,
					ETH_32_POOLS);
				return (-EINVAL);
			}
		}

		/* For DCB mode check our configuration before we go further */
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_DCB) {
			const struct odp_eth_dcb_rx_conf *conf;

			if (nb_rx_q != ETH_DCB_NUM_QUEUES) {
				ODP_ERR("ethdev port_id=%d "
					"DCB, nb_rx_q "
					"!= %d\n",
					port_id,
					ETH_DCB_NUM_QUEUES);
				return (-EINVAL);
			}

			conf =
				&dev_conf->rx_adv_conf.
				dcb_rx_conf;
			if (!((conf->nb_tcs == ETH_4_TCS) ||
			      (conf->nb_tcs == ETH_8_TCS))) {
				ODP_ERR("ethdev port_id=%d "
					"DCB selected, "
					"nb_tcs != %d or nb_tcs "
					"!= %d\n",
					port_id,
					ETH_4_TCS, ETH_8_TCS);
				return (-EINVAL);
			}
		}

		if (dev_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
			const struct odp_eth_dcb_tx_conf *conf;

			if (nb_tx_q != ETH_DCB_NUM_QUEUES) {
				ODP_ERR("ethdev port_id=%d "
					"DCB, nb_tx_q "
					"!= %d\n",
					port_id,
					ETH_DCB_NUM_QUEUES);
				return (-EINVAL);
			}

			conf =
				&dev_conf->tx_adv_conf.
				dcb_tx_conf;
			if (!((conf->nb_tcs == ETH_4_TCS) ||
			      (conf->nb_tcs == ETH_8_TCS))) {
				ODP_ERR("ethdev port_id=%d "
					"DCB selected, "
					"nb_tcs != %d or nb_tcs "
					"!= %d\n",
					port_id, ETH_4_TCS,
					ETH_8_TCS);
				return (-EINVAL);
			}
		}
	}

	return 0;
}

int odp_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_q,
			  uint16_t nb_tx_q,
			  const struct odp_eth_conf *
			  dev_conf)
{
	int diag;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	if (nb_tx_q > ODP_MAX_QUEUES_PER_PORT) {
		ODP_ERR(
			"Number of TX queues requested (%u) is "
			"greater than max supported(%d)\n",
			nb_tx_q, ODP_MAX_QUEUES_PER_PORT);
		return (-EINVAL);
	}

	if (nb_rx_q > ODP_MAX_QUEUES_PER_PORT) {
		ODP_ERR(
			"Number of RX queues requested (%u) is "
			"greater than max supported(%d)\n",
			nb_rx_q, ODP_MAX_QUEUES_PER_PORT);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];

	if ((*dev->dev_ops->dev_infos_get == NULL) ||
		(*dev->dev_ops->dev_configure == NULL)) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		ODP_ERR(
			"port %d must be stopped to allow configuration\n",
			port_id);
		return (-EBUSY);
	}

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);
	if (nb_rx_q == 0) {
		ODP_ERR("ethdev port_id=%d nb_rx_q == 0\n",
			port_id);
		return (-EINVAL);
	}

	if (nb_rx_q > dev_info.max_rx_queues) {
		ODP_ERR(
			"ethdev port_id=%d nb_rx_queues=%d > %d\n",
			port_id, nb_rx_q,
			dev_info.max_rx_queues);
		return (-EINVAL);
	}

	if (nb_tx_q == 0) {
		ODP_ERR("ethdev port_id=%d nb_tx_q == 0\n",
			port_id);
		return (-EINVAL);
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		ODP_ERR(
			"ethdev port_id=%d nb_tx_queues=%d > %d\n",
			port_id, nb_tx_q,
			dev_info.max_tx_queues);
		return (-EINVAL);
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, dev_conf,
	       sizeof(dev->data->dev_conf));

	/*
	 * If link state interrupt is enabled, check that the
	 * device supports it.
	 */
	if (dev_conf->intr_conf.lsc == 1) {
		const struct odp_pci_driver *pci_drv =
			&dev->driver->pci_drv;

		if (!(pci_drv->drv_flags &
		      ODP_PCI_DRV_INTR_LSC)) {
			ODP_ERR(
				"driver %s does not support lsc\n",
				pci_drv->name);
			return (-EINVAL);
		}
	}

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (dev_conf->rxmode.jumbo_frame == 1) {
		if (dev_conf->rxmode.max_rx_pkt_len >
		    dev_info.max_rx_pktlen) {
			ODP_ERR(
				"ethdev port_id=%d max_rx_pkt_len %u"
				" > max valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.
				max_rx_pkt_len,
				(unsigned)dev_info.max_rx_pktlen);
			return (-EINVAL);
		} else if (dev_conf->rxmode.max_rx_pkt_len <
			   ODP_ETHER_MIN_LEN) {
			ODP_ERR(
				"ethdev port_id=%d max_rx_pkt_len %u"
				" < min valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.
				max_rx_pkt_len,
				(unsigned)ODP_ETHER_MIN_LEN);
			return (-EINVAL);
		}
	} else if ((dev_conf->rxmode.max_rx_pkt_len <
		    ODP_ETHER_MIN_LEN) ||
		   (dev_conf->rxmode.max_rx_pkt_len >
		    ODP_ETHER_MAX_LEN)) {
		/* Use default value */
		dev->data->dev_conf.rxmode.max_rx_pkt_len =
			ODP_ETHER_MAX_LEN;
	}

	/* multipe queue mode checking */
	diag =
		odp_eth_dev_check_mq_mode(port_id, nb_rx_q,
					  nb_tx_q,
					  dev_conf);
	if (diag != 0) {
		ODP_ERR(
			"port%d odp_eth_dev_check_mq_mode = %d\n",
			port_id, diag);
		return diag;
	}

	/*
	 * Setup new number of RX/TX queues and reconfigure device.
	 */
	diag = odp_eth_dev_tx_queue_config(dev, nb_tx_q);
	if (diag != 0) {
		ODP_ERR(
			"port%d odp_eth_dev_tx_queue_config = %d\n",
			port_id, diag);
		odp_eth_dev_rx_queue_config(dev, 0);
		return diag;
	}

	diag = odp_eth_dev_rx_queue_config(dev, nb_rx_q);
	if (diag != 0) {
		ODP_ERR(
			"port%d odp_eth_dev_rx_queue_config = %d\n",
			port_id, diag);
		return diag;
	}

	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		ODP_ERR("port%d dev_configure = %d\n",
			port_id, diag);
		odp_eth_dev_rx_queue_config(dev, 0);
		odp_eth_dev_tx_queue_config(dev, 0);
		return diag;
	}

	return 0;
}

static void odp_eth_dev_config_restore(uint8_t port_id)
{
	uint16_t i;
	uint32_t pool = 0;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;
	struct odp_ether_addr	addr;

	dev = &odp_eth_devices[port_id];

	odp_eth_dev_info_get(port_id, &dev_info);

	if (ODP_ETH_DEV_SRIOV(dev).active)
		pool = ODP_ETH_DEV_SRIOV(dev).def_vmdq_idx;

	/* replay MAC address configuration */
	for (i = 0; i < dev_info.max_mac_addrs; i++) {
		addr = dev->data->mac_addrs[i];

		/* skip zero address */
		if (is_zero_ether_addr(&addr))
			continue;

		/* add address to the hardware */
		if (*dev->dev_ops->mac_addr_add &&
		    (dev->data->mac_pool_sel[i] &
		     (1ULL << pool))) {
			(*dev->dev_ops->mac_addr_add)(dev,
						      &addr, i,
						      pool);
		} else {
			ODP_ERR("port %d: MAC address "
				"array not supported\n",
				port_id);

			/* exit the loop but not return an error */
			break;
		}
	}

	/* replay allmulticast configuration */
	if (odp_eth_allmulticast_get(port_id) == 1)
		odp_eth_allmulticast_enable(port_id);
	else if (odp_eth_allmulticast_get(port_id) == 0)
		odp_eth_allmulticast_disable(port_id);

	/* replay promiscuous configuration */
	if (odp_eth_promiscuous_get(port_id) == 1)
		odp_eth_promiscuous_enable(port_id);
	else if (odp_eth_promiscuous_get(port_id) == 0)
		odp_eth_promiscuous_disable(port_id);
}

int odp_eth_dev_start(uint8_t port_id)
{
	int diag;
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];
	if (*dev->dev_ops->dev_start == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	if (dev->data->dev_started != 0) {
		ODP_ERR(
			"Device with port_id=%u already started\n",
			port_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	odp_eth_dev_config_restore(port_id);

	if (dev->data->dev_conf.intr_conf.lsc != 0) {
		if (*dev->dev_ops->link_update == NULL) {
			printf( "%s:%d function not supported",
				    __func__, __LINE__);
			return -ENOTSUP;
		}
		(*dev->dev_ops->link_update)(dev, 0);
	}

	return 0;
}

void odp_eth_dev_stop(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_RET(); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_stop == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}

	if (dev->data->dev_started == 0) {
		ODP_ERR("Device with port_id=%u"
			" already stopped\n",
			port_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

int odp_eth_dev_set_link_up(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_set_link_up == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->dev_set_link_up)(dev);
}

int odp_eth_dev_set_link_down(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_set_link_down == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->dev_set_link_down)(dev);
}

void odp_eth_dev_close(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_RET(); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_close == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_close)(dev);
}

int odp_eth_rx_queue_setup(uint8_t	port_id,
			   uint16_t	rx_queue_id,
			   uint16_t	nb_rx_desc,
			   unsigned int socket_id,
			   const struct odp_eth_rxconf *
			   rx_conf,
			   void	       *mp)
{
	int ret;

	/* uint32_t mbp_buf_size; */
	struct odp_eth_dev *dev;

	/* struct odp_pktmbuf_pool_private *mbp_priv; */
	struct odp_eth_dev_info dev_info;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		ODP_ERR("Invalid RX queue_id=%d\n",
			rx_queue_id);
		return (-EINVAL);
	}

	if (dev->data->dev_started) {
		ODP_ERR(
			"port %d must be stopped to allow configuration\n",
			port_id);
		return -EBUSY;
	}

	if (*dev->dev_ops->rx_queue_setup == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	/*
	 * Check the size of the mbuf data buffer.
	 * This value must be provided in the private data of the memory pool.
	 * First check that the memory pool has a valid private data.
	 */
	odp_eth_dev_info_get(port_id, &dev_info);

	if (rx_conf == NULL)
		rx_conf = &dev_info.default_rxconf;

	ret =
		(*dev->dev_ops->rx_queue_setup)(dev,
						rx_queue_id,
						nb_rx_desc,
						socket_id,
						rx_conf, mp);

	return ret;
}

int odp_eth_tx_queue_setup(uint8_t port_id,
			   uint16_t tx_queue_id,
			   uint16_t nb_tx_desc,
			   unsigned int socket_id,
			   const struct odp_eth_txconf *
			   tx_conf, void *mp)
{
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/

	/* PROC_PRIMARY_OR_ERR_RET(-E_ODP_SECONDARY); */

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		ODP_ERR("Invalid TX queue_id=%d\n",
			tx_queue_id);
		return (-EINVAL);
	}

	if (dev->data->dev_started) {
		ODP_ERR(
			"port %d must be stopped to allow configuration\n",
			port_id);
		return -EBUSY;
	}

	if (*dev->dev_ops->tx_queue_setup == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	odp_eth_dev_info_get(port_id, &dev_info);

	if (tx_conf == NULL)
		tx_conf = &dev_info.default_txconf;

	return (*dev->dev_ops->tx_queue_setup)(dev, tx_queue_id,
					       nb_tx_desc,
					       socket_id,
					       tx_conf, mp);
}

void odp_eth_promiscuous_enable(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->promiscuous_enable == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	(*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = 1;
}

void odp_eth_promiscuous_disable(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->promiscuous_disable == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	dev->data->promiscuous = 0;
	(*dev->dev_ops->promiscuous_disable)(dev);
}

int odp_eth_promiscuous_get(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &odp_eth_devices[port_id];
	return dev->data->promiscuous;
}

void odp_eth_allmulticast_enable(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->allmulticast_enable == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	(*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = 1;
}

void odp_eth_allmulticast_disable(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->allmulticast_disable == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	dev->data->all_multicast = 0;
	(*dev->dev_ops->allmulticast_disable)(dev);
}

int odp_eth_allmulticast_get(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &odp_eth_devices[port_id];
	return dev->data->all_multicast;
}

static inline int odp_eth_dev_atomic_read_link_status(struct
						      odp_eth_dev
						      * dev,
						      struct
						      odp_eth_link
						      * link)
{
	struct odp_eth_link *dst = link;
	struct odp_eth_link *src = &dev->data->dev_link;

	if (!odp_atomic_cmpset_u64_a64((odp_atomic_u64_t *)dst,
				       *(uint64_t *)dst,
				       *(uint64_t *)src))
		return -1;

	return 0;
}

void odp_eth_link_get(uint8_t		   port_id,
		      struct odp_eth_link *eth_link)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0) {
		odp_eth_dev_atomic_read_link_status(dev,
						    eth_link);
	} else {
		if (*dev->dev_ops->link_update == NULL) {
			printf( "%s:%d function not supported",
				    __func__, __LINE__);
			return;
		}
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}
}

void odp_eth_link_get_nowait(uint8_t		  port_id,
			     struct odp_eth_link *eth_link)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0) {
		odp_eth_dev_atomic_read_link_status(dev,
						    eth_link);
	} else {
		if (*dev->dev_ops->link_update == NULL) {
			printf( "%s:%d function not supported",
				    __func__, __LINE__);
			return;
		}
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}
}

int odp_eth_stats_get(uint8_t		    port_id,
		      struct odp_eth_stats *stats)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	memset(stats, 0, sizeof(*stats));

	if (*dev->dev_ops->stats_get == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->stats_get)(dev, stats);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	return 0;
}

void odp_eth_stats_reset(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->stats_reset == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	(*dev->dev_ops->stats_reset)(dev);
}

/* retrieve ethdev extended statistics */
int odp_eth_xstats_get(uint8_t		      port_id,
		       struct odp_eth_xstats *xstats,
		       unsigned		      n)
{
	unsigned count, i, q;
	uint64_t val;
	char *stats_ptr;
	struct odp_eth_stats eth_stats;
	struct odp_eth_dev  *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &odp_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL)
		return (*dev->dev_ops->xstats_get)(dev, xstats,
						   n);

	/* else, return generic statistics */
	count  = ODP_NB_STATS;
	count += dev->data->nb_rx_queues * ODP_NB_RXQ_STATS;
	count += dev->data->nb_tx_queues * ODP_NB_TXQ_STATS;
	if (n < count)
		return count;

	/* now fill the xstats structure */

	count = 0;
	memset(&eth_stats, 0, sizeof(eth_stats));
	odp_eth_stats_get(port_id, &eth_stats);

	/* global stats */
	for (i = 0; i < ODP_NB_STATS; i++) {
		stats_ptr = (char *)&eth_stats +
			    odp_stats_strings[i].offset;
		val = *(uint64_t *)stats_ptr;
		snprintf(xstats[count].name,
			 sizeof(xstats[count].name),
			 "%s", odp_stats_strings[i].name);
		xstats[count++].value = val;
	}

	/* per-txq stats */
	for (q = 0; q < dev->data->nb_tx_queues; q++)
		for (i = 0; i < ODP_NB_TXQ_STATS; i++) {
			stats_ptr  = (char *)&eth_stats;
			stats_ptr +=
				odp_txq_stats_strings[i].offset;
			stats_ptr += q * sizeof(uint64_t);
			val = *(uint64_t *)stats_ptr;
			snprintf(xstats[count].name,
				 sizeof(xstats[count].name),
				 "tx_queue_%u_%s", q,
				 odp_txq_stats_strings[i].name);
			xstats[count++].value = val;
		}

	/* per-rxq stats */
	for (q = 0; q < dev->data->nb_rx_queues; q++)
		for (i = 0; i < ODP_NB_RXQ_STATS; i++) {
			stats_ptr  = (char *)&eth_stats;
			stats_ptr +=
				odp_rxq_stats_strings[i].offset;
			stats_ptr += q * sizeof(uint64_t);
			val = *(uint64_t *)stats_ptr;
			snprintf(xstats[count].name,
				 sizeof(xstats[count].name),
				 "rx_queue_%u_%s", q,
				 odp_rxq_stats_strings[i].name);
			xstats[count++].value = val;
		}

	return count;
}

/* reset ethdev extended statistics */
void odp_eth_xstats_reset(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_reset != NULL) {
		(*dev->dev_ops->xstats_reset)(dev);
		return;
	}

	/* fallback to default */
	odp_eth_stats_reset(port_id);
}

static int set_queue_stats_mapping(uint8_t  port_id,
				   uint16_t queue_id,
				   uint8_t  stat_idx,
				   uint8_t  is_rx)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->queue_stats_mapping_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->queue_stats_mapping_set)
		       (dev, queue_id, stat_idx, is_rx);
}

int odp_eth_dev_set_tx_queue_stats_mapping(uint8_t port_id,
					   uint16_t
					   tx_queue_id,
					   uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, tx_queue_id,
				       stat_idx,
				       STAT_QMAP_TX);
}

int odp_eth_dev_set_rx_queue_stats_mapping(uint8_t port_id,
					   uint16_t
					   rx_queue_id,
					   uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, rx_queue_id,
				       stat_idx,
				       STAT_QMAP_RX);
}

void odp_eth_dev_info_get(uint8_t		   port_id,
			  struct odp_eth_dev_info *dev_info)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];

	memset(dev_info, 0, sizeof(struct odp_eth_dev_info));

	if (*dev->dev_ops->dev_infos_get == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return;
	}
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);
	dev_info->pci_dev = dev->pci_dev;
	if (dev->driver)
		dev_info->driver_name =
			dev->driver->pci_drv.name;
}

void odp_eth_get_macaddr(uint8_t		port_id,
			 struct odp_ether_addr *mac_addr)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &odp_eth_devices[port_id];
	ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);
}

int odp_eth_dev_get_mtu(uint8_t port_id, uint16_t *mtu)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev  = &odp_eth_devices[port_id];
	*mtu = dev->data->mtu;
	return 0;
}

int odp_eth_dev_set_mtu(uint8_t port_id, uint16_t mtu)
{
	int ret;
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (*dev->dev_ops->mtu_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (!ret)
		dev->data->mtu = mtu;

	return ret;
}

int odp_eth_dev_vlan_filter(uint8_t port_id,
			    uint16_t vlan_id, int on)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (vlan_id > 4095) {
		ODP_ERR(
			"(port_id=%d) invalid vlan_id=%u > 4095\n",
			port_id, (unsigned)vlan_id);
		return (-EINVAL);
	}

	if (!(dev->data->dev_conf.rxmode.hw_vlan_filter)) {
		ODP_ERR("port %d: vlan-filtering disabled\n",
			port_id);
		return (-ENODEV);
	}

	if (*dev->dev_ops->vlan_filter_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return (*dev->dev_ops->vlan_filter_set)(dev, vlan_id,
						on);
}

int odp_eth_dev_set_vlan_strip_on_queue(uint8_t	 port_id,
					uint16_t rx_queue_id,
					int	 on)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		ODP_ERR("Invalid rx_queue_id=%d\n", port_id);
		return (-EINVAL);
	}

	if (*dev->dev_ops->vlan_strip_queue_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id,
					      on);

	return 0;
}

int odp_eth_dev_set_vlan_ether_type(uint8_t  port_id,
				    uint16_t tpid)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->vlan_tpid_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->vlan_tpid_set)(dev, tpid);

	return 0;
}

int odp_eth_dev_set_vlan_offload(uint8_t port_id,
				 int	 offload_mask)
{
	struct odp_eth_dev *dev;
	int ret	 = 0;
	int mask = 0;
	int cur, org = 0;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	/*check which option changed by application*/
	cur = !!(offload_mask & ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_strip);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_strip =
			(uint8_t)cur;
		mask |= ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_extend);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_extend =
			(uint8_t)cur;
		mask |= ETH_VLAN_EXTEND_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_filter);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_filter =
			(uint8_t)cur;
		mask |= ETH_VLAN_FILTER_MASK;
	}

	/*no change*/
	if (mask == 0)
		return ret;

	if (*dev->dev_ops->vlan_offload_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->vlan_offload_set)(dev, mask);

	return ret;
}

int odp_eth_dev_get_vlan_offload(uint8_t port_id)
{
	struct odp_eth_dev *dev;
	int ret = 0;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.rxmode.hw_vlan_strip)
		ret |= ETH_VLAN_STRIP_OFFLOAD;

	if (dev->data->dev_conf.rxmode.hw_vlan_extend)
		ret |= ETH_VLAN_EXTEND_OFFLOAD;

	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		ret |= ETH_VLAN_FILTER_OFFLOAD;

	return ret;
}

int odp_eth_dev_set_vlan_pvid(uint8_t port_id,
			      uint16_t pvid, int on)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->vlan_pvid_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->vlan_pvid_set)(dev, pvid, on);

	return 0;
}

int odp_eth_dev_fdir_add_signature_filter(uint8_t port_id,
					  struct
					  odp_fdir_filter *
					  fdir_filter,
					  uint8_t queue)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_SIGNATURE) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return (-ENODEV);
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type, source & destinations ports "
			"should be null!\n");
		return (-EINVAL);
	}

	if (*dev->dev_ops->fdir_add_signature_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_add_signature_filter)(dev,
							  fdir_filter,
							  queue);
}

int odp_eth_dev_fdir_update_signature_filter(
	uint8_t			port_id,
	struct odp_fdir_filter *fdir_filter,
	uint8_t			queue)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_SIGNATURE) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return (-ENODEV);
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type, source & destinations ports "
			"should be null!\n");
		return (-EINVAL);
	}

	if (*dev->dev_ops->fdir_update_signature_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_update_signature_filter)(dev,
							     fdir_filter,
							     queue);
}

int odp_eth_dev_fdir_remove_signature_filter(
	uint8_t			port_id,
	struct odp_fdir_filter *fdir_filter)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_SIGNATURE) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return (-ENODEV);
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type source & destinations ports "
			"should be null!\n");
		return (-EINVAL);
	}

	if (*dev->dev_ops->fdir_remove_signature_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_remove_signature_filter)(dev,
							     fdir_filter);
}

int odp_eth_dev_fdir_get_infos(uint8_t		    port_id,
			       struct odp_eth_fdir *fdir)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (!(dev->data->dev_conf.fdir_conf.mode)) {
		ODP_ERR("port %d: pkt-filter disabled\n",
			port_id);
		return (-ENODEV);
	}

	if (*dev->dev_ops->fdir_infos_get == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	(*dev->dev_ops->fdir_infos_get)(dev, fdir);
	return 0;
}

int odp_eth_dev_fdir_add_perfect_filter(uint8_t	 port_id,
					struct
					odp_fdir_filter *
					fdir_filter,
					uint16_t soft_id,
					uint8_t	 queue,
					uint8_t	 drop)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_PERFECT) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return (-ENODEV);
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type, source & destinations ports "
			"should be null!\n");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == ODP_FDIR_IPTYPE_IPV6)
		return (-ENODEV);

	if (*dev->dev_ops->fdir_add_perfect_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_add_perfect_filter)(dev,
							fdir_filter,
							soft_id,
							queue,
							drop);
}

int odp_eth_dev_fdir_update_perfect_filter(uint8_t  port_id,
					   struct
					   odp_fdir_filter *
					   fdir_filter,
					   uint16_t soft_id,
					   uint8_t  queue,
					   uint8_t  drop)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_PERFECT) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return (-ENODEV);
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type, source & destinations ports "
			"should be null!\n");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == ODP_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	if (*dev->dev_ops->fdir_update_perfect_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_update_perfect_filter)(dev,
							   fdir_filter,
							   soft_id,
							   queue,
							   drop);
}

int odp_eth_dev_fdir_remove_perfect_filter(uint8_t  port_id,
					   struct
					   odp_fdir_filter *
					   fdir_filter,
					   uint16_t soft_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode !=
	    ODP_FDIR_MODE_PERFECT) {
		ODP_ERR("port %d: invalid FDIR mode=%u\n",
			port_id,
			dev->data->dev_conf.fdir_conf.mode);
		return -ENOTSUP;
	}

	if (((fdir_filter->l4type == ODP_FDIR_L4TYPE_SCTP) ||
	     (fdir_filter->l4type == ODP_FDIR_L4TYPE_NONE)) &&
	    (fdir_filter->port_src || fdir_filter->port_dst)) {
		ODP_ERR(" Port are meaningless for SCTP and "
			"None l4type, source & destinations ports "
			"should be null!\n");
		return -EINVAL;
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == ODP_FDIR_IPTYPE_IPV6)
		return -ENOTSUP;

	if (*dev->dev_ops->fdir_remove_perfect_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_remove_perfect_filter)(dev,
							   fdir_filter,
							   soft_id);
}

int odp_eth_dev_fdir_set_masks(uint8_t port_id,
			       struct odp_fdir_masks *
			       fdir_mask)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (!(dev->data->dev_conf.fdir_conf.mode)) {
		ODP_ERR("port %d: pkt-filter disabled\n",
			port_id);
		return (-ENODEV);
	}

	if (*dev->dev_ops->fdir_set_masks == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->fdir_set_masks)(dev, fdir_mask);
}

int odp_eth_dev_flow_ctrl_get(uint8_t port_id,
			      struct odp_eth_fc_conf *
			      fc_conf)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->flow_ctrl_get == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	memset(fc_conf, 0, sizeof(*fc_conf));
	return (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf);
}

int odp_eth_dev_flow_ctrl_set(uint8_t port_id,
			      struct odp_eth_fc_conf *
			      fc_conf)
{
	struct odp_eth_dev *dev;

	if ((fc_conf->send_xon != 0) &&
	    (fc_conf->send_xon != 1)) {
		ODP_ERR("Invalid send_xon, only 0/1 allowed\n");
		return (-EINVAL);
	}

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->flow_ctrl_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf);
}

int odp_eth_dev_priority_flow_ctrl_set(uint8_t port_id,
				       struct
				       odp_eth_pfc_conf *
				       pfc_conf)
{
	struct odp_eth_dev *dev;

	if (pfc_conf->priority >
	    (ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		ODP_ERR("Invalid priority, only 0-7 allowed\n");
		return (-EINVAL);
	}

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	/* High water, low water validation are device specific */
	if (*dev->dev_ops->priority_flow_ctrl_set)
		return (*dev->dev_ops->priority_flow_ctrl_set)(
			dev, pfc_conf);

	return (-ENOTSUP);
}

static inline int odp_eth_check_reta_mask(
	struct odp_eth_rss_reta_entry64 *reta_conf,
	uint16_t			 reta_size)
{
	uint16_t i, num;

	if (!reta_conf)
		return -EINVAL;

	if (reta_size !=
	    ODP_ALIGN(reta_size, ODP_RETA_GROUP_SIZE)) {
		ODP_ERR(
			"Invalid reta size, should be %u aligned\n",
			ODP_RETA_GROUP_SIZE);
		return -EINVAL;
	}

	num = reta_size / ODP_RETA_GROUP_SIZE;
	for (i = 0; i < num; i++)
		if (reta_conf[i].mask)
			return 0;

	return -EINVAL;
}

static inline int odp_eth_check_reta_entry(
	struct odp_eth_rss_reta_entry64 *reta_conf,
	uint16_t			 reta_size,
	uint8_t				 max_rxq)
{
	uint16_t i, idx, shift;

	if (!reta_conf)
		return -EINVAL;

	if (max_rxq == 0) {
		ODP_ERR("No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / ODP_RETA_GROUP_SIZE;
		shift = i % ODP_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) &&
		    (reta_conf[idx].reta[shift] >= max_rxq)) {
			ODP_ERR(
				"reta_conf[%u]->reta[%u]: %u exceeds "
				"the maximum rxq index: %u\n",
				idx, shift,
				reta_conf[idx].reta[shift],
				max_rxq);
			return -EINVAL;
		}
	}

	return 0;
}

int odp_eth_dev_rss_reta_update(uint8_t	 port_id,
				struct
				odp_eth_rss_reta_entry64 *
				reta_conf,
				uint16_t reta_size)
{
	struct odp_eth_dev *dev;
	int ret;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	/* Check mask bits */
	ret = odp_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &odp_eth_devices[port_id];

	/* Check entry value */
	ret = odp_eth_check_reta_entry(reta_conf, reta_size,
				       dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	if (*dev->dev_ops->reta_update == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->reta_update)(dev, reta_conf,
					    reta_size);
}

int odp_eth_dev_rss_reta_query(uint8_t	port_id,
			       struct
			       odp_eth_rss_reta_entry64 *
			       reta_conf,
			       uint16_t reta_size)
{
	struct odp_eth_dev *dev;
	int ret;

	if (port_id >= nb_ports) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	/* Check mask bits */
	ret = odp_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->reta_query == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->reta_query)(dev, reta_conf,
					   reta_size);
}

int odp_eth_dev_rss_hash_update(uint8_t port_id,
				struct odp_eth_rss_conf *
				rss_conf)
{
	struct odp_eth_dev *dev;
	uint16_t rss_hash_protos;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	rss_hash_protos = rss_conf->rss_hf;
	if ((rss_hash_protos != 0) &&
	    ((rss_hash_protos & ETH_RSS_PROTO_MASK) == 0)) {
		ODP_ERR("Invalid rss_hash_protos=0x%x\n",
			rss_hash_protos);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->rss_hash_update == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->rss_hash_update)(dev, rss_conf);
}

int odp_eth_dev_rss_hash_conf_get(uint8_t port_id,
				  struct odp_eth_rss_conf *
				  rss_conf)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->rss_hash_conf_get == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->rss_hash_conf_get)(dev,
						  rss_conf);
}

int odp_eth_dev_udp_tunnel_add(uint8_t port_id,
			       struct odp_eth_udp_tunnel *
			       udp_tunnel)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	if (udp_tunnel == NULL) {
		ODP_ERR("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= ODP_TUNNEL_TYPE_MAX) {
		ODP_ERR("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->udp_tunnel_add == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->udp_tunnel_add)(dev, udp_tunnel);
}

int odp_eth_dev_udp_tunnel_delete(uint8_t port_id,
				  struct odp_eth_udp_tunnel
					 *udp_tunnel)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		ODP_ERR("Invalid udp_tunnel parametr\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= ODP_TUNNEL_TYPE_MAX) {
		ODP_ERR("Invalid tunnel type\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->udp_tunnel_del == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->udp_tunnel_del)(dev, udp_tunnel);
}

int odp_eth_led_on(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_led_on == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return ((*dev->dev_ops->dev_led_on)(dev));
}

int odp_eth_led_off(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->dev_led_off == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return ((*dev->dev_ops->dev_led_off)(dev));
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static inline int get_mac_addr_index(uint8_t port_id,
				     struct odp_ether_addr *
				     addr)
{
	struct odp_eth_dev_info dev_info;
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];
	unsigned i;

	odp_eth_dev_info_get(port_id, &dev_info);

	for (i = 0; i < dev_info.max_mac_addrs; i++)
		if (memcmp(addr, &dev->data->mac_addrs[i],
			   ODP_ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

static struct odp_ether_addr null_mac_addr = {
	{0, 0, 0, 0, 0, 0}
};

int odp_eth_dev_mac_addr_add(uint8_t		    port_id,
			     struct odp_ether_addr *addr,
			     uint32_t		    pool)
{
	struct odp_eth_dev *dev;
	int index;
	uint64_t pool_mask;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->mac_addr_add == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	if (is_zero_ether_addr(addr)) {
		ODP_ERR("port %d: Cannot add NULL MAC address\n",
			port_id);
		return (-EINVAL);
	}

	if (pool >= ETH_64_POOLS) {
		ODP_ERR("pool id must be 0-%d\n",
			ETH_64_POOLS - 1);
		return (-EINVAL);
	}

	index = get_mac_addr_index(port_id, addr);
	if (index < 0) {
		index =
			get_mac_addr_index(port_id,
					   &null_mac_addr);
		if (index < 0) {
			ODP_ERR(
				"port %d: MAC address array full\n",
				port_id);
			return (-ENOSPC);
		}
	} else {
		pool_mask = dev->data->mac_pool_sel[index];

		/* Check if both MAC address and pool is alread there,
		 * and do nothing */
		if (pool_mask & (1ULL << pool))
			return 0;
	}

	/* Update NIC */
	(*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	/* Update address in NIC data structure */
	ether_addr_copy(addr, &dev->data->mac_addrs[index]);

	/* Update pool bitmap in NIC data structure */
	dev->data->mac_pool_sel[index] |= (1ULL << pool);

	return 0;
}

int odp_eth_dev_mac_addr_remove(uint8_t		       port_id,
				struct odp_ether_addr *addr)
{
	struct odp_eth_dev *dev;
	int index;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->mac_addr_remove == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	index = get_mac_addr_index(port_id, addr);
	if (index == 0) {
		ODP_ERR(
			"port %d: Cannot remove default MAC address\n",
			port_id);
		return (-EADDRINUSE);
	} else if (index < 0) {
		return 0; /* Do nothing if address wasn't found */
	}

	/* Update NIC */
	(*dev->dev_ops->mac_addr_remove)(dev, index);

	/* Update address in NIC data structure */
	ether_addr_copy(&null_mac_addr,
			&dev->data->mac_addrs[index]);

	/* reset pool bitmap */
	dev->data->mac_pool_sel[index] = 0;

	return 0;
}

int odp_eth_dev_set_vf_rxmode(uint8_t port_id, uint16_t vf,
			      uint16_t rx_mode, uint8_t on)
{
	uint16_t num_vfs;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("set VF RX mode:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	odp_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		ODP_ERR("set VF RX mode:invalid VF id %d\n",
			vf);
		return (-EINVAL);
	}

	if (rx_mode == 0) {
		ODP_ERR(
			"set VF RX mode:mode mask ca not be zero\n");
		return (-EINVAL);
	}

	if (*dev->dev_ops->set_vf_rx_mode == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_vf_rx_mode)(dev, vf, rx_mode,
					       on);
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static inline int get_hash_mac_addr_index(uint8_t port_id,
					  struct
					  odp_ether_addr *
					  addr)
{
	struct odp_eth_dev_info dev_info;
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];
	unsigned i;

	odp_eth_dev_info_get(port_id, &dev_info);
	if (!dev->data->hash_mac_addrs)
		return -1;

	for (i = 0; i < dev_info.max_hash_mac_addrs; i++)
		if (memcmp(addr, &dev->data->hash_mac_addrs[i],
			   ODP_ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

int odp_eth_dev_uc_hash_table_set(uint8_t port_id,
				  struct odp_ether_addr *
				  addr,
				  uint8_t on)
{
	int index;
	int ret;
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR(
			"unicast hash setting:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (is_zero_ether_addr(addr)) {
		ODP_ERR("port %d: Cannot add NULL MAC address\n",
			port_id);
		return (-EINVAL);
	}

	index = get_hash_mac_addr_index(port_id, addr);

	/* Check if it's already there, and do nothing */
	if ((index >= 0) && (on))
		return 0;

	if (index < 0) {
		if (!on) {
			ODP_ERR(
				"port %d: the MAC address was not"
				" set in UTA\n", port_id);
			return (-EINVAL);
		}

		index =
			get_hash_mac_addr_index(port_id,
						&null_mac_addr);
		if (index < 0) {
			ODP_ERR(
				"port %d: MAC address array full\n",
				port_id);
			return (-ENOSPC);
		}
	}

	if (*dev->dev_ops->uc_hash_table_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	ret = (*dev->dev_ops->uc_hash_table_set)(dev, addr, on);
	if (ret == 0) {
		/* Update address in NIC data structure */
		if (on)
			ether_addr_copy(addr,
					&dev->data->
					hash_mac_addrs[index]);
		else
			ether_addr_copy(&null_mac_addr,
					&dev->data->
					hash_mac_addrs[index]);
	}

	return ret;
}

int odp_eth_dev_uc_all_hash_table_set(uint8_t port_id,
				      uint8_t on)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR(
			"unicast hash setting:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->uc_all_hash_table_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->uc_all_hash_table_set)(dev, on);
}

int odp_eth_dev_set_vf_rx(uint8_t port_id, uint16_t vf,
			  uint8_t on)
{
	uint16_t num_vfs;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	odp_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		ODP_ERR("port %d: invalid vf id\n", port_id);
		return (-EINVAL);
	}

	if (*dev->dev_ops->set_vf_rx == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_vf_rx)(dev, vf, on);
}

int odp_eth_dev_set_vf_tx(uint8_t port_id, uint16_t vf,
			  uint8_t on)
{
	uint16_t num_vfs;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("set pool tx:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	odp_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		ODP_ERR("set pool tx:invalid pool id=%d\n", vf);
		return (-EINVAL);
	}

	if (*dev->dev_ops->set_vf_tx == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_vf_tx)(dev, vf, on);
}

int odp_eth_dev_set_vf_vlan_filter(uint8_t  port_id,
				   uint16_t vlan_id,
				   uint64_t vf_mask,
				   uint8_t  vlan_on)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("VF VLAN filter:invalid port id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];

	if (vf_mask == 0) {
		ODP_ERR(
			"VF VLAN filter:pool_mask can not be 0\n");
		return (-EINVAL);
	}

	if (vlan_id > ODP_ETHER_MAX_VLAN_ID) {
		ODP_ERR("VF VLAN filter:invalid VLAN id=%d\n",
			vlan_id);
		return (-EINVAL);
	}

	if (*dev->dev_ops->set_vf_vlan_filter == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_vf_vlan_filter)(dev, vlan_id,
						   vf_mask,
						   vlan_on);
}

int odp_eth_set_queue_rate_limit(uint8_t  port_id,
				 uint16_t queue_idx,
				 uint16_t tx_rate)
{
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;
	struct odp_eth_link link;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR(
			"set queue rate limit:invalid port id=%d\n",
			port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];
	odp_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (tx_rate > link.link_speed) {
		ODP_ERR(
			"set queue rate limit:invalid tx_rate=%d, "
			"bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	if (queue_idx > dev_info.max_tx_queues) {
		ODP_ERR("set queue rate limit:port %d: "
			"invalid queue id=%d\n",
			port_id, queue_idx);
		return -EINVAL;
	}

	if (*dev->dev_ops->set_queue_rate_limit == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_queue_rate_limit)(dev,
						     queue_idx,
						     tx_rate);
}

int odp_eth_set_vf_rate_limit(uint8_t port_id, uint16_t vf,
			      uint16_t tx_rate,
			      uint64_t q_msk)
{
	struct odp_eth_dev *dev;
	struct odp_eth_dev_info dev_info;
	struct odp_eth_link link;

	if (q_msk == 0)
		return 0;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("set VF rate limit:invalid port id=%d\n",
			port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];
	odp_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (tx_rate > link.link_speed) {
		ODP_ERR("set VF rate limit:invalid tx_rate=%d, "
			"bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	if (vf > dev_info.max_vfs) {
		ODP_ERR("set VF rate limit:port %d: "
			"invalid vf id=%d\n", port_id, vf);
		return -EINVAL;
	}

	if (*dev->dev_ops->set_vf_rate_limit == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->set_vf_rate_limit)(dev, vf,
						  tx_rate,
						  q_msk);
}

int odp_eth_mirror_rule_set(uint8_t port_id,
			    struct odp_eth_vmdq_mirror_conf
			    *mirror_conf,
			    uint8_t rule_id, uint8_t on)
{
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if (mirror_conf->dst_pool >= ETH_64_POOLS) {
		ODP_ERR("Invalid dst pool, pool id must"
			" be 0-%d\n", (ETH_64_POOLS - 1));
		return (-EINVAL);
	}

	if (mirror_conf->rule_type_mask == 0) {
		ODP_ERR("mirror rule type can not be 0.\n");
		return (-EINVAL);
	}

	if ((mirror_conf->rule_type_mask &
	     ETH_VMDQ_POOL_MIRROR) &&
	    (mirror_conf->pool_mask == 0)) {
		ODP_ERR("Invalid mirror pool, pool mask can not"
			" be 0.\n");
		return (-EINVAL);
	}

	if (rule_id >= ETH_VMDQ_NUM_MIRROR_RULE) {
		ODP_ERR(
			"Invalid rule_id, rule_id must be 0-%d\n",
			ETH_VMDQ_NUM_MIRROR_RULE - 1);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->mirror_rule_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return (*dev->dev_ops->mirror_rule_set)(dev,
						mirror_conf,
						rule_id, on);
}

int odp_eth_mirror_rule_reset(uint8_t port_id,
			      uint8_t rule_id)
{
	struct odp_eth_dev *dev = &odp_eth_devices[port_id];

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if (rule_id >= ETH_VMDQ_NUM_MIRROR_RULE) {
		ODP_ERR(
			"Invalid rule_id, rule_id must be 0-%d\n",
			ETH_VMDQ_NUM_MIRROR_RULE - 1);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->mirror_rule_reset == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}

	return (*dev->dev_ops->mirror_rule_reset)(dev, rule_id);
}

int odp_eth_dev_callback_register(uint8_t	    port_id,
				  enum odp_eth_event_type
				  event,
				  odp_eth_dev_cb_fn cb_fn,
				  void		   *cb_arg)
{
	struct odp_eth_dev *dev;
	struct odp_eth_dev_callback *user_cb;

	if (!cb_fn)
		return (-EINVAL);

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];
	odp_spinlock_lock(&odp_eth_dev_cb_lock);

	TAILQ_FOREACH(user_cb, &dev->link_intr_cbs, next)
	{
		if ((user_cb->cb_fn == cb_fn) &&
		    (user_cb->cb_arg == cb_arg) &&
		    (user_cb->event == event))
			break;
	}

	/* create a new callback. */
	if (!user_cb)
		user_cb = odp_zmalloc("user_cb",
				      sizeof(struct
					     odp_eth_dev_callback),
				      0);

	if (user_cb) {
		user_cb->cb_fn	= cb_fn;
		user_cb->cb_arg = cb_arg;
		user_cb->event	= event;
		TAILQ_INSERT_TAIL(&dev->link_intr_cbs, user_cb,
				  next);
	}

	odp_spinlock_unlock(&odp_eth_dev_cb_lock);

	return ((user_cb == NULL) ? -ENOMEM : 0);
}

int odp_eth_dev_callback_unregister(uint8_t	      port_id,
				    enum odp_eth_event_type
				    event,
				    odp_eth_dev_cb_fn cb_fn,
				    void	     *cb_arg)
{
	int ret;
	struct odp_eth_dev *dev;
	struct odp_eth_dev_callback *cb, *next;

	if (!cb_fn)
		return (-EINVAL);

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &odp_eth_devices[port_id];
	odp_spinlock_lock(&odp_eth_dev_cb_lock);

	ret = 0;
	for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL;
	     cb = next) {
		next = TAILQ_NEXT(cb, next);

		if ((cb->cb_fn != cb_fn) ||
		    (cb->event != event) ||
		    ((cb->cb_arg != (void *)-1) &&
		     (cb->cb_arg != cb_arg)))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (!cb->active) {
			TAILQ_REMOVE(&dev->link_intr_cbs, cb,
				     next);
			free(cb);
		} else {
			ret = -EAGAIN;
		}
	}

	odp_spinlock_unlock(&odp_eth_dev_cb_lock);

	return ret;
}

void _odp_eth_dev_callback_process(struct odp_eth_dev     *
				   dev,
				   enum odp_eth_event_type
				   event)
{
	struct odp_eth_dev_callback *cb_lst;
	struct odp_eth_dev_callback  dev_cb;

	odp_spinlock_lock(&odp_eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &dev->link_intr_cbs, next)
	{
		if ((!cb_lst->cb_fn) ||
		    (cb_lst->event != event))
			continue;

		dev_cb = *cb_lst;
		cb_lst->active = 1;
		odp_spinlock_unlock(&odp_eth_dev_cb_lock);
		dev_cb.cb_fn(dev->data->port_id, dev_cb.event,
			     dev_cb.cb_arg);
		odp_spinlock_lock(&odp_eth_dev_cb_lock);
		cb_lst->active = 0;
	}
	odp_spinlock_unlock(&odp_eth_dev_cb_lock);
}

#ifdef ODP_NIC_BYPASS
int odp_eth_dev_bypass_init(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_init == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_init)(dev);
	return 0;
}

int odp_eth_dev_bypass_state_show(uint8_t   port_id,
				  uint32_t *state)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_state_show == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_state_show)(dev, state);
	return 0;
}

int odp_eth_dev_bypass_state_set(uint8_t   port_id,
				 uint32_t *new_state)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_state_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_state_set)(dev, new_state);
	return 0;
}

int odp_eth_dev_bypass_event_show(uint8_t   port_id,
				  uint32_t  event,
				  uint32_t *state)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_state_show == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_event_show)(dev, event, state);
	return 0;
}

int odp_eth_dev_bypass_event_store(uint8_t  port_id,
				   uint32_t event,
				   uint32_t state)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_event_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_event_set)(dev, event, state);
	return 0;
}

int odp_eth_dev_wd_timeout_store(uint8_t  port_id,
				 uint32_t timeout)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_wd_timeout_set == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_wd_timeout_set)(dev, timeout);
	return 0;
}

int odp_eth_dev_bypass_ver_show(uint8_t	  port_id,
				uint32_t *ver)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}

	if (*dev->dev_ops->bypass_ver_show == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_ver_show)(dev, ver);
	return 0;
}

int odp_eth_dev_bypass_wd_timeout_show(uint8_t port_id,
				       uint32_t *
				       wd_timeout)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}


	if (*dev->dev_ops->bypass_wd_timeout_show == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_wd_timeout_show)(dev,
						wd_timeout);
	return 0;
}

int odp_eth_dev_bypass_wd_reset(uint8_t port_id)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &odp_eth_devices[port_id];
	if (dev == NULL) {
		ODP_ERR("Invalid port device\n");
		return (-ENODEV);
	}


	if (*dev->dev_ops->bypass_wd_reset == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	(*dev->dev_ops->bypass_wd_reset)(dev);
	return 0;
}
#endif

int odp_eth_dev_filter_supported(uint8_t port_id,
				 enum odp_filter_type
				 filter_type)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->filter_ctrl == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type,
					    ODP_ETH_FILTER_NOP,
					    NULL);
}

int odp_eth_dev_filter_ctrl(uint8_t		 port_id,
			    enum odp_filter_type filter_type,
			    enum odp_filter_op	 filter_op,
			    void		*arg)
{
	struct odp_eth_dev *dev;

	if (!odp_eth_dev_is_valid_port(port_id)) {
		ODP_ERR("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &odp_eth_devices[port_id];

	if (*dev->dev_ops->filter_ctrl == NULL) {
		printf( "%s:%d function not supported",
			    __func__, __LINE__);
		return -ENOTSUP;
	}
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type,
					    filter_op, arg);
}

void *odp_eth_add_rx_callback(uint8_t		 port_id,
			      uint16_t		 queue_id,
			      odp_rx_callback_fn fn,
			      void		*user_param)
{
#ifndef ODP_ETHDEV_RXTX_CALLBACKS
	odp_err = ENOTSUP;
	return NULL;
#endif

	/* check input parameters */
	if (!odp_eth_dev_is_valid_port(port_id) || (!fn) ||
	    (queue_id >=
	     odp_eth_devices[port_id].data->nb_rx_queues)) {
		odp_err = EINVAL;
		return NULL;
	}

	struct odp_eth_rxtx_callback *cb =
		odp_zmalloc("cb", sizeof(*cb), 0);

	if (cb == NULL) {
		odp_err = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;
	cb->next  =
		odp_eth_devices[port_id].post_rx_burst_cbs[
			queue_id];
	odp_eth_devices[port_id].post_rx_burst_cbs[queue_id] =
		cb;
	return cb;
}

void *odp_eth_add_tx_callback(uint8_t		 port_id,
			      uint16_t		 queue_id,
			      odp_tx_callback_fn fn,
			      void		*user_param)
{
#ifndef ODP_ETHDEV_RXTX_CALLBACKS
	odp_err = ENOTSUP;
	return NULL;
#endif

	/* check input parameters */
	if (!odp_eth_dev_is_valid_port(port_id) || (!fn) ||
	    (queue_id >=
	     odp_eth_devices[port_id].data->nb_tx_queues)) {
		odp_err = EINVAL;
		return NULL;
	}

	struct odp_eth_rxtx_callback *cb =
		odp_zmalloc("cb", sizeof(*cb), 0);

	if (cb == NULL) {
		odp_err = ENOMEM;
		return NULL;
	}

	cb->fn.tx = fn;
	cb->param = user_param;
	cb->next  =
		odp_eth_devices[port_id].pre_tx_burst_cbs[
			queue_id];
	odp_eth_devices[port_id].pre_tx_burst_cbs[queue_id] =
		cb;
	return cb;
}

int odp_eth_remove_rx_callback(uint8_t	port_id,
			       uint16_t queue_id,
			       struct odp_eth_rxtx_callback
				       *user_cb)
{
#ifndef ODP_ETHDEV_RXTX_CALLBACKS
	return (-ENOTSUP);
#endif

	/* Check input parameters. */
	if (!odp_eth_dev_is_valid_port(port_id) || (!user_cb) ||
	    (queue_id >=
	     odp_eth_devices[port_id].data->nb_rx_queues))
		return (-EINVAL);

	struct odp_eth_dev *dev = &odp_eth_devices[port_id];
	struct odp_eth_rxtx_callback *cb =
		dev->post_rx_burst_cbs[queue_id];
	struct odp_eth_rxtx_callback *prev_cb;

	/* Reset head pointer and remove user cb if first in the list. */
	if (cb == user_cb) {
		dev->post_rx_burst_cbs[queue_id] =
			user_cb->next;
		return 0;
	}

	/* Remove the user cb from the callback list. */
	do {
		prev_cb = cb;
		cb = cb->next;

		if (cb == user_cb) {
			prev_cb->next = user_cb->next;
			return 0;
		}
	} while (cb != NULL);

	/* Callback wasn't found. */
	return (-EINVAL);
}

int odp_eth_remove_tx_callback(uint8_t	port_id,
			       uint16_t queue_id,
			       struct odp_eth_rxtx_callback
				       *user_cb)
{
#ifndef ODP_ETHDEV_RXTX_CALLBACKS
	return (-ENOTSUP);
#endif

	/* Check input parameters. */
	if (!odp_eth_dev_is_valid_port(port_id) || (!user_cb) ||
	    (queue_id >=
	     odp_eth_devices[port_id].data->nb_tx_queues))
		return (-EINVAL);

	struct odp_eth_dev *dev = &odp_eth_devices[port_id];
	struct odp_eth_rxtx_callback *cb =
		dev->pre_tx_burst_cbs[queue_id];
	struct odp_eth_rxtx_callback *prev_cb;

	/* Reset head pointer and remove user cb if first in the list. */
	if (cb == user_cb) {
		dev->pre_tx_burst_cbs[queue_id] = user_cb->next;
		return 0;
	}

	/* Remove the user cb from the callback list. */
	do {
		prev_cb = cb;
		cb = cb->next;

		if (cb == user_cb) {
			prev_cb->next = user_cb->next;
			return 0;
		}
	} while (cb != NULL);

	/* Callback wasn't found. */
	return (-EINVAL);
}

