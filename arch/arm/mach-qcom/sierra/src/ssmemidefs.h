/*************
 *
 * Filename:  ssmemidefs.h
 *
 * Purpose:   internal definitions for ssmem
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 **************/

/* Include files */
#include "ssmemudefs.h"
#include "ssmem_address.h"

/* Local constants and enumerated types */
#define SSMEM_MEM_BASE_ADDR          SIERRA_SMEM_BASE_PHY
#define SSMEM_MEM_SIZE               SIERRA_SMEM_SIZE

#define SSMEM_FRAMWORK_TOTAL_SIZE    2048
/* user regions can be right after framework regions but can be
 * also forced at other offset
 */
#define SSMEM_USER_REGION_OFFSET     0x80000

#define SSMEM_RG_SZ_VERSION_INFO     64
#define SSMEM_RG_SZ_ALLOCATION_TABLE 1632
#define SSMEM_RG_SZ_HEAP_INFO        64
#define SSMEM_RG_SZ_SPINLOCK_ARRAY   64

/* Structures, enums, etc. */


#include "ssmemiproto.h"
