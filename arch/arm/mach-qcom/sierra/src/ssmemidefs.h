/*************
 *
 * Filename:  ssmemidefs.h
 *
 * Purpose:   internal definitions for ssmem
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
 **************/

/* Include files */
#include "ssmemudefs.h"
#include "ssmem_address.h"

/* Local constants and enumerated types */
#define SSMEM_MEM_BASE_ADDR          SIERRA_SMEM_BASE_PHY
#define SSMEM_MEM_SIZE               SIERRA_SMEM_SIZE

#define SSMEM_FRAMWORK_TOTAL_SIZE    2048
/* user regions can be right after framework regions but can be
 * also forced at other offset
 */
#define SSMEM_USER_REGION_OFFSET     0x80000

#define SSMEM_RG_SZ_VERSION_INFO     64
#define SSMEM_RG_SZ_ALLOCATION_TABLE 1632
#define SSMEM_RG_SZ_HEAP_INFO        64
#define SSMEM_RG_SZ_SPINLOCK_ARRAY   64

/* Structures, enums, etc. */


#include "ssmemiproto.h"
