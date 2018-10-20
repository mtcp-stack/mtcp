#include <stdint.h>
#include <sys/types.h>
#include <netinet/ip.h>

#include "mtcp.h"
#include "icmp.h"
#include "eth_out.h"
#include "ip_in.h"
#include "ip_out.h"
#include "debug.h"
#include "arp.h"

#define IP_NEXT_PTR(iph) ((uint8_t *)iph + (iph->ihl << 2))
/*----------------------------------------------------------------------------*/
void 
DumpICMPPacket(mtcp_manager_t mtcp, struct icmphdr *icmph, uint32_t saddr, uint32_t daddr);
/*----------------------------------------------------------------------------*/
static uint16_t
ICMPChecksum(uint16_t *icmph, int len)
{
	assert(len >= 0);
	
	uint16_t ret = 0;
	uint32_t sum = 0;
	uint16_t odd_byte;
	
	while (len > 1) {
		sum += *icmph++;
		len -= 2;
	}
	
	if (len == 1) {
		*(uint8_t*)(&odd_byte) = * (uint8_t*)icmph;
		sum += odd_byte;
	}
	
	sum =  (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	ret =  ~sum;
	
	return ret; 
}
/*----------------------------------------------------------------------------*/
static int
ICMPOutput(struct mtcp_manager *mtcp, uint32_t saddr, uint32_t daddr,
	   uint8_t icmp_type, uint8_t icmp_code, uint16_t icmp_id, uint16_t icmp_seq,
	   uint8_t *icmpd, uint16_t len)
{
	struct icmphdr *icmph;
	
	icmph = (struct icmphdr *)IPOutputStandalone(mtcp, 
			IPPROTO_ICMP, 0, saddr, daddr, sizeof(struct icmphdr) + len);
	if (!icmph)
		return -1;
	
	/* Fill in the icmp header */
	icmph->icmp_type = icmp_type;
	icmph->icmp_code = icmp_code;
	icmph->icmp_checksum = 0;
	ICMP_ECHO_SET_ID(icmph, htons(icmp_id));
	ICMP_ECHO_SET_SEQ(icmph, htons(icmp_seq));
	
	/* Fill in the icmp data */
	if (len > 0)
		memcpy((void *)(icmph + 1), icmpd, len);
	
	/* Calculate ICMP Checksum with header and data */
	icmph->icmp_checksum = 
		ICMPChecksum((uint16_t *)icmph, sizeof(struct icmphdr) + len);
	
#if DBGMSG
	DumpICMPPacket(mtcp, icmph, saddr, daddr);
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
void
RequestICMP(mtcp_manager_t mtcp, uint32_t saddr, uint32_t daddr,
	    uint16_t icmp_id, uint16_t icmp_sequence,
	    uint8_t *icmpd, uint16_t len)
{
	/* send icmp request with given parameters */
	ICMPOutput(mtcp, saddr, daddr, ICMP_ECHO, 0, ntohs(icmp_id), ntohs(icmp_sequence),
		   icmpd, len);
}
/*----------------------------------------------------------------------------*/
static int 
ProcessICMPECHORequest(mtcp_manager_t mtcp, struct iphdr *iph, int len)
{
	int ret = 0;
	struct icmphdr *icmph = (struct icmphdr *) IP_NEXT_PTR(iph);
	
	/* Check correctness of ICMP checksum and send ICMP echo reply */
	if (ICMPChecksum((uint16_t *) icmph, len - (iph->ihl << 2)) )
		ret = ERROR;
	else
		ICMPOutput(mtcp, iph->daddr, iph->saddr, ICMP_ECHOREPLY, 0, 
			   ntohs(ICMP_ECHO_GET_ID(icmph)), ntohs(ICMP_ECHO_GET_SEQ(icmph)), 
			   (uint8_t *) (icmph + 1),
			   (uint16_t) (len - (iph->ihl << 2) - sizeof(struct icmphdr)) );
	
	return ret;
}
/*----------------------------------------------------------------------------*/
int 
ProcessICMPPacket(mtcp_manager_t mtcp, struct iphdr *iph, int len)
{
	struct icmphdr *icmph = (struct icmphdr *) IP_NEXT_PTR(iph);
	int i;
	int to_me = FALSE;
	
	/* process the icmp messages destined to me */
	for (i = 0; i < CONFIG.eths_num; i++) {
		if (iph->daddr == CONFIG.eths[i].ip_addr) {
			to_me = TRUE;
		}
	}
	
	if (!to_me)
		return TRUE;
	
	switch (icmph->icmp_type) {
        case ICMP_ECHO:
		ProcessICMPECHORequest(mtcp, iph, len);
		break;
		
        case ICMP_DEST_UNREACH:
		TRACE_INFO("[INFO] ICMP Destination Unreachable message received\n");
		break;
		
        case ICMP_TIME_EXCEEDED:
		TRACE_INFO("[INFO] ICMP Time Exceeded message received\n");
		break;
		
        default:
		TRACE_INFO("[INFO] Unsupported ICMP message type %x received\n",
			   icmph->icmp_type);
		break;
	}
	
	return TRUE;
}
/*----------------------------------------------------------------------------*/
void 
DumpICMPPacket(mtcp_manager_t mtcp, struct icmphdr *icmph, uint32_t saddr, uint32_t daddr)
{
	uint8_t *t;
	
	thread_printf(mtcp, mtcp->log_fp, "ICMP header: \n");
	thread_printf(mtcp, mtcp->log_fp, "Type: %d, "
		      "Code: %d, ID: %d, Sequence: %d\n", 
		      icmph->icmp_type, icmph->icmp_code,
		      ntohs(ICMP_ECHO_GET_ID(icmph)), ntohs(ICMP_ECHO_GET_SEQ(icmph)));
	
	t = (uint8_t *)&saddr;
	thread_printf(mtcp, mtcp->log_fp, "Sender IP: %u.%u.%u.%u\n",
		      t[0], t[1], t[2], t[3]);
	
	t = (uint8_t *)&daddr;
	thread_printf(mtcp, mtcp->log_fp, "Target IP: %u.%u.%u.%u\n",
		      t[0], t[1], t[2], t[3]);
}
/*----------------------------------------------------------------------------*/
#undef IP_NEXT_PTR
