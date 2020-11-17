/************
 *
 * Filename:  sec_ssmem_structure.h
 *
 * Purpose:   SEC SSMEM structure defines
 *
 * Notes:     This file will be used for all the images: boot/mpss/apss/kernel
 *            so please kee it clean
 *            This file will be open source since it is used by Linux Kernel
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

#ifndef SEC_SSMEM_STRUCTURE_H
#define SEC_SSMEM_STRUCTURE_H

#define SEC_SSMEM_VER    SSMEM_RG_USER_VER_1P0

/* hash table entry: 8 byte control info + SHA256 hash */
#define SEC_SHA256_HASH_LEN               32
#define SEC_HASH_CONTROL_INFO_LEN         8
#define SEC_HASH_OFFSET                   SEC_HASH_CONTROL_INFO_LEN
#define SEC_HASH_ENTRY_LEN                (SEC_HASH_CONTROL_INFO_LEN + SEC_SHA256_HASH_LEN)

/************
 *
 * Name:     sec_ssmem_s
 *
 * Purpose:  sec shared memory region layout
 *
 * Notes:    none
 *
 ************/
PACKED struct PACKED_POST sec_ssmem_s
{
  uint8_t  control_info[SEC_HASH_CONTROL_INFO_LEN];  /* control info      */
  uint8_t  cert_hash[SEC_SHA256_HASH_LEN];           /* hash of the cert  */
};

#endif /* SEC_SSMEM_STRUCTURE_H */
