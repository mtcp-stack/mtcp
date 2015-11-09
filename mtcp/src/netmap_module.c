/* for io_module_func def'ns */
#include "io_module.h"
#ifndef DISABLE_NETMAP
/* for mtcp related def'ns */
#include "mtcp.h"
/* for errno */
#include <errno.h>
/* for logging */
#include "debug.h"
/* for num_devices_* */
#include "config.h"
/* for netmap definitions */
#define NETMAP_WITH_LIBS
#include "netmap_user.h"
/* for poll */
#include <sys/poll.h>
/*----------------------------------------------------------------------------*/
#define MAX_PKT_BURST			64
#define ETHERNET_FRAME_SIZE		1514
#define MAX_IFNAMELEN			(IF_NAMESIZE + 10)

struct netmap_private_context {
	struct nm_desc *local_nmd[MAX_DEVICES];
	int fd[MAX_DEVICES];
	char *recv_pktbuf[MAX_DEVICES][MAX_PKT_BURST];
	unsigned int recv_pkt_cnt[MAX_DEVICES];
	unsigned int recv_pkt_len[MAX_DEVICES][MAX_PKT_BURST];
	char send_pktbuf[MAX_DEVICES][MAX_PKT_BURST][ETHERNET_FRAME_SIZE];
	unsigned int send_pkt_cnt[MAX_DEVICES];
} __attribute__((aligned(__WORDSIZE)));

struct netmap_global {
	struct nm_desc *nmd;
	int main_fd;
};
struct netmap_global ng[MAX_DEVICES];
/*----------------------------------------------------------------------------*/
void
netmap_init_handle(struct mtcp_thread_context *ctxt)
{
	struct netmap_private_context *npc;
	char nifname[MAX_IFNAMELEN];
	int j;

	/* create and initialize private I/O module context */
	ctxt->io_private_context = calloc(1, sizeof(struct netmap_private_context));
	if (ctxt->io_private_context == NULL) {
		TRACE_ERROR("Failed to initialize ctxt->io_private_context: "
			    "Can't allocate memory\n");
		exit(EXIT_FAILURE);
	}
	
	npc = (struct netmap_private_context *)ctxt->io_private_context;

	/* set wmbufs correctly */
	for (j = 0; j < num_devices_attached; j++) {
		if (if_indextoname(devices_attached[j], nifname) == NULL) {
			TRACE_ERROR("Failed to initialize interface %s\n",
				    nifname);
			exit(EXIT_FAILURE);
		}
		sprintf(nifname, "netmap:%s", nifname);
		struct nm_desc nmd = *ng[j].nmd;
		uint64_t nmd_flags = 0;
		nmd.self = &nmd;
		
		if (ctxt->cpu > 0) {
			/*
			 * the first thread uses the fd opened by the main thread,
			 * the other threads re-open /dev/netmap
			 */
			nmd.req.nr_flags = ng[j].nmd->req.nr_flags & ~NR_REG_MASK;
			nmd.req.nr_flags |= NR_REG_ONE_NIC;
			nmd.req.nr_ringid = ctxt->cpu;

			npc->local_nmd[j] = nm_open(nifname, NULL, nmd_flags |
						    NM_OPEN_IFNAME | NM_OPEN_NO_MMAP,
						    &nmd);
			if (npc->local_nmd[j] == NULL) {
				TRACE_ERROR("Unable to open %s: %s\n",
					    nifname, strerror(errno));
				exit(EXIT_FAILURE);
			}
		} else {
			npc->local_nmd[j] = ng[j].nmd;
		}
		npc->fd[j] = ng[j].nmd->fd;
	}
}
/*----------------------------------------------------------------------------*/
int
netmap_link_devices(struct mtcp_thread_context *ctxt)
{
	/* linking takes place during mtcp_init() */
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void
netmap_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
	/* 
	 * do nothing over here - memory reclamation
	 * will take place in dpdk_recv_pkts 
	 */
}
/*----------------------------------------------------------------------------*/
int
netmap_send_pkts(struct mtcp_thread_context *ctxt, int nif)
{
	int i;
	struct netmap_private_context *npc;
	npc = (struct netmap_private_context *)ctxt->io_private_context;

	for (i = 0; i < npc->send_pkt_cnt[nif]; i++) {
		//local_nmd
	}

	npc->send_pkt_cnt[nif] = 0;
	return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t *
netmap_get_wptr(struct mtcp_thread_context *ctxt, int nif, uint16_t pktsize)
{
	struct netmap_private_context *npc;
	npc = (struct netmap_private_context *)ctxt->io_private_context;
	
	
	return (uint8_t *)npc->send_pktbuf[nif][npc->send_pkt_cnt[nif]++];
}
/*----------------------------------------------------------------------------*/
int32_t
netmap_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
	struct netmap_private_context *npc;
	struct netmap_if *nifp;
	int i, count = 0;

	npc = (struct netmap_private_context *)ctxt->io_private_context;
	for (i = npc->local_nmd[ifidx]->first_rx_ring; i <= npc->local_nmd[ifidx]->last_rx_ring; i++) {
		int m, cur, count;
		struct netmap_ring *rxring;

		nifp = npc->local_nmd[ifidx]->nifp;
		rxring = NETMAP_RXRING(nifp, i);
		if (nm_ring_empty(rxring))
			continue;
		cur = rxring->cur;
		count = nm_ring_space(rxring);
		if (count > MAX_PKT_BURST)
			count = MAX_PKT_BURST;
		/* receive packets */
		for (m = 0; m < count; m++) {
			struct netmap_slot *slot = &rxring->slot[cur];
			npc->recv_pktbuf[ifidx][npc->recv_pkt_cnt[ifidx]] = NETMAP_BUF(rxring, slot->buf_idx);
			npc->recv_pkt_len[ifidx][npc->recv_pkt_cnt[ifidx]] = slot->len;
			cur = nm_ring_next(rxring, cur);
			npc->recv_pkt_cnt[ifidx]++;
		}
		rxring->head = rxring->cur = cur; 
	}
	return count;
}
/*----------------------------------------------------------------------------*/
uint8_t *
netmap_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
{
	struct netmap_private_context *npc;
	npc = (struct netmap_private_context *)ctxt->io_private_context;

	*len = npc->recv_pkt_len[ifidx][index];
	return (unsigned char *)npc->recv_pktbuf[ifidx][index];
}
/*----------------------------------------------------------------------------*/
int32_t
netmap_select(struct mtcp_thread_context *ctxt)
{
	struct netmap_private_context *npc;

	npc = (struct netmap_private_context *)ctxt->io_private_context;
	struct pollfd pfd = { .fd = npc->fd[0], .events = POLLIN };

	if (poll(&pfd, 1, 1 * 1000) <= 0) {
		TRACE_ERROR("Poll failed! (cpu: %d, err: %d)\n", 
			    ctxt->cpu, errno);
		exit(EXIT_FAILURE);
	}
	
	if (pfd.revents & POLLERR) {
		TRACE_ERROR("Poll failed! (cpu: %d\n, err: %d)\n",
			    ctxt->cpu, errno);
		exit(EXIT_FAILURE);
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
void
netmap_destroy_handle(struct mtcp_thread_context *ctxt)
{
}
/*----------------------------------------------------------------------------*/
void
netmap_load_module(void)
{
	char nifname[MAX_IFNAMELEN];
	struct nmreq base_nmd;
	int j;
	
	memset(&base_nmd, 0, sizeof(base_nmd));

	for (j = 0; j < num_devices_attached; j++) {
		if (if_indextoname(devices_attached[j], nifname) == NULL) {
			TRACE_ERROR("Failed to initialize interface %s\n",
				    nifname);
			exit(EXIT_FAILURE);
		}
		sprintf(nifname, "netmap:%s", nifname);
		ng[j].nmd = nm_open(nifname, &base_nmd, 0, NULL);
		if (ng[j].nmd == NULL) {
			TRACE_ERROR("Failed to call nm_open for iface %s on g_nmd\n",
				    nifname);
			exit(EXIT_FAILURE);
		}
		struct nm_desc saved_desc = *(ng[j].nmd);
		saved_desc.self = &saved_desc;
		saved_desc.mem = NULL;
		nm_close(ng[j].nmd);
		saved_desc.req.nr_flags &= ~NR_REG_MASK;
		saved_desc.req.nr_flags |= NR_REG_ONE_NIC;
		saved_desc.req.nr_ringid = 0;
		ng[j].nmd = nm_open(nifname, &base_nmd, NM_OPEN_IFNAME, &saved_desc);
		if (ng[j].nmd == NULL) {
			TRACE_ERROR("Unable to open %s: %s", nifname, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		ng[j].main_fd = ng[j].nmd->fd;
		TRACE_LOG("mapped %dKB at %p", g_nmd[j].req.nr_memsize>>10, 
			  ng[j].nmd->mem);
	}	
}
/*----------------------------------------------------------------------------*/
io_module_func netmap_module_func = {
	.load_module		   = netmap_load_module,
	.init_handle		   = netmap_init_handle,
	.link_devices		   = netmap_link_devices,
	.release_pkt		   = netmap_release_pkt,
	.send_pkts		   = netmap_send_pkts,
	.get_wptr   		   = netmap_get_wptr,
	.recv_pkts		   = netmap_recv_pkts,
	.get_rptr	   	   = netmap_get_rptr,
	.select			   = netmap_select,
	.destroy_handle		   = netmap_destroy_handle
};
/*----------------------------------------------------------------------------*/
#else
io_module_func netmap_module_func = {
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
#endif /* !DISABLE_NETMAP */
