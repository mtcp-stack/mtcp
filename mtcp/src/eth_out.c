#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <linux/if_ether.h>
#include <linux/tcp.h>
#include <netinet/ip.h>

#include "mtcp.h"
#include "arp.h"
#include "eth_out.h"
#include "debug.h"

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define MAX_WINDOW_SIZE 65535

/*----------------------------------------------------------------------------*/
enum ETH_BUFFER_RETURN {BUF_RET_MAYBE, BUF_RET_ALWAYS};
/*----------------------------------------------------------------------------*/
#if !(E_PSIO || USE_CHUNK_BUF)
/* XXX - ignored at the moment (this is disabled by default) */
inline void 
InitWriteChunks(struct ps_handle* handle, struct ps_chunk *w_chunk) 
{
	int i, ret;
	for (i = 0; i < ETH_NUM; i++)
	{
		ret = ps_alloc_chunk(handle, &w_chunk[i]);
		if (ret != 0)
		{
			perror("ps_alloc_chunk");
			exit(1);
		}
		w_chunk[i].queue.ifindex = i;
		w_chunk[i].recv_blocking = 0;
		w_chunk[i].cnt = 0;
	}
}
/*----------------------------------------------------------------------------*/
int
FlushWriteBuffer(struct mtcp_thread_context* ctx, int ifidx)
{
	int ret = 0;
	struct ps_chunk* w_chunk = ctx->w_chunk;
	mtcp_manager_t mtcp = ctx->mtcp_manager;
	int i;
	int drop = 0;
	assert(ctx != NULL);
	assert(w_chunk != NULL);
			
	if (w_chunk[ifidx].cnt > 0) {

		STAT_COUNT(mtcp->runstat.rounds_tx_try);

		ret = ps_send_chunk(ctx->handle, &w_chunk[ifidx]);
		drop = ctx->w_chunk[ifidx].cnt - ret;

		if (ret < 0) {
			TRACE_ERROR("ps_send_chunk failed to send chunks, %d:%d\n", 
					ifidx, w_chunk[ifidx].cnt);
			return ret;
		} else {
#ifdef NETSTAT
			mtcp->nstat.tx_packets[ifidx] += ret;
#endif /* NETSTAT */

			for (i = 0; i < ret; i++) {
#ifdef PKTDUMP
				DumpPacket(mtcp, 
						w_chunk[ifidx].buf + w_chunk[ifidx].info[i].offset, 
						w_chunk[ifidx].info[i].len, "OUT", ifidx);
#endif /* PKTDUMP */

#ifdef NETSTAT
				mtcp->nstat.tx_bytes[ifidx] += w_chunk[ifidx].info[i].len + 24;
#endif /* NETSTAT */
			}

#ifdef NETSTAT
			if (ret != w_chunk[ifidx].cnt) {
				mtcp->nstat.tx_drops[ifidx] += (w_chunk[ifidx].cnt - ret);
			}
#endif /* NETSTAT */

			if (ret == 0) {
				return ret;
			}
		}
		
#ifdef PKTDUMP
		thread_printf(mtcp, mtcp->log_fp, "sent chunks, ret: %d (tries: %d)\n", 
				ret, w_chunk[ifidx].cnt);
		thread_printf(mtcp, mtcp->log_fp, "======================================"
					"======================================================"
					"====================\n\n");
#endif /* PKTDUMP */

		if (drop > 0) {
			ctx->w_chunk[ifidx].cnt = drop;
			for (i = 0; i < drop; i++) {
				ctx->w_chunk[ifidx].info[i].len = 
						ctx->w_chunk[ifidx].info[ret + i].len;
				ctx->w_chunk[ifidx].info[i].offset = 
					ctx->w_chunk[ifidx].info[ret + i].offset;
			}
			ctx->w_off[ifidx] = ctx->w_chunk[ifidx].info[drop - 1].offset +
					(ctx->w_chunk[ifidx].info[drop - 1].len + 63) / 64 * 64;
			ctx->w_cur_idx[ifidx] += ret;
		} else {
			ctx->w_chunk[ifidx].cnt = 0;
			ctx->w_off[ifidx] = 0;
			ctx->w_cur_idx[ifidx] = 0;
		}

	}

	return ret;
}
/*----------------------------------------------------------------------------*/
static inline char *
GetWriteBuffer(struct mtcp_thread_context *ctx, int method, int ifidx, int len)
{
	struct ps_chunk *w_chunk = ctx->w_chunk;
	uint32_t *w_off = ctx->w_off;
	int w_idx;

	assert(w_chunk != NULL);
	assert(w_off != NULL);
	
	if (ifidx < 0 || ifidx >= CONFIG.eths_num )
		return NULL;

	//pthread_mutex_lock(&ctx->send_lock);

	if (ctx->w_cur_idx[ifidx] + w_chunk[ifidx].cnt >= MAX_SEND_PCK_CHUNK) {
		if (method == BUF_RET_MAYBE) {
			return NULL;
		} else if (method == BUF_RET_ALWAYS) {
			if (FlushWriteBuffer(ctx, ifidx) <= 0)
				return NULL;
		} else {
			assert(0);
		}
	}

	assert(ctx->w_cur_idx[ifidx] + w_chunk[ifidx].cnt < MAX_SEND_PCK_CHUNK);
	assert(w_off[ifidx] < MAX_PACKET_SIZE * MAX_CHUNK_SIZE);

	w_idx = w_chunk[ifidx].cnt++;
	w_chunk[ifidx].info[w_idx].len = len;
	w_chunk[ifidx].info[w_idx].offset = w_off[ifidx];
	w_off[ifidx] += (len + 63) / 64 * 64;

	//pthread_mutex_unlock(&ctx->send_lock);

	return (w_chunk[ifidx].buf + w_chunk[ifidx].info[w_idx].offset);
}
/*----------------------------------------------------------------------------*/
#else /* E_PSIO */
int 
FlushSendChunkBuf(mtcp_manager_t mtcp, int nif)
{
	return 0;
}
#endif /* E_PSIO */
/*----------------------------------------------------------------------------*/
uint8_t *
EthernetOutput(struct mtcp_manager *mtcp, uint16_t h_proto, 
		int nif, unsigned char* dst_haddr, uint16_t iplen)
{
	uint8_t *buf;
	struct ethhdr *ethh;
	int i;

#if E_PSIO || USE_CHUNK_BUF
	buf = mtcp->iom->get_wptr(mtcp->ctx, nif, iplen + ETHERNET_HEADER_LEN);
#else
	buf = GetWriteBuffer(mtcp->ctx, 
			BUF_RET_MAYBE, nif, iplen + ETHERNET_HEADER_LEN);
#endif
	if (!buf) {
		//TRACE_DBG("Failed to get available write buffer\n");
		return NULL;
	}
	//memset(buf, 0, ETHERNET_HEADER_LEN + iplen);

//	if (!stream->sndvar->d_haddr) {
//		stream->sndvar->d_haddr = GetDestinationHWaddr(stream->daddr);
//	}

#if 0
	TRACE_DBG("dst_hwaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
				stream->sndvar->d_haddr[0], stream->sndvar->d_haddr[1], 
				stream->sndvar->d_haddr[2],	stream->sndvar->d_haddr[3], 
				stream->sndvar->d_haddr[4], stream->sndvar->d_haddr[5]);
#endif

	ethh = (struct ethhdr *)buf;
	for (i = 0; i < ETH_ALEN; i++) {
		ethh->h_source[i] = CONFIG.eths[nif].haddr[i];
		ethh->h_dest[i] = dst_haddr[i];
	}
	ethh->h_proto = htons(h_proto);

	return (uint8_t *)(ethh + 1);
}
/*----------------------------------------------------------------------------*/
