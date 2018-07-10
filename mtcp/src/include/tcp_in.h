#ifndef TCP_IN_H
#define TCP_IN_H

#include <linux/if_ether.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <netinet/ip.h>

#include "mtcp.h"
#include "fhash.h"

#ifndef TCP_FLAGS
#define TCP_FLAGS
#define TCP_FLAG_FIN	0x01	// 0000 0001
#define TCP_FLAG_SYN	0x02	// 0000 0010
#define TCP_FLAG_RST	0x04	// 0000 0100
#define TCP_FLAG_PSH	0x08	// 0000 1000
#define TCP_FLAG_ACK	0x10	// 0001 0000
#define TCP_FLAG_URG	0x20	// 0010 0000
#endif
#define TCP_FLAG_SACK	0x40	// 0100 0000
#define TCP_FLAG_WACK	0x80	// 1000 0000

#define TCP_OPT_FLAG_MSS			0x02	// 0000 0010
#define TCP_OPT_FLAG_WSCALE			0x04	// 0000 0100
#define TCP_OPT_FLAG_SACK_PERMIT	0x08	// 0000 1000
#define TCP_OPT_FLAG_SACK			0x10	// 0001 0000
#define TCP_OPT_FLAG_TIMESTAMP		0x20	// 0010 0000	

#define TCP_OPT_MSS_LEN			4
#define TCP_OPT_WSCALE_LEN		3
#define TCP_OPT_SACK_PERMIT_LEN	2
#define TCP_OPT_SACK_LEN		10
#define TCP_OPT_TIMESTAMP_LEN	10

#define TCP_DEFAULT_MSS			1460
#define TCP_DEFAULT_WSCALE		7
#define TCP_INITIAL_WINDOW		14600	// initial window size

#define TCP_SEQ_LT(a,b) 		((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)		((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b) 		((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)		((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)	(TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

/* convert timeval to timestamp (precision: 1 ms) */
#define HZ						1000
#define TIME_TICK				(1000000/HZ)		// in us
#define TIMEVAL_TO_TS(t)		(uint32_t)((t)->tv_sec * HZ + \
								((t)->tv_usec / TIME_TICK))

#define TS_TO_USEC(t)			((t) * TIME_TICK)
#define TS_TO_MSEC(t)			(TS_TO_USEC(t) / 1000)

#define USEC_TO_TS(t)			((t) / TIME_TICK)
#define MSEC_TO_TS(t)			(USEC_TO_TS((t) * 1000))
#define SEC_TO_TS(t)			(t * HZ)

#define SEC_TO_USEC(t)			((t) * 1000000)
#define SEC_TO_MSEC(t)			((t) * 1000)
#define MSEC_TO_USEC(t)			((t) * 1000)
#define USEC_TO_SEC(t)			((t) / 1000000)
//#define TCP_TIMEWAIT			(MSEC_TO_USEC(5000) / TIME_TICK)	// 5s
#define TCP_TIMEWAIT			0
#define TCP_INITIAL_RTO			(MSEC_TO_USEC(500) / TIME_TICK)		// 500ms
#define TCP_FIN_RTO				(MSEC_TO_USEC(500) / TIME_TICK)		// 500ms
#define TCP_TIMEOUT				(MSEC_TO_USEC(30000) / TIME_TICK)	// 30s

#define TCP_MAX_RTX				16
#define TCP_MAX_SYN_RETRY		7
#define TCP_MAX_BACKOFF			7

enum tcp_state
{
	TCP_ST_CLOSED		= 0, 
	TCP_ST_LISTEN		= 1, 
	TCP_ST_SYN_SENT		= 2, 
	TCP_ST_SYN_RCVD		= 3,  
	TCP_ST_ESTABLISHED	= 4, 
	TCP_ST_FIN_WAIT_1	= 5, 
	TCP_ST_FIN_WAIT_2	= 6, 
	TCP_ST_CLOSE_WAIT	= 7, 
	TCP_ST_CLOSING		= 8, 
	TCP_ST_LAST_ACK		= 9, 
	TCP_ST_TIME_WAIT	= 10
};

enum tcp_option
{
	TCP_OPT_END			= 0,
	TCP_OPT_NOP			= 1,
	TCP_OPT_MSS			= 2,
	TCP_OPT_WSCALE		= 3,
	TCP_OPT_SACK_PERMIT	= 4, 
	TCP_OPT_SACK		= 5,
	TCP_OPT_TIMESTAMP	= 8
};

enum tcp_close_reason
{
	TCP_NOT_CLOSED		= 0, 
	TCP_ACTIVE_CLOSE	= 1, 
	TCP_PASSIVE_CLOSE	= 2, 
	TCP_CONN_FAIL		= 3, 
	TCP_CONN_LOST		= 4, 
	TCP_RESET			= 5, 
	TCP_NO_MEM			= 6, 
	TCP_NOT_ACCEPTED	= 7, 
	TCP_TIMEDOUT		= 8
};

void 
ParseTCPOptions(tcp_stream *cur_stream, 
		uint32_t cur_ts, uint8_t *tcpopt, int len);

extern inline int 
ProcessTCPUplink(mtcp_manager_t mtcp, uint32_t cur_ts, tcp_stream *cur_stream, 
		const struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, 
		uint8_t *payload, int payloadlen, uint32_t window);

int
ProcessTCPPacket(struct mtcp_manager *mtcp, uint32_t cur_ts, const int ifidx,
					const struct iphdr* iph, int ip_len);
uint16_t 
TCPCalcChecksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr);

#endif /* TCP_IN_H */
