/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <sys/queue.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <odp/crypto.h>
#include <odp_internal.h>
#include <odp/atomic.h>
#include <odp/spinlock.h>
#include <odp/sync.h>
#include <odp/debug.h>
#include <odp/align.h>
#include <odp/shared_memory.h>
/*#include <odp_crypto_internal.h>*/
#include <odp_debug_internal.h>
#include <odp/hints.h>
#include <odp/random.h>
#include <odp_packet_internal.h>
#include <odp_uio_internal.h>

#define FILE_TRUE 1
#define FILE_MAX  256

TAILQ_HEAD(odp_uio_driver_list, odp_uio_driver);
TAILQ_HEAD(, odp_uio_device) odp_uio_dev_list;
TAILQ_HEAD(, odp_uio_data) odp_uio_data_list;

struct odp_uio_driver_list odp_uio_drv_list =
	TAILQ_HEAD_INITIALIZER(odp_uio_drv_list);

struct dev_file_info {
	int	       flag;
	struct dirent *file;
};

struct dev_file_info dev_file[FILE_MAX];

struct odp_uio_driver *odp_check_uio_drv_is_reg(char *drv_name)
{
	struct odp_uio_driver *acc_drv;
	int len;

	TAILQ_FOREACH(acc_drv, &odp_uio_drv_list, next)
	{
		if (strlen(drv_name) != strlen(acc_drv->name))
			continue;

		len = strlen(drv_name);
		if (strncmp(drv_name, acc_drv->name, len) == 0)
			return acc_drv;
	}

	return NULL;
}

void odp_uio_register(struct odp_uio_driver *driver)
{
	if (!driver) {
		ODP_ERR("driver is null!\n");
		return;
	}

	if (!(driver->dev_init)) {
		ODP_ERR("dev_init is null!\n");
		return;
	}

	if (!(driver->dev_uninit)) {
		ODP_ERR("dev_uninit is null!\n");
		return;
	}

	if (odp_check_uio_drv_is_reg(driver->name)) {
		ODP_ERR("call odp_check_uio_drv_is_reg failed!\n");
		return;
	}

	TAILQ_INSERT_TAIL(&odp_uio_drv_list, driver, next);
}

void odp_uio_unregister(struct odp_uio_driver *driver)
{
	struct odp_uio_driver *drv_temp = NULL;
	struct odp_uio_driver *drv_next = NULL;
	int len;

	if (!driver) {
		ODP_ERR("driver is null!\n");
		return;
	}

	for (drv_temp = TAILQ_FIRST(&odp_uio_drv_list);
	     drv_temp != NULL; drv_temp = drv_next) {
		if (strlen(drv_temp->name) == strlen(driver->name)) {
			len = strlen(driver->name);
			if (strncmp(drv_temp->name, driver->name, len) == 0) {
				TAILQ_REMOVE(&odp_uio_drv_list,
					     drv_temp, next);

				return;
			}
		}

		drv_next = TAILQ_NEXT(drv_next, next);
	}

	ODP_ERR("drv is not find!\n");
}

int odp_uio_drv_load_pro(void)
{
	DIR *d;
	struct dirent *file;
	char file_path_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	void *dev_drv_dlopen_handle;

	d = opendir(ODP_DEV_DRV_PATH);
	if (!d) {
		ODP_ERR("call opendir failed!\n");
		return (-1);
	}

	while ((file = readdir(d)) != NULL) {
		if ((strncmp(file->d_name, ".", 1) == 0) ||
		    (strncmp(file->d_name, "..", 2) == 0))
			continue;

		memset((void *)file_path_name, 0, sizeof(file_path_name));
		(void)strncpy(file_path_name, ODP_DEV_DRV_PATH,
			      strlen(ODP_DEV_DRV_PATH) + 1);
		(void)strcat(file_path_name, file->d_name);

		dev_drv_dlopen_handle = dlopen(file_path_name, RTLD_LAZY);
		if (!dev_drv_dlopen_handle) {
			ODP_ERR("call dlopen failed! dlopen err:%s.\n",
				dlerror());
			return (-1);
		}

		ODP_PRINT("open shared lib %s, addr = 0x%lx\n",
			  file_path_name,
			  *(unsigned long *)dev_drv_dlopen_handle);
	}

	closedir(d);
	return 0;
}

int odp_uio_dev_pro(void)
{
	int max = 0;
	int i = 0;
	char *p = NULL;
	DIR  *d = NULL;
	struct dirent *file = NULL;
	char  file_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	struct odp_uio_device *dev = NULL;

	d = opendir(ODP_UIO_DEV_PATH);
	if (!d) {
		ODP_ERR("call opendir failed!\n");
		return (-1);
	}

	while ((file = readdir(d)) != NULL) {
		if (strncmp(file->d_name, "uio", 3) != 0)
			continue;

		p = &file->d_name[3];
		dev_file[atoi(p)].flag = FILE_TRUE;
		dev_file[atoi(p)].file = file;
		max++;

		if (max >= FILE_MAX)
			break;
	}

	for (i = 0; i < max; i++) {
		if (dev_file[i].flag != FILE_TRUE)
			continue;

		file = dev_file[i].file;

		memset((void *)file_name, 0, sizeof(file_name));
		(void)strncpy(file_name, ODP_UIO_DEV_PATH,
			      strlen(ODP_UIO_DEV_PATH) + 1);
		(void)strcat(file_name, file->d_name);

		dev = malloc(sizeof(struct odp_uio_device));

		if (!dev) {
			ODP_PRINT("call malloc failed!\n");
			closedir(d);
			return (-1);
		}

		memset(dev, 0, sizeof(struct odp_uio_device));
		(void)strncpy(dev->name, file->d_name,
			      strlen(file->d_name) + 1);
		dev->numa_node = 0;

		TAILQ_INSERT_TAIL(&odp_uio_dev_list, dev, next);
	}

	closedir(d);

	return 0;
}

int odp_uio_mmap_pro(void)
{
	DIR *d;
	struct dirent *file;
	char file_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	char addr_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	char size_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	char mmap_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	struct odp_uio_device *dev;
	int  uio_fd, addr_fd, size_fd;
	uint64_t uio_size;
	uint64_t uio_addr;
	void *access_address;
	char  uio_addr_buf[32], uio_size_buf[32];
	int   i;
	char  uiodev_name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	char  uio_name_buf[32];
	int   uioname_fd;

	TAILQ_FOREACH(dev, &odp_uio_dev_list, next)
	{
		memset((void *)file_name, 0, sizeof(file_name));
		(void)strncpy(file_name, ODP_UIO_DEV_PATH,
			      strlen(ODP_UIO_DEV_PATH) + 1);
		(void)strcat(file_name, dev->name);

		memset((void *)uiodev_name, 0, sizeof(mmap_name));
		(void)strncpy(uiodev_name, ODP_UIO_CLASS_PATH,
			      strlen(ODP_UIO_CLASS_PATH) + 1);
		(void)strcat(uiodev_name, dev->name);
		(void)strcat(uiodev_name, "/name");
		uioname_fd = open(uiodev_name, O_RDONLY);
		if (uioname_fd < 0) {
			ODP_ERR("call open %s failed!\n", uiodev_name);
			return (-1);
		}

		read(uioname_fd, uio_name_buf, sizeof(uio_name_buf));
		if (strncmp(uio_name_buf, "igb_uio", 7) == 0) {
			dev->driver = NULL;
			continue;
		}

		memset((void *)mmap_name, 0, sizeof(mmap_name));
		(void)strncpy(mmap_name, ODP_UIO_CLASS_PATH,
			      strlen(ODP_UIO_CLASS_PATH) + 1);
		(void)strcat(mmap_name, dev->name);
		(void)strcat(mmap_name, "/maps/");

		uio_fd = open(file_name, O_RDWR);
		if (uio_fd < 0) {
			ODP_ERR("call open %s failed!\n", file_name);
			return (-1);
		}

		d = opendir(mmap_name);
		if (!d) {
			ODP_ERR("call opendir %s failed!\n", mmap_name);
			return (-1);
		}

		i = 0;
		dev->uio_fd = uio_fd;

		while ((file = readdir(d)) != NULL) {
			if (strncmp(file->d_name, "map", 3) != 0)
				continue;

			(void)strncpy(addr_name, mmap_name,
				      strlen(mmap_name) + 1);
			(void)strncpy(size_name, mmap_name,
				      strlen(mmap_name) + 1);
			(void)strcat(addr_name, file->d_name);
			(void)strcat(size_name, file->d_name);
			(void)strcat(addr_name, "/addr");
			(void)strcat(size_name, "/size");

			addr_fd = open(addr_name, O_RDONLY);
			size_fd = open(size_name, O_RDONLY);
			if ((addr_fd < 0) || (size_fd < 0)) {
				ODP_ERR("call open failed!\n");
				return (-1);
			}

			read(addr_fd, uio_addr_buf, sizeof(uio_addr_buf));
			read(size_fd, uio_size_buf, sizeof(uio_size_buf));
			uio_addr = (uint64_t)strtoull(uio_addr_buf, NULL, 0);
			uio_size = (uint64_t)strtol(uio_size_buf, NULL, 0);

			access_address =
				(void *)mmap(NULL, uio_size,
					     PROT_READ | PROT_WRITE,
					     MAP_SHARED, uio_fd, UIO_OFFSET(i));
			if (access_address == (void *)-1) {
				ODP_ERR("call mmap failed!\n");
				close(addr_fd);
				close(size_fd);
				return (-1);
			}

			close(addr_fd);
			close(size_fd);

			dev->mem_resource[i].addr = access_address;
			dev->mem_resource[i].len  = uio_size;
			dev->mem_resource[i].phys_addr = uio_addr;
			i++;
		}
	}

	return 0;
}

int odp_uio_dev_drv_match(void)
{
	char name[ODP_PATH_LEN + ODP_UIO_NAME_MAX_LEN];
	struct odp_uio_device *dev = NULL;
	struct odp_uio_driver *drv = NULL;
	int fd, flag;
	char uio_drv_name[ODP_UIO_NAME_MAX_LEN];

	TAILQ_FOREACH(dev, &odp_uio_dev_list, next)
	{
		memset((void *)name, 0, sizeof(name));
		(void)strncpy(name, ODP_UIO_CLASS_PATH,
			      strlen(ODP_UIO_CLASS_PATH) + 1);
		(void)strcat(name, dev->name);
		(void)strcat(name, "/name");

		fd = open(name, O_RDONLY);
		if (fd < 0) {
			ODP_ERR("call open %s failed!\n", name);
			return (-1);
		}

		memset((void *)uio_drv_name, 0, sizeof(uio_drv_name));
		read(fd, uio_drv_name, sizeof(uio_drv_name));

		TAILQ_FOREACH(drv, &odp_uio_drv_list, next)
		{
			if (strncmp(uio_drv_name, drv->name,
				    strlen(drv->name)) == 0) {
				dev->driver = drv;
				flag = 1;
				break;
			}
		}
		close(fd);
	}
	if (!flag) {
		ODP_ERR("drv is not find!\n");
		return -1;
	} else {
		return 0;
	}
}

int odp_uio_dev_init(void)
{
	int iret;
	struct odp_uio_device *dev;

	TAILQ_FOREACH(dev, &odp_uio_dev_list, next)
	{
		if ((dev->driver == NULL) || (dev->driver->dev_init == NULL))
			continue;

		iret = dev->driver->dev_init(dev);
		if (iret) {
			ODP_ERR("call dev_init failed!\n");
			return iret;
		}
	}

	return 0;
}

int odp_uio_init_global(void)
{
	int iret;

	TAILQ_INIT(&odp_uio_dev_list);
	TAILQ_INIT(&odp_uio_data_list);

	memset(dev_file, 0, sizeof(struct dev_file_info) * FILE_MAX);

	iret = odp_uio_dev_pro();
	if (iret) {
		ODP_ERR("call odp_uio_dev_pro failed!\n");
		return iret;
	}

	iret = odp_uio_mmap_pro();
	if (iret) {
		ODP_ERR("call odp_uio_mmap_pro failed!\n");
		return iret;
	}

	iret = odp_uio_dev_drv_match();
	if (iret) {
		ODP_ERR("call odp_uio_dev_drv_match failed!\n");
		return iret;
	}

	iret = odp_uio_dev_init();
	if (iret) {
		ODP_ERR("call odp_uio_dev_init failed!\n");
		return iret;
	}

	/* PRINT("call odp_uio_init ok!\n"); */
	return 0;
}

int odp_uio_term_global(void)
{
	return 0;
}

struct odp_uio_data *odp_uio_check_name_is_alloc(char *name)
{
	struct odp_uio_data *uio_data;
	int len;

	TAILQ_FOREACH(uio_data, &odp_uio_data_list, next)
	{
		if (strlen(name) != strlen(uio_data->name))
			continue;

		len = strlen(name);
		if (strncmp(name, uio_data->name, len) == 0)
			return uio_data;
	}

	return NULL;
}

void *odp_uio_dev_data_alloc(char *name, uint32_t len)
{
	struct odp_uio_data *uio_data;

	if (!name) {
		ODP_ERR("name is null!\n");
		return NULL;
	}

	if (len == 0) {
		ODP_ERR("Invalid len = %u!\n", len);
		return NULL;
	}

	if (odp_uio_check_name_is_alloc(name)) {
		ODP_ERR("call odp_uio_check_name_is_alloc failed!\n");
		return NULL;
	}

	uio_data = (struct odp_uio_data *)malloc(len +
						 sizeof(struct odp_uio_data));
	if (!uio_data) {
		ODP_ERR("call malloc failed!\n");
		return NULL;
	}

	memset((void *)uio_data, 0, len + sizeof(struct odp_uio_data));
	strncpy(uio_data->name, name, strlen(name));
	uio_data->data = (void *)((uint8_t *)uio_data +
				  sizeof(struct odp_uio_data));

	TAILQ_INSERT_TAIL(&odp_uio_data_list, uio_data, next);
	return uio_data->data;
}

void odp_uio_dev_data_free(char *name)
{
	struct odp_uio_data *uio_data_temp;
	struct odp_uio_data *uio_date_next;

	if (!name) {
		ODP_ERR("name is null!\n");
		return;
	}

	for (uio_data_temp = TAILQ_FIRST(&odp_uio_data_list);
	     uio_data_temp != NULL; uio_data_temp = uio_date_next) {
		if (strlen(uio_data_temp->name) == strlen(name))
			if (strncmp(uio_data_temp->name, name,
				    strlen(name)) == 0) {
				free((void *)((uint8_t *)uio_data_temp->data -
					      sizeof(struct odp_uio_data)));
				TAILQ_REMOVE(&odp_uio_data_list,
					     uio_data_temp, next);
				return;
			}

		uio_date_next = TAILQ_NEXT(uio_data_temp, next);
	}

	ODP_ERR("name is not find!\n");
}
