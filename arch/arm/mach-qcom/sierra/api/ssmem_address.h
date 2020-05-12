/************
 *
 * Filename:  ssmem_address.h
 *
 * Purpose:   global define for ssmem region address
 *
 * NOTES:     this file will be used in multiple places.
 *            it will also be used in sbl.scl compiling so keep it clean and don't include
 *            other header files in this file
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/

#ifndef SSMEM_ADDRESS_H
#define SSMEM_ADDRESS_H

/* External Definitions */

/* Sierra shared memory
 *
 * A section of memory at the top of DDR is reserved for Sierra
 * boot-app messages, crash information, and other data.  This section
 * is non-initialized in order to preserve data across reboots.  The
 * size and base are (unfortunately) defined in multiple places.
 *
 * WARNING:  These definitions must be kept in sync
 *
 * modem_proc/config/9635/cust_config.xml
 * apps_proc/kernel/arch/arm/boot/dts/qcom/mdm9630.dtsi
 *
 */

#ifndef DDR_MEM_BASE
#define DDR_MEM_BASE 0x80000000
#endif

#define SIERRA_MEM_SIZE           0x10000000    /* 256 MB */
#define SIERRA_SMEM_SIZE          0x00100000    /*   1 MB */
#define SIERRA_SMEM_BASE_PHY      ((DDR_MEM_BASE + SIERRA_MEM_SIZE) - (SIERRA_SMEM_SIZE))

#endif /* SSMEM_ADDRESS_H */
