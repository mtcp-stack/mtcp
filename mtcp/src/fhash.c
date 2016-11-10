#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>

#include "debug.h"
#include "fhash.h"

//#include "stdint.h" /* Replace with <stdint.h> if appropriate */
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
	|| defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
		+(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

inline uint32_t
SuperFastHash (const char * data, int len) {
	register uint32_t hash = len, tmp;
	int rem;

	if (len <= 0 || data == NULL) return 0;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (;len > 0; len--) {
		hash  += get16bits (data);
		tmp    = (get16bits (data+2) << 11) ^ hash;
		hash   = (hash << 16) ^ tmp;
		data  += 2*sizeof (uint16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */
	switch (rem) {
		case 3: hash += get16bits (data);
				hash ^= hash << 16;
				hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
				hash += hash >> 11;
				break;
		case 2: hash += get16bits (data);
				hash ^= hash << 11;
				hash += hash >> 17;
				break;
		case 1: hash += (signed char)*data;
				hash ^= hash << 10;
				hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}

/*----------------------------------------------------------------------------*/
inline unsigned int
HashFlow(const tcp_stream *flow)
{
#if 0
	register unsigned int hash, i;
	char *key = (char *)&flow->saddr;

	for (hash = i = 0; i < 12; ++i) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash & (NUM_BINS - 1);
#else
	return SuperFastHash((const char *)&flow->saddr, 12) & (NUM_BINS - 1);
#endif
}
/*---------------------------------------------------------------------------*/
#define EQUAL_FLOW(flow1, flow2) \
	(flow1->saddr == flow2->saddr && flow1->sport == flow2->sport && \
	 flow1->daddr == flow2->daddr && flow1->dport == flow2->dport)
/*---------------------------------------------------------------------------*/
struct hashtable * 
CreateHashtable(void)            // equality
{
	int i;
	struct hashtable* ht = calloc(1, sizeof(struct hashtable));
	if (!ht){
		TRACE_ERROR("calloc: CreateHashtable");
		return 0;
	}

	/* init the tables */
	for (i = 0; i < NUM_BINS; i++)
		TAILQ_INIT(&ht->ht_table[i]);
	return ht;
}
/*----------------------------------------------------------------------------*/
void
DestroyHashtable(struct hashtable *ht)
{
	free(ht);	
}
/*----------------------------------------------------------------------------*/
int 
HTInsert(struct hashtable *ht, tcp_stream *item, unsigned int *hash)
{
	/* create an entry*/ 
	int idx;

	assert(ht);
	assert(ht->ht_count <= 65535); // uint16_t ht_count 

	if (hash)
		idx = (int)*hash;
	else
		idx = HashFlow(item);

	assert(idx >=0 && idx < NUM_BINS);

#if STATIC_TABLE
	int i;
	for (i = 0; i < TCP_AR_CNT; i++) {
		// insert into empty array slot
		if (!ht->ht_array[idx][i]) {
			ht->ht_array[idx][i] = item;
			item->ht_idx = i;
			ht->ht_count++;
			return 0;
		}
	}
	
	TRACE_INFO("[WARNING] HTSearch() cnt: %d!!\n", TCP_AR_CNT);	
#endif

	TAILQ_INSERT_TAIL(&ht->ht_table[idx], item, rcvvar->he_link);
	item->rcvvar->he_mybucket = &ht->ht_table[idx];
	item->ht_idx = TCP_AR_CNT;
	ht->ht_count++;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void* 
HTRemove(struct hashtable *ht, tcp_stream *item)
{
	hash_bucket_head *head;
	//int idx = HashFlow(item);

#if STATIC_TABLE
	if (item->ht_idx < TCP_AR_CNT) {
		assert(ht_array[idx][item->ht_idx]);
		ht->ht_array[idx][item->ht_idx] = NULL;
	} else {
#endif
		//head = &ht->ht_table[idx];
		head = item->rcvvar->he_mybucket;
		assert(head);
		TAILQ_REMOVE(head, item, rcvvar->he_link);	
#if STATIC_TABLE
	}
#endif

	ht->ht_count--;
	return (item);
}	
/*----------------------------------------------------------------------------*/
tcp_stream* 
HTSearch(struct hashtable *ht, const tcp_stream *item, unsigned int *hash)
{
	tcp_stream *walk;
	hash_bucket_head *head;
	int idx;

#if STATIC_TABLE
	int i;

	idx = HashFlow(item);

	for (i = 0; i < TCP_AR_CNT; i++) {
		if (ht->ht_array[idx][i]) {
			if (EQUAL_FLOW(ht->ht_array[idx][i], item)) 
				return ht->ht_array[idx][i];
		}
	}
#endif

	idx = HashFlow(item);
	*hash = idx;

	head = &ht->ht_table[idx];

	TAILQ_FOREACH(walk, head, rcvvar->he_link) {
		if (EQUAL_FLOW(walk, item)) 
			return walk;
	}

	return NULL;
}
/*----------------------------------------------------------------------------*/
