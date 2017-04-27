/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

/* enable strtok */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#include <example_debug.h>

#include <odp.h>

#include <odp_ipsec_sa_db.h>

/** Global pointer to sa db */
static sa_db_t *sa_db;

/** Global pointer to tun db */
static tun_db_t *tun_db;

void init_sa_db(void)
{
	odp_shm_t shm;

	shm = odp_shm_reserve("shm_sa_db",
			      sizeof(sa_db_t),
			      ODP_CACHE_LINE_SIZE,
			      0);

	sa_db = odp_shm_addr(shm);

	if (sa_db == NULL) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(sa_db, 0, sizeof(*sa_db));
}

void init_tun_db(void)
{
	odp_shm_t shm;

	shm = odp_shm_reserve("shm_tun_db",
			      sizeof(tun_db_t),
			      ODP_CACHE_LINE_SIZE,
			      0);
	tun_db = odp_shm_addr(shm);

	if (!tun_db) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(tun_db, 0, sizeof(*tun_db));
}

int create_sa_db_entry(char *input, odp_bool_t cipher)
{
	int pos = 0;
	char *local;
	char *str;
	char *save;
	char *token;
	sa_db_entry_t *entry = &sa_db->array[sa_db->index];

	/* Verify we have a good entry */
	if (MAX_DB <= sa_db->index)
		return -1;

	/* Make a local copy */
	local = malloc(strlen(input) + 1);
	if (NULL == local)
		return -1;
	strcpy(local, input);

	/* Set cipher versus auth */
	entry->alg.cipher = cipher;

	/* Setup for using "strtok_r" to search input string */
	str = local;
	save = NULL;

	/* Parse tokens separated by ':' */
	while (NULL != (token = strtok_r(str, ":", &save))) {
		str = NULL;  /* reset str for subsequent strtok_r calls */

		/* Parse token based on its position */
		switch (pos) {
		case 0:
			parse_ipv4_string(token, &entry->src_ip, NULL);
			break;
		case 1:
			parse_ipv4_string(token, &entry->dst_ip, NULL);
			break;
		case 2:
			if (cipher) {
				if (0 == strcmp(token, "3des")) {
					entry->alg.u.cipher =
						ODP_CIPHER_ALG_3DES_CBC;
					entry->block_len  = 8;
					entry->iv_len	  = 8;
				} else {
					entry->alg.u.cipher =
						ODP_CIPHER_ALG_NULL;
				}
			} else {
				if (0 == strcmp(token, "md5")) {
					entry->alg.u.auth =
						ODP_AUTH_ALG_MD5_96;
					entry->icv_len	  = 12;
				} else if (!strcmp(token, "sha256")) {
					entry->alg.u.auth =
						ODP_AUTH_ALG_SHA256_128;
					entry->icv_len	  = 16;
				} else {
					entry->alg.u.auth = ODP_AUTH_ALG_NULL;
				}
			}
			break;
		case 3:
			entry->spi = strtol(token, NULL, 16);
			break;
		case 4:
			parse_key_string(token,
					 &entry->key,
					 &entry->alg);
			break;
		default:
			printf("ERROR: extra token \"%s\" at position %d\n",
			       token, pos);
			break;
		}

		/* Advance to next position */
		pos++;
	}

	/* Verify we parsed exactly the number of tokens we expected */
	if (5 != pos) {
		printf("ERROR: \"%s\" contains %d tokens, expected 5\n",
		       input,
		       pos);
		free(local);
		return -1;
	}

	/* Add route to the list */
	sa_db->index++;
	entry->next = sa_db->list;
	sa_db->list = entry;

	free(local);
	return 0;
}

int create_tun_db_entry(char *input)
{
	int pos = 0;
	char *local;
	char *str;
	char *save;
	char *token;
	tun_db_entry_t *entry = &tun_db->array[tun_db->index];

	/* Verify we have a good entry */
	if (MAX_DB <= tun_db->index)
		return -1;

	/* Make a local copy */
	local = malloc(strlen(input) + 1);
	if (NULL == local)
		return -1;
	strcpy(local, input);

	/* Setup for using "strtok_r" to search input string */
	str = local;
	save = NULL;

	/* Parse tokens separated by ':' */
	while (NULL != (token = strtok_r(str, ":", &save))) {
		str = NULL;  /* reset str for subsequent strtok_r calls */

		/* Parse token based on its position */
		switch (pos) {
		case 0:
			parse_ipv4_string(token, &entry->src_ip, NULL);
			break;
		case 1:
			parse_ipv4_string(token, &entry->dst_ip, NULL);
			break;
		case 2:
			parse_ipv4_string(token, &entry->tun_src_ip, NULL);
			break;
		case 3:
			parse_ipv4_string(token, &entry->tun_dst_ip, NULL);
			break;
		default:
			printf("ERROR: extra token \"%s\" at position %d\n",
			       token, pos);
			break;
		}
		pos++;
	}

	/* Verify we parsed exactly the number of tokens we expected */
	if (4 != pos) {
		printf("ERROR: \"%s\" contains %d tokens, expected 4\n",
		       input,
		       pos);
		free(local);
		return -1;
	}

	/* Add route to the list */
	tun_db->index++;
	entry->next = tun_db->list;
	tun_db->list = entry;

	free(local);
	return 0;
}

tun_db_entry_t *find_tun_db_entry(uint32_t ip_src,
				  uint32_t ip_dst)
{
	tun_db_entry_t *entry = NULL;

	/* Scan all entries and return first match */
	for (entry = tun_db->list; NULL != entry; entry = entry->next) {
		if (entry->src_ip != ip_src)
			continue;
		if (entry->dst_ip != ip_dst)
			continue;
		break;
	}
	return entry;
}

void dump_sa_db(void)
{
	sa_db_entry_t *entry;

	printf("\n"
	       "Security association table\n"
	       "--------------------------\n");

	for (entry = sa_db->list; NULL != entry; entry = entry->next) {
		uint32_t idx;
		char src_ip_str[MAX_STRING];
		char dst_ip_str[MAX_STRING];
		uint8_t *p = entry->key.data;

		printf(" %s %s %s %X %d ",
		       entry->alg.cipher ? "esp" : "ah ",
		       ipv4_addr_str(src_ip_str, entry->src_ip),
		       ipv4_addr_str(dst_ip_str, entry->dst_ip),
		       entry->spi,
		       entry->alg.cipher ?
		       (int)entry->alg.u.cipher :
		       (int)entry->alg.u.auth);

		/* Brute force key display */
		for (idx = 0; idx < entry->key.length; idx++)
			printf("%02X", *p++);

		printf("\n");
	}
}

sa_db_entry_t *find_sa_db_entry(ip_addr_range_t *src,
				ip_addr_range_t *dst,
				odp_bool_t cipher)
{
	sa_db_entry_t *entry = NULL;

	/* Scan all entries and return first match */
	for (entry = sa_db->list; NULL != entry; entry = entry->next) {
		if (cipher != entry->alg.cipher)
			continue;
		if (!match_ip_range(entry->src_ip, src))
			continue;
		if (!match_ip_range(entry->dst_ip, dst))
			continue;
		break;
	}
	return entry;
}

void dump_tun_db(void)
{
	tun_db_entry_t *entry;

	printf("\n"
	       "Tunnel table\n"
	       "--------------------------\n");

	for (entry = tun_db->list; NULL != entry; entry = entry->next) {
		char src_ip_str[MAX_STRING];
		char dst_ip_str[MAX_STRING];
		char tun_src_ip_str[MAX_STRING];
		char tun_dst_ip_str[MAX_STRING];

		printf(" %s:%s %s:%s ",
		       ipv4_addr_str(src_ip_str, entry->src_ip),
		       ipv4_addr_str(dst_ip_str, entry->dst_ip),
		       ipv4_addr_str(tun_src_ip_str, entry->tun_src_ip),
		       ipv4_addr_str(tun_dst_ip_str, entry->tun_dst_ip)
		      );

		printf("\n");
	}
}
