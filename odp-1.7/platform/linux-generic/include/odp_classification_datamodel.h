/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Classification Datamodel
 * Describes the classification internal data model
 */

#ifndef ODP_CLASSIFICATION_DATAMODEL_H_
#define ODP_CLASSIFICATION_DATAMODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/spinlock.h>
#include <odp/classification.h>
#include <odp_pool_internal.h>
#include <odp_packet_internal.h>
#include <odp_queue_internal.h>

/* Maximum Class Of Service Entry */
#define ODP_COS_MAX_ENTRY		64
/* Maximum PMR Set Entry */
#define ODP_PMRSET_MAX_ENTRY		64
/* Maximum PMR Entry */
#define ODP_PMR_MAX_ENTRY		64
/* Maximum PMR Terms in a PMR Set */
#define ODP_PMRTERM_MAX			8
/* Maximum PMRs attached in PKTIO Level */
#define ODP_PKTIO_MAX_PMR		8
/* L2 Priority Bits */
#define ODP_COS_L2_QOS_BITS		3
/* Max L2 QoS value */
#define ODP_COS_MAX_L2_QOS		(1 << ODP_COS_L2_QOS_BITS)
/* L2 DSCP Bits */
#define ODP_COS_L3_QOS_BITS		6
/* Max L3 QoS Value */
#define ODP_COS_MAX_L3_QOS		(1 << ODP_COS_L3_QOS_BITS)
/* Max PMR Term bits */
#define ODP_PMR_TERM_BYTES_MAX		8

#define MAX_CLS_SUPPORT			17 /* 16 queue cls + 1 global cls */
#define GLOBAL_CLS_ID			(MAX_CLS_SUPPORT - 1)


/**
Packet Matching Rule Term Value

Stores the Term and Value mapping for a PMR.
The maximum size of value currently supported in 64 bits
**/
typedef struct pmr_term_value {
	odp_pmr_term_t  term;	/* PMR Term */
	uint64_t	val;	/**< Value to be matched */
	uint64_t	mask;	/**< Masked set of bits to be matched */
	uint32_t	offset;	/**< Offset if term == ODP_PMR_CUSTOM_FRAME */
	uint32_t	val_sz;	/**< Size of the value to be matched */
} pmr_term_value_t;

/*
Class Of Service
*/
struct cos_s {
	queue_entry_t *queue;		/* Associated Queue */
	pool_entry_t *pool;		/* Associated Buffer pool */
	union pmr_u *pmr;		/* Chained PMR */
	union cos_u *linked_cos;	/* CoS linked with the PMR */
	uint32_t valid;			/* validity Flag */
	odp_cls_drop_t drop_policy;		/* Associated Drop Policy */
	odp_queue_group_t queue_group;	/* Associated Queue Group */
	odp_cos_flow_set_t flow_set;	/* Assigned Flow Set */
	char name[ODP_COS_NAME_LEN];	/* name */
	size_t headroom;		/* Headroom for this CoS */
	odp_spinlock_t lock;		/* cos lock */
};

typedef union cos_u {
	struct cos_s s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct cos_s))];
} cos_t;


/**
Packet Matching Rule

**/
struct pmr_s {
	uint32_t valid;			/* Validity Flag */
	odp_atomic_u32_t count;		/* num of packets matching this rule */
	uint32_t num_pmr;		/* num of PMR Term Values*/
	odp_spinlock_t lock;		/* pmr lock*/
	pmr_term_value_t  pmr_term_value[1];	/* Associated PMR Term */
};

typedef union pmr_u {
	struct pmr_s s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct pmr_s))];
} pmr_t;

/**
Packet Matching Rule Set

This structure is implemented as a extension over struct pmr_s
In order to use same pointer to access both pmr_s and pmr_set_s
'num_pmr' value is used to differentiate between pmr_s and pmr_set_s struct
**/
struct pmr_set_s {
	pmr_t pmr;
	pmr_term_value_t  pmr_term_value[ODP_PMRTERM_MAX - 1];
			/* List of associated PMR Terms */
};

typedef union pmr_set_u {
	struct pmr_set_s s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct pmr_set_s))];
} pmr_set_t;

/**
L2 QoS and CoS Map

This structure holds the mapping between L2 QoS value and
corresponding cos_t object
**/
typedef struct pmr_l2_cos {
	odp_spinlock_t lock;	/* pmr_l2_cos lock */
	cos_t *cos[ODP_COS_MAX_L2_QOS];	/* Array of CoS objects */
} pmr_l2_cos_t;

/**
L3 QoS and CoS Map

This structure holds the mapping between L3 QoS value and
corresponding cos_t object
**/
typedef struct pmr_l3_cos {
	odp_spinlock_t lock;	/* pmr_l3_cos lock */
	cos_t *cos[ODP_COS_MAX_L3_QOS];	/* Array of CoS objects */
} pmr_l3_cos_t;

/**
Linux Generic Classifier

This structure is stored in pktio_entry and holds all
the classifier configuration value.
**/
typedef struct classifier {
	odp_spinlock_t lock;		/*pktio_cos lock */
	uint32_t num_pmr;		/* num of PMRs linked to given PKTIO*/
	pmr_t *pmr[ODP_PKTIO_MAX_PMR];	/* PMRs linked with this PKTIO */
	cos_t *cos[ODP_PKTIO_MAX_PMR];	/* CoS linked with this PKTIO */
	cos_t *error_cos;		/* Associated Error CoS */
	cos_t *default_cos;		/* Associated Default CoS */
	uint32_t l3_precedence;		/* L3 QoS precedence */
	pmr_l2_cos_t l2_cos_table;	/* L2 QoS-CoS table map */
	pmr_l3_cos_t l3_cos_table;	/* L3 Qos-CoS table map */
	odp_cos_flow_set_t flow_set;	/* Flow Set to be calculated
					for this pktio */
	size_t headroom;		/* Pktio Headroom */
	size_t skip;			/* Pktio Skip Offset */
} classifier_t;

/**
Class of Service Table
**/
typedef struct odp_cos_table {
	cos_t cos_entry[ODP_COS_MAX_ENTRY];
} cos_tbl_t;

/**
PMR set table
**/
typedef struct pmr_set_tbl {
	pmr_set_t pmr_set[ODP_PMRSET_MAX_ENTRY];
} pmr_set_tbl_t;

/**
PMR table
**/
typedef struct pmr_tbl {
	pmr_t pmr[ODP_PMR_MAX_ENTRY];
} pmr_tbl_t;

#ifdef __cplusplus
}
#endif
#endif
