/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP packet IO - implementation internal
 */

#ifndef ODP_PACKET_IO_INTERNAL_H_
#define ODP_PACKET_IO_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/spinlock.h>
#include <odp/ticketlock.h>
#include <odp_classification_datamodel.h>
#include <odp_align_internal.h>
#include <odp_debug_internal.h>
#include <odp_kni.h>
#include <odp_pkt_uio.h>
#include <odp/config.h>
#include <odp/hints.h>
#include <net/if.h>

#define PKTIO_MAX_QUEUES 64
#include <odp_packet_socket.h>
#include <odp_packet_netmap.h>
#include <odp_packet_tap.h>

#define PKTIO_NAME_LEN 256

#define PKTIN_INVALID  ((odp_pktin_queue_t) {ODP_PKTIO_INVALID, 0})
#define PKTOUT_INVALID ((odp_pktout_queue_t) {ODP_PKTIO_INVALID, 0})

/** Determine if a socket read/write error should be reported. Transient errors
 *  that simply require the caller to retry are ignored, the _send/_recv APIs
 *  are non-blocking and it is the caller's responsibility to retry if the
 *  requested number of packets were not handled. */
#define SOCK_ERR_REPORT(e) (e != EAGAIN && e != EWOULDBLOCK && e != EINTR)

/* Forward declaration */
struct pktio_if_ops;

typedef struct {
	odp_queue_t loopq;		/**< loopback queue for "loop" device */
	odp_bool_t promisc;		/**< promiscuous mode state */
} pkt_loop_t;


#ifdef HAVE_PCAP
typedef struct {
	char *fname_rx;		/**< name of pcap file for rx */
	char *fname_tx;		/**< name of pcap file for tx */
	void *rx;		/**< rx pcap handle */
	void *tx;		/**< tx pcap handle */
	void *tx_dump;		/**< tx pcap dumper handle */
	odp_pool_t pool;	/**< rx pool */
	unsigned char *buf;	/**< per-pktio temp buffer */
	int loops;		/**< number of times to loop rx pcap */
	int loop_cnt;		/**< number of loops completed */
	odp_bool_t promisc;	/**< promiscuous mode state */
} pkt_pcap_t;
#endif

struct pktio_entry {
	const struct pktio_if_ops *ops; /**< Implementation specific methods */
	/* These two locks together lock the whole pktio device */
	odp_ticketlock_t rxl;		/**< RX ticketlock */
	odp_ticketlock_t txl;		/**< TX ticketlock */
	int taken;			/**< is entry taken(1) or free(0) */
	int cls_enabled;		/**< is classifier enabled */
	odp_pktio_t handle;		/**< pktio handle */
	odp_queue_t outq_default;	/**< default out queue */
	union {
		pkt_loop_t pkt_loop;            /**< Using loopback for IO */
		pkt_sock_t pkt_sock;		/**< using socket API for IO */
		pkt_sock_mmap_t pkt_sock_mmap;	/**< using socket mmap
						 *   API for IO */
		pkt_netmap_t pkt_nm;		/**< using netmap API for IO */
#ifdef HAVE_PCAP
		pkt_pcap_t pkt_pcap;		/**< Using pcap for IO */
#endif
		pkt_odp_t pkt_odp;
		pkt_tap_t pkt_tap;		/**< using TAP for IO */
		pkt_kni_t  pkt_kni;
	};
	enum {
		STATE_START = 0,
		STATE_STOP
	} state;
	classifier_t cls[MAX_CLS_SUPPORT];	/**< classifier linked */
						/**< with this pktio */
	odp_pktio_stats_t stats;	/**< statistic counters for pktio */
	enum {
		STATS_SYSFS = 0,
		STATS_ETHTOOL,
		STATS_UNSUPPORTED
	} stats_type;
	char name[PKTIO_NAME_LEN];	/**< name of pktio provided to
					   pktio_open() */
	odp_pktio_t id;
	odp_pktio_param_t param;

	/* Storage for queue handles
	 * Multi-queue support is pktio driver specific */
	unsigned num_in_queue;
	unsigned num_out_queue;

	struct {
		odp_queue_t        queue;
		odp_pktin_queue_t  pktin;
	} in_queue[PKTIO_MAX_QUEUES];

	struct {
		odp_pktout_queue_t pktout;
	} out_queue[PKTIO_MAX_QUEUES];
};

typedef union {
	struct pktio_entry s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct pktio_entry))];
} pktio_entry_t;

typedef struct {
	odp_spinlock_t lock;
	pktio_entry_t entries[ODP_CONFIG_PKTIO_ENTRIES];
} pktio_table_t;

int is_free(pktio_entry_t *entry);

typedef struct pktio_if_ops {
	const char *name;
	int (*init)(void);
	int (*term)(void);
	int (*open)(odp_pktio_t pktio, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool);
	int (*close)(pktio_entry_t *pktio_entry);
	int (*start)(pktio_entry_t *pktio_entry);
	int (*stop)(pktio_entry_t *pktio_entry);
	int (*stats)(pktio_entry_t *pktio_entry, odp_pktio_stats_t *stats);
	int (*stats_reset)(pktio_entry_t *pktio_entry);
	int (*recv)(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len);
	int (*send)(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len);
	int (*mtu_get)(pktio_entry_t *pktio_entry);
	int (*promisc_mode_set)(pktio_entry_t *pktio_entry,  int enable);
	int (*promisc_mode_get)(pktio_entry_t *pktio_entry);
	int (*mac_get)(pktio_entry_t *pktio_entry, void *mac_addr);
	int (*link_status)(pktio_entry_t *pktio_entry);
	int (*capability)(pktio_entry_t *pktio_entry,
			  odp_pktio_capability_t *capa);
	int (*input_queues_config)(pktio_entry_t *pktio_entry,
				   const odp_pktin_queue_param_t *param);
	int (*output_queues_config)(pktio_entry_t *pktio_entry,
				    const odp_pktout_queue_param_t *p);
	int (*in_queues)(pktio_entry_t *entry, odp_queue_t queues[], int num);
	int (*pktin_queues)(pktio_entry_t *entry, odp_pktin_queue_t queues[],
			    int num);
	int (*pktout_queues)(pktio_entry_t *entry, odp_pktout_queue_t queues[],
			     int num);
	int (*recv_queue)(pktio_entry_t *entry, int index,
			  odp_packet_t packets[], int num);
	int (*send_queue)(pktio_entry_t *entry, int index,
			  odp_packet_t packets[], int num);
} pktio_if_ops_t;

int _odp_packet_cls_enq(pktio_entry_t *pktio_entry, const uint8_t *base,
			uint16_t buf_len, odp_packet_t *pkt_ret);

extern void *pktio_entry_ptr[];

static inline int pktio_to_id(odp_pktio_t pktio)
{
	return _odp_typeval(pktio) - 1;
}

static inline pktio_entry_t *get_pktio_entry(odp_pktio_t pktio)
{
	if (odp_unlikely(pktio == ODP_PKTIO_INVALID))
		return NULL;

	if (odp_unlikely(_odp_typeval(pktio) > ODP_CONFIG_PKTIO_ENTRIES)) {
		ODP_DBG("pktio limit %d/%d exceed\n",
			_odp_typeval(pktio), ODP_CONFIG_PKTIO_ENTRIES);
		return NULL;
	}

	return pktio_entry_ptr[pktio_to_id(pktio)];
}

static inline int pktio_cls_enabled(pktio_entry_t *entry, int index)
{
	return entry->s.cls_enabled & (1 << index);
}

static inline void pktio_cls_enabled_set(pktio_entry_t *entry, int index)
{
	entry->s.cls_enabled |= (1 << index);
}

static inline void pktio_cls_enabled_init(pktio_entry_t *entry)
{
	entry->s.cls_enabled = 0;
}

int pktin_poll(pktio_entry_t *entry, int num_queue, int index[]);

/*
 * Dummy single queue implementations of multi-queue API
 */
int single_capability(odp_pktio_capability_t *capa);
int single_input_queues_config(pktio_entry_t *entry,
			       const odp_pktin_queue_param_t *param);
int single_output_queues_config(pktio_entry_t *entry,
				const odp_pktout_queue_param_t *param);
int single_in_queues(pktio_entry_t *entry, odp_queue_t queues[], int num);
int single_pktin_queues(pktio_entry_t *entry, odp_pktin_queue_t queues[],
			int num);
int single_pktout_queues(pktio_entry_t *entry, odp_pktout_queue_t queues[],
			 int num);
int single_recv_queue(pktio_entry_t *entry, int index, odp_packet_t packets[],
		      int num);
int single_send_queue(pktio_entry_t *entry, int index, odp_packet_t packets[],
		      int num);

extern const pktio_if_ops_t netmap_pktio_ops;
extern const pktio_if_ops_t sock_mmsg_pktio_ops;
extern const pktio_if_ops_t sock_mmap_pktio_ops;
extern const pktio_if_ops_t loopback_pktio_ops;
#ifdef HAVE_PCAP
extern const pktio_if_ops_t pcap_pktio_ops;
#endif
extern const pktio_if_ops_t tap_pktio_ops;
extern const pktio_if_ops_t * const pktio_if_ops[];
extern const pktio_if_ops_t hns_eth_pktio_ops;
extern const pktio_if_ops_t kni_pktio_ops;
extern const pktio_if_ops_t ixgbe_eth_pktio_ops;

int sysfs_stats(pktio_entry_t *pktio_entry,
		odp_pktio_stats_t *stats);
int sock_stats_fd(pktio_entry_t *pktio_entry,
		  odp_pktio_stats_t *stats,
		  int fd);
int sock_stats_reset_fd(pktio_entry_t *pktio_entry, int fd);

#ifdef __cplusplus
}
#endif

#endif
