#ifndef __ARP_H_
#define __ARP_H_

#define MAX_ARPENTRY 1024

int 
InitARPTable();

unsigned char * 
GetHWaddr(uint32_t ip);

unsigned char *
GetDestinationHWaddr(uint32_t dip, uint8_t is_gateway);

void 
RequestARP(mtcp_manager_t mtcp, uint32_t ip, int nif, uint32_t cur_ts);

int 
ProcessARPPacket(mtcp_manager_t mtcp, uint32_t cur_ts,
		const int ifidx, unsigned char* pkt_data, int len);

void 
ARPTimer(mtcp_manager_t mtcp, uint32_t cur_ts);

void 
PrintARPTable();

#endif /* __ARP_H_ */
