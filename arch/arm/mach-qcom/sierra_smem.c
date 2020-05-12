/* arch/arm/mach-msm/sierra_smem.c
 *
 * Copyright (c) 2012 Sierra Wireless, Inc
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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/io.h>

#include <mach/sierra_smem.h>
/* mach-msm/include is in the include path, add sierra/api to it */
#include <../sierra/api/ssmemudefs.h>

unsigned char * sierra_smem_base_addr_get(void)
{
        return ssmem_smem_base_addr_get();
}

static ssize_t sierra_smem_read(struct file *fp, char __user *buf,
                                size_t count, loff_t *posp)
{
        unsigned char * memp = sierra_smem_base_addr_get();
        loff_t  pos = *posp;
        ssize_t retval = -EFAULT;

        if (!memp) {
                pr_err("%s: failed to aquire memory region\n", __func__);
                retval = -ENOMEM;
        }
        else if (pos <= SIERRA_SMEM_SIZE) {

                /* truncate to the end of the region, if needed */
                ssize_t len = min(count, (size_t)(SIERRA_SMEM_SIZE - pos));
                memp += pos;

                retval = (ssize_t)copy_to_user(buf, memp, len);

                if (retval) {
                        pr_err("%s: failed to copy %d bytes\n", __func__, (int)retval);
                }
                else {
                        *posp += len;
                        retval = len;
                }
        }

        return retval;
}

/* the write function will be used mainly to write errdump related fields
 * offset field will indicate which field to update
 */
static ssize_t sierra_smem_write(struct file *fp, const char __user *buf,
                                 size_t count, loff_t *posp)
{
        unsigned char * memp = sierra_smem_base_addr_get();
        loff_t  pos = *posp;
        ssize_t retval = -EFAULT;

        if (!memp) {
                pr_err("%s: failed to aquire memory region\n", __func__);
                retval = -ENOMEM;
        }
        else if (pos <= SIERRA_SMEM_SIZE) {
                /* truncate to the end of the region, if needed */
                ssize_t len = min(count, (size_t)(SIERRA_SMEM_SIZE - pos));
                memp += pos;

                retval = (ssize_t)copy_from_user(memp, buf, len);

                if (retval) {
                        pr_err("%s: failed to copy %d bytes\n", __func__, (int)retval);
                }
                else {
                        *posp += len;
                        retval = len;
                }
        }

        return retval;
}

static int sierra_smem_open(struct inode *inode, struct file *file)
{
        if(!sierra_smem_base_addr_get()) {
                return -EFAULT;
        }
        else {
                return 0;
        }
}

static int sierra_smem_release(struct inode *inode, struct file *file)
{
        return 0;
}

static struct file_operations sierra_smem_fops = {
        .owner = THIS_MODULE,
        .read = sierra_smem_read,
        .write = sierra_smem_write,
        .llseek = default_llseek,
        .open = sierra_smem_open,
        .release = sierra_smem_release,
};

static struct miscdevice sierra_smem_misc = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = "sierra_smem",
        .fops = &sierra_smem_fops,
};

static int __init sierra_smem_init(void)
{
        (void)sierra_smem_base_addr_get();
        return misc_register(&sierra_smem_misc);
}

static void __exit sierra_smem_exit(void)
{
        misc_deregister(&sierra_smem_misc);
}

module_init(sierra_smem_init);
module_exit(sierra_smem_exit);

MODULE_AUTHOR("Brad Du <bdu@sierrawireless.com>");
MODULE_DESCRIPTION("Sierra SMEM driver");
MODULE_LICENSE("GPL v2");

/* export a compatibilty string to allow this driver to reserve the
 * needed physical region via the dev tree
 */
//EXPORT_COMPAT("qcom,sierra-smem");
