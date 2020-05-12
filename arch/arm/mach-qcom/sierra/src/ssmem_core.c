/************
 *
 * Filename:  ssmem_core.c
 *
 * Purpose:   core functions to provide SSMEM framework
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

/* Local constants and enumerated types */

/* Local structures */
_local struct ssmem_alloc_entry_s *ssmem_alloc_tablep = NULL;
_local int ssmem_alloc_table_size = 0;
_local struct ssmem_spinlock_array_s *ssmem_spinlock_arrayp = NULL;
_local struct ssmem_heap_info_s *ssmem_heap_infop = NULL;

/* Functions */

/************
 *
 * Name:     ssmem_smem_end_addr_get
 *
 * Purpose:  get end address SSMEM area
 *
 * Parms:    none
 *
 * Return:   end address of SSMEM
 *
 * Abort:    None
 *
 * Notes:
 *
 ************/
_package uint8_t *ssmem_smem_end_addr_get(
  void)
{
  return ssmem_smem_base_addr_get() + SSMEM_MEM_SIZE;
}

/************
 *
 * Name:     ssmem_framework_version_check
 *
 * Purpose:  check framework version
 *
 * Parms:    regionp - region start addr
 *           sizep   - output buffer to store region size
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    none
 *
 * Notes:    will check major versions and return faile if not match
 *           Called as part of framework initializaiton
 *
 ************/
_local boolean ssmem_framework_version_check(
  uint8_t *regionp,
  int     *sizep)
{
  struct ssmem_version_info_s *verp;
  struct ssmem_region_header_s *headerp;
  uint16_t version, idx, max_idx;

  headerp = (struct ssmem_region_header_s *)regionp;
  verp = (struct ssmem_version_info_s *)(regionp + SSMEM_FRAMEWORK_HEADER_SZ);

  max_idx = SSMEM_VER_IDX_MAX;
  /* in case there are less valid versions */
  if ((headerp->user_size > sizeof(uint16_t)) &&
      (headerp->user_size < (max_idx * sizeof(uint16_t))))
  {
    max_idx = headerp->user_size / sizeof(uint16_t);
  }

  for( idx = 0; idx < max_idx; idx++ )
  {
    version = verp->versions[idx] & SSMEM_VER_MAJOR_BM;

    /* If a version exists then compare the major numbers */
    if( (version != 0) &&
        (version != (SSMEM_FRAMEWORK_VERSION & SSMEM_VER_MAJOR_BM)) )
    {
      return FALSE;
    }
  }

  /* check crc and get region size */
  return ssmem_priv_meta_check(regionp, SSMEM_RG_ID_VERSION_INFO,
                               SSMEM_FRAMEWORK_VERSION, sizep);
}

/************
 *
 * Name:     ssmem_heap_alloc
 *
 * Purpose:  allocate buffer from heap
 *
 * Parms:    buf_size - size of the buffer requested
 *
 * Return:   pionter to the buffer requested
 *
 * Abort:    none
 *
 * Notes:    buf_size is already properly aligned
 *           Must be called after framework initialization
 *
 ************/
_package void *ssmem_heap_alloc(
  int    buf_size)
{
  int  remaining, offset;
  void   *result = NULL;

  if (!ssmem_heap_infop ||
      !ssmem_heap_infop->initialized)
  {
    return NULL;
  }


  if (!ssmem_spin_lock(SSMEM_SPINLOCK_ID_FRAMEWK))
  {
    return NULL;
  }

  remaining = ssmem_heap_infop->heap_remaining;

  if( buf_size <= remaining )
  {
    offset = ssmem_heap_infop->free_offset;

    /* Update heap info */
    ssmem_heap_infop->free_offset    = offset + buf_size;
    ssmem_heap_infop->heap_remaining = remaining - buf_size;

    /* update meta data for the heap info region */
    if (ssmem_meta_update(SSMEM_RG_ID_HEAP_INFO))
    {
      result = ssmem_smem_base_addr_get() + offset;
    }
  }

  ssmem_spin_unlock( SSMEM_SPINLOCK_ID_FRAMEWK );

  return result;
}

/************
 *
 * Name:     ssmem_alloc_table_check
 *
 * Purpose:  check and get allocation table
 *
 * Parms:    regionp - region start addr
 *           sizep   - output buffer to store region size
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    none
 *
 * Notes:    will update global var ssmem_alloc_tablep
 *           Called as part of framework initializaiton
 *
 ************/
_local boolean ssmem_alloc_table_check(
  uint8_t *regionp,
  int     *sizep)
{
  struct ssmem_region_header_s *headerp;

  if (!ssmem_priv_meta_check(regionp, SSMEM_RG_ID_ALLOCATION_TABLE,
                             SSMEM_FRAMEWORK_VERSION, sizep))
  {
    return FALSE;
  }

  headerp = (struct ssmem_region_header_s *)regionp;
  ssmem_alloc_tablep = (struct ssmem_alloc_entry_s *)
                              (regionp + SSMEM_FRAMEWORK_HEADER_SZ);

  ssmem_alloc_table_size = headerp->user_size / SSMEM_ALLOCATION_ENTRY_SZ;

  return TRUE;
}

/************
 *
 * Name:     ssmem_alloc_entry_get
 *
 * Purpose:  get alloc info for a region
 *
 * Parms:    region_id - region ID
 *
 * Return:   alloc entry for the region
 *           or NULL if the region hasn't been allocated yet
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
_package struct ssmem_alloc_entry_s *ssmem_alloc_entry_get(
  int region_id)
{
  struct ssmem_alloc_entry_s *entryp;

  if (ssmem_alloc_tablep && (region_id < ssmem_alloc_table_size))
  {
    entryp = &(ssmem_alloc_tablep[region_id]);

    if (entryp->allocated)
    {
      return entryp;
    }
  }

  SWI_PRINT(SWI_MED, "ssmem region %d not allocated", region_id);
  return NULL;
}

/************
 *
 * Name:     ssmem_alloc_entry_add
 *
 * Purpose:  add/overwrite an entry when allocating dynamic regions
 *
 * Parms:    region_id - region ID
 *           regionp   - region start address
 *           region_sz - size of the region
 *
 * Return:   address of newly added entry or NULL if cannot add
 *
 * Abort:    none
 *
 * Notes:    need spinlock before updating allocation table
 *           Must be called after framework initialization
 *
 ************/
_package struct ssmem_alloc_entry_s *ssmem_alloc_entry_add(
  int      region_id,
  uint8_t *regionp,
  int      region_sz)
{
  struct ssmem_alloc_entry_s *entryp;

  if (!ssmem_alloc_tablep ||
     (region_id >= ssmem_alloc_table_size) ||
     (regionp < ssmem_smem_base_addr_get()) ||
     (regionp >= ssmem_smem_end_addr_get()))
  {
    SWI_PRINT(SWI_ERROR, "ssmem entry add invalid para");
    return NULL;
  }

  if (!ssmem_spin_lock(SSMEM_SPINLOCK_ID_FRAMEWK))
  {
    return NULL;
  }

  entryp = &(ssmem_alloc_tablep[region_id]);

  entryp->offset = regionp - ssmem_smem_base_addr_get();
  entryp->size = region_sz;
  entryp->allocated = SSMEM_ALLOCATION_ENTRY_ALLOCATED;

  /* update meta data for allocation table region */
  if (!ssmem_meta_update(SSMEM_RG_ID_ALLOCATION_TABLE))
  {
    entryp = NULL;
  }

  ssmem_spin_unlock( SSMEM_SPINLOCK_ID_FRAMEWK );

  return entryp;
}

/************
 *
 * Name:     ssmem_framework_init
 *
 * Purpose:  initialize ssmem
 *
 * Parms:    none
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    This function will only do ready only access to framework regions
 *
 ************/
_package boolean ssmem_framework_init(
  void)
{
  uint8_t *base_addrp, *regionp;
  int region_size, size;
  boolean retval = FALSE;

  do
  {
    /* detect start of SMEM */
    base_addrp = ssmem_smem_base_addr_get();

    if (!base_addrp ||
        (*((uint32_t *)base_addrp) != SSMEM_MAGIC_NUMBER))
    {
      SWI_PRINT(SWI_ERROR, "cannot detect SSMEM base");
      break;
    }

    /* framework version check
     * assuming there is no gaps between first several regions:
     * magic, version info, allocation table
     */
    regionp = base_addrp + SSMEM_RG_SZ_MAGIC;
    if (!ssmem_framework_version_check(regionp, &region_size))
    {
      SWI_PRINT(SWI_ERROR, "SSMEM framework version check failed");
      break;
    }

    /* allocation table check */
    regionp += region_size;
    if (!ssmem_alloc_table_check(regionp, &region_size))
    {
      SWI_PRINT(SWI_ERROR, "SSMEM cannot get allocation table");
      break;
    }

    ssmem_heap_infop = ssmem_get(SSMEM_RG_ID_HEAP_INFO,
                                 SSMEM_FRAMEWORK_VERSION, &size);

    ssmem_spinlock_arrayp = ssmem_get(SSMEM_RG_ID_SPINLOCK_ARRAY,
                                      SSMEM_FRAMEWORK_VERSION, &size);

    if (!ssmem_heap_infop ||
        !(ssmem_heap_infop->initialized) ||
        !ssmem_spinlock_arrayp)
    {
      SWI_PRINT(SWI_ERROR, "SSMEM heap and spinlock region invalid");
      break;
    }

    ssmem_spin_lock_init();

    /* Set framework version for current subsystem?
     * This will not buy us anything, compare against boot version is enough
     * boot version is set at ssmem_framework_version_set
     */

    SWI_PRINT(SWI_HIGH, "SSMEM init OK");
    retval = TRUE;

  } while (0);

  return retval;
}

/************
 *
 * Name:     ssmem_framework_one_time_init
 *
 * Purpose:  one time init of the framework
 *
 * Parms:    none
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    If initialization failed:
 *           SBL - will re-initialize SSMEM regions
 *           other - will leave it uninitialized and all ssmem call will fail
 *
 ************/
_package boolean ssmem_framework_one_time_init(
  void)
{
  static boolean init_called = FALSE, retval;

  if (!init_called)
  {
    init_called = TRUE;
#ifdef SWI_IMAGE_BOOT
    retval = ssmem_boot_init();
#else
    retval = ssmem_framework_init();
#endif /* !SWI_IMAGE_BOOT */
  }

  return retval;
}
