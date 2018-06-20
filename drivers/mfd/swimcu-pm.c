/*
 * swimcu-pm.c  --  Device access for Sierra Wireless MCU power management.
 *
 * Copyright (c) 2016 Sierra Wireless, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/kmod.h>
#include <linux/reboot.h>
#include <linux/timekeeping.h>
#include <linux/alarmtimer.h>
#include <linux/gpio.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/gpio.h>
#include <linux/mfd/swimcu/pm.h>
#include <linux/mfd/swimcu/mciprotocol.h>
#include <linux/mfd/swimcu/mcidefs.h>
#include <mach/swimcu.h>


#define SWIMCU_DISABLE                       0
#define SWIMCU_ENABLE                        1

/* Maximum time value allowed in configuration of MCU functionality
*  40 days and 40 nights (in seconds).
*/
#define SWIMCU_MAX_TIME                      (3456000)

/* MCU calibration support starts from MCUFW_002.005 */
#define SWIMCU_CALIBRATE_SUPPORT_VER_MAJOR   2
#define SWIMCU_CALIBRATE_SUPPORT_VER_MINOR   5

/* Default value used to initialize calibrate data */
#define SWIMCU_CALIBRATE_DATA_DEFAULT        1

#define SWIMCU_CALIBRATE_TIME_MIN            15000    /* milliseconds */
#define SWIMCU_CALIBRATE_TIME_MAX            60000    /* milliseconds */
#define SWIMCU_CALIBRATE_TIME_DEFAULT        30000    /* milliseconds */

#define SWIMCU_CALIBRATE_MDM2MCU             1
#define SWIMCU_CALIBRATE_MCU2MDM             -1

/* The MCU timer timeout value is trimmed off by this percentage
*  to mitigate possible deviation for the MCU LPO 1K clock over
*  temerature (expect max 1.5%).
*/
#define SWIMCU_CALIBRATE_TEMPERATURE_FACTOR  2       /* percent */

/* Values for TOD update status variable */
#define SWIMCU_CALIBRATE_TOD_UPDATE_AVAIL    0
#define SWIMCU_CALIBRATE_TOD_UPDATE_OK       1
#define SWIMCU_CALIBRATE_TOD_UPDATE_FAILED   -1

/* Constants for MCU watchdog feature */
#define SWIMCU_WATCHDOG_TIMEOUT_INVALID      0
#define SWIMCU_WATCHDOG_RESET_DELAY_DEFAULT  1         /* second */

/* Two groups of PM data store on MCU persistent across MDM reset */
#define SWIMCU_PM_DATA_MAX_SIZE  (MCI_PROTOCOL_DATA_GROUP_SIZE * \
                                  MCI_PROTOCOL_MAX_NUMBER_OF_DATA_GROUPS)

/* Group 0: clock/time-related info */
#define SWIMCU_PM_DATA_CALIBRATE_MDM_TIME      0
#define SWIMCU_PM_DATA_CALIBRATE_MCU_TIME      1
#define SWIMCU_PM_DATA_EXPECTED_ULPM_TIME      2
#define SWIMCU_PM_DATA_PRE_ULPM_TOD            3
#define SWIMCU_PM_DATA_4_RESERVED              4

/* Group 1: User wakeup source configuration */
#define SWIMCU_PM_DATA_WUSRC_TIMEOUT           5
#define SWIMCU_PM_DATA_WUSRC_GPIO_IRQS         6
#define SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL      7
#define SWIMCU_PM_DATA_WUSRC_ADC2_CONFIG       8
#define SWIMCU_PM_DATA_WUSRC_ADC3_CONFIG       9

/* adc select and thresholds configuration */
#define SWIMCU_WUSRC_ADC_SELECTED_MASK         0x80000000
#define SWIMCU_WUSRC_ADC_SELECTED_SHIFT        31
#define SWIMCU_WUSRC_ADC_ABOVE_THRES_MASK      0x00FFF000
#define SWIMCU_WUSRC_ADC_ABOVE_THRES_SHIFT     12
#define SWIMCU_WUSRC_ADC_BELOW_THRES_MASK      0x00000FFF
#define SWIMCU_WUSRC_ADC_BELOW_THRES_SHIFT     0

/* default values */
#define SWIMCU_PM_DATA_CALIBRATE_DEFAULT       1

#define SWIMCU_WUSRC_ADC_INTERVAL_DEFAULT      1000   /* milli-seconds */
#define SWIMCU_WUSRC_ADC_BELOW_THRES_DEFAULT   0
#define SWIMCU_WUSRC_ADC_ABOVE_THRES_DEFAULT   1800
#define SWIMCU_WUSRC_ADC_THRES_DEFAULT     (SWIMCU_WUSRC_ADC_BELOW_THRES_DEFAULT + \
		((SWIMCU_WUSRC_ADC_ABOVE_THRES_DEFAULT << SWIMCU_WUSRC_ADC_ABOVE_THRES_SHIFT) & \
		SWIMCU_WUSRC_ADC_ABOVE_THRES_MASK))

/* power management data persistent over power cycle */
static uint32_t swimcu_pm_data[SWIMCU_PM_DATA_MAX_SIZE] =
{
	/* group 0 */
	SWIMCU_PM_DATA_CALIBRATE_DEFAULT,  /* SWIMCU_PM_DATA_CALIBRATE_MDM_TIME */
	SWIMCU_PM_DATA_CALIBRATE_DEFAULT,  /* SWIMCU_PM_DATA_CALIBRATE_MCU_TIME */
	0,                                 /* SWIMCU_PM_DATA_EXPECTED_ULPM_TIME */
	0,                                 /* SWIMCU_PM_DATA_PRE_ULPM_TOD       */
	0,                                 /* SWIMCU_PM_DATA_4_RESERVED         */

	/* group 1 */
	0,                                 /* SWIMCU_PM_DATA_WUSRC_TIMEOUT      */
	0x0,                               /* SWIMCU_PM_DATA_WUSRC_GPIO_IRQS    */
	SWIMCU_WUSRC_ADC_INTERVAL_DEFAULT, /* SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL */
	SWIMCU_WUSRC_ADC_THRES_DEFAULT,    /* SWIMCU_PM_DATA_WUSRC_ADC2_CONFIG  */
	SWIMCU_WUSRC_ADC_THRES_DEFAULT,    /* SWIMCU_PM_DATA_WUSRC_ADC3_CONFIG  */
};

/* Macros for constructing a SYSFS node of integer type
*  node   -- name of a tree node.
*  dft    -- default value of the node.
*  min    -- low limit (inclusive) of valid node value.
*  max    -- high limit (inclusive) of valid node value.
*  str    -- name of the SYSFS node to be shown.
*  notify -- whether to notify user of the value change (true/false).
*/
#define SWIMCU_PM_INT_ATTR_SHOW(node)                                \
	static ssize_t swimcu_pm_##node##_attr_show(struct kobject *kobj,   \
		struct kobj_attribute *attr, char *buf)                     \
	{                                                                   \
		return scnprintf(buf, PAGE_SIZE, "%i\n", swimcu_pm_##node); \
	}

#define SWIMCU_PM_INT_ATTR_STORE(node, min, max, str, notify)            \
	static ssize_t swimcu_pm_##node##_attr_store(struct kobject *kobj,                 \
	struct kobj_attribute *attr, const char *buf, size_t count)                        \
	{                                                                                  \
		int value;                                                                 \
		int ret = kstrtoint(buf, 0, &value);                                       \
		if (!ret && value >= min && value <= max) {                                \
			if (swimcu_pm_##node != value) {                                   \
				swimcu_pm_##node = value;                                  \
				if (notify) sysfs_notify(kobj, NULL, str);                 \
			}                                                                  \
			ret = count;                                                       \
		} else {                                                                   \
			ret = -EINVAL;                                                     \
			pr_err("%s: invalid input %s (%i~%i)\n", __func__, buf, min, max); \
		}                                                                          \
		return ret;                                                                \
	}

#define SWIMCU_PM_INT_ATTR(node, dft, min, max, str, notify)    \
	static int swimcu_pm_##node = dft;                                  \
	SWIMCU_PM_INT_ATTR_SHOW(node)                                       \
	SWIMCU_PM_INT_ATTR_STORE(node, min, max, str, notify)               \
	static const struct kobj_attribute swimcu_##node##_attr = {         \
		.attr = {.name = str, .mode = S_IRUGO | S_IWUSR | S_IWGRP}, \
		.show = &swimcu_pm_##node##_attr_show,                      \
		.store = &swimcu_pm_##node##_attr_store,                    \
	};

#define ADC_ATTR_SHOW(name)                                       	\
	static ssize_t pm_adc_##name##_attr_show(struct kobject *kobj,  \
		struct kobj_attribute *attr, char *buf)                 \
	{                                                               \
		unsigned int value = 0;                                 \
		enum swimcu_adc_index adc;                              \
		enum wusrc_index wi = find_wusrc_index_from_kobj(kobj); \
                                                                        \
		if (WUSRC_INVALID != wi) {                              \
			adc = wusrc_param[wi].id;                       \
			value = swimcu_pm_wusrc_adc_##name##_get(adc);  \
		}                                                       \
		return scnprintf(buf, PAGE_SIZE, "%d\n", value);        \
	}

#define ADC_ATTR_STORE(name)                                            \
	static ssize_t pm_adc_##name##_attr_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
	{                                                               \
		unsigned int value = 0;                                 \
		enum swimcu_adc_index adc;                              \
		enum wusrc_index wi = find_wusrc_index_from_kobj(kobj); \
		int ret;                                                \
                                                                        \
		if (WUSRC_INVALID == wi) {                              \
			return -EINVAL;                                 \
		}                                                       \
		adc = wusrc_param[wi].id;                               \
		ret = kstrtouint(buf, 0, &value);                       \
		if (!ret) {                                             \
			if (value <= SWIMCU_ADC_VREF) {                 \
				swimcu_pm_wusrc_adc_##name##_set(adc, value); \
				ret = count;                            \
			}                                               \
			else {                                          \
				ret = -ERANGE;                          \
			}                                               \
		}                                                       \
		return ret;                                             \
	};

/* generate extra debug logs */
#ifdef SWIMCU_DEBUG
module_param_named(
	debug_mask, swimcu_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);
#endif

/* modem power state in low power mode, default off */
static int swimcu_pm_mdm_pwr = MCI_PROTOCOL_MDM_STATE_OFF;
module_param_named(
	modem_power, swimcu_pm_mdm_pwr, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* mcu reset source, for lack of a better place */
static int swimcu_reset_source = 0;
module_param_named(
	reset_source, swimcu_reset_source, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* mcu fault mask, to record communication errors and irregular MCU behaviour */
module_param_named(
	fault_mask, swimcu_fault_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* mcu fault count, number of fault events */
module_param_named(
	fault_count, swimcu_fault_count, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* we can't free the memory here as it is freed by the i2c device on exit */
static void release_kobj(struct kobject *kobj)
{
	swimcu_log(INIT, "%s: %s\n", __func__, kobj->name);
}

static struct kobj_type ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.release = release_kobj,
};

/* Mapping table for MCU IRQ type enumerated values and corresponding ASCII strings */
static const struct swimcu_irq_type_name_map_s {
	enum mci_pin_irqc_type_e type;
	char *name;
} swimcu_irq_type_name_map[] = {
	{MCI_PIN_IRQ_DISABLED,     "none"},
	{MCI_PIN_IRQ_DISABLED,     "off"},
	{MCI_PIN_IRQ_LOGIC_ZERO,   "low"},
	{MCI_PIN_IRQ_RISING_EDGE,  "rising"},
	{MCI_PIN_IRQ_FALLING_EDGE, "falling"},
	{MCI_PIN_IRQ_EITHER_EDGE,  "both"},
	{MCI_PIN_IRQ_LOGIC_ONE,    "high"},
};

/* Enumerated index into wakeup source configuration table.
*  GPIO must be enumerated first: the wakeup source configuration/
*  recovery operation relies on this ordering.
*/
enum wusrc_index {
	WUSRC_INVALID  = -1,
	WUSRC_MIN      = 0,
	WUSRC_GPIO36   = WUSRC_MIN,
	WUSRC_GPIO38   = 1,
	WUSRC_NUM_GPIO = 2,
	WUSRC_TIMER    = 2,
	WUSRC_ADC2     = 3,
	WUSRC_ADC3     = 4,
	WUSRC_MAX      = WUSRC_ADC3,
};

/* Data structure used to record the state of wakeup source config process
*  Used to recover the original config in case of failure
*/
struct swimcu_wusrc_config_state_s
{
	uint32_t wusrc_mask;                     /* configured wakeup source   */
	uint32_t gpio_pin_mask;                  /* configured GPIO pins       */
	uint32_t adc_pin_mask;                   /* configured ADC pins        */
	uint8_t  recovery_irqs[WUSRC_NUM_GPIO];  /* original GPIO pin IRQ type */
};

/* Table of the parameters (constants) of the available wakeup source */
static const struct wusrc_param {
	enum mci_protocol_wakeup_source_type_e type;  /* type of the wakeup source */
	int                                    id;    /* index into the local configuration table of the wakeup type */
	uint32_t                               mask;  /* GPIO: mask identifier, TIMER: HW timer type, ADC mask identifier */
} wusrc_param[] = {
	[WUSRC_GPIO36] = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS, SWIMCU_GPIO_PTA0, MCI_PROTOCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTA0},
	[WUSRC_GPIO38] = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS, SWIMCU_GPIO_PTB0, MCI_PROTOCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTB0},
	[WUSRC_TIMER]  = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER,    0,                0,                                             },
	[WUSRC_ADC2]   = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC,      SWIMCU_ADC_PTA12, MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTA12},
	[WUSRC_ADC3]   = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC,      SWIMCU_ADC_PTB1,  MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTB1},
};

/* Mapping table of "triggered" status and associated SYSFS KObject of the wakeup sources */
static struct swimcu_pm_wusrc_status {
	struct kobject *kobj;
	int             triggered;
} swimcu_pm_wusrc_status[] = {
	[WUSRC_GPIO36] = {NULL, 0},
	[WUSRC_GPIO38] = {NULL, 0},
	[WUSRC_TIMER]  = {NULL, 0},
	[WUSRC_ADC2]   = {NULL, 0},
	[WUSRC_ADC3]   = {NULL, 0},
};

static char* poweroff_argv[] = {"/sbin/poweroff", NULL};

#define SWIMCU_PM_WAIT_SYNC_TIME 40000

#define PM_STATE_IDLE     0
#define PM_STATE_SYNC     1
#define PM_STATE_SHUTDOWN 2

/* Power management configuration */
static int swimcu_pm_enable = SWIMCU_PM_OFF;
static int swimcu_pm_state = PM_STATE_IDLE;

/* MCU watchdog configuration */
static int swimcu_watchdog_enable  = SWIMCU_DISABLE;
static u32 swimcu_watchdog_timeout = SWIMCU_WATCHDOG_TIMEOUT_INVALID;
static u32 swimcu_watchdog_reset_delay = SWIMCU_WATCHDOG_RESET_DELAY_DEFAULT;
static u32 swimcu_watchdog_renew_count = 0;

/* MCU psm support configuration (psm time is aliased to ulpm time) */
static u32 swimcu_psm_active_time = 0;
static enum mci_protocol_pm_psm_sync_option_e
                swimcu_psm_sync_select = MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE;


/* SYSFS support for MCU 1K LPO clock calibration */
static int swimcu_lpo_calibrate_enable   = SWIMCU_DISABLE;
static u32 swimcu_lpo_calibrate_mcu_time = SWIMCU_CALIBRATE_TIME_DEFAULT;
static struct timespec swimcu_calibrate_start_tv;

/* status of post-ULPM TOD update */
static int swimcu_pm_tod_update_status = SWIMCU_CALIBRATE_TOD_UPDATE_FAILED;

/*****************************
* Wakeup sourc configuration *
******************************/
/************
*
* Name:     swimcu_pm_wusrc_gpio_irq_set
*
* Purpose:  Set IRQ type of a specific MCU GPIO (as a ULPM wakeup source)
*
* Parms:    index - index of the GPIO for which IRQ type to be set
*           irq   - IRQ type to be set
*
* Return:   0 if successful; -1 otherwise
*
* Abort:    none
*
************/
int swimcu_pm_wusrc_gpio_irq_set(
	enum swimcu_gpio_index index,
	enum mci_pin_irqc_type_e irq)
{
	int ret = 0;
	uint8_t *irq_typep = (uint8_t*)
		&(swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_GPIO_IRQS]);

	switch (index)
	{
		case SWIMCU_GPIO_PTA0:

			irq_typep[WUSRC_GPIO36] = 0xFF & irq;
			break;

		case SWIMCU_GPIO_PTB0:

			irq_typep[WUSRC_GPIO38] = 0xFF & irq;
			break;

		default:

			ret= -1;
	}

	return ret;
}

/************
*
* Name:     swimcu_pm_wusrc_gpio_irq_get
*
* Purpose:  Get configured IRQ type of a specific MCU GPIO (as wakeup source)
*
* Parms:    index - index of the GPIO for which IRQ type to be returned
*
* Return:   configured IRQ type if successful; MCI_PIN_IRQ_DISABLED otherwise.
*
* Abort:    none
*
************/
static enum mci_pin_irqc_type_e swimcu_pm_wusrc_gpio_irq_get(
	enum swimcu_gpio_index index)
{
	enum mci_pin_irqc_type_e irq;
	uint8_t *irq_typep = (uint8_t*)
		&(swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_GPIO_IRQS]);

	switch (index)
	{
		case SWIMCU_GPIO_PTA0:

			irq = (enum mci_pin_irqc_type_e) irq_typep[WUSRC_GPIO36];
			break;

		case SWIMCU_GPIO_PTB0:

			irq = (enum mci_pin_irqc_type_e) irq_typep[WUSRC_GPIO38];
			break;

		default:

			irq = MCI_PIN_IRQ_DISABLED;
	}

	return irq;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_config_datap_get (helper function)
*
* Purpose:  Get pointer to data encoding the ADC wakeup source configuration
*
* Parms:    index - index of the ADC for which the data pointer to be returned
*
* Return:   Pointer to the configured ADC config if successful; NULL otherwise.
*
* Abort:    none
*
************/
static uint32_t* swimcu_pm_wusrc_adc_config_datap_get(enum swimcu_adc_index index)
{
	if (index == SWIMCU_ADC_PTA12)
	{
		return &(swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC2_CONFIG]);
	}

	if (index == SWIMCU_ADC_PTB1)
	{
		return &(swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC3_CONFIG]);
	}

	return NULL;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_above_set
*
* Purpose:  Set the ADC threshold above which a wakeup triggers
*
* Parms:    index - index of the ADC for which the property to set
*           above - value of 'above' threshold to set
*
* Return:   0 if successful, -1 otherwise
*
* Abort:    none
*
************/
static int swimcu_pm_wusrc_adc_above_set(enum swimcu_adc_index index, uint32_t above)
{
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);

	if (!datap)
	{
		return -1;
	}

	above <<= SWIMCU_WUSRC_ADC_ABOVE_THRES_SHIFT;
	above &=  SWIMCU_WUSRC_ADC_ABOVE_THRES_MASK;

	*datap &= ~SWIMCU_WUSRC_ADC_ABOVE_THRES_MASK;
	*datap |= above;

	return 0;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_above_get
*
* Purpose:  get the ADC threshold above which a wakeup triggers
*
* Parms:    index - index of the ADC for which the property to get
*
* Return:   The value of the 'above' threshold
*
* Abort:    none
*
************/
static uint32_t swimcu_pm_wusrc_adc_above_get(enum swimcu_adc_index index)
{
	uint32_t above = SWIMCU_WUSRC_ADC_ABOVE_THRES_DEFAULT;
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);

	if (datap)
	{
		above = (*datap) & SWIMCU_WUSRC_ADC_ABOVE_THRES_MASK;
		above >>= SWIMCU_WUSRC_ADC_ABOVE_THRES_SHIFT;
	}

	return above;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_below_set
*
* Purpose:  Set the threshold below which a wakeup triggers
*
* Parms:    index - index of the ADC for which the property to set
*           below - value of 'below' threshold
*
* Return:   0 if successful, -1 otherwise
*
* Abort:    none
*
************/
static int swimcu_pm_wusrc_adc_below_set(enum swimcu_adc_index index, uint32_t below)
{
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);
	if (!datap)
	{
		return -1;
	}

	*datap &= ~SWIMCU_WUSRC_ADC_BELOW_THRES_MASK;
	*datap |= below & SWIMCU_WUSRC_ADC_BELOW_THRES_MASK;
	return 0;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_below_get
*
* Purpose:  Get the threshold below which a wakeup triggers
*
* Parms:    index - index of the ADC for which the property to get
*
* Return:   The value of the 'below' threshold
*
* Abort:    none
*
************/
static uint32_t swimcu_pm_wusrc_adc_below_get(enum swimcu_adc_index index)
{
	uint32_t below = SWIMCU_WUSRC_ADC_BELOW_THRES_DEFAULT;
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);

	if (datap)
	{
		below = (*datap) & SWIMCU_WUSRC_ADC_BELOW_THRES_MASK;
	}

	return below;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_select_get
*
* Purpose:  To set whether a specific ADC is selected as a wakeup source
*
* Parms:    index  - index of the ADC which is selected or deselected
*           select - whether the ADC is selected (1 select, 0 deselect)
*
* Return:   0 if successful; -1 otherwise.
*
* Abort:    none
*
************/
static int swimcu_pm_wusrc_adc_select_set(enum swimcu_adc_index index, int selected)
{
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);
	if (!datap)
	{
		return -1;
	}

	if (selected)
	{
		*datap |= SWIMCU_WUSRC_ADC_SELECTED_MASK;
	}
	else
	{
		*datap &= ~SWIMCU_WUSRC_ADC_SELECTED_MASK;
	}
	return 0;
}

/************
*
* Name:     swimcu_pm_wusrc_adc_select_get
*
* Purpose:  Query whether specific ADC is selected as a wakeup source
*
* Parms:    index - index of the ADC
*
* Return:   1 if selected; 0 otherwise
*
* Abort:    none
*
************/
static int swimcu_pm_wusrc_adc_select_get(enum swimcu_adc_index index)
{
	int select = 0;
	uint32_t* datap = swimcu_pm_wusrc_adc_config_datap_get(index);

	if (datap)
	{
		select = ((*datap) & SWIMCU_WUSRC_ADC_SELECTED_MASK) ? 1 : 0;
	}

	return select;
}

/************
*
* Name:     swimcu_calibrate_data_get
*
* Purpose:  To get calibration data for MDM/MCU time conversion
*
* Parms:    swimcup       - pointer to the swimcu data object
*           cal_dir       - calibrate direction (MDM2MCU or MCU2MDM)
*           cal_time      - time interval to be calibrated
*           cal_mdm_timep - pointer to storage for reuturned calibrate MDM time
*           cal_mcu_timep - pointer to storage for reuturned calibrate MCU time
*
* Return:   none (always successful)
*
* Note:     Expensive 64-bit division on 32-bit machine is prohibited in kernel.
*           The calibrate result is truncated by a factor to avoid possible
*           overflow in 32-bit multiplication during time conversion. To achieve
*           max precision, this truncation factor is calculated based on the
*           input time to be converted and the direction of the conversion.
*
* Abort:    none
*
************/
static void swimcu_calibrate_data_get(
	struct swimcu *swimcup,
	int cal_dir,
	u32 cal_time,
	u32 *cal_mdm_timep,
	u32 *cal_mcu_timep)
{
	u32 factor;

	mutex_lock(&swimcup->calibrate_mutex);

	/* Calculate the minimal truncation factor; add 1 to avoid overflow in conversion */
	if (cal_dir == SWIMCU_CALIBRATE_MDM2MCU)
	{
		factor = swimcup->calibrate_mcu_time;
	}
	else if (cal_dir == SWIMCU_CALIBRATE_MCU2MDM)
	{
		factor = swimcup->calibrate_mdm_time;
	}
	else
	{
		factor = max(swimcup->calibrate_mdm_time, swimcup->calibrate_mcu_time);
	}
	factor = cal_time / (U32_MAX / factor) + 1;

	/* scale down the calibration data proportionally by the calculated factor */
	*cal_mdm_timep = swimcup->calibrate_mdm_time / factor;
	*cal_mcu_timep = swimcup->calibrate_mcu_time / factor;

	mutex_unlock(&swimcup->calibrate_mutex);
}

/************
*
* Name:     swimcu_mdm_sec_to_mcu_time_ms
*
* Purpose:  Convert mdm time (in seconds) to equivalent MCU time in millisecond
*
* Parms:    swimcup  - pointer to the swimcu data object
*           mdm_time - MDM time interval to be converted
*
* Return:   equivalent mcu_time in milliseconds
*
* Abort:    none
*
************/
static u32 swimcu_mdm_sec_to_mcu_time_ms(
	struct swimcu *swimcup,
	u32 mdm_time)
{
	u32 mcu_time, remainder, cal_mcu_time, cal_mdm_time;

	swimcu_calibrate_data_get(swimcup,
		SWIMCU_CALIBRATE_MDM2MCU, mdm_time, &cal_mdm_time, &cal_mcu_time);

	pr_info("%s: mdm time=%u seconds to be calibrated %u/%u\n",
		__func__, mdm_time, cal_mcu_time, cal_mdm_time);

	mdm_time *= cal_mcu_time;

	mcu_time = mdm_time / cal_mdm_time;            /* seconds */
	mcu_time *= MSEC_PER_SEC;                      /* milliseconds */

	/* keep millisecond precision on MCU side */
	remainder = mdm_time % cal_mdm_time;
	remainder *= MSEC_PER_SEC;                     /* milliseconds */
	remainder /= cal_mdm_time;                     /* milliseconds*/

	pr_info("%s: mcu time %u ms + remainder time %u ms = %u ms\n",
		__func__, mcu_time, remainder, mcu_time + remainder);
	return (mcu_time + remainder);
}

/************
*
* Name:     swimcu_lpo_calibrate_calc
*
* Purpose:  Calculate the MCU LPO calibration result
*
* Parms:    swimcup  - pointer to the swimcu data object
*           mcu_time - actual mcu time elapsed
*
* Return:   true if successful;
*           false if MDM time changed during calibration
*
* Abort:    none
*
************/
static bool swimcu_lpo_calibrate_calc(struct swimcu *swimcup, u32 mcu_time)
{
	u32 mdm_time, delta;
	struct timespec stop_tv;

	if (mcu_time < SWIMCU_CALIBRATE_TIME_MIN)
	{
		pr_err("%s: calibration time too short %d (%d)\n",
			__func__, mcu_time, SWIMCU_CALIBRATE_TIME_MIN);
		return false;
	}

	getrawmonotonic(&stop_tv);
	swimcu_log(PM, "%s: MCU calibrate start: %ld.%09ld stop: %ld.%09ld\n", __func__,
		swimcu_calibrate_start_tv.tv_sec, swimcu_calibrate_start_tv.tv_nsec,
		stop_tv.tv_sec, stop_tv.tv_nsec);

	mdm_time = stop_tv.tv_sec - swimcu_calibrate_start_tv.tv_sec;
	mdm_time *= MSEC_PER_SEC;
	mdm_time += (stop_tv.tv_nsec - swimcu_calibrate_start_tv.tv_nsec) / NSEC_PER_MSEC;

	/* Throw away bogus data (exceeds max MCU LPO frequency deviation) */
	if (mdm_time >  mcu_time)
	{
		delta = mdm_time - mcu_time;
	}
	else
	{
		delta = mcu_time - mdm_time;
	}

	if (delta > (mcu_time / 10))
	{
		pr_err("%s: bogus data MCU time=%d vs MDM time=%d \n", __func__, mcu_time, mdm_time);
		return false;
	}

	mutex_lock(&swimcup->calibrate_mutex);
	swimcup->calibrate_mdm_time = mdm_time;
	swimcup->calibrate_mcu_time = mcu_time;
	swimcu_lpo_calibrate_mcu_time = mcu_time;
	mutex_unlock(&swimcup->calibrate_mutex);

	swimcu_log(INIT, "%s: MCU time=%d vs MDM time=%d \n", __func__, mcu_time, mdm_time);
	return true;
}

/************
*
* Name:     swimcu_lpo_calibrate_do_enable
*
* Purpose:  Enable/Disable LPO calibration
*
* Parms:    swimcup - pointer to the swimcu data object
*           enable  - Boolean flag to indicate enable/disable LPO calibration
*
* Return:   0 if success;
*           -EPERM if the specific enable/disable operation has already performed.
*           -EINVAL if calibration time is not valid;
*           -EIO if fails to communicate with MCU
*
* Abort:    none
*
************/
static int swimcu_lpo_calibrate_do_enable(struct swimcu *swimcup, bool enable)
{
	enum mci_protocol_status_code_e s_code;
	enum mci_protocol_hw_timer_state_e timer_state;
	u32 remainder;

	if (enable == swimcu_lpo_calibrate_enable)
	{
		if (swimcu_lpo_calibrate_enable == SWIMCU_DISABLE)
		{
			pr_err("%s: MCU LDO calibrate already stopped\n", __func__);
		}
		else
		{
			pr_err("%s: MCU LDO calibrate already started\n", __func__);
		}
		return -EPERM;
	}

	if (enable == SWIMCU_DISABLE)
	{
		s_code = mci_appl_timer_stop(swimcup, &timer_state, &remainder);
		if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			pr_err("%s: cannot send command to stop MCU timer status=%d\n", __func__, s_code);

			/* Reset the state variable to allow next calibration after recovery */
			swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
			return -EIO;
		}

		if (timer_state == MCI_PROTOCOL_HW_TIMER_STATE_IDLE)
		{
			swimcu_log(PM, "%s: calibration timer has already expired\n", __func__);
			/* Too late to stop the calibration: do nothing.
			*  A calibrate event is expected to be delivered and processed.
			*/
		}
		else
		{
			if (timer_state == MCI_PROTOCOL_HW_TIMER_STATE_CALIBRATE_RUNNING)
			{
				(void)swimcu_lpo_calibrate_calc(swimcup, (swimcu_lpo_calibrate_mcu_time - remainder));
			}
			else
			{
				pr_err("%s: stopped other timer in state %d unexpectedly\n", __func__, timer_state);
			}

			/* Reset the state variable to allow next calibration after recovery */
			swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
		}
	}
	else
	{
		/* first set the enble flag to prevent calibrate time change */
		swimcu_lpo_calibrate_enable = SWIMCU_ENABLE;

		if (swimcu_lpo_calibrate_mcu_time < SWIMCU_CALIBRATE_TIME_MIN)
		{
			pr_err("%s: calibration time is too short %d (%d)\n",
				__func__, swimcu_lpo_calibrate_mcu_time, SWIMCU_CALIBRATE_TIME_MIN);

			swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
			return -EINVAL;
		}

		s_code = mci_appl_timer_calibrate_start(swimcup, swimcu_lpo_calibrate_mcu_time);
		if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			pr_err("%s: failed MCU command status %d\n", __func__, s_code);

			swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
			return -EIO;
		}
		getrawmonotonic(&swimcu_calibrate_start_tv);
	}

	return 0;
}

/************
*
* Name:     find_wusrc_index_from_kobj
*
* Purpose:  Match the wusrc index from the provided kobj
*
* Parms:    kobj - kobject ptr of the sysfs node
*
* Return:   a valid index if found
*           -1 if not found
*
* Abort:    none
*
************/
static enum wusrc_index find_wusrc_index_from_kobj(struct kobject *kobj)
{
	enum wusrc_index wi;

	for (wi = 0; wi <= WUSRC_MAX; wi++) {
		if (swimcu_pm_wusrc_status[wi].kobj == kobj) {
			break;
		}
	}

	if (wi > WUSRC_MAX) {
		pr_err("%s: fail %s\n", __func__, kobj->name);
		wi = WUSRC_INVALID;
	}

	return wi;
}

/************
*
* Name:     find_wusrc_index_from_id
*
* Purpose:  Match the wusrc index from the provided type/id
*
* Parms:    type - gpio or timer
*           id - if gpio, the gpio number (0 - 7)
*
* Return:   a valid index if found
*           -1 if not found
*
* Abort:    none
*
************/
static enum wusrc_index find_wusrc_index_from_id(enum mci_protocol_wakeup_source_type_e type, int id)
{
	enum wusrc_index wi;

	for (wi = 0; wi <= WUSRC_MAX; wi++) {
		if ((type == wusrc_param[wi].type) && (id == wusrc_param[wi].id)) {
			break;
		}
	}

	if (wi > WUSRC_MAX) {
		pr_err("%s: fail type %d id %d\n", __func__, type, id);
		wi = WUSRC_INVALID;
	}

	return wi;
}

/************
*
* Name:     pm_ulpm_config
*
* Purpose:  Configure MCU for ULPM and select wakeup sources
*
* Parms:    swimcu - device driver data
*	    wu_source  - bitmask of wakeup sources
*
* Return:   0 if successful
*	    -ERRNO otherwise

* Abort:    none
*
************/
static int pm_ulpm_config(struct swimcu* swimcu, u16 wu_source)
{

	struct mci_pm_profile_config_s pm_config;
	int ret = 0;
	int rc;

	pm_config.active_power_mode = MCI_PROTOCOL_POWER_MODE_RUN;
	pm_config.active_idle_time = 100;
	pm_config.standby_power_mode = MCI_PROTOCOL_POWER_MODE_VLPS;
	pm_config.standby_mdm_state = swimcu_pm_mdm_pwr;
	pm_config.standby_wakeup_sources = wu_source;
	pm_config.mdm_on_conds_bitset_any = 0;
	pm_config.mdm_on_conds_bitset_all = 0;

	swimcu_log(PM, "%s: pm prof cfg src=%x\n", __func__, wu_source);
	if(MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
	  (rc = swimcu_pm_profile_config(swimcu, &pm_config,
		MCI_PROTOCOL_PM_OPTYPE_SET))) {
		pr_err("%s: pm enable fail %d\n", __func__, rc);
		ret = -EIO;
	}
	return ret;
}

/************
*
* Name:     pm_reboot_call
*
* Purpose:  Handler for reboot notifier
*
* Parms:    this - notifier block associated with this handler
*           code - reboot code. We are only interested in SYS_POWER_OFF/SYS_RESTART
*           cmd  - not used.
*
* Return:   0 always
*
* Abort:    none
*
************/
int pm_reboot_call(
	struct notifier_block *this, unsigned long code, void *cmd)
{
	int rc;
	uint32_t time_ms;
	enum mci_protocol_hw_timer_state_e timer_state;
	struct swimcu* swimcu = container_of(this, struct swimcu, reboot_nb);

	if (SYS_POWER_OFF == code)
	{
		switch(swimcu_pm_state) {
		case PM_STATE_SYNC:
			/*
			 * ULPM has been configured,
			 * notify MCU that it is safe to remove power
			 */
			if(MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
			  (rc = swimcu_pm_pwr_off(swimcu))) {
				pr_err("%s: pm poweroff fail %d\n", __func__, rc);
			}
			break;

		case PM_STATE_IDLE:
			/*
			 * Userspace is already shutdown at this point,
			 * so we can set a sync wait time of 0 to shutdown
			 * immediately after ULPM is configured
			 */
			rc = swimcu_pm_wait_time_config(swimcu, 0, 0);
			if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != rc) {
				pr_err("%s: pm wait_time_config failed %d\n", __func__, rc);
			}

			rc = pm_ulpm_config(swimcu, 0);
			if(MCI_PROTOCOL_STATUS_CODE_SUCCESS != rc) {
				pr_err("%s: pm ulpm_config fail %d\n", __func__, rc);
			}
			break;

		case PM_STATE_SHUTDOWN:
		default:
			break;
		}
	}
	else if (SYS_RESTART == code)
	{
		if (swimcu_watchdog_enable == SWIMCU_ENABLE)
		{
			rc = mci_appl_timer_stop(swimcu, &timer_state, &time_ms);

			if (rc == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
				swimcu_watchdog_enable = SWIMCU_DISABLE;
				swimcu_log(PM, "%s: Watchdog timer stopped in state %d with remaining time %d\n",
				__func__, timer_state, time_ms);
			}
			else
			{
				pr_err("%s: cannot stop MCU Watchdog: %d\n", __func__, rc);
			}
		}
	}

	return NOTIFY_DONE;
}


/************
*
* Name:     pm_panic_call
*
* Purpose:  Handler for panic notifier
*
* Parms:    this  - notifier block associated with this handler
*           event - not used
*           ptr   - not used
*
* Return:   0 always
*
* Abort:    none
*
************/
int pm_panic_call(
	struct notifier_block *this, unsigned long event, void *ptr)
{
	int rc;
	uint32_t time_ms;
	enum mci_protocol_hw_timer_state_e timer_state;
	struct swimcu* swimcu = container_of(this, struct swimcu, panic_nb);

	if (swimcu_watchdog_enable == SWIMCU_ENABLE)
	{
		rc = mci_appl_timer_stop(swimcu, &timer_state, &time_ms);

		if (rc == MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			swimcu_watchdog_enable = SWIMCU_DISABLE;
			swimcu_log(PM, "%s: Watchdog timer stopped in state %d with remaining time %d\n",
				__func__, timer_state, time_ms);
		}
		else
		{
			pr_err("%s: cannot stop MCU Watchdog: %d\n", __func__, rc);
		}
	}

	return NOTIFY_DONE;
}

/************
*
* Name:     swimcu_pm_psm_sync_option_default
*
* Purpose:  To get default PSM synchronization option supported by MCU
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   Default PSMsynchronization option
*
* Abort:    none
*
************/
static enum mci_protocol_pm_psm_sync_option_e
swimcu_pm_psm_sync_option_default(
	struct swimcu *swimcup)
{
	if (swimcup->opt_func_mask & (MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_2 | MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_3))
	{
		return MCI_PROTOCOL_PM_PSM_SYNC_OPTION_B;
	}
	else if (swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_1)
	{
		return MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A;
	}
	else
	{
		return MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE;
	}
}

/************
*
* Name:     swimcu_pm_psm_time_get
*
* Purpose:  To get PSM time from PMIC RTC
*
* Parms:    none
*
* Return:   Remaining PSM time interval (in seconds)
*
* Abort:    none
*
************/
static u32 swimcu_pm_psm_time_get(void)
{
	unsigned long rtc_secs, alarm_secs, interval;
	struct rtc_wkalrm rtc_alarm;
	struct rtc_time   rtc_time;
	struct rtc_device *rtc = alarmtimer_get_rtcdev();
	int ret;

	if (!rtc)
	{
		pr_err("%s: failed to get RTC device\n", __func__);
		return 0;
	}

	/* retrieve configured RTC time and RTC Alarm settings; convert to seconds */
	ret = rtc_read_alarm(rtc, &rtc_alarm);
	if (ret)
	{
		pr_err("%s: failed to read alarm ret=%d\n", __func__, ret);
		return 0;
	}
	(void)rtc_tm_to_time(&rtc_alarm.time, &alarm_secs);

	ret = rtc_read_time(rtc, &rtc_time);
	if (ret)
	{
		pr_err("%s: failed to read time ret=%d\n", __func__, ret);
		return 0;
	}
	(void)rtc_tm_to_time(&rtc_time, &rtc_secs);

	if (alarm_secs > rtc_secs)
	{
		interval = alarm_secs - rtc_secs;
		pr_info("%s: alarm %lu rtc %lu interval %lu", __func__, alarm_secs, rtc_secs, interval);
	}
	else
	{
		interval = 0;
		pr_err("%s: invalid configuration alarm %lu rtc %lu", __func__, alarm_secs, rtc_secs);
	}

	return (u32)interval;
}


/************
*
* Name:     swimcu_pm_data_store
*
* Purpose:  To store data on MCU persistent across PSM/ULPM cycle
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   none
*
* Abort:    none
*
************/
void swimcu_pm_data_store(struct swimcu *swimcup)
{
	enum mci_protocol_status_code_e s_code;
	struct timeval tv;
	uint8_t i;

	/* Save MCU clock calibration data */
	mutex_lock(&swimcup->calibrate_mutex);
	swimcu_pm_data[SWIMCU_PM_DATA_CALIBRATE_MDM_TIME] =
		swimcup->calibrate_mdm_time;
	swimcu_pm_data[SWIMCU_PM_DATA_CALIBRATE_MCU_TIME] =
		swimcup->calibrate_mcu_time;
	mutex_unlock(&swimcup->calibrate_mutex);

	/* Save pre-ULPM time of the day */
	tv.tv_sec = 0;
	do_gettimeofday(&tv);
	tv.tv_sec += (tv.tv_usec + USEC_PER_SEC/2)/USEC_PER_SEC;
	swimcu_pm_data[SWIMCU_PM_DATA_PRE_ULPM_TOD] = tv.tv_sec;

	for (i = 0; i < MCI_PROTOCOL_MAX_NUMBER_OF_DATA_GROUPS; i++)
	{
		swimcu_log(INIT, "%s: sending persistent data group %d to MCU\n", __func__, i);
		s_code = swimcu_appl_data_store(swimcup, i,
			&(swimcu_pm_data[i * MCI_PROTOCOL_DATA_GROUP_SIZE]),
			MCI_PROTOCOL_DATA_GROUP_SIZE);
		if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			/* it may fail if MCUFW does not support the feature but continue any way */
			pr_err("%s: failed to store data to MCU %d\n", __func__, s_code);
		}
	}
}

/************
*
* Name:     swimcu_pm_lpo_calibrate_start
*
* Purpose:  To start MCU LPO clock calibration
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   none
*
* Abort:    none
*
************/
static void swimcu_pm_lpo_calibrate_start(struct swimcu *swimcup)
{
	int ret;

	/* Start the MCU LPO calibration with the previous calibration duration or
	*  default duration, whichever is longer.
	*/
	swimcup->calibrate_mcu_time =
		max(swimcup->calibrate_mcu_time, (u32)SWIMCU_CALIBRATE_TIME_DEFAULT);
	ret = swimcu_lpo_calibrate_do_enable(swimcup, SWIMCU_ENABLE);
	if (ret)
	{
		pr_err("%s: Failed to start MCU timer calibration %d\n", __func__, ret);
	}
	else
	{
		swimcu_log(INIT, "%s: MCU LPO calibration started %u\n",
			__func__, swimcu_lpo_calibrate_mcu_time);
	}
}

/************
*
* Name:     swimcu_pm_tod_update
*
* Purpose:  To update time of the day in Linux system
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   none
*
* Abort:    none
*
************/
static void swimcu_pm_tod_update(struct swimcu *swimcup)
{
	enum mci_protocol_status_code_e s_code;
	enum mci_protocol_pm_psm_sync_option_e sync_opt;
	u32 ulpm_time_sec, ulpm_time_ms, remainder, cal_mdm_time, cal_mcu_time;
	struct timespec tv;
	int ret = SWIMCU_CALIBRATE_TOD_UPDATE_FAILED;
	uint8_t count;

	swimcu_pm_tod_update_status = SWIMCU_CALIBRATE_TOD_UPDATE_FAILED;

	s_code = swimcu_appl_psm_duration_get(swimcup, &ulpm_time_ms, &sync_opt);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code)
	{
		pr_err("%s: failed to get ULPM duration: %d\n", __func__, s_code);
		goto TOD_UPDATE_EXIT;
	}

	/* restore sync select configuration */
	swimcu_psm_sync_select = sync_opt;
	if ((sync_opt == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A) ||
		(sync_opt == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_B))
	{
		swimcu_log(INIT, "%s: no TOD recovery is required for sync option %d\n", __func__, sync_opt);
		goto TOD_UPDATE_EXIT;
	}

	swimcu_log(INIT, "%s: MCUFW elapsed PSM tme: %dms\n", __func__, ulpm_time_ms);

	if (ulpm_time_ms == 0)
	{
		pr_err("%s: nil PSM elapsed time\n", __func__);
		goto TOD_UPDATE_EXIT;
	}

	/* Split elapsed ULPM time into seconds and milliseconds to avoid overflow */
	ulpm_time_sec = ulpm_time_ms / MSEC_PER_SEC;
	ulpm_time_ms %= MSEC_PER_SEC;

	swimcu_calibrate_data_get(swimcup,
		SWIMCU_CALIBRATE_MCU2MDM, ulpm_time_sec, &cal_mdm_time, &cal_mcu_time);

	/* Convert "seconds" portion of elapsed ULPM duration */
	ulpm_time_sec *= cal_mdm_time;
	remainder     = ulpm_time_sec % cal_mcu_time;
	ulpm_time_sec = ulpm_time_sec / cal_mcu_time;

	/* Convert "milliseconds" portion, plus remainder from the "seconds" portion */
	ulpm_time_ms = ulpm_time_ms * cal_mdm_time + remainder * MSEC_PER_SEC;
	ulpm_time_ms = ulpm_time_ms / cal_mcu_time;

	swimcu_log(INIT, "%s: MDM time %u sec %u ms (%u/%u)\n",
		__func__, ulpm_time_sec, ulpm_time_ms, cal_mdm_time, cal_mcu_time);

	if (ulpm_time_ms >= MSEC_PER_SEC)
	{
		ulpm_time_sec += 1;
		ulpm_time_ms -= MSEC_PER_SEC;
	}

	/* Final post-ULPM time of the day */
	tv.tv_sec  = ulpm_time_sec + swimcu_pm_data[SWIMCU_PM_DATA_PRE_ULPM_TOD];
	tv.tv_nsec = ulpm_time_ms * NSEC_PER_MSEC;

	ret = do_settimeofday(&tv);
	if (ret == 0)
	{
		swimcu_pm_tod_update_status = SWIMCU_CALIBRATE_TOD_UPDATE_OK;

		swimcu_log(INIT, "%s  pre-ULPM tod: %u sec\n",
			__func__, swimcu_pm_data[SWIMCU_PM_DATA_PRE_ULPM_TOD]);
		swimcu_log(INIT, "%s set post-ULPM tod: %ld sec\n", __func__, tv.tv_sec);
	}
	else
	{
		pr_err("%s failed to set post-ULPM RTC ret=%d\n", __func__, ret);
	}

TOD_UPDATE_EXIT:

	/* Reset the pre ULPM TOD value stored on MCU (in Group 0 clock/time info)
	*  to prevent any possible erroneous TOD update due to non-ULPM resets.
	*/
	swimcu_pm_data[SWIMCU_PM_DATA_PRE_ULPM_TOD] = 0;
	count = MCI_PROTOCOL_DATA_GROUP_SIZE;
	s_code = swimcu_appl_data_store(swimcup, 0, &(swimcu_pm_data[0]), count);
	if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
	{
		pr_err("%s: failed to clear TOD stored on MCU\n", __func__);
	}

	/* perform MCU LPO calibration */
	swimcu_pm_lpo_calibrate_start(swimcup);
}

/************
*
* Name:     swimcu_pm_data_restore
*
* Purpose:  To restore previously saved data from MCU
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   none
*
* Abort:    none
*
************/
void swimcu_pm_data_restore(struct swimcu *swimcup)
{
	enum mci_protocol_status_code_e s_code;
	uint8_t i, j, count;

	for (i = 0; i < MCI_PROTOCOL_MAX_NUMBER_OF_DATA_GROUPS; i++)
	{
		count = MCI_PROTOCOL_DATA_GROUP_SIZE;
		s_code = swimcu_appl_data_retrieve(swimcup, i,
			&(swimcu_pm_data[i * MCI_PROTOCOL_DATA_GROUP_SIZE]), &count);

		if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			pr_err("%s: failed to retrive data stored on MCU\n", __func__);

			/* perhaps MCUFW does not support the feature */
			continue;
		}

		swimcu_log(INIT, "%s: retrieved persistent data group %d from MCU\n", __func__, i);
		for (j = 0; j < MCI_PROTOCOL_DATA_GROUP_SIZE; j++)
		{
			swimcu_log(INIT, "swimcu_pm_data[%d]:  0x%08x\n", j, swimcu_pm_data[i*MCI_PROTOCOL_DATA_GROUP_SIZE+j]);
		}
	}

	if (swimcu_pm_data[SWIMCU_PM_DATA_CALIBRATE_MCU_TIME] > 0)
	{
		/* Restore the calibration data */
		mutex_lock(&swimcup->calibrate_mutex);
		swimcup->calibrate_mcu_time =
				swimcu_pm_data[SWIMCU_PM_DATA_CALIBRATE_MCU_TIME];
		swimcup->calibrate_mdm_time =
			swimcu_pm_data[SWIMCU_PM_DATA_CALIBRATE_MDM_TIME];
		mutex_unlock(&swimcup->calibrate_mutex);
	}

	if (swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL] == 0)
	{
		/* initialization on first-time powerup */
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL] = SWIMCU_WUSRC_ADC_INTERVAL_DEFAULT;
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC2_CONFIG] = SWIMCU_WUSRC_ADC_THRES_DEFAULT;
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC3_CONFIG] = SWIMCU_WUSRC_ADC_THRES_DEFAULT;
	}
}

/************
*
* Name:     swimcu_pm_psm_timer_config
*
* Purpose:  To configure PSM timer wakeup source (if needed)
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   0 if successful; negative errno otherwise.
*
* Abort:    none
*
************/
static int swimcu_pm_psm_timer_config(struct swimcu *swimcup)
{
	uint32_t timeout;
	int ret;

	pr_info("%s: user-selected psm sync option %d\n", __func__, swimcu_psm_sync_select);

	/* select default sync option if none is specified by user */
	if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE)
	{
		swimcu_psm_sync_select = swimcu_pm_psm_sync_option_default(swimcup);
		if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE)
		{
			pr_err("%s: no PSM synchronization support\n", __func__);
			return -EPERM;
		}
	}

	if (swimcu_psm_sync_select != MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A)
	{
		/* attempt to read remaining PSM time if sync option A is not specified */
		timeout = swimcu_pm_psm_time_get();
		pr_info("%s: configured psm time %d\n", __func__, timeout);
		if (timeout > 0)
		{
			/* mitigate the risk of LPO clock variation over temperature */
			timeout *= (100 - SWIMCU_CALIBRATE_TEMPERATURE_FACTOR);
			timeout /= 100;
			pr_info("%s: at floor of tempreture variation %d\n", __func__, timeout);

			timeout = swimcu_mdm_sec_to_mcu_time_ms(swimcup, timeout);
			pr_info("%s: device calibration %d\n", __func__, timeout);
		}
		else
		{
			/* fall back to sync option A with invalid zero PSM time*/
			pr_info("%s: cannot get PSM time--fall back to option A\n", __func__);
			swimcu_psm_sync_select = MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A;
		}
	}
	else
	{
		timeout = 0;
	}

	swimcu_log(INIT, "%s: sending psm_sync_config sync option %d max_wait %u psm time %u\n",
		__func__, swimcu_psm_sync_select, SWIMCU_PM_WAIT_SYNC_TIME, timeout);

	ret = swimcu_psm_sync_config(swimcup,
		swimcu_psm_sync_select, SWIMCU_PM_WAIT_SYNC_TIME, timeout);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != ret)
	{
		pr_err("%s: cannot config MCU for PSM synchronization %d\n", __func__, ret);
		return -EIO;
	}

	swimcu_pm_data[SWIMCU_PM_DATA_EXPECTED_ULPM_TIME] = timeout;
	return 0;
}

/************
*
* Name:     swimcu_pm_wusrc_config
*
* Purpose:  To configure ULPM wakeup source
*
* Parms:    swimcup - pointer to device driver data
*           pm      - requested power management mode
*           statep  - pointer to the data storage for the configuration state
*
* Return:   0 if successful; negative errno otherwise.
*
* Abort:    none
*
************/
static int swimcu_pm_wusrc_config(
	struct swimcu *swimcup,
	int pm,
	struct swimcu_wusrc_config_state_s * statep)
{
	struct mci_wakeup_source_config_s wusrc_config = {0};
	enum swimcu_gpio_index gpio;
	enum swimcu_adc_index  adc;
	enum wusrc_index wi;
	int err_code;
	int irq, wrsrc_irq;
	unsigned int above, below;

	/* configure enabled wakeup sources per their types */
	for (wi = 0; wi < ARRAY_SIZE(wusrc_param); wi++)
	{
		switch (wusrc_param[wi].type)
		{
			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS:

				gpio = (enum swimcu_gpio_index) wusrc_param[wi].id;
				wrsrc_irq = swimcu_pm_wusrc_gpio_irq_get(gpio);
				if (wrsrc_irq == MCI_PIN_IRQ_DISABLED)
				{
					break;
				}

				/* save the current IRQ type for recovery just for in-case */
				err_code = swimcu_gpio_get(swimcup, SWIMCU_GPIO_GET_EDGE, gpio, &irq);
				if (err_code)
				{
					pr_err("%s: failed to get IRQ for gpio %d err=%d\n", __func__, gpio, err_code);
					return err_code;
				}
				statep->recovery_irqs[wi] = irq & 0xFF;

				/* set specific IRQ type configured by user */
				err_code = swimcu_gpio_set(swimcup, SWIMCU_GPIO_SET_EDGE, gpio, wrsrc_irq);
				if (err_code < 0)
				{
					pr_err("%s: failed to set irqc 0x%x for gpio %d (err=%d)\n",
						__func__, wrsrc_irq, gpio, err_code);
					return err_code;
				}

				if (wrsrc_irq != MCI_PIN_IRQ_DISABLED)
				{
					statep->gpio_pin_mask |= wusrc_param[wi].mask;
				}

				swimcu_log(INIT, "%s: configured GPIO wakeup source 0x%x \n",__func__, statep->gpio_pin_mask);

				break;

			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC:

				adc = wusrc_param[wi].id;
				if (!swimcu_pm_wusrc_adc_select_get(adc))
				{
					break;
				}

				do {
					/* set up HW tiemr driven ADC */
					err_code = swimcu_adc_set_trigger_mode(adc, MCI_PROTOCOL_ADC_TRIGGER_MODE_HW,
						swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL]);
					if (err_code)
					{
						pr_err("%s: failed (%d) to set ADC trigger mode \n",__func__, err_code);
						break;
					}

					/*
					* if above > below, then trigger when value falls in the range (0, below) or (above, 1800)
					* if above <= below, trigger when value falls in the range (above, below)
					*/
					above = swimcu_pm_wusrc_adc_above_get(adc);
					below = swimcu_pm_wusrc_adc_below_get(adc);
					if (above > below)
					{
						err_code = swimcu_adc_set_compare_mode(adc,
							MCI_PROTOCOL_ADC_COMPARE_MODE_BEYOND, above, below);
					}
					else
					{
						err_code = swimcu_adc_set_compare_mode(adc,
							MCI_PROTOCOL_ADC_COMPARE_MODE_WITHIN, above, below);
					}

					if (err_code)
					{
						pr_err("%s: failed (%d) to set ADC trigger mode\n",__func__, err_code);
						break;
					}

					err_code = swimcu_adc_init_and_start(swimcup, adc);
					if (err_code)
					{
						pr_err("%s: failed (%d) to start ADC\n",__func__, err_code);
						break;
					}
				}while (false);

				if (!err_code)
				{
					swimcu_log(INIT, "%s: config adc index %d as wakeup source\n", __func__, adc);
					statep->adc_pin_mask |= wusrc_param[wi].mask;
				}
				break;

			case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER:

				/* single source of the type; config depends on PM type requested */
				break;

			default:
				/* do nothing */;
				break;
		} /* end of switch statement */
	} /* end of wakeup source configuration for-loop */

	/* Specify GPIO and ADC as wakeup source if configured */
	pr_err("%s: check statep->gpio_pin_mask 0x%x \n",__func__, statep->gpio_pin_mask);
	if (statep->gpio_pin_mask)
	{
		wusrc_config.args.pins = statep->gpio_pin_mask;
		wusrc_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS;
		err_code = swimcu_wakeup_source_config(swimcup, &wusrc_config, MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET);
		if (err_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			pr_err("%s: failed to GPIO config 0x%x (%d)\n", __func__, statep->gpio_pin_mask, err_code);
			return -EIO;
		}

		statep->wusrc_mask |= (u16)MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS;
		pr_err("%s: statep->wusrc_mask=0x%x\n", __func__, statep->wusrc_mask);
	}

	if (statep->adc_pin_mask)
	{
		wusrc_config.args.pins = statep->adc_pin_mask;
		wusrc_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC;
		err_code = swimcu_wakeup_source_config(swimcup, &wusrc_config, MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET);
		if (err_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
		{
			pr_err("%s: failed to GPIO config 0x%x (%d)\n", __func__, statep->adc_pin_mask, err_code);
			return -EIO;
		}

		statep->wusrc_mask |= (u16)MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC;
		pr_err("%s: statep->adc_pin_mask=0x%x\n", __func__, statep->adc_pin_mask);
	}

	/* timer-based wakeup source configration */
	if (pm == SWIMCU_PM_PSM_SYNC)
	{
		err_code = swimcu_pm_psm_timer_config(swimcup);
		if (err_code)
		{
			pr_err("%s: failed to config timer wakeup source %d\n", __func__, err_code);
			return err_code;
		}

		statep->wusrc_mask |= MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;
	}
	else  /* Non-PSM ULPM */
	{
		if (swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT] > 0)
		{
			/* wakeup time has been validated upon user input (in seconds)
			*  which garantees no overflow in the following calculation.
			*/
			wusrc_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;
			wusrc_config.args.timeout =
				swimcu_mdm_sec_to_mcu_time_ms(swimcup,
					swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT]);
			err_code = swimcu_wakeup_source_config(swimcup,
				&wusrc_config, MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET);
			if (err_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
			{
				pr_err("%s: timer wu fail %d\n", __func__, err_code);
				return -EIO;
			}

			swimcu_pm_data[SWIMCU_PM_DATA_EXPECTED_ULPM_TIME] = wusrc_config.args.timeout;
			statep->wusrc_mask |= MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;

			swimcu_log(INIT, "%s: ULPM wakeup time %u (mcu=%u)\n", __func__,
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT], wusrc_config.args.timeout);
		}
	}

	return 0;
}

/************
*
* Name:     swimcu_pm_wusrc_config_reset
*
* Purpose:  To reset ULPM wakeup source configuration
*
* Parms:    swimcup - pointer to device driver data
*           statep  - pointer to wakeup source config state data
*
* Return:   none
*
* Note:     Best-effort to reverse the wakeup source configuration.
*
* Abort:    none
*
************/
static void swimcu_pm_wusrc_config_reset(
	struct swimcu *swimcup,
	struct swimcu_wusrc_config_state_s * statep)
{
	struct mci_wakeup_source_config_s wusrc_config;
	enum mci_protocol_wakeup_source_type_e type_mask;
	enum wusrc_index wi;
	enum swimcu_gpio_index gpio;
	enum mci_pin_irqc_type_e irq;
	enum mci_protocol_hw_timer_state_e timer_state;
	uint32_t timeout = 0;

	swimcu_log(INIT, "%s\n", __func__);
	if (statep->wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS)
	{
		for (wi = WUSRC_MIN; wi < WUSRC_NUM_GPIO; wi++)
		{
			/* disable interrupt for configurated GPIO wakeup source */
			if (statep->gpio_pin_mask & wusrc_param[wi].mask)
			{
				gpio = (enum swimcu_gpio_index) wusrc_param[wi].id;
				irq = swimcu_pm_wusrc_gpio_irq_get(gpio);
				if (statep->recovery_irqs[wi] != irq)
				{
					(void) swimcu_gpio_set(swimcup,
						SWIMCU_GPIO_SET_EDGE, gpio, statep->recovery_irqs[wi]);
				}
			}
		}
	}

	if (statep->wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER)
	{
		(void)mci_appl_timer_stop(swimcup, &timer_state, &timeout);
	}

	if (statep->wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC)
	{
		(void)swimcu_adc_deinit(swimcup);
	}

	/* Clear all configured wakeup source types */
	type_mask = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS;
	while (statep->wusrc_mask)
	{
		if (statep->wusrc_mask & type_mask)
		{
			wusrc_config.source_type = type_mask;
			switch (type_mask)
			{
				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS:

					wusrc_config.args.pins = statep->gpio_pin_mask;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER:

					wusrc_config.args.timeout = 0;
					break;

				case MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC:

					wusrc_config.args.pins = statep->adc_pin_mask;
					break;

				default:
					pr_err("%s ignore invalid wakeup source type 0x%x\n", __func__, type_mask);
			}

			(void)swimcu_wakeup_source_config(swimcup,
				&wusrc_config, MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_CLEAR);

			statep->wusrc_mask &= ~type_mask;
		}

		type_mask <<= 1;
	}
}

/************
*
* Name:     pm_set_mcu_ulpm_enable
*
* Purpose:  Configure MCU with triggers and enter ultra low power mode
*
* Parms:    swimcu - device driver data
*           pm - 0 , 1, 4, 5: do nothing
*              - 2: enter ULPM, power switch wakeup only
*              - 3: enter PSM
*              - 6: enter ULPM with user-configured wakeup sources
*
* Return:   0 if successful
*           -ERRNO otherwise
*
* Abort:    none
*
************/
static int pm_set_mcu_ulpm_enable(struct swimcu *swimcu, int pm)
{
	enum mci_protocol_status_code_e rc;
	struct swimcu_wusrc_config_state_s cfg_state = {0};
	bool watchdog_disabled = false;
	int ret = 0;

	enum mci_protocol_hw_timer_state_e timer_state;
	uint32_t timeout = 0;

	if ((pm < SWIMCU_PM_OFF) || (pm > SWIMCU_PM_MAX))
	{
		swimcu_log(PM, "%s: invalid power mode %d\n", __func__, pm);
		return -ERANGE;
	}

	if (pm == SWIMCU_PM_OFF) {
		swimcu_log(PM, "%s: disable\n", __func__);
		return 0;
	}

	if (pm == SWIMCU_PM_PSM_REQUEST ||
	    pm == SWIMCU_PM_PSM_IN_PROGRESS ||
	    pm == SWIMCU_PM_BOOT_SOURCE) {
		swimcu_log(PM, "%s: PSM request in progress %d\n",__func__, pm);
		return 0;
	}

	if (SWIMCU_ENABLE == swimcu_lpo_calibrate_enable)
	{
		/* stop the LPO calibration; continue the ULPM entry process */
		(void)swimcu_lpo_calibrate_do_enable(swimcu, SWIMCU_DISABLE);
	}

	swimcu_log(PM, "%s: process pm option %d\n", __func__, pm);

	if (swimcu_watchdog_enable == SWIMCU_ENABLE)
	{
		rc = mci_appl_timer_stop(swimcu, &timer_state, &timeout);
		if (rc == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
			swimcu_watchdog_enable = SWIMCU_DISABLE;
			watchdog_disabled = true;
			swimcu_log(PM, "%s: timer stopped in state %d with remaining time %d\n",
				__func__, timer_state, timeout);
		}
		else
		{
			pr_err("%s: cannot stop MCU Watchdog: %d\n", __func__, rc);
		}
	}

	/* Wakeup source must be configured except the software power down case */
	if (pm != SWIMCU_PM_POWER_SWITCH)
	{
		ret = swimcu_pm_wusrc_config(swimcu, pm, &cfg_state);
		if (ret != 0)
		{
			goto ULPM_CONFIG_FAILED;
		}
		pr_err("%s: wakeup source setup mask=0x%x\n", __func__, cfg_state.wusrc_mask);
	}


	if (!cfg_state.wusrc_mask && (pm != SWIMCU_PM_POWER_SWITCH))
	{
		pr_err("%s: no wake sources set for PSM/ULPM request %d\n", __func__, pm);
		goto ULPM_CONFIG_FAILED;
	}

	/* save the calibration and PSM data before power down */
	swimcu_pm_data_store(swimcu);

	if (pm == SWIMCU_PM_PSM_SYNC)
	{
		/* safe shutdown configured */
		swimcu_pm_state = PM_STATE_SYNC;

		/* In option A, MCU always on forever to wait for MDM message */
		if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A)
		{
			cfg_state.wusrc_mask &= ~MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;
		}
	}
	else
	{
		pr_info("%s: sending wait_time_config", __func__);
		rc = swimcu_pm_wait_time_config(swimcu, SWIMCU_PM_WAIT_SYNC_TIME, 0);
		if (MCI_PROTOCOL_STATUS_CODE_SUCCESS == rc)
		{
			swimcu_pm_state = PM_STATE_SYNC;
		}
		else if (MCI_PROTOCOL_STATUS_CODE_UNKNOWN_COMMAND == rc)
		{
			pr_info("%s: pm wait_time_config not recognized by MCU, \
				proceed with legacy shutdown\n", __func__);
			swimcu_pm_state = PM_STATE_SHUTDOWN;
		}
	}

	pr_info("%s: sending ulpm_config", __func__);
	rc = pm_ulpm_config(swimcu, cfg_state.wusrc_mask);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS !=rc)
	{
		pr_err("%s: pm enable fail %d\n", __func__, rc);
		ret = -EIO;
		goto ULPM_CONFIG_FAILED;
	}

	if(PM_STATE_SYNC == swimcu_pm_state) {
		call_usermodehelper(poweroff_argv[0], poweroff_argv, NULL, UMH_NO_WAIT);
	}

	return 0;

ULPM_CONFIG_FAILED:

	/* reverse the wakeup source configuration */
	swimcu_pm_wusrc_config_reset(swimcu, &cfg_state);

	if (watchdog_disabled)
	{
		rc = mci_appl_watchdog_start(swimcu,
			swimcu_watchdog_timeout * 1000, swimcu_watchdog_reset_delay * 1000);
		if (rc == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
			swimcu_watchdog_enable = SWIMCU_ENABLE;
			swimcu_log(PM, "%s: Watchdog timer restarted\n", __func__);
		}
		else
		{
			pr_err("%s: cannot restart MCU Watchdog: %d\n", __func__, rc);
		}
	}

	swimcu_pm_state = PM_STATE_IDLE;

	return ret;
}

/****************************
 * SYSFS nodes construction *
 ****************************/
/* sysfs entries to set IRQ from GPIO input as boot_source */
static ssize_t pm_gpio_edge_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	enum wusrc_index wi;
	enum mci_pin_irqc_type_e irqc_type;
	int ti;
	int gpio;
	int ret = -EINVAL;

	wi = find_wusrc_index_from_kobj(kobj);
	if ((wi != WUSRC_INVALID) && (wusrc_param[wi].type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS)) {
		gpio = wusrc_param[wi].id;
	}
	else {
		pr_err("%s: unrecognized GPIO %s\n", __func__, kobj->name);
		return -EINVAL;
	}

	irqc_type = swimcu_pm_wusrc_gpio_irq_get(gpio);

	for (ti = ARRAY_SIZE(swimcu_irq_type_name_map) - 1; ti > 0; ti--) {
		/* if never found we exit at ti == 0: "off" */
		if (irqc_type == swimcu_irq_type_name_map[ti].type) {
			swimcu_log(PM, "%s: found gpio %d trigger %d\n", __func__, gpio, ti);
			break;
		}
	}
	ret = scnprintf(buf, PAGE_SIZE, swimcu_irq_type_name_map[ti].name);

	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
};


static ssize_t pm_gpio_edge_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct swimcu *swimcup;
	enum wusrc_index wi;
	int ti;
	int gpio;
	int ret;

	/* find associated GPIO number and IRQ type */
	wi = find_wusrc_index_from_kobj(kobj);
	if ((wi == WUSRC_INVALID) ||
		(wusrc_param[wi].type != MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS))
	{
		pr_err("%s: unrecognized GPIO %s\n", __func__, kobj->name);
		return -EINVAL;
	}
	gpio = wusrc_param[wi].id;

	for (ti = ARRAY_SIZE(swimcu_irq_type_name_map) - 1; ti >= 0; ti--)
	{
		/* if never found we exit at ti == -1: invalid */
		if (sysfs_streq(buf, swimcu_irq_type_name_map[ti].name))
		{
			if (0 != swimcu_gpio_irq_support_check(gpio))
			{
				pr_err("%s: IRQ not supported on gpio%d\n", __func__, gpio);
				return -EPERM;
			}

			break;
		}
	}

	if(ti < 0) {
		pr_err("%s: unknown trigger %s\n", __func__, buf);
		return -EINVAL;
	}

	swimcup = container_of(kobj->parent, struct swimcu, pm_boot_source_kobj);
	ret = swimcu_gpio_set(swimcup,
		SWIMCU_GPIO_SET_EDGE, gpio, swimcu_irq_type_name_map[ti].type);
	if (ret < 0)
	{
		pr_err("%s: failed set IRQ for gpio %d ret=%d\n", __func__, gpio, ret);
		return ret;
	}

	/* successfully setup the wakeup source */
	swimcu_pm_wusrc_gpio_irq_set(gpio, swimcu_irq_type_name_map[ti].type);
	swimcu_pm_wusrc_status[wi].triggered = 0;

	return count;
};

static ssize_t pm_timer_timeout_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n",
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT]);
}

static ssize_t pm_timer_timeout_attr_store(struct kobject *kobj,
        struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t tmp_time;

	if (0 == (ret = kstrtouint(buf, 0, &tmp_time))) {
		if (tmp_time <= SWIMCU_MAX_TIME)
		{
			swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT] = tmp_time;
			swimcu_pm_wusrc_status[WUSRC_TIMER].triggered = 0;
			return count;
		}
		else
		{
			ret = -ERANGE;
		}
	}
	pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	return ret;
};

static ssize_t enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int tmp_enable;
	struct swimcu *swimcu = container_of(kobj, struct swimcu, pm_boot_source_kobj);

	ret = kstrtoint(buf, 0, &tmp_enable);
	if (0 == ret)
	{
		ret = pm_set_mcu_ulpm_enable(swimcu, tmp_enable);
		if (0 == ret) {
			swimcu_pm_enable = tmp_enable;
			sysfs_notify(&swimcu->pm_psm_kobj, NULL, "enable");
			ret = count;
		}
	}

	if (ret < 0)
		pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	return ret;
};

static ssize_t clear_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret, wi;
	struct swimcu_wusrc_config_state_s state = {0};
	struct swimcu *swimcup = container_of(kobj, struct swimcu, pm_boot_source_kobj);

	ret = kstrtoint(buf, 0, &state.wusrc_mask);
	if (0 == ret)
	{
		/* validate user input set bit masks */
		if ((state.wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ALL) &&
			(state.wusrc_mask & ~MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ALL) == 0)
		{
			/* For wakeup sources of other types (TIMTER and ADC), clear local config
			*  only because they are activated only on the entrance to PSM/ULPM.
			*  For wakeup sources of GPIO type, clear local config and disable GPIO
			*  interrupts on MCU, where may have been enabled when configured.
			*/
			if (state.wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER)
			{
				/* clear local configuration  */
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT] = 0;
			}

			if (state.wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC)
			{
				/* clear local configuration  */
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL] = SWIMCU_WUSRC_ADC_INTERVAL_DEFAULT;
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC2_CONFIG] = SWIMCU_WUSRC_ADC_THRES_DEFAULT;
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC3_CONFIG] = SWIMCU_WUSRC_ADC_THRES_DEFAULT;
			}

			if (state.wusrc_mask & MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS)
			{
				/* disable remote configuration on MCU */
				for (wi = WUSRC_MIN; wi < WUSRC_NUM_GPIO; wi++)
				{
					if (MCI_PIN_IRQ_DISABLED != swimcu_pm_wusrc_gpio_irq_get(wusrc_param[wi].id))
					{
						state.gpio_pin_mask |= wusrc_param[wi].mask;
						state.recovery_irqs[wi] = MCI_PIN_IRQ_DISABLED;
					}
				}
				swimcu_pm_wusrc_config_reset(swimcup, &state);

				/* clear local configuration */
				swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_GPIO_IRQS] = 0x0;
			}

			ret = count;
		}
		else
		{
			ret = -ERANGE;
		}
	}

	if (ret < 0)
		pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	return ret;
};

static ssize_t update_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct swimcu *swimcu = container_of(kobj, struct swimcu, pm_firmware_kobj);

	/* stop the LPO calibration before MCUFW update */
	(void)swimcu_lpo_calibrate_do_enable(swimcu, SWIMCU_DISABLE);

	/* transition the MCU to boot mode, reset then required to continue firmware update */
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS == swimcu_to_boot_transit(swimcu)) {
		ret = count;
	}
	else {
		ret = -EIO;
		pr_err("%s: invalid input %s\n", __func__, buf);
	}
	return ret;
};

static ssize_t available_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct swimcu *swimcu = container_of(kobj, struct swimcu, pm_firmware_kobj);
	bool available;

	available = (swimcu->version_major != 0) || (swimcu->version_minor != 0);
	return scnprintf(buf, PAGE_SIZE, "%d\n", available);
}

static ssize_t version_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct swimcu *swimcu = container_of(kobj, struct swimcu, pm_firmware_kobj);

	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS == swimcu_ping(swimcu)) {
	  (void) swimcu_pm_sysfs_opt_update(swimcu);
	}

	return scnprintf(buf, PAGE_SIZE, "%03d.%03d\n", swimcu->version_major, swimcu->version_minor);
}

static ssize_t triggered_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int triggered = 0;
	enum wusrc_index wi = find_wusrc_index_from_kobj(kobj);

	if (wi != WUSRC_INVALID)
	{
		triggered = swimcu_pm_wusrc_status[wi].triggered;
	}
	swimcu_log(PM, "%s: %d = %d\n", __func__, wi, triggered);

	return scnprintf(buf, PAGE_SIZE, "%d\n", triggered);
}

ADC_ATTR_SHOW(above)
ADC_ATTR_SHOW(below)
ADC_ATTR_SHOW(select)

ADC_ATTR_STORE(above)
ADC_ATTR_STORE(below)

static ssize_t pm_adc_select_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int select = 0;
	enum swimcu_adc_index adc;
	enum wusrc_index wi = find_wusrc_index_from_kobj(kobj);
	int ret = -EINVAL;
	int i;

	if (WUSRC_INVALID == wi) {
		return ret;
	}

	adc = wusrc_param[wi].id;
	if (0 == (ret = kstrtouint(buf, 0, &select))) {
		if (select > 1) {
			ret = -EINVAL;
		}
	}

	for (i = 0; i < SWIMCU_NUM_ADC; i++) {
		if (select && swimcu_pm_wusrc_adc_select_get(i) && (i != adc)) {
			pr_err("%s: cannot select more than 1 adc as boot_source", __func__);
			ret = -EPERM;
		}
	}

	if (!ret) {
		swimcu_pm_wusrc_adc_select_set(adc, select);
		ret = count;
	}
	return ret;
};

static ssize_t pm_adc_interval_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n",
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL]);
}

static ssize_t pm_adc_interval_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int interval;

	if (0 == (ret = kstrtouint(buf, 0, &interval))) {
		if (interval <= SWIMCU_MAX_TIME) {
			swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_ADC_INTERVAL] = interval;
			return count;
		}
		else {
			ret = -ERANGE;
		}
	}
	pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	return ret;
};

/* watchdog subdirectory */
static ssize_t swimcu_watchdog_timeout_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_watchdog_timeout);
}

static ssize_t swimcu_watchdog_timeout_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	uint32_t tmp_time;

	if (0 == (ret = kstrtouint(buf, 0, &tmp_time))) {
		if (tmp_time <= SWIMCU_MAX_TIME) {
			swimcu_watchdog_timeout = tmp_time;
			return count;
		}
		else {
			ret = -ERANGE;
		}
	}
	else
	{
		ret = -EINVAL;
	}

	if (ret < 0)
	{
		pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	}
	return ret;
};

static ssize_t swimcu_watchdog_reset_delay_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_watchdog_reset_delay);
}

static ssize_t swimcu_watchdog_reset_delay_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t tmp_time;

	if (0 == (ret = kstrtouint(buf, 0, &tmp_time))) {
		if (tmp_time <= SWIMCU_MAX_TIME) {
			swimcu_watchdog_reset_delay = tmp_time;
			ret = count;
		}
		else {
			ret = -ERANGE;
		}
	}
	else
	{
		ret = -EINVAL;
	}

	if (ret < 0)
	pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);

	return ret;
};

static ssize_t swimcu_watchdog_renew_count_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_watchdog_renew_count);
}

static ssize_t swimcu_watchdog_enable_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_watchdog_enable);
}

static ssize_t swimcu_watchdog_enable_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int tmp;
	struct swimcu *swimcup = container_of(kobj, struct swimcu, pm_watchdog_kobj);
	enum mci_protocol_status_code_e s_code;
	enum mci_protocol_hw_timer_state_e timer_state;
	u32 time_ms;

	if (0 != (ret = kstrtoint(buf, 0, &tmp))) {
		ret = -EINVAL;
		goto WATCHDOG_ENABLE_EXIT;
	}

	if ((tmp != SWIMCU_DISABLE) && (tmp != SWIMCU_ENABLE)) {
		ret = -ERANGE;
		goto WATCHDOG_ENABLE_EXIT;
	}

	if (tmp == SWIMCU_DISABLE) {
		s_code = mci_appl_timer_stop(swimcup, &timer_state, &time_ms);
		if (s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
			swimcu_log(PM, "%s: Watchdog timer stopped in state %d with remaining time %d\n",
				__func__, timer_state, time_ms);
		}
	}
	else {
		if ((swimcu_watchdog_timeout == 0) || swimcu_watchdog_reset_delay == 0) {
			pr_err("%s: invalid paarms for start operation timeout=%d reset delay=%d\n",
				__func__, swimcu_watchdog_timeout, swimcu_watchdog_reset_delay);
			ret = -EINVAL;
			goto WATCHDOG_ENABLE_EXIT;
		}

		s_code = mci_appl_watchdog_start(swimcup,
			swimcu_watchdog_timeout * 1000, swimcu_watchdog_reset_delay * 1000);
	}

	if (s_code == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		swimcu_watchdog_enable = tmp;
		ret = count;
	}
	else {
		pr_err("%s: failed MCU command status %d\n", __func__, s_code);
		ret = -EIO;
	}

WATCHDOG_ENABLE_EXIT:

	if (ret < 0) {
		pr_err("%s: input error %s ret %d\n", __func__, buf, ret);
	}

	return ret;
};

/* sysfs entry to access watchdog timeout configuration */
static const struct kobj_attribute swimcu_watchdog_timeout_attr = {
	.attr = {
		.name = "timeout",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_watchdog_timeout_attr_show,
	.store = &swimcu_watchdog_timeout_attr_store,
};

static const struct kobj_attribute swimcu_watchdog_reset_delay_attr = {
	.attr = {
		.name = "reset_delay",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_watchdog_reset_delay_attr_show,
	.store = &swimcu_watchdog_reset_delay_attr_store,
};

/* sysfs entries to enable/disable MCU watchdog */
static const struct kobj_attribute swimcu_watchdog_enable_attr = {
	.attr = {
		.name = "enable",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_watchdog_enable_attr_show,
	.store = &swimcu_watchdog_enable_attr_store,
};

static const struct kobj_attribute swimcu_watchdog_renew_count_attr = {
	.attr = {
		.name = "count",
		.mode = S_IRUGO
	},
	.show = &swimcu_watchdog_renew_count_attr_show,
};

static ssize_t swimcu_lpo_calibrate_mcu_time_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_lpo_calibrate_mcu_time);
}

static ssize_t swimcu_lpo_calibrate_mcu_time_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 tmp;

	/* calibration time interval change is not allowed during calibration */
	if (SWIMCU_ENABLE == swimcu_lpo_calibrate_enable)
	{
		pr_err("%s: Calibration in process\n", __func__);
		return -EIO;
	}

	if (0 != kstrtouint(buf, 0, &tmp))
	{
		return -EINVAL;
	}

	if ((tmp < SWIMCU_CALIBRATE_TIME_MIN) || (tmp > SWIMCU_CALIBRATE_TIME_MAX))
	{
		return -ERANGE;
	}

	swimcu_lpo_calibrate_mcu_time = tmp;
	return count;
}

static ssize_t swimcu_lpo_calibrate_mdm_time_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct swimcu *swimcup = container_of(kobj, struct swimcu, pm_calibrate_kobj);
	u32 mdm_time = swimcu_lpo_calibrate_mcu_time;

	mutex_lock(&swimcup->calibrate_mutex);
	mdm_time *= swimcup->calibrate_mdm_time;
	mdm_time /= swimcup->calibrate_mcu_time;
	mutex_unlock(&swimcup->calibrate_mutex);

	return scnprintf(buf, PAGE_SIZE, "%d\n", mdm_time);
}

static ssize_t swimcu_lpo_calibrate_enable_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_lpo_calibrate_enable);
}

static ssize_t swimcu_lpo_calibrate_enable_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct swimcu *swimcup = container_of(kobj, struct swimcu, pm_calibrate_kobj);
	int tmp, ret;

	if (0 != (ret = kstrtoint(buf, 0, &tmp)))
	{
		ret = -EINVAL;
	}
	else if ((tmp != SWIMCU_DISABLE) && (tmp != SWIMCU_ENABLE))
	{
		ret = -ERANGE;
	}
	else
	{
		ret = swimcu_lpo_calibrate_do_enable(swimcup, (bool)tmp);
	}

	if (ret < 0) {
		pr_err("%s: input error %s ret %d\n", __func__, buf, ret);
		return ret;
	}

	return count;
};

/* sysfs entry to access calibrate timeout configuration */
static const struct kobj_attribute swimcu_lpo_calibrate_mcu_time_attr = {
	.attr = {
		.name = "mcu_time",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_lpo_calibrate_mcu_time_attr_show,
	.store = &swimcu_lpo_calibrate_mcu_time_attr_store,
};

static const struct kobj_attribute swimcu_lpo_calibrate_mdm_time_attr = {
	.attr = {
		.name = "mdm_time",
		.mode = S_IRUGO},
	.show = &swimcu_lpo_calibrate_mdm_time_attr_show,
};

/* sysfs entries to enable/disable MCU calibrate */
static const struct kobj_attribute swimcu_lpo_calibrate_enable_attr = {
	.attr = {
		.name = "enable",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_lpo_calibrate_enable_attr_show,
	.store = &swimcu_lpo_calibrate_enable_attr_store,
};

/* sysfs entries to initiate restoration of TOD from MCU data */
static ssize_t swimcu_tod_update_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_pm_tod_update_status);
}

static ssize_t swimcu_tod_update_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct swimcu *swimcup = container_of(kobj, struct swimcu, pm_calibrate_kobj);
	int tmp, ret;

	/* This is one-time restoration operation for each startup */
	if (swimcu_pm_tod_update_status != SWIMCU_CALIBRATE_TOD_UPDATE_AVAIL)
	{
		return -EPERM;
	}

	if (0 != (ret = kstrtoint(buf, 0, &tmp)))
	{
		ret = -EINVAL;
	}
	else if (tmp)
	{
		swimcu_pm_tod_update(swimcup);
		ret = count;
	}
	else
	{
		ret = -ERANGE;
	}

	if (ret < 0) {
		pr_err("%s: input error %s ret %d\n", __func__, buf, ret);
	}

	return ret;
};

static const struct kobj_attribute swimcu_tod_update_attr = {
	.attr = {
		.name = "tod_update",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP },
	.show = &swimcu_tod_update_attr_show,
	.store = &swimcu_tod_update_attr_store,
};

/************
*
* Name:     swimcu_watchdog_event_handle
*
* Purpose:  To handle an MCU watchdog timeout event
*
* Parms:    swimcu - pointer to the SWIMCU data structure
*           delay  - time delay within which the MCU watchdog must be restarted;
*                    or the device will be reset by the MCU.
*
* Return:   Nothing
*
* Abort:    none
*
************/
void swimcu_watchdog_event_handle(struct swimcu *swimcup, u32 delay)
{
	int err;
	char event_str[] = "MCU_WATCHDOG";
	char *envp[] = { event_str, NULL };

	if (swimcu_pm_enable > SWIMCU_PM_OFF) {
		pr_err("%s: ULPM (%d) requested, do not renew watchdog\n", __func__, swimcu_pm_enable);
		return;
	}

	if (swimcu_watchdog_enable == SWIMCU_DISABLE) {
		pr_err("%s: Ignore an event for disabled MCU watchdog \n", __func__);
		return;
	}

	swimcu_log(PM, "%s: MCU watchdog event, reset delay=%d ms\n", __func__, delay);

	mci_appl_watchdog_start(swimcup,
		swimcu_watchdog_timeout * 1000, swimcu_watchdog_reset_delay * 1000);

	swimcu_log(PM, "%s: MCU watchdog renewed %d: timeout %d reset delay %d\n", __func__,
		swimcu_watchdog_renew_count, swimcu_watchdog_timeout, swimcu_watchdog_reset_delay);

	swimcu_watchdog_renew_count++;

	kobject_get(&swimcup->dev->kobj);
	err = kobject_uevent_env(&swimcup->dev->kobj, KOBJ_CHANGE, envp);
	if (err)
	{
		pr_err("%s: error %d signaling uevent\n", __func__, err);
	}
	kobject_put(&swimcup->dev->kobj);
}

/************
*
* Name:     swimcu_calibrate_event_handle
*
* Purpose:  To handle an MCU calibrate timeout event
*
* Parms:    swimcu    - pointer to the SWIMCU data structure
*           remainder - remainder of the timeout value when the timer is stopped
*
* Return:   Nothing
*
* Abort:    none
*
************/
void swimcu_calibrate_event_handle(struct swimcu *swimcup, u32 remainder)
{
	enum mci_protocol_status_code_e s_code;

	swimcu_log(INIT, "%s: MCU calibrate completed with remaining time %d\n", __func__, remainder);

	if (swimcu_lpo_calibrate_calc(swimcup, swimcu_lpo_calibrate_mcu_time - remainder))
	{
		swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
		return;
	}

	/* restart calibration */
	s_code = mci_appl_timer_calibrate_start(swimcup, swimcu_lpo_calibrate_mcu_time);
	if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
	{
		pr_err("%s: failed to restart LPO calibration status=%d\n", __func__, s_code);
		swimcu_lpo_calibrate_enable = SWIMCU_DISABLE;
		return;
	}

	getrawmonotonic(&swimcu_calibrate_start_tv);
}

/* PSM synchronization support directory */
static ssize_t swimcu_psm_sync_support_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 psm_opt;
	struct swimcu *swimcup;

	/* Extract PSM support option bits and shift right to make it starting from bit 0 */
	swimcup = container_of(kobj, struct swimcu, pm_psm_kobj);
	psm_opt = swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_ALL;
	psm_opt >>= 1;
	return scnprintf(buf, PAGE_SIZE, "%d\n", psm_opt);
}

static ssize_t swimcu_psm_sync_select_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_psm_sync_select);
}

static ssize_t swimcu_psm_sync_select_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u32 sync_support_mask, sync_select;
	struct swimcu *swimcup;

	ret = kstrtouint(buf, 0, &sync_select);
	if (0 == ret) {
		/* make sure the selected option is supported by the MCUFW */
		swimcup = container_of(kobj, struct swimcu, pm_psm_kobj);
		sync_support_mask = swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_ALL;

		/* user input is the bit position in the bitmask starting from bit 0
		*  supported PSM sync option in opt_func_mask starting from bit 1
		*/
		if ((sync_select > 0) && ((1 << sync_select) & sync_support_mask)) {
			swimcu_psm_sync_select = (enum mci_protocol_pm_psm_sync_option_e) sync_select;
			ret = count;
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
	}

	if (ret < 0) {
		pr_err("%s: invalid input %s ret %d\n", __func__, buf, ret);
	}
	return ret;
}

static ssize_t swimcu_psm_enable_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", swimcu_pm_enable);
}

static ssize_t swimcu_psm_enable_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp_enable, ret;
	struct swimcu *swimcup;

	ret = kstrtouint(buf, 0, &tmp_enable);
	if (0 == ret)
	{
		swimcup = container_of(kobj, struct swimcu, pm_psm_kobj);
		ret = pm_set_mcu_ulpm_enable(swimcup, tmp_enable);
		if (0 == ret) {
			swimcu_pm_enable = tmp_enable;
			sysfs_notify(kobj, NULL, "enable");
			ret = count;
		} else {
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
		pr_err("%s: invalid input %s\n", __func__, buf);
	}
	return ret;
}

static ssize_t swimcu_psm_active_time_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", swimcu_psm_active_time);
}

static ssize_t swimcu_psm_active_time_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp_active_time, ret;

	ret = kstrtouint(buf, 0, &tmp_active_time);
	if (!ret)
	{
		swimcu_psm_active_time = tmp_active_time;
		ret = count;
	} else {
		ret = -EINVAL;
		pr_err("%s: invalid input %s\n", __func__, buf);
	}
	return ret;
}

static ssize_t swimcu_psm_time_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n",
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT]);
}

static ssize_t swimcu_psm_time_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp_psm_time, ret;

	ret = kstrtouint(buf, 0, &tmp_psm_time);
	if (!ret)
	{
		swimcu_pm_data[SWIMCU_PM_DATA_WUSRC_TIMEOUT] = tmp_psm_time;
		ret = count;
	} else {
		ret = -EINVAL;
		pr_err("%s: invalid input %s\n", __func__, buf);
	}
	return ret;
}

/* sysfs entry to read PSM synchronization support options */
static const struct kobj_attribute swimcu_psm_sync_support_attr = {
	.attr = {
		.name = "sync_support",
		.mode = S_IRUGO
	},
	.show = &swimcu_psm_sync_support_attr_show,
};

/* sysfs entry to select PSM synchronization options */
static const struct kobj_attribute swimcu_psm_sync_select_attr = {
	.attr = {
		.name = "sync_select",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP
	},
	.show = &swimcu_psm_sync_select_attr_show,
	.store = &swimcu_psm_sync_select_attr_store,
};

/* sysfs entry to enable PSM/ULPM synchronization */
static const struct kobj_attribute swimcu_psm_enable_attr = {
	.attr = {
		.name = "enable",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP
	},
	.show = &swimcu_psm_enable_attr_show,
	.store = &swimcu_psm_enable_attr_store,
};

/* sysfs entry to set PSM/ULPM active time */
static const struct kobj_attribute swimcu_psm_active_time_attr = {
	.attr = {
		.name = "active_time",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP
	},
	.show = &swimcu_psm_active_time_attr_show,
	.store = &swimcu_psm_active_time_attr_store,
};

/* sysfs entry to set PSM/ULPM PSM time */
static const struct kobj_attribute swimcu_psm_time_attr = {
	.attr = {
		.name = "psm_time",
		.mode = S_IRUGO | S_IWUSR | S_IWGRP
	},
	.show = &swimcu_psm_time_attr_show,
	.store = &swimcu_psm_time_attr_store,
};

SWIMCU_PM_INT_ATTR(psm_status, 0, -11, 13, "status", true)

/************
*
* Name:     swimcu_set_wakeup_source
*
* Purpose:  Store the wakeup source to be read from triggered node
*
* Parms:    type - type of the wakeup source
*           value - information of the wakeup source (type-dependent)
*
* Return:   Nothing
*
* Abort:    none
*
************/
void swimcu_set_wakeup_source(enum mci_protocol_wakeup_source_type_e type, u16 value)
{
	enum wusrc_index wi = WUSRC_INVALID;

	swimcu_log(PM, "%s: type %d val 0x%x\n", __func__, type, value);

	if (type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER) {
		wi = WUSRC_TIMER;
	}
	else if (type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC) {
		enum swimcu_adc_index adc = swimcu_get_adc_from_chan(value);
		wi = find_wusrc_index_from_id(type, adc);
	}
	else if (type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS){
		int port = GET_WUSRC_PORT(value);
		int pin = GET_WUSRC_PIN(value);
		enum swimcu_gpio_index gpio = swimcu_get_gpio_from_port_pin(port, pin);
		wi = find_wusrc_index_from_id(type, gpio);
	}

	if (wi != WUSRC_INVALID) {
		swimcu_log(PM, "%s: %d\n", __func__, wi);
		swimcu_pm_wusrc_status[wi].triggered = 1;
	}
	else {
		pr_err("%s: unknown wakeup pin 0x%x\n", __func__, value);
	}
}

/************
*
* Name:     swimcu_set_reset_source
*
* Purpose:  Store the reset source to be read from reset_source parameter
*
* Parms:    value - reset source bit mask (mci_protocol_reset_source_e)
*
* Return:   Nothing
*
* Abort:    none
*
************/
void swimcu_set_reset_source(enum mci_protocol_reset_source_e value)
{
	swimcu_log(INIT, "%s: 0x%x\n", __func__, value);
	swimcu_reset_source = value;
}

/* MCU has 2 interruptible GPIOs, PTA0 and PTB0 that map to index 2 and 4
 * respectively on gpiochip200, which in turn appear as GPIO36 and 38 on the WP76xx */
static const struct kobj_attribute pm_gpio_edge_attr[] = {
	__ATTR(edge,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_gpio_edge_attr_show,
		&pm_gpio_edge_attr_store),
};

static const struct kobj_attribute pm_triggered_attr = __ATTR_RO(triggered);

/* sysfs entries to set boot_source timer timeout value */
static const struct kobj_attribute pm_timer_timeout_attr[] = {
	__ATTR(timeout,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_timer_timeout_attr_show,
		&pm_timer_timeout_attr_store),
};

/* sysfs entries to set boot_source enable */
static const struct kobj_attribute swimcu_pm_enable_attr = __ATTR_WO(enable);

/* sysfs entries to clear wakeup source of selected types */
static const struct kobj_attribute swimcu_pm_wusrc_clear_attr = __ATTR_WO(clear);

/* sysfs entries to initiate firmware upgrade */
static const struct kobj_attribute fw_update_attr = __ATTR_WO(update);

/* sysfs entry to read current mcu firmware version */
static const struct kobj_attribute fw_version_attr = __ATTR_RO(version);

/* sysfs entry to show MCU available without firmware version query */
static const struct kobj_attribute fw_available_attr = __ATTR_RO(available);

static const struct kobj_attribute pm_adc_trig_attr[] = {
	__ATTR(below,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_adc_below_attr_show,
		&pm_adc_below_attr_store),

	__ATTR(above,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_adc_above_attr_show,
		&pm_adc_above_attr_store),

	__ATTR(select,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_adc_select_attr_show,
		&pm_adc_select_attr_store),
};

static const struct kobj_attribute pm_adc_interval_attr = \
	__ATTR(interval,
		S_IRUGO | S_IWUSR | S_IWGRP,
		&pm_adc_interval_attr_show,
		&pm_adc_interval_attr_store);

/************
*
* Name:     swimcu_pm_sysfs_deinit
*
* Purpose:  Remove sysfs tree under /sys/module/swimcu_pm
*
* Parms:    swimcu - device data ptr
*
* Return:   nothing
*
* Abort:    none
*
************/
void swimcu_pm_sysfs_deinit(struct swimcu *swimcu)
{
	if (swimcu->pm_firmware_kobj.state_initialized)
		kobject_put(&swimcu->pm_firmware_kobj);
	if (swimcu->pm_boot_source_kobj.state_initialized)
		kobject_put(&swimcu->pm_boot_source_kobj);
}

/************
*
* Name:     swimcu_pm_sysfs_init
*
* Purpose:  Setup sysfs tree under /sys/module/swimcu_pm
*
* Parms:    swimcu - device data ptr contains kobj data so attr functions
*                    can retrieve this structure to access the driver (e.g. i2c)
*           func_flags - select which attributes to init
*                        SWIMCU_FUNC_FLAG_FWUPD - firmware
*                        SWIMCU_FUNC_FLAG_PM - boot_source
*
* Return:   0 if successful
*           -ERRNO otherwise
*
* Abort:    none
*
************/
int swimcu_pm_sysfs_init(struct swimcu *swimcu, int func_flags)
{
	struct kobject *module_kobj;
	struct boot_sources_s {
		struct kobject **kobj;
		struct kobject *kobj_parent;
		const struct kobj_attribute *custom_attr;
		char *name;
		unsigned int num_cust_kobjs;
	} boot_source[] = {
		{&swimcu_pm_wusrc_status[WUSRC_GPIO36].kobj, &swimcu->pm_boot_source_kobj,     pm_gpio_edge_attr,     "gpio36", ARRAY_SIZE(pm_gpio_edge_attr)},
		{&swimcu_pm_wusrc_status[WUSRC_GPIO38].kobj, &swimcu->pm_boot_source_kobj,     pm_gpio_edge_attr,     "gpio38", ARRAY_SIZE(pm_gpio_edge_attr)},
		{&swimcu_pm_wusrc_status[WUSRC_TIMER].kobj,  &swimcu->pm_boot_source_kobj,     pm_timer_timeout_attr, "timer",  ARRAY_SIZE(pm_timer_timeout_attr)},
		{&swimcu_pm_wusrc_status[WUSRC_ADC2].kobj,   &swimcu->pm_boot_source_adc_kobj, pm_adc_trig_attr,      "adc2",   ARRAY_SIZE(pm_adc_trig_attr)},
		{&swimcu_pm_wusrc_status[WUSRC_ADC3].kobj,   &swimcu->pm_boot_source_adc_kobj, pm_adc_trig_attr,      "adc3",   ARRAY_SIZE(pm_adc_trig_attr)},
	};

	int i;
	int j;
	int ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto sysfs_add_exit;
	}

	swimcu_log(INIT, "%s: func_flags=0x%x\n", __func__, func_flags);
	if (func_flags & SWIMCU_FUNC_FLAG_FWUPD) {
	/* firmware object */

		ret = kobject_init_and_add(&swimcu->pm_firmware_kobj, &ktype, module_kobj, "firmware");
		if (ret) {
			pr_err("%s: cannot create firmware kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_firmware_kobj, &fw_version_attr.attr);
		if (ret) {
			pr_err("%s: cannot create version\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_firmware_kobj, &fw_update_attr.attr);
		if (ret) {
			pr_err("%s: cannot create update\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_firmware_kobj, &fw_available_attr.attr);
		if (ret) {
			pr_err("%s: cannot create MCUFW available: ret=%d\n", __func__, -ret);
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_firmware_kobj, KOBJ_ADD);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_PM) {
	/* boot_source object */

		ret = kobject_init_and_add(&swimcu->pm_boot_source_kobj, &ktype, module_kobj, "boot_source");
		if (ret) {
			pr_err("%s: cannot create boot_source kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}
		ret = kobject_init_and_add(&swimcu->pm_boot_source_adc_kobj, &ktype, &swimcu->pm_boot_source_kobj, "adc");
		if (ret) {
			pr_err("%s: cannot create adc kobject for boot_source\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}
		ret = sysfs_create_file(&swimcu->pm_boot_source_adc_kobj, &pm_adc_interval_attr.attr);
		if (ret) {
			pr_err("%s: cannot create interval file for adc\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}
		/* populate boot_source sysfs tree */
		for (i = 0; i < ARRAY_SIZE(boot_source); i++) {
			swimcu_log(PM, "%s: create kobj %d for %s", __func__, i, boot_source[i].name);
			*boot_source[i].kobj = kobject_create_and_add(boot_source[i].name, boot_source[i].kobj_parent);
			if (!*boot_source[i].kobj) {
				pr_err("%s: cannot create boot_source kobject for %s\n", __func__, boot_source[i].name);
				ret = -ENOMEM;
				goto sysfs_add_exit;
			}
			ret = sysfs_create_file(*boot_source[i].kobj, &pm_triggered_attr.attr);
			if (ret) {
				pr_err("%s: cannot create triggered file for %s\n", __func__, boot_source[i].name);
				ret = -ENOMEM;
				goto sysfs_add_exit;
			}
			for (j = 0; j < boot_source[i].num_cust_kobjs; j++) {
				ret = sysfs_create_file(*boot_source[i].kobj, &boot_source[i].custom_attr[j].attr);
				if (ret) {
					pr_err("%s: cannot create custom file for %s\n", __func__, boot_source[i].name);
					ret = -ENOMEM;
					goto sysfs_add_exit;
				}
			}
		}

		ret = sysfs_create_file(&swimcu->pm_boot_source_kobj, &swimcu_pm_enable_attr.attr);
		if (ret) {
			pr_err("%s: cannot create enable\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_boot_source_kobj, &swimcu_pm_wusrc_clear_attr.attr);
		if (ret) {
			pr_err("%s: cannot create clear\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_boot_source_kobj, KOBJ_ADD);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_PSM) {
	/* power save mode object */
		ret = kobject_init_and_add(&swimcu->pm_psm_kobj, &ktype, module_kobj, "psm");
		if (ret) {
			pr_err("%s: cannot create PSM kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_sync_support_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync support node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_sync_select_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync select node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_enable_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync enable node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_psm_kobj, KOBJ_ADD);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_CALIBRATE)
	{
		ret = kobject_init_and_add(&swimcu->pm_calibrate_kobj, &ktype, module_kobj, "calibrate");
		if (ret)
		{
			pr_err("%s: cannot create CALIBRATE kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_calibrate_kobj, &swimcu_lpo_calibrate_mcu_time_attr.attr);
		if (ret)
		{
			pr_err("%s: cannot create CALIBRATE mcu timeout node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_calibrate_kobj, &swimcu_lpo_calibrate_mdm_time_attr.attr);
		if (ret)
		{
			pr_err("%s: cannot create CALIBRATE mdm time node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_calibrate_kobj, &swimcu_lpo_calibrate_enable_attr.attr);
		if (ret)
		{
			pr_err("%s: cannot create CALIBRATE calibrate enable node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_calibrate_kobj, KOBJ_ADD);

		/* Start calibration if no post-ULPM TOD udpate will be performed;
		*  otherwise, expose the "tod_update" node for user to trigger TOD
		*  update, after which the calibration will be started
		*/
		if (swimcu_pm_data[SWIMCU_PM_DATA_PRE_ULPM_TOD] == 0)
		{
			swimcu_pm_lpo_calibrate_start(swimcu);
		}
		else
		{
			swimcu_pm_tod_update_status = SWIMCU_CALIBRATE_TOD_UPDATE_AVAIL;
			ret = sysfs_create_file(&swimcu->pm_calibrate_kobj, &swimcu_tod_update_attr.attr);
			if (ret)
			{
				pr_err("%s: cannot create CALIBRATE TOD restore node\n", __func__);
				ret = -ENOMEM;
				goto sysfs_add_exit;
			}
		}
	}

	if (func_flags & SWIMCU_FUNC_FLAG_WATCHDOG) {

		ret = kobject_init_and_add(&swimcu->pm_watchdog_kobj, &ktype, module_kobj, "watchdog");
		if (ret) {
			pr_err("%s: cannot create WATCHDOG kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_watchdog_kobj, &swimcu_watchdog_timeout_attr.attr);
		if (ret) {
			pr_err("%s: cannot create WATCHDOG timeout node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_watchdog_kobj, &swimcu_watchdog_reset_delay_attr.attr);
		if (ret) {
			pr_err("%s: cannot create WATCHDOG reset dealy node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_watchdog_kobj, &swimcu_watchdog_renew_count_attr.attr);
		if (ret) {
			pr_err("%s: cannot create WATCHDOG renew node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}
		ret = sysfs_create_file(&swimcu->pm_watchdog_kobj, &swimcu_watchdog_enable_attr.attr);
		if (ret) {
			pr_err("%s: cannot create WATCHDOG enable node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_watchdog_kobj, KOBJ_ADD);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_PSM) {
		ret = kobject_init_and_add(&swimcu->pm_psm_kobj, &ktype, module_kobj, "psm");
		if (ret) {
			pr_err("%s: cannot create PSM kobject\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_sync_support_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync support node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		/* set default sync option for the first-time power up */
		if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE)
		{
			swimcu_psm_sync_select = swimcu_pm_psm_sync_option_default(swimcu);
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_sync_select_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync select node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_enable_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM sync enable node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_active_time_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM active_time node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_time_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM psm_time node\n", __func__);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		ret = sysfs_create_file(&swimcu->pm_psm_kobj, &swimcu_psm_status_attr.attr);
		if (ret) {
			pr_err("%s: cannot create PSM status node (ret=%d)\n", __func__, ret);
			ret = -ENOMEM;
			goto sysfs_add_exit;
		}

		kobject_uevent(&swimcu->pm_psm_kobj, KOBJ_ADD);
	}


	swimcu_log(INIT, "%s: success func=0x%x\n", __func__, func_flags);
	return 0;

sysfs_add_exit:
	swimcu_log(INIT, "%s: fail func=0x%x, ret %d\n", __func__, func_flags, ret);
	swimcu_pm_sysfs_deinit(swimcu);
	return ret;
}

/************
*
* Name:     swimcu_pm_opt_sysfs_remove
*
* Purpose:  Remove specific sysfs tree(s) under /sys/module/swimcu_pm
*
* Parms:    swimcup    - pointer to device data structure.
*           func_flags - bitmask indicates the functions of which the sysfs tree
*                        to be removed:
*                        SWIMCU_FUNC_FLAG_WATCHDOG
*                        SWIMCU_FUNC_FLAG_PSM
*
* Return:   0 if successful;
*           -ERRNO otherwise
*
* Abort:    none
*
************/
static void swimcu_pm_opt_sysfs_remove(struct swimcu *swimcup, int func_flags)
{
	struct kobject *module_kobj;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj)
	{
		pr_err("%s: cannot find kobject for module %s\n", __func__, KBUILD_MODNAME);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_WATCHDOG) {

		swimcu_log(INIT, "%s: remove WATCHDOG sysfs nodes\n", __func__);

		kobject_uevent(&swimcup->pm_watchdog_kobj, KOBJ_REMOVE);

		sysfs_remove_file(&swimcup->pm_watchdog_kobj, &swimcu_watchdog_timeout_attr.attr);
		sysfs_remove_file(&swimcup->pm_watchdog_kobj, &swimcu_watchdog_reset_delay_attr.attr);
		sysfs_remove_file(&swimcup->pm_watchdog_kobj, &swimcu_watchdog_renew_count_attr.attr);
		sysfs_remove_file(&swimcup->pm_watchdog_kobj, &swimcu_watchdog_enable_attr.attr);

		/* unlink kobject from hierarchy. */
		kobject_del(&swimcup->pm_watchdog_kobj);
	}

	if (func_flags & SWIMCU_FUNC_FLAG_PSM) {

		swimcu_log(INIT, "%s: remove PSM sysfs nodes\n", __func__);

		kobject_uevent(&swimcup->pm_psm_kobj, KOBJ_REMOVE);

		sysfs_remove_file(&swimcup->pm_psm_kobj, &swimcu_psm_sync_support_attr.attr);
		sysfs_remove_file(&swimcup->pm_psm_kobj, &swimcu_psm_sync_select_attr.attr);
		sysfs_remove_file(&swimcup->pm_psm_kobj, &swimcu_psm_enable_attr.attr);

		kobject_del(&swimcup->pm_psm_kobj);
	}
}

/************
*
* Name:     swimcu_pm_sysfs_opt_update
*
* Purpose:  Update a sysfs tree under /sys/module/swimcu_pm for optional function
*
* Parms:    swimcup - pointer to device data structure
*
* Return:   0 if successful;
*           -ERRNO otherwise
*
* Abort:    none
*
************/
int swimcu_pm_sysfs_opt_update(struct swimcu *swimcup)
{
	int ret = 0;

	if ((swimcup->version_major > SWIMCU_CALIBRATE_SUPPORT_VER_MAJOR) ||
			((swimcup->version_major == SWIMCU_CALIBRATE_SUPPORT_VER_MAJOR) &&
			(swimcup->version_minor >= SWIMCU_CALIBRATE_SUPPORT_VER_MINOR))) {
		if (!(swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_CALIBRATE)) {
			ret = swimcu_pm_sysfs_init(swimcup, SWIMCU_FUNC_FLAG_CALIBRATE);
			if (0 == ret) {
				swimcup->driver_init_mask |= SWIMCU_DRIVER_INIT_CALIBRATE;
			} else {
				dev_err(swimcup->dev, "Calibrate sysfs init failed\n");
				return ret;
			}
		}
	} else {
		if (swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_CALIBRATE) {
			swimcu_pm_opt_sysfs_remove(swimcup, SWIMCU_FUNC_FLAG_CALIBRATE);
			swimcup->driver_init_mask &= ~SWIMCU_FUNC_FLAG_CALIBRATE;
		}
	}

	if (swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_WATCHDOG) {
		if (!(swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_WATCHDOG)) {
			ret = swimcu_pm_sysfs_init(swimcup, SWIMCU_FUNC_FLAG_WATCHDOG);
			if (0 == ret) {
				swimcup->driver_init_mask |= SWIMCU_DRIVER_INIT_WATCHDOG;
			} else {
				dev_err(swimcup->dev, "WATCHDOG sysfs init failed\n");
				return ret;
			}
		}
	} else {
		if (swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_WATCHDOG) {
			swimcu_pm_opt_sysfs_remove(swimcup, SWIMCU_FUNC_FLAG_WATCHDOG);
			swimcup->driver_init_mask &= ~SWIMCU_DRIVER_INIT_WATCHDOG;
		}
	}

	if (swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_ALL) {
		if (!(swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_PSM)) {
			ret = swimcu_pm_sysfs_init(swimcup, SWIMCU_FUNC_FLAG_PSM);
			if (0 == ret) {
				swimcup->driver_init_mask |= SWIMCU_DRIVER_INIT_PSM;
			} else {
				dev_err(swimcup->dev, "PSM sysfs init failed\n");
				return ret;
			}
		}
	} else {
		if (swimcup->driver_init_mask & SWIMCU_DRIVER_INIT_PSM) {
			swimcu_pm_opt_sysfs_remove(swimcup, SWIMCU_FUNC_FLAG_PSM);
			swimcup->driver_init_mask &= ~SWIMCU_DRIVER_INIT_PSM;
		}
	}

	return 0;
}

