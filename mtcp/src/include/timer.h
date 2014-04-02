#ifndef __TIMER_H_
#define __TIMER_H_

#include "mtcp.h"
#include "tcp_stream.h"

#define RTO_HASH 3000

struct rto_hashstore 
{
	uint32_t rto_now_idx; // pointing the hs_table_s index
	uint32_t rto_now_ts; // 
	
	TAILQ_HEAD(rto_head , tcp_stream) rto_list[RTO_HASH+1];
};

struct rto_hashstore* 
InitRTOHashstore();

inline void 
AddtoRTOList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void 
RemoveFromRTOList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void 
AddtoTimewaitList(mtcp_manager_t mtcp, tcp_stream *cur_stream, uint32_t cur_ts);

inline void 
RemoveFromTimewaitList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void 
AddtoTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void 
RemoveFromTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void 
UpdateTimeoutList(mtcp_manager_t mtcp, tcp_stream *cur_stream);

inline void
UpdateRetransmissionTimer(mtcp_manager_t mtcp, 
		tcp_stream *cur_stream, uint32_t cur_ts);

void
CheckRtmTimeout(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh);

void 
CheckTimewaitExpire(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh);

void 
CheckConnectionTimeout(mtcp_manager_t mtcp, uint32_t cur_ts, int thresh);

#endif /* __TIMER_H_ */
