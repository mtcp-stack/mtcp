#ifndef __IO_MODULE_H__
#define __IO_MODULE_H__
/*----------------------------------------------------------------------------*/
/* for type def'ns */
#include <stdint.h>
/* for ps lib funcs */
#include "ps.h"
/*----------------------------------------------------------------------------*/
/**
 * Declaration to soothe down the warnings 
 */
struct mtcp_thread_context;
/**
 * io_module_funcs - contains template for the various 10Gbps pkt I/O
 *                 - libraries that can be adopted.
 *
 *		   load_module()    : Used to set system-wide I/O module 
 *				      initialization.
 *
 *                 init_handle()    : Used to initialize the driver library
 *                                  : Also use the context to create/initialize
 *                                  : a private packet I/O data structures.
 *
 *                 link_devices()   : Used to add link(s) to the mtcp stack.
 *				      Returns 0 on success; -1 on failure.
 *
 *		   release_pkt()    : release the packet if mTCP does not need
 *				      to process it (e.g. non-IPv4, non-TCP pkts).
 *
 *		   get_wptr()	    : retrieve the next empty pkt buffer for the 
 * 				      application for packet writing. Returns
 *				      ptr to pkt buffer.
 *
 *		   send_pkts()	    : transmit batch of packets via interface 
 * 				      idx (=nif). 
 *				      Returns 0 on success; -1 on failure
 *
 *		   get_rptr()	    : retrieve next pkt for application for
 *				      packet read.
 *				      Returns ptr to pkt buffer.
 *			       
 *		   recv_pkts()	    : recieve batch of packets from the interface, 
 *				      ifidx.
 *				      Returns no. of packets that are read from
 *				      the iface.
 *
 *		   select()	    : for blocking I/O
 *
 *		   destroy_handle() : free up resources allocated during 
 * 				      init_handle(). Normally called during 
 *				      process termination.
 *
 *                 dev_ioctl()      : contains submodules for select drivers
 *		   
 */
typedef struct io_module_func {
	void	  (*load_module)(void);
	void      (*init_handle)(struct mtcp_thread_context *ctx);
	int32_t   (*link_devices)(struct mtcp_thread_context *ctx);
	void      (*release_pkt)(struct mtcp_thread_context *ctx, int ifidx, unsigned char *pkt_data, int len);
	uint8_t * (*get_wptr)(struct mtcp_thread_context *ctx, int ifidx, uint16_t len);
	int32_t   (*send_pkts)(struct mtcp_thread_context *ctx, int nif);
	uint8_t * (*get_rptr)(struct mtcp_thread_context *ctx, int ifidx, int index, uint16_t *len);
	int32_t   (*recv_pkts)(struct mtcp_thread_context *ctx, int ifidx);
	int32_t	  (*select)(struct mtcp_thread_context *ctx);
	void	  (*destroy_handle)(struct mtcp_thread_context *ctx);
	int32_t	  (*dev_ioctl)(struct mtcp_thread_context *ctx, int nif, int cmd, void *argp);
} io_module_func __attribute__((aligned(__WORDSIZE)));
/*----------------------------------------------------------------------------*/
/* set I/O module context */
int SetInterfaceInfo(char *);

/* retrive device-specific endian type */
int FetchEndianType();
/*----------------------------------------------------------------------------*/
/* ptr to the `running' I/O module context */
extern io_module_func *current_iomodule_func;

/* dev_ioctl related macros */
#define PKT_TX_IP_CSUM          0x01
#define PKT_TX_TCP_CSUM         0x02
#define PKT_RX_TCP_LROSEG	0x03
#define PKT_TX_TCPIP_CSUM	0x04
#define PKT_RX_IP_CSUM		0x05
#define PKT_RX_TCP_CSUM		0x06
#define PKT_TX_TCPIP_CSUM_PEEK	0x07
#define DRV_NAME		0x08

/* registered psio context */
#ifdef DISABLE_PSIO
#define ps_list_devices(x) 		0
#endif
extern io_module_func ps_module_func;
extern struct ps_device devices[MAX_DEVICES];

/* registered dpdk context */
extern io_module_func dpdk_module_func;

/* registered netmap context */
extern io_module_func netmap_module_func;

/* Macro to assign IO module */
#define AssignIOModule(m) {						\
		if (!strcmp(m, "psio"))					\
			current_iomodule_func = &ps_module_func;	\
		else if (!strcmp(m, "dpdk"))				\
			current_iomodule_func = &dpdk_module_func;	\
		else if (!strcmp(m, "netmap"))				\
			current_iomodule_func = &netmap_module_func;	\
		else							\
			assert(0);					\
	}
/*----------------------------------------------------------------------------*/
#endif /* !__IO_MODULE_H__ */
