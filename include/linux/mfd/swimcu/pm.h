/*
 * pm.h  --  Power management driver for Sierra Wireless MCU
 *
 * Copyright (c) 2016 Sierra Wireless, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_SWIMCU_PM_H_
#define __LINUX_MFD_SWIMCU_PM_H_

#include <linux/kernel.h>
#include <linux/mutex.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/mciprotocol.h>
/*
 * SWIMCU device initialization and exit.
 */
int swimcu_pm_sysfs_init(struct swimcu *swimcu, int func_flags);

int swimcu_pm_sysfs_opt_update(struct swimcu *swimcu);

void swimcu_pm_sysfs_deinit(struct swimcu *swimcu);

void swimcu_wakeup_trigger(void);

void swimcu_set_wakeup_source(enum mci_protocol_wakeup_source_type_e type, u16 value);

void swimcu_set_reset_source(enum mci_protocol_reset_source_e value);

void swimcu_watchdog_event_handle(struct swimcu *swimcu, u32 delay);

void swimcu_calibrate_event_handle(struct swimcu *swimcup, u32 time);

void wake_n_set_callback(void (*event_cb)(void));

int pm_reboot_call(struct notifier_block *this, unsigned long code, void* cmd);

int pm_panic_call(struct notifier_block *this, unsigned long event, void* ptr);

#endif
