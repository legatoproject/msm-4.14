/*************
 *
 * Filename:  ssmemuproto.h
 *
 * Purpose:   user prototypes for ssmem
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

/* Prototypes */

extern void *ssmem_get(
  int      region_id,
  uint16_t version,
  int     *sizep);

extern void *ssmem_acquire(
  int       region_id,
  uint16_t  version,
  int       size);

extern boolean ssmem_meta_update(int region_id);

extern boolean ssmem_release(int region_id);

extern void ssmem_mpss_up_notification(void);

/* export this function temporary, it should be internal */
extern uint8_t *ssmem_smem_base_addr_get(void);

/* shared memory keys data retrieve function */
extern uint8_t *ssmem_keys_get(int* sizep);

/* shared memory keys data release function */
extern boolean ssmem_keys_release(void);




