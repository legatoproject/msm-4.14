/*
********************************************************************************
* Name:  ssmem_keystore.c
*
* Sierra Wireless keystore interface module
*
* This provides an interface to read the first IMA kernel key from the
* keystore v1
*
* Copyright (C) 2019 Sierra Wireless Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or (at
* your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
* NON INFRINGEMENT.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
********************************************************************************
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <../sierra/api/ssmemudefs.h>
#include <../sierra/api/ssmem_keystore.h>

typedef struct
{
	u8 *list;
	unsigned long size;
} certs_rec_t;

int scrape_ssmem_for_keys(certs_rec_t *rec)
{
	sec_ssmem_key_hdr_s *sskey;
	u8 *list = NULL;
	unsigned long list_size = 0;
	int offset = SEC_OEM_KEY_LENGTH;
	uint8_t* keysp;
	int size;
	int count = 0;
	int ret = -1;

	keysp = ssmem_keys_get(&size);

	if (keysp == NULL || size == 0) {
		pr_notice("Keystore: not present\n");
		goto bail_out;
	}

	pr_debug("Keystore: keysp %p size %d", keysp, size);

	while(offset < size)
	{
		sskey = (sec_ssmem_key_hdr_s*)(keysp + offset);
		pr_debug("Keystore: key length %d at offset %d\n",
			 sskey->length, offset);

		if (sskey->length >= 256 ) {
			count++;
			pr_debug("Keystore found x509.cert[%d]\n", count);
			list = (u8 *)krealloc(list, list_size + sskey->length,
					      GFP_ATOMIC);
			if (!list)
				panic("Can't reallocate cert list.");

			memcpy(list + list_size,
			       keysp + offset + sizeof(sec_ssmem_key_hdr_s),
			       sskey->length);
			list_size += (unsigned long)sskey->length;
		}

		offset += sizeof(sec_ssmem_key_hdr_s) + sskey->length;
	}

	if (list){
		/* Cert list created, return success */
		rec->list = list;
		rec->size = list_size;
		ret = 0;
		pr_info("Loaded %d keys from keystore\n", count);
	}

bail_out:
	ssmem_keys_release();
	return ret;
}

unsigned long keystore_size(void *p)
{
	return (p != NULL ? ((certs_rec_t*)p)->size : 0);
}

u8 *keystore_list(void *p)
{
	return (p != NULL ? ((certs_rec_t*)p)->list : NULL);
}

void *keystore_init(void)
{
	certs_rec_t *certs;

	pr_notice("Init keystore X.509 certificates\n");

	certs = (certs_rec_t *)kmalloc(sizeof(certs_rec_t), GFP_ATOMIC);
	if (certs == NULL)
		panic("Can't allocate memory for keystore certs.");

	certs->size = 0;

	if (scrape_ssmem_for_keys(certs) != 0) {
		pr_notice("Keystore empty.\n");
		kfree(certs);
		return NULL;
	}

	return (void *)certs;
}
