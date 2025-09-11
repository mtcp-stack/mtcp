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
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include "debug.h"
#include "io_module.h"
#include "mtcp.h"
#include "config.h"

#define CHUNK_SIZE 64
#define BUF_SIZE (64 * 1024 * 1024)
#define SNAPLEN 65535
#define TIMEOUT_MS 1
#define MAX_TX_BURST 1024
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
} __attribute__((aligned(__WORDSIZE)));

/*----------------------------------------------------------------------------*/
void pcap_init_handle(struct mtcp_thread_context *ctxt) //,int designated_port)
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

    pcap_t *temp = pcap_create(CONFIG.eths[0].dev_name, errbuf);
    if (!temp)
    {
        TRACE_ERROR("pcap_create failed: %s\n", errbuf);
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
    pcap_setnonblock(temp, 1, errbuf);

    if (pcap_activate(temp) < 0)
    {
        TRACE_ERROR("pcap_activate failed: %s\n", pcap_geterr(temp));
        exit(EXIT_FAILURE);
    }

    ppc->handle = temp;
}
/*----------------------------------------------------------------------------*/
void pcap_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
    /**************************************************************** */
    // free received packets here all after processing packets
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;
    for (int k = 0; k < ppc->num_pkts_in_chunk; k++)
    {
        if (ppc->hdrs[k])
        {
            free(ppc->hdrs[k]);
            ppc->hdrs[k] = NULL;
        }
        if (ppc->pkts[k])
        {
            free((void *)ppc->pkts[k]);
            ppc->pkts[k] = NULL;
        }
    }
    ppc->num_pkts_in_chunk = 0;

    /**************************************************************** */
    return;
}
/*----------------------------------------------------------------------------*/
int32_t
pcap_send_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
    TRACE_ERROR("ENTER PCAP_SEND_PKTS\n");
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;
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
    TRACE_ERROR("[pcap_send_pkts] flushing %d packets\n", ppc->w_buf.cnt);

    // Check pcap statistics after flushing
    struct pcap_stat ps;
    if (pcap_stats(ppc->handle, &ps) == 0)
    {
        TRACE_ERROR("[pcap_stats] recv=%u drop=%u ifdrop=%u\n",
                    ps.ps_recv, ps.ps_drop, ps.ps_ifdrop);
    }
    else
    {
        TRACE_ERROR("[pcap_stats] failed: %s\n", pcap_geterr(ppc->handle));
    }

    ppc->w_buf.cnt = 0; // reset buffer
    return sent;
}
/*----------------------------------------------------------------------------*/

uint8_t *
pcap_get_wptr(struct mtcp_thread_context *ctxt, int ifidx, uint16_t len) // used when mtcp wants to transmit packet. get a buffer for outgoing packet
{
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;

    if (ppc->w_buf.cnt >0)
    {
        pcap_send_pkts(ctxt, ifidx);
    }

    // allocate buffer (heap )
    uint8_t *buf = malloc(len); //!! neeed free logic in the code!!
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
    if(!ppc->hdrs[index])
        *len = ppc->hdrs[index]->caplen;
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
    struct iphdr *iph = (struct iphdr *)(bytes + sizeof(struct ether_header));
    struct tcphdr *tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ihl << 2));
    if (tcph->ack)
    {
        printf("Captured ACK: ack_seq=%u win=%u\n",
               ntohl(tcph->ack_seq), ntohs(tcph->window));
    }
    /************************************************************ */
    if (ppc->num_pkts_in_chunk < CHUNK_SIZE)
    { // safety guard
        // deep copy pkthdr and buf
        struct pcap_pkthdr *hdr = (struct pcap_pkthdr *)malloc(sizeof(struct pcap_pkthdr));
        memcpy(hdr, h, sizeof(struct pcap_pkthdr));
        ppc->hdrs[ppc->num_pkts_in_chunk] = hdr;
        u_char *bts = malloc(hdr->caplen);
        memcpy(bts, bytes, hdr->caplen);
        ppc->pkts[ppc->num_pkts_in_chunk] = bts;
        ppc->num_pkts_in_chunk++;
    }
}
/*----------------------------------------------------------------------------*/
int32_t
pcap_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
    TRACE_ERROR("ENTER PCAP_RECV_PKTS\n");
    struct pcap_private_context *ppc = ctxt->io_private_context;
    printf("pcap_rev_pkts: num_pkts_in_chunk=%d\n", ppc->num_pkts_in_chunk);

    // for (int i = 0; i < ppc->num_pkts_in_chunk; i++)
    // {
    //     if(!ppc->hdrs[i])
    //         printf("hdrs[%d] is NULL\n",i);
    //     if(!ppc->pkts[i])
    //         printf("pkts[%d] is NULL\n",i);
    //     printf("  pkt %d: len=%u caplen=%u\n", i,ppc->hdrs[i]->caplen);
    // }
    ppc->num_pkts_in_chunk = 0;
    int ret = pcap_dispatch(ppc->handle, CHUNK_SIZE, pcap_packet_handler, (u_char *)ppc);
    TRACE_ERROR("pcap dispatch returned %d\n\n", ret);
    // receive up to max CHUNK_SIZE packets
    // packets in socket's receive buffers are copied to user application buffer
    if (ret < 0) // error
    {
        TRACE_ERROR("pcap_dispatch failed: %s\n", pcap_geterr(ppc->handle));
        return -1;
    }
    if (ret == 0)
    {
        printf("timeout expired on live caputure\n");
    }
    TRACE_ERROR("QUITTING PCAP_RECV_PKTS\n");
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