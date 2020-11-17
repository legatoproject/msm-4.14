/************
 *
 * Filename:  ssmem_lk.c
 *
 * Purpose:   utility functions for ssmem in LK
 *
 * Notes:     This file is only used LK
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
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
#include "aaglobal_linux.h"
#include "aadebug_linux.h"
#include "ssmemidefs.h"

#include <platform/iomap.h>
#include <arch/arm/mmu.h>

/* Local constants and enumerated types */


/* Functions */

/************
 *
 * Name:     ssmem_spin_lock_init
 *
 * Purpose:  acquire a spin lock
 *
 * Parms:    none
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    for LK, can always acqure the spin lock since
 *           no other sub-system is running
 *
 ************/
_package boolean ssmem_spin_lock_init(
  void)
{
  return TRUE;
}

/************
 *
 * Name:     ssmem_spin_lock
 *
 * Purpose:  acquire a spin lock
 *
 * Parms:    lock_id - lock ID
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    for LK, can always acqure the spin lock since
 *           no other sub-system is running
 *
 ************/
_package boolean ssmem_spin_lock(
  int lock_id)
{
  return TRUE;
}

/************
 *
 * Name:     ssmem_spin_unlock
 *
 * Purpose:  release a spin lock
 *
 * Parms:    lock_id - lock ID
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    for LK, can always acqure the spin lock since
 *           no other sub-system is running
 *
 ************/
_package boolean ssmem_spin_unlock(
  int lock_id)
{
  return TRUE;
}

/************
 *
 * Name:     ssmem_smem_base_addr_get
 *
 * Purpose:  get SMEM base address
 *
 * Parms:    none
 *
 * Return:   Sierra SMEM base address
 *
 * Abort:    none
 *
 * Notes:    will also map SMEM region if needed
 *
 ************/
_global uint8_t *ssmem_smem_base_addr_get(void)
{
  static boolean mmu_inited = FALSE;

  if (!mmu_inited)
  {
    mmu_inited = TRUE;

    /* map SMEM virtual = phy addr. the follow function will map 1MB
     * assuming SIERRA_SMEM_SIZE is less or equal to 1MB
     */
    arm_mmu_map_section(SSMEM_MEM_BASE_ADDR,
                        SSMEM_MEM_BASE_ADDR,
                        (MMU_MEMORY_TYPE_DEVICE_SHARED |
                         MMU_MEMORY_AP_READ_WRITE |
                         MMU_MEMORY_XN));

  }

  return (uint8_t *)SSMEM_MEM_BASE_ADDR;
}

