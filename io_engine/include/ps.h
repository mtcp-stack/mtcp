#ifndef _PS_H_
#define _PS_H_

#define MAX_DEVICES	16
#define MAX_RINGS	64

/* IN: option for ps_wait(); */
#define PS_CTL_IN 0x1		/* The associated queue is available to read */
#define PS_CTL_OUT 0x2 	/* The associated queue is available to write */
/* The associated queue is available to write or read */
#define PS_CTL_INOUT (PS_CTL_IN | PS_CTL_OUT)	

/* OUT: return values for ps_wait() */
#define PS_SEND_AVAILABLE 0x1		/* The associated queue is available to read */
#define PS_RECEIVE_AVAILABLE 0x2 	/* The associated queue is available to write */
/* The associated queue is available to read and write */
#define PS_ALL_AVAILABLE (PS_SEND_AVAILABLE | PS_RECEIVE_AVAILABLE) 	

#define PS_SEND_MIN 256

#ifdef __KERNEL__

#define PS_MAJOR 1010
#define PS_NAME "packet_shader"

#define MAX_BUFS (12*4)

struct ____cacheline_aligned ps_context {
	struct semaphore sem;

	wait_queue_head_t wq;

	int num_attached;
	struct ixgbe_ring *rx_rings[MAX_RINGS];
	int next_ring;

	struct ps_pkt_info *info;
	/* char *buf; */

	int num_bufs;
	int buf_refcnt[MAX_BUFS];
	char *kbufs[MAX_BUFS];
	char __user *ubufs[MAX_BUFS];
};

#else	/* __KERNEL__ */

#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <linux/types.h>

#define __user

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))

#if defined(__i386__) || defined(__x86_64__)
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	asm("  movl (%1), %0\n"
	    "  subl $4, %2\n"
	    "  jbe 2f\n"
	    "  addl 4(%1), %0\n"
	    "  adcl 8(%1), %0\n"
	    "  adcl 12(%1), %0\n"
	    "1: adcl 16(%1), %0\n"
	    "  lea 4(%1), %1\n"
	    "  decl %2\n"
	    "  jne      1b\n"
	    "  adcl $0, %0\n"
	    "  movl %0, %2\n"
	    "  shrl $16, %0\n"
	    "  addw %w2, %w0\n"
	    "  adcl $0, %0\n"
	    "  notl %0\n"
	    "2:"
	    /* Since the input registers which are loaded with iph and ih
	       are modified, we must also specify them as outputs, or gcc
	       will assume they contain their original values. */
	    : "=r" (sum), "=r" (iph), "=r" (ihl)
	    : "1" (iph), "2" (ihl)
	       : "memory");
	return (__sum16)sum;
}
#else
#define __force
typedef unsigned int u32;

static inline __sum16 csum_fold(__wsum csum)
{
	u32 sum = (__force u32)csum;;

	sum += (sum << 16);
	csum = (sum < csum);
	sum >>= 16;
	sum += csum;

	return (__force __sum16)~sum;
}

/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 *
 *	By Jorge Cwik <jorge@laser.satlink.net>, adapted for linux by
 *	Arnt Gulbrandsen.
 */
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	const unsigned int *word = iph;
	const unsigned int *stop = word + ihl;
	unsigned int csum;
	int carry;

	csum = word[0];
	csum += word[1];
	carry = (csum < word[1]);
	csum += carry;

	csum += word[2];
	carry = (csum < word[2]);
	csum += carry;

	csum += word[3];
	carry = (csum < word[3]);
	csum += carry;

	word += 4;
	do {
		csum += *word;
		carry = (csum < *word);
		csum += carry;
		word++;
	} while (word != stop);

	return csum_fold(csum);
}
#endif

#endif	/* __KERNEL__ */

struct ps_device {
	char name[IFNAMSIZ];
	char dev_addr[ETH_ALEN];
	uint32_t ip_addr;	/* network order */

	/* NOTE: this is different from kernel's internal index */
	int ifindex;

	/* This is kernel's ifindex. */
	int kifindex;

	int num_rx_queues;
	int num_tx_queues;
};

struct ps_queue {
	int ifindex;
	int qidx;
};

#define MAX_PACKET_SIZE	2048
#define MAX_CHUNK_SIZE	4096
#define ENTRY_CNT	4096

#define PS_CHECKSUM_RX_UNKNOWN 	0
#define PS_CHECKSUM_RX_GOOD	1
#define PS_CHECKSUM_RX_BAD 	2

struct ps_pkt_info {
	uint32_t offset;
	uint16_t len;
	uint8_t checksum_rx;
};

struct ps_chunk {
	/* number of packets to send/recv */
	int cnt;
	int recv_blocking;

	/* 
	   for RX: output (where did these packets come from?)
	   for TX: input (which interface do you want to xmit?)
	 */
	struct ps_queue queue;

	struct ps_pkt_info __user *info;
	char __user *buf;
};

struct ps_chunk_buf {

	uint16_t cnt;
	uint16_t next_to_use;
	uint16_t next_to_send;
	uint32_t next_offset;

	struct ps_queue queue;
	void __user *lock;
	struct ps_pkt_info __user *info;
	char __user *buf;
};

struct ps_packet {
	int ifindex;
	int len;
	char __user *buf;
};

#define NID_ZERO(isp)    (isp = 0)
#define NID_SET(id, isp) (isp |= 1 << id)
#define NID_CLR(id, isp) (isp &= ~(1 << id))
#define NID_ISSET(id, isp)   (isp & (1 << id))

// maximum number of interface descriptor is 16
typedef uint16_t nids_set;
struct ps_event {
	long timeout;	
	int qidx;

	nids_set rx_nids;
	nids_set tx_nids;
};

static inline void prefetcht0(void *p)
{
	asm volatile("prefetcht0 (%0)\n\t"
			: 
			: "r" (p)
		    );
}

static inline void prefetchnta(void *p)
{
	asm volatile("prefetchnta (%0)\n\t"
			: 
			: "r" (p)
		    );
}

static inline void memcpy_aligned(void *to, const void *from, size_t len)
{
	if (len <= 64) {
		memcpy(to, from, 64);
	} else if (len <= 128) {
		memcpy(to, from, 64);
		memcpy((uint8_t *)to + 64, (uint8_t *)from + 64, 64);
	} else {
		size_t offset;

		for (offset = 0; offset < len; offset += 64)
			memcpy((uint8_t *)to + offset, 
					(uint8_t *)from + offset, 
					64);
	}
}

#define PS_IOC_LIST_DEVICES 	0
#define PS_IOC_ATTACH_RX_DEVICE	1
#define PS_IOC_DETACH_RX_DEVICE	2
#define PS_IOC_RECV_CHUNK	3
#define PS_IOC_SEND_CHUNK	4
#define PS_IOC_SLOWPATH_PACKET	5
#define PS_IOC_RECV_CHUNK_IFIDX	6
#define PS_IOC_SEND_CHUNK_BUF	7
#define PS_IOC_GET_TXENTRY	8
#define PS_IOC_SELECT	9

#ifndef __KERNEL__

struct ps_handle {
	int fd;

	uint64_t rx_chunks[MAX_DEVICES];
	uint64_t rx_packets[MAX_DEVICES];
	uint64_t rx_bytes[MAX_DEVICES];

	uint64_t tx_chunks[MAX_DEVICES];
	uint64_t tx_packets[MAX_DEVICES];
	uint64_t tx_bytes[MAX_DEVICES];

	void *priv;
};

int ps_list_devices(struct ps_device *devices);
int ps_init_handle(struct ps_handle *handle);
void ps_close_handle(struct ps_handle *handle);
int ps_attach_rx_device(struct ps_handle *handle, struct ps_queue *queue);
int ps_detach_rx_device(struct ps_handle *handle, struct ps_queue *queue);
int ps_alloc_chunk(struct ps_handle *handle, struct ps_chunk *chunk);
void ps_free_chunk(struct ps_chunk *chunk);
int ps_alloc_chunk_buf(struct ps_handle *handle, 
		int ifidx, int qidx, struct ps_chunk_buf *c_buf);
void ps_free_chunk_buf(struct ps_chunk_buf *c_buf);
char* ps_assign_chunk_buf(struct ps_chunk_buf *c_buf, int len);
int ps_recv_chunk(struct ps_handle *handle, struct ps_chunk *chunk);
int ps_recv_chunk_ifidx(struct ps_handle *handle, struct ps_chunk *chunk, int ifidx);
int ps_send_chunk(struct ps_handle *handle, struct ps_chunk *chunk);
int ps_send_chunk_buf(struct ps_handle *handle, struct ps_chunk_buf *chunk);
int ps_select(struct ps_handle *handle, struct ps_event * event);
int ps_get_txentry(struct ps_handle *handle, struct ps_queue * queue);
int ps_slowpath_packet(struct ps_handle *handle, struct ps_packet *packet);

void dump_packet(char *buf, int len);
void dump_chunk(struct ps_chunk *chunk);

int get_num_cpus();
int bind_cpu(int cpu);
uint64_t rdtsc();

#endif

#endif	/* _PS_H_ */
