/* Copyright (c) 2013-2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP shared memory
 */

#ifndef ODP_API_SHARED_MEMORY_H_
#define ODP_API_SHARED_MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif


/** @defgroup odp_shared_memory ODP SHARED MEMORY
 *  Operations on shared memory.
 *  @{
 */

/**
 * @typedef odp_shm_t
 * ODP shared memory block
 */

/**
 * @def ODP_SHM_INVALID
 * Invalid shared memory block
 */

/**
 * @def ODP_SHM_NULL
 * Synonym for buffer pool use
 */

/** Maximum shared memory block name length in chars */
#define ODP_SHM_NAME_LEN 32

/*
 * Shared memory flags
 */

/* Share level */
#define ODP_SHM_SW_ONLY 0x1 /**< Application SW only, no HW access */
#define ODP_SHM_PROC    0x2 /**< Share with external processes */
#define ODP_SHM_CNTNUS_PHY    0x4 /**< Make sure the block's physical address is continuous */

/**
 * Shared memory block info
 */
typedef struct odp_shm_info_t {
	const char *name;      /**< Block name */
	void       *addr;      /**< Block address */
	uint64_t    size;      /**< Block size in bytes */
	uint64_t    page_size; /**< Memory page size */
	uint32_t    flags;     /**< ODP_SHM_* flags */
} odp_shm_info_t;


/**
 * Reserve a contiguous block of shared memory
 *
 * @param[in] name   Name of the block (maximum ODP_SHM_NAME_LEN - 1 chars)
 * @param[in] size   Block size in bytes
 * @param[in] align  Block alignment in bytes
 * @param[in] flags  Shared memory parameter flags (ODP_SHM_*).
 *                   Default value is 0.
 *
 * @return Handle of the reserved block
 * @retval ODP_SHM_INVALID on failure
 */
odp_shm_t odp_shm_reserve(const char *name, uint64_t size, uint64_t align,
			  uint32_t flags);

/**
 * Free a contiguous block of shared memory
 *
 * Frees a previously reserved block of shared memory.
 * @note Freeing memory that is in use will result in UNDEFINED behavior
 *
 * @param[in] shm Block handle
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_shm_free(odp_shm_t shm);

/**
 * Lookup for a block of shared memory
 *
 * @param[in] name   Name of the block
 *
 * @return A handle to the block if it is found by name
 * @retval ODP_SHM_INVALID on failure
 */
odp_shm_t odp_shm_lookup(const char *name);


/**
 * Shared memory block address
 *
 * @param[in] shm   Block handle
 *
 * @return Memory block address
 * @retval NULL on failure
 */
void *odp_shm_addr(odp_shm_t shm);


/**
 * Shared memory block info
 * @note This is the only shared memory API function which accepts invalid
 * shm handles (any bit value) without causing undefined behavior.
 *
 * @param[in]  shm   Block handle
 * @param[out] info  Block info pointer for output
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int odp_shm_info(odp_shm_t shm, odp_shm_info_t *info);


/**
 * Print all shared memory blocks
 */
void odp_shm_print_all(void);

/**
 * Get printable value for an odp_shm_t
 *
 * @param hdl  odp_shm_t handle to be printed
 * @return     uint64_t value that can be used to print/display this
 *             handle
 *
 * @note This routine is intended to be used for diagnostic purposes
 * to enable applications to generate a printable value that represents
 * an odp_shm_t handle.
 */
uint64_t odp_shm_to_u64(odp_shm_t hdl);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
