/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP uio
 */

#ifndef ODP_API_UIO_H_
#define ODP_API_UIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE 4096

#define ODP_PATH_LEN 128
#if (!defined(__arm32__))
#define ODP_DEV_DRV_PATH "/usr/lib64/odp/"
#else
#define ODP_DEV_DRV_PATH "/usr/lib/odp/"
#endif
#define ODP_UIO_DEV_PATH   "/dev/"
#define ODP_UIO_CLASS_PATH "/sys/class/uio/"

/**
 * A structure describing a UIO resource.
 */
struct odp_uio_resource {
	uint64_t phys_addr; /**< Physical address, 0 if no resource. */
	uint64_t len;       /**< Length of the resource. */
	void	*addr;      /**< Virtual address, NULL when not mapped. */
};

/** Maximum number of UIO resources. */
#define ODP_UIO_MAX_RESOURCE (6)
#define ODP_UIO_NAME_MAX_LEN (32)

#define UIO_OFFSET(n) ((n) * PAGE_SIZE)

/**
 * A structure describing a UIO device.
 */
struct odp_uio_device {
	TAILQ_ENTRY(odp_uio_device) next; /**< Next probed UIO device. */

	char name[ODP_UIO_NAME_MAX_LEN];  /**< device name. */

	/**< UIO Memory Resource */
	struct odp_uio_resource mem_resource[ODP_UIO_MAX_RESOURCE];
	struct odp_uio_driver  *driver;    /**< Associated driver  */
	int			uio_fd;
	int			numa_node; /**< NUMA node connection */
};

/**
 * Initialisation function for the driver called during UIO probing.
 */
typedef int (odp_uio_dev_init_t)(struct odp_uio_device *uio_dev);

/**
 * Uninitialisation function for the driver called during hotplugging.
 */
typedef int (odp_uio_dev_uninit_t)(struct odp_uio_device *uio_dev);

/**
 * A structure describing a UIO driver.
 */
struct odp_uio_driver {
	TAILQ_ENTRY(odp_uio_driver) next;                 /**< Next in list. */

	char		      name[ODP_UIO_NAME_MAX_LEN]; /**< Driver name. */
	odp_uio_dev_init_t   *dev_init;                   /**< Device init. function. */
	odp_uio_dev_uninit_t *dev_uninit;                 /**< Device uninit function. */
};

struct odp_uio_data {
	TAILQ_ENTRY(odp_uio_data) next;   /**< Next in list. */

	char  name[ODP_UIO_NAME_MAX_LEN];
	void *data;
};

/*****************************************************************************
   Function     : odp_uio_register
   Description  : uio driver register
   Input        : struct odp_uio_driver *driver:UIO driver
   Output       : NA
   Return       :
   Create By    :
   Modification :
   Restriction  :
*****************************************************************************/
void odp_uio_register(struct odp_uio_driver *driver);

/*****************************************************************************
   Function     : odp_uio_unregister
   Description  : uio driver unregister
   Input        : struct odp_uio_driver *driver:UIO driver
   Output       : NA
   Return       :
   Create By    :
   Modification :
   Restriction  :
*****************************************************************************/
void odp_uio_unregister(struct odp_uio_driver *driver);

/*****************************************************************************
   Function     : odp_uio_init
   Description  : ODP uio device init
   Input        : NA
   Output       : NA
   Return       :
   Create By    :
   Modification :
   Restriction  :
*****************************************************************************/
int odp_uio_init(void);

/*****************************************************************************
   Function     : odp_uio_dev_data_alloc
   Description  : device data alloc
   Input        : char *name/uint32_t len
   Output       : NA
   Return       :
   Create By    :
   Modification :
   Restriction  :
*****************************************************************************/
void *odp_uio_dev_data_alloc(char *name, uint32_t len);

/*****************************************************************************
   Function     : odp_uio_dev_data_free
   Description  : device data free
   Input        : char *name:
   Output       : NA
   Return       :
   Create By    :
   Modification :
   Restriction  :
*****************************************************************************/
void odp_uio_dev_data_free(char *name);

int odp_uio_drv_load_pro(void);
#ifdef __cplusplus
}
#endif
#endif
