#ifndef ADDR_POOL_H
#define ADDR_POOL_H

#include <netinet/in.h>
#include <sys/queue.h>

#define MIN_PORT (1025)
#define MAX_PORT (65535 + 1)
/*----------------------------------------------------------------------------*/
typedef struct addr_pool *addr_pool_t;
/*----------------------------------------------------------------------------*/
/* CreateAddressPool()                                                        */
/* Create address pool for given address range.                               */
/* addr_base: the base address in network order.                              */
/* num_addr: number of addresses to use as source IP                          */
/*----------------------------------------------------------------------------*/
addr_pool_t 
CreateAddressPool(in_addr_t addr_base, int num_addr);
/*----------------------------------------------------------------------------*/
/* CreateAddressPoolPerCore()                                                 */
/* Create address pool only for the given core number.                        */
/* All addresses and port numbers should be in network order.                 */
/*----------------------------------------------------------------------------*/
addr_pool_t 
CreateAddressPoolPerCore(int core, int num_queues, 
		in_addr_t saddr_base, int num_addr, in_addr_t daddr, in_port_t dport);
/*----------------------------------------------------------------------------*/
void
DestroyAddressPool(addr_pool_t ap);
/*----------------------------------------------------------------------------*/
int 
FetchAddress(addr_pool_t ap, int core, int num_queues, 
		const struct sockaddr_in *daddr, struct sockaddr_in *saddr);
/*----------------------------------------------------------------------------*/
int 
FetchAddressPerCore(addr_pool_t ap, int core, int num_queues, 
		    const struct sockaddr_in *daddr, struct sockaddr_in *saddr);
/*----------------------------------------------------------------------------*/
int 
FreeAddress(addr_pool_t ap, const struct sockaddr_in *addr);
/*----------------------------------------------------------------------------*/

#endif /* ADDR_POOL_H */
