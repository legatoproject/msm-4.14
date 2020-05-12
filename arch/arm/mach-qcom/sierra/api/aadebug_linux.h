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
