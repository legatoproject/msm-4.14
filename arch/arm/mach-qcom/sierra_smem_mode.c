/* arch/arm/mach-msm/sierra_smem_mode.c
 *
 * Sierra SMEM MSG region mailbox functions used to set/get flags
 * These functions don't rely on Sierra SMEM driver,
 * and can be used in early kernel start
 *
 * Copyright (c) 2015 Sierra Wireless, Inc
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

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/crc32.h>

#include <sierra/api/cowork_ssmem_structure.h>
#include <sierra/api/ssmemudefs.h>

int sierra_smem_get_factory_mode(void)
{
  struct cowork_ssmem_s *coworkp;
  int mode = (-1);

  coworkp = ssmem_cowork_get();
  if (!coworkp)
  {
    pr_err("%s: error getting SSMEM cowork region", __func__);
  }
  else
  {
    mode = coworkp->mode;
  }

  return mode;
}
EXPORT_SYMBOL(sierra_smem_get_factory_mode);
