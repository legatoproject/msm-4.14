/************
 *
 * Filename:   ssmem_utils.c
 *
 * Purpose:    ssmem utility functions
 *
 * Notes:     This code is common to all images
 *
 *            DO NOT CREATE SEPARATE BOOT OR APP VERSIONS OF THIS CODE
 *
 * Copyright:  (c) 2016 Sierra Wireless, Inc.
 *             All rights reserved
 *
 ************/

/* Include files */
/* for LK or Linux kernel */
#if defined(SWI_IMAGE_LK) || defined(__KERNEL__)
#include "aaglobal_linux.h"
#include "aadebug_linux.h"
#else
#include "aaglobal.h"
#include "aadebug.h"
#endif
#include "ssmemidefs.h"

#ifdef SWI_IMAGE_LK
#include <crc32.h>
#elif defined(__KERNEL__)
#include <linux/crc32.h>
#else
#include "crudefs.h"
#endif


/* Local constants and enumerated types */

/* Local structures */


/* Functions */

/************
 *
 * Name:     ssmem_priv_meta_check
 *
 * Purpose:  metadata check for a region
 *
 * Parms:    regionp   - region start address
 *           region_id - region id
 *           user_ver  - user data version
 *           sizep     - output buffer to store region total size
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:
 *
 ************/
_package boolean ssmem_priv_meta_check(
  uint8_t  *regionp,
  int       region_id,
  uint16_t  user_ver,
  int      *sizep)
{
  struct ssmem_region_header_s *headerp;
  uint32_t crc, region_sz, *crcp;
  boolean valid_flag = FALSE;

  do
  {
    if (!regionp)
    {
      break;
    }

    headerp = (struct ssmem_region_header_s *)regionp;

    /* validate region ID and framework version */
    if ((headerp->region_id != region_id) ||
        ((headerp->framew_version & SSMEM_VER_MAJOR_BM) !=
         (SSMEM_FRAMEWORK_VERSION & SSMEM_VER_MAJOR_BM)))
    {
      SWI_PRINT(SWI_ERROR, "ssmem region %d != %d invalid framew ver %d",
                region_id, headerp->region_id, headerp->framew_version);
      break;
    }

    /* check the length to make sure it will not exceed SSMEM boundary
     * only a rough check is conducted here
     * double check user size
     */
    if (((regionp + headerp->length) >= ssmem_smem_end_addr_get()) ||
        (headerp->user_size > headerp->length))
    {
      SWI_PRINT(SWI_ERROR, "ssmem region %d invalid length:%d,%d",
                region_id, headerp->length, headerp->user_size);
      break;
    }

    /* check user version (only if provided user_ver > 0) */
    if (user_ver && (headerp->user_version != user_ver))
    {
      SWI_PRINT(SWI_HIGH, "ssmem region %d version not match:%x,%x",
                region_id, headerp->user_version, user_ver);

      if ((headerp->user_version & SSMEM_VER_MAJOR_BM) !=
          (user_ver & SSMEM_VER_MAJOR_BM))
      {
        SWI_PRINT(SWI_ERROR, "ssmem region %d user versin check failed",
                  region_id);
        break;
      }
    }

    /* double check CRC */
    region_sz = headerp->length + SSMEM_REGION_SZ_LENGTH_DIFF;
    crc = ssmem_priv_crc32(regionp, region_sz - (sizeof(uint32_t)));
    /* crcp can be declared as uint32 pointer since it is properly aligned */
    crcp = (uint32_t *)(regionp + region_sz - (sizeof(uint32_t)));
    if (crc != *crcp)
    {
      SWI_PRINT(SWI_ERROR, "ssmem region %d crc check failed:%x != %x",
                region_id, crc, *crcp);
      break;
    }

    valid_flag = TRUE;

    if (sizep)
    {
      *sizep = region_sz;
    }

  } while (0);

  return valid_flag;
}

/************
 *
 * Name:     ssmem_priv_meta_update
 *
 * Purpose:  update meta data for a region
 *
 * Parms:    regionp      - region start addr
 *           region_id    - region ID
 *           region_sz    - region total size
 *           user_version - user data version
 *           user_size    - user data size
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
_package boolean ssmem_priv_meta_update(
  uint8_t *regionp,
  int      region_id,
  int      region_sz,
  uint16_t user_version,
  int      user_size)
{
  struct ssmem_region_header_s *headerp;
  uint32_t crc;

  if (((user_size + SSMEM_META_TOTAL_SZ) > region_sz) ||
      (region_sz % SSMEM_ALIGN_SZ))
  {
    /* cannot fit into the region, or region size is not properly aligned */
    SWI_PRINT(SWI_ERROR, "invalid ssmem region %d size, %d:%d",
              region_id, region_sz, user_size);
    return FALSE;
  }

  headerp = (struct ssmem_region_header_s *)regionp;
  headerp->region_id = region_id;
  headerp->framew_version = SSMEM_FRAMEWORK_VERSION;
  headerp->length = region_sz - SSMEM_REGION_SZ_LENGTH_DIFF;
  headerp->user_version = user_version;
  headerp->user_size = user_size;

  /* calculate CRC on whole region except CRC32 itself */
  crc = ssmem_priv_crc32(regionp, region_sz - (sizeof(uint32_t)));

  /* save crc, assume the address to store crc at the end of the region is 4
   * byte aligned so we can do a direct int assign here.
   * both start of region and region size are properly aligned
   */
  *(uint32_t *)(regionp + region_sz - (sizeof(uint32_t))) = crc;

  return TRUE;
}

/************
 *
 * Name:     ssmem_priv_crc32
 *
 * Purpose:  CRC32 function
 *
 * Parms:    msgp - data
 *           size - size of data
 *
 * Return:   CRC32 of the data
 *
 * Abort:    None
 *
 * Notes:
 *
 ************/
_package uint32_t ssmem_priv_crc32(
  uint8_t *msgp,
  int      size)
{
#ifdef SWI_IMAGE_LK
  return crc32(~0, msgp, size);
#elif defined(__KERNEL__)
  return crc32_le(~0, msgp, size);
#else /* amss */
  return crcrc32(msgp, size, CRSTART_CRC32);
#endif
}
