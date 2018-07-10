#ifndef IP_IN_H
#define IP_IN_H

#include "mtcp.h"

int
ProcessIPv4Packet(mtcp_manager_t mtcp, uint32_t cur_ts, 
				  const int ifidx, unsigned char* pkt_data, int len);

#endif /* IP_IN_H */
