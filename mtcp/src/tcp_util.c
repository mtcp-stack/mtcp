#include <assert.h>

#include "tcp_util.h"
#include "tcp_ring_buffer.h"
#include "eventpoll.h"
#include "debug.h"
#include "timer.h"
#include "ip_in.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*---------------------------------------------------------------------------*/
void 
ParseTCPOptions(tcp_stream *cur_stream, 
		uint32_t cur_ts, uint8_t *tcpopt, int len)
{
	int i;
	unsigned int opt, optlen;

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		
		if (opt == TCP_OPT_END) {	// end of option field
			break;
		} else if (opt == TCP_OPT_NOP) {	// no option
			continue;
		} else {

			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > len) {
				break;
			}

			if (opt == TCP_OPT_MSS) {
				cur_stream->sndvar->mss = *(tcpopt + i++) << 8;
				cur_stream->sndvar->mss += *(tcpopt + i++);
				cur_stream->sndvar->eff_mss = cur_stream->sndvar->mss;
#if TCP_OPT_TIMESTAMP_ENABLED
				cur_stream->sndvar->eff_mss -= (TCP_OPT_TIMESTAMP_LEN + 2);
#endif
			} else if (opt == TCP_OPT_WSCALE) {
				cur_stream->sndvar->wscale_peer = *(tcpopt + i++);
			} else if (opt == TCP_OPT_SACK_PERMIT) {
				cur_stream->sack_permit = TRUE;
				TRACE_SACK("Remote SACK permited.\n");
			} else if (opt == TCP_OPT_TIMESTAMP) {
				TRACE_TSTAMP("Saw peer timestamp!\n");
				cur_stream->saw_timestamp = TRUE;
				cur_stream->rcvvar->ts_recent = ntohl(*(uint32_t *)(tcpopt + i));
				cur_stream->rcvvar->ts_last_ts_upd = cur_ts;
				i += 8;
			} else {
				// not handle
				i += optlen - 2;
			}
		}
	}
}
/*---------------------------------------------------------------------------*/
inline int  
ParseTCPTimestamp(tcp_stream *cur_stream, 
		struct tcp_timestamp *ts, uint8_t *tcpopt, int len)
{
	int i;
	unsigned int opt, optlen;

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		
		if (opt == TCP_OPT_END) {	// end of option field
			break;
		} else if (opt == TCP_OPT_NOP) {	// no option
			continue;
		} else {
			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > len) {
				break;
			}

			if (opt == TCP_OPT_TIMESTAMP) {
				ts->ts_val = ntohl(*(uint32_t *)(tcpopt + i));
				ts->ts_ref = ntohl(*(uint32_t *)(tcpopt + i + 4));
				return TRUE;
			} else {
				// not handle
				i += optlen - 2;
			}
		}
	}
	return FALSE;
}
#if TCP_OPT_SACK_ENABLED
/*----------------------------------------------------------------------------*/
void 
ParseSACKOption(tcp_stream *cur_stream, 
		uint32_t ack_seq, uint8_t *tcpopt, int len)
{
	int i, j;
	unsigned int opt, optlen;
	uint32_t left_edge, right_edge;

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		
		if (opt == TCP_OPT_END) {	// end of option field
			break;
		} else if (opt == TCP_OPT_NOP) {	// no option
			continue;
		} else {
			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > len) {
				break;
			}

			if (opt == TCP_OPT_SACK) {
				j = 0;
				while (j < optlen - 2) {
					left_edge = ntohl(*(uint32_t *)(tcpopt + i + j));
					right_edge = ntohl(*(uint32_t *)(tcpopt + i + j + 4));

					cur_stream->rcvvar->sack_table
							[cur_stream->rcvvar->sacks].left_edge = left_edge;
					cur_stream->rcvvar->sack_table
							[cur_stream->rcvvar->sacks].right_edge = right_edge;
					cur_stream->rcvvar->sacks++;
					j += 8;
#if RTM_STAT
					cur_stream->rstat->sack_cnt++;
					cur_stream->rstat->sack_bytes += (right_edge - left_edge);
#endif
					if (cur_stream->rcvvar->dup_acks == 3) {
#if RTM_STAT
						cur_stream->rstat->tdp_sack_cnt++;
						cur_stream->
							rstat->tdp_sack_bytes += (right_edge - left_edge);
#endif
						TRACE_LOSS("SACK entry. "
								"left_edge: %u, right_edge: %u (ack_seq: %u)\n",
								left_edge, right_edge, ack_seq);

					}
					TRACE_SACK("Found SACK entry. "
							"left_edge: %u, right_edge: %u\n", 
							left_edge, right_edge);
				}
				i += j;
			} else {
				// not handle
				i += optlen - 2;
			}
		}
	}
}
#endif /* TCP_OPT_SACK_ENABLED */
/*---------------------------------------------------------------------------*/
uint16_t
TCPCalcChecksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
	uint32_t sum;
	uint16_t *w;
	int nleft;
	
	sum = 0;
	nleft = len;
	w = buf;
	
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	
	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);
	
	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(IPPROTO_TCP);
	
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	
	sum = ~sum;
	
	return (uint16_t)sum;
}
/*---------------------------------------------------------------------------*/
void 
PrintTCPOptions(uint8_t *tcpopt, int len)
{
	int i;
	unsigned int opt, optlen;

	for (i = 0; i < len; i++) {
		printf("%u ", tcpopt[i]);
	}
	printf("\n");

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		
		if (opt == TCP_OPT_END) {	// end of option field
			break;
		} else if (opt == TCP_OPT_NOP) {	// no option
			continue;
		} else {

			optlen = *(tcpopt + i++);

			printf("Option: %d", opt);
			printf(", length: %d", optlen);

			if (opt == TCP_OPT_MSS) {
				uint16_t mss;
				mss = *(tcpopt + i++) << 8;
				mss += *(tcpopt + i++);
				printf(", MSS: %u", mss);
			} else if (opt == TCP_OPT_SACK_PERMIT) {
				printf(", SACK permit");
			} else if (opt == TCP_OPT_TIMESTAMP) {
				uint32_t ts_val, ts_ref;
				ts_val = *(uint32_t *)(tcpopt + i);
				i += 4;
				ts_ref = *(uint32_t *)(tcpopt + i);
				i += 4;
				printf(", TSval: %u, TSref: %u", ts_val, ts_ref);
			} else if (opt == TCP_OPT_WSCALE) {
				uint8_t wscale;
				wscale = *(tcpopt + i++);
				printf(", Wscale: %u", wscale);
			} else {
				// not handle
				i += optlen - 2;
			}
			printf("\n");
		}
	}
}
