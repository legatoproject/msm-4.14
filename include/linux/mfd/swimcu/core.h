/*
 * core.h  --  Core Driver for Sierra Wireless MCU
 *
 * adapted from:
 * core.h  --  Core Driver for Wolfson WM8350 PMIC
 *
 * Copyright (c) 2016 Sierra Wireless, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_SWIMCU_CORE_H_
#define __LINUX_MFD_SWIMCU_CORE_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/completion.h>

#include <linux/mfd/swimcu/gpio.h>

/* Kinetis Application I2C configuration */
#define SWIMCU_APPL_I2C_ADDR    0x3A
#define SWIMCU_APPL_I2C_FREQ    100
#define SWIMCU_APPL_I2C_ID      1

#define SWIMCU_PM_OFF           0
#define SWIMCU_PM_BOOT_SOURCE   1
#define SWIMCU_PM_POWER_SWITCH  2
#define SWIMCU_PM_PSM_SYNC      3
#define SWIMCU_PM_MAX           SWIMCU_PM_PSM_SYNC

#define SWIMCU_PSM_IDLE         0  /* No PSM is requested */
#define SWIMCU_PSM_REQUEST      1  /* To request PSM */
#define SWIMCU_PSM_ACCEPT       2  /* To accept PSM request */
#define SWIMCU_PSM_ENTER        3  /* To enter PSM state */

#define SWIMCU_ADC_VREF         1800
#define SWIMCU_ADC_INTERVAL_MAX 65535

enum swimcu_adc_index
{
	SWIMCU_ADC_FIRST = 0,
	SWIMCU_ADC_PTA12 = SWIMCU_ADC_FIRST, /* ADC2 */
	SWIMCU_ADC_PTB1 = 1, /* ADC3 */
	SWIMCU_ADC_LAST = SWIMCU_ADC_PTB1,
	SWIMCU_NUM_ADC = SWIMCU_ADC_LAST+1,
	SWIMCU_ADC_INVALID = SWIMCU_NUM_ADC,
};

enum swimcu_adc_compare_mode
{
	SWIMCU_ADC_COMPARE_DISABLED = 0,
	SWIMCU_ADC_COMPARE_ABOVE,
	SWICMU_ADC_COMPARE_BELOW,
	SWIMCU_ADC_COMPARE_WITHIN,
	SWIMCU_ADC_COMPARE_BEYOND,
};

#define SWIMCU_FUNC_FLAG_FWUPD       (1 << 0)
#define SWIMCU_FUNC_FLAG_PM          (1 << 1)
#define SWIMCU_FUNC_FLAG_EVENT       (1 << 2)
#define SWIMCU_FUNC_FLAG_WATCHDOG    (1 << 3)
#define SWIMCU_FUNC_FLAG_PSM         (1 << 4)

#define SWIMCU_FUNC_MANDATORY        (SWIMCU_FUNC_FLAG_FWUPD | \
                                      SWIMCU_FUNC_FLAG_PM |    \
                                      SWIMCU_FUNC_FLAG_EVENT)


#define SWIMCU_FUNC_OPTIONAL         (SWIMCU_FUNC_FLAG_WATCHDOG | \
                                      SWIMCU_FUNC_FLAG_PSM)

#define SWIMCU_DRIVER_INIT_FIRST     0
#define SWIMCU_DRIVER_INIT_EVENT     (1 << 0)
#define SWIMCU_DRIVER_INIT_ADC       (1 << 1)
#define SWIMCU_DRIVER_INIT_PING      (1 << 2)
#define SWIMCU_DRIVER_INIT_FW        (1 << 3)
#define SWIMCU_DRIVER_INIT_PM        (1 << 4)
#define SWIMCU_DRIVER_INIT_GPIO      (1 << 5)
#define SWIMCU_DRIVER_INIT_REBOOT    (1 << 6)
#define SWIMCU_DRIVER_INIT_WATCHDOG  (1 << 7)
#define SWIMCU_DRIVER_INIT_PSM       (1 << 8)

#define SWIMCU_DEBUG

#define SWIMCU_INIT_DEBUG_LOG        0x0001
#define SWIMCU_EVENT_DEBUG_LOG       0x0002
#define SWIMCU_PROT_DEBUG_LOG        0x0004
#define SWIMCU_PM_DEBUG_LOG          0x0008
#define SWIMCU_GPIO_DEBUG_LOG        0x0010
#define SWIMCU_ADC_DEBUG_LOG         0x0020
#define SWIMCU_FW_DEBUG_LOG          0x0040
#define SWIMCU_MISC_DEBUG_LOG        0x0080
#define SWIMCU_ALL_DEBUG_LOG         0x00ff

#define SWIMCU_DEFAULT_DEBUG_LOG SWIMCU_INIT_DEBUG_LOG

#ifdef SWIMCU_DEBUG
#define SWIMCU_INIT_LOG     (swimcu_debug_mask & SWIMCU_INIT_DEBUG_LOG)
#define SWIMCU_EVENT_LOG    (swimcu_debug_mask & SWIMCU_EVENT_DEBUG_LOG)
#define SWIMCU_PROT_LOG     (swimcu_debug_mask & SWIMCU_PROT_DEBUG_LOG)
#define SWIMCU_PM_LOG       (swimcu_debug_mask & SWIMCU_PM_DEBUG_LOG)
#define SWIMCU_GPIO_LOG     (swimcu_debug_mask & SWIMCU_GPIO_DEBUG_LOG)
#define SWIMCU_ADC_LOG      (swimcu_debug_mask & SWIMCU_ADC_DEBUG_LOG)
#define SWIMCU_FW_LOG       (swimcu_debug_mask & SWIMCU_FW_DEBUG_LOG)
#define SWIMCU_MISC_LOG     (swimcu_debug_mask & SWIMCU_MISC_DEBUG_LOG)
#else
#define SWIMCU_INIT_LOG     (false)
#define SWIMCU_EVENT_LOG    (false)
#define SWIMCU_PROT_LOG     (false)
#define SWIMCU_PM_LOG       (false)
#define SWIMCU_GPIO_LOG     (false)
#define SWIMCU_ADC_LOG      (false)
#define SWIMCU_FW_LOG       (false)
#define SWIMCU_MISC_LOG     (false)
#endif
#define swimcu_log(id, ...) do { if (SWIMCU_##id##_LOG) pr_info(__VA_ARGS__); } while (0)

extern int swimcu_debug_mask;

#define SWIMCU_FAULT_TX_TO        0x0001
#define SWIMCU_FAULT_TX_NAK       0x0002
#define SWIMCU_FAULT_RX_TO        0x0004
#define SWIMCU_FAULT_RX_CRC       0x0008
#define SWIMCU_FAULT_RESET        0x0100
#define SWIMCU_FAULT_EVENT_OFLOW  0x0200

#define SWIMCU_FAULT_COUNT_MAX    9999

extern int swimcu_fault_mask;
extern int swimcu_fault_count;

struct swimcu_hwmon {
	struct platform_device *pdev;
	struct device *classdev;
};

struct swimcu {
	struct device *dev;
	struct i2c_client *client;
	int i2c_driver_id;

	int driver_init_mask;

	u8 version_major;
	u8 version_minor;
	u8 target_dev_id;
	u16 opt_func_mask;

	struct mutex mcu_transaction_mutex;

	struct mutex adc_mutex;
	int adc_init_mask;

	int gpio_irq_base;
	struct mutex gpio_irq_lock;

	struct notifier_block nb;
	struct notifier_block reboot_nb;

	struct kobject pm_boot_source_kobj;
	struct kobject pm_firmware_kobj;
	struct kobject pm_boot_source_adc_kobj;
	struct kobject pm_psm_kobj;

	/* Client devices */
	struct swimcu_gpio gpio;
	struct swimcu_hwmon hwmon;
};

/**
 * Data to be supplied by the platform to initialise the SWIMCU.
 *
 * @init: Function called during driver initialisation.  Should be
 *        used by the platform to configure GPIO functions and similar.
 * @irq_high: Set if SWIMCU IRQ is active high.
 * @irq_base: Base IRQ for genirq (not currently used).
 * @gpio_base: Base for gpiolib.
 */
struct swimcu_platform_data {
	int gpio_base;
	int nr_gpio;
	int gpio_irq_base;
	int adc_base;
	int nr_adc;
	u16 func_flags;
};


/*
 * SWIMCU device initialization and exit.
 */
int swimcu_device_init(struct swimcu *swimcu);
void swimcu_device_exit(struct swimcu *swimcu);

/*
 * ADC Readback
 */
int swimcu_read_adc(struct swimcu *swimcu, int channel);
int swimcu_adc_init_and_start(struct swimcu *swimcu, enum swimcu_adc_index adc);
int swimcu_adc_set_trigger_mode(enum swimcu_adc_index adc, int trigger, int interval);
int swimcu_adc_set_compare_mode(enum swimcu_adc_index adc, enum swimcu_adc_compare_mode mode,
				unsigned compare_val1, unsigned compare_val2);
int swimcu_get_adc_from_chan(int channel);

void swimcu_set_fault_mask(int fault);
#endif
