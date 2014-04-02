#ifndef __IP_IN_H_
#define __IP_IN_H_

#include "mtcp.h"

int
ProcessIPv4Packet(mtcp_manager_t mtcp, uint32_t cur_ts, 
				  const int ifidx, unsigned char* pkt_data, int len);

#endif /* __IP_IN_H_ */
