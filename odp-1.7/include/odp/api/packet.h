/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


/**
 * @file
 *
 * ODP packet descriptor
 */

#ifndef ODP_API_PACKET_H_
#define ODP_API_PACKET_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup odp_packet ODP PACKET
 *  Operations on a packet.
 *  @{
 */

/**
 * @typedef odp_packet_t
 * ODP packet
 */

/**
 * @def ODP_PACKET_INVALID
 * Invalid packet
 */

/**
 * @def ODP_PACKET_OFFSET_INVALID
 * Invalid packet offset
 */

/**
 * @typedef odp_packet_seg_t
 * ODP packet segment
 */

/**
 * @def ODP_PACKET_SEG_INVALID
 * Invalid packet segment
 */

/*
 *
 * Alloc and free
 * ********************************************************
 *
 */
/* void show_len(void);*/
/**
 * Allocate a packet from a buffer pool
 *
 * Allocates a packet of the requested length from the specified buffer pool.
 * Pool must have been created with ODP_POOL_PACKET type. The
 * packet is initialized with data pointers and lengths set according to the
 * specified len, and the default headroom and tailroom length settings. All
 * other packet metadata are set to their default values.
 *
 * @param pool          Pool handle
 * @param len           Packet data length
 *
 * @return Handle of allocated packet
 * @retval ODP_PACKET_INVALID  Packet could not be allocated
 *
 * @note The default headroom and tailroom used for packets is specified by
 * the ODP_CONFIG_PACKET_HEADROOM and ODP_CONFIG_PACKET_TAILROOM defines in
 * odp_config.h.
 */
odp_packet_t odp_packet_alloc(odp_pool_t pool, uint32_t len);

/**
 * Allocate multiple packets from a buffer pool
 *
 * Otherwise like odp_packet_alloc(), but allocates multiple
 * packets from a pool.
 *
 * @param pool          Pool handle
 * @param len           Packet data length
 * @param[out] pkt      Array of packet handles for output
 * @param num           Maximum number of packets to allocate
 *
 * @return Number of packets actually allocated (0 ... num)
 * @retval <0 on failure
 *
 */
int odp_packet_alloc_multi(odp_pool_t pool, uint32_t len,
			   odp_packet_t pkt[], int num);

/**
 * Free packet
 *
 * Frees the packet into the buffer pool it was allocated from.
 *
 * @param pkt           Packet handle
 */
void odp_packet_free(odp_packet_t pkt);

/**
 * Free multiple packets
 *
 * Otherwise like odp_packet_free(), but frees multiple packets
 * to their originating pools.
 *
 * @param pkt           Array of packet handles
 * @param num           Number of packet handles to free
 */
void odp_packet_free_multi(const odp_packet_t pkt[], int num);

/**
 * Reset packet
 *
 * Resets all packet metadata to their default values. Packet length is used
 * to initialize pointers and lengths. It must be less than the total buffer
 * length of the packet minus the default headroom length. Packet is not
 * modified on failure.
 *
 * @param pkt           Packet handle
 * @param len           Packet data length
 *
 * @retval 0 on success
 * @retval <0 on failure
 *
 * @see odp_packet_buf_len()
 */
int odp_packet_reset(odp_packet_t pkt, uint32_t len);

/**
 * Get packet handle from event
 *
 * Converts an ODP_EVENT_PACKET type event to a packet.
 *
 * @param ev   Event handle
 *
 * @return Packet handle
 *
 * @see odp_event_type()
 */
odp_packet_t odp_packet_from_event(odp_event_t ev);

/**
 * Convert packet handle to event
 *
 * @param pkt  Packet handle
 *
 * @return Event handle
 */
odp_event_t odp_packet_to_event(odp_packet_t pkt);

/*
 *
 * Pointers and lengths
 * ********************************************************
 *
 */

/**
 * Packet head address
 *
 * Returns start address of the first segment. Packet level headroom starts
 * from here. Use odp_packet_data() or odp_packet_l2_ptr() to return the
 * packet data start address.
 *
 * @param pkt  Packet handle
 *
 * @return Pointer to the start address of the first packet segment
 *
 * @see odp_packet_data(), odp_packet_l2_ptr(), odp_packet_headroom()
 */
void *odp_packet_head(odp_packet_t pkt);

/**
 * Total packet buffer length
 *
 * Returns sum of buffer lengths over all packet segments.
 *
 * @param pkt  Packet handle
 *
 * @return  Total packet buffer length in bytes
 *
 * @see odp_packet_reset()
 */
uint32_t odp_packet_buf_len(odp_packet_t pkt);

/**
 * Packet data pointer
 *
 * Returns the current packet data pointer. When a packet is received
 * from packet input, this points to the first byte of the received
 * packet. Packet level offsets are calculated relative to this position.
 *
 * User can adjust the data pointer with head_push/head_pull (does not modify
 * segmentation) and add_data/rem_data calls (may modify segmentation).
 *
 * @param pkt  Packet handle
 *
 * @return  Pointer to the packet data
 *
 * @see odp_packet_l2_ptr(), odp_packet_seg_len()
 */
void *odp_packet_data(odp_packet_t pkt);

/**
 * Packet data physical address
 *
 * Returns the current packet physical addressr. When a packet is received
 * from packet input, this points to the first byte of the received
 * packet by hardware nic. Packet level offsets are calculated relative to this position.
 *
 * User can adjust the address with head_push/head_pull (does not modify
 * segmentation) and add_data/rem_data calls (may modify segmentation).
 *
 * @param pkt  Packet handle
 *
 * @return  Pointer to the packet data
 *
 * @see odp_packet_l2_ptr(), odp_packet_seg_len()
 */
uint64_t odp_packet_data_phyaddr(odp_packet_t pkt);

/**
 * Packet segment data length
 *
 * Returns number of data bytes following the current data pointer
 * (odp_packet_data()) location in the segment.
 *
 * @param pkt  Packet handle
 *
 * @return  Segment data length in bytes (pointed by odp_packet_data())
 *
 * @see odp_packet_data()
 */
uint32_t odp_packet_seg_len(odp_packet_t pkt);

/**
 * Packet data length
 *
 * Returns sum of data lengths over all packet segments.
 *
 * @param pkt  Packet handle
 *
 * @return Packet data length
 */
uint32_t odp_packet_len(odp_packet_t pkt);

/**
 * Packet headroom length
 *
 * Returns the current packet level headroom length.
 *
 * @param pkt  Packet handle
 *
 * @return Headroom length
 */
uint32_t odp_packet_headroom(odp_packet_t pkt);

/**
 * Packet tailroom length
 *
 * Returns the current packet level tailroom length.
 *
 * @param pkt  Packet handle
 *
 * @return Tailroom length
 */
uint32_t odp_packet_tailroom(odp_packet_t pkt);

/**
 * Packet tailroom pointer
 *
 * Returns pointer to the start of the current packet level tailroom.
 *
 * User can adjust the tail pointer with tail_push/tail_pull (does not modify
 * segmentation) and add_data/rem_data calls (may modify segmentation).
 *
 * @param pkt  Packet handle
 *
 * @return  Tailroom pointer
 *
 * @see odp_packet_tailroom()
 */
void *odp_packet_tail(odp_packet_t pkt);

/**
 * Push out packet head
 *
 * Increase packet data length by moving packet head into packet headroom.
 * Packet headroom is decreased with the same amount. The packet head may be
 * pushed out up to 'headroom' bytes. Packet is not modified if there's not
 * enough headroom space.
 *
 * odp_packet_xxx:
 * seg_len  += len
 * len      += len
 * headroom -= len
 * data     -= len
 *
 * Operation does not modify packet segmentation or move data. Handles and
 * pointers remain valid. User is responsible to update packet metadata
 * offsets when needed.
 *
 * @param pkt  Packet handle
 * @param len  Number of bytes to push the head (0 ... headroom)
 *
 * @return The new data pointer
 * @retval NULL  Requested offset exceeds available headroom
 *
 * @see odp_packet_headroom(), odp_packet_pull_head()
 */
void *odp_packet_push_head(odp_packet_t pkt, uint32_t len);

/**
 * Pull in packet head
 *
 * Decrease packet data length by removing data from the head of the packet.
 * Packet headroom is increased with the same amount. Packet head may be pulled
 * in up to seg_len - 1 bytes (i.e. packet data pointer must stay in the
 * first segment). Packet is not modified if there's not enough data.
 *
 * odp_packet_xxx:
 * seg_len  -= len
 * len      -= len
 * headroom += len
 * data     += len
 *
 * Operation does not modify packet segmentation or move data. Handles and
 * pointers remain valid. User is responsible to update packet metadata
 * offsets when needed.
 *
 * @param pkt  Packet handle
 * @param len  Number of bytes to pull the head (0 ... seg_len - 1)
 *
 * @return The new data pointer
 * @retval NULL  Requested offset exceeds packet segment length
 *
 * @see odp_packet_seg_len(), odp_packet_push_head()
 */
void *odp_packet_pull_head(odp_packet_t pkt, uint32_t len);

/**
 * Push out packet tail
 *
 * Increase packet data length by moving packet tail into packet tailroom.
 * Packet tailroom is decreased with the same amount. The packet tail may be
 * pushed out up to 'tailroom' bytes. Packet is not modified if there's not
 * enough tailroom.
 *
 * last_seg:
 * data_len += len
 *
 * odp_packet_xxx:
 * len      += len
 * tail     += len
 * tailroom -= len
 *
 * Operation does not modify packet segmentation or move data. Handles,
 * pointers and offsets remain valid.
 *
 * @param pkt  Packet handle
 * @param len  Number of bytes to push the tail (0 ... tailroom)
 *
 * @return The old tail pointer
 * @retval NULL  Requested offset exceeds available tailroom
 *
 * @see odp_packet_tailroom(), odp_packet_pull_tail()
 */
void *odp_packet_push_tail(odp_packet_t pkt, uint32_t len);

/**
 * Pull in packet tail
 *
 * Decrease packet data length by removing data from the tail of the packet.
 * Packet tailroom is increased with the same amount. Packet tail may be pulled
 * in up to last segment data_len - 1 bytes. (i.e. packet tail must stay in the
 * last segment). Packet is not modified if there's not enough data.
 *
 * last_seg:
 * data_len -= len
 *
 * odp_packet_xxx:
 * len      -= len
 * tail     -= len
 * tailroom += len
 *
 * Operation does not modify packet segmentation or move data. Handles and
 * pointers remain valid. User is responsible to update packet metadata
 * offsets when needed.
 *
 * @param pkt  Packet handle
 * @param len  Number of bytes to pull the tail (0 ... last_seg:data_len - 1)
 *
 * @return The new tail pointer
 * @retval NULL  The specified offset exceeds allowable data length
 */
void *odp_packet_pull_tail(odp_packet_t pkt, uint32_t len);

/**
 * Packet offset pointer
 *
 * Returns pointer to data in the packet offset. The packet level byte offset is
 * calculated from the current odp_packet_data() position. Optionally outputs
 * handle to the segment and number of data bytes in the segment following the
 * pointer.
 *
 * @param      pkt      Packet handle
 * @param      offset   Byte offset into the packet
 * @param[out] len      Number of data bytes remaining in the segment (output).
 *                      Ignored when NULL.
 * @param[out] seg      Handle to the segment containing the address (output).
 *                      Ignored when NULL.
 *
 * @return Pointer to the offset
 * @retval NULL  Requested offset exceeds packet length
 */
void *odp_packet_offset(odp_packet_t pkt, uint32_t offset, uint32_t *len,
			odp_packet_seg_t *seg);

/*
 *
 * Meta-data
 * ********************************************************
 *
 */

/**
 * Packet pool
 *
 * Returns handle to the buffer pool where the packet was allocated from.
 *
 * @param pkt   Packet handle
 *
 * @return Buffer pool handle
 */
odp_pool_t odp_packet_pool(odp_packet_t pkt);

/**
 * Packet input interface
 *
 * Returns handle to the packet IO interface which received the packet or
 * ODP_PKTIO_INVALID when the packet was allocated/reset by the application.
 *
 * @param pkt   Packet handle
 *
 * @return Packet interface handle
 * @retval ODP_PKTIO_INVALID  Packet was not received on any interface
 */
odp_pktio_t odp_packet_input(odp_packet_t pkt);

/**
 * User context pointer
 *
 * Return previously stored user context pointer.
 *
 * @param pkt  Packet handle
 *
 * @return User context pointer
 */
void *odp_packet_user_ptr(odp_packet_t pkt);

/**
 * Set user context pointer
 *
 * Each packet has room for a user defined context pointer. The pointer value
 * does not necessarily represent a valid address - e.g. user may store any
 * value of type intptr_t. ODP may use the pointer for data prefetching, but
 * must ignore any invalid addresses.
 *
 * @param pkt  Packet handle
 * @param ctx  User context pointer
 */
void odp_packet_user_ptr_set(odp_packet_t pkt, const void *ctx);

/**
 * User area address
 *
 * Each packet has an area for user data. Size of the area is fixed and defined
 * in packet pool parameters.
 *
 * @param pkt  Packet handle
 *
 * @return       User area address associated with the packet
 * @retval NULL  The packet does not have user area
 */
void *odp_packet_user_area(odp_packet_t pkt);

/**
 * User area size
 *
 * The size is fixed and defined in packet pool parameters.
 *
 * @param pkt  Packet handle
 *
 * @return  User area size in bytes
 */
uint32_t odp_packet_user_area_size(odp_packet_t pkt);

/**
 * Layer 2 start pointer
 *
 * Returns pointer to the start of the layer 2 header. Optionally, outputs
 * number of data bytes in the segment following the pointer.
 *
 * @param      pkt      Packet handle
 * @param[out] len      Number of data bytes remaining in the segment (output).
 *                      Ignored when NULL.
 *
 * @return  Layer 2 start pointer
 * @retval  NULL packet does not contain a valid L2 header
 *
 * @see odp_packet_l2_offset(), odp_packet_l2_offset_set(), odp_packet_has_l2()
 */
void *odp_packet_l2_ptr(odp_packet_t pkt, uint32_t *len);

/**
 * Layer 2 start offset
 *
 * Returns offset to the start of the layer 2 header. The offset is calculated
 * from the current odp_packet_data() position in bytes.
 *
 * User is responsible to update the offset when modifying the packet data
 * pointer position.
 *
 * @param pkt  Packet handle
 *
 * @return  Layer 2 start offset
 * @retval ODP_PACKET_OFFSET_INVALID packet does not contain a valid L2 header
 *
 * @see odp_packet_l2_offset_set(), odp_packet_has_l2()
 */
uint32_t odp_packet_l2_offset(odp_packet_t pkt);

/**
 * Set layer 2 start offset
 *
 * Set offset to the start of the layer 2 header. The offset is calculated from
 * the current odp_packet_data() position in bytes. Offset must not exceed
 * packet data length. Packet is not modified on an error.
 *
 * @param pkt     Packet handle
 * @param offset  Layer 2 start offset (0 ... odp_packet_len()-1)
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_packet_l2_offset_set(odp_packet_t pkt, uint32_t offset);

/**
 * Layer 3 start pointer
 *
 * Returns pointer to the start of the layer 3 header. Optionally, outputs
 * number of data bytes in the segment following the pointer.
 *
 * @param      pkt      Packet handle
 * @param[out] len      Number of data bytes remaining in the segment (output).
 *                      Ignored when NULL.
 *
 * @return  Layer 3 start pointer
 * @retval NULL packet does not contain a valid L3 header
 *
 * @see odp_packet_l3_offset(), odp_packet_l3_offset_set(), odp_packet_has_l3()
 */
void *odp_packet_l3_ptr(odp_packet_t pkt, uint32_t *len);

/**
 * Layer 3 start offset
 *
 * Returns offset to the start of the layer 3 header. The offset is calculated
 * from the current odp_packet_data() position in bytes.
 *
 * User is responsible to update the offset when modifying the packet data
 * pointer position.
 *
 * @param pkt  Packet handle
 *
 * @return  Layer 3 start offset, or ODP_PACKET_OFFSET_INVALID when packet does
 *          not contain a valid L3 header.
 *
 * @see odp_packet_l3_offset_set(), odp_packet_has_l3()
 */
uint32_t odp_packet_l3_offset(odp_packet_t pkt);

/**
 * Set layer 3 start offset
 *
 * Set offset to the start of the layer 3 header. The offset is calculated from
 * the current odp_packet_data() position in bytes. Offset must not exceed
 * packet data length. Packet is not modified on an error.
 *
 * @param pkt     Packet handle
 * @param offset  Layer 3 start offset (0 ... odp_packet_len()-1)
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_packet_l3_offset_set(odp_packet_t pkt, uint32_t offset);

/**
 * Layer 4 start pointer
 *
 * Returns pointer to the start of the layer 4 header. Optionally, outputs
 * number of data bytes in the segment following the pointer.
 *
 * @param      pkt      Packet handle
 * @param[out] len      Number of data bytes remaining in the segment (output).
 *                      Ignored when NULL.
 *
 * @return  Layer 4 start pointer
 * @retval NULL packet does not contain a valid L4 header
 *
 * @see odp_packet_l4_offset(), odp_packet_l4_offset_set(), odp_packet_has_l4()
 */
void *odp_packet_l4_ptr(odp_packet_t pkt, uint32_t *len);

/**
 * Layer 4 start offset
 *
 * Returns offset to the start of the layer 4 header. The offset is calculated
 * from the current odp_packet_data() position in bytes.
 *
 * User is responsible to update the offset when modifying the packet data
 * pointer position.
 *
 * @param pkt  Packet handle
 *
 * @return  Layer 4 start offset
 * @retval ODP_PACKET_OFFSET_INVALID packet does not contain a valid L4 header
 *
 * @see odp_packet_l4_offset_set(), odp_packet_has_l4()
 */
uint32_t odp_packet_l4_offset(odp_packet_t pkt);

/**
 * Set layer 4 start offset
 *
 * Set offset to the start of the layer 4 header. The offset is calculated from
 * the current odp_packet_data() position in bytes. Offset must not exceed
 * packet data length. Packet is not modified on an error.
 *
 * @param pkt     Packet handle
 * @param offset  Layer 4 start offset (0 ... odp_packet_len()-1)
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_packet_l4_offset_set(odp_packet_t pkt, uint32_t offset);

/**
 * Packet flow hash value
 *
 * Returns the hash generated from the packet header. Use
 * odp_packet_has_flow_hash() to check if packet contains a hash.
 *
 * @param      pkt      Packet handle
 *
 * @return  Hash value
 *
 * @note Zero can be a valid hash value.
 * @note The hash algorithm and the header fields defining the flow (therefore
 * used for hashing) is platform dependent. It is possible a platform doesn't
 * generate any hash at all.
 * @note The returned hash is either the platform generated (if any), or if
 * odp_packet_flow_hash_set() were called then the value set there.
 */
uint32_t odp_packet_flow_hash(odp_packet_t pkt);

/**
 * Set packet flow hash value
 *
 * Store the packet flow hash for the packet and sets the flow hash flag. This
 * enables (but does not require!) application to reflect packet header
 * changes in the hash.
 *
 * @param      pkt              Packet handle
 * @param      flow_hash        Hash value to set
 *
 * @note If the platform needs to keep the original hash value, it has to
 * maintain it internally. Overwriting the platform provided value doesn't
 * change how the platform handles this packet after it.
 * @note The application is not required to keep this hash valid for new or
 * modified packets.
 */
void odp_packet_flow_hash_set(odp_packet_t pkt, uint32_t flow_hash);

/**
 * Tests if packet is segmented
 *
 * @param pkt  Packet handle
 *
 * @retval 0 Packet is not segmented
 * @retval 1 Packet is segmented
 */
int odp_packet_is_segmented(odp_packet_t pkt);

/**
 * Number of segments
 *
 * Returns number of segments in the packet. A packet has always at least one
 * segment.
 *
 * @param pkt  Packet handle
 *
 * @return Number of segments (>0)
 */
int odp_packet_num_segs(odp_packet_t pkt);

/**
 * First segment in packet
 *
 * A packet has always the first segment (has at least one segment).
 *
 * @param pkt  Packet handle
 *
 * @return Handle to the first segment
 */
odp_packet_seg_t odp_packet_first_seg(odp_packet_t pkt);

/**
 * Last segment in packet
 *
 * A packet has always the last segment (has at least one segment).
 *
 * @param pkt  Packet handle
 *
 * @return Handle to the last segment
 */
odp_packet_seg_t odp_packet_last_seg(odp_packet_t pkt);

/**
 * Next segment in packet
 *
 * Returns handle to the next segment after the current segment, or
 * ODP_PACKET_SEG_INVALID if there are no more segments. Use
 * odp_packet_first_seg() to get handle to the first segment.
 *
 * @param pkt   Packet handle
 * @param seg   Current segment handle
 *
 * @return Handle to the next segment
 * @retval ODP_PACKET_SEG_INVALID if there are no more segments
 */
odp_packet_seg_t odp_packet_next_seg(odp_packet_t pkt, odp_packet_seg_t seg);


/*
 *
 * Segment level
 * ********************************************************
 *
 */

/**
 * Segment buffer address
 *
 * Returns start address of the segment.
 *
 * @param pkt  Packet handle
 * @param seg  Segment handle
 *
 * @return  Start address of the segment
 * @retval NULL on failure
 *
 * @see odp_packet_seg_buf_len()
 */
void *odp_packet_seg_buf_addr(odp_packet_t pkt, odp_packet_seg_t seg);

/**
 * Segment buffer length
 *
 * Returns segment buffer length in bytes.
 *
 * @param pkt  Packet handle
 * @param seg  Segment handle
 *
 * @return  Segment buffer length in bytes
 *
 * @see odp_packet_seg_buf_addr()
 */
uint32_t odp_packet_seg_buf_len(odp_packet_t pkt, odp_packet_seg_t seg);

/**
 * Segment data pointer
 *
 * Returns pointer to the first byte of data in the segment.
 *
 * @param pkt  Packet handle
 * @param seg  Segment handle
 *
 * @return  Pointer to the segment data
 * @retval NULL on failure
 *
 * @see odp_packet_seg_data_len()
 */
void *odp_packet_seg_data(odp_packet_t pkt, odp_packet_seg_t seg);

/**
 * Segment data length
 *
 * Returns segment data length in bytes.
 *
 * @param pkt  Packet handle
 * @param seg  Segment handle
 *
 * @return  Segment data length in bytes
 *
 * @see odp_packet_seg_data()
 */
uint32_t odp_packet_seg_data_len(odp_packet_t pkt, odp_packet_seg_t seg);


/*
 *
 * Manipulation
 * ********************************************************
 *
 */


/**
 * Add data into an offset
 *
 * Increases packet data length by adding new data area into the specified
 * offset. The operation returns a new packet handle on success. It may modify
 * packet segmentation and move data. Handles and pointers must be updated
 * after the operation. User is responsible to update packet metadata offsets
 * when needed. The packet is not modified on an error.
 *
 * @param pkt     Packet handle
 * @param offset  Byte offset into the packet
 * @param len     Number of bytes to add into the offset
 *
 * @return New packet handle
 * @retval ODP_PACKET_INVALID on failure
 */
odp_packet_t odp_packet_add_data(odp_packet_t pkt, uint32_t offset,
				 uint32_t len);

/**
 * Remove data from an offset
 *
 * Decreases packet data length by removing data from the specified offset.
 * The operation returns a new packet handle on success, and may modify
 * packet segmentation and move data. Handles and pointers must be updated
 * after the operation. User is responsible to update packet metadata offsets
 * when needed. The packet is not modified on an error.
 *
 * @param pkt     Packet handle
 * @param offset  Byte offset into the packet
 * @param len     Number of bytes to remove from the offset
 *
 * @return New packet handle
 * @retval ODP_PACKET_INVALID on failure
 */
odp_packet_t odp_packet_rem_data(odp_packet_t pkt, uint32_t offset,
				 uint32_t len);


/*
 *
 * Copy
 * ********************************************************
 *
 */

/**
 * Copy packet
 *
 * Create a new copy of the packet. The new packet is exact copy of the source
 * packet (incl. data and metadata). The pool must have been created with
 * ODP_POOL_PACKET type.
 *
 * @param pkt   Packet handle
 * @param pool  Buffer pool for allocation of the new packet.
 *
 * @return Handle to the copy of the packet
 * @retval ODP_PACKET_INVALID on failure
 */
odp_packet_t odp_packet_copy(odp_packet_t pkt, odp_pool_t pool);

/**
 * Copy data from packet
 *
 * Copy 'len' bytes of data from the packet level offset to the destination
 * address.
 *
 * @param pkt    Packet handle
 * @param offset Byte offset into the packet
 * @param len    Number of bytes to copy
 * @param dst    Destination address
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_packet_copydata_out(odp_packet_t pkt, uint32_t offset,
			    uint32_t len, void *dst);

/**
 * Copy data into packet
 *
 * Copy    'len' bytes of data from the source address into the packet level
 * offset. Maximum number of bytes to copy is packet data length minus the
 * offset. Packet is not modified on an error.
 *
 * @param pkt    Packet handle
 * @param offset Byte offset into the packet
 * @param len    Number of bytes to copy
 * @param src    Source address
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_packet_copydata_in(odp_packet_t pkt, uint32_t offset,
			   uint32_t len, const void *src);

/*
 *
 * Debugging
 * ********************************************************
 *
 */

/**
 * Print packet to the console
 *
 * Print all packet debug information to the console.
 *
 * @param pkt  Packet handle
 */
void odp_packet_print(odp_packet_t pkt);

/**
 * Perform full packet validity check
 *
 * The operation may consume considerable number of cpu cycles depending on
 * the check level.
 *
 * @param pkt  Packet handle
 *
 * @retval 0 Packet is not valid
 * @retval 1 Packet is valid
 */
int odp_packet_is_valid(odp_packet_t pkt);

/**
 * Get printable value for an odp_packet_t
 *
 * @param hdl  odp_packet_t handle to be printed
 * @return     uint64_t value that can be used to print/display this
 *             handle
 *
 * @note This routine is intended to be used for diagnostic purposes
 * to enable applications to generate a printable value that represents
 * an odp_packet_t handle.
 */
uint64_t odp_packet_to_u64(odp_packet_t hdl);

/**
 * Get printable value for an odp_packet_seg_t
 *
 * @param hdl  odp_packet_seg_t handle to be printed
 * @return     uint64_t value that can be used to print/display this
 *             handle
 *
 * @note This routine is intended to be used for diagnostic purposes
 * to enable applications to generate a printable value that represents
 * an odp_packet_seg_t handle.
 */
uint64_t odp_packet_seg_to_u64(odp_packet_seg_t hdl);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
