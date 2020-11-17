/*************
 *
 * Filename:   ssmemiproto.h
 *
 * Purpose:    ssmem package internal functions
 *
 * Note:       none
 *
 * Copyright:  (c) 2016 Sierra Wireless, Inc.
 *             All rights reserved
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

extern uint8_t *ssmem_smem_end_addr_get(void);

extern void *ssmem_heap_alloc(int buf_size);

extern struct ssmem_alloc_entry_s *ssmem_alloc_entry_get(int region_id);

extern struct ssmem_alloc_entry_s *ssmem_alloc_entry_add(
  int      region_id,
  uint8_t *regionp,
  int      region_sz);

extern boolean ssmem_framework_init(void);

extern boolean ssmem_priv_meta_check(
  uint8_t  *regionp,
  int       region_id,
  uint16_t  user_ver,
  int      *sizep);

extern boolean ssmem_priv_meta_update(
  uint8_t *regionp,
  int      region_id,
  int      region_sz,
  uint16_t user_version,
  int      user_size);

extern uint32_t ssmem_priv_crc32(uint8_t *msgp, int size);

extern boolean ssmem_spin_lock_init(void);

extern boolean ssmem_spin_lock(int lock_id);

extern boolean ssmem_spin_unlock(int lock_id);

extern boolean ssmem_boot_init(void);

extern boolean ssmem_framework_one_time_init(void);



