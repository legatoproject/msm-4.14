/*************
 *
 * Name:      aadebug_linux.h
 *
 * Purpose:   LK and Linux kernel include file for Sierra debugging utilities
 *
 *  Note:     this file is open source
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
 *************/

#ifndef AADEBUG_LINUX_H
#define AADEBUG_LINUX_H



/*
 * Little kernel
 */
#if defined(SWI_IMAGE_LK)

#include <debug.h>

#define SWI_ERROR                       CRITICAL
#define SWI_HIGH                        ALWAYS
#define SWI_MED                         INFO
#define SWI_LOW                         SPEW

#define SWI_PRINT                       dprintf
#define SWI_PRINTF                      dprintf

#define SWI_ASSERT                      assert

/*
 * Linux kernel images
 */
#elif defined(__KERNEL__)

#include <linux/module.h>

#define SWI_ERROR                       3
#define SWI_HIGH                        2
#define SWI_MED                         1
#define SWI_LOW                         0

#define SWI_PRINT(lvl, fmt, ...)        do { if (lvl) { printk(KERN_ERR "SWI %s: " fmt "\n", __func__, ##__VA_ARGS__); }} while (0)
#define SWI_PRINTF(lvl, fmt, ...)       do { if (lvl) { printk(KERN_ERR "SWI %s: " fmt "\n", __func__, ##__VA_ARGS__); }} while (0)

#define SWI_ASSERT(x)                   BUG_ON(!(x))


/*
 * Default - Linux/Cygwin
 */
#else

#define SWI_PROC_PREFIX

#define SWI_PRINT(lvl, ...)         fprintf(stderr, FMT_PFX __VA_ARGS__)
#define SWI_PRINTF(lvl, ...)        fprintf(stderr, FMT_PFX __VA_ARGS__)
#define SWI_LOG(lvl, ...)           fprintf(stderr, FMT_PFX __VA_ARGS__)

#define SWI_ASSERT(condition)

#endif



#endif /* AADEBUG_LINUX_H */
