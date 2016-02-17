/* for io_module_func def'ns */
#include "io_module.h"
#ifndef DISABLE_DPDK
/* for mtcp related def'ns */
#include "mtcp.h"
/* for errno */
#include <errno.h>
/* for logging */
#include "debug.h"
/* for num_devices_* */
#include "config.h"
/* for rte_max_eth_ports */
#include <rte_common.h>
/* for rte_eth_rxconf */
#include <rte_ethdev.h>
/* for delay funcs */
#include <rte_cycles.h>
#define ENABLE_STATS_IOCTL		1
#ifdef ENABLE_STATS_IOCTL
/* for close */
#include <unistd.h>
/* for open */
#include <fcntl.h>
/* for ioctl */
#include <sys/ioctl.h>
#endif /* !ENABLE_STATS_IOCTL */
/*----------------------------------------------------------------------------*/
/* Essential macros */
#define MAX_RX_QUEUE_PER_LCORE		MAX_CPUS
#define MAX_TX_QUEUE_PER_PORT		MAX_CPUS

#define MBUF_SIZE 			(2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF				8192
#define MEMPOOL_CACHE_SIZE		256

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 			8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 			8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 			4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 			36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH			0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH			0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST			64/*128*/

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT	128
#define RTE_TEST_TX_DESC_DEFAULT	128

static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;
/*----------------------------------------------------------------------------*/
/* packet memory pools for storing packet bufs */
static struct rte_mempool *pktmbuf_pool[MAX_CPUS] = {NULL};

//#define DEBUG				1
#ifdef DEBUG
/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
#endif

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= 	ETH_MQ_RX_RSS,
		.max_rx_pkt_len = 	ETHER_MAX_LEN,
		.split_hdr_size = 	0,
		.header_split   = 	0, /**< Header Split disabled */
		.hw_ip_checksum = 	1, /**< IP checksum offload enabled */
		.hw_vlan_filter = 	0, /**< VLAN filtering disabled */
		.jumbo_frame    = 	0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 	1, /**< CRC stripped by hardware */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = 	NULL,
			.rss_hf = 	ETH_RSS_TCP
		},
	},
	.txmode = {
		.mq_mode = 		ETH_MQ_TX_NONE,
	},
#if 0
	.fdir_conf = {
                .mode = RTE_FDIR_MODE_PERFECT,
                .pballoc = RTE_FDIR_PBALLOC_256K,
                .status = RTE_FDIR_REPORT_STATUS_ALWAYS,
                //.flexbytes_offset = 0x6,
                .drop_queue = 127,
        },
#endif
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = 		RX_PTHRESH, /* RX prefetch threshold reg */
		.hthresh = 		RX_HTHRESH, /* RX host threshold reg */
		.wthresh = 		RX_WTHRESH, /* RX write-back threshold reg */
	},
	.rx_free_thresh = 		32,
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = 		TX_PTHRESH, /* TX prefetch threshold reg */
		.hthresh = 		TX_HTHRESH, /* TX host threshold reg */
		.wthresh = 		TX_WTHRESH, /* TX write-back threshold reg */
	},
	.tx_free_thresh = 		0, /* Use PMD default values */
	.tx_rs_thresh = 		0, /* Use PMD default values */
	/*
	 * As the example won't handle mult-segments and offload cases,
	 * set the flag by default.
	 */
	.txq_flags = 			0x0,
};

struct mbuf_table {
	unsigned len; /* length of queued packets */
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct dpdk_private_context {
	struct mbuf_table rmbufs[RTE_MAX_ETHPORTS];
	struct mbuf_table wmbufs[RTE_MAX_ETHPORTS];
	struct rte_mempool *pktmbuf_pool;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
#ifdef ENABLE_STATS_IOCTL
	int fd;
#endif /* !ENABLE_STATS_IOCTL */
} __rte_cache_aligned;

#ifdef ENABLE_STATS_IOCTL
/**
 * stats struct passed on from user space to the driver
 */
struct stats_struct {
	uint64_t tx_bytes;
	uint64_t tx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_pkts;
	uint8_t qid;
	uint8_t dev;
};
#endif /* !ENABLE_STATS_IOCTL */
/*----------------------------------------------------------------------------*/
void
dpdk_init_handle(struct mtcp_thread_context *ctxt)
{
	struct dpdk_private_context *dpc;
	int i, j;
	char mempool_name[20];

	/* create and initialize private I/O module context */
	ctxt->io_private_context = calloc(1, sizeof(struct dpdk_private_context));
	if (ctxt->io_private_context == NULL) {
		TRACE_ERROR("Failed to initialize ctxt->io_private_context: "
			    "Can't allocate memory\n");
		exit(EXIT_FAILURE);
	}
	
	sprintf(mempool_name, "mbuf_pool-%d", ctxt->cpu);
	dpc = (struct dpdk_private_context *)ctxt->io_private_context;
	dpc->pktmbuf_pool = pktmbuf_pool[ctxt->cpu];

	/* set wmbufs correctly */
	for (j = 0; j < num_devices_attached; j++) {
		/* Allocate wmbufs for each registered port */
		for (i = 0; i < MAX_PKT_BURST; i++) {
			dpc->wmbufs[j].m_table[i] = rte_pktmbuf_alloc(pktmbuf_pool[ctxt->cpu]);
			if (dpc->wmbufs[j].m_table[i] == NULL) {
				TRACE_ERROR("Failed to allocate %d:wmbuf[%d] on device %d!\n",
					    ctxt->cpu, i, j);
				exit(EXIT_FAILURE);
			}
		}
		/* set mbufs queue length to 0 to begin with */
		dpc->wmbufs[j].len = 0;
	}

#ifdef ENABLE_STATS_IOCTL
	dpc->fd = open("/dev/dpdk-iface", O_RDWR);
	if (dpc->fd == -1) {
		TRACE_ERROR("Can't open /dev/dpdk-iface for context->cpu: %d!\n",
			    ctxt->cpu);
		exit(EXIT_FAILURE);
	}
#endif /* !ENABLE_STATS_IOCTL */
}
/*----------------------------------------------------------------------------*/
int
dpdk_link_devices(struct mtcp_thread_context *ctxt)
{
	/* linking takes place during mtcp_init() */
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void
dpdk_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
	/* 
	 * do nothing over here - memory reclamation
	 * will take place in dpdk_recv_pkts 
	 */
}
/*----------------------------------------------------------------------------*/
int
dpdk_send_pkts(struct mtcp_thread_context *ctxt, int nif)
{
	struct dpdk_private_context *dpc;
	mtcp_manager_t mtcp;
	int ret, i;
	
	dpc = (struct dpdk_private_context *)ctxt->io_private_context;
	mtcp = ctxt->mtcp_manager;
	ret = 0;
	
	/* if there are packets in the queue... flush them out to the wire */
	if (dpc->wmbufs[nif].len >/*= MAX_PKT_BURST*/ 0) {
		struct rte_mbuf **pkts;
#ifdef ENABLE_STATS_IOCTL
		struct stats_struct ss;
#endif /* !ENABLE_STATS_IOCTL */
		int cnt = dpc->wmbufs[nif].len;
		pkts = dpc->wmbufs[nif].m_table;
#ifdef NETSTAT
		mtcp->nstat.tx_packets[nif] += cnt;
#ifdef ENABLE_STATS_IOCTL
		ss.tx_pkts = mtcp->nstat.tx_packets[nif];
		ss.tx_bytes = mtcp->nstat.tx_bytes[nif];
		ss.rx_pkts = mtcp->nstat.rx_packets[nif];
		ss.rx_bytes = mtcp->nstat.rx_bytes[nif];
		ss.qid = ctxt->cpu;
		ss.dev = nif;
		ioctl(dpc->fd, 0, &ss);
#endif /* !ENABLE_STATS_IOCTL */
#endif
		do {
			/* tx cnt # of packets */
			ret = rte_eth_tx_burst(nif, ctxt->cpu, 
					       pkts, cnt);
			pkts += ret;
			cnt -= ret;
			/* if not all pkts were sent... then repeat the cycle */
		} while (cnt > 0);

		/* time to allocate fresh mbufs for the queue */
		for (i = 0; i < dpc->wmbufs[nif].len; i++) {
			dpc->wmbufs[nif].m_table[i] = rte_pktmbuf_alloc(pktmbuf_pool[ctxt->cpu]);
			/* error checking */
			if (unlikely(dpc->wmbufs[nif].m_table[i] == NULL)) {
				TRACE_ERROR("Failed to allocate %d:wmbuf[%d] on device %d!\n",
					    ctxt->cpu, i, nif);
				exit(EXIT_FAILURE);
			}
		}
		/* reset the len of mbufs var after flushing of packets */
		dpc->wmbufs[nif].len = 0;
	}
	
	return ret;
}
/*----------------------------------------------------------------------------*/
uint8_t *
dpdk_get_wptr(struct mtcp_thread_context *ctxt, int nif, uint16_t pktsize)
{
	struct dpdk_private_context *dpc;
	mtcp_manager_t mtcp;
	struct rte_mbuf *m;
	uint8_t *ptr;
	int len_of_mbuf;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;
	mtcp = ctxt->mtcp_manager;
	
	/* sanity check */
	if (unlikely(dpc->wmbufs[nif].len == MAX_PKT_BURST))
		return NULL;

	len_of_mbuf = dpc->wmbufs[nif].len;
	m = dpc->wmbufs[nif].m_table[len_of_mbuf];
	
	/* retrieve the right write offset */
	ptr = (void *)rte_pktmbuf_mtod(m, struct ether_hdr *);
	m->pkt_len = m->data_len = pktsize;
	m->nb_segs = 1;
	m->next = NULL;

#ifdef NETSTAT
	mtcp->nstat.tx_bytes[nif] += pktsize + 24;
#endif
	
	/* increment the len_of_mbuf var */
	dpc->wmbufs[nif].len = len_of_mbuf + 1;
	
	return (uint8_t *)ptr;
}
/*----------------------------------------------------------------------------*/
static inline void
free_pkts(struct rte_mbuf **mtable, unsigned len)
{
	int i;

	/* free the freaking packets */
	for (i = 0; i < len; i++) {
		rte_pktmbuf_free_seg(mtable[i]);
		RTE_MBUF_PREFETCH_TO_FREE(mtable[i+1]);
	}
}
/*----------------------------------------------------------------------------*/
int32_t
dpdk_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
	struct dpdk_private_context *dpc;
	int ret;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;

	if (dpc->rmbufs[ifidx].len != 0) {
		free_pkts(dpc->rmbufs[ifidx].m_table, dpc->rmbufs[ifidx].len);
		dpc->rmbufs[ifidx].len = 0;
	}

	ret = rte_eth_rx_burst((uint8_t)ifidx, ctxt->cpu,
			       dpc->pkts_burst, MAX_PKT_BURST);

	dpc->rmbufs[ifidx].len = ret;

	return ret;
}
/*----------------------------------------------------------------------------*/
uint8_t *
dpdk_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
{
	struct dpdk_private_context *dpc;
	struct rte_mbuf *m;
	uint8_t *pktbuf;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;	


	m = dpc->pkts_burst[index];
	//rte_prefetch0(rte_pktmbuf_mtod(m, void *));
	*len = m->pkt_len;
	pktbuf = rte_pktmbuf_mtod(m, uint8_t *);

	/* enqueue the pkt ptr in mbuf */
	dpc->rmbufs[ifidx].m_table[index] = m;

	return pktbuf;
}
/*----------------------------------------------------------------------------*/
int32_t
dpdk_select(struct mtcp_thread_context *ctxt)
{
	/* this is empty as dpdk is non-blocking */

	return 0;
}
/*----------------------------------------------------------------------------*/
void
dpdk_destroy_handle(struct mtcp_thread_context *ctxt)
{
	struct dpdk_private_context *dpc;
	int i;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;	

	/* free wmbufs */
	for (i = 0; i < num_devices_attached; i++)
		free_pkts(dpc->wmbufs[i].m_table, MAX_PKT_BURST);

#ifdef ENABLE_STATS_IOCTL
	/* free fd */
	close(dpc->fd);
#endif /* !ENABLE_STATS_IOCTL */

	/* free it all up */
	free(dpc);
}
/*----------------------------------------------------------------------------*/
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 			100 /* 100ms */
#define MAX_CHECK_TIME 			90 /* 9s (90 * 100ms) in total */

	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}
/*----------------------------------------------------------------------------*/
#if 0
static void
dpdk_enable_fdir(int portid, uint8_t is_master)
{
	struct rte_fdir_masks fdir_masks;
	struct rte_fdir_filter fdir_filter;
	int ret;
	
	memset(&fdir_filter, 0, sizeof(struct rte_fdir_filter));
	fdir_filter.iptype = RTE_FDIR_IPTYPE_IPV4;
	fdir_filter.l4type = RTE_FDIR_L4TYPE_TCP;
	fdir_filter.ip_dst.ipv4_addr = CONFIG.eths[portid].ip_addr;
	
	if (is_master) {
		memset(&fdir_masks, 0, sizeof(struct rte_fdir_masks));
		fdir_masks.src_ipv4_mask = 0x0;
		fdir_masks.dst_ipv4_mask = 0xFFFFFFFF;
		fdir_masks.src_port_mask = 0x0;
		fdir_masks.dst_port_mask = 0x0;
		
		/*
		 * enable the following if the filter is IP-only
		 * (non-TCP, non-UDP)
		 */
		/* fdir_masks.only_ip_flow = 1; */
		rte_eth_dev_fdir_set_masks(portid, &fdir_masks);
		ret = rte_eth_dev_fdir_add_perfect_filter(portid,
							  &fdir_filter,
							  0,
							  CONFIG.multi_process_curr_core,
							  0);
	} else {
		ret = rte_eth_dev_fdir_update_perfect_filter(portid,
							     &fdir_filter,
							     0,
							     CONFIG.multi_process_curr_core,
							     0);
	}
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			 "fdir_add_perfect_filter_t call failed!: %d\n",
			 ret);
	}
	fprintf(stderr, "Filter for device ifidx: %d added\n", portid);
}
#endif
/*----------------------------------------------------------------------------*/
void
dpdk_load_module(void)
{
	int portid, rxlcore_id, ret;
	/* setting the rss key */
	static const uint8_t key[] = {
		0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
	};

	port_conf.rx_adv_conf.rss_conf.rss_key = (uint8_t *)&key;
	port_conf.rx_adv_conf.rss_conf.rss_key_len = sizeof(key);

	if (!CONFIG.multi_process || (CONFIG.multi_process && CONFIG.multi_process_is_master)) {
		for (rxlcore_id = 0; rxlcore_id < CONFIG.num_cores; rxlcore_id++) {
			char name[20];
			sprintf(name, "mbuf_pool-%d", rxlcore_id);
			/* create the mbuf pools */
			pktmbuf_pool[rxlcore_id] =
				rte_mempool_create(name, NB_MBUF,
						   MBUF_SIZE, MEMPOOL_CACHE_SIZE,
						   sizeof(struct rte_pktmbuf_pool_private),
						   rte_pktmbuf_pool_init, NULL,
						   rte_pktmbuf_init, NULL,
						   rte_socket_id(), 0);
			if (pktmbuf_pool[rxlcore_id] == NULL)
				rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");	
		}
		
		/* Initialise each port */
		for (portid = 0; portid < num_devices_attached; portid++) {
			/* init port */
			printf("Initializing port %u... ", (unsigned) portid);
			fflush(stdout);
			ret = rte_eth_dev_configure(portid, CONFIG.num_cores, CONFIG.num_cores, &port_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
					 ret, (unsigned) portid);
			
			/* init one RX queue per CPU */
			fflush(stdout);
#ifdef DEBUG
			rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
#endif
			for (rxlcore_id = 0; rxlcore_id < CONFIG.num_cores; rxlcore_id++) {
				ret = rte_eth_rx_queue_setup(portid, rxlcore_id, nb_rxd,
							     rte_eth_dev_socket_id(portid), &rx_conf,
							     pktmbuf_pool[rxlcore_id]);
				if (ret < 0)
					rte_exit(EXIT_FAILURE, 
						 "rte_eth_rx_queue_setup:err=%d, port=%u, queueid: %d\n",
						 ret, (unsigned) portid, rxlcore_id);
			}
			
			/* init one TX queue on each port per CPU (this is redundant for this app) */
			fflush(stdout);
			for (rxlcore_id = 0; rxlcore_id < CONFIG.num_cores; rxlcore_id++) {
				ret = rte_eth_tx_queue_setup(portid, rxlcore_id, nb_txd,
							     rte_eth_dev_socket_id(portid), &tx_conf);
				if (ret < 0)
					rte_exit(EXIT_FAILURE, 
						 "rte_eth_tx_queue_setup:err=%d, port=%u, queueid: %d\n",
						 ret, (unsigned) portid, rxlcore_id);
			}
			
			/* Start device */
			ret = rte_eth_dev_start(portid);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
					 ret, (unsigned) portid);
			
			printf("done: \n");
			rte_eth_promiscuous_enable(portid);
			
#ifdef DEBUG
			printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
			       (unsigned) portid,
			       ports_eth_addr[portid].addr_bytes[0],
			       ports_eth_addr[portid].addr_bytes[1],
			       ports_eth_addr[portid].addr_bytes[2],
			       ports_eth_addr[portid].addr_bytes[3],
			       ports_eth_addr[portid].addr_bytes[4],
			       ports_eth_addr[portid].addr_bytes[5]);
#endif
#if 0
			/* if multi-process support is enabled, then turn on FDIR */
			if (CONFIG.multi_process)
				dpdk_enable_fdir(portid, CONFIG.multi_process_is_master);
#endif
		}
	} else { /* CONFIG.multi_process && !CONFIG.multi_process_is_master */
		for (rxlcore_id = 0; rxlcore_id < CONFIG.num_cores; rxlcore_id++) {
                        char name[20];
                        sprintf(name, "mbuf_pool-%d", rxlcore_id);
                        /* initialize the mbuf pools */
                        pktmbuf_pool[rxlcore_id] =
                                rte_mempool_lookup(name);
                        if (pktmbuf_pool[rxlcore_id] == NULL)
                                rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
                }
#if 0
                for (portid = 0; portid < num_devices_attached; portid++)
			dpdk_enable_fdir(portid, CONFIG.multi_process_is_master);
#endif
	}
	
	check_all_ports_link_status(num_devices_attached, 0xFFFFFFFF);
}
/*----------------------------------------------------------------------------*/
io_module_func dpdk_module_func = {
	.load_module		   = dpdk_load_module,
	.init_handle		   = dpdk_init_handle,
	.link_devices		   = dpdk_link_devices,
	.release_pkt		   = dpdk_release_pkt,
	.send_pkts		   = dpdk_send_pkts,
	.get_wptr   		   = dpdk_get_wptr,
	.recv_pkts		   = dpdk_recv_pkts,
	.get_rptr	   	   = dpdk_get_rptr,
	.select			   = dpdk_select,
	.destroy_handle		   = dpdk_destroy_handle
};
/*----------------------------------------------------------------------------*/
#else
io_module_func dpdk_module_func = {
	.load_module		   = NULL,
	.init_handle		   = NULL,
	.link_devices		   = NULL,
	.release_pkt		   = NULL,
	.send_pkts		   = NULL,
	.get_wptr   		   = NULL,
	.recv_pkts		   = NULL,
	.get_rptr	   	   = NULL,
	.select			   = NULL,
	.destroy_handle		   = NULL
};
/*----------------------------------------------------------------------------*/
#endif /* !DISABLE_DPDK */
