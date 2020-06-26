/* arch/arm/mach-msm/sierra_tzdev.c
 *
 * OEM Secure File System(Secure storage) encrypt/decrypt files functions
 *
 * Copyright (c) 2016 Sierra Wireless, Inc
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
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <soc/qcom/scm.h>
#include <asm/cacheflush.h>
#include <linux/fs.h>
#include <linux/io.h>

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <linux/qcrypto.h>
#include <linux/delay.h>
#include <linux/clk.h>


// Include file that contains the command structure definitions.
#include "tzbsp_crypto_api.h"

struct tzdev_op_req {
  uint8_t*        enckey;
  uint32_t        encklen;
  uint8_t*        plain_data;
  uint32_t        plain_dlen;
  uint8_t*        encrypted_buffer;
  uint32_t        encrypted_len;
};

// Flags used to indicate which parts of tzdev_op_req structure are to be
// allocated and/or copied between kernel and user space.
#define TZDEV_COPY_ENCKEY           1
#define TZDEV_COPY_PLAIN_DATA       2
#define TZDEV_COPY_ENCRYPTED_BUFFER 4

// Maximum sizes for tzdev_op_req buffers
//
// Trivia: why is the TZDEV_MAX_ENCKEY so large?
// The tzdev_rsa_test utility declares and
// passes array of 2048 *bytes* for a keysize of 2048 *bits*,
// because the bit size constant is used for array declaration.
//
#define TZDEV_MAX_ENCKEY           2100
#define TZDEV_MAX_PLAIN_DATA       8000
#define TZDEV_MAX_ENCRYPTED_BUFFER 8000

// Context structure for data passing in ioctl routine
struct tzdev_ioctl_ctx {
  struct tzdev_op_req usr;      // bit copy of user request structure
  struct tzdev_op_req krn;      // kernel side structure with own buffers.
  struct tzdev_op_req __user *orig_usr; // original user space pointer
};

// SCM command structure for encapsulating the request and response addresses.
struct scm_cmd_buf_s
{
  uint32_t req_addr;
  uint32_t req_size;
  uint32_t resp_addr;
  uint32_t resp_size;
};

#define TZDEV_IOCTL_MAGIC      0x9B

#define TZDEV_IOCTL_KEYGEN_REQ          _IOWR(TZDEV_IOCTL_MAGIC, 0x16, struct tzdev_op_req)
#define TZDEV_IOCTL_SEAL_REQ            _IOWR(TZDEV_IOCTL_MAGIC, 0x17, struct tzdev_op_req)
#define TZDEV_IOCTL_UNSEAL_REQ          _IOWR(TZDEV_IOCTL_MAGIC, 0x18, struct tzdev_op_req)

#define SCM_SYM_ID_CMD         0x3

static int sierra_tzdev_open_times = 0; /* record device open times, for shared resources using */

static struct crypto_clock {
  char *name;
  struct clk *clk;
} crypto_clk_list[] = {
  {"crypto_clk_src", NULL},
  {"gcc_crypto_clk", NULL},
  {"gcc_crypto_axi_clk", NULL},
  {"gcc_crypto_ahb_clk", NULL}
};

static void tzdev_clock_deinit(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(crypto_clk_list); i++) {
    struct crypto_clock *clock = &(crypto_clk_list[i]);
    if (clock->clk)
      clk_put(clock->clk);
  }
}

static int tzdev_clock_init(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(crypto_clk_list); i++) {
    struct crypto_clock *clock = &(crypto_clk_list[i]);
    if (!clock->clk) {
      /* get clock */
      clock->clk = clk_get_sys(NULL, clock->name);
      if (IS_ERR(clock->clk)) {
        pr_err("%s, unknown clock %s\n", __func__, clock->name);
        clock->clk = NULL;
        goto fail_release_clocks;
      }
    }
  }
  return 0;

fail_release_clocks:
  tzdev_clock_deinit();
  return -ENODEV;
}

static void tzdev_clock_prepare_enable(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(crypto_clk_list); i++) {
    int ret;
    struct crypto_clock *clock = &(crypto_clk_list[i]);
    BUG_ON(!clock->clk);
    ret = clk_prepare_enable(clock->clk);
  }
}

static void tzdev_clock_disable_unprepare(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(crypto_clk_list); i++) {
    struct crypto_clock *clock = &(crypto_clk_list[i]);
    BUG_ON(!clock->clk);
    clk_disable_unprepare(clock->clk);
  }
}

static int tzdev_storage_service_generate_key(uint8_t *key_material, uint32_t* key_size )
{
  // Define request and response structures.
  tz_storage_service_gen_key_cmd_t *gen_key_reqp = NULL;
  tz_storage_service_gen_key_resp_t *gen_key_respp = NULL;
  int rc = 0;
  int key_material_len = *key_size;

  gen_key_reqp = kmalloc(sizeof(tz_storage_service_gen_key_cmd_t), GFP_KERNEL);
  gen_key_respp = kmalloc(sizeof(tz_storage_service_gen_key_resp_t), GFP_KERNEL);
  if ((!gen_key_reqp) || (!gen_key_respp))
  {
    printk(KERN_CRIT "%s()_line%d: cannot allocate req/resp\n", __func__,__LINE__);
    rc = -ENOMEM;
    goto end;
  }

  memset(key_material, 0, key_material_len);
  memset(gen_key_reqp, 0, sizeof(tz_storage_service_gen_key_cmd_t));
  memset(gen_key_respp, 0, sizeof(tz_storage_service_gen_key_resp_t));


  // Populate generate key request structure.
  gen_key_reqp->cmd_id = TZ_STOR_SVC_GENERATE_KEY;
  gen_key_reqp->key_blob.key_material = (void *)SCM_BUFFER_PHYS(key_material);
  gen_key_reqp->key_blob.key_material_len = key_material_len;

  // Flush memory
  dmac_flush_range(key_material, ((void *)key_material + (sizeof(uint8_t) * key_material_len)));
  dmac_flush_range(gen_key_reqp, ((void *)gen_key_reqp + (sizeof(tz_storage_service_gen_key_cmd_t))));
  dmac_flush_range(gen_key_respp, ((void *)gen_key_respp + (sizeof(tz_storage_service_gen_key_resp_t))));

  // Call into TZ
  if (is_scm_armv8()) {
    struct scm_desc desc = {0};
    desc.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL, SCM_RW, SCM_VAL);
    desc.args[0] = SCM_BUFFER_PHYS(gen_key_reqp);
    desc.args[1] = SCM_BUFFER_SIZE(tz_storage_service_gen_key_cmd_t);
    desc.args[2] = SCM_BUFFER_PHYS(gen_key_respp);
    desc.args[3] = SCM_BUFFER_SIZE(tz_storage_service_gen_key_resp_t);

    tzdev_clock_prepare_enable();
    rc = scm_call2(SCM_SIP_FNID(SCM_SVC_CRYPTO, SCM_SYM_ID_CMD), &desc);
    tzdev_clock_disable_unprepare();
  } else {
    struct scm_cmd_buf_s scm_cmd_buf = {0};
    // Populate scm command structure.
    scm_cmd_buf.req_addr = SCM_BUFFER_PHYS(gen_key_reqp);
    scm_cmd_buf.req_size = SCM_BUFFER_SIZE(tz_storage_service_gen_key_cmd_t);
    scm_cmd_buf.resp_addr = SCM_BUFFER_PHYS(gen_key_respp);
    scm_cmd_buf.resp_size = SCM_BUFFER_SIZE(tz_storage_service_gen_key_resp_t);

    rc = scm_call(TZ_SVC_CRYPTO, TZ_CRYPTO_SERVICE_SYM_ID,
            (void *)&scm_cmd_buf,
            SCM_BUFFER_SIZE(scm_cmd_buf),
            NULL,
            0);
  }

  // Check return value
  if (rc)
  {
    pr_err("%s()_no%d: scm_call fail  with return val %d\n",__func__,__LINE__, rc);
    goto end;
  }

  // Invalidate cache
  dmac_inv_range(gen_key_respp, ((void *)gen_key_respp + (sizeof(tz_storage_service_gen_key_resp_t))));
  dmac_inv_range(key_material, ((void *)key_material + (sizeof(uint8_t) * key_material_len)));

  // Check return value
  if (gen_key_respp->status != 0)
  {
    pr_err("%s()_line%d: resp.status=%d\n",__func__,__LINE__, gen_key_respp->status);
    rc = -1;
    goto end;
  }

  // Sanity check of command id.
  if (TZ_STOR_SVC_GENERATE_KEY != gen_key_respp->cmd_id)
  {
    pr_err("%s()_line%d: resp.cmd_id %d not matched\n",__func__,__LINE__, gen_key_respp->cmd_id);
    rc = -1;
    goto end;
  }

  *key_size = gen_key_respp->key_blob_size;

end:
  // Free data buffers
  if(gen_key_reqp)
  {
    kfree(gen_key_reqp);
  }

  if(gen_key_respp)
  {
    kfree(gen_key_respp);
  }

  return rc;
}

static int tzdev_seal_data_using_aesccm(uint8_t *plain_data, uint32_t plain_data_len,
  uint8_t *sealed_buffer, uint32_t *sealed_data_len,uint8_t *key_material, uint32_t key_material_len)
{
  // Define request and response structures.
  tz_storage_service_seal_data_cmd_t *seal_data_reqp = NULL;
  tz_storage_service_seal_data_resp_t *seal_data_respp = NULL;
  uint32_t sealed_len = *sealed_data_len;
  int rc = 0;

  seal_data_reqp = kmalloc(sizeof(tz_storage_service_seal_data_cmd_t), GFP_KERNEL);
  seal_data_respp = kmalloc(sizeof(tz_storage_service_seal_data_resp_t), GFP_KERNEL);
  if ((!seal_data_reqp) || (!seal_data_respp))
  {
    printk(KERN_CRIT "%s()_line%d: cannot allocate req/resp\n", __func__,__LINE__);
    rc = -1;
    goto end;
  }

  memset(seal_data_reqp, 0, sizeof(tz_storage_service_seal_data_cmd_t));
  memset(seal_data_respp, 0, sizeof(tz_storage_service_seal_data_resp_t));

  memset(sealed_buffer, 0, sealed_len);

  // Populate seal data request structure.
  seal_data_reqp->cmd_id = TZ_STOR_SVC_SEAL_DATA;
  // Key material is the output from the generate key command.
  seal_data_reqp->key_blob.key_material = (void *)SCM_BUFFER_PHYS(key_material);
  seal_data_reqp->key_blob.key_material_len = key_material_len;
  // Plain data
  seal_data_reqp->plain_data = (uint8 *)SCM_BUFFER_PHYS(plain_data);
  seal_data_reqp->plain_dlen = plain_data_len;
  // Output buffer
  seal_data_reqp->output_buffer = (uint8 *)SCM_BUFFER_PHYS(sealed_buffer);
  seal_data_reqp->output_len = sealed_len;

  // Flush memory
  dmac_flush_range(plain_data, ((void *)plain_data + (sizeof(uint8_t) * plain_data_len)));
  dmac_flush_range(sealed_buffer, ((void *)sealed_buffer + (sizeof(uint8_t) * sealed_len)));
  dmac_flush_range(key_material, ((void *)key_material + (sizeof(uint8_t) * key_material_len)));
  dmac_flush_range(seal_data_reqp, ((void *)seal_data_reqp + (sizeof(tz_storage_service_seal_data_cmd_t))));
  dmac_flush_range(seal_data_respp, ((void *)seal_data_respp + (sizeof(tz_storage_service_seal_data_resp_t))));

  // Call into TZ
  if (is_scm_armv8()) {
    struct scm_desc desc = {0};
    desc.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL, SCM_RW, SCM_VAL);
    desc.args[0] = SCM_BUFFER_PHYS(seal_data_reqp);
    desc.args[1] = SCM_BUFFER_SIZE(tz_storage_service_seal_data_cmd_t);
    desc.args[2] = SCM_BUFFER_PHYS(seal_data_respp);
    desc.args[3] = SCM_BUFFER_SIZE(tz_storage_service_seal_data_resp_t);

    tzdev_clock_prepare_enable();
    rc = scm_call2(SCM_SIP_FNID(SCM_SVC_CRYPTO, SCM_SYM_ID_CMD), &desc);
    tzdev_clock_disable_unprepare();
  } else {
    struct scm_cmd_buf_s scm_cmd_buf = {0};
    // Populate scm command structure.
    scm_cmd_buf.req_addr = SCM_BUFFER_PHYS(seal_data_reqp);
    scm_cmd_buf.req_size = SCM_BUFFER_SIZE(tz_storage_service_seal_data_cmd_t);
    scm_cmd_buf.resp_addr = SCM_BUFFER_PHYS(seal_data_respp);
    scm_cmd_buf.resp_size = SCM_BUFFER_SIZE(tz_storage_service_seal_data_resp_t);

    rc = scm_call(TZ_SVC_CRYPTO, TZ_CRYPTO_SERVICE_SYM_ID,
        (void *)&scm_cmd_buf,
        SCM_BUFFER_SIZE(scm_cmd_buf),
        NULL,
        0);
  }

  // Check return value
  if (rc)
  {
    pr_err("%s()_%d: scm_call failed, rc=%d \n", __func__,__LINE__,rc);
    goto end;
  }

  dmac_inv_range(seal_data_respp, ((void *)seal_data_respp + (sizeof(tz_storage_service_seal_data_resp_t))));
  dmac_inv_range(sealed_buffer, ((void *)sealed_buffer + (sizeof(uint8_t) * sealed_len)));

  // Check return value
  if (seal_data_respp->status != 0)
  {
    pr_err("%s(): TZ_STOR_SVC_SEAL_DATA status: %d\n", __func__,
            seal_data_respp->status);
    rc = -1;
    goto end;
  }

  // Sanity check of command id.
  if (TZ_STOR_SVC_SEAL_DATA != seal_data_respp->cmd_id)
  {
    pr_err("%s(): TZ_STOR_SVC_SEAL_DATA invalid cmd_id: %d\n", __func__,
            seal_data_respp->cmd_id);
    rc = -1;
    goto end;
  }

  *sealed_data_len = seal_data_respp->sealed_data_len;

end:
  // Free req buffer
  if(seal_data_reqp)
  {
    kfree(seal_data_reqp);
  }
  if(seal_data_respp)
  {
    kfree(seal_data_respp);
  }

  return rc;
}

static int tzdev_unseal_data_using_aesccm(uint8_t *sealed_buffer, uint32_t sealed_buffer_len,
  uint8_t *output_buffer_unseal, uint32_t *unseal_len,uint8_t *key_material, uint32_t key_material_len )
{
  // Define request and response structures.
  tz_storage_service_unseal_data_cmd_t *unseal_datareqp;
  tz_storage_service_unseal_data_resp_t *unseal_datarespp;
  uint32_t output_len_unseal = *unseal_len;
  int rc = 0;

  unseal_datareqp = kmalloc(sizeof(tz_storage_service_unseal_data_cmd_t), GFP_KERNEL);
  unseal_datarespp = kmalloc(sizeof(tz_storage_service_unseal_data_resp_t), GFP_KERNEL);
  if ((!unseal_datareqp) || (!unseal_datarespp))
  {
    printk(KERN_CRIT "%s()_line%d: cannot allocate req/resp\n", __func__,__LINE__);
    rc = -1;
    goto end;
  }

  memset(unseal_datareqp, 0, sizeof(tz_storage_service_unseal_data_cmd_t));
  memset(unseal_datarespp, 0, sizeof(tz_storage_service_unseal_data_resp_t));

  memset(output_buffer_unseal, 0, output_len_unseal);

  // Populate seal data request structure.
  unseal_datareqp->cmd_id = TZ_STOR_SVC_UNSEAL_DATA;
  // Key material is the output from the generate key command.
  unseal_datareqp->key_blob.key_material =  (void *)SCM_BUFFER_PHYS(key_material);
  unseal_datareqp->key_blob.key_material_len = key_material_len;
  // Encrypted data
  unseal_datareqp->sealed_data = (void *)SCM_BUFFER_PHYS(sealed_buffer);
  unseal_datareqp->sealed_dlen = sealed_buffer_len;

  // Plain data
  unseal_datareqp->output_buffer = (void *)SCM_BUFFER_PHYS(output_buffer_unseal);
  unseal_datareqp->output_len = output_len_unseal;

  // Flush memory
  dmac_flush_range(sealed_buffer, ((void *)sealed_buffer + (sizeof(uint8_t) * sealed_buffer_len)));
  dmac_flush_range(output_buffer_unseal, ((void *)output_buffer_unseal + (sizeof(uint8_t) * output_len_unseal)));
  dmac_flush_range(key_material, ((void *)key_material + (sizeof(uint8_t) * key_material_len)));
  dmac_flush_range(unseal_datareqp, ((void *)unseal_datareqp + (sizeof(tz_storage_service_unseal_data_cmd_t))));
  dmac_flush_range(unseal_datarespp, ((void *)unseal_datarespp + (sizeof(tz_storage_service_unseal_data_resp_t))));

  // Call into TZ
  if (is_scm_armv8()) {
    struct scm_desc desc = {0};
    desc.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL, SCM_RW, SCM_VAL);
    desc.args[0] = SCM_BUFFER_PHYS(unseal_datareqp);
    desc.args[1] = SCM_BUFFER_SIZE(tz_storage_service_unseal_data_cmd_t);
    desc.args[2] = SCM_BUFFER_PHYS(unseal_datarespp);
    desc.args[3] = SCM_BUFFER_SIZE(tz_storage_service_unseal_data_resp_t);

    tzdev_clock_prepare_enable();
    rc = scm_call2(SCM_SIP_FNID(SCM_SVC_CRYPTO, SCM_SYM_ID_CMD), &desc);
    tzdev_clock_disable_unprepare();
  } else {
    struct scm_cmd_buf_s scm_cmd_buf = {0};
    // Populate scm command structure.
    scm_cmd_buf.req_addr = SCM_BUFFER_PHYS(unseal_datareqp);
    scm_cmd_buf.req_size = SCM_BUFFER_SIZE(tz_storage_service_unseal_data_cmd_t);
    scm_cmd_buf.resp_addr = SCM_BUFFER_PHYS(unseal_datarespp);
    scm_cmd_buf.resp_size = SCM_BUFFER_SIZE(tz_storage_service_unseal_data_resp_t);

    rc = scm_call(TZ_SVC_CRYPTO, TZ_CRYPTO_SERVICE_SYM_ID,
        (void *)&scm_cmd_buf,
        SCM_BUFFER_SIZE(scm_cmd_buf),
        NULL,
        0);
  }

  // Check return value
  if (rc)
  {
    pr_err("%s(): TZ_STOR_SVC_UNSEAL_DATA ret: %d\n", __func__, rc);
    goto end;
  }

  // Invalidate cache
  dmac_inv_range(unseal_datarespp, ((void *)unseal_datarespp + (sizeof(tz_storage_service_unseal_data_cmd_t))));
  dmac_inv_range(output_buffer_unseal, ((void *)output_buffer_unseal + (sizeof(uint8_t) * output_len_unseal)));

  // Check response status
  if (unseal_datarespp->status != 0)
  {
    pr_err("%s(): TZ_STOR_SVC_UNSEAL_DATA status: %d\n", __func__,
            unseal_datarespp->status);
    rc = -1;
    goto end;
  }

  // Sanity check of cmd_id
  if (TZ_STOR_SVC_UNSEAL_DATA != unseal_datarespp->cmd_id)
  {
    pr_err("%s(): TZ_STOR_SVC_UNSEAL_DATA invalid cmd_id: %d\n", __func__,
            unseal_datarespp->cmd_id);
    rc = -1;
    goto end;
  }

  *unseal_len = unseal_datarespp->unsealed_data_len;

end:
  // free req buffer
  if(unseal_datareqp)
  {
    kfree(unseal_datareqp);
  }
  if(unseal_datarespp)
  {
    kfree(unseal_datarespp);
  }

  return rc;
}

static void sierra_tzdev_free_req_buffers(struct tzdev_ioctl_ctx *tic)
{
  struct tzdev_op_req *krn = &tic->krn;

  if (krn->encklen)
    kfree(krn->enckey);
  if (krn->plain_data)
    kfree(krn->plain_data);
  if (krn->encrypted_buffer)
    kfree(krn->encrypted_buffer);
}

//
// This is called at the beginning of the ioctl operation
// to copy the request structure from user to kernel space.
// After this we have all the pointers and lengths; we still
// need to do copy_from_user to get the data itself.
//
// Here we also store the original user space request pointer
// into the tic structure. Later we need that when we copy
// in the reverse direction as the last step of the ioctl,
// in sierra_tzdev_copy_to_user.
//
// The two levels of copying from user space are split into the
// two functions sierrqa_tzdev_ioctl_prepare and sierra_tzdev_copy_from_user
// because the second function is invoked more than once to
// copy the selected buffers or to just allocate memory.
//
// tic:         context holding copy of user request and
//              the kernel facsimile of that structure
// req:         pointer to user request
//
// Returns a standard Linux error: -EFAULT or 0.
//
static int sierra_tzdev_ioctl_prepare(struct tzdev_ioctl_ctx *tic,
                                      struct tzdev_op_req __user *req)
{
  tic->orig_usr = req;
  return copy_from_user(&tic->usr, req, sizeof tic->usr) ? -EFAULT : 0;
}

//
// Copy selected parts of the tzdev_op_req data from user
// space to kernel space. This is used by sierra_tzdev_ioctl
// for receiving in or in/out parameters, and for reserving space
// for out parameters.
//
// Comments in the sierra_tzdev_ioctl, where it handles the
// TZDEV_IOCTL_SEAL_REQ command, give some hints about the usage
// of these functions.
//
// tic:         context holding copy of user request and
//              the kernel facsimile of that structure
// flags:       bitmask indicating which fields to operate on.
// alloc_only:  boolean indicating that a buffers are to be
//              allocated only, without doing any copying
//              from user space. This is useful for preparing
//              space for data to be returned to user space later
//              (i.e. "out parameters" of the ioctl).
//
// For every field group specified by flags, the function reads the length of
// the buffer from the user space src parameter, and copies that length to the
// same field in the dst parameter. Then in the dst structure, it allocates a
// buffer that large, and puts it in the pointer associated with the length.
// Unless alloc_only is true, then a copy_from_user operation is performed
// to fill the allocated buffer from its user space counterpart.
//
// Returns a standard Linux error: 0 success, -EFAULT, etc.
//
static int sierra_tzdev_copy_from_user(struct tzdev_ioctl_ctx *tic,
                                       unsigned flags,
                                       bool alloc_only)
{
  int rc = -EFAULT;
  struct tzdev_op_req *src = &tic->usr;
  struct tzdev_op_req *dst = &tic->krn;

  if (flags & TZDEV_COPY_ENCKEY) {
    dst->encklen = src->encklen;

    if (dst->encklen > TZDEV_MAX_ENCKEY) {
      printk(KERN_INFO "%s()_line%d: %lu byte key too large\n",
             __func__, __LINE__, (unsigned long) dst->encklen);
      rc = -ENOSPC;
      goto out;
    }

    if ((dst->enckey = kmalloc(dst->encklen, GFP_KERNEL)) == NULL) {
      printk(KERN_CRIT "%s()_line%d: cannot allocate key_material\n",
             __func__, __LINE__);
      rc = -ENOMEM;
      goto out;
    }
    if (!alloc_only &&
        copy_from_user(dst->enckey,
                       src->enckey,
                       dst->encklen) != 0)
      goto out;
  }

  if (flags & TZDEV_COPY_PLAIN_DATA) {
    dst->plain_dlen = src->plain_dlen;

    if (dst->plain_dlen > TZDEV_MAX_PLAIN_DATA) {
      printk(KERN_INFO "%s()_line%d: %lu byte plain data too large\n",
             __func__, __LINE__, (unsigned long) dst->plain_dlen);
      rc = -ENOSPC;
      goto out;
    }

    if ((dst->plain_data = kmalloc(dst->plain_dlen, GFP_KERNEL)) == NULL) {
      printk(KERN_CRIT "%s()_line%d: cannot allocate plain data\n",
             __func__, __LINE__);
      rc = -ENOMEM;
      goto out;
    }
    if (!alloc_only && copy_from_user(dst->plain_data,
                                      src->plain_data,
                                      dst->plain_dlen) != 0)
      goto out;
  }

  if (flags & TZDEV_COPY_ENCRYPTED_BUFFER) {
    dst->encrypted_len = src->encrypted_len;

    if (dst->encrypted_len > TZDEV_MAX_ENCRYPTED_BUFFER) {
      printk(KERN_INFO "%s()_line%d: %lu byte encrypted data too large\n",
             __func__, __LINE__, (unsigned long) dst->encrypted_len);
      rc = -ENOSPC;
      goto out;
    }

    if ((dst->encrypted_buffer = kmalloc(dst->encrypted_len, GFP_KERNEL)) == NULL) {
      printk(KERN_CRIT "%s()_line%d: cannot allocate sealed data\n",
             __func__, __LINE__);
      rc = -ENOMEM;
      goto out;
    }
    if (!alloc_only && copy_from_user(dst->encrypted_buffer,
                                      src->encrypted_buffer,
                                      dst->encrypted_len) != 0)
      goto out;
  }

  rc = 0;

out:
  if (rc != 0 && rc != -ENOMEM)
    printk(KERN_ERR "%s: copy_from_user failed\n", __func__);

  return rc;
}

//
// Copy selected parts of the tzdev_op_req data from kernel
// space to user space. This is used by sierra_tzdev_ioctl
// for passing out parmeters back to user space.
//
// tic:         context holding copy of user request and
//              the kernel facsimile of that structure
// flags:       bitmask indicating which fields to operate on.
//
// For every field group specified by flags, the function reads the length of
// the buffer from the kernel space src parameter, and copies that length to the
// same field in the user-space dst structure. It also copies the associated
// buffer from kernel space to user space, using the respective pointer
// fields in the two structures.
//
// Returns a standard Linux error: 0 success, -EFAULT, etc.
//
static int sierra_tzdev_copy_to_user(struct tzdev_ioctl_ctx *tic,
                                     unsigned flags)
{
  int rc = -EFAULT;
  struct tzdev_op_req *src = &tic->krn;
  struct tzdev_op_req *dst = &tic->usr;

  if (flags & TZDEV_COPY_ENCKEY) {
    dst->encklen = src->encklen;
    if ((copy_to_user(dst->enckey, src->enckey, src->encklen)) != 0)
      goto out;
  }

  if (flags & TZDEV_COPY_PLAIN_DATA) {
    dst->plain_dlen = src->plain_dlen;
    if ((copy_to_user(dst->plain_data, src->plain_data, src->plain_dlen)) != 0)
      goto out;
  }

  if (flags & TZDEV_COPY_ENCRYPTED_BUFFER) {
    dst->encrypted_len = src->encrypted_len;
    if ((copy_to_user(dst->encrypted_buffer, src->encrypted_buffer,
                      src->encrypted_len)) != 0)
      goto out;
  }

  if ((copy_to_user(tic->orig_usr, &tic->usr, sizeof *tic->orig_usr)) != 0)
    goto out;

  rc = 0;

out:
  if (rc != 0)
    printk(KERN_ERR "%s: copy_to_user/put_user failed\n", __func__);

  return rc;
}

static long sierra_tzdev_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
  struct tzdev_ioctl_ctx tic = { {0} };
  struct tzdev_op_req *krn = &tic.krn;
  int rc  = sierra_tzdev_ioctl_prepare(&tic,
                                       (struct tzdev_op_req __user *) arg);

  if (rc != 0)
    goto out;

  switch (cmd)
  {
    /*Generate keyblob*/
    case TZDEV_IOCTL_KEYGEN_REQ:
      {
        uint32_t output_len;

        if ((rc = sierra_tzdev_copy_from_user(&tic,
                                              TZDEV_COPY_ENCKEY,
                                              true)) != 0)
          goto out;

        output_len = krn->encklen;

        rc = tzdev_storage_service_generate_key(krn->enckey, &output_len);

        pr_info("%s()_line%d:TZDEV_IOCTL_KEYGEN_REQ, get key_size:%d, rc=%d\n",
                 __func__, __LINE__, output_len, rc);

        if (rc != 0)
          goto out;

        if (output_len > krn->encklen) {
          rc = -EFAULT;
          goto out;
        }

        krn->encklen = output_len;

        rc = sierra_tzdev_copy_to_user(&tic, TZDEV_COPY_ENCKEY);
      }
      break;

    /* encrypt data */
    case TZDEV_IOCTL_SEAL_REQ:
      {
        uint32_t output_len;

        //
        // Copy the in-parameters from user space to kernel space.
        // For this operation, we need the encryption key, and the plain data,
        // so we specify these two in the bitmask.
        //
        if ((rc = sierra_tzdev_copy_from_user(&tic,
                                              TZDEV_COPY_ENCKEY | TZDEV_COPY_PLAIN_DATA,
                                              false)) != 0) // alloc_only is false: we really copy
          goto out;

        //
        // We must reserve, in the kernel-side request structure, a buffer to
        // receive the encrypted block. To do this we use the same function, this
        // time using the flags parameter to select the encrypted buffer,
        // and set alloc_only to true, so the payload is not copied from user
        // space, only the length field is copied. The kernel-side buffer is reserved
        // only, without being initialized from user space.
        //
        if ((rc = sierra_tzdev_copy_from_user(&tic,
                                              TZDEV_COPY_ENCRYPTED_BUFFER,
                                              true)) != 0) // alloc_only true: reserve space only
          goto out;

        //
        // The kernel buffer now has the encrypted_len set by the second
        // sierra_tzdev_copy_from_user above. We use this temporary variable,
        // rather than passing &krn->encrypted_len directly to the crypto
        // routine. We can then perform a sanity check on this.
        //
        output_len = krn->encrypted_len;

        rc = tzdev_seal_data_using_aesccm(krn->plain_data, krn->plain_dlen,
                                          krn->encrypted_buffer, &output_len,
                                          krn->enckey, krn->encklen);

        pr_info("%s()_line%d: TZDEV_IOCTL_SEAL_REQ: plain_data_len:%d, seal_data_len:%d, rc=%d\n",
                __func__, __LINE__, krn->plain_dlen, output_len, rc);

        if (rc != 0)
          goto out;

        if (output_len > krn->encrypted_len) {
          rc = -EFAULT;
          goto out;
        }

        //
        // Store back the actual length reported by the crypto function.
        //
        krn->encrypted_len = output_len;

        //
        // Now propagate the encrypted buffer up to user space.
        //
        rc = sierra_tzdev_copy_to_user(&tic, TZDEV_COPY_ENCRYPTED_BUFFER);
      }
      break;

    /* decrypt data */
    case TZDEV_IOCTL_UNSEAL_REQ:
      {
        uint32_t output_len;

        if ((rc = sierra_tzdev_copy_from_user(&tic,
                                              TZDEV_COPY_ENCKEY | TZDEV_COPY_ENCRYPTED_BUFFER,
                                              false)) != 0)
          goto out;

        if ((rc = sierra_tzdev_copy_from_user(&tic,
                                              TZDEV_COPY_PLAIN_DATA,
                                              true)) != 0)
          goto out;

        output_len = krn->plain_dlen;

        rc = tzdev_unseal_data_using_aesccm(krn->encrypted_buffer, krn->encrypted_len,
                                            krn->plain_data, &output_len,
                                            krn->enckey, krn->encklen);
        pr_info("%s()_line%d: TZDEV_IOCTL_UNSEAL_REQ: sealed data len:%d, plain_data_len:%d, rc=%d\n",
                 __func__, __LINE__, krn->encrypted_len, output_len, rc);

        if (rc != 0)
          goto out;

        if (output_len > krn->plain_dlen) {
          rc = -EFAULT;
          goto out;
        }

        krn->plain_dlen = output_len;

        if (sierra_tzdev_copy_to_user(&tic, TZDEV_COPY_PLAIN_DATA) != 0)
          goto out;
      }
      break;

    default:
      rc = -EINVAL;
      break;
  }

out:
  sierra_tzdev_free_req_buffers(&tic);
  return rc;
}


static int sierra_tzdev_open(struct inode *inode, struct file *file)
{
  sierra_tzdev_open_times++;
  pr_info("%s()_%d: sierra_tzdev_open_times=%d \n", __func__,__LINE__,sierra_tzdev_open_times);
  return 0;
}

static int sierra_tzdev_release(struct inode *inode, struct file *file)
{
  sierra_tzdev_open_times--;
  pr_info("%s()_%d: tzdev_driver_open_times=%d \n", __func__,__LINE__,sierra_tzdev_open_times);
  if(sierra_tzdev_open_times < 0)
  {
    return ENODEV;
  }
  else
  {
    return 0;
  }
}

static struct file_operations sierra_tzdev_fops = {
  .owner = THIS_MODULE,
  .unlocked_ioctl = sierra_tzdev_ioctl,
  .open = sierra_tzdev_open,
  .release = sierra_tzdev_release,
};

static struct miscdevice sierra_tzdev_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "tzdev",
  .fops = &sierra_tzdev_fops,
};

static int __init sierra_tzdev_init(void)
{
  tzdev_clock_init();
  return misc_register(&sierra_tzdev_misc);
}

static void __exit sierra_tzdev_exit(void)
{
  misc_deregister(&sierra_tzdev_misc);
  tzdev_clock_deinit();
}

module_init(sierra_tzdev_init);
module_exit(sierra_tzdev_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure storage driver");

