/************
 *
 * Filename:  ssmem_user.c
 *
 * Purpose:   SSMEM user functions
 *
 * Notes:     This code is common to all images
 *
 *            DO NOT CREATE SEPARATE BOOT OR APP VERSIONS OF THIS CODE
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
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

/* linux kernel */
#if defined(__KERNEL__)
#include <linux/string.h>
#else
#include <string.h>
#endif

/* Local constants and enumerated types */

/* Local structures */


/* Functions */

/************
 *
 * Name:     ssmem_get
 *
 * Purpose:  get ssmem address and size for a region
 *
 * Parms:    region_id - region id
 *           version   - user data version
 *           sizep     - output buffer to store user data size
 *
 * Return:   region user data addres or NULL if region is not valid
 *
 * Abort:    None
 *
 * Notes:
 *
 ************/
_global void *ssmem_get(
  int      region_id,
  uint16_t version,
  int     *sizep)
{
  struct ssmem_alloc_entry_s *entryp;
  struct ssmem_region_header_s *headerp;
  uint8_t *regionp = NULL;
  boolean valid_flag = FALSE;

  ssmem_framework_one_time_init();

  do
  {
    entryp = ssmem_alloc_entry_get(region_id);
    if (!entryp)
    {
      SWI_PRINT(SWI_ERROR, "ssmem_get: region %d not exists", region_id);
      break;
    }

    /* get region start address */
    regionp = ssmem_smem_base_addr_get();
    if (!regionp)
    {
      break;
    }

    regionp += entryp->offset;


    if (entryp->flags & SSMEM_RG_FLAG_PROPRIETARY)
    {
      /* priperietary data structure, no version and CRC check */
      if (sizep)
      {
        *sizep = entryp->size;
      }

      valid_flag = TRUE;
      break;
    }
    else /* SSMEM compliant structure */
    {
      /* validate version and CRC and other metadata */
      if (!ssmem_priv_meta_check(regionp, region_id, version, NULL))
      {
        break;
      }

      headerp = (struct ssmem_region_header_s *)regionp;

      /* get user data size and address */
      if (sizep)
      {
        *sizep = headerp->user_size;
      }

      /* adjust regionp to point to user data */
      regionp += SSMEM_FRAMEWORK_HEADER_SZ;

      valid_flag = TRUE;

    } /* end if SSMEM compliant structure */


  } while (0);

  if (!valid_flag)
  {
    regionp = NULL;
  }

  return regionp;
}

/************
 *
 * Name:     ssmem_acquire
 *
 * Purpose:  acquire a SSMEM region with specified user data version and size
 *           It will also zero initialize the region (simiar to calloc)
 *
 * Parms:    region_id - region id
 *           version   - user data version
 *           size      - user data size
 *
 * Return:   region user data addres or NULL if cannot acquire region
 *
 * Abort:    None
 *
 * Notes:
 *
 ************/
_global void *ssmem_acquire(
  int       region_id,
  uint16_t  version,
  int       size)
{
  struct ssmem_alloc_entry_s *entryp;
  uint8_t *regionp = NULL;
  int region_sz;
  boolean valid_flag = FALSE, skip_alloc = FALSE;

  ssmem_framework_one_time_init();

  do
  {
    /* get region start address */
    regionp = ssmem_smem_base_addr_get();
    if (!regionp)
    {
      break;
    }

    /* requested region size */
    region_sz = size + SSMEM_META_TOTAL_SZ;

    /* check if region is allocated already */
    entryp = ssmem_alloc_entry_get(region_id);
    if (entryp)
    {
      /* already allocated, check if it is big enough */
      if (entryp->flags & SSMEM_RG_FLAG_PROPRIETARY)
      {
        /* region size is user size for this case */
        region_sz = size;
      }

      if (entryp->size >= region_sz)
      {
        /* reuse this region but will zero initialize the region */
        regionp += entryp->offset;
        memset(regionp, 0, entryp->size);
        region_sz = entryp->size;
        skip_alloc = TRUE;
      }
      else if (entryp->flags & SSMEM_RG_FLAG_PROPRIETARY)
      {
        /* no dynamic allocation support for proprietary regions, fail */
        break;
      }
    }

    if (!skip_alloc)
    {
      /* add some padding */
      region_sz += SSMEM_PADDING_SZ;

      /* make sure region size is muiltple of ALIGN_SZ */
      if (region_sz % SSMEM_ALIGN_SZ)
      {
        region_sz += (SSMEM_ALIGN_SZ - (region_sz % SSMEM_ALIGN_SZ));
      }

      /* allocate the region from the heap */
      regionp = ssmem_heap_alloc(region_sz);

      if (regionp)
      {
        entryp = ssmem_alloc_entry_add(region_id, regionp, region_sz);
        SWI_PRINT(SWI_MED, "ssmem region %d added, size %d",
                  region_id, region_sz);
      }

      if (!regionp || !entryp)
      {
        SWI_PRINT(SWI_ERROR, "ssmem region %d allocation failed", region_id);
        break;
      }

      /* zero init the allocated buffer */
      memset(regionp, 0, region_sz);
    } /* end if dynamic allocation */

    /* for SSMEM compliant structure */
    if (!(entryp->flags & SSMEM_RG_FLAG_PROPRIETARY))
    {
      /* fill in meta data */
      if (!ssmem_priv_meta_update(regionp, region_id, region_sz, version, size))
      {
        break;
      }

      /* adjust regionp to point to user data */
      regionp += SSMEM_FRAMEWORK_HEADER_SZ;
    }

    valid_flag = TRUE;

  } while (0);

  if (!valid_flag)
  {
    regionp = NULL;
  }

  return regionp;
}

/************
 *
 * Name:     ssmem_meta_update
 *
 * Purpose:  update metadata of the region
 *
 * Parms:    region_id - region id
 *
 * Return:   TRUE if updated OK
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    None
 *
 ************/
_global boolean ssmem_meta_update(
  int       region_id)
{
  struct ssmem_alloc_entry_s *entryp;
  struct ssmem_region_header_s *headerp;
  uint8_t *regionp = NULL;
  boolean valid_flag = FALSE;

  do
  {
    entryp = ssmem_alloc_entry_get(region_id);
    if (!entryp)
    {
      SWI_PRINT(SWI_ERROR, "ssmem_update: region %d not exists", region_id);
      break;
    }

    /* get region start address */
    regionp = ssmem_smem_base_addr_get();
    if (!regionp)
    {
      break;
    }

    regionp += entryp->offset;


    if (entryp->flags & SSMEM_RG_FLAG_PROPRIETARY)
    {
      /* priperietary data structure, no need to update */
      valid_flag = TRUE;
      break;
    }
    else /* SSMEM compliant structure */
    {
      headerp = (struct ssmem_region_header_s *)regionp;

      /* fill in meta data, user version and size are not provided,
       * use existing one
       */
      if (!ssmem_priv_meta_update(regionp, region_id, entryp->size,
                                  headerp->user_version, headerp->user_size))
      {
        break;
      }

      valid_flag = TRUE;

    } /* end if SSMEM compliant structure */


  } while (0);

  return valid_flag;
}

/************
 *
 * Name:     ssmem_release
 *
 * Purpose:  release a SSMEM region
 *
 * Parms:    region_id - region id
 *
 * Return:   TRUE if released OK
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    It will not release memory but just invalidate the data
 *           This API is rarely used.
 *           It is only used if user want to invalidate the region explicitly
 *
 ************/
_global boolean ssmem_release(
  int       region_id)
{
  struct ssmem_alloc_entry_s *entryp;
  uint8_t *regionp = NULL;
  boolean valid_flag = FALSE;

  do
  {
    entryp = ssmem_alloc_entry_get(region_id);
    if (!entryp)
    {
      SWI_PRINT(SWI_ERROR, "ssmem_release: region %d not exists", region_id);
      break;
    }

    /* get region start address */
    regionp = ssmem_smem_base_addr_get();
    if (!regionp)
    {
      break;
    }

    regionp += entryp->offset;


    if (entryp->flags & SSMEM_RG_FLAG_PROPRIETARY)
    {
      /* priperietary data structure, no way to invalidate */
      valid_flag = TRUE;
      break;
    }
    else /* SSMEM compliant structure */
    {
      /* zero crc so that next ssmem_get will fail */
      memset(regionp, 0, entryp->size);

      valid_flag = TRUE;

    } /* end if SSMEM compliant structure */


  } while (0);

  return valid_flag;
}

