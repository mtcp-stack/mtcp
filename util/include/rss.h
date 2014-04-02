#ifndef _RSS_H_
#define _RSS_H_

#include <netinet/in.h>

/* return RSS hash value (32 bit) based on 4 tuple values */
uint32_t GetRSSHash(in_addr_t sip, in_addr_t dip, 
					in_port_t sp, in_port_t dp);

/* sip, dip, sp, dp: host-byte order */
int GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
				  in_port_t sp, in_port_t dp, int num_cpus);
#endif
