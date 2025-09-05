#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h> // htons/ntohs
#include <net/ethernet.h>
#include <errno.h>
#include <pcap.h>
#include "debug.h"
#include "io_module.h"
#include "mtcp.h"
#include "config.h"
#define CHUNK_SIZE 64

// assume single core (x need to RSS packets to multiple cores)
// x concept of core affinity
// x concept of specific NIC queue
// think of just one NIC interface(here may use virtual ethernet interface)

// struct pcap_pkthdr defintion
// --> struct timeval ts(time info when the packet was captured), uint32 len(length of packet), uint32 caplen(length of portion present)
/*----------------------------------------------------------------------------*/
#define MAX_TX_BURST 1024 // you can tune this

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

    // Open the device (assume CONFIG.eths[0].dev_name set in config)
    ppc->handle = pcap_open_live(CONFIG.eths[0].dev_name,
                                 65535, 1, 1, errbuf);

    // enlarge pcap buffer
    pcap_set_buffer_size(ppc->handle, 64 * 1024 * 1024);
    // promiscuous mode and timeout info may be modified

    if (!ppc->handle)
    {
        TRACE_ERROR("pcap_open_live failed: %s\n", errbuf);
        exit(EXIT_FAILURE);
    }
}
/*----------------------------------------------------------------------------*/
void pcap_release_pkt(struct mtcp_thread_context *ctxt, int ifidx, unsigned char *pkt_data, int len)
{
    return; // useless since I use filter that only allows TCP packet
}
/*----------------------------------------------------------------------------*/

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

        if (pcap_sendpacket(ppc->handle, buf, len) == 0)
        {
            sent++;
        }
        else
        {
            TRACE_ERROR("pcap_sendpacket failed: %s\n", pcap_geterr(ppc->handle));
        }
        free(buf); // free after sending
    }

    ppc->w_buf.cnt = 0; // reset buffer
    return sent;
}

uint8_t *
pcap_get_wptr(struct mtcp_thread_context *ctxt, int ifidx, uint16_t len) // used when mtcp wants to transmit packet. get a buffer for outgoing packet
{
    struct pcap_private_context *ppc =
        (struct pcap_private_context *)ctxt->io_private_context;

    if (ppc->w_buf.cnt >= MAX_TX_BURST)
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

    if (ppc->num_pkts_in_chunk < 64)
    { // safety guard
        ppc->hdrs[ppc->num_pkts_in_chunk] = (struct pcap_pkthdr *)h;
        ppc->pkts[ppc->num_pkts_in_chunk] = bytes;
        ppc->num_pkts_in_chunk++;
    }
}
/*----------------------------------------------------------------------------*/
int32_t
pcap_recv_pkts(struct mtcp_thread_context *ctxt, int ifidx)
{
    struct pcap_private_context *ppc = ctxt->io_private_context;
    // use pcap_dipatch();
    ppc->num_pkts_in_chunk = 0;
    int ret = pcap_dispatch(ppc->handle, CHUNK_SIZE, pcap_packet_handler, (u_char *)ppc);
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
    return ret;
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