/************
 *
 * Filename:  ssmem_linux.c
 *
 * Purpose:   SSMEM Linux driver
 *
 * Notes:     This file will provide Linux specific utility functions
 *            and driver interface
 *            SSMEM functions can be called before driver is loaded (for
 *            example other modules requring SSMEM API) so SSMEM can be
 *            initialized before driver is started
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
 ************/

/* Include files */
#include "aaglobal_linux.h"
#include "aadebug_linux.h"
#include "ssmemidefs.h"

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

/* Local constants and enumerated types */

_local uint8_t *ssmem_base = NULL;
_local boolean mpss_started = FALSE;

static DEFINE_MUTEX(ssmem_ioctl_lock);

_local spinlock_t framework_lock;

/* Functions */

/************
 *
 * Name:     ssmem_spin_lock_init
 *
 * Purpose:  acquire a spin lock
 *
 * Parms:    none
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    Before mpss is up and runing, Linux kernel can be assumed to have
 *           exclusive access to SSMEM. Note that the spinlock functions below
 *           can only lock within Linux subsystem/processor.
 *           Once mpss is up, kernel cannot acquire spinlock anymore
 *
 ************/
_package boolean ssmem_spin_lock_init(
  void)
{
  spin_lock_init(&framework_lock);
  return TRUE;
}

/************
 *
 * Name:     ssmem_spin_lock
 *
 * Purpose:  acquire a spin lock
 *
 * Parms:    lock_id - lock ID. Only framework spinlock is supported and assumed
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    see notes in ssmem_spin_lock_init
 *
 ************/
_package boolean ssmem_spin_lock(
  int lock_id)
{
  if (mpss_started)
  {
    return FALSE;
  }
  else
  {
    return spin_trylock(&framework_lock);
  }
}

/************
 *
 * Name:     ssmem_spin_unlock
 *
 * Purpose:  release a spin lock
 *
 * Parms:    lock_id - lock ID
 *
 * Return:   TRUE if success
 *           FALSE otherwise
 *
 * Abort:    None
 *
 * Notes:    see notes in ssmem_spin_lock_init
 *
 ************/
_package boolean ssmem_spin_unlock(
  int lock_id)
{
  spin_unlock(&framework_lock);
  return TRUE;
}

/************
 *
 * Name:     ssmem_mpss_up_notification
 *
 * Purpose:  notification that mpss is up
 *
 * Parms:    None
 *
 * Return:   None
 *
 * Abort:    None
 *
 * Notes:    see notes in ssmem_spin_lock_init
 *
 ************/
_global void ssmem_mpss_up_notification(
  void)
{
  mpss_started = TRUE;
}

/************
 *
 * Name:     ssmem_smem_base_addr_get
 *
 * Purpose:  get SMEM base address
 *
 * Parms:    none
 *
 * Return:   Sierra SMEM base address
 *
 * Abort:    none
 *
 * Notes:    will also map SMEM region if needed
 *
 ************/
_global uint8_t *ssmem_smem_base_addr_get(void)
{
  if (!ssmem_base)
  {
    ssmem_base = (unsigned char *)ioremap_nocache(SSMEM_MEM_BASE_ADDR,
                                                  SSMEM_MEM_SIZE);
    if (!ssmem_base)
    {
      SWI_PRINT(SWI_ERROR, "sierra_smem_base_addr_get error");
    }
  }

  return ssmem_base;
}

/************
 *
 * Name:     ssmem_dev_ioctl
 *
 * Purpose:  driver ioctl interface
 *
 * Parms:    file - file handle
 *           cmd  - command ID
 *           arg  - user data
 *
 * Return:   0 if success or other error code
 *
 * Abort:    none
 *
 * Notes:    It is protected by ssmem_ioctl_lock
 *
 ************/
_local long ssmem_dev_ioctl(
  struct file    *file,
  unsigned        cmd,
  unsigned long   arg)
{
  struct ssmem_ioctl_req_s __user *ioctl_user_reqp = NULL;
  struct ssmem_ioctl_req_s local_req;
  uint8_t *user_datap;
  int rc = 0, user_size;

  mutex_lock(&ssmem_ioctl_lock);

  ioctl_user_reqp = (struct ssmem_ioctl_req_s *)arg;


  if (copy_from_user((void *)&local_req, (void *)ioctl_user_reqp,
                     sizeof(struct ssmem_ioctl_req_s)))
  {
     SWI_PRINT(SWI_ERROR, "copy_from_user failed");
     rc = -EFAULT;
     goto end;
  }

  switch (cmd)
  {
    /* acquire */
    case SSMEM_IOCTL_ACQUIRE:
      user_datap = ssmem_acquire(local_req.region_id, local_req.user_version,
                                 local_req.user_size);

      if (!user_datap)
      {
        SWI_PRINT(SWI_ERROR, "region %d acquire failed, ver %d, size %d",
                  local_req.region_id, local_req.user_version,
                  local_req.user_size);
        rc = EFAULT;
        break;
      }

      /* success, copy to user if required */
      if (local_req.user_datap &&
          copy_to_user(local_req.user_datap, user_datap, local_req.user_size))
      {
        SWI_PRINT(SWI_ERROR, "copy_to_user failed");
        rc = -EFAULT;
        break;
      }
      break;

    /* get */
    case SSMEM_IOCTL_GET:
      user_datap = ssmem_get(local_req.region_id, local_req.user_version,
                             &user_size);

      if (!user_datap)
      {
        SWI_PRINT(SWI_ERROR, "region %d get failed, ver %x",
                  local_req.region_id, local_req.user_version);
        rc = EFAULT;
        break;
      }

      /* success, check if need to copy to user based on user_size */
      if (local_req.user_size > 0)
      {
        if (user_size > local_req.user_size)
        {
          /* provided user size is too small */
          SWI_PRINT(SWI_ERROR, "region %d invalid size: %d < %d",
                    local_req.region_id, user_size, local_req.user_size);
          rc = EFAULT;
          break;
        }

        if (local_req.user_datap &&
            copy_to_user(local_req.user_datap, user_datap, user_size))
        {
          SWI_PRINT(SWI_ERROR, "copy_to_user failed");
          rc = -EFAULT;
          break;
        }
      }

      /* return user_size unconditionally */
      if (copy_to_user(&(ioctl_user_reqp->user_size), &user_size, sizeof(uint32_t)))
      {
        SWI_PRINT(SWI_ERROR, "copy_to_user size failed");
        rc = -EFAULT;
        break;
      }
      break;

    /* update */
    case SSMEM_IOCTL_UPDATE:
      user_datap = ssmem_get(local_req.region_id, local_req.user_version,
                             &user_size);

      if (!user_datap)
      {
        SWI_PRINT(SWI_ERROR, "region %d update failed, ver %d",
                  local_req.region_id, local_req.user_version);
        rc = EFAULT;
        break;
      }

      /* must be exact size to update */
      if (user_size != local_req.user_size)
      {
        SWI_PRINT(SWI_ERROR, "region %d invalid size: %d , %d",
                  local_req.region_id, user_size, local_req.user_size);
        rc = EFAULT;
        break;
      }

      /* update region data and metadata */
      if (local_req.user_datap &&
          (copy_from_user(user_datap, local_req.user_datap, user_size) ||
           !ssmem_meta_update(local_req.region_id)))
      {
        SWI_PRINT(SWI_ERROR, "update region failed");
        rc = -EFAULT;
        break;
      }
      break;

    /* release */
    case SSMEM_IOCTL_RELEASE:
      if (!ssmem_release(local_req.region_id))
      {
        SWI_PRINT(SWI_ERROR, "region %d release failed", local_req.region_id);
        rc = EFAULT;
        break;
      }
      break;


    default:
      rc = -EINVAL;
      break;
  }

end:
  mutex_unlock(&ssmem_ioctl_lock);
  return rc;
}

/************
 *
 * Name:     ssmem_dev_read
 *
 * Purpose:  driver read interface
 *
 * Parms:    file   - file handle
 *           buf    - user buffer for the read
 *           count  - requested read size
 *           posp   - file position pointer
 *
 * Return:   read size or error number
 *
 * Abort:    none
 *
 * Notes:    It is protected by ssmem_ioctl_lock
 *           This is more a testing interface from Linux CLI.
 *           Region and its offset are specified by file offset:
 *           (region ID - 2 byte | offset - 2 byte)
 *           IOCTL should be used instead in most cases.
 *
 ************/
_local ssize_t ssmem_dev_read(
  struct file *file,
  char __user *buf,
  size_t       count,
  loff_t      *posp)
{
  loff_t  pos = *posp;
  int region_id, offset, user_size, read_size;
  uint8_t *user_datap;
  ssize_t retval = -EFAULT;

  mutex_lock(&ssmem_ioctl_lock);

  /* get region ID and offset */
  region_id = pos >> 16;
  offset = pos & 0xFFFF;

  user_datap = ssmem_get(region_id, 0, &user_size);

  if (user_datap && (user_size > offset))
  {
    read_size = user_size - offset;

    if (count < read_size)
    {
      read_size = count;
    }

    if (copy_to_user(buf, user_datap + offset, read_size))
    {
      SWI_PRINT(SWI_ERROR, "copy_to_user failed");
    }
    else
    {
      *posp += read_size;
      retval = read_size;
    }
  }

  mutex_unlock(&ssmem_ioctl_lock);
  return retval;
}

/************
 *
 * Name:     ssmem_dev_open
 *
 * Purpose:  driver open interface
 *
 * Parms:    inode - inode
 *           file  - file
 *
 * Return:   0 if success or other error code
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
_local int ssmem_dev_open(struct inode *inode, struct file *file)
{
  return 0;
}

/************
 *
 * Name:     ssmem_dev_release
 *
 * Purpose:  driver release interface
 *
 * Parms:    inode - inode
 *           file  - file
 *
 * Return:   0 if success or other error code
 *
 * Abort:    none
 *
 * Notes:    none
 *
 ************/
_local int ssmem_dev_release(struct inode *inode, struct file *file)
{
  return 0;
}

/************
 *
 * standard driver structure and functions below
 *
 ************/
_local struct file_operations ssmem_dev_fops = {
  .owner          = THIS_MODULE,
  .unlocked_ioctl = ssmem_dev_ioctl,
  .read           = ssmem_dev_read,
  .llseek         = default_llseek,
  .open           = ssmem_dev_open,
  .release        = ssmem_dev_release,
};

_local struct miscdevice ssmem_dev_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "sierra_ssmem",
  .fops = &ssmem_dev_fops,
};

static int ssmem_probe(struct platform_device *pdev)
{
  struct device_node *nodep;
  const __be32 *basep;
  u64 size, base;

  /* get SSMEM settings from device tree and do sanity check */
  if (pdev->dev.of_node)
  {
    nodep = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
    if (nodep)
    {
      basep = of_get_address(nodep, 0, &size, NULL);

      if (basep)
      {
        base = of_translate_address(nodep, basep);
        /* could check here (base != OF_BAD_ADDR) */

        if ((base != SSMEM_MEM_BASE_ADDR) ||
            (size != SSMEM_MEM_SIZE))
        {
          SWI_PRINT(SWI_ERROR, "ssmem settings incorrect %x, %x",
                    (unsigned int)base, (unsigned int)size);

          /* could panic or re-init ssmem based on new setting but this is not
           * necessary for now
           */
        }
      }
    }
  }

  ssmem_framework_one_time_init();
  return misc_register(&ssmem_dev_misc);
}

static int ssmem_remove(struct platform_device *pdev)
{
  misc_deregister(&ssmem_dev_misc);
  return 0;
}

static struct of_device_id ssmem_match[] = {
	{
		.compatible = "sierra,ssmem",
	},
	{}
};

static struct platform_driver ssmem_plat_driver = {
	.probe = ssmem_probe,
	.remove = ssmem_remove,
	.driver = {
		.name = "sierra_ssmem",
		.owner = THIS_MODULE,
		.of_match_table = ssmem_match,
	},
};

static int __init sierra_ssmem_init(void)
{
	return platform_driver_register(&ssmem_plat_driver);
}

static void __exit sierra_ssmem_exit(void)
{
	platform_driver_unregister(&ssmem_plat_driver);
}

device_initcall(sierra_ssmem_init);
module_exit(sierra_ssmem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sierra SSMEM driver");
