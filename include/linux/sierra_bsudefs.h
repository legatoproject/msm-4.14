/* kernel/include/linux/sierra_bsudefs.h
 *
 * Copyright (C) 2013 Sierra Wireless, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SIERRA_BS_UDEFS_H
#define SIERRA_BS_UDEFS_H


/* Define 6-bit HW revision number for WP76 DV5.2 hardware build */
#define BS_HW_ID_DV_5_2      ((0x5 << 2) | 0x1)

enum bshwtype
{
	BSQCTMTP,               /* Qualcomm MTP 9x30 */
	BSHWNONE,               /* HW type NONE (Fuse has not been blown yet) */
	BSAR7582,               /* 0x02 - Automotive 7582 */
	BSAR7584,               /* 0x03 - Automotive 7584 */
	BSAR7586,               /* 0x04 - Automotive 7586 */
	BSAR7588,               /* 0x05 - Automotive 7588 */
	BSAR8582,               /* 0x06 - Automotive 8582 */
	BSAR7582_NC,            /* 0x07 - Automotive 7582 without codec */
	BSAR7584_NC,            /* 0x08 - Automotive 7584 without codec */
	BSAR7586_NC,            /* 0x09 - Automotive 7586 without codec */
	BSAR7588_NC,            /* 0x0A - Automotive 7588 without codec */
	BSAR8582_NC,            /* 0x0B - Automotive 8582 without codec */
	BSWP7601,               /* 0x0C - WP7601 */
	BSWP7603,               /* 0x0D - WP7603 */
	BSWP7601_1,             /* 0x0E - WP7601-1 */
	BSWP7603_1,             /* 0x0F - WP7603-1 */
	BSWP7605,               /* 0x10 - WP7605  */
	BSWP7605_1,             /* 0x11 - WP7605-1 */
	BSWP7607,               /* 0x12 - WP7607 */
	BSWP7607_1,             /* 0x13 - WP7607-1 */
	BSWP7607_2,             /* 0x14 - WP7607-2 */
	BSWP7700,               /* 0x15 - WP7700 */
	BSWP7702,               /* 0x16 - WP7702 */
	BSHWUNKNOWN,            /* Unknown HW */
	BSHWINVALID = 0xFF      /* Invalid HW */
};

/************
 *
 * Name:		 bs_prod_family_e
 *
 * Purpose:  To enumerate product family types
 *
 * Members:  See below
 *
 * Notes:		 None
 *
 ************/
enum bs_prod_family_e
{
	BS_PROD_FAMILY_QCTMTP,				/* Qualcomm MTP 9x30 */
	BS_PROD_FAMILY_NONE,					/* product family type NONE */
	BS_PROD_FAMILY_EM,						/* product family Embedded Module */
	BS_PROD_FAMILY_MC,						/* product family MiniCard */
	BS_PROD_FAMILY_AR,						/* product family Automotive */
	BS_PROD_FAMILY_WP,						/* product family WP */
	BS_PROD_FAMILY_UNKNOWN,				/* product family Unknown */
	BS_PROD_FAMILY_INVALID = 0xFF	/* product family Invalid */
};

/************
 *
 * Name:		 bsfeature
 *
 * Purpose:  Enumerated list of different features supported by different hardware variants
 *
 * Members:  see below
 *
 * Notes:		 None
 *
 ************/
enum bsfeature
{
	BSFEATURE_MINICARD,				/* if the hardware is a MiniCard */
	BSFEATURE_EM,							/* if the device is EM product */
	BSFEATURE_AR,							/* if the hardware is an AR product */
	BSFEATURE_WP,							/* if the hardware is a WP product */
	BSFEATURE_W_DISABLE,			/* if W_DISABLE is supported */
	BSFEATURE_VOICE,					/* if voice is supported */
	BSFEATURE_HSUPA,					/* if the hardware supports HSUPA */
	BSFEATURE_GPIOSAR,				/* if GPIO controlled SAR backoff is supported */
	BSFEATURE_RMAUTOCONNECT,	/* if auto-connect feature is device centric */
	BSFEATURE_UART,						/* if the hardware support UART */
	BSFEATURE_ANTSEL,					/* if the hardware supports ANTSEL */
	BSFEATURE_INSIM,					/* Internal SIM supported (eSIM) */
	BSFEATURE_OOBWAKE,				/* if has OOB_WAKE GPIO */
	BSFEATURE_CDMA,						/* if the hardware supports CDMA/1x */
	BSFEATURE_GSM,						/* if the hardware supports GSM/EDGE */
	BSFEATURE_WCDMA,					/* if the hardware supports WCDMA */
	BSFEATURE_LTE,						/* if the hardware supports LTE */
	BSFEATURE_TDSCDMA,				/* if the hardware supports TDSCDMA */
	BSFEATURE_UBIST,					/* if Dell UBIST is supported */
	BSFEATURE_GPSSEL,					/* if GPS antenna selection is supported */
	BSFEATURE_SIMHOTSWAP,			/* if hardware supports SIM detection via GPIO */
	BSFEATURE_WM8944,					/* if WM8944 codec is supported */
	BSFEATURE_SEGLOADING,			/* if Segment Loading Feature enabled*/
	BSFEATURE_WWANLED,				/* if WWANLED is supported */
	BSFEATURE_POWERFAULT,			/* if POWERFAULT is supported */
	BSFEATURE_MAX							/* Used for bounds checking */
};

/************
 *
 * Name:     bs_uart_func_e
 *
 * Purpose:  Enumerated list of different functions supported by App processor
 *
 * Members:  BS_UART_FUNC_DISABLED  - UART disabled
 *           BS_UART_FUNC_AT - UART reserved for AT service
 *           BS_UART_FUNC_DM - UART reserved for DM service
 *           BS_UART_FUNC_NMEA - UART reserved for NMEA service
 *           BS_UART_FUNC_RS485 - UART used for Linux application using RS485
 *           BS_UART_FUNC_RS232_FC - UART used for Linux application using RS232 with Hardware flow control
 *           BS_UART_FUNC_CONSOLE - UART reserved for CONSOLE service
 *           BS_UART_FUNC_APP - UART open for all application usage
 *           BS_UART_FUNC_APP - used for bounds checking
 *           BS_UART_FUNC_INVALID - function is invalid
 *
 * Notes:    None
 *
 ************/
enum bs_uart_func_e
{
  BS_UART_FUNC_DISABLED = 0,
  BS_UART_FUNC_AT       = 1,
  BS_UART_FUNC_DM       = 2,
  BS_UART_FUNC_NMEA     = 4,
  BS_UART_FUNC_RS485    = 14,
  BS_UART_FUNC_RS232_FC = 15,
  BS_UART_FUNC_CONSOLE  = 16,
  BS_UART_FUNC_APP      = 17,
  BS_UART_FUNC_MAX,
  BS_UART_FUNC_INVALID  = 0xFF,
};

/************
 *
 * Members:  BS_UART1_LINE - line number of UART1
 *           BS_UART2_LINE - line number of UART2
 *           BS_UART_LINE_MAX - used for bounds checking
 * Notes:    None
 *
 ************/
enum bs_uart_line_e
{
  BS_UART1_LINE = 0,
  BS_UART2_LINE,
  BS_UART_LINE_MAX,
};

/************
 *
 * Members:  BS_UART_TYPE_HSL - high speed lite UART
 *           BS_UART_TYPE_HS - high speed UART
 *           BS_UART_TYPE_MAX - used for bounds checking
 * Notes:    None
 *
 ************/
enum bs_uart_type_e
{
  BS_UART_TYPE_HSL = 0,
  BS_UART_TYPE_HS,
  BS_UART_TYPE_MAX,
};


#include "sierra_bsuproto.h"
#endif /* SIERRA_BSUDEFS_H */
