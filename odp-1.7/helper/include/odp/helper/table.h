/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP table
 *
 * TCAM(Ternary Content Addressable Memory) is used widely in packet
 * forwarding to speedup the table lookup.
 *
 * This file contains a simple interface for creating and using the table.
 * Table here means a collection of related data held in a structured
 * format.
 * Some examples for table are ARP Table, Routing Table, etc.The table
 * contains many entries, each consist of key and associated data
 * (called key/value pair or rule/action pair in some paper.).
 * Enclosed are some classical table examples, used publicly
 * in data plane packet processing:
 *
 * <H3>Use Case: ARP table</H3>
 *   Once a route has been identified for an IP packet (so the output
 *   interface and the IP address of the next hop station are known),
 *   the MAC address of the next hop station is needed in order to
 *   send this packet onto the next leg of the journey
 *   towards its destination (as identified by its destination IP address).
 *   The MAC address of the next hop station becomes the destination
 *   MAC address of the outgoing Ethernet frame.
 *   <ol>
 *   <li>Key: The pair of (Output interface, Next Hop IP address)
 *   <li>Associated Data: MAC address of the next hop station
 *   <li>Algorithm: Hash
 *	 </ol>
 *
 * <H3>Use Case: Routing Table</H3>
 *   When each router receives a packet, it searches its routing table
 *   to find the best match between the destination IP address of
 *   the packet and one of the network addresses in the routing table.
 *   <ol>
 *   <li>Key: destination IP address
 *   <li>Associated Data: The pair of (Output interface, Next Hop IP address)
 *   <li>Algorithm: LPM(Longest Prefix Match)
 *   </ol>
 *
 * <H3>Use Case: Flow Classification</H3>
 *   The flow classification is executed at least once for each
 *   input packet.This operation maps each incoming packet against
 *   one of the known traffic
 *   flows in the flow database that typically contains millions of flows.
 *   <ol>
 *   <li>Key:n-tuple of packet fields that uniquely identify a traffic flow.
 *   <li>Associated data:
 *     actions and action meta-data describing what processing to be
 *     applied for the packets of the current flow, such as whether
 *     encryption/decryption is required on this packet, what kind of cipher
 *     algorithm should be chosed.
 *   <li>Algorithm: Hash
 *   </ol>
 *
 *  All these different types of lookup tables have the common operations:
 *    create a table, destroy a table, add (key,associated data),
 *    delete key and look up the associated data via the key.
 *  Usually these operations are software based, but also can be
 *  hardware accelerated such as using TCAM to implement ARP table
 *  or Routing Table.
 *  And specific alogithm can be used for specific lookup table.
 *
 *  notes: key/value and key/associated data mean the same thing
 *         in this file unless otherwise mentioned.
 *
 */

#ifndef ODPH_TABLE_H_
#define ODPH_TABLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @def ODPH_TABLE_NAME_LEN
 * Max length of table name
 */
#define ODPH_TABLE_NAME_LEN      32

#include <odp/helper/strong_types.h>
/** ODP table handle */
typedef ODPH_HANDLE_T(odph_table_t);

/**
* create a table
* Generally, tables only support key-value pair both with fixed size
*
* @param name
*    name of this table, max ODPH_TABLE_NAME_LEN - 1
*    May be specified as NULL for anonymous table
* @param capacity
*    Max memory usage this table use, in MBytes
* @param key_size
*    fixed size of the 'key' in bytes.
* @param value_size
*    fixed size of the 'value' in bytes.
* @return
*   Handle to table instance or NULL if failed
* @note
*/
typedef odph_table_t (*odph_table_create)(const char *name,
						uint32_t capacity,
						uint32_t key_size,
						uint32_t value_size);

/**
 * Find a table by name
 *
 * @param name      Name of the table
 *
 * @return Handle of found table
 * @retval NULL  table could not be found
 *
 * @note This routine cannot be used to look up an anonymous
 *       table (one created with no name).
 *       This API supports Multiprocess
 */
typedef odph_table_t (*odph_table_lookup)(const char *name);

/**
 * Destroy a table previously created by odph_table_create()
 *
 * @param table  Handle of the table to be destroyed
 *
 * @retval 0 Success
 * @retval -1 Failure
 *
 * @note This routine destroys a previously created pool
 *       also should free any memory allocated at creation
 *
 */
typedef int (*odph_table_destroy)(odph_table_t table);

/**
 * Add (key,associated data) pair into the specific table.
 * When no associated data is currently assocated with key,
 * then the (key,assocatied data) association is created.
 * When key is already associated with data0, then association (key, data0)
 * will be removed and association (key, associated data) is created.
 *
 * @param table  Handle of the table that the element be added
 *
 * @param key   address of 'key' in key-value pair.
 *              User should make sure the address and 'key_size'
 *              bytes after are accessible
 * @param value address of 'value' in key-value pair
 *              User should make sure the address and 'value_size'
 *              bytes after are accessible
 * @retval 0 Success
 * @retval -1 Failure
 * @note  Add a same key again with a new value, the older one will
 *        be covered.
 */
typedef int (*odph_table_put_value)(odph_table_t table, void *key,
							void *value);

/**
 * Lookup the associated data via specific key.
 * When no value is currently associated with key, then this operation
 * restuns <0 to indicate the lookup miss.
 * When key is associated with value,
 * then this operation returns value.
 * The (key,value) association won't change.
 *
 * @param table  Handle of the table that the element be added
 *
 * @param key   address of 'key' in key-value pair
 *              User should make sure the address and key_size bytes after
 *              are accessible
 *
 * @param buffer   output The buffer address to the 'value'
 *                 After successfully found, the content of 'value' will be
 *                 copied to this address
 *                 User should make sure the address and value_size bytes
 *                 after are accessible
 * @param buffer_size  size of the buffer
 *                     should be equal or bigger than value_size
 * @retval 0 Success
 * @retval -1 Failure
 *
 * @note
 */
typedef int (*odph_table_get_value)(odph_table_t table, void *key,
						void *buffer,
						uint32_t buffer_size);
/**
 * Delete the association specified by key
 * When no data is currently associated with key, this operation
 * has no effect. When key is already associated data ad0,
 * then (key,ad0) pair is deleted.
 *
 * @param table Handle of the table that the element will be removed from
 *
 * @param key   address of 'key' in key-value pair
 *              User should make sure the address and key_size bytes after
 *              are accessible
 *
 * @retval 0 Success
 * @retval -1 Failure
 *
 * @note
 */
typedef int (*odph_table_remove_value)(odph_table_t table, void *key);

/**
 * Table interface set. Defining the table operations.
 */
typedef struct odp_table_ops {
	odph_table_create        f_create;       /**< Table Create */
	odph_table_lookup        f_lookup;       /**< Table Lookup */
	odph_table_destroy       f_des;          /**< Table Destroy */
	/** add (key,associated data) pair into the specific table */
	odph_table_put_value     f_put;
	/** lookup the associated data via specific key */
	odph_table_get_value     f_get;
	/** delete the association specified by key */
	odph_table_remove_value  f_remove;
} odph_table_ops_t;

#ifdef __cplusplus
}
#endif

#endif

