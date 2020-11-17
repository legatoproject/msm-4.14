/*************
 *
 * Filename:  cowork_ssmem_structure.h
 *
 * Purpose:   COWORK SSMEM structure defines
 *
 * Notes:     This file will be used for all the images: boot/mpss/lk/kernel
 *            so please keep it clean
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
 **************/
#ifndef COWORK_SSMEM_STRUCTURE_H
#define COWORK_SSMEM_STRUCTURE_H

/* Include files */
#if defined(SWI_IMAGE_LK) || defined(__KERNEL__)
#include "aaglobal_linux.h"
#else
#include "aaglobal.h"
#endif

/* Constants and enumerated types */
#define COWORK_SSMEM_VER    SSMEM_RG_USER_VER_1P0

/* Structure */
/************
 *
 * Name:     cowork_ssmem_s
 *
 * Purpose:  cowork shared memory region layout
 *
 * Notes:    This region has both cowork and mode
 *           flags from the old region.
 *
 ************/
PACKED struct PACKED_POST cowork_ssmem_s
{
  uint32_t gpio_flags[2];    /* External gpio owner flags. */
  uint8_t  uart_fun[2];      /* UART1 and UART2 function */
  uint8_t  ri_owner;         /* RI owner */
  uint8_t  sleep_ind;        /* Sleep indication function */
  uint8_t  reset_type;       /* Reset type */
  uint8_t  reserved[2];      /* Unused memory to align struct to 32 bit boundary */
  uint8_t  boot_quiet;       /* Indicate whether bootquiet is enabled or not */
  uint32_t functions;        /* Bitmask for functions configured by modem side(ex. HISC) */
  uint32_t mode;             /* gpio mode (normal or factory mode) */
};

/* Prototype for cowork region */
extern struct cowork_ssmem_s *ssmem_cowork_get(
  void);
#endif /* COWORK_SSMEM_STRUCTURE_H */
