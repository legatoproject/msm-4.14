/*************
 *
 * Filename:  ssmemuproto.h
 *
 * Purpose:   user prototypes for ssmem
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
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




