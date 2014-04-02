#include <stdio.h>
#include <string.h>
#include <stdint.h>
//#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

//static int num_queue = 4;

/*-------------------------------------------------------------*/ 
static void 
BuildKeyCache(uint32_t *cache, int cache_len)
{
#define NBBY 8 /* number of bits per byte */

	// 16bit test set
/*	
	static const uint8_t key[] = {
		 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00
	};
*/	
    /*
	static const uint8_t key[] = {
		 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
		 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
		 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
		 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a,
		 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a, 0x6d, 0x5a
	};
        */

	/*
	static const uint8_t key[] = {
		 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	*/

	// 32bit test set
	/*	
	static const uint8_t key[] = {
		 0x6d, 0x5a, 0x56, 0x25, 0x6d, 0x5a, 0x56, 0x25,
		 0x6d, 0x5a, 0x56, 0x25, 0x6d, 0x5a, 0x56, 0x25,
		 0x6d, 0x5a, 0x56, 0x25, 0x6d, 0x5a, 0x56, 0x25,
		 0x6d, 0x5a, 0x56, 0x25, 0x6d, 0x5a, 0x56, 0x25,
		 0x6d, 0x5a, 0x56, 0x25, 0x6d, 0x5a, 0x56, 0x25
	};
	*/
	/* ixgbe driver had a different set of keys than that of Microsoft
	   RSS keys */
/*	static const uint8_t key[] = {
		0x3D, 0xD7, 0x91, 0xE2,
		0x6C, 0xEC, 0x05, 0x18,
		0x0D, 0xB3, 0x94, 0x2A,
		0xEC, 0x2B, 0x4F, 0xA5,
		0x7C, 0xAF, 0x49, 0xEA,
		0x3D, 0xAD, 0x14, 0xE2,
		0xBE, 0xAA, 0x55, 0xB8,
		0xEA, 0x67, 0x3E, 0x6A,
		0x17, 0x4D, 0x36, 0x14,
		0x0D, 0x20, 0xED, 0x3B};
*/

#if 0
	/* Microsoft RSS keys */
	static const uint8_t key[] = {
		 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
		 0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
		 0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
		 0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
		 0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
	};
#endif
        /* Keys for system testing */
	static const uint8_t key[] = {
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
		 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
	};

	uint32_t result = (((uint32_t)key[0]) << 24) | (((uint32_t)key[1]) << 16) | 		              (((uint32_t)key[2]) << 8)  | ((uint32_t)key[3]);
	uint32_t idx = 32;
	int i;

	for (i = 0; i < cache_len; i++, idx++) {
		uint8_t shift = (idx % NBBY);
		uint32_t bit;

		cache[i] = result;
		bit = ((key[idx/NBBY] << shift) & 0x80) ? 1 : 0;
		result = ((result << 1) | bit);
	}

}
/*-------------------------------------------------------------*/ 
static uint32_t 
GetRSSHash(in_addr_t sip, in_addr_t dip, in_port_t sp, in_port_t dp)
{
#define MSB32 0x80000000
#define MSB16 0x8000
#define KEY_CACHE_LEN 96

	uint32_t res = 0;
	int i;
	static int first = 1;
	static uint32_t key_cache[KEY_CACHE_LEN] = {0};
	
	if (first) {
		BuildKeyCache(key_cache, KEY_CACHE_LEN);
		first = 0;
	}

	for (i = 0; i < 32; i++) {
		if (sip & MSB32)
			res ^= key_cache[i];
		sip <<= 1;
	}
	for (i = 0; i < 32; i++) {
		if (dip & MSB32)
			res ^= key_cache[32+i];
		dip <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (sp & MSB16)
			res ^= key_cache[64+i];
		sp <<= 1;
	}
	for (i = 0; i < 16; i++) {
		if (dp & MSB16)
			res ^= key_cache[80+i];
		dp <<= 1;
	}
	return res;
}
/*-------------------------------------------------------------------*/ 
/* RSS redirection table is in the little endian byte order (intel)  */
/*                                                                   */
/* idx: 0 1 2 3 | 4 5 6 7 | 8 9 10 11 | 12 13 14 15 | 16 17 18 19 ...*/
/* val: 3 2 1 0 | 7 6 5 4 | 11 10 9 8 | 15 14 13 12 | 19 18 17 16 ...*/
/* qid = val % num_queues */
/*-------------------------------------------------------------------*/ 
int
GetRSSCPUCore(in_addr_t sip, in_addr_t dip, 
			  in_port_t sp, in_port_t dp, int num_queues)
{
	#define RSS_BIT_MASK 0x0000007F

	static const uint32_t off[4] = {3, 1, -1, -3};
	uint32_t masked = GetRSSHash(sip, dip, sp, dp) & RSS_BIT_MASK;

	masked += off[masked & 0x3];
	return (masked % num_queues);

}
#if _TEST_RSS_
/*-------------------------------------------------------------*/ 
static void
VerifyRSSHash(void)
{
	 in_addr_t faddr, laddr;
	 in_port_t fport, lport;
	 char *src[]  = {"66.9.149.187", 
					 "199.92.111.2", 
					 "24.19.198.95", 
					 "38.27.205.30", 
					 "153.39.163.191"};
	 char *dest[] = {"161.142.100.80", 
					 "65.69.140.83",
					 "12.22.207.184", 
					 "209.142.163.6", 
					 "202.188.127.2"};
	 in_port_t src_port[]  = {2794, 14230, 12898, 48228, 44251};
	 in_port_t dest_port[] = {1766, 4739, 38024, 2217, 1303};
	 uint32_t correct_hash[] = {0x51ccc178, 
				    0xc626b0ea, 
				    0x5c2b394a, 
				    0xafc7327f,
				    0x10e828a2};
	 int i;

	 /*
	  * RSS hash calculation verification example is from
	  * http://msdn.microsoft.com/en-us/library/ff571021%28v=vs.85%29.aspx
	  */
	 
	 for (i = 0; i < 5; i++) {
		 struct in_addr addr;
		 
		 if (inet_aton(src[i], &addr) == 0) {
			 fprintf(stderr, "inet_aton error\n");
			 exit(-1);
		 }
		 faddr = ntohl(addr.s_addr);

		 if (inet_aton(dest[i], &addr) == 0) {
			 fprintf(stderr, "inet_aton error\n");
			 exit(-1);
		 }
		 laddr = ntohl(addr.s_addr);
		 
		 fport = src_port[i];
		 lport = dest_port[i];

		 printf("(%15s %15s %5d %5d)  0x%08x, correct_hash: 0x%08x\n",
				src[i], dest[i], src_port[i], dest_port[i],
				GetRSSHash(faddr, laddr, fport, lport), correct_hash[i]);
	 }
}

static unsigned long next = 2192123;
unsigned int myrand(void){
	next = next * 1103515245 + 12345;
	return next/65536;
}

static void
CheckRSSHash(int cnt, const char* src_ip, const char* dest_ip, int32_t src_port, int32_t dest_port)
{

	 struct in_addr saddr, daddr;
	 char saddr_str[15], daddr_str[15];
	 in_port_t sport, dport;
	 long queue_cnt[num_queue];
	 int queue_idx;


	 int i;
	 for( i = 0; i < num_queue; i++)
	 {
		 queue_cnt[i] = 0;
	 }

	 printf("src\tdest\tqueue_idx\n");
	
	 for( i =0; i < cnt; i++){
		// Only generate src/dest address when no address specified
                if (src_ip == NULL) {
                    saddr.s_addr = (in_addr_t) myrand();
                } else {
                    if (inet_aton(src_ip, &saddr) == 0) saddr.s_addr = (in_addr_t) myrand();
                }
                if (dest_ip == NULL) {
                    daddr.s_addr = (in_addr_t) myrand();
                } else {
                    if (inet_aton(dest_ip, &daddr) == 0) daddr.s_addr = (in_addr_t) myrand();
                }
		
		int32_t ports = (int32_t) myrand();
                if (src_port > 0) {
                    sport = htons(src_port);
                } else {
                    sport = ports;
                }
                if (dest_port > 0) {
                    dport = htons(dest_port);
                } else {
                    dport = ports >> 16;
                }
		
		//get rss hash
		queue_idx = GetRSSCPUCore(saddr.s_addr, daddr.s_addr, sport, dport, num_queue);
		//create logs
		strncpy(saddr_str, inet_ntoa(saddr), 15);
		strncpy(daddr_str, inet_ntoa(daddr), 15);
		printf("%15s:%5d\t%15s:%5d\t%d\n", saddr_str, ntohs(sport), daddr_str, ntohs(dport), queue_idx);
		
		queue_idx = GetRSSCPUCore(daddr.s_addr, saddr.s_addr, dport, sport, num_queue);
		printf("%15s:%5d\t%15s:%5d\t%d\n", daddr_str, ntohs(dport), saddr_str, ntohs(sport), queue_idx);

		queue_cnt[queue_idx]++;
	 }

	 printf("\n-----summary-----\n");
	 for( i = 0; i < num_queue; i++)
	 {
		 printf("%ld\n", queue_cnt[i]);
	 }
	 printf("\n");

}

void 
print_usage(int argc, char** argv){
	printf("Usage: %s -c number_of_test_ip [options]\n", argv[0]);
        printf("Options:\n");
        printf(" -r seed(long) : Specifiy random seed\n");
        printf(" -s srcIP : Source IP address\n");
        printf(" -S srcPort : Source port number\n");
        printf(" -d destIP : Destination IP address\n");
        printf(" -D destPort : Destination port number\n");
        printf("Any of source/destination IP/port could be omitted.\n");
        printf("If absent, random generated address is used.\n");
}

/*-------------------------------------------------------------*/
int 
main(int argc, char** argv)
{
        int opt;
        int cnt = -1;
        long seed = -1;
        int32_t sport = -1, dport = -1;
        char *srcIP = 0, *destIP = 0;
        while ((opt=getopt(argc, argv, "c:r:sS:dD:")) != -1) {
            switch (opt) {
                case 'c':
                    cnt = atoi(optarg);
                    break;
                case 'r':
                    seed = atol(optarg);
                    break;
                case 's':
                    srcIP = strdup(argv[optind]);
                    break;
                case 'S':
                    sport = atoi(optarg);
                    break;
                case 'd':
                    destIP = strdup(argv[optind]);
                    break;
                case 'D':
                    dport = atoi(optarg);
                    break;
            }
        }

        if (cnt < 1) {
		print_usage(argc, argv);
		exit(0);
	}
        if (seed > 0) next = seed;

        // Test configuration:
        // 10.10.4.11:X (rock5) -> 10.10.2.10:80 (rock4)
	CheckRSSHash(cnt, srcIP, destIP, sport, dport);
	
        if (srcIP) free(srcIP);
        if (destIP) free(destIP);
	return 0;
}
#endif
