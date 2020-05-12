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
