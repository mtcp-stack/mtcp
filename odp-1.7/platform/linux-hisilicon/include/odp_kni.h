/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _ODP_KNI_H_
#define _ODP_KNI_H_

/**
 * @file
 * RTE KNI
 *
 * The KNI library provides the ability to create and destroy kernel NIC
 * interfaces that may be used by the RTE application to receive/transmit
 * packets from/to Linux kernel net interfaces.
 *
 * This library provide two APIs to burst receive packets from KNI interfaces,
 * and burst transmit packets to KNI interfaces.
 */

#include <odp_pci.h>
#include <odp_memory.h>

#include "odp_kni_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rte_mbuf;

/**
 * Structure which has the function pointers for KNI interface.
 */
struct odp_kni_ops {
	uint8_t port_id; /* Port ID */

	/* Pointer to function of changing MTU */
	int (*change_mtu)(uint8_t port_id, unsigned new_mtu);

	/* Pointer to function of configuring network interface */
	int (*config_network_if)(uint8_t port_id, uint8_t if_up);
};

/**
 * Structure for configuring KNI device.
 */
struct odp_kni_conf {
	/*
	 * KNI name which will be used in relevant network device.
	 * Let the name as short as possible, as it will be part of
	 * mm_district name.
	 */
	char		    name[ODP_KNI_NAMESIZE];
	uint32_t	    core_id;    /* Core ID to bind kernel thread on */
	uint16_t	    group_id;   /* Group ID */
	unsigned	    mbuf_size;  /* mbuf size */
	struct odp_pci_addr addr;
	struct odp_pci_id   id;

	uint8_t force_bind : 1; /* Flag to bind kernel thread */
};

/**
 * KNI context
 */
struct odp_kni {
	char	   name[ODP_KNI_NAMESIZE];  /**< KNI interface name */
	uint16_t   group_id;                /**< Group ID of KNI devices */
	uint32_t   slot_id;                 /**< KNI pool slot ID */
	odp_pool_t pktmbuf_pool;            /**< pkt mbuf mempool */
	unsigned   mbuf_size;               /**< mbuf size */

	struct odp_kni_fifo *tx_q;          /**< TX queue */
	struct odp_kni_fifo *rx_q;          /**< RX queue */
	struct odp_kni_fifo *alloc_q;       /**< Allocated mbufs queue */
	struct odp_kni_fifo *free_q;        /**< To be freed mbufs queue */

	/* For request & response */
	struct odp_kni_fifo *req_q;         /**< Request queue */
	struct odp_kni_fifo *resp_q;        /**< Response queue */
	void		    *sync_addr;     /**< Req/Resp Mem address */

	struct odp_kni_ops ops;             /**< operations for request */
	uint8_t		   in_use : 1;      /**< kni in use */
};

typedef struct odp_kni pkt_kni_t;

/**
 * Initialize and preallocate KNI subsystem
 *
 * This function is to be executed on the MASTER core only, after EAL
 * initialization and before any KNI interface is attempted to be
 * allocated
 *
 * @param max_kni_ifaces
 *  The maximum number of KNI interfaces that can coexist concurrently
 */
int odp_kni_init(unsigned int max_kni_ifaces);

/**
 * Allocate KNI interface according to the port id, mbuf size, mbuf pool,
 * configurations and callbacks for kernel requests.The KNI interface created
 * in the kernel space is the net interface the traditional Linux application
 * talking to.
 *
 * The odp_kni_alloc shall not be called before odp_kni_init() has been
 * called. odp_kni_alloc is thread safe.
 *
 * @param pktmbuf_pool
 *  The mempool for allocting mbufs for packets.
 * @param conf
 *  The pointer to the configurations of the KNI device.
 * @param ops
 *  The pointer to the callbacks for the KNI kernel requests.
 *
 * @return
 *  - The pointer to the context of a KNI interface.
 *  - NULL indicate error.
 */
struct odp_kni *odp_kni_alloc(odp_pool_t		 pktmbuf_pool,
			      const struct odp_kni_conf *conf,
			      struct odp_kni_ops	*ops);

/**
 * It create a KNI device for specific port.
 *
 * Note: It is deprecated and just for backward compatibility.
 *
 * @param port_id
 *  Port ID.
 * @param mbuf_size
 *  mbuf size.
 * @param pktmbuf_pool
 *  The mempool for allocting mbufs for packets.
 * @param ops
 *  The pointer to the callbacks for the KNI kernel requests.
 *
 * @return
 *  - The pointer to the context of a KNI interface.
 *  - NULL indicate error.
 */
struct odp_kni *odp_kni_create(uint8_t		   port_id,
			       unsigned		   mbuf_size,
			       odp_pool_t	   pktmbuf_pool,
			       struct odp_kni_ops *ops);

/**
 * Release KNI interface according to the context. It will also release the
 * paired KNI interface in kernel space. All processing on the specific KNI
 * context need to be stopped before calling this interface.
 *
 * odp_kni_release is thread safe.
 *
 * @param kni
 *  The pointer to the context of an existent KNI interface.
 *
 * @return
 *  - 0 indicates success.
 *  - negative value indicates failure.
 */
int odp_kni_release(struct odp_kni *kni);

/**
 * It is used to handle the request mbufs sent from kernel space.
 * Then analyzes it and calls the specific actions for the specific requests.
 * Finally constructs the response mbuf and puts it back to the resp_q.
 *
 * @param kni
 *  The pointer to the context of an existent KNI interface.
 *
 * @return
 *  - 0
 *  - negative value indicates failure.
 */
int odp_kni_handle_request(struct odp_kni *kni);

/**
 * Retrieve a burst of packets from a KNI interface. The retrieved packets are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles the freeing of
 * the mbufs in the free queue of KNI interface.
 *
 * @param kni
 *  The KNI interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets retrieved.
 */
unsigned odp_kni_rx_burst(struct odp_kni *kni, odp_packet_t mbufs[],
			  unsigned num);

/**
 * Send a burst of packets to a KNI interface. The packets to be sent out are
 * stored in rte_mbuf structures whose pointers are supplied in the array of
 * mbufs, and the maximum number is indicated by num. It handles allocating
 * the mbufs for KNI interface alloc queue.
 *
 * @param kni
 *  The KNI interface context.
 * @param mbufs
 *  The array to store the pointers of mbufs.
 * @param num
 *  The maximum number per burst.
 *
 * @return
 *  The actual number of packets sent.
 */
unsigned odp_kni_tx_burst(struct odp_kni *kni, odp_packet_t mbufs[],
			  unsigned num);

/**
 * Get the port id from KNI interface.
 *
 * Note: It is deprecated and just for backward compatibility.
 *
 * @param kni
 *  The KNI interface context.
 *
 * @return
 *  On success: The port id.
 *  On failure: ~0x0
 */
uint8_t odp_kni_get_port_id(struct odp_kni *kni) __attribute__ ((deprecated));

/**
 * Get the KNI context of its name.
 *
 * @param name
 *  pointer to the KNI device name.
 *
 * @return
 *  On success: Pointer to KNI interface.
 *  On failure: NULL.
 */
struct odp_kni *odp_kni_get(const char *name);

/**
 * Get the name given to a KNI device
 *
 * @param kni
 *   The KNI instance to query
 * @return
 *   The pointer to the KNI name
 */
const char *odp_kni_get_name(const struct odp_kni *kni);

/**
 * Get the KNI context of the specific port.
 *
 * Note: It is deprecated and just for backward compatibility.
 *
 * @param port_id
 *  the port id.
 *
 * @return
 *  On success: Pointer to KNI interface.
 *  On failure: NULL
 */
struct odp_kni *odp_kni_info_get(uint8_t port_id) __attribute__ ((deprecated));

/**
 * Register KNI request handling for a specified port,and it can
 * be called by master process or slave process.
 *
 * @param kni
 *  pointer to struct odp_kni.
 * @param ops
 *  ponter to struct odp_kni_ops.
 *
 * @return
 *  On success: 0
 *  On failure: -1
 */
int odp_kni_register_handlers(struct odp_kni *kni, struct odp_kni_ops *ops);

/**
 *  Unregister KNI request handling for a specified port.
 *
 *  @param kni
 *   pointer to struct odp_kni.
 *
 *  @return
 *   On success: 0
 *   On failure: -1
 */
int odp_kni_unregister_handlers(struct odp_kni *kni);

/**
 *  Close KNI device.
 */
void odp_kni_close(void);

#ifdef __cplusplus
}
#endif
#endif /* _ODP_KNI_H_ */
