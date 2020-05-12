/*************
 *
 * Filename:  ssmemudefs.h
 *
 * Purpose:   user definitions for ssmem
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 **************/
#ifndef SSMEMUDEFS_H
#define SSMEMUDEFS_H

/* Include files */
#if defined(SWI_IMAGE_LK) || defined(__KERNEL__)
#include "aaglobal_linux.h"
#else
#include "aaglobal.h"
#endif

/* Constants and enumerated types */
#define SSMEM_MAGIC_NUMBER                      0x5345524D
#define SSMEM_RG_SZ_MAGIC                       16

#define SSMEM_FRAMEWORK_VERSION                 0x0100
#define SSMEM_VER_IDX_MAX                       0x10
#define SSMEM_VER_IDX_SBL                       0x00
#define SSMEM_VER_MAJOR_BM                      0xFF00

#define SSMEM_RG_FLAG_PROPRIETARY               0x01

#define SSMEM_ALLOCATION_ENTRY_ALLOCATED        1

#define SSMEM_NUM_SPINLOCKS                     8
#define SSMEM_SPINLOCK_ID_FRAMEWK               0

#define SSMEM_PADDING_SZ                        0x08
#define SSMEM_ALIGN_SZ                          0x10

#define SEC_KEY_ID_LENGTH   (4)
#define SEC_OEM_KEY_LENGTH  (40)
#define SEC_OEM_KEY_MAX     (16)

/************
 *
 * Name:     ssmem_region_id_e
 *
 * Purpose:  To enumerate the region IDs
 *
 * Members:  See below
 *
 * Notes:    These IDs may be exposed to host driver so please
 *           don't modify existing region ID.
 *
 ************/
enum ssmem_region_id_e
{
  SSMEM_RG_ID_MIN                  = 0,
  SSMEM_RG_ID_MAGIC                = SSMEM_RG_ID_MIN,
  SSMEM_RG_ID_FRAMEWORK_START      = SSMEM_RG_ID_MAGIC,
  SSMEM_RG_ID_VERSION_INFO         = 1,
  SSMEM_RG_ID_ALLOCATION_TABLE     = 2,
  SSMEM_RG_ID_HEAP_INFO            = 3,
  SSMEM_RG_ID_SPINLOCK_ARRAY       = 4,
  SSMEM_RG_ID_FRAMEWORK_END        = SSMEM_RG_ID_SPINLOCK_ARRAY,
  SSMEM_RG_ID_CWE_HEADER           = 10,
  SSMEM_RG_ID_MSG_A2B              = 11,
  SSMEM_RG_ID_MSG_B2A              = 12,
  SSMEM_RG_ID_ERR_MPSS             = 13,
  SSMEM_RG_ID_ERR_APSS             = 14,
  SSMEM_RG_ID_USBD                 = 15,
  SSMEM_RG_ID_CACHE                = 16,
  SSMEM_RG_ID_EFS_LOG              = 17,
  SSMEM_RG_ID_FWUPDATE_STATUS      = 18,
  SSMEM_RG_ID_IMSW                 = 19,
  SSMEM_RG_ID_KEYS                 = 20,
  SSMEM_RG_ID_COWORK               = 21,
  SSMEM_RG_ID_LAST                 = SSMEM_RG_ID_COWORK,
  SSMEM_RG_ID_MAX                  = 100,
  SSMEM_RG_ID_INVALID,
};


/* Structures */

/************
 *
 * Name:     ssmem_region_header_s
 *
 * Purpose:  SSMEM region header
 *
 * Notes:    packed to work acrocss processors
 *
 ************/
PACKED struct PACKED_POST ssmem_region_header_s
{
  uint16_t         region_id;     /* region ID                */
  uint16_t         framew_version;/* framework version        */
  uint32_t         length;        /* length of following data */
  uint16_t         reserved;      /* padding for alignment    */
  uint16_t         user_version;  /* user data version        */
  uint32_t         user_size;     /* length of user data      */
};

#define SSMEM_FRAMEWORK_HEADER_SZ    sizeof(struct ssmem_region_header_s)
/* meta: header + CRC at the end */
#define SSMEM_META_TOTAL_SZ          (SSMEM_FRAMEWORK_HEADER_SZ + sizeof(uint32_t))
/* region size and length difference */
#define SSMEM_REGION_SZ_LENGTH_DIFF  8

/************
 *
 * Name:     ssmem_version_info_s
 *
 * Purpose:  SSMEM version info structure
 *
 * Notes:    packed to work acrocss processors
 *
 ************/
PACKED struct PACKED_POST ssmem_version_info_s
{
  uint16_t         versions[SSMEM_VER_IDX_MAX]; /* version info for subsystems */
};

/************
 *
 * Name:     ssmem_alloc_entry_s
 *
 * Purpose:  SSMEM allocation table structure
 *
 * Notes:    packed to work acrocss processors
 *
 ************/
PACKED struct PACKED_POST ssmem_alloc_entry_s
{
  uint32_t         allocated;     /* if the region is allocated */
  uint32_t         offset;        /* offset from start of SSMEM */
  uint32_t         size;          /* size of region including meta
                                     data and user data */
  uint32_t         flags;         /* region flags */
};

#define SSMEM_ALLOCATION_ENTRY_SZ  sizeof(struct ssmem_alloc_entry_s)
#define SSMEM_ALLOCATION_TABLE_SZ  (SSMEM_RG_ID_MAX * SSMEM_ALLOCATION_ENTRY_SZ)

/************
 *
 * Name:     ssmem_heap_info_s
 *
 * Purpose:  SSMEM heap info structure
 *
 * Notes:    packed to work acrocss processors
 *
 ************/
PACKED struct PACKED_POST ssmem_heap_info_s
{
  uint32_t initialized;         /* if this structure is initialized */
  uint32_t free_offset;         /* free offset from start of SSMEM  */
  uint32_t heap_remaining;      /* size of available memory         */
  uint32_t start_offset;        /* offset of heap start             */
};

/************
 *
 * Name:     ssmem_spinlock_array_s
 *
 * Purpose:  SSMEM spinlock array structure
 *
 * Notes:    packed to work acrocss processors
 *
 ************/
PACKED struct PACKED_POST ssmem_spinlock_array_s
{
  uint32_t locks[SSMEM_NUM_SPINLOCKS];  /* spinlocks for various regions */
};

/************
 *
 * Name:     ssmem_ioctl_req_s
 *
 * Purpose:  SSMEM driver IOCTL request structure
 *
 * Notes:    SSMEM driver and user space interface
 *
 ************/
struct ssmem_ioctl_req_s
{
  uint16_t         region_id;     /* region ID                */
  uint16_t         user_version;  /* user data version        */
  uint32_t         user_size;     /* length of user data      */
  uint8_t         *user_datap;    /* buffer for user data     */
};

/************
 *
 * Name:     sec_ssmem_key_hdr_s
 *
 * Purpose:  describe key header structure on flash
 *
 * Notes:    FileHold #41110272
 *
 ************/
typedef PACKED struct PACKED_POST
{
    uint8_t     version;                /* key version
                                             - 0: OEM key
                                             - 1: multiple key supported
                                         */

    uint8_t     type;                   /* key type
                                             - 0: public key sha256 digest
                                             - 1: public key DER encoded
                                             - 2: public key PEM format
                                         */

    uint16_t    length;                 /* key length */

    char        id[SEC_KEY_ID_LENGTH];  /* key id
                                             - "IMA0" - IMA .system key
                                             - "RFS0" - Rootfs dm-verity key
                                             - "LGT0" - Legato dm-verity key
                                         */
} sec_ssmem_key_hdr_s;

#define SSMEM_IOCTL_ACQUIRE          _IOWR('S', 0x10, struct ssmem_ioctl_req_s)
#define SSMEM_IOCTL_GET              _IOWR('S', 0x11, struct ssmem_ioctl_req_s)
#define SSMEM_IOCTL_UPDATE           _IOWR('S', 0x12, struct ssmem_ioctl_req_s)
#define SSMEM_IOCTL_RELEASE          _IOWR('S', 0x13, struct ssmem_ioctl_req_s)

#include "ssmem_user_regions.h"

#include "ssmemuproto.h"
#endif /* SSMEMUDEFS_H */

