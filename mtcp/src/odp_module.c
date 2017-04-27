/* for io_module_func def'ns */
#include "io_module.h"
#ifndef DISABLE_ODP
/* for mtcp related def'ns */
#include "mtcp.h"
/* for errno */
#include <errno.h>
/* for logging */
#include "debug.h"
/* for num_devices_* */
#include "config.h"
/* for odp definitions */
/*----------------------------------------------------------------------------*/
/** @def MAX_WORKERS
 * @brief Maximum number of worker threads
 */
#define MAX_WORKERS            16

/** @def SHM_PKT_POOL_SIZE
 * @brief Size of the shared memory block
 */
#define SHM_PKT_POOL_SIZE      96000

/** @def SHM_PKT_POOL_BUF_SIZE
 * @brief Buffer size of the packet pool buffer
 */
#define SHM_PKT_POOL_BUF_SIZE  1856

/** @def MAX_PKT_BURST
 * @brief Maximum number of packet bursts
 */
#define MAX_PKT_BURST          32

#define MAX_QUEUES 16

#define MAX_IFNAME_LEN 1024
#define SPLIT_NAME_SEP ", "
#define PKTIO_NAME_PREFIX "pktio_"
/*----------------------------------------------------------------------------*/

#include <string.h>
#include <odp.h>
#include "debug.h"

struct odp_packet_table_t {
        odp_packet_t pkts[MAX_PKT_BURST];
        unsigned num;
}__attribute__((aligned(__WORDSIZE)));

struct odp_pktin_queue_args_t {
        odp_pktin_queue_t queues[MAX_QUEUES];
        unsigned num;
};
struct odp_pktout_queue_args_t {
        odp_pktout_queue_t queues[MAX_QUEUES];
        unsigned num;
};

struct odp_private_context {
        struct odp_packet_table_t snd_pkttbl[MAX_DEVICES];
        struct odp_packet_table_t rcv_pkttbl[MAX_DEVICES];
        struct odp_pktin_queue_args_t  pktin[MAX_DEVICES];
        struct odp_pktout_queue_args_t pktout[MAX_DEVICES];
} __attribute__((aligned(__WORDSIZE)));

struct queues_assigned_args_t {
        unsigned char start_index;
        unsigned char end_index;
};

struct pktio_args_t {
        odp_pktio_t pktio;
        struct queues_assigned_args_t in_queues_assigned[MAX_DEVICES];
        struct queues_assigned_args_t out_queues_assigned[MAX_DEVICES];
};

#define ODP_POOL_NAME "odp_packet_pool"

void odp_release_module();

void dump_packets(struct mtcp_thread_context *ctxt, int ifidx, odp_packet_t pkts[], int num, const char* inf) {
  mtcp_manager_t mtcp;
  char *buf;
  int i, len;

  
  TRACE_INFO("dump %d pakcets for %s\n", num, inf);
    
  mtcp = ctxt->mtcp_manager;
  for (i = 0 ; i < num ; i ++) {
    buf = (char*)odp_packet_data(pkts[i]);
    len = odp_packet_buf_len(pkts[i]);
    DumpPacket(mtcp, buf, len, inf, ifidx);
  }
}

static struct pktio_args_t pktios[MAX_DEVICES];
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* for debug */
#include <stdarg.h>

// #define _DEBUG_
int
odp_mtcp_log(odp_log_level_t level, const char *fmt, ...)
{
	va_list args;
	FILE *logfd;

	switch (level) {
	case ODP_LOG_ERR:
	case ODP_LOG_UNIMPLEMENTED:
	case ODP_LOG_ABORT:
		logfd = stderr;
		break;
	default:
		logfd = stdout;
	}

	va_start(args, fmt);

	fprintf(logfd, fmt,__FUNCTION__, __LINE__, args);
	
	va_end(args);

	return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
int
parse_pktio_name(char* pktio_name)
{
        int port_id, pre_len;
	
	port_id = -1;
	pre_len = strlen(PKTIO_NAME_PREFIX);
	if (strncmp(pktio_name, PKTIO_NAME_PREFIX, pre_len) != 0) {
	  return port_id;
	}

	port_id = atoi(pktio_name + pre_len);
	if (port_id < 0 || port_id >= MAX_DEVICES) {
	        port_id = -1;
	}

	return port_id;
}

int
odp_init_interfaces(char* dev_name_list, int* port_id_list)
{
        int num_dev, port_id, i, j;
	char* pktio_name;
	odp_init_t log_params;
	odp_pool_param_t params;
	odp_pktio_param_t pktio_param;
	odp_pool_t pool;
	odp_pktio_t pktio;
	unsigned char haddr[ETH_ALEN * 2];
	
	log_params.log_fn = odp_mtcp_log;
	log_params.abort_fn = odp_mtcp_log;

  	/* Init ODP before calling anything else */
	if (odp_init_global(&log_params, NULL)) {
		TRACE_ERROR("Error: ODP global init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Init this thread, namely the control thread */
	if (odp_init_local(ODP_THREAD_CONTROL)) {
		TRACE_ERROR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Create packet pool */
	odp_pool_param_init(&params);
	params.pkt.seg_len = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.len     = SHM_PKT_POOL_BUF_SIZE;
	params.pkt.num     = SHM_PKT_POOL_SIZE;
	params.type        = ODP_POOL_PACKET;

	// @TODO create one pool for each interface
	pool = odp_pool_create(ODP_POOL_NAME, &params);
	if (pool == ODP_POOL_INVALID) {
		TRACE_ERROR("Error: packet pool create failed.\n");
		exit(EXIT_FAILURE);
	}
	odp_pool_print(pool);
	
	for (i = 0; i < MAX_DEVICES; ++i) {
		port_id_list[i] = -1;
	}

	odp_pktio_param_init(&pktio_param);
	pktio_param.in_mode = ODP_PKTIN_MODE_DIRECT;
	pktio_param.out_mode = ODP_PKTOUT_MODE_DIRECT;

	num_dev = 0;
	pktio_name = strtok(dev_name_list, SPLIT_NAME_SEP);
	while (pktio_name != NULL) {
	        port_id = parse_pktio_name(pktio_name);
		if (port_id != -1 &&
		    (pktio = odp_pktio_open(pktio_name, pool, &pktio_param)) != ODP_PKTIO_INVALID) {
		        if (ETH_ALEN != odp_pktio_mac_addr(pktio, haddr, ETH_ALEN)) {
			        TRACE_ERROR("Error: ODP read mac error for %d\n", num_dev);
				exit(EXIT_FAILURE);			        
		        }
			port_id_list[num_dev] = port_id;
			for (j = 0; j < ETH_ALEN; ++j) {
			        CONFIG.eths[num_dev].haddr[j] = haddr[j];
			}
			pktios[num_dev ++].pktio = pktio;
		}
	        pktio_name = strtok(NULL, SPLIT_NAME_SEP);
	}
	
	return num_dev;
}

void
odp_load_module(void)
{
        TRACE_INFO("[MTCP-ODP]: load module.\n");

	odp_pktio_t pktio;
	odp_pktio_capability_t capa;
	odp_pktin_queue_param_t in_queue_param;
	odp_pktout_queue_param_t out_queue_param;
	odp_pktio_op_mode_t mode_rx = ODP_PKTIO_OP_MT;
	odp_pktio_op_mode_t mode_tx = ODP_PKTIO_OP_MT;
	int eidx, tidx, curr_idx, curr_num;
	unsigned num_rx, num_tx, num_cores;
	
	odp_pktin_queue_param_init(&in_queue_param);
	odp_pktout_queue_param_init(&out_queue_param);

        in_queue_param.op_mode = mode_rx;
        in_queue_param.num_queues  = 1; /* for hns, it must be 1 */
	out_queue_param.op_mode = mode_tx;
        out_queue_param.num_queues  = 1; /* for hns, it must be 1 */

	/* this is hard-coded for hns*/
	num_rx = num_tx = 16;
	num_cores = CONFIG.num_cores;
	
	for (eidx = 0; eidx < CONFIG.eths_num; ++eidx) {
	        pktio = pktios[eidx].pktio;

		/* set queue numbers */
	        odp_pktio_capability(pktio, &capa);
		TRACE_INFO("in_queues: %d\t out_queues:%d\n",
			   capa.max_input_queues, capa.max_output_queues);

		if (odp_pktin_queue_config(pktio, &in_queue_param)) {
		        TRACE_ERROR("Error: input queue config failed\n");
		        exit(EXIT_FAILURE);
		}

		if (odp_pktout_queue_config(pktio, &out_queue_param)) {
		        TRACE_ERROR("Error: output queue config failed\n");
		        exit(EXIT_FAILURE);
		}
		
	        odp_pktio_start(pktios[eidx].pktio);

		for (curr_idx = tidx = 0; tidx < num_cores; ++tidx) {
		        curr_num = (num_rx / num_cores) + (tidx < (num_rx % num_cores));
		        pktios[eidx].in_queues_assigned[tidx].start_index = curr_idx;
			pktios[eidx].in_queues_assigned[tidx].end_index = curr_idx + curr_num - 1;
			curr_idx += curr_num; 
		}

		for (curr_idx = tidx = 0; tidx < num_cores; ++tidx) {
		        curr_num = (num_tx / num_cores) + (tidx < (num_tx % num_cores));
		        pktios[eidx].out_queues_assigned[tidx].start_index = curr_idx;
			pktios[eidx].out_queues_assigned[tidx].end_index = curr_idx + curr_num - 1;
			curr_idx += curr_num; 
		}

		for (tidx = 0; tidx < num_cores; ++tidx) {
		        TRACE_INFO("%u-%u: in_queues: %u ~ %u\tout_queues: %u ~ %u\n",
				   eidx, tidx,
				   pktios[eidx].in_queues_assigned[tidx].start_index,
				   pktios[eidx].in_queues_assigned[tidx].end_index,
				   pktios[eidx].out_queues_assigned[tidx].start_index,
				   pktios[eidx].out_queues_assigned[tidx].end_index);
		}
	}
}
/*----------------------------------------------------------------------------*/
void
odp_init_handle(struct mtcp_thread_context *ctxt)
{
        TRACE_INFO("[MTCP-ODP]: init handle.\n");

	struct odp_private_context *opc;
	int i;
	/* create and initialize private I/O module context */
	ctxt->io_private_context = calloc(1, sizeof(struct odp_private_context));
	opc = (struct odp_private_context *)ctxt->io_private_context;
	
 	/* Init this thread */
	if (odp_init_local(ODP_THREAD_WORKER)) {
		TRACE_ERROR("Error: ODP local init failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Init packets tables */
	for (i = 0; i < MAX_DEVICES; ++i) {
	        opc->snd_pkttbl[i].num = 0;
		opc->rcv_pkttbl[i].num = 0;
	}
}
/*----------------------------------------------------------------------------*/
int
odp_link_devices(struct mtcp_thread_context *ctxt)
{
	/* linking takes place during mtcp_init() */
	TRACE_INFO("[MTCP-ODP]: link devices.\n");

	struct odp_private_context *opc;
	odp_pktin_queue_t in_queue;
	odp_pktout_queue_t out_queue;
	int tidx, eidx, qidx, q_num;
	int in_start, out_start, in_end, out_end;
	
	opc = (struct odp_private_context *)ctxt->io_private_context;
	tidx = ctxt->cpu;

	for (eidx = 0; eidx < CONFIG.eths_num; ++eidx) {
	        odp_pktin_queue(pktios[eidx].pktio, &in_queue, 1);
		odp_pktout_queue(pktios[eidx].pktio, &out_queue, 1);
		
	        in_start  = pktios[eidx].in_queues_assigned[tidx].start_index;
		in_end    = pktios[eidx].in_queues_assigned[tidx].end_index;
		out_start = pktios[eidx].out_queues_assigned[tidx].start_index;
		out_end   = pktios[eidx].out_queues_assigned[tidx].end_index;
	        opc->pktin[eidx].num = in_end - in_start + 1;
		opc->pktout[eidx].num = out_end - out_start + 1;

		for (q_num = opc->pktin[eidx].num, qidx = 0; qidx < q_num; ++qidx) {
		        in_queue.index = in_start + qidx;
		        opc->pktin[eidx].queues[qidx] = in_queue;
		}
		for (q_num = opc->pktout[eidx].num, qidx = 0; qidx < q_num; ++qidx) {
		        out_queue.index = out_start + qidx;
		        opc->pktout[eidx].queues[qidx] = out_queue;
		}

		TRACE_INFO("%d - %d: in-%d {%u ~ %u} \t out-%d {%u ~ %u}\n", eidx, tidx,
			   opc->pktin[eidx].num,
			   opc->pktin[eidx].queues[0].index,
			   opc->pktin[eidx].queues[opc->pktin[eidx].num - 1].index,
			   opc->pktout[eidx].num,
			   opc->pktout[eidx].queues[0].index,
			   opc->pktout[eidx].queues[opc->pktout[eidx].num - 1].index);
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void
odp_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
	/* 
	 * do nothing over here - memory reclamation
	 * will take place in dpdk_recv_pkts 
	 */
         /* TRACE_INFO("[MTCP-ODP]: release_pkt.\n"); */
}
/*----------------------------------------------------------------------------*/
uint8_t *
odp_get_wptr(struct mtcp_thread_context *ctxt, int ifidx, uint16_t pktsize)
{
#ifdef  _DEBUG_
        TRACE_INFO("[MTCP-ODP]: get_wptr.\n");
#endif

	struct odp_private_context *opc;
	mtcp_manager_t mtcp;
	odp_pool_t pool;
	odp_packet_t pkt;

	mtcp = ctxt->mtcp_manager;
	opc = (struct odp_private_context *)ctxt->io_private_context;
	if (opc->snd_pkttbl[ifidx].num == MAX_PKT_BURST) {
#ifdef _DEBUG_
	        TRACE_INFO("[MTCP-ODP] Warning: exceed the burst size.\n");
#endif
		odp_send_pkts(ctxt, ifidx);
	}

	pool = odp_pool_lookup(ODP_POOL_NAME);
	if (pool == ODP_POOL_INVALID) {
	        TRACE_ERROR("[MTCP-ODP] Error: can not find the pool.");
	        return NULL;	  
	}
	
	pkt = odp_packet_alloc(pool, pktsize);
	if (pkt == ODP_PACKET_INVALID) {
	        TRACE_ERROR("[MTCP-ODP] Error: can not allocate packet.");
	        return NULL;	
	}
	
	opc->snd_pkttbl[ifidx].pkts[opc->snd_pkttbl[ifidx].num] = pkt;
	opc->snd_pkttbl[ifidx].num = opc->snd_pkttbl[ifidx].num + 1;

#ifdef NETSTAT
	//mtcp->nstat.tx_bytes[nif] += pktsize + 24;
#endif

	return (uint8_t *)odp_packet_data(pkt);
}
/*----------------------------------------------------------------------------*/
int
odp_send_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
#ifdef  _DEBUG_
        TRACE_INFO("[MTCP-ODP]: send_pkts.\n");
#endif

        /* nif is the real index of CONFIG.eths */
	
	struct odp_private_context *opc;
	mtcp_manager_t mtcp;
	odp_pktout_queue_t pktout;
	odp_packet_t *pkts;
	int sent, expected, total, qidx, q_num;

	mtcp = ctxt->mtcp_manager;
	opc = (struct odp_private_context *)ctxt->io_private_context;
	q_num = opc->pktout[ifidx].num;

	pkts = opc->snd_pkttbl[ifidx].pkts;
	expected = opc->snd_pkttbl[ifidx].num;
	total = 0;
	for (qidx = 0; qidx < q_num && expected; ++qidx) {
	        pktout = opc->pktout[ifidx].queues[qidx];
		sent = odp_pktio_send_queue(pktout, pkts, expected);
		expected -= sent;
		total += sent;
		if (qidx == q_num - 1 && expected > 0) {
		        qidx = -1;
		}
	}

	opc->snd_pkttbl[ifidx].num = 0;

#ifdef NETSTAT
	//mtcp->nstat.tx_packets[nif] += sent;
#endif
	return total;
}
/*----------------------------------------------------------------------------*/
int32_t
odp_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
#ifdef  _DEBUG_
        TRACE_INFO("[MTCP-ODP]: recv_pkts.\n");
#endif

        /* ifidx is the real index of CONFIG.eths */
  
	struct odp_private_context *opc;
	odp_pktin_queue_t pktin;
	int total, got, expected, qidx, q_num;

	opc = (struct odp_private_context *)ctxt->io_private_context;
	q_num = opc->pktin[ifidx].num;
	
 	if (opc->rcv_pkttbl[ifidx].num != 0) {
	        /* process remaining packets */
	        odp_packet_free_multi(opc->rcv_pkttbl[ifidx].pkts, opc->rcv_pkttbl[ifidx].num);
		opc->rcv_pkttbl[ifidx].num = 0;
	}

	total = 0;
	expected = MAX_PKT_BURST;
	for (qidx = 0; qidx < q_num; ++qidx) {
	        pktin = opc->pktin[ifidx].queues[qidx];
	        got = odp_pktio_recv_queue(pktin, opc->rcv_pkttbl[ifidx].pkts + total, expected);
		expected -= got;
		total += got;
		if (expected == 0) {
		        break;
		}
	}

	opc->rcv_pkttbl[ifidx].num = total;

#ifdef  _DEBUG_
	if (total > 0) {
	  dump_packets(ctxt, ifidx, opc->rcv_pkttbl[ifidx].pkts, total, "[LYB-IN]");
	}
#endif

	return total;
}
/*----------------------------------------------------------------------------*/
uint8_t *
odp_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
{
#ifdef  _DEBUG_
        TRACE_INFO("[MTCP-ODP]: get rptr.\n");
#endif

	/* ifidx is the real index of CONFIG.eths */

	struct odp_private_context *opc;
	odp_packet_t pkt;
	
	opc = (struct odp_private_context *)ctxt->io_private_context;

	pkt = opc->rcv_pkttbl[ifidx].pkts[index];
	*len = (uint16_t)odp_packet_buf_len(pkt);
	return (uint8_t *)odp_packet_data(pkt);
}
/*----------------------------------------------------------------------------*/
int32_t
odp_select(struct mtcp_thread_context *ctxt)
{
        struct odp_private_context *opc = (struct odp_private_context *)ctxt->io_private_context;
	int eidx;

	for (eidx = 0; eidx < CONFIG.eths_num; ++eidx) {
	        if (opc->rcv_pkttbl[eidx].num != 0) {
		          odp_packet_free_multi(opc->rcv_pkttbl[eidx].pkts, opc->rcv_pkttbl[eidx].num);
		          opc->rcv_pkttbl[eidx].num = 0;
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
void
odp_destroy_handle(struct mtcp_thread_context *ctxt)
{
        TRACE_INFO("[MTCP-ODP]: destroy handle.\n");
	
	int i, ret;
	if (ctxt != NULL) {
	  	struct odp_private_context *opc;
		opc = (struct odp_private_context *)ctxt->io_private_context;

		for (i = 0 ; i < num_devices_attached; i ++) {
		  odp_packet_free_multi(opc->snd_pkttbl[i].pkts, opc->snd_pkttbl[i].num);
		}
	
		free(opc);
	}
	else {
	        odp_release_module();
	}
}
/*----------------------------------------------------------------------------*/


void
odp_release_module()
{
        odp_pktio_t pktio;
	odp_pool_t pool;
        int portid;
	int ret;

	for (portid = 0; portid < num_devices_attached; portid ++) {
	        pktio = pktios[portid].pktio;

		TRACE_INFO("ODP pktio stop %d.\n", portid);
		ret = odp_pktio_stop(pktio);
		if (ret < 0) {
		        TRACE_ERROR("stop pktio failed on %d", portid);
		        continue;
		}

		TRACE_INFO("ODP pktio close %d.\n", portid);
		ret = odp_pktio_close(pktio);
		if (ret < 0) {
		        TRACE_ERROR("close pktio failed on %d", portid);
		        continue;
		}
	}

	TRACE_INFO("ODP pool term.\n");
	pool = odp_pool_lookup(ODP_POOL_NAME);
	ret = odp_pool_destroy(pool);
	if (ret < 0) {
	        TRACE_ERROR("destroy pool failed");
	}
}
/*----------------------------------------------------------------------------*/
io_module_func odp_module_func = {
	.load_module		   = odp_load_module,
	.init_handle		   = odp_init_handle,
	.link_devices		   = odp_link_devices,
	.release_pkt		   = odp_release_pkt,
	.send_pkts		   = odp_send_pkts,
	.get_wptr   		   = odp_get_wptr,
	.recv_pkts		   = odp_recv_pkts,
	.get_rptr	   	   = odp_get_rptr,
	.select			   = odp_select,
	.destroy_handle		   = odp_destroy_handle
};
/*----------------------------------------------------------------------------*/
#else
io_module_func odp_module_func = {
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
#endif /* !DISABLE_ODP */
