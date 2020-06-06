/* kernel/include/linux/sierra_gpio_wake_n.h
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

#ifndef CONFIG_SIERRA_GPIO_WAKE_N
#define CONFIG_SIERRA_GPIO_WAKE_N
#include <linux/notifier.h>

extern int sierra_gpio_wake_notifier_register(struct notifier_block *nb);

extern int sierra_gpio_wake_notifier_unregister(struct notifier_block *nb);
#endif
