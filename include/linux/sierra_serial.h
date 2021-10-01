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
extern int uart_create_sysfs_config(struct device *dev);
extern bool uart_is_function_console(struct device *dev);
#ifdef CONFIG_SIERRA_FX30
extern bool uart_is_function_rs485(struct device *dev);
#else
#define uart_is_function_rs485(x) (false)
#endif
#else
#define uart_create_sysfs_config(x) (0)
#define uart_is_function_console(x) (true)
#define uart_is_function_rs485(x) (false)
#endif

#endif /* CONFIG_SIERRA_SERIAL_H */
