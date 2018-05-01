/* for io_module_func def'ns */
#include "io_module.h"
#ifdef ENABLE_ONVM
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
#include <rte_errno.h>
#define ENABLE_STATS_IOCTL		1
#ifdef ENABLE_STATS_IOCTL
/* for close */
#include <unistd.h>
/* for open */
#include <fcntl.h>
/* for ioctl */
#include <sys/ioctl.h>
#endif /* !ENABLE_STATS_IOCTL */
/* for ip pseudo-chksum */
#include <rte_ip.h>

/* for onvm rings */
#include <onvm_nflib.h>
#include <onvm_pkt_helper.h>

/*----------------------------------------------------------------------------*/
/* Essential macros */
//#define MAX_RX_QUEUE_PER_LCORE	MAX_CPUS
//#define MAX_TX_QUEUE_PER_PORT		MAX_CPUS
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"

#ifdef ENABLELRO
#define MBUF_SIZE			(16384 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#else
#define MBUF_SIZE			(2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#endif /* !ENABLELRO */
#define NB_MBUF				8192
#define MEMPOOL_CACHE_SIZE		256
//#define RX_IDLE_ENABLE			1
#define RX_IDLE_TIMEOUT			1	/* in micro-seconds */
#define RX_IDLE_THRESH			64
#define MAX_PKT_BURST			((uint16_t)32)/*64*//*128*/

/*
 * Configurable number of RX/TX ring descriptors
 */
//#define RTE_TEST_RX_DESC_DEFAULT	128
//#define RTE_TEST_TX_DESC_DEFAULT	128

/*----------------------------------------------------------------------------*/
/* packet memory pool for storing packet bufs */
static struct rte_mempool *pktmbuf_pool = NULL;

//#define DEBUG				1
#ifdef DEBUG
/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
#endif

static struct rte_eth_dev_info dev_info[RTE_MAX_ETHPORTS];

struct mbuf_table {
	unsigned len; /* length of queued packets */
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct dpdk_private_context {
	struct mbuf_table rmbufs[RTE_MAX_ETHPORTS];
	struct mbuf_table wmbufs[RTE_MAX_ETHPORTS];
	struct rte_mempool *pktmbuf_pool;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
#ifdef RX_IDLE_ENABLE
	uint8_t rx_idle;
#endif
#ifdef ENABLELRO
	struct rte_mbuf *cur_rx_m;
#endif
#ifdef ENABLE_STATS_IOCTL
	int fd;
        uint32_t cur_ts;
#endif /* !ENABLE_STATS_IOCTL */
} __rte_cache_aligned;

/* onvm structs */
struct onvm_nf_info *nf_info;
struct rte_ring *rx_ring;
struct rte_ring *tx_ring;
volatile struct onvm_nf *nf;

#ifdef ENABLE_STATS_IOCTL
/**
 * stats struct passed on from user space to the driver
 */
struct stats_struct {
	uint64_t tx_bytes;
	uint64_t tx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_pkts;
        uint64_t rmiss;
        uint64_t rerr;
        uint64_t terr;
	uint8_t qid;
	uint8_t dev;

};
#endif /* !ENABLE_STATS_IOCTL */
/*----------------------------------------------------------------------------*/
void
onvm_init_handle(struct mtcp_thread_context *ctxt)
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
	dpc->pktmbuf_pool = pktmbuf_pool;

	/* Complete onvm handshake */
	onvm_nflib_nf_ready(nf_info);
	
	/* Initialize onvm rings*/
	nf = onvm_nflib_get_nf(nf_info->instance_id);	
	rx_ring = nf->rx_q;
	tx_ring = nf->tx_q;

	/* set wmbufs correctly */
	for (j = 0; j < num_devices_attached; j++) {
		/* Allocate wmbufs for each registered port */
			for (i = 0; i < MAX_PKT_BURST; i++) {
			dpc->wmbufs[j].m_table[i] = rte_pktmbuf_alloc(pktmbuf_pool);
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
		TRACE_ERROR("Can't open /dev/dpdk-iface for context->cpu: %d! "
			    "Are you using mlx4/mlx5 driver?\n",
			    ctxt->cpu);
	}
#endif /* !ENABLE_STATS_IOCTL */
}
/*----------------------------------------------------------------------------*/
int
onvm_link_devices(struct mtcp_thread_context *ctxt)
{
	/* linking takes place during mtcp_init() */
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void
onvm_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
	/* 
	 * do nothing over here - memory reclamation
	 * will take place in onvm_recv_pkts 
	 */
}
/*----------------------------------------------------------------------------*/
int
onvm_send_pkts(struct mtcp_thread_context *ctxt, int nif)
{
	struct dpdk_private_context *dpc;
	mtcp_manager_t mtcp;
	int ret, i;
	struct onvm_pkt_meta* meta;
	struct onvm_ft_ipv4_5tuple key;
        int ifidx;

        ifidx = nif;	
	dpc = (struct dpdk_private_context *)ctxt->io_private_context;
	mtcp = ctxt->mtcp_manager;
	ret = 0;
	
	/* if there are packets in the queue... flush them out to the wire */
	if (dpc->wmbufs[nif].len >/*= MAX_PKT_BURST*/ 0) {
		struct rte_mbuf **pkts;
#ifdef ENABLE_STATS_IOCTL
                struct rte_eth_stats stats;
		struct stats_struct ss;
#endif /* !ENABLE_STATS_IOCTL */
		int cnt = dpc->wmbufs[nif].len;
		pkts = dpc->wmbufs[nif].m_table;
#ifdef NETSTAT
		mtcp->nstat.tx_packets[nif] += cnt;
#ifdef ENABLE_STATS_IOCTL
		/* only pass stats after >= 1 sec interval */
		if (abs(mtcp->cur_ts - dpc->cur_ts) >= 1000 &&
		    likely(dpc->fd >= 0)) {
			/* rte_get_stats is global func, use only for 1 core */
			if (ctxt->cpu == 0) {
				rte_eth_stats_get(CONFIG.eths[ifidx].ifindex, &stats);
				ss.rmiss = stats.imissed;
				ss.rerr = stats.ierrors;
				ss.terr = stats.oerrors;
			} else 
				ss.rmiss = ss.rerr = ss.terr = 0;
			
			ss.tx_pkts = mtcp->nstat.tx_packets[ifidx];
			ss.tx_bytes = mtcp->nstat.tx_bytes[ifidx];
			ss.rx_pkts = mtcp->nstat.rx_packets[ifidx];
			ss.rx_bytes = mtcp->nstat.rx_bytes[ifidx];
			ss.qid = ctxt->cpu;
			ss.dev = CONFIG.eths[ifidx].ifindex;
			/* pass the info now */
			ioctl(dpc->fd, 0, &ss);
			dpc->cur_ts = mtcp->cur_ts;
			if (ctxt->cpu == 0)
				rte_eth_stats_reset(CONFIG.eths[ifidx].ifindex);
		}
#endif /* !ENABLE_STATS_IOCTL */
#endif
		
		for (i = 0; i < cnt; i++) {
			meta = onvm_get_pkt_meta(pkts[i]);
			if (CONFIG.onvm_dest == (uint16_t) -1) {
				meta->action = ONVM_NF_ACTION_OUT;
				meta->destination = CONFIG.eths[nif].ifindex;
			} else {
				onvm_ft_fill_key(&key, pkts[i]);
				pkts[i]->hash.rss = onvm_softrss(&key);
				meta->action = ONVM_NF_ACTION_TONF;
				meta->destination = CONFIG.onvm_dest;
			}
		}
		ret = rte_ring_enqueue_bulk(tx_ring, (void * const*)pkts ,cnt, NULL);
		if (cnt > 0 && ret == 0) {
			TRACE_ERROR("Dropped %d packets", cnt);
			nf->stats.tx_drop += cnt;
		}
		nf->stats.tx += cnt;
		
		/* time to allocate fresh mbufs for the queue */
		for (i = 0; i < dpc->wmbufs[nif].len; i++) {
			dpc->wmbufs[nif].m_table[i] = rte_pktmbuf_alloc(pktmbuf_pool);
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
onvm_get_wptr(struct mtcp_thread_context *ctxt, int nif, uint16_t pktsize)
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
		rte_pktmbuf_free(mtable[i]);
		RTE_MBUF_PREFETCH_TO_FREE(mtable[i+1]);
	}
}
/*----------------------------------------------------------------------------*/
int32_t
onvm_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
	struct dpdk_private_context *dpc;
	int ret;
	void *pkts[MAX_PKT_BURST];	
	int i;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;

	if (dpc->rmbufs[ifidx].len != 0) {
		free_pkts(dpc->rmbufs[ifidx].m_table, dpc->rmbufs[ifidx].len);
		dpc->rmbufs[ifidx].len = 0;
	}

	ret = rte_ring_dequeue_burst(rx_ring, pkts, MAX_PKT_BURST, NULL);
	
	for (i = 0; i < ret; i++) {
		dpc->pkts_burst[i] = (struct rte_mbuf*)pkts[i];
	}
	
#ifdef RX_IDLE_ENABLE	
	dpc->rx_idle = (likely(ret != 0)) ? 0 : dpc->rx_idle + 1;
#endif
	dpc->rmbufs[ifidx].len = ret;

	return ret;
}
/*----------------------------------------------------------------------------*/
uint8_t *
onvm_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
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

	/* verify checksum values from ol_flags */
	if ((m->ol_flags & (PKT_RX_L4_CKSUM_BAD | PKT_RX_IP_CKSUM_BAD)) != 0) {
		TRACE_ERROR("%s(%p, %d, %d): mbuf with invalid checksum: "
			    "%p(%lu);\n",
			    __func__, ctxt, ifidx, index, m, m->ol_flags);
		pktbuf = NULL;
	}
#ifdef ENABLELRO
	dpc->cur_rx_m = m;
#endif /* ENABLELRO */

	return pktbuf;
}
/*----------------------------------------------------------------------------*/
int32_t
onvm_select(struct mtcp_thread_context *ctxt)
{
#ifdef RX_IDLE_ENABLE
	struct dpdk_private_context *dpc;
	
	dpc = (struct dpdk_private_context *) ctxt->io_private_context;
	if (dpc->rx_idle > RX_IDLE_THRESH) {
		dpc->rx_idle = 0;
		usleep(RX_IDLE_TIMEOUT);
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
void
onvm_destroy_handle(struct mtcp_thread_context *ctxt)
{
	struct dpdk_private_context *dpc;
	int i;

	dpc = (struct dpdk_private_context *) ctxt->io_private_context;	

	/* free wmbufs */
	for (i = 0; i < num_devices_attached; i++)
		free_pkts(dpc->wmbufs[i].m_table, MAX_PKT_BURST);

#ifdef ENABLE_STATS_IOCTL
	/* free fd */
	if (dpc->fd >= 0)
		close(dpc->fd);
#endif /* !ENABLE_STATS_IOCTL */

	/* free it all up */
	free(dpc);
}
/*----------------------------------------------------------------------------*/
void
onvm_load_module(void)
{
	int i, portid;

	pktmbuf_pool=rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (pktmbuf_pool == NULL){
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool, errno: %d\n",
			 rte_errno);	
	}
	
	for (i = 0; i < num_devices_attached; ++i) {
		/* get portid form the index of attached devices */
		portid = devices_attached[i];
		/* check port capabilities */
		rte_eth_dev_info_get(portid, &dev_info[portid]);
	}
}
/*----------------------------------------------------------------------------*/
int32_t
onvm_dev_ioctl(struct mtcp_thread_context *ctx, int nif, int cmd, void *argp)
{
	struct dpdk_private_context *dpc;
	struct rte_mbuf *m;
	int len_of_mbuf;
	struct iphdr *iph;
	struct tcphdr *tcph;
	void **argpptr = (void **)argp;
#ifdef ENABLELRO
	uint8_t *payload, *to;
	int seg_off;
#endif

	if (cmd == DRV_NAME) {
		*argpptr = (void *)dev_info[nif].driver_name;
		return 0;
	}

	int eidx = CONFIG.nif_to_eidx[nif];

	iph = (struct iphdr *)argp;
	dpc = (struct dpdk_private_context *)ctx->io_private_context;
	len_of_mbuf = dpc->wmbufs[eidx].len;

	switch (cmd) {
	case PKT_TX_IP_CSUM:
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) == 0)
			goto dev_ioctl_err;
		m = dpc->wmbufs[eidx].m_table[len_of_mbuf - 1];
		m->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_IPV4;
		m->l2_len = sizeof(struct ether_hdr);
		m->l3_len = (iph->ihl<<2);
		break;
	case PKT_TX_TCP_CSUM:
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0)
			goto dev_ioctl_err;
		m = dpc->wmbufs[eidx].m_table[len_of_mbuf - 1];
		tcph = (struct tcphdr *)((unsigned char *)iph + (iph->ihl<<2));
		m->ol_flags |= PKT_TX_TCP_CKSUM;
		tcph->check = rte_ipv4_phdr_cksum((struct ipv4_hdr *)iph, m->ol_flags);
		break;
#ifdef ENABLELRO
	case PKT_RX_TCP_LROSEG:
		m = dpc->cur_rx_m;
		//if (m->next != NULL)
		//	rte_prefetch0(rte_pktmbuf_mtod(m->next, void *));
		iph = rte_pktmbuf_mtod_offset(m, struct iphdr *, sizeof(struct ether_hdr));
		tcph = (struct tcphdr *)((u_char *)iph + (iph->ihl << 2));
		payload = (uint8_t *)tcph + (tcph->doff << 2);

		seg_off = m->data_len -
			sizeof(struct ether_hdr) - (iph->ihl << 2) -
			(tcph->doff << 2);

		to = (uint8_t *) argp;
		m = m->next;
		memcpy(to, payload, seg_off);
		while (m != NULL) {
			//if (m->next != NULL)
			//	rte_prefetch0(rte_pktmbuf_mtod(m->next, void *));
			memcpy(to + seg_off,
			       rte_pktmbuf_mtod(m, uint8_t *),
			       m->data_len);
			seg_off += m->data_len;
			m = m->next;
		}
		break;
#endif
	case PKT_TX_TCPIP_CSUM:
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) == 0)
			goto dev_ioctl_err;
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0)
			goto dev_ioctl_err;
		m = dpc->wmbufs[eidx].m_table[len_of_mbuf - 1];
		iph = rte_pktmbuf_mtod_offset(m, struct iphdr *, sizeof(struct ether_hdr));
		tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ihl<<2));
		m->l2_len = sizeof(struct ether_hdr);
		m->l3_len = (iph->ihl<<2);
		m->l4_len = (tcph->doff<<2);
		m->ol_flags = PKT_TX_TCP_CKSUM | PKT_TX_IP_CKSUM | PKT_TX_IPV4;
		tcph->check = rte_ipv4_phdr_cksum((struct ipv4_hdr *)iph, m->ol_flags);
		break;
	case PKT_RX_IP_CSUM:
		if ((dev_info[nif].rx_offload_capa & DEV_RX_OFFLOAD_IPV4_CKSUM) == 0)
			goto dev_ioctl_err;
		break;
	case PKT_RX_TCP_CSUM:
		if ((dev_info[nif].rx_offload_capa & DEV_RX_OFFLOAD_TCP_CKSUM) == 0)
			goto dev_ioctl_err;
		break;
	case PKT_TX_TCPIP_CSUM_PEEK:
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) == 0)
			goto dev_ioctl_err;
		if ((dev_info[nif].tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0)
			goto dev_ioctl_err;
		break;
	default:
		goto dev_ioctl_err;
	}
	return 0;
 dev_ioctl_err:
	return -1;
}
/*----------------------------------------------------------------------------*/
io_module_func onvm_module_func = {
	.load_module		   = onvm_load_module,
	.init_handle		   = onvm_init_handle,
	.link_devices		   = onvm_link_devices,
	.release_pkt		   = onvm_release_pkt,
	.send_pkts		   = onvm_send_pkts,
	.get_wptr		   = onvm_get_wptr,
	.recv_pkts		   = onvm_recv_pkts,
	.get_rptr		   = onvm_get_rptr,
	.select			   = onvm_select,
	.destroy_handle		   = onvm_destroy_handle,
	.dev_ioctl		   = onvm_dev_ioctl
};
/*----------------------------------------------------------------------------*/
#else
io_module_func onvm_module_func = {
	.load_module		   = NULL,
	.init_handle		   = NULL,
	.link_devices		   = NULL,
	.release_pkt		   = NULL,
	.send_pkts		   = NULL,
	.get_wptr		   = NULL,
	.recv_pkts		   = NULL,
	.get_rptr		   = NULL,
	.select			   = NULL,
	.destroy_handle		   = NULL,
	.dev_ioctl		   = NULL
};
/*----------------------------------------------------------------------------*/
#endif /* ENABLE_ONVM */
