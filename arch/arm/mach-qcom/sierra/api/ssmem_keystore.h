/*
********************************************************************************
* Name:  ssmem_keystore.h
*
* Sierra Wireless keystore kernel v1.
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

#ifndef _SSMEM_KEYSTORE_H
#define _SSMEM_KEYSTORE_H

#if defined CONFIG_KEYS_KEYSTORE && defined CONFIG_SIERRA
unsigned long keystore_size(void *p);
u8 *keystore_list(void *p);
void *keystore_init(void);
#else
static inline unsigned long keystore_size(void *p) {return 0;}
static inline u8 *keystore_list(void *p) {return NULL;}
static inline void *keystore_init(void) {return NULL; /* fail */}
#endif

#endif /* _SSMEM_KEYSTORE_H */
