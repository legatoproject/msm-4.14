/* arch/arm/mach-msm/sierra_bsidefs.h
 *
 * Copyright (C) 2016 Sierra Wireless, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef BS_IDEFS_H
#define BS_IDEFS_H
/* Local constants and enumerated types */

/* RAM Copies of HW type, rev, etc. */
extern bool bs_hwcfg_read;

/* Structures */
/************
 *
 * Name:     bs_hwconfig_s
 *
 * Purpose:  to allow easy access to fields of the hardware configuration
 *
 * Members:  see below
 *
 * Notes:
 *
 ************/
union bs_hwconfig_s
{
  uint32_t all;                 /* single uint32 containing all fields */
  struct __packed               /* struct of individual fields         */
  {
    uint8_t family;             /* hardware family                     */
    uint8_t type;               /* hardware type                       */
    uint8_t rev;                /* hardware revision                   */
    uint8_t spare;              /* spare                               */
  } hw;
};

#endif
