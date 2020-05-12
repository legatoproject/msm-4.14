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



