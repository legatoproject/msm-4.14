/*
 *
 * Copyright (C) 2017 Sierra Wireless, Inc
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

#ifndef CONFIG_SIERRA_SERIAL_H
#define CONFIG_SIERRA_SERIAL_H
#include <linux/types.h>
#include <linux/device.h>
#include <linux/sierra_bsudefs.h>
#ifdef CONFIG_SIERRA
extern void uart_create_sysfs_config(struct device *dev);
#else
#define uart_create_sysfs_config(x)
#endif

#endif /* CONFIG_SIERRA_SERIAL_H */
