#ifndef __DPDK_IFACE_COMMON_H__
#define __DPDK_IFACE_COMMON_H__
/*--------------------------------------------------------------------------*/
/* for ETH_ALEN */
#include <linux/if.h>
/*--------------------------------------------------------------------------*/
/* major number */
#define MAJOR_NO		511 //1110
/* dev name */
#define DEV_NAME		"dpdk-iface"
#define DEV_PATH		"/dev/"DEV_NAME
/* ioctl# */
#define SEND_STATS		 0
#define CREATE_IFACE		 1
#define CLEAR_IFACE		 4
#define FETCH_PCI_ADDRESS	 5
/* max qid */
#define MAX_QID			128
#ifndef MAX_DEVICES
#define MAX_DEVICES		128
#endif
#define PCI_DOM			"%04hX"
#define PCI_BUS			"%02hhX"
#define PCI_DEVICE		"%02hhX"
#define PCI_FUNC		"%01hhX"
#define PCI_LENGTH		13
/*--------------------------------------------------------------------------*/
typedef struct PciAddress {
	uint16_t domain;
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} PciAddress;
/*--------------------------------------------------------------------------*/
typedef struct PciDevice {
	union {
		uint8_t *ports_eth_addr;
		char ifname[IFNAMSIZ];
	};
	PciAddress pa;
} PciDevice __attribute__((aligned(64)));
/*--------------------------------------------------------------------------*/
#endif /* __DPDK_IFACE_COMMON_H__ */
