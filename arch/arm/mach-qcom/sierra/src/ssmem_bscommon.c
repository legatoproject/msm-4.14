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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
