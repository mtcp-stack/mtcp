/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP byteorder
 */

#ifndef ODP_API_BYTEORDER_H_
#define ODP_API_BYTEORDER_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup odp_compiler_optim ODP COMPILER / OPTIMIZATION
 *  Macros that check byte order and operations for byte order conversion.
 *  @{
 */

/**
 * @def ODP_BIG_ENDIAN
 * Big endian byte order
 *
 * @def ODP_LITTLE_ENDIAN
 * Little endian byte order
 *
 * @def ODP_BIG_ENDIAN_BITFIELD
 * Big endian bit field
 *
 * @def ODP_LITTLE_ENDIAN_BITFIELD
 * Little endian bit field
 *
 * @def ODP_BYTE_ORDER
 * Selected byte order
 */

/**
 * @typedef odp_u16le_t
 * unsigned 16bit little endian
 *
 * @typedef odp_u16be_t
 * unsigned 16bit big endian
 *
 * @typedef odp_u32le_t
 * unsigned 32bit little endian
 *
 * @typedef odp_u32be_t
 * unsigned 32bit big endian
 *
 * @typedef odp_u64le_t
 * unsigned 64bit little endian
 *
 * @typedef odp_u64be_t
 * unsigned 64bit big endian
 *
 * @typedef odp_u16sum_t
 * unsigned 16bit bitwise
 *
 * @typedef odp_u32sum_t
 * unsigned 32bit bitwise
 */

/*
 * Big Endian -> CPU byte order:
 */

/**
 * Convert 16bit big endian to cpu native uint16_t
 * @param be16  big endian 16bit
 * @return  cpu native uint16_t
 */
uint16_t odp_be_to_cpu_16(odp_u16be_t be16);

/**
 * Convert 32bit big endian to cpu native uint32_t
 * @param be32  big endian 32bit
 * @return  cpu native uint32_t
 */
uint32_t odp_be_to_cpu_32(odp_u32be_t be32);

/**
 * Convert 64bit big endian to cpu native uint64_t
 * @param be64  big endian 64bit
 * @return  cpu native uint64_t
 */
uint64_t odp_be_to_cpu_64(odp_u64be_t be64);


/*
 * CPU byte order -> Big Endian:
 */

/**
 * Convert cpu native uint16_t to 16bit big endian
 * @param cpu16  uint16_t in cpu native format
 * @return  big endian 16bit
 */
odp_u16be_t odp_cpu_to_be_16(uint16_t cpu16);

/**
 * Convert cpu native uint32_t to 32bit big endian
 * @param cpu32  uint32_t in cpu native format
 * @return  big endian 32bit
 */
odp_u32be_t odp_cpu_to_be_32(uint32_t cpu32);

/**
 * Convert cpu native uint64_t to 64bit big endian
 * @param cpu64  uint64_t in cpu native format
 * @return  big endian 64bit
 */
odp_u64be_t odp_cpu_to_be_64(uint64_t cpu64);


/*
 * Little Endian -> CPU byte order:
 */

/**
 * Convert 16bit little endian to cpu native uint16_t
 * @param le16  little endian 16bit
 * @return  cpu native uint16_t
 */
uint16_t odp_le_to_cpu_16(odp_u16le_t le16);

/**
 * Convert 32bit little endian to cpu native uint32_t
 * @param le32  little endian 32bit
 * @return  cpu native uint32_t
 */
uint32_t odp_le_to_cpu_32(odp_u32le_t le32);

/**
 * Convert 64bit little endian to cpu native uint64_t
 * @param le64  little endian 64bit
 * @return  cpu native uint64_t
 */
uint64_t odp_le_to_cpu_64(odp_u64le_t le64);


/*
 * CPU byte order -> Little Endian:
 */

/**
 * Convert cpu native uint16_t to 16bit little endian
 * @param cpu16  uint16_t in cpu native format
 * @return  little endian 16bit
 */
odp_u16le_t odp_cpu_to_le_16(uint16_t cpu16);

/**
 * Convert cpu native uint32_t to 32bit little endian
 * @param cpu32  uint32_t in cpu native format
 * @return  little endian 32bit
 */
odp_u32le_t odp_cpu_to_le_32(uint32_t cpu32);

/**
 * Convert cpu native uint64_t to 64bit little endian
 * @param cpu64  uint64_t in cpu native format
 * @return  little endian 64bit
 */
odp_u64le_t odp_cpu_to_le_64(uint64_t cpu64);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
