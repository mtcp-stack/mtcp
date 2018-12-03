#include <stdint.h>
#include <sys/types.h>
/* for inet_ntoa() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mtcp.h"
#include "arp.h"
#include "eth_out.h"
#include "debug.h"

#define ARP_PAD_LEN 18			/* arp pad length to fit 64B packet size */
#define ARP_TIMEOUT_SEC 1		/* 1 second arp timeout */

/*----------------------------------------------------------------------------*/
enum arp_hrd_format
{
	arp_hrd_ethernet = 1
};
/*----------------------------------------------------------------------------*/
enum arp_opcode
{
	arp_op_request = 1, 
	arp_op_reply = 2, 
};
/*----------------------------------------------------------------------------*/
struct arphdr
{
	uint16_t ar_hrd;			/* hardware address format */
	uint16_t ar_pro;			/* protocol address format */
	uint8_t ar_hln;				/* hardware address length */
	uint8_t ar_pln;				/* protocol address length */
	uint16_t ar_op;				/* arp opcode */
	
	uint8_t ar_sha[ETH_ALEN];	/* sender hardware address */
	uint32_t ar_sip;			/* sender ip address */
	uint8_t ar_tha[ETH_ALEN];	/* targe hardware address */
	uint32_t ar_tip;			/* target ip address */

	uint8_t pad[ARP_PAD_LEN];
} __attribute__ ((packed));
/*----------------------------------------------------------------------------*/
struct arp_queue_entry
{
	uint32_t ip;		/* target ip address */
	int nif_out;		/* output interface number */
	uint32_t ts_out;	/* last sent timestamp */

	TAILQ_ENTRY(arp_queue_entry) arp_link;
};
/*----------------------------------------------------------------------------*/
struct arp_manager
{
	TAILQ_HEAD (, arp_queue_entry) list;
	pthread_mutex_t lock;
};
/*----------------------------------------------------------------------------*/
struct arp_manager g_arpm;
/*----------------------------------------------------------------------------*/
void 
DumpARPPacket(mtcp_manager_t mtcp, struct arphdr *arph);
/*----------------------------------------------------------------------------*/
int 
InitARPTable()
{
	CONFIG.arp.entries = 0;

	CONFIG.arp.entry = (struct arp_entry *)
				calloc(MAX_ARPENTRY, sizeof(struct arp_entry));
	if (CONFIG.arp.entry == NULL) {
		perror("calloc");
		return -1;
	}

	TAILQ_INIT(&g_arpm.list);
	pthread_mutex_init(&g_arpm.lock, NULL);

	return 0;
}
/*----------------------------------------------------------------------------*/
unsigned char *
GetHWaddr(uint32_t ip)
{
	int i;
	unsigned char *haddr = NULL;
	for (i = 0; i < CONFIG.eths_num; i++) {
		if (ip == CONFIG.eths[i].ip_addr) {
			haddr = CONFIG.eths[i].haddr;
			break;
		}	
	}

	return haddr;
}
/*----------------------------------------------------------------------------*/
unsigned char *
GetDestinationHWaddr(uint32_t dip, uint8_t is_gateway)
{
	unsigned char *d_haddr = NULL;
	int prefix = 0;
	int i;

	if (is_gateway == 1 && CONFIG.arp.gateway)
		d_haddr = (CONFIG.arp.gateway)->haddr;
	else {	
		/* Longest prefix matching */
		for (i = 0; i < CONFIG.arp.entries; i++) {
			if (CONFIG.arp.entry[i].prefix == 1) {
				if (CONFIG.arp.entry[i].ip == dip) {
					d_haddr = CONFIG.arp.entry[i].haddr;
					break;
				}	
			} else {
				if ((dip & CONFIG.arp.entry[i].ip_mask) ==
				    CONFIG.arp.entry[i].ip_masked) {
					
					if (CONFIG.arp.entry[i].prefix > prefix) {
						d_haddr = CONFIG.arp.entry[i].haddr;
						prefix = CONFIG.arp.entry[i].prefix;
					}
				}
			}
		}
	}
	
	return d_haddr;
}
/*----------------------------------------------------------------------------*/
static int 
ARPOutput(struct mtcp_manager *mtcp, int nif, int opcode,
		uint32_t dst_ip, unsigned char *dst_haddr, unsigned char *target_haddr)
{
	if (!dst_haddr)
		return -1;

	/* Allocate a buffer */
	struct arphdr *arph = (struct arphdr *)EthernetOutput(mtcp, 
			ETH_P_ARP, nif, dst_haddr, sizeof(struct arphdr));
	if (!arph) {
		return -1;
	}
	/* Fill arp header */
	arph->ar_hrd = htons(arp_hrd_ethernet);
	arph->ar_pro = htons(ETH_P_IP);
	arph->ar_hln = ETH_ALEN;
	arph->ar_pln = 4;
	arph->ar_op = htons(opcode);

	/* Fill arp body */
	int edix = CONFIG.nif_to_eidx[nif];
	arph->ar_sip = CONFIG.eths[edix].ip_addr;
	arph->ar_tip = dst_ip;

	memcpy(arph->ar_sha, CONFIG.eths[edix].haddr, arph->ar_hln);
	if (target_haddr) {
		memcpy(arph->ar_tha, target_haddr, arph->ar_hln);
	} else {
		memcpy(arph->ar_tha, dst_haddr, arph->ar_hln);
	}
	memset(arph->pad, 0, ARP_PAD_LEN);

#if DBGMSG
	DumpARPPacket(mtcp, arph);
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
RegisterARPEntry(uint32_t ip, const unsigned char *haddr)
{
	int idx = CONFIG.arp.entries;
	
	CONFIG.arp.entry[idx].prefix = 32;
	CONFIG.arp.entry[idx].ip = ip;
	memcpy(CONFIG.arp.entry[idx].haddr, haddr, ETH_ALEN);
	CONFIG.arp.entry[idx].ip_mask = -1;
	CONFIG.arp.entry[idx].ip_masked = ip;

	if (CONFIG.gateway && ((CONFIG.gateway)->daddr &
			       CONFIG.arp.entry[idx].ip_mask) ==
	    CONFIG.arp.entry[idx].ip_masked) {
		CONFIG.arp.gateway = &CONFIG.arp.entry[idx];
		TRACE_CONFIG("ARP Gateway SET!\n");
	}
	
	CONFIG.arp.entries = idx + 1;

	TRACE_CONFIG("Learned new arp entry.\n");
	PrintARPTable();

	return 0;
}
/*----------------------------------------------------------------------------*/
void 
RequestARP(mtcp_manager_t mtcp, uint32_t ip, int nif, uint32_t cur_ts)
{
	struct arp_queue_entry *ent;
	unsigned char haddr[ETH_ALEN];
	unsigned char taddr[ETH_ALEN];

	pthread_mutex_lock(&g_arpm.lock);
	/* if the arp request is in progress, return */
	TAILQ_FOREACH(ent, &g_arpm.list, arp_link) {
		if (ent->ip == ip) {
			pthread_mutex_unlock(&g_arpm.lock);
			return;
		}
	}

	ent = (struct arp_queue_entry *)calloc(1, sizeof(struct arp_queue_entry));
	ent->ip = ip;
	ent->nif_out = nif;
	ent->ts_out = cur_ts;
	TAILQ_INSERT_TAIL(&g_arpm.list, ent, arp_link);
	pthread_mutex_unlock(&g_arpm.lock);

	/* else, broadcast arp request */
	memset(haddr, 0xFF, ETH_ALEN);
	memset(taddr, 0x00, ETH_ALEN);
	ARPOutput(mtcp, nif, arp_op_request, ip, haddr, taddr);
}
/*----------------------------------------------------------------------------*/
static int 
ProcessARPRequest(mtcp_manager_t mtcp, 
		struct arphdr *arph, int nif, uint32_t cur_ts)
{
	unsigned char *temp;

	/* register the arp entry if not exist */
	temp = GetDestinationHWaddr(arph->ar_sip, 0);
	if (!temp) {
		RegisterARPEntry(arph->ar_sip, arph->ar_sha);
	}

	/* send arp reply */
	ARPOutput(mtcp, nif, arp_op_reply, arph->ar_sip, arph->ar_sha, NULL);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int 
ProcessARPReply(mtcp_manager_t mtcp, struct arphdr *arph, uint32_t cur_ts)
{
	unsigned char *temp;
	struct arp_queue_entry *ent;

	/* register the arp entry if not exist */
	temp = GetDestinationHWaddr(arph->ar_sip, 0);
	if (!temp) {
		RegisterARPEntry(arph->ar_sip, arph->ar_sha);
	}

	/* remove from the arp request queue */
	pthread_mutex_lock(&g_arpm.lock);
	TAILQ_FOREACH(ent, &g_arpm.list, arp_link) {
		if (ent->ip == arph->ar_sip) {
			TAILQ_REMOVE(&g_arpm.list, ent, arp_link);
			free(ent);
			break;
		}
	}
	pthread_mutex_unlock(&g_arpm.lock);

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
ProcessARPPacket(mtcp_manager_t mtcp, uint32_t cur_ts,
		                  const int ifidx, unsigned char *pkt_data, int len)
{
	struct arphdr *arph = (struct arphdr *)(pkt_data + sizeof(struct ethhdr));
	int i, nif;
	int to_me = FALSE;
	
	/* process the arp messages destined to me */
	for (i = 0; i < CONFIG.eths_num; i++) {
		if (arph->ar_tip == CONFIG.eths[i].ip_addr) {
			to_me = TRUE;
		}
	}

	if (!to_me)
		return TRUE;
	
#if DBGMSG
	DumpARPPacket(mtcp, arph);
#endif

	switch (ntohs(arph->ar_op)) {
		case arp_op_request:
		        nif = CONFIG.eths[ifidx].ifindex; // use the port index as argument
			ProcessARPRequest(mtcp, arph, nif, cur_ts);
			break;

		case arp_op_reply:
			ProcessARPReply(mtcp, arph, cur_ts);
			break;

		default:
			break;
	}

	return TRUE;
}
/*----------------------------------------------------------------------------*/
/* ARPTimer: wakes up every milisecond and check the ARP timeout              */
/*           timeout is set to 1 second                                       */
/*----------------------------------------------------------------------------*/
void 
ARPTimer(mtcp_manager_t mtcp, uint32_t cur_ts)
{
	struct arp_queue_entry *ent, *ent_tmp;

	/* if the arp requet is timed out, retransmit */
	pthread_mutex_lock(&g_arpm.lock);
	TAILQ_FOREACH_SAFE(ent, &g_arpm.list, arp_link, ent_tmp) {
		if (TCP_SEQ_GT(cur_ts, ent->ts_out + SEC_TO_TS(ARP_TIMEOUT_SEC))) {
			struct in_addr ina;
			ina.s_addr = ent->ip;
			TRACE_INFO("[CPU%2d] ARP request for %s timed out.\n", 
				   mtcp->ctx->cpu, inet_ntoa(ina));
			TAILQ_REMOVE(&g_arpm.list, ent, arp_link);
			free(ent);
		}
	}
	pthread_mutex_unlock(&g_arpm.lock);
}
/*----------------------------------------------------------------------------*/
void
PrintARPTable()
{
	int i;
		
	/* print out process start information */
	TRACE_CONFIG("ARP Table:\n");
	for (i = 0; i < CONFIG.arp.entries; i++) {
			
		uint8_t *da = (uint8_t *)&CONFIG.arp.entry[i].ip;

		TRACE_CONFIG("IP addr: %u.%u.%u.%u, "
				"dst_hwaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
				da[0], da[1], da[2], da[3],
				CONFIG.arp.entry[i].haddr[0],
				CONFIG.arp.entry[i].haddr[1],
				CONFIG.arp.entry[i].haddr[2],
				CONFIG.arp.entry[i].haddr[3],
				CONFIG.arp.entry[i].haddr[4],
				CONFIG.arp.entry[i].haddr[5]);
	}
	if (CONFIG.arp.entries == 0)
		TRACE_CONFIG("(blank)\n");

	TRACE_CONFIG("----------------------------------------------------------"
			"-----------------------\n");
}
/*----------------------------------------------------------------------------*/
void 
DumpARPPacket(mtcp_manager_t mtcp, struct arphdr *arph)
{
	uint8_t *t;

	thread_printf(mtcp, mtcp->log_fp, "ARP header: \n");
	thread_printf(mtcp, mtcp->log_fp, "Hardware type: %d (len: %d), "
		      "protocol type: %d (len: %d), opcode: %d\n", 
		      ntohs(arph->ar_hrd), arph->ar_hln, 
		      ntohs(arph->ar_pro), arph->ar_pln, ntohs(arph->ar_op));
	t = (uint8_t *)&arph->ar_sip;
	thread_printf(mtcp, mtcp->log_fp, "Sender IP: %u.%u.%u.%u, "
		      "haddr: %02X:%02X:%02X:%02X:%02X:%02X\n", 
		      t[0], t[1], t[2], t[3], 
		      arph->ar_sha[0], arph->ar_sha[1], arph->ar_sha[2], 
		      arph->ar_sha[3], arph->ar_sha[4], arph->ar_sha[5]);
	t = (uint8_t *)&arph->ar_tip;
	thread_printf(mtcp, mtcp->log_fp, "Target IP: %u.%u.%u.%u, "
		      "haddr: %02X:%02X:%02X:%02X:%02X:%02X\n", 
		      t[0], t[1], t[2], t[3], 
		      arph->ar_tha[0], arph->ar_tha[1], arph->ar_tha[2], 
		      arph->ar_tha[3], arph->ar_tha[4], arph->ar_tha[5]);
}
/*----------------------------------------------------------------------------*/
