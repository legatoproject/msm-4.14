/* arch/arm/mach-msm/sierra_smem_msg.c
 *
 * Sierra SMEM MSG region mailbox functions used to set/get flags 
 * These functions don't rely on Sierra SMEM driver,
 * and can be used in early kernel start
 *
 * Copyright (c) 2015 Sierra Wireless, Inc
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

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/crc32.h>

#include <mach/sierra_smem.h>
#include "sierra/api/ssmemudefs.h"

int sierra_smem_get_download_mode(void)
{
        struct bc_smem_message_s *b2amsgp;
        unsigned char *virtual_addr;
        int download_mode = 0;

        virtual_addr = sierra_smem_base_addr_get();
        if (virtual_addr) {

                /*  APPL mailbox */
                virtual_addr += BSMEM_MSG_APPL_MAILBOX_OFFSET;

                b2amsgp = (struct bc_smem_message_s *)virtual_addr;

                if (b2amsgp->magic_beg == BC_SMEM_MSG_MAGIC_BEG &&
                    b2amsgp->magic_end == BC_SMEM_MSG_MAGIC_END &&
                    (b2amsgp->in.flags & BC_MSG_B2A_DLOAD_MODE)) {
                        /* doube check CRC */
                        if (b2amsgp->version < BC_SMEM_MSG_CRC32_VERSION_MIN ||
                            b2amsgp->crc32 == 
                              crc32_le(~0, (void *)b2amsgp, BC_MSG_CRC_SZ)) { 
                                download_mode = 1;
                        }
                 }
        }

        return download_mode;
}
EXPORT_SYMBOL(sierra_smem_get_download_mode);

int sierra_smem_boothold_mode_set(void)
{
        struct bc_smem_message_s *a2bmsgp;
        uint64_t a2bflags = 0;
        unsigned char *virtual_addr;

        virtual_addr = sierra_smem_base_addr_get();
        if (virtual_addr) {

                /*  APPL mailbox */
                virtual_addr += BSMEM_MSG_APPL_MAILBOX_OFFSET;
        }
        else {

                return -1;
        }

        a2bmsgp = (struct bc_smem_message_s *)virtual_addr;

        /* SWI_TBD bdu 2014:12:17 - might need lock here, need to unify with 
         * bcmsg.c defines and APIs 
         */
        if (a2bmsgp->magic_beg == BC_SMEM_MSG_MAGIC_BEG &&
            a2bmsgp->magic_end == BC_SMEM_MSG_MAGIC_END &&
            (a2bmsgp->version < BC_SMEM_MSG_CRC32_VERSION_MIN ||
             a2bmsgp->crc32 == crc32_le(~0, (void *)a2bmsgp, BC_MSG_CRC_SZ))) {

                a2bflags = a2bmsgp->out.flags;
        }
        else {

                memset((void *)a2bmsgp, 0, sizeof(struct bc_smem_message_s));
                a2bmsgp->in.launchcode  = BC_MSG_LAUNCH_CODE_INVALID;
                a2bmsgp->in.recover_cnt = BC_MSG_RECOVER_CNT_INVALID;
                a2bmsgp->in.hwconfig    = BC_MSG_HWCONFIG_INVALID;
                a2bmsgp->in.usbdescp    = BC_MSG_USB_DESC_INVALID;
                a2bmsgp->out.launchcode  = BC_MSG_LAUNCH_CODE_INVALID;
                a2bmsgp->out.recover_cnt = BC_MSG_RECOVER_CNT_INVALID;
                a2bmsgp->out.hwconfig    = BC_MSG_HWCONFIG_INVALID;
                a2bmsgp->out.usbdescp    = BC_MSG_USB_DESC_INVALID;
                a2bmsgp->version   = BC_SMEM_MSG_VERSION;
                a2bmsgp->magic_beg = BC_SMEM_MSG_MAGIC_BEG;
                a2bmsgp->magic_end = BC_SMEM_MSG_MAGIC_END;
                a2bflags = 0;
        }

        a2bflags |= BC_MSG_A2B_BOOT_HOLD;
        a2bmsgp->out.flags = a2bflags;
        a2bmsgp->crc32 = crc32_le(~0, (void *)a2bmsgp, BC_MSG_CRC_SZ);
  
        return 0;
}
EXPORT_SYMBOL(sierra_smem_boothold_mode_set);


/************
 *
 * Name:     sierra_smem_warm_reset_cmd_get
 *
 * Purpose:  check if warm reset if needed
 *
 * Parms:    none
 *
 * Return:   > 0 if warm reset required
 *           0 otherwise
 *
 * Abort:    none
 *
 * Notes:    See msm_restart_prepare, default is hard reset.
 *           use warm reset only if SMEM content need to be retained
 *
 ************/
int sierra_smem_warm_reset_cmd_get(void)
{
  int i,ret = 0;
  struct bc_smem_message_s *a2bmsgp = NULL;
  unsigned char *virtual_addr;

  for (i = BCMSG_MBOX_MIN; i <= BCMSG_MBOX_MAX; i++)
  {
    if (i != BCMSG_MBOX_BOOT)
    {
      virtual_addr = sierra_smem_base_addr_get();
      if (virtual_addr &&
          (i >= BCMSG_MBOX_MIN) &&
          (i <= BCMSG_MBOX_MAX))
      {
        /*  APPL mailbox */
        virtual_addr += (BSMEM_MSG_OFFSET + (BC_MSG_SIZE_MAX * i));

        a2bmsgp = (struct bc_smem_message_s *)virtual_addr;
      }
      if (a2bmsgp &&
          ((a2bmsgp->out.flags & BC_MSG_A2B_BOOT_HOLD) ||
           (a2bmsgp->out.flags & BC_MSG_A2B_WARM_BOOT_CMD)))
      {
        ret = 1;
        break;
      }
    }
  }

  return ret;
}

int sierra_smem_im_recovery_mode_set(void)
{
        struct imsw_smem_im_s *immsgp;
        unsigned char *virtual_addr;

        virtual_addr = sierra_smem_base_addr_get();
        if (virtual_addr) {

                /*  IM region */
                virtual_addr += BSMEM_IM_OFFSET;
        }
        else {

                return -1;
        }

        immsgp = (struct imsw_smem_im_s *)virtual_addr;
        /* reinit IM region since boot will reconstruct it anyway */
        memset((void *)immsgp, 0, sizeof(struct imsw_smem_im_s));

        /* set magic numbers: start, end, recovery */
        immsgp->magic_beg = IMSW_SMEM_MAGIC_BEG;
        immsgp->magic_recovery = IMSW_SMEM_MAGIC_RECOVERY;
        immsgp->magic_end = IMSW_SMEM_MAGIC_END;
        /* precalcualted CRC */
        immsgp->crc32 = crc32_le(~0, (void *)immsgp, 
                                 sizeof(struct imsw_smem_im_s) - sizeof(immsgp->crc32));

        return 0;
}
EXPORT_SYMBOL(sierra_smem_im_recovery_mode_set);

uint32_t sierra_smem_get_hwconfig(void)
{
	struct bc_smem_message_s *b2amsgp;
	unsigned char *virtual_addr;
	uint32_t hwconfig = BC_MSG_HWCONFIG_INVALID;

	virtual_addr = sierra_smem_base_addr_get();

	if (virtual_addr != NULL) {
	/*  APPL mailbox */
	virtual_addr += BSMEM_MSG_APPL_MAILBOX_OFFSET;
	b2amsgp = (struct bc_smem_message_s *)virtual_addr;

	if (b2amsgp->magic_beg == BC_SMEM_MSG_MAGIC_BEG &&
			b2amsgp->magic_end == BC_SMEM_MSG_MAGIC_END) {
				/* doube check CRC */
				if (b2amsgp->crc32 ==  crc32_le(~0, (void *)b2amsgp, BC_MSG_CRC_SZ)) {
					hwconfig = b2amsgp->in.hwconfig ;
				}
			}
		}
	return hwconfig;
}
EXPORT_SYMBOL(sierra_smem_get_hwconfig);

uint8_t *ssmem_keys_get(int* sizep)
{
  return (uint8_t*) ssmem_get(SSMEM_RG_ID_KEYS,
                              SSMEM_FRAMEWORK_VERSION,
                              sizep);
}
EXPORT_SYMBOL(ssmem_keys_get);

boolean ssmem_keys_release()
{
  /* do nothing, keys remains in shared memory */
  return TRUE;
}
EXPORT_SYMBOL(ssmem_keys_release);
