/************
 *
 * Filename:  ssmem_address.h
 *
 * Purpose:   global define for ssmem region address
 *
 * NOTES:     this file will be used in multiple places.
 *            it will also be used in sbl.scl compiling so keep it clean and don't include
 *            other header files in this file
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

#ifndef SSMEM_ADDRESS_H
#define SSMEM_ADDRESS_H

/* External Definitions */

/* Sierra shared memory
 *
 * A section of memory at the top of DDR is reserved for Sierra
 * boot-app messages, crash information, and other data.  This section
 * is non-initialized in order to preserve data across reboots.  The
 * size and base are (unfortunately) defined in multiple places.
 *
 * WARNING:  These definitions must be kept in sync
 *
 * modem_proc/config/9635/cust_config.xml
 * apps_proc/kernel/arch/arm/boot/dts/qcom/mdm9630.dtsi
 *
 */

#ifndef DDR_MEM_BASE
#define DDR_MEM_BASE 0x80000000
#endif

#define SIERRA_MEM_SIZE           0x10000000    /* 256 MB */
#define SIERRA_SMEM_SIZE          0x00100000    /*   1 MB */
#define SIERRA_SMEM_BASE_PHY      ((DDR_MEM_BASE + SIERRA_MEM_SIZE) - (SIERRA_SMEM_SIZE))

#endif /* SSMEM_ADDRESS_H */
