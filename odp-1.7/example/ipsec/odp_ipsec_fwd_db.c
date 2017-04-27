/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/* enable strtok */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#include <example_debug.h>

#include <odp.h>

#include <odp_ipsec_fwd_db.h>

/** Global pointer to fwd db */
fwd_db_t *fwd_db;

void init_fwd_db(void)
{
	odp_shm_t shm;

	shm = odp_shm_reserve("shm_fwd_db",
			      sizeof(fwd_db_t),
			      ODP_CACHE_LINE_SIZE,
			      0);

	fwd_db = odp_shm_addr(shm);

	if (fwd_db == NULL) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(fwd_db, 0, sizeof(*fwd_db));
}

int create_fwd_db_entry(char *input)
{
	int pos = 0;
	char *local;
	char *str;
	char *save;
	char *token;
	fwd_db_entry_t *entry = &fwd_db->array[fwd_db->index];

	/* Verify we haven't run out of space */
	if (MAX_DB <= fwd_db->index)
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
			parse_ipv4_string(token,
					  &entry->subnet.addr,
					  &entry->subnet.mask);
			break;
		case 1:
			strncpy(entry->oif, token, OIF_LEN - 1);
			entry->oif[OIF_LEN - 1] = 0;
			break;
		case 2:
			parse_mac_string(token, entry->dst_mac);
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
	if (3 != pos) {
		printf("ERROR: \"%s\" contains %d tokens, expected 3\n",
		       input,
		       pos);
		free(local);
		return -1;
	}

	/* Reset queue to invalid */
	entry->queue = ODP_QUEUE_INVALID;

	/* Add route to the list */
	fwd_db->index++;
	entry->next = fwd_db->list;
	fwd_db->list = entry;

	free(local);
	return 0;
}

void resolve_fwd_db(char *intf, odp_queue_t outq, uint8_t *mac)
{
	fwd_db_entry_t *entry;

	/* Walk the list and attempt to set output queue and MAC */
	for (entry = fwd_db->list; NULL != entry; entry = entry->next) {
		if (strcmp(intf, entry->oif))
			continue;

		entry->queue = outq;
		memcpy(entry->src_mac, mac, ODPH_ETHADDR_LEN);
	}
}

void dump_fwd_db_entry(fwd_db_entry_t *entry)
{
	char subnet_str[MAX_STRING];
	char mac_str[MAX_STRING];

	printf(" %s %s %s\n",
	       ipv4_subnet_str(subnet_str, &entry->subnet),
	       entry->oif,
	       mac_addr_str(mac_str, entry->dst_mac));
}

void dump_fwd_db(void)
{
	fwd_db_entry_t *entry;

	printf("\n"
	       "Routing table\n"
	       "-------------\n");

	for (entry = fwd_db->list; NULL != entry; entry = entry->next)
		dump_fwd_db_entry(entry);
}

fwd_db_entry_t *find_fwd_db_entry(uint32_t dst_ip)
{
	fwd_db_entry_t *entry;

	for (entry = fwd_db->list; NULL != entry; entry = entry->next)
		if (entry->subnet.addr == (dst_ip & entry->subnet.mask))
			break;
	return entry;
}
