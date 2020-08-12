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
#define SWIMCU_CALIBRATE_TIME_DEFAULT        25000    /* milliseconds */

/* The precision of the calibrate result is truncated by this factor
*  to avoid overflow in calculation of user-input time conversion
*  (expect max 10% deviation for the MCU LPO 1K clock).
*/
#define SWIMCU_CALIBRATE_TRUNCATE_FACTOR     50

/* The MCU timer timeout value is trimmed off by this percentage
*  to migigate possible deviation for the MCU LPO 1K clock over
*  temerature (expect max 1.5%).
*/
#define SWIMCU_CALIBRATE_TEMPERATURE_FACTOR  2       /* percent */

/* Constants for MCU watchdog feature */
#define SWIMCU_WATCHDOG_TIMEOUT_INVALID      0
#define SWIMCU_WATCHDOG_RESET_DELAY_DEFAULT  1         /* second */

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
			value = adc_trigger_config[adc].name;           \
		}                                                       \
		return scnprintf(buf, PAGE_SIZE, "%d\n", value);        \
	}

#define ADC_ATTR_STORE(name)                                      	\
	static ssize_t pm_adc_##name##_attr_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
	{                                                               \
		unsigned int value = 0;                                 \
		enum swimcu_adc_index adc;                              \
		enum wusrc_index wi = find_wusrc_index_from_kobj(kobj); \
		int ret;						\
	                                                                \
		if (WUSRC_INVALID == wi) {                              \
			return -EINVAL; 			        \
		}                                                       \
		adc = wusrc_param[wi].id;				\
		ret = kstrtouint(buf, 0, &value);                       \
		if (!ret) {                                             \
			if (value <= SWIMCU_ADC_VREF) {			\
				adc_trigger_config[adc].name = value;   \
				ret = count;				\
			}						\
			else {						\
				ret = -ERANGE;				\
			}						\
		}							\
		return ret;						\
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

static const struct pin_trigger_map {
	enum mci_pin_irqc_type_e type;
	char *name;
} pin_trigger[] = {
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

static const struct wusrc_param {
	enum mci_protocol_wakeup_source_type_e type;
	int id;
	uint32_t mask;
} wusrc_param[] = {
	[WUSRC_GPIO36] = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS, SWIMCU_GPIO_PTA0, MCI_PROTOCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTA0},
	[WUSRC_GPIO38] = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS, SWIMCU_GPIO_PTB0, MCI_PROTOCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTB0},
	[WUSRC_TIMER]  = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER, 0, 0},
	[WUSRC_ADC2]   = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC, SWIMCU_ADC_PTA12, MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTA12},
	[WUSRC_ADC3]   = {MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC, SWIMCU_ADC_PTB1, MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTB1},
};

static struct wusrc_value {
	struct kobject *kobj;
	int triggered;
} wusrc_value[] = {
	[WUSRC_GPIO36] = {NULL, 0},
	[WUSRC_GPIO38] = {NULL, 0},
	[WUSRC_TIMER]  = {NULL, 0},
	[WUSRC_ADC2]   = {NULL, 0},
	[WUSRC_ADC3]   = {NULL, 0},
};

static struct adc_trigger_config {
	unsigned int above;
	unsigned int below;
	bool select;
} adc_trigger_config[] = {
	[SWIMCU_ADC_PTA12] = {0, 1800, false},
	[SWIMCU_ADC_PTB1]  = {0, 1800, false},
};

static uint32_t adc_interval = 0;

/* SWIMCU_PM_DATA_WUSRC_GPIO_IRQS */
static uint8_t swimcu_wusrc_gpio_irq_cfg[WUSRC_NUM_GPIO] = {MCI_PIN_IRQ_DISABLED};

static char* poweroff_argv[] = {"/sbin/poweroff", NULL};

#define SWIMCU_PM_WAIT_SYNC_TIME 40000

#define PM_STATE_IDLE     0
#define PM_STATE_SYNC     1
#define PM_STATE_SHUTDOWN 2

/* Power management configuration */
static u32 swimcu_wakeup_time = 0;
static int swimcu_pm_enable = SWIMCU_PM_OFF;
static int swimcu_pm_state = PM_STATE_IDLE;

/* MCU watchdog configuration */
static int swimcu_watchdog_enable  = SWIMCU_DISABLE;
static u32 swimcu_watchdog_timeout = SWIMCU_WATCHDOG_TIMEOUT_INVALID;
static u32 swimcu_watchdog_reset_delay = SWIMCU_WATCHDOG_RESET_DELAY_DEFAULT;
static u32 swimcu_watchdog_renew_count = 0;

/* MCU psm support configuration */
static u32 swimcu_psm_active_time = 0;
static enum mci_protocol_pm_psm_sync_option_e
                swimcu_psm_sync_select = MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE;

#define SWIMCU_PM_DATA_GROUP_INDEX_0              0
#define SWIMCU_PM_DATA_INDEX_CALIBRATE_MDM_TIME   0
#define SWIMCU_PM_DATA_INDEX_CALIBRATE_MCU_TIME   1
#define SWIMCU_PM_DATA_INDEX_EXPECTED_ULPM_TIME   2
#define SWIMCU_PM_DATA_INDEX_PRE_ULPM_RTC_TIME    3
#define SWIMCU_PM_DATA_INDEX_RESERVED             4

static u32 swimcu_pm_data_group[MCI_PROTOCOL_DATA_GROUP_SIZE];

/* SYSFS support for MCU 1K LPO clock calibration */
static int swimcu_lpo_calibrate_enable   = SWIMCU_DISABLE;
static u32 swimcu_lpo_calibrate_mcu_time = SWIMCU_CALIBRATE_TIME_DEFAULT;
static struct timespec swimcu_calibrate_start_tv;

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

	switch (index)
	{
		case SWIMCU_GPIO_PTA0:

			swimcu_wusrc_gpio_irq_cfg[WUSRC_GPIO36] = 0xFF & irq;
			break;

		case SWIMCU_GPIO_PTB0:

			swimcu_wusrc_gpio_irq_cfg[WUSRC_GPIO38] = 0xFF & irq;
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

	switch (index)
	{
		case SWIMCU_GPIO_PTA0:

			irq = (enum mci_pin_irqc_type_e) swimcu_wusrc_gpio_irq_cfg[WUSRC_GPIO36];
			break;

		case SWIMCU_GPIO_PTB0:

			irq = (enum mci_pin_irqc_type_e) swimcu_wusrc_gpio_irq_cfg[WUSRC_GPIO38];
			break;

		default:

			irq = MCI_PIN_IRQ_DISABLED;
	}

	return irq;
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
	u32 mcu_time, remainder;

	pr_info("%s: mdm time=%u seconds to be calibrated %d/%d\n", __func__,
		mdm_time, swimcup->calibrate_mcu_time, swimcup->calibrate_mdm_time);

	mutex_lock(&swimcup->calibrate_mutex);
	mdm_time *= swimcup->calibrate_mcu_time;
	mcu_time = mdm_time / swimcup->calibrate_mdm_time; /* seconds */

	/* keep millisecond precision on MCU side */
	remainder = mdm_time % swimcup->calibrate_mdm_time;
	remainder *= MSEC_PER_SEC;                          /* milliseconds */
	remainder = mdm_time / swimcup->calibrate_mdm_time; /* milliseconds*/

	mutex_unlock(&swimcup->calibrate_mutex);

	mcu_time *= MSEC_PER_SEC;                           /* milliseconds */

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

	/* Truncated calibration results to prevent overflow in conversion */
	swimcup->calibrate_mdm_time = mdm_time / SWIMCU_CALIBRATE_TRUNCATE_FACTOR;
	swimcup->calibrate_mcu_time = mcu_time / SWIMCU_CALIBRATE_TRUNCATE_FACTOR;
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
		if (wusrc_value[wi].kobj == kobj) {
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
			if (MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
                           (rc = swimcu_pm_wait_time_config(swimcu, 0, 0))) {
				pr_err("%s: pm wait_time_config failed %d\n", __func__, rc);
			}

			if(MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
                          (rc = pm_ulpm_config(swimcu, 0))) {
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
	if (swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_3)
	{
		return MCI_PROTOCOL_PM_PSM_SYNC_OPTION_C;
	}
	else if (swimcup->opt_func_mask & MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_2)
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
	unsigned long rtc_secs, alarm_secs;
	u32 interval;
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
		pr_info("%s: alarm %lu rtc %lu interval %u", __func__, alarm_secs, rtc_secs, interval);
	}
	else
	{
		interval = 0;
		pr_err("%s: invalid configuration alarm %lu rtc %lu", __func__, alarm_secs, rtc_secs);
	}

	return interval;
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

	/* Save MCU clock calibration data */
	mutex_lock(&swimcup->calibrate_mutex);
	swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_CALIBRATE_MDM_TIME] =
		swimcup->calibrate_mdm_time;
	swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_CALIBRATE_MCU_TIME] =
		swimcup->calibrate_mcu_time;
	mutex_unlock(&swimcup->calibrate_mutex);

	/* Save pre-ULPM RTC clock */
	tv.tv_sec = 0;
	do_gettimeofday(&tv);
	tv.tv_sec += (tv.tv_usec + USEC_PER_SEC/2)/USEC_PER_SEC;
	swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_PRE_ULPM_RTC_TIME] = tv.tv_sec;
	pr_info("%s: sending persistent data to MCU\n", __func__);

	s_code = swimcu_appl_data_store(swimcup,
		SWIMCU_PM_DATA_GROUP_INDEX_0, swimcu_pm_data_group, MCI_PROTOCOL_DATA_GROUP_SIZE);
	if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
	{
		/* it may fail if MCUFW does not support the feature but continue any way */
		pr_err("%s: failed to store data to MCU %d\n", __func__, s_code);
	}
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
	int count;

	count = MCI_PROTOCOL_DATA_GROUP_SIZE;
	s_code = swimcu_appl_data_retrieve(swimcup, 0, swimcu_pm_data_group, &count);
	if (s_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS)
	{
		pr_err("%s: failed to retrive data stored on MCU\n", __func__);
		return;
	}

	if (count && (swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_CALIBRATE_MCU_TIME] != 0))
	{
		/* restore the calibration data */
		mutex_lock(&swimcup->calibrate_mutex);
		swimcup->calibrate_mcu_time =
			swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_CALIBRATE_MCU_TIME];
		swimcup->calibrate_mdm_time =
			swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_CALIBRATE_MDM_TIME];
		mutex_unlock(&swimcup->calibrate_mutex);
	}
}

/************
*
* Name:     swimcu_pm_rtc_restore
*
* Purpose:  To restore PMIC RTC time
*
* Parms:    swimcup - pointer to device driver data
*
* Return:   none
*
* Abort:    none
*
************/
void swimcu_pm_rtc_restore(struct swimcu *swimcup)
{
	enum mci_protocol_status_code_e s_code;
	enum mci_protocol_pm_psm_sync_option_e sync_opt;
	u32 ulpm_time;
	struct timespec tv;
	int ret;

	s_code = swimcu_appl_psm_duration_get(swimcup, &ulpm_time, &sync_opt);
	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code)
	{
		pr_err("%s: failed to get ULPM duration: %d\n", __func__, s_code);
		return;
	}

	swimcu_psm_sync_select = sync_opt;
	if ((sync_opt == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A) ||
		(sync_opt == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_B))
	{
		pr_err("%s: no RTC recovery is required for sync option %d\n", __func__, sync_opt);
		return;
	}

	pr_err("%s: MCUFW elapsed PSM tme: %d\n", __func__, ulpm_time);
	if (ulpm_time == 0)
	{
		pr_err("%s: invalid PSM elapsed time: %d\n", __func__, ulpm_time);
		return;
	}

	/* converted from MCU time to MDM time (milliseconds) */
	ulpm_time *= swimcup->calibrate_mdm_time;
	ulpm_time /= swimcup->calibrate_mcu_time;

	pr_err("%s ULPM duration converted to MDM time scale: %d ms\n", __func__, ulpm_time);

	/* pre-psm rtc stored on MCU */
	tv.tv_sec = ulpm_time / MSEC_PER_SEC;
	tv.tv_nsec = (ulpm_time % MSEC_PER_SEC) * NSEC_PER_MSEC;

	pr_err("%s ULPM duration converted to MDM time scale: %lu s %lu ns\n",
		__func__, tv.tv_sec, tv.tv_nsec);

	tv.tv_sec += swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_PRE_ULPM_RTC_TIME];

	pr_err("%s updated post-PSM RTC: %lu sec\n", __func__, tv.tv_sec);

	ret = do_settimeofday(&tv);
	if (ret == 0)
	{
		pr_err("%s set post-ULPM RTC\n", __func__);
	}
	else
	{
		pr_err("%s failed to set post-ULPM RTC ret=%d\n", __func__, ret);
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
	int ret = 0;
	enum mci_protocol_status_code_e rc;
	enum mci_protocol_hw_timer_state_e timer_state;
	enum wusrc_index wi;
	struct mci_wakeup_source_config_s wu_config;
	int gpio, ext_gpio;
	int gpio_cnt = 0;
	uint32_t wu_pin_bits = 0;
	u16 wu_source = 0;
	enum swimcu_adc_index adc_wu_src = SWIMCU_ADC_INVALID;
	enum swimcu_adc_compare_mode adc_compare_mode;
	int adc_bitmask;
	uint32_t timeout;
	bool watchdog_disabled = false;

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
		swimcu_log(PM, "%s: PSM request in progress\n",__func__);
		return 0;
	}

	if (SWIMCU_ENABLE == swimcu_lpo_calibrate_enable)
	{
		pr_err("%s: MCU LPO calibration in process\n", __func__);
		return -EIO;
	}

	if ((pm == SWIMCU_PM_PSM_SYNC) || (pm == SWIMCU_PM_ULPM_FALLBACK)) {
		/* setup GPIO and ADC wakeup sources */
		for( wi = 0; wi < ARRAY_SIZE(wusrc_param); wi++ ) {
			if( wusrc_param[wi].type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS ) {
				gpio = wusrc_param[wi].id;
				if (swimcu_pm_wusrc_gpio_irq_get(gpio) != MCI_PIN_IRQ_DISABLED)
				{
					ret = swimcu_gpio_set(swimcu,
						SWIMCU_GPIO_SET_EDGE, gpio, swimcu_pm_wusrc_gpio_irq_get(gpio));
					if (ret < 0) {
						pr_err("%s: irqc set fail %d\n", __func__, gpio);
						goto wu_fail;
					}

					swimcu_log(PM, "%s: configure GPIO %d as wakeup source \n",__func__, gpio);
					wu_pin_bits |= wusrc_param[wi].mask;
					gpio_cnt++;
				}
			} else if (MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC == wusrc_param[wi].type &&
				   adc_trigger_config[wusrc_param[wi].id].select) {
					adc_wu_src = wusrc_param[wi].id;
					adc_bitmask = wusrc_param[wi].mask;
			}
		}

		if (gpio_cnt > 0) {
			wu_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS;
			wu_config.args.pins = wu_pin_bits;
			if( MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
				(rc = swimcu_wakeup_source_config(swimcu, &wu_config,
				MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET))) {
				pr_err("%s: ext pin wu fail %d\n", __func__, rc);
				ret = -EIO;
				goto wu_fail;
			}
			wu_source |= (u16)MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS;
			swimcu_log(PM, "%s: wu on pins 0x%x\n", __func__, wu_pin_bits);
		}

		if (swimcu_wakeup_time > 0)
		{
			wu_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;

			/* wakeup time has been validated upon user input (in seconds)
			*  which garantees no overflow in the following calculation.
			*/
			wu_config.args.timeout =
				swimcu_mdm_sec_to_mcu_time_ms(swimcu, swimcu_wakeup_time);

			swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_EXPECTED_ULPM_TIME] = swimcu_wakeup_time;

			if( MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
				(rc = swimcu_wakeup_source_config(swimcu, &wu_config,
				MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET)) )
			{
				pr_err("%s: timer wu fail %d\n", __func__, rc);
				ret = -EIO;
				goto wu_fail;
			}
			wu_source |= (u16)MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;
			swimcu_log(PM, "%s: wu on timer %u (mcu=%u)\n",
				__func__, swimcu_wakeup_time, wu_config.args.timeout);
		}

		if (SWIMCU_ADC_INVALID != adc_wu_src) {
			wu_config.source_type = MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC;
			wu_config.args.channel = adc_bitmask;

			swimcu_adc_set_trigger_mode(adc_wu_src,
						   MCI_PROTOCOL_ADC_TRIGGER_MODE_HW,
					           adc_interval);
			/*
			* if above > below, then trigger when value falls in the range (0, below) or (above, 1800)
			* if above <= below, trigger when value falls in the range (above, below)
			*/
			if (adc_trigger_config[adc_wu_src].above >
				adc_trigger_config[adc_wu_src].below) {
				adc_compare_mode = MCI_PROTOCOL_ADC_COMPARE_MODE_BEYOND;
			} else {
				adc_compare_mode = MCI_PROTOCOL_ADC_COMPARE_MODE_WITHIN;
			}
			swimcu_adc_set_compare_mode(adc_wu_src,
						    adc_compare_mode,
						    adc_trigger_config[adc_wu_src].above,
						    adc_trigger_config[adc_wu_src].below);
			swimcu_adc_init_and_start(swimcu, adc_wu_src);

			if (MCI_PROTOCOL_STATUS_CODE_SUCCESS !=
				(rc = swimcu_wakeup_source_config(swimcu, &wu_config,
					MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET))) {
				pr_err("%s: adc wu fail %d\n", __func__, rc);
				ret = -EIO;
				goto wu_fail;
			}
			wu_source |= (u16)MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC;
			swimcu_log(PM, "%s: wu on adc %d\n", __func__, adc_wu_src);
		}

	}

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

	if ((wu_source != 0) || (pm == SWIMCU_PM_POWER_SWITCH) || (pm == SWIMCU_PM_PSM_SYNC)) {

		if (pm == SWIMCU_PM_PSM_SYNC)
		{
			pr_info("%s: user-selected psm sync option %d\n", __func__, swimcu_psm_sync_select);

			/* select default sync option if none is specified by user */
			if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE)
			{
				swimcu_psm_sync_select = swimcu_pm_psm_sync_option_default(swimcu);
				if (swimcu_psm_sync_select == MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE)
				{
					pr_err("%s: no PSM synchronization support\n", __func__);
					ret = -EPERM;
					goto wu_fail;
				}
			}

			if (swimcu_psm_sync_select != MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A)
			{
				/* attempt to read remaining PSM time if sync option A is not specified */
				timeout = swimcu_pm_psm_time_get();

				/* save the PSM time persistent across the ULPM cycle */
				swimcu_pm_data_group[SWIMCU_PM_DATA_INDEX_EXPECTED_ULPM_TIME] = timeout;

				pr_info("%s: configured psm time %d\n", __func__, timeout);
				if (timeout > 0)
				{
					/* mitigate the risk of LPO clock variation over temperature */
					timeout *= (100 - SWIMCU_CALIBRATE_TEMPERATURE_FACTOR);
					timeout /= 100;
					pr_err("%s: at floor of tempreture variation %d\n", __func__, timeout);

					timeout = swimcu_mdm_sec_to_mcu_time_ms(swimcu, timeout);
					pr_info("%s: device calibration %d\n", __func__, timeout);
				}
				else
				{
					/* fall back to sync option A with invalid zero PSM time*/
					pr_err("%s: cannot get PSM time--fall back to option A\n", __func__);
					swimcu_psm_sync_select = MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A;
				}
			}
			else
			{
				timeout = 0;
			}

			pr_info("%s: sending psm_sync_config sync option %d max_wait %u psm time %u\n",
				__func__, swimcu_psm_sync_select, SWIMCU_PM_WAIT_SYNC_TIME, timeout);
			rc = swimcu_psm_sync_config(swimcu,
				swimcu_psm_sync_select, SWIMCU_PM_WAIT_SYNC_TIME, timeout);
			if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != rc)
			{
				pr_err("%s: cannot config MCU for PSM synchronization %d\n", __func__, rc);
				ret = -EIO;
				goto wu_fail;
			}

			if (timeout > 0)
			{
				wu_source |= MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER;
			}
			swimcu_pm_state = PM_STATE_SYNC;
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

		/* save the calibration and PSM data before power down */
		swimcu_pm_data_store(swimcu);

		pr_info("%s: sending ulpm_config", __func__);
		rc = pm_ulpm_config(swimcu, wu_source);
		if (MCI_PROTOCOL_STATUS_CODE_SUCCESS !=rc)
		{
			pr_err("%s: pm enable fail %d\n", __func__, rc);
			ret = -EIO;
			goto wu_fail;
		}

		if(PM_STATE_SYNC == swimcu_pm_state) {
			call_usermodehelper(poweroff_argv[0], poweroff_argv, NULL, UMH_NO_WAIT);
		}
	}
	else {
		pr_err("%s: no wake sources set\n", __func__);
		/* nothing to clean up in this case */
		return -EPERM;
	}
	return 0;

wu_fail:
	/* free any gpio's that have been requested */
	for( wi = 0; wi < ARRAY_SIZE(wusrc_param); wi++ ) {
		if( wusrc_param[wi].type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS ) {
			gpio = wusrc_param[wi].id;
			if (swimcu_pm_wusrc_gpio_irq_get(gpio) !=  MCI_PIN_IRQ_DISABLED) {
				/* configured for wakeup */
				ext_gpio = SWIMCU_GPIO_TO_SYS(gpio);
				gpio_free(ext_gpio);
				swimcu_log(PM, "%s: free %d\n", __func__, gpio);
			}
		}
	}

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

/* sysfs entries to set GPIO input as boot_source */
static ssize_t pm_gpio_attr_show(
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

	for (ti = ARRAY_SIZE(pin_trigger) - 1; ti > 0; ti--) {
		/* if never found we exit at ti == 0: "off" */
		if (irqc_type == pin_trigger[ti].type) {
			swimcu_log(PM, "%s: found gpio %d trigger %d\n", __func__, gpio, ti);
			break;
		}
	}
	ret = scnprintf(buf, PAGE_SIZE, pin_trigger[ti].name);

	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
};

static ssize_t pm_gpio_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	enum wusrc_index wi;
	int ti;
	int gpio;

	wi = find_wusrc_index_from_kobj(kobj);
	if ((wi != WUSRC_INVALID) &&
		(wusrc_param[wi].type == MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS)) {
		gpio = wusrc_param[wi].id;
	}
	else {
		pr_err("%s: unrecognized GPIO %s\n", __func__, kobj->name);
		return -EINVAL;
	}

	for (ti = ARRAY_SIZE(pin_trigger) - 1; ti >= 0; ti--)
	{
		/* if never found we exit at ti == -1: invalid */
		if (sysfs_streq(buf, pin_trigger[ti].name))
		{
			if (0 != swimcu_gpio_irq_support_check(gpio))
			{
				pr_err("%s: IRQ not supported on gpio%d\n", __func__, gpio);
				return -EPERM;
			}

			swimcu_pm_wusrc_gpio_irq_set(gpio, pin_trigger[ti].type);
			wusrc_value[wi].triggered = 0;

			swimcu_log(PM, "%s: setting gpio %d to trigger %d\n", __func__, gpio, ti);
			break;
		}
	}

	if(ti < 0) {
		pr_err("%s: unknown trigger %s\n", __func__, buf);
		return -EINVAL;
	}

	return count;
};

static ssize_t pm_timer_timeout_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", swimcu_wakeup_time);
}

static ssize_t pm_timer_timeout_attr_store(struct kobject *kobj,
        struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t tmp_time;

	if (0 == (ret = kstrtouint(buf, 0, &tmp_time))) {
		if (tmp_time <= SWIMCU_MAX_TIME)
		{
			swimcu_wakeup_time = tmp_time;
			wusrc_value[WUSRC_TIMER].triggered = 0;
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
	if (0 == ret) {

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

static ssize_t update_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct swimcu *swimcu = container_of(kobj, struct swimcu, pm_firmware_kobj);

	/* stop the LPO calibration before MCUFW update */
	(void)swimcu_lpo_calibrate_do_enable(swimcu, false);

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
		triggered = wusrc_value[wi].triggered;

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
		if (select && adc_trigger_config[i].select && (i != adc)) {
			pr_err("%s: cannot select more than 1 adc as boot_source", __func__);
			ret = -EPERM;
		}
	}

	if (!ret) {
		adc_trigger_config[adc].select = select;
		ret = count;
	}
	return ret;
};

static ssize_t pm_adc_interval_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", adc_interval);
}

static ssize_t pm_adc_interval_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int interval;

	if (0 == (ret = kstrtouint(buf, 0, &interval))) {
		if (interval <= SWIMCU_MAX_TIME) {
			adc_interval = interval;
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
	return scnprintf(buf, PAGE_SIZE, "%u\n", swimcu_wakeup_time);
}

static ssize_t swimcu_psm_time_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp_psm_time, ret;

	ret = kstrtouint(buf, 0, &tmp_psm_time);
	if (!ret)
	{
		swimcu_wakeup_time = tmp_psm_time;
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
		wusrc_value[wi].triggered = 1;
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
		&pm_gpio_attr_show,
		&pm_gpio_attr_store),
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
		{&wusrc_value[WUSRC_GPIO36].kobj, &swimcu->pm_boot_source_kobj, pm_gpio_edge_attr, "gpio36", ARRAY_SIZE(pm_gpio_edge_attr)},
		{&wusrc_value[WUSRC_GPIO38].kobj, &swimcu->pm_boot_source_kobj, pm_gpio_edge_attr, "gpio38", ARRAY_SIZE(pm_gpio_edge_attr)},
		{&wusrc_value[WUSRC_TIMER].kobj, &swimcu->pm_boot_source_kobj, pm_timer_timeout_attr, "timer", ARRAY_SIZE(pm_timer_timeout_attr)},
		{&wusrc_value[WUSRC_ADC2].kobj, &swimcu->pm_boot_source_adc_kobj, pm_adc_trig_attr, "adc2", ARRAY_SIZE(pm_adc_trig_attr)},
		{&wusrc_value[WUSRC_ADC3].kobj, &swimcu->pm_boot_source_adc_kobj, pm_adc_trig_attr, "adc3", ARRAY_SIZE(pm_adc_trig_attr)},
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

		/* start the calibration if its data cannot be retieved from MCU */
		if ((swimcu->calibrate_mcu_time == SWIMCU_CALIBRATE_DATA_DEFAULT) &&
			(swimcu->calibrate_mdm_time == SWIMCU_CALIBRATE_DATA_DEFAULT))
		{
			ret = swimcu_lpo_calibrate_do_enable(swimcu, true);
			if (ret)
			{
				pr_err("%s: Failed to start MCU timer calibration %d\n", __func__, ret);
			}
			else
			{
				swimcu_log(INIT, "%s: MCU LPO calibration started %d\n",
					__func__, swimcu_lpo_calibrate_mcu_time);
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

