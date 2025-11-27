#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include "debug.h"
#include "io_module.h"
#include "mtcp.h"
#include "config.h"

#define CHUNK_SIZE 262144
#define BUF_SIZE (128 * 1024 * 1024)
#define SNAPLEN 65535
#define TIMEOUT_MS 50
#define MAX_TX_BURST 128

// assume single core (x need to RSS packets to multiple cores)
// x concept of core affinity
// x concept of specific NIC queue
// think of just one NIC interface(here may use virtual ethernet interface)

// struct pcap_pkthdr defintion
// --> struct timeval ts(time info when the packet was captured), uint32 len(length of packet), uint32 caplen(length of portion present)
/*----------------------------------------------------------------------------*/

struct tx_pkt
{
    uint8_t *buf;
    int len;
};

struct tx_buf
{
    struct tx_pkt pkts[MAX_TX_BURST];
    int cnt;
};

struct pcap_private_context
{
    pcap_t *handle;
    struct tx_buf w_buf;
    struct pcap_pkthdr *hdrs[CHUNK_SIZE]; // this contains metatdata for each packet captured
    // metadata includes time it was captured and total length of this packet
    const u_char *pkts[CHUNK_SIZE];
    int num_pkts_in_chunk;

    int tx_sock;
    struct sockaddr_ll tx_addr;

} __attribute__((aligned(__WORDSIZE)));

/*----------------------------------------------------------------------------*/
// improving tx functionality of pcap

// init_tx_sock
// creates an AF_PACKET raw socket
// lookup the interface index for a given interface name
// fill a sockaddr_ll with the interface index and protocol
// bind the socket to that NIC interface + Protocol
/*----------------------------------------------------------------------------*/
static int init_tx_sock(struct mtcp_thread_context *ctxt)
{ // for stream 11 -> may be ens2f1np1
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;
    int s;
    if ((s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) // send/receive raw socket including L2 header
    {
        perror("tx_sock initialziation failed");
        return -1;
    }
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, CONFIG.eths[0].dev_name);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("tx socekt ioctl failed");
        close(s);
        return -1;
    }

    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_IP);

    if (bind(s, (struct sockaddr *)&sll, sizeof(sll)) < 0)
    {
        perror("tx_sock bind error");
        close(s);
        return -1;
    } // attaches socket to specific interface and specifc EtherType

    ppc->tx_addr = sll;
    ppc->tx_sock = s;

    return 0;
}

/*----------------------------------------------------------------------------*/
void pcap_init_handle(struct mtcp_thread_context *ctxt)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    struct pcap_private_context *ppc;

    ctxt->io_private_context = calloc(1, sizeof(struct pcap_private_context));
    if (!ctxt->io_private_context)
    {
        TRACE_ERROR("Failed to allocate pcap_private_context\n");
        exit(EXIT_FAILURE);
    }
    ppc = (struct pcap_private_context *)ctxt->io_private_context;
    printf("Using pcap interface: [%s]\n", CONFIG.eths[0].dev_name);

    pcap_t *temp = pcap_create(CONFIG.eths[0].dev_name, errbuf);
    if (!temp)
    {
        TRACE_ERROR("pcap_create failed: %s\n", errbuf);
        exit(EXIT_FAILURE);
    }
    // make tx_sock
    if (init_tx_sock(ctxt) < 0)
    {
        perror("error initialzing tx_sock ");
        exit(EXIT_FAILURE);
    }

    if (pcap_set_snaplen(temp, SNAPLEN) != 0)
        TRACE_ERROR("pcap_set_snaplen failed\n");
    if (pcap_set_promisc(temp, 1) != 0)
        TRACE_ERROR("pcap_set_promisc failed\n");
    if (pcap_set_timeout(temp, TIMEOUT_MS) != 0)
        TRACE_ERROR("pcap_set_timeout failed\n");
    if (pcap_set_buffer_size(temp, BUF_SIZE) != 0)
        TRACE_ERROR("pcap_set_buffer_size failed\n");
    if (pcap_setnonblock(temp, 1, errbuf) < 0)
        TRACE_ERROR("pcap_setnonblock failed: %s\n", errbuf);

    // pcap_set_immediate_mode(temp, 1); // set immediate mode

    if (pcap_activate(temp) < 0)
    {
        TRACE_ERROR("pcap_activate failed: %s\n", pcap_geterr(temp));
        exit(EXIT_FAILURE);
    }
    ppc->handle = temp;
    struct bpf_program fp;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, CONFIG.eths[0].dev_name);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);

    unsigned char *m = (unsigned char *)ifr.ifr_hwaddr.sa_data;

    char filter[128];
    sprintf(filter,
            "not ether src %02x:%02x:%02x:%02x:%02x:%02x",
            m[0], m[1], m[2], m[3], m[4], m[5]);
    pcap_compile(ppc->handle, &fp, filter, 0, PCAP_NETMASK_UNKNOWN);
    pcap_setfilter(ppc->handle, &fp);
}
/*----------------------------------------------------------------------------*/
void pcap_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;

    ppc->num_pkts_in_chunk = 0;
    return;
}
/*----------------------------------------------------------------------------*/
static int raw_send_batch(struct mtcp_thread_context *ctxt)
{
    struct pcap_private_context *ppc = (struct pcap_private_context *)ctxt->io_private_context;
    int cnt = ppc->w_buf.cnt;
    if (cnt == 0)
        return 0;

    struct mmsghdr msgs[cnt];
    struct iovec iov[cnt];

    memset(msgs, 0, sizeof(msgs));
    memset(iov, 0, sizeof(iov));

    for (int i = 0; i < cnt; i++)
    {
        iov[i].iov_base = ppc->w_buf.pkts[i].buf;
        iov[i].iov_len = ppc->w_buf.pkts[i].len;

        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_name = &ppc->tx_addr;
        msgs[i].msg_hdr.msg_namelen = sizeof(ppc->tx_addr);
    }

    int sent = sendmmsg(ppc->tx_sock, msgs, cnt, 0);
    if (sent < 0)
    {
        perror("sendmmsg error");
        sent = 0;
    }
    /* free only successfully sent buffers */
    for (int i = 0; i < sent; i++)
    {
        free(ppc->w_buf.pkts[i].buf);
    }

    for (int i = sent; i < cnt; i++)
    {
        free(ppc->w_buf.pkts[i].buf);
    }

    ppc->w_buf.cnt = 0;
    return sent;
}

/*----------------------------------------------------------------------------*/

int32_t
pcap_send_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
    // TRACE_ERROR("ENTER PCAP_SEND_PKTS\n");

    return raw_send_batch(ctxt);
    /*
        int sent = 0;

        for (int i = 0; i < ppc->w_buf.cnt; i++)
        {
            uint8_t *buf = ppc->w_buf.pkts[i].buf;
            int len = ppc->w_buf.pkts[i].len;
            if (pcap_inject(ppc->handle, buf, len) == len)
            {
                sent++;
            }
            else
            {
                TRACE_ERROR("pcap_inject failed: %s\n", pcap_geterr(ppc->handle));
            }
            free(buf);
        }
        // TRACE_ERROR("[pcap_send_pkts] flushing %d packets\n", ppc->w_buf.cnt);

        ppc->w_buf.cnt = 0; // reset buffer
        return sent;
    */
}
/*----------------------------------------------------------------------------*/

uint8_t *
pcap_get_wptr(struct mtcp_thread_context *ctxt, int ifidx, uint16_t len) // used when mtcp wants to transmit packet. get a buffer for outgoing packet
{
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;

    if (ppc->w_buf.cnt == MAX_TX_BURST) // if count reached maximum capacity flush it first, then allocate new buffer 
    {
        raw_send_batch(ctxt);
    }

    // allocate buffer (heap )
    uint8_t *buf = malloc(len);
    if (!buf)
        return NULL;

    ppc->w_buf.pkts[ppc->w_buf.cnt].buf = buf;
    ppc->w_buf.pkts[ppc->w_buf.cnt].len = len;
    ppc->w_buf.cnt++;

    return buf; // caller fills this buffer
}
/*----------------------------------------------------------------------------*/
uint8_t *
pcap_get_rptr(struct mtcp_thread_context *ctxt, int ifidx, int index, uint16_t *len)
{
    struct pcap_private_context *ppc;
    uint8_t *pktbuf;
    ppc = (struct pcap_private_context *)ctxt->io_private_context;
    pktbuf = (uint8_t *)ppc->pkts[index];
    if (ppc->hdrs[index])
        *len = ppc->hdrs[index]->caplen;
    else
        *len = 0;
    return pktbuf;
}
/*----------------------------------------------------------------------------*/
static void
pcap_packet_handler(u_char *user,
                    const struct pcap_pkthdr *h,
                    const u_char *bytes)
{
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)user;
    /************************************************************ */
    // For Debuggging purpose
    // struct iphdr *iph = (struct iphdr *)(bytes + sizeof(struct ether_header));
    // struct tcphdr *tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ihl << 2));

    /************************************************************ */
    // if (tcph->ack)
    // {
    //     printf("Captured ACK: ack_seq=%u win=%u\n",
    //            ntohl(tcph->ack_seq), ntohs(tcph->window));
    // }
    /************************************************************ */

    if (ppc->num_pkts_in_chunk < CHUNK_SIZE)
    {
        ppc->hdrs[ppc->num_pkts_in_chunk] = (struct pcap_pkthdr *)h;
        ppc->pkts[ppc->num_pkts_in_chunk] = bytes;
        ppc->num_pkts_in_chunk++;
    }
}
/*----------------------------------------------------------------------------*/
int32_t
pcap_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
    // TRACE_ERROR("ENTER PCAP_RECV_PKTS\n");
    struct pcap_private_context *ppc = ctxt->io_private_context;
    // printf("pcap_rev_pkts: num_pkts_in_chunk=%d\n", ppc->num_pkts_in_chunk);
    // ppc->num_pkts_in_chunk = 0;
    int ret = pcap_dispatch(ppc->handle, CHUNK_SIZE, pcap_packet_handler, (u_char *)ppc);
    // TRACE_ERROR("pcap dispatch returned %d\n\n", ret);
    // receive up to max CHUNK_SIZE packets
    // packets in socket's receive buffers are copied to user application buffer
    if (ret < 0) // error
    {
        TRACE_ERROR("pcap_dispatch failed: %s\n", pcap_geterr(ppc->handle));
        return -1;
    }
    // if (ret == 0)
    // {
    //     fprintf(stderr, "timeout expired on live caputure\n");
    // }
    //  TRACE_ERROR("QUITTING PCAP_RECV_PKTS\n");
    return ppc->num_pkts_in_chunk;
}
/*----------------------------------------------------------------------------*/
int32_t
pcap_select(struct mtcp_thread_context *ctxt) // useless
{
    return 0;
}
/*----------------------------------------------------------------------------*/
int pcap_link_devices(struct mtcp_thread_context *ctxt) // no cnocept of NIC RX queue affinity
{
    return 0;
}
/*----------------------------------------------------------------------------*/
void pcap_destroy_handle(struct mtcp_thread_context *ctxt)
{
    struct pcap_private_context *ppc = ctxt->io_private_context;
    if (ppc)
    {
        if (ppc->handle)
            pcap_close(ppc->handle);
        free(ppc);
    }
}
/*----------------------------------------------------------------------------*/
void pcap_load_module() // useless,, just a guard
{

    if (CONFIG.multi_process)
    {
        TRACE_LOG("pcap module does not support multi-process.\n");
        exit(EXIT_FAILURE);
    }
}
/*----------------------------------------------------------------------------*/
io_module_func pcap_module_func = {
    .load_module = pcap_load_module,
    .init_handle = pcap_init_handle,
    .link_devices = pcap_link_devices,
    .release_pkt = pcap_release_pkt,
    .send_pkts = pcap_send_pkts,
    .get_wptr = pcap_get_wptr,
    .recv_pkts = pcap_recv_pkts,
    .get_rptr = pcap_get_rptr,
    .select = pcap_select,
    .destroy_handle = pcap_destroy_handle,
    .dev_ioctl = NULL};
/*----------------------------------------------------------------------------*/