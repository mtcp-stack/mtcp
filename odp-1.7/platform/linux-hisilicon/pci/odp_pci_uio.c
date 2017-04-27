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

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/pci_regs.h>

/* #include <odp_log.h> */
#include <odp_pci.h>
#include <odp_syslayout.h>
#include <odp_common.h>
#include <odp/config.h>

/* #include "odp_pci_dev_ids.h" */
#include "odp_filesystem.h"
#include "odp_pci_init.h"
#include "odp_debug_internal.h"
#include "odp_uio_internal.h"

static struct odp_tailq_elem odp_uio_tailq = {
	.name = "UIO_RESOURCE_LIST",
};

ODP_REGISTER_TAILQ(odp_uio_tailq)

#define OFF_MAX ((uint64_t)(off_t)-1)

void *pci_map_addr = NULL;

static int pci_uio_set_bus_master(int dev_fd)
{
	uint16_t reg;
	int ret;

	ret = pread(dev_fd, &reg, sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		ODP_PRINT("Cannot read command from PCI config space!\n");
		return -1;
	}

	/* return if bus mastering is already on */
	if (reg & PCI_COMMAND_MASTER)
		return 0;

	reg |= PCI_COMMAND_MASTER;

	ret = pwrite(dev_fd, &reg, sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		ODP_PRINT("Cannot write command to PCI config space!\n");
		return -1;
	}

	return 0;
}

static int pci_uio_map_secondary(struct odp_pci_device *dev)
{
	int fd, i;
	struct mapped_pci_resource *uio_res;
	struct mapped_pci_res_list *uio_res_list =
		ODP_TAILQ_CAST(odp_uio_tailq.head, mapped_pci_res_list);

	TAILQ_FOREACH(uio_res, uio_res_list, next)
	{
		/* skip this element if it doesn't match our PCI address */
		if (odp_compare_pci_addr(&uio_res->pci_addr, &dev->addr))
			continue;

		for (i = 0; i != uio_res->nb_maps; i++) {
			/*
			 * open devname, to mmap it
			 */
			fd = open(uio_res->maps[i].path, O_RDWR);
			if (fd < 0) {
				ODP_PRINT("Cannot open %s: %s\n",
					  uio_res->maps[i].path,
					  strerror(errno));
				return -1;
			}

			void *mapaddr =
				pci_map_resource(uio_res->maps[i].addr,
						 fd,
						 (off_t)uio_res->maps[i].offset,
						 (size_t)uio_res->maps[i].size,
						 0);

			if (mapaddr != uio_res->maps[i].addr) {
				if (mapaddr == MAP_FAILED)
					ODP_PRINT(
						"Cannot mmap device resource file %s: %s\n",
						uio_res->maps[i].path,
						strerror(errno));
				else
					ODP_PRINT(
						"Cannot mmap device resource file %s to address: %p\n",
						uio_res->maps[i].path,
						uio_res->maps[i].addr);

				close(fd);
				return -1;
			}

			/* fd is not needed in slave process, close it */
			close(fd);
		}

		return 0;
	}

	ODP_PRINT("Cannot find resource for device\n");
	return 1;
}

/*
 * Return the uioX char device used for a pci device. On success, return
 * the UIO number and fill dstbuf string with the path of the device in
 * sysfs. On error, return a negative value. In this case dstbuf is
 * invalid.
 */
static int pci_get_uio_dev(struct odp_pci_device *dev, char *dstbuf,
			   unsigned int buflen)
{
	struct odp_pci_addr *loc = &dev->addr;
	unsigned int uio_num;
	struct dirent *e;
	DIR *dir;
	char dirname[ODP_PATH_MAX];

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX */

	snprintf(dirname, sizeof(dirname),
		 SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/uio",
		 loc->domain, loc->bus, loc->devid, loc->function);

	dir = opendir(dirname);
	if (!dir) {
		/* retry with the parent directory */
		snprintf(dirname, sizeof(dirname),
			 SYSFS_PCI_DEVICES "/" PCI_PRI_FMT,
			 loc->domain, loc->bus, loc->devid, loc->function);
		dir = opendir(dirname);

		if (!dir) {
			ODP_PRINT("Cannot opendir %s\n", dirname);
			return -1;
		}
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;

		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1;
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if ((errno == 0) && (endptr != (e->d_name + shortprefix_len))) {
			snprintf(dstbuf, buflen, "%s/uio%u", dirname, uio_num);
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if ((errno == 0) && (endptr != (e->d_name + longprefix_len))) {
			snprintf(dstbuf, buflen, "%s/uio:uio%u",
				 dirname, uio_num);
			break;
		}
	}

	closedir(dir);

	/* No uio resource found */
	if (!e)
		return -1;

	return uio_num;
}

/* map the PCI resource of a PCI device in virtual memory */
int pci_uio_map_resource(struct odp_pci_device *dev)
{
	int i, map_idx;
	char dirname[ODP_PATH_MAX];
	char cfgname[ODP_PATH_MAX];
	char devname[ODP_PATH_MAX]; /* contains the /dev/uioX */
	void *mapaddr;
	int  uio_num;
	uint64_t phaddr;
	struct odp_pci_addr *loc = &dev->addr;
	struct mapped_pci_resource *uio_res;
	struct mapped_pci_res_list *uio_res_list =
		ODP_TAILQ_CAST(odp_uio_tailq.head, mapped_pci_res_list);
	struct pci_map *maps;

	dev->intr_handle.fd = -1;
	dev->intr_handle.uio_cfg_fd = -1;
	dev->intr_handle.type = ODP_INTR_HANDLE_UNKNOWN;

	/* secondary processes - use already recorded details */
	if (odp_process_type() != ODP_PROC_PRIMARY)
		return pci_uio_map_secondary(dev);

	/* find uio resource */
	uio_num = pci_get_uio_dev(dev, dirname, sizeof(dirname));
	if (uio_num < 0) {
		ODP_PRINT("  " PCI_PRI_FMT " not managed by UIO"
			  " driver, skipping\n",
			  loc->domain, loc->bus, loc->devid, loc->function);
		return 1;
	}

	snprintf(devname, sizeof(devname), "/dev/uio%u", uio_num);

	/* save fd if in primary process */
	dev->intr_handle.fd = open(devname, O_RDWR);
	if (dev->intr_handle.fd < 0) {
		ODP_PRINT("Cannot open %s: %s\n",
			  devname, strerror(errno));
		return -1;
	}

	dev->intr_handle.type = ODP_INTR_HANDLE_UIO;

	snprintf(cfgname, sizeof(cfgname),
		 "/sys/class/uio/uio%u/device/config", uio_num);
	dev->intr_handle.uio_cfg_fd = open(cfgname, O_RDWR);
	if (dev->intr_handle.uio_cfg_fd < 0) {
		ODP_PRINT("Cannot open %s: %s\n",
			  cfgname, strerror(errno));
		return -1;
	}

	/* set bus master that is not done by uio_pci_generic */
	if (pci_uio_set_bus_master(dev->intr_handle.uio_cfg_fd)) {
		ODP_PRINT("Cannot set up bus mastering!\n");
		return -1;
	}

	/* allocate the mapping details for secondary processes*/
	uio_res = malloc(sizeof(*uio_res));
	if (!uio_res) {
		ODP_PRINT("%s(): cannot store uio mmap details\n", __func__);
		return -1;
	}

	memset((void *)uio_res, 0, sizeof(*uio_res));
	snprintf(uio_res->path, sizeof(uio_res->path), "%s", devname);
	memcpy(&uio_res->pci_addr, &dev->addr, sizeof(uio_res->pci_addr));

	/* Map all BARs */
	maps = uio_res->maps;
	for (i = 0, map_idx = 0; i != PCI_MAX_RESOURCE; i++) {
		/* int fd;*/
		int fail = 0;

		/* skip empty BAR */
		phaddr = dev->mem_resource[i].phys_addr;
		if (phaddr == 0)
			continue;

		/* update devname for mmap  */
		snprintf(devname, sizeof(devname),
			 SYSFS_PCI_DEVICES "/" PCI_PRI_FMT "/resource%d",
			 loc->domain, loc->bus, loc->devid, loc->function,
			 i);

		/*
		 * open resource file, to mmap it
		 */

		mapaddr = pci_map_resource(NULL, dev->intr_handle.fd,
					   (map_idx) * PAGE_SIZE,
					   (size_t)dev->mem_resource[i].len, 0);
		if (mapaddr == MAP_FAILED)
			fail = 1;

		maps[map_idx].path = malloc(strlen(devname) + 1);
		if (!maps[map_idx].path)
			fail = 1;

		if (fail) {
			free(uio_res);
			return -1;
		}

		maps[map_idx].phaddr = dev->mem_resource[i].phys_addr;
		maps[map_idx].size = dev->mem_resource[i].len;
		maps[map_idx].addr = mapaddr;
		maps[map_idx].offset = 0;
		strcpy(maps[map_idx].path, devname);
		map_idx++;
		dev->mem_resource[i].addr = mapaddr;
	}

	uio_res->nb_maps = map_idx;

	TAILQ_INSERT_TAIL(uio_res_list, uio_res, next);

	return 0;
}

#ifdef ODP_LIBHODP_HOTPLUG
static void pci_uio_unmap(struct mapped_pci_resource *uio_res)
{
	int i;

	if (!uio_res)
		return;

	for (i = 0; i != uio_res->nb_maps; i++)
		pci_unmap_resource(uio_res->maps[i].addr,
				   (size_t)uio_res->maps[i].size);
}

static struct mapped_pci_resource
*pci_uio_find_resource(struct odp_pci_device *dev)
{
	struct mapped_pci_resource *uio_res;
	struct mapped_pci_res_list *uio_res_list =
		ODP_TAILQ_CAST(odp_uio_tailq.head, mapped_pci_res_list);

	if (!dev)
		return NULL;

	TAILQ_FOREACH(uio_res, uio_res_list, next)
	{
		/* skip this element if it doesn't match our PCI address */
		if (!odp_compare_pci_addr(&uio_res->pci_addr, &dev->addr))
			return uio_res;
	}
	return NULL;
}

/* unmap the PCI resource of a PCI device in virtual memory */
void pci_uio_unmap_resource(struct odp_pci_device *dev)
{
	struct mapped_pci_resource *uio_res;
	struct mapped_pci_res_list *uio_res_list =
		ODP_TAILQ_CAST(odp_uio_tailq.head, mapped_pci_res_list);

	if (!dev)
		return;

	/* find an entry for the device */
	uio_res = pci_uio_find_resource(dev);
	if (!uio_res)
		return;

	/* secondary processes - just free maps */
	if (odp_process_type() != ODP_PROC_PRIMARY)
		return pci_uio_unmap(uio_res);

	TAILQ_REMOVE(uio_res_list, uio_res, next);

	/* unmap all resources */
	pci_uio_unmap(uio_res);

	/* free uio resource */
	free(uio_res);

	/* close fd if in primary process */
	close(dev->intr_handle.fd);

	dev->intr_handle.fd = -1;
	dev->intr_handle.type = ODP_INTR_HANDLE_UNKNOWN;
}
#endif /* HODP_LIBHODP_HOTPLUG */
