/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_MISC_H_
#define ODP_IPSEC_MISC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>
#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/ipsec.h>

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_DB          32   /**< maximum number of data base entries */
#define MAX_LOOPBACK    10   /**< maximum number of loop back interfaces */
#define MAX_STRING      32   /**< maximum string length */
#define MAX_IV_LEN      32   /**< Maximum IV length in bytes */

#define KEY_BITS_3DES       192  /**< 3DES cipher key length in bits */
#define KEY_BITS_MD5_96     128  /**< MD5_96 auth key length in bits */
#define KEY_BITS_SHA256_128 256  /**< SHA256_128 auth key length in bits */

/**< Number of bits represnted by a string of hexadecimal characters */
#define KEY_STR_BITS(str) (4 * strlen(str))

/** IPv4 helpers for data length and uint8t pointer */
#define ipv4_data_len(ip) (odp_be_to_cpu_16(ip->tot_len) - sizeof(odph_ipv4hdr_t))
#define ipv4_data_p(ip) ((uint8_t *)((odph_ipv4hdr_t *)ip + 1))

/** Helper for calculating encode length using data length and block size */
#define ESP_ENCODE_LEN(x, b) ((((x) + (b - 1)) / b) * b)

/** Get rid of path in filename - only for unix-type paths using '/' */
#define NO_PATH(file_name) (strrchr((file_name), '/') ?                 \
			    strrchr((file_name), '/') + 1 : (file_name))

/**
 * IPsec key
 */
typedef struct {
	uint8_t  data[32];  /**< Key data */
	uint8_t  length;    /**< Key length */
} ipsec_key_t;

/**
 * IPsec algorithm
 */
typedef struct {
	odp_bool_t cipher;
	union {
		odp_cipher_alg_t cipher;
		odp_auth_alg_t   auth;
	} u;
} ipsec_alg_t;

/**
 * IP address range (subnet)
 */
typedef struct ip_addr_range_s {
	uint32_t  addr;     /**< IP address */
	uint32_t  mask;     /**< mask, 1 indicates bits are valid */
} ip_addr_range_t;

/**
 * Parse text string representing a key into ODP key structure
 *
 * @param keystring  Pointer to key string to convert
 * @param key        Pointer to ODP key structure to populate
 * @param alg        Cipher/authentication algorithm associated with the key
 *
 * @return 0 if successful else -1
 */
static inline
int parse_key_string(char *keystring,
		     ipsec_key_t *key,
		     ipsec_alg_t *alg)
{
	int idx;
	int key_bits_in = KEY_STR_BITS(keystring);
	char temp[3];

	key->length = 0;

	/* Algorithm is either cipher or authentication */
	if (alg->cipher) {
		if ((alg->u.cipher == ODP_CIPHER_ALG_3DES_CBC) &&
		    (KEY_BITS_3DES == key_bits_in))
			key->length = key_bits_in / 8;

	} else {
		if ((alg->u.auth == ODP_AUTH_ALG_MD5_96) &&
		    (KEY_BITS_MD5_96 == key_bits_in))
			key->length = key_bits_in / 8;
		else if ((alg->u.auth == ODP_AUTH_ALG_SHA256_128) &&
			 (KEY_BITS_SHA256_128 == key_bits_in))
			key->length = key_bits_in / 8;
	}

	for (idx = 0; idx < key->length; idx++) {
		temp[0] = *keystring++;
		temp[1] = *keystring++;
		temp[2] = 0;
		key->data[idx] = strtol(temp, NULL, 16);
	}

	return key->length ? 0 : -1;
}

/**
 * Check IPv4 address against a range/subnet
 *
 * @param addr  IPv4 address to check
 * @param range Pointer to address range to check against
 *
 * @return 1 if match else 0
 */
static inline
int match_ip_range(uint32_t addr, ip_addr_range_t *range)
{
	return (range->addr == (addr & range->mask));
}

/**
 * Generate text string representing IPv4 address
 *
 * @param b    Pointer to buffer to store string
 * @param addr IPv4 address
 *
 * @return Pointer to supplied buffer
 */
static inline
char *ipv4_addr_str(char *b, uint32_t addr)
{
	sprintf(b, "%03d.%03d.%03d.%03d",
		0xFF & ((addr) >> 24),
		0xFF & ((addr) >> 16),
		0xFF & ((addr) >>  8),
		0xFF & ((addr) >>  0));
	return b;
}

/**
 * Parse text string representing an IPv4 address or subnet
 *
 * String is of the format "XXX.XXX.XXX.XXX(/W)" where
 * "XXX" is decimal value and "/W" is optional subnet length
 *
 * @param ipaddress  Pointer to IP address/subnet string to convert
 * @param addr       Pointer to return IPv4 address
 * @param mask       Pointer (optional) to return IPv4 mask
 *
 * @return 0 if successful else -1
 */
static inline
int parse_ipv4_string(char *ipaddress, uint32_t *addr, uint32_t *mask)
{
	int b[4];
	int qualifier = 32;
	int converted;

	if (strchr(ipaddress, '/')) {
		converted = sscanf(ipaddress, "%d.%d.%d.%d/%d",
				   &b[3], &b[2], &b[1], &b[0],
				   &qualifier);
		if (5 != converted)
			return -1;
	} else {
		converted = sscanf(ipaddress, "%d.%d.%d.%d",
				   &b[3], &b[2], &b[1], &b[0]);
		if (4 != converted)
			return -1;
	}

	if ((b[0] > 255) || (b[1] > 255) || (b[2] > 255) || (b[3] > 255))
		return -1;
	if (!qualifier || (qualifier > 32))
		return -1;

	*addr = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;
	if (mask)
		*mask = ~(0xFFFFFFFF & ((1ULL << (32 - qualifier)) - 1));

	return 0;
}

/**
 * Generate text string representing IPv4 range/subnet, output
 * in "XXX.XXX.XXX.XXX/W" format
 *
 * @param b     Pointer to buffer to store string
 * @param range Pointer to IPv4 address range
 *
 * @return Pointer to supplied buffer
 */
static inline
char *ipv4_subnet_str(char *b, ip_addr_range_t *range)
{
	int idx;
	int len;

	for (idx = 0; idx < 32; idx++)
		if (range->mask & (1 << idx))
			break;
	len = 32 - idx;

	sprintf(b, "%03d.%03d.%03d.%03d/%d",
		0xFF & ((range->addr) >> 24),
		0xFF & ((range->addr) >> 16),
		0xFF & ((range->addr) >>  8),
		0xFF & ((range->addr) >>  0),
		len);
	return b;
}

/**
 * Generate text string representing MAC address
 *
 * @param b     Pointer to buffer to store string
 * @param mac   Pointer to MAC address
 *
 * @return Pointer to supplied buffer
 */
static inline
char *mac_addr_str(char *b, uint8_t *mac)
{
	sprintf(b, "%02X.%02X.%02X.%02X.%02X.%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return b;
}

/**
 * Parse text string representing a MAC address into byte araray
 *
 * String is of the format "XX.XX.XX.XX.XX.XX" where XX is hexadecimal
 *
 * @param macaddress  Pointer to MAC address string to convert
 * @param mac         Pointer to MAC address byte array to populate
 *
 * @return 0 if successful else -1
 */
static inline
int parse_mac_string(char *macaddress, uint8_t *mac)
{
	int macwords[ODPH_ETHADDR_LEN];
	int converted;

	converted = sscanf(macaddress,
			   "%x.%x.%x.%x.%x.%x",
			   &macwords[0], &macwords[1], &macwords[2],
			   &macwords[3], &macwords[4], &macwords[5]);
	if (6 != converted)
		return -1;

	mac[0] = macwords[0];
	mac[1] = macwords[1];
	mac[2] = macwords[2];
	mac[3] = macwords[3];
	mac[4] = macwords[4];
	mac[5] = macwords[5];

	return 0;
}

/**
 * Locate IPsec headers (AH and/or ESP) in packet
 *
 * @param ip     Pointer to packets IPv4 header
 * @param ah_p   Pointer to location to return AH header pointer
 * @param esp_p  Pointer to location to return ESP header pointer
 *
 * @return length of IPsec headers found
 */
static inline
int locate_ipsec_headers(odph_ipv4hdr_t *ip,
			 odph_ahhdr_t **ah_p,
			 odph_esphdr_t **esp_p)
{
	uint8_t *in = ipv4_data_p(ip);
	odph_ahhdr_t *ah = NULL;
	odph_esphdr_t *esp = NULL;

	if (ODPH_IPPROTO_AH == ip->proto) {
		ah = (odph_ahhdr_t *)in;
		in += ((ah)->ah_len + 2) * 4;
		if (ODPH_IPPROTO_ESP == ah->next_header) {
			esp = (odph_esphdr_t *)in;
			in += sizeof(odph_esphdr_t);
		}
	} else if (ODPH_IPPROTO_ESP == ip->proto) {
		esp = (odph_esphdr_t *)in;
		in += sizeof(odph_esphdr_t);
	}

	*ah_p = ah;
	*esp_p = esp;
	return in - (ipv4_data_p(ip));
}

/**
 * Adjust IPv4 length
 *
 * @param ip   Pointer to IPv4 header
 * @param adj  Signed adjustment value
 */
static inline
void ipv4_adjust_len(odph_ipv4hdr_t *ip, int adj)
{
	ip->tot_len = odp_cpu_to_be_16(odp_be_to_cpu_16(ip->tot_len) + adj);
}

/**
 * Verify crypto operation completed successfully
 *
 * @param status  Pointer to cryto completion structure
 *
 * @return TRUE if all OK else FALSE
 */
static inline
odp_bool_t is_crypto_compl_status_ok(odp_crypto_compl_status_t *status)
{
	if (status->alg_err != ODP_CRYPTO_ALG_ERR_NONE)
		return FALSE;
	if (status->hw_err != ODP_CRYPTO_HW_ERR_NONE)
		return FALSE;
	return TRUE;
}


#ifdef __cplusplus
}
#endif

#endif
