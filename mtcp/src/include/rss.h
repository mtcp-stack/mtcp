#ifndef RSS_H
#define RSS_H

#include <netinet/in.h>

/* sip, dip, sp, dp: in network byte order */
int GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
		  in_port_t sp, in_port_t dp, int num_queues,
		  uint8_t endian_check);

#endif /* RSS_H */
