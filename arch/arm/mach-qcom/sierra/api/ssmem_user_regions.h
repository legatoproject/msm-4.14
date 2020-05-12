/*************
 *
 * Filename:  ssmem_user_regions.h
 *
 * Purpose:   definition for user regions
 *
 * Notes:     Most of user regions will be managed by user app
 *            and their size/version/structure don't need to be
 *            listed here.
 *            This file will include defines for all user regions
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 **************/
#ifndef SSMEM_USER_REGIONS_H
#define SSMEM_USER_REGIONS_H



/* Constants and enumerated types */
#define SSMEM_RG_USER_VER_1P0        0x0100


#define SSMEM_RG_SZ_CWE_HEADER       0x800    /* 512 * 4 headers */
#define SSMEM_RG_VER_CWE_HEADER      SSMEM_RG_USER_VER_1P0


/* Structures */

#endif /* SSMEM_USER_REGIONS_H */

