#ifndef ICMP_H
#define ICMP_H
/*----------------------------------------------------------------------------*/
struct icmphdr {
	uint8_t  icmp_type;
	uint8_t  icmp_code;
	uint16_t icmp_checksum;
	union {
		struct {
			uint16_t icmp_id;
			uint16_t icmp_sequence;
		} echo;                     // ECHO | ECHOREPLY
		struct {
			uint16_t unused;
			uint16_t nhop_mtu;
		} dest;                     // DEST_UNREACH
	} un;
};
/*----------------------------------------------------------------------------*/
/* getters and setters for ICMP fields */
#define ICMP_ECHO_GET_ID(icmph)          (icmph->un.echo.icmp_id)
#define ICMP_ECHO_GET_SEQ(icmph)         (icmph->un.echo.icmp_sequence)
#define ICMP_DEST_UNREACH_GET_MTU(icmph) (icmph->un.dest.nhop_mtu)

#define ICMP_ECHO_SET_ID(icmph, id)      (icmph->un.echo.icmp_id = id)
#define ICMP_ECHO_SET_SEQ(icmph, seq)    (icmph->un.echo.icmp_sequence = seq)

void
RequestICMP(mtcp_manager_t mtcp, uint32_t saddr, uint32_t daddr,
	    uint16_t icmp_id, uint16_t icmp_seq,
	    uint8_t *icmpd, uint16_t len);

int 
ProcessICMPPacket(mtcp_manager_t mtcp, struct iphdr *iph, int len);

/* ICMP types */
#define ICMP_ECHOREPLY      0   /* Echo Reply               */
#define ICMP_DEST_UNREACH   3   /* Destination Unreachable  */
#define ICMP_SOURCE_QUENCH  4   /* Source Quench            */
#define ICMP_REDIRECT       5   /* Redirect (change route)  */
#define ICMP_ECHO           8   /* Echo Request             */
#define ICMP_TIME_EXCEEDED  11  /* Time Exceeded            */
#define ICMP_PARAMETERPROB  12  /* Parameter Problem        */
#define ICMP_TIMESTAMP      13  /* Timestamp Request        */
#define ICMP_TIMESTAMPREPLY 14  /* Timestamp Reply          */
#define ICMP_INFO_REQUEST   15  /* Information Request      */
#define ICMP_INFO_REPLY     16  /* Information Reply        */
#define ICMP_ADDRESS        17  /* Address Mask Request     */
#define ICMP_ADDRESSREPLY   18  /* Address Mask Reply       */
/*----------------------------------------------------------------------------*/
#endif /* ICMP_H */
