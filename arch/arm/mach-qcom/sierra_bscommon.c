/* arch/arm/mach-msm/sierra_bscommon.c
 *
 * Copyright (C) 2013 Sierra Wireless, Inc
 * Author: Alex Tan <atan@sierrawireless.com>
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

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>
#include <mach/sierra_bsidefs.h>
#include <mach/sierra_smem.h>
#include <linux/sierra_bsudefs.h>
#include <sierra/api/cowork_ssmem_structure.h>
#include <sierra/api/ssmemudefs.h>

/* RAM Copies of HW type, rev, etc. */
/* note that there is a copy for the bootloader and another for the app */
union bs_hwconfig_s bs_hwcfg;
bool bs_hwcfg_read = false;

/************
 *
 * Name:     bsgetgpioflag()
 *
 * Purpose:  Returns the concatenation of external gpio owner flags
 *
 * Parms:    none
 *
 * Return:   Extern GPIO owner flag
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
uint64_t bsgetgpioflag(void)
{
        struct cowork_ssmem_s *coworkp;
        uint64_t result = 0;

        coworkp = ssmem_cowork_get();
        if (!coworkp)
        {
                pr_err("%s: error getting SSMEM cowork region", __func__);
        }
        else
        {
                result = (uint64_t)(coworkp->gpio_flags[0]) |
                         ((uint64_t)(coworkp->gpio_flags[1]) << 32);
        }

        return result;
}
EXPORT_SYMBOL(bsgetgpioflag);

/************
 *
 * Name:     bsgethsicflag()
 *
 * Purpose:  Returns the hsic is enabled or not
 *
 * Parms:    none
 *
 * Return:   returns whether hsic is enabled or not
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
bool bsgethsicflag(void)
{
        struct cowork_ssmem_s *coworkp;
        uint32_t result = 0;

        coworkp = ssmem_cowork_get();
        if (!coworkp)
        {
                pr_err("%s: error getting SSMEM cowork region", __func__);
        }
        else if (coworkp->functions)
        {
                result = 1;
        }
        else
        {
                /* Nothing to do */
        }

        return result;
}
EXPORT_SYMBOL(bsgethsicflag);

/************
 *
 * Name:     bs_hwtype_get
 *
 * Purpose:  Returns hardware type read from QFPROM
 *
 * Parms:    none
 *
 * Return:   hardware type
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
enum bshwtype bs_hwtype_get (void)
{
	if (!bs_hwcfg_read)
	{
		bs_hwcfg.all = sierra_smem_get_hwconfig();
		bs_hwcfg_read = true;
	}

	return (enum bshwtype) bs_hwcfg.hw.type;
}
EXPORT_SYMBOL(bs_hwtype_get);

/************
 *
 * Name:     bs_hwrev_get
 *
 * Purpose:  Returns hardware revision read from QFPROM
 *
 * Parms:    none
 *
 * Return:   hardware major revision number
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
uint8_t bs_hwrev_get (void)
{
	if (!bs_hwcfg_read)
	{
		bs_hwcfg.all = sierra_smem_get_hwconfig();
		bs_hwcfg_read = true;
	}

	return bs_hwcfg.hw.rev;
}
EXPORT_SYMBOL(bs_hwrev_get);

/************
 *
 * Name:     bs_prod_family_get
 *
 * Purpose:  Returns product family read from QFPROM / SMEM
 *
 * Parms:    none
 *
 * Return:   product family
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
enum bs_prod_family_e bs_prod_family_get (void)
{
	if (!bs_hwcfg_read)
	{
		bs_hwcfg.all = sierra_smem_get_hwconfig();
		bs_hwcfg_read = true;
	}

	return (enum bs_prod_family_e) bs_hwcfg.hw.family;
}
EXPORT_SYMBOL(bs_prod_family_get);


/************
 *
 * Name:     bs_support_get
 *
 * Purpose:  To check if the hardware supports a particular feature
 *
 * Parms:    feature - feature to check
 *
 * Return:   true if hardware supports this feature
 *           false otherwise
 *
 * Abort:    none
 *
 * Notes:    This function is primarily designed to keep hardware variant
 *           checks to a central location.
 *
 ************/
bool bs_support_get (enum bsfeature feature)
{
	bool supported = false;
	enum bs_prod_family_e prodfamily;
	enum bshwtype hwtype;

	prodfamily = bs_prod_family_get();
	hwtype = bs_hwtype_get();

	switch (feature)
	{
		case BSFEATURE_AR:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_AR:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_WP:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_WM8944:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;
				default:
					break;
			}
			break;
/*SWI_TBD BChen LXSWI9X28-9 [2016-12-08] sync code from modem_proc/sierra/bs/src/bsuser.c
 *disable the redundant judgement ATM
 */
#if 0
		case BSFEATURE_MINICARD:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_MC:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_EM:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_EM:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_W_DISABLE:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_EM:
				case BS_PROD_FAMILY_MC:
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_WWANLED:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;
				default:
					break;
			}
			break;

		case BSFEATURE_VOICE:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_HSUPA:
			supported = true;
			break;

		case BSFEATURE_GPIOSAR:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_RMAUTOCONNECT:
			supported = true;
			break;

		case BSFEATURE_UART:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_AR:
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;

				default:
					break;
			}
			break;

		case BSFEATURE_ANTSEL:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_INSIM:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_OOBWAKE:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_WP:
					supported = true;
					break;

				default:
					break;
			}
			break;

		case BSFEATURE_CDMA:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_GSM:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_WCDMA:
			supported = true;
			break;

		case BSFEATURE_LTE:
			supported = true;
			break;

		case BSFEATURE_TDSCDMA:
			switch (hwtype)
			{
					case BSAR7586:
						supported = true;
						break;
					default:
						supported = FALSE;
						break;
			}
			break;

		case BSFEATURE_UBIST:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_GPSSEL:
			switch (hwtype)
			{
				default:
					break;
			}
			break;

		case BSFEATURE_SIMHOTSWAP:
			switch (hwtype)
			{
				default:
					supported = true;
					break;
			}
			break;

		case BSFEATURE_SEGLOADING:
			switch (hwtype)
			{
				case BSAR7586:
					supported = true;
					break;
				default:
					supported = FALSE;
					break;
			}
			break;

		case BSFEATURE_POWERFAULT:
			switch (prodfamily)
			{
				case BS_PROD_FAMILY_AR:
					supported = true;
					break;

				default:
					supported = FALSE;
					break;
			}
			break;
#endif
		default:
			break;
	}

	return supported;
}
EXPORT_SYMBOL(bs_support_get);

/************
 *
 * Name:     bs_uart_fun_get()
 *
 * Purpose:  Provide to get UARTs function seting
 *
 * Parms:    uart Number
 *
 * Return:   UART function
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
int8_t bs_uart_fun_get (uint uart_num)
{
        struct cowork_ssmem_s *coworkp;
        int8_t result = (-1);

        if (uart_num >= BS_UART_LINE_MAX) {
                return result;
        }

        coworkp = ssmem_cowork_get();
        if (!coworkp)
        {
                pr_err("%s: error getting SSMEM cowork region", __func__);
        }
        else
        {
                /*get UART function setting*/
                result = coworkp->uart_fun[uart_num];
        }

        return result;
}
EXPORT_SYMBOL(bs_uart_fun_get);

/************
 *
 * Name:     bsgetriowner()
 *
 * Purpose:  Provide to get RI owner seting
 *
 * Parms:    none
 *
 * Return:   RI owner
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
int8_t bsgetriowner(void)
{
        struct cowork_ssmem_s *coworkp;
        int8_t result = (-1);

        coworkp = ssmem_cowork_get();
        if (!coworkp)
        {
                pr_err("%s: error getting SSMEM cowork region", __func__);
        }
        else
        {
                result = coworkp->ri_owner;
        }

        return result;
}

EXPORT_SYMBOL(bsgetriowner);
