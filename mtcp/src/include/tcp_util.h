#ifndef TCP_UTIL_H
#define TCP_UTIL_H

#include "mtcp.h"
#include "tcp_stream.h"

struct tcp_timestamp
{
	uint32_t ts_val;
	uint32_t ts_ref;
};

void ParseTCPOptions(tcp_stream *cur_stream,
		        uint32_t cur_ts, uint8_t *tcpopt, int len);

extern inline int
ParseTCPTimestamp(tcp_stream *cur_stream,
		        struct tcp_timestamp *ts, uint8_t *tcpopt, int len);

#if TCP_OPT_SACK_ENABLED
void
ParseSACKOption(tcp_stream *cur_stream,
		        uint32_t ack_seq, uint8_t *tcpopt, int len);
#endif

uint16_t
TCPCalcChecksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr);

void
PrintTCPOptions(uint8_t *tcpopt, int len);

#endif /* TCP_UTIL_H */	
