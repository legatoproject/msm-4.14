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
