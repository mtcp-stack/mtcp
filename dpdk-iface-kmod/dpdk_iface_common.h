#ifndef __DPDK_IFACE_COMMON_H__
#define __DPDK_IFACE_COMMON_H__
/*--------------------------------------------------------------------------*/
/* major number */
#define MAJOR_NO		1110
/* dev name */
#define DEV_NAME		"dpdk-iface"
#define DEV_PATH		"/dev/"DEV_NAME
/* ioctl# */
#define SEND_STATS		 0
#define CREATE_IFACE		 1
#define CLEAR_IFACE		 4
/* max qid */
#define MAX_QID			128
#define MAX_DEVICES		128
/*--------------------------------------------------------------------------*/
#endif /* __DPDK_IFACE_COMMON_H__ */
