/************
 *
 * Filename:  ssmem_bscommon.c
 *
 * Purpose:   Common ssmem functions
 *
 * Notes:     This file will be used for all the images: boot/mpss/lk/kernel
 *
 * Copyright: (c) 2017 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/

 /* Include files */
#if defined(SWI_IMAGE_LK) || defined(__KERNEL__)
#include "aadebug_linux.h"
#else
#include "aadebug.h"
#endif

#include "cowork_ssmem_structure.h"
#include "ssmemudefs.h"

#if defined(__KERNEL__)
#include <linux/string.h>
#else
#include <string.h>
#endif

#ifdef SWI_IMAGE_BOOT
/* This is the default cowork message. */
_local const struct cowork_ssmem_s cowork_default =
{
  {0, 0},                   /* gpio_flags */
  {0, 0},                   /* uart_fun */
  0,                        /* ri_owner, 0 - Modem */
  0,                        /* sleep_ind */
  0,                        /* reset_type */
  {0, 0},                   /* reserved */
  0,                        /* boot_quiet */
  0,                        /* functions */
  0,                        /* mode, 0 - normal mode */
};
#endif

_local struct cowork_ssmem_s *coworkp = NULL;

/* Functions */

/************
 *
 * Name:     ssmem_cowork_get
 *
 * Purpose:  Get the pointer to the cowork region
 *
 * Parms:    None
 *
 * Return:   Pointer to cowork region
 *
 * Abort:    None
 *
 * Notes:    Acquire will only be attempted during
 *           boot.
 *
 ************/
_global struct cowork_ssmem_s *ssmem_cowork_get(
  void)
{
  static boolean cowork_ssmem_loaded = FALSE;
  int size;

  if (!cowork_ssmem_loaded)
  {
    coworkp = (struct cowork_ssmem_s *)ssmem_get(SSMEM_RG_ID_COWORK,
                                                 COWORK_SSMEM_VER,
                                                 &size);
    if (!coworkp)
    {
#ifndef SWI_IMAGE_BOOT
      SWI_PRINT(SWI_ERROR, "SSMEM: Error getting cowork region");
#else
      coworkp = (struct cowork_ssmem_s *)ssmem_acquire(SSMEM_RG_ID_COWORK,
                                                       COWORK_SSMEM_VER,
                                                       sizeof(struct cowork_ssmem_s));
      if (!coworkp)
      {
        SWI_PRINT(SWI_ERROR, "SSMEM: Error acquiring cowork region");
      }
      else
      {
        memmove(coworkp,
                &cowork_default,
                sizeof(struct cowork_ssmem_s));
        ssmem_meta_update(SSMEM_RG_ID_COWORK);
      }
#endif
    }
    cowork_ssmem_loaded = TRUE;
  }

  return coworkp;
}
