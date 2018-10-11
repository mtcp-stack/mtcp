/* for io_module_func def'ns */
#include "io_module.h"
#ifndef DISABLE_PSIO
/* for mtcp related def'ns */
#include "mtcp.h"
/* for psio related def'ns */
#include "ps.h"
/* for errno */
#include <errno.h>
/* for logging */
#include "debug.h"
/* for num_devices_* */
#include "config.h"
/* for ETHER_CRC_LEN */
#include <net/ethernet.h>
/*----------------------------------------------------------------------------*/
#define PS_CHUNK_SIZE 			64
#define PS_SELECT_TIMEOUT 		100		/* in us */

/*
 * Ethernet frame overhead
 */

#define ETHER_IFG			12
#define	ETHER_PREAMBLE			8
#define ETHER_OVR			(ETHER_CRC_LEN + ETHER_PREAMBLE + ETHER_IFG)
/*----------------------------------------------------------------------------*/
struct ps_device devices[MAX_DEVICES];
/*----------------------------------------------------------------------------*/
struct psio_private_context {
	struct ps_handle handle;
	struct ps_chunk_buf w_chunk_buf[ETH_NUM];
	struct ps_chunk chunk;
	struct ps_event event;

	nids_set rx_avail;
	nids_set tx_avail;
	struct timeval last_tx_set[ETH_NUM];
} __attribute__((aligned(__WORDSIZE)));
/*----------------------------------------------------------------------------*/
void
psio_init_handle(struct mtcp_thread_context *ctxt)
{
	int i, ret;
	struct psio_private_context *ppc;
	struct timeval cur_ts;

	/* create and initialize private I/O module context */
	ctxt->io_private_context = calloc(1, sizeof(struct psio_private_context));
	if (ctxt->io_private_context == NULL) {
		TRACE_ERROR("Failed to initialize ctxt->io_private_context: "
			    "Can't allocate memory\n");
		exit(EXIT_FAILURE);
	}
	
	ppc = (struct psio_private_context *)ctxt->io_private_context;
	if (ps_init_handle(&ppc->handle)) {
		perror("ps_init_handle");
		TRACE_ERROR("Failed to initialize ps handle.\n");
		exit(EXIT_FAILURE);
	}

	/* create buffer for reading ingress batch of packet */
	if (ps_alloc_chunk(&ppc->handle, &ppc->chunk) != 0) {
		perror("ps_alloc_chunk");
		TRACE_ERROR("Failed to allocate ps_chunk\n");
		exit(EXIT_FAILURE);
	}

	/* create packet write chunk */
	for (i = 0; i < num_devices_attached; i++) {
		ret = ps_alloc_chunk_buf(&ppc->handle, i, ctxt->cpu, &ppc->w_chunk_buf[i]);
		if (ret != 0) {
			TRACE_ERROR("Failed to allocate ps_chunk_buf.\n");
			exit(EXIT_FAILURE);
		}
	}
	
	gettimeofday(&cur_ts, NULL);
	
	/* initialize PSIO parameters */
	ppc->chunk.recv_blocking = 0;
	ppc->event.timeout = PS_SELECT_TIMEOUT;
	ppc->event.qidx = ctxt->cpu;
	NID_ZERO(ppc->event.rx_nids);
	NID_ZERO(ppc->event.tx_nids);
	NID_ZERO(ppc->rx_avail);
	//NID_ZERO(ppc->tx_avail);

	for (i = 0; i < CONFIG.eths_num; i++) {
		ppc->last_tx_set[i] = cur_ts;
		NID_SET(i, ppc->tx_avail);
	}
}
/*----------------------------------------------------------------------------*/
int
psio_link_devices(struct mtcp_thread_context *ctxt)
{
	struct psio_private_context *ppc;
	int ret;
	int i, working;
	
	ppc = (struct psio_private_context *)ctxt->io_private_context;
	working = -1;

	/* attaching (device, queue) */
	for (i = 0 ; i < num_devices_attached ; i++) {
		struct ps_queue queue;
		queue.ifindex = devices_attached[i];
		
		if (devices[devices_attached[i]].num_rx_queues <= ctxt->cpu) {
			continue;
		}
		
		working = 0;
		queue.ifindex = devices_attached[i];
		queue.qidx = ctxt->cpu;
		
#if 0
		TRACE_DBG("attaching RX queue xge%d:%d to CPU%d\n", 
			  queue.ifindex, queue.qidx, mtcp->ctxt->cpu);
#endif
		ret = ps_attach_rx_device(&ppc->handle, &queue);
		if (ret != 0) {
			perror("ps_attach_rx_device");
			exit(1);
		}

	}
	return working;
}
/*----------------------------------------------------------------------------*/
void
psio_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
	struct psio_private_context *ppc;
	struct ps_packet packet;

	ppc = (struct psio_private_context *)ctxt->io_private_context;
	/* pass the packet to the kernel */
	packet.ifindex = ifidx;
	packet.len = len;
	packet.buf = (char *)pkt_data;
	ps_slowpath_packet(&ppc->handle, &packet);
}
/*----------------------------------------------------------------------------*/
static int
psio_flush_pkts(struct mtcp_thread_context *ctx, int nif)
{
	struct ps_chunk_buf *c_buf;
	mtcp_manager_t mtcp;
	struct psio_private_context *ppc;
	int send_cnt, to_send_cnt = 0;
	int start_idx, i;

	ppc = (struct psio_private_context *)ctx->io_private_context;
	c_buf = &ppc->w_chunk_buf[nif];
	mtcp = ctx->mtcp_manager;

	/* if chunk (for writing) is not there... then return */
	if (!c_buf)
		return -1;
	
	to_send_cnt = c_buf->cnt;
	if (to_send_cnt > 0) {
		STAT_COUNT(mtcp->runstat.rounds_tx_try);
		start_idx = c_buf->next_to_send;
		send_cnt = ps_send_chunk_buf(&ppc->handle, c_buf);
		
		for (i = 0; i < send_cnt; i++) {
#ifdef NETSTAT
			mtcp->nstat.tx_bytes[nif] += c_buf->info[start_idx].len + ETHER_OVR;
#endif
#if PKTDUMP
			DumpPacket(mtcp, c_buf->buf + c_buf->info[start_idx].offset, 
				   c_buf->info[start_idx].len, "OUT", nif);
			
#endif
			start_idx = (start_idx + 1) % ENTRY_CNT;
		}
		if (send_cnt < 0) {
			TRACE_ERROR("ps_send_chunk_buf failed. "
				    "ret: %d, error: %s\n", send_cnt, strerror(errno));
#ifdef NETSTAT
		} else {
			mtcp->nstat.tx_packets[nif] += send_cnt;
#endif
		}
		
		return send_cnt;
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
int
psio_send_pkts(struct mtcp_thread_context *ctxt, int nif)
{
	struct psio_private_context *ppc;
	mtcp_manager_t mtcp;
	int ret, prev_cnt;

	ppc = (struct psio_private_context *)ctxt->io_private_context;
	mtcp = ctxt->mtcp_manager;

#if 0
	/* if tx if not available, pass */
	if (!NID_ISSET(nif, ppc->tx_avail)) {
		NID_SET(nif, ppc->event.tx_nids);
		return -1;
	}
#endif
	while ((prev_cnt = ppc->w_chunk_buf[nif].cnt) > 0) {
		ret = psio_flush_pkts(ctxt, nif);
		if (ret <= 0) {
			if (ret < 0) 
				TRACE_ERROR("ps_send_chunk_buf failed to send.\n");
			NID_SET(nif, ppc->event.tx_nids);
			NID_CLR(nif, ppc->tx_avail);
			break;
		} else if (ret < prev_cnt) {
			NID_CLR(nif, ppc->tx_avail);
			NID_SET(nif, ppc->event.tx_nids);
			STAT_COUNT(mtcp->runstat.rounds_tx);
			break;
		} else {
			STAT_COUNT(mtcp->runstat.rounds_tx);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t *
psio_get_wptr(struct mtcp_thread_context *ctxt, int nif, uint16_t len)
{
	struct psio_private_context *ppc;
	struct ps_chunk_buf *c_buf;

	ppc = (struct psio_private_context *) ctxt->io_private_context;
	c_buf = &ppc->w_chunk_buf[nif];

	/* retrieve the right write offset */
	return (uint8_t *)ps_assign_chunk_buf(c_buf, len);
}
/*----------------------------------------------------------------------------*/
int32_t
psio_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
	int ret, no_rx_packet;
	struct psio_private_context *ppc;
		
	ppc = (struct psio_private_context *) ctxt->io_private_context;
	no_rx_packet = 0;
	
	ppc->chunk.cnt = PS_CHUNK_SIZE;
	
	ret = ps_recv_chunk_ifidx(&ppc->handle, &ppc->chunk, ifidx);
	if (ret < 0) {
		if (errno != EAGAIN) {
			TRACE_ERROR("ps_recv_chunk_ifidx failed to read packets.\n");
			perror("ps_recv_chunk_ifidx()");
		}
		NID_SET(ifidx, ppc->event.rx_nids);
		no_rx_packet = 1;
	} else if (ret == 0) {
		NID_SET(ifidx, ppc->event.rx_nids);
		no_rx_packet = 1;
	}
	
	if (!no_rx_packet)
		NID_SET(ifidx, ppc->rx_avail);

	return ret;
}
/*----------------------------------------------------------------------------*/
uint8_t *
psio_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
{
	struct psio_private_context *ppc;
	uint8_t *pktbuf;

	ppc = (struct psio_private_context *) ctxt->io_private_context;	
	pktbuf = (uint8_t *)(ppc->chunk.buf + ppc->chunk.info[index].offset);
	*len = ppc->chunk.info[index].len;

	(void)(ifidx);
	return pktbuf;
}
/*----------------------------------------------------------------------------*/
int32_t
psio_select(struct mtcp_thread_context *ctxt)
{
	struct psio_private_context *ppc;
	mtcp_manager_t mtcp;
	struct timeval cur_ts;
	int i, ret;
	
	ppc = (struct psio_private_context *) ctxt->io_private_context;	
	mtcp = ctxt->mtcp_manager;
	gettimeofday(&cur_ts, NULL);
	

	if (!ppc->rx_avail || ppc->event.tx_nids) {
		for (i = 0; i < CONFIG.eths_num; i++) {
			if (ppc->w_chunk_buf[i].cnt > 0)
				NID_SET(i, ppc->event.tx_nids);
			if (mtcp->n_sender[i]->control_list_cnt > 0 || 
			    mtcp->n_sender[i]->send_list_cnt > 0 || 
			    mtcp->n_sender[i]->ack_list_cnt > 0) { 
				if (cur_ts.tv_sec > ppc->last_tx_set[i].tv_sec || 
				    cur_ts.tv_usec > ppc->last_tx_set[i].tv_usec) {
					NID_SET(i, ppc->event.tx_nids);
					ppc->last_tx_set[i] = cur_ts;
				}
			}    
		}
		
		TRACE_SELECT("BEFORE: rx_avail: %d, tx_avail: %d, event.rx_nids: %0x, event.tx_nids: %0x\n", 
			     ppc->rx_avail, ppc->tx_avail, ppc->event.rx_nids, ppc->event.tx_nids);
		mtcp->is_sleeping = TRUE;
		ret = ps_select(&ppc->handle, &ppc->event);
		mtcp->is_sleeping = FALSE;
#if TIME_STAT
		gettimeofday(&select_ts, NULL);
		UpdateStatCounter(&mtcp->rtstat.select, 
				  TimeDiffUs(&select_ts, &xmit_ts));
#endif
		if (ret < 0) {
			if (errno != EAGAIN && errno != EINTR) {
				perror("ps_select");
				exit(EXIT_FAILURE);
			}
			if (errno == EINTR) {
				STAT_COUNT(mtcp->runstat.rounds_select_intr);
			}
		} else {
			TRACE_SELECT("ps_select(): event.rx_nids: %0x, event.tx_nids: %0x\n", 
				     ppc->event.rx_nids, ppc->event.tx_nids);
			if (ppc->event.rx_nids != 0) {
				STAT_COUNT(mtcp->runstat.rounds_select_rx);
			}
			if (ppc->event.tx_nids != 0) {
				for (i = 0; i < CONFIG.eths_num; i++) {
					if (NID_ISSET(i, ppc->event.tx_nids)) {
						NID_SET(i, ppc->tx_avail);
					}
				}
				STAT_COUNT(mtcp->runstat.rounds_select_tx);
			}
		}
		TRACE_SELECT("AFTER: rx_avail: %d, tx_avail: %d, event.rx_nids: %d, event.tx_nids: %d\n", 
			     ppc->rx_avail, ppc->tx_avail, ppc->event.rx_nids, ppc->event.tx_nids);
		STAT_COUNT(mtcp->runstat.rounds_select);
	}

	/* reset psio parameters */
	ppc->event.timeout = PS_SELECT_TIMEOUT;
	NID_ZERO(ppc->event.rx_nids);
	NID_ZERO(ppc->event.tx_nids);
	NID_ZERO(ppc->rx_avail);
	//NID_ZERO(ppc->tx_avail);

	return 0;
}
/*----------------------------------------------------------------------------*/
void
psio_destroy_handle(struct mtcp_thread_context *ctxt)
{
	struct psio_private_context *ppc;

	ppc = (struct psio_private_context *) ctxt->io_private_context;	

	/* free it all up */
	free(ppc);
}
/*----------------------------------------------------------------------------*/
void
psio_load_module(void)
{
	/* PSIO does not support FDIR/multi-process support */
	if (CONFIG.multi_process) {
		TRACE_LOG("PSIO module does not provide multi-process support\n");
		exit(EXIT_FAILURE);
	}
}
/*----------------------------------------------------------------------------*/
io_module_func ps_module_func = {
	.load_module		   = psio_load_module,
	.init_handle		   = psio_init_handle,
	.link_devices		   = psio_link_devices,
	.release_pkt		   = psio_release_pkt,
	.send_pkts		   = psio_send_pkts,
	.get_wptr   		   = psio_get_wptr,
	.recv_pkts		   = psio_recv_pkts,
	.get_rptr	   	   = psio_get_rptr,
	.select			   = psio_select,
	.destroy_handle		   = psio_destroy_handle,
	.dev_ioctl		   = NULL
};
#else
io_module_func ps_module_func = {
	.load_module		   = NULL,
	.init_handle		   = NULL,
	.link_devices		   = NULL,
	.release_pkt		   = NULL,
	.send_pkts		   = NULL,
	.get_wptr   		   = NULL,
	.recv_pkts		   = NULL,
	.get_rptr	   	   = NULL,
	.select			   = NULL,
	.destroy_handle		   = NULL,
	.dev_ioctl		   = NULL
};
/*----------------------------------------------------------------------------*/
#endif /* !DISABLE_PSIO */
