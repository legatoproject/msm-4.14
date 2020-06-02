/*
 * swimcu-core.c  --  Device access for Sierra Wireless MCU
 *
 * adapted from:
 *
 * wm8350-core.c  --  Device access for Wolfson WM8350
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
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/gpio.h>
#include <linux/mfd/swimcu/pm.h>
#include <linux/mfd/swimcu/mciprotocol.h>
#include <linux/mfd/swimcu/mcidefs.h>
#include <linux/sierra_gpio_wake_n.h>
#include <linux/sierra_bsudefs.h>
/*
 * SWIMCU Device IO
 */
#ifdef SWIMCU_DEBUG
int swimcu_debug_mask = SWIMCU_DEFAULT_DEBUG_LOG;
#endif

#define MCI_ADC_SCALE_12_BIT    ((1 << 12) - 1)
#define MCI_ADC_SCALE_10_BIT    ((1 << 10) - 1)
#define MCI_ADC_SCALE_8_BIT     ((1 << 8 ) - 1)

#define MCI_ADC_SCALE           MCI_ADC_SCALE_12_BIT
#define MCI_ADC_RESOLUTION      MCI_PROTOCOL_ADC_RESOLUTION_12_BITS

int swimcu_fault_mask = 0;
int swimcu_fault_count = 0;

/* WPx5 ADC2 and ADC3 provided by MCU */
static const enum mci_protocol_adc_channel_e adc_chan_cfg[] = {
	[SWIMCU_ADC_PTA12] = MCI_PROTOCOL_ADC0_SE0,
	[SWIMCU_ADC_PTB1]  = MCI_PROTOCOL_ADC0_SE8
};

static struct mci_adc_config_s adc_config[] = {
	{
		.channel = MCI_PROTOCOL_ADC0_SE0,
		.resolution_mode = MCI_ADC_RESOLUTION,
		.low_power_conv  = MCI_PROTOCOL_ADC_LOW_POWER_CONV_DISABLE,
		.high_speed_conv = MCI_PROTOCOL_ADC_HIGH_SPEED_CONV_DISABLE,
		.sample_period = MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_4,
		.hw_average = true,
		.sample_count = MCI_ADC_HW_AVERAGE_SAMPLES_32,
		.trigger_mode = MCI_PROTOCOL_ADC_TRIGGER_MODE_SW,
		.trigger_type = MCI_PROTOCOL_ADC_TRIGGER_SOFTWARE,
		.trigger_interval = 0,
		.hw_compare.value1 = 0,
		.hw_compare.value2 = 0,
		.hw_compare.mode = MCI_PROTOCOL_ADC_COMPARE_MODE_DISABLED,
	},
	{
		.channel = MCI_PROTOCOL_ADC0_SE8,
		.resolution_mode = MCI_ADC_RESOLUTION,
		.low_power_conv  = MCI_PROTOCOL_ADC_LOW_POWER_CONV_DISABLE,
		.high_speed_conv = MCI_PROTOCOL_ADC_HIGH_SPEED_CONV_DISABLE,
		.sample_period = MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_4,
		.hw_average = true,
		.sample_count = MCI_ADC_HW_AVERAGE_SAMPLES_32,
		.trigger_mode = MCI_PROTOCOL_ADC_TRIGGER_MODE_SW,
		.trigger_type = MCI_PROTOCOL_ADC_TRIGGER_SOFTWARE,
		.trigger_interval = 0,
		.hw_compare.value1 = 0,
		.hw_compare.value2 = 0,
		.hw_compare.mode = MCI_PROTOCOL_ADC_COMPARE_MODE_DISABLED,
	},
};
/************
*
* Name:     swimcu_get_adc_from_chan
*
* Purpose:  get swimcu adc index from mci protocol adc channel
*
* Parms:    chan - adc channel
*
* Return:   adc index corresponding to channel
*
* Abort:    none
*
************/
int swimcu_get_adc_from_chan(int channel)
{
	enum swimcu_adc_index adc;

	for (adc = SWIMCU_ADC_FIRST; adc < SWIMCU_NUM_ADC; adc++) {
		if (channel == adc_chan_cfg[adc]) {
			break;
		}
	}
	return adc;
}

/************
*
* Name:     swimcu_set_fault_mask
*
* Purpose:  Register a MCU fault event
*
* Parms:    fault - fault bit mask
*
* Return:   Nothing
*
* Abort:    none
*
************/
void swimcu_set_fault_mask(int fault)
{
	if (swimcu_fault_mask == 0) /* first time, reset counter */
		swimcu_fault_count = 0;
	swimcu_fault_mask |= fault;
	if (swimcu_fault_count < SWIMCU_FAULT_COUNT_MAX) {
		swimcu_fault_count++;
		swimcu_log(INIT, "%s: 0x%x, cnt %d\n", __func__, fault, swimcu_fault_count);
	}
}

/************
 *
 * Name:     swimcu_adc_set_trigger_mode
 *
 * Purpose:  set the trigger mode for a given ADC channel
 *
 * Parms:    adc       - 0 or 1
 *           trigger   - 0 for software trigger, 1 for hardware timer trigger
 *           interval  - time in ms between adc readings, only used when trigger is hardware
 *
 * Return:   none
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
int swimcu_adc_set_trigger_mode (
	enum swimcu_adc_index adc,
	int trigger, int interval)
{
	int ret = -EINVAL;

	if (interval < SWIMCU_ADC_INTERVAL_MAX) {
		adc_config[adc].trigger_mode = trigger;

		if (MCI_PROTOCOL_ADC_TRIGGER_MODE_HW == trigger) {
			adc_config[adc].trigger_type =
				MCI_PROTOCOL_ADC_TRIGGER_LPTMR0;
			adc_config[adc].trigger_interval = interval;
		}
		else {
			adc_config[adc].trigger_type =
				MCI_PROTOCOL_ADC_TRIGGER_SOFTWARE;
		}
		ret = 0;
	}
	return ret;
}

/************
 *
 * Name:     swimcu_adc_set_compare_mode
 *
 * Purpose:  setup the ADC hardware comparator
 *
 * Parms:    adc           - 0 or 1
 *           mode          - compare function mode
 *           compare_val1  - 1st trigger value in mV
 *           compare_val2  - 2nd trigger value, only used
 *                           for WITHIN and BEYOND compare modes
 *
 * Return:   0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:
 *
 ************/
int swimcu_adc_set_compare_mode (
	enum swimcu_adc_index adc,
	enum swimcu_adc_compare_mode mode,
	unsigned compare_val1,
	unsigned compare_val2)
{
	int cv1;
	int cv2;

	if (compare_val1 > SWIMCU_ADC_VREF ||
		compare_val2 > SWIMCU_ADC_VREF) {
		return -EINVAL;
	}

	compare_val1 = (compare_val1 * MCI_ADC_SCALE) / SWIMCU_ADC_VREF;
	compare_val2 = (compare_val2 * MCI_ADC_SCALE) / SWIMCU_ADC_VREF;

	if (SWIMCU_ADC_COMPARE_WITHIN == mode ||
		SWIMCU_ADC_COMPARE_BEYOND == mode) {
		/* ADC CV1 register must always be the
		* greatest value for range comparison */
		cv1 = (compare_val1 > compare_val2) ? compare_val1 : compare_val2;
		cv2 = (cv1 == compare_val1) ? compare_val2 : compare_val1;

		adc_config[adc].hw_compare.value1 = cv1;
		adc_config[adc].hw_compare.value2 = cv2;
	} else {
		/* CV2 is ignored for non-range compare functions */
		adc_config[adc].hw_compare.value1 = compare_val1;
	}

	adc_config[adc].hw_compare.mode = mode;
	return 0;
}

/************
 *
 * Name:     swimcu_adc_init_and_start
 *
 * Purpose:  Initialize MCU adc
 *
 * Parms:    swimcu - driver data block
 *           adc - a valid adc channel index
 *
 * Return:    0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called on startup or if adc read fails
 *
 ************/
int swimcu_adc_init_and_start(struct swimcu *swimcu, enum swimcu_adc_index adc)
{
	int rcode = 0;
	int adc_mask;

	if (adc < ARRAY_SIZE(adc_chan_cfg)) {
		adc_mask = (1 << adc);
		/* Init command automatically starts the ADC */
		if (MCI_PROTOCOL_STATUS_CODE_SUCCESS ==
			swimcu_adc_init(swimcu, &adc_config[adc])) {
			swimcu->adc_init_mask |= adc_mask;
		}
		else {
			swimcu->adc_init_mask &= ~adc_mask;
			pr_err("%s: fail chan %d\n", __func__, adc);
			rcode = -EIO;
		}
	}
	return rcode;
}

/************
 *
 * Name:     reset_recovery
 *
 * Purpose:  refresh MCU configuration
 *
 * Parms:    swimcu - driver data block
 *
 * Return:   nothing
 *
 * Abort:    if we hit the max fault_count we suspend recovery.
 *           Chances are that is what is causing the fault.
 *
 * Notes:    Called on reset event from MCU.
 *           This is most likely to occur after
 *           a MCU firmware update.
 *
 ************/
static void reset_recovery( struct swimcu *swimcu )
{
	swimcu->adc_init_mask = 0;   /* re-init ADC before next access */
	if (swimcu_fault_count < SWIMCU_FAULT_COUNT_MAX) {
		swimcu_device_init(swimcu);
		swimcu_gpio_refresh(swimcu); /* restore MCU with last gpio config */
		swimcu_set_fault_mask(SWIMCU_FAULT_RESET);
		swimcu_log(INIT, "swimcu %s: complete\n", __func__);
	}
	else {
		swimcu_log(INIT, "swimcu %s: suspended\n", __func__);
	}
}

/************
 *
 * Name:     swimcu_read_adc
 *
 * Purpose:  Read ADC value from MCU
 *
 * Parms:    swimcu - driver data block
 *           channel - 0 or 1
 *
 * Return:   >=0 ADC value in mV if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from hwmon driver
 *
 ************/
int swimcu_read_adc( struct swimcu *swimcu, int channel )
{
	uint16_t adc_val = 0;
	int rcode;
	enum mci_protocol_adc_channel_e adc_chan;
	enum mci_protocol_status_code_e ret;
	int adc_mask;

	if (channel >= ARRAY_SIZE(adc_chan_cfg)) {
		pr_err("%s: invalid chan %d\n", __func__, channel);
		return -EPERM;
	}
	adc_mask = (1 << channel);

	mutex_lock(&swimcu->adc_mutex);
	if (0 == (swimcu->adc_init_mask & adc_mask)) {
		if (swimcu_adc_init_and_start(swimcu, channel)) {
			pr_err("%s: fail to init chan %d\n", __func__, channel);
			rcode = -EIO;
			goto read_adc_exit;
		}
	}

	adc_chan = adc_chan_cfg[channel];

	swimcu_log(ADC, "%s: channel %d\n", __func__, channel);

	/* start ADC sample */
	ret = swimcu_adc_restart(swimcu, adc_chan);

	if (ret != MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		pr_warn("%s restart failed on chan %d, try init\n", __func__, adc_chan);
		if (swimcu_adc_init_and_start(swimcu, channel)) {
			pr_err("%s: fail to init chan %d\n", __func__, channel);
			rcode = -EIO;
			goto read_adc_exit;
		}
	}

	/* convert ADC value to mV */
	if (swimcu_adc_get(swimcu, adc_chan, &adc_val) == MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
		rcode = (int)((adc_val * SWIMCU_ADC_VREF) >>
				(adc_config[channel].resolution_mode == MCI_PROTOCOL_ADC_RESOLUTION_8_BITS ? 8 : 12));
	}
	else {
		rcode = -EIO;
		pr_warn("%s adc read failed on chan %d\n", __func__, adc_chan);
	}

read_adc_exit:
	mutex_unlock(&swimcu->adc_mutex);

	return rcode;
}
EXPORT_SYMBOL_GPL(swimcu_read_adc);

/************
 *
 * Name:     swimcu_process_events
 *
 * Purpose:  To process events from MCU
 *
 * Parms:    swimcu - driver data block
 *
 * Return:    0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    if >50 events or MCU event query fails
 *
 * Notes:    typically called after trigger from active low MICRO_IRQ_N
 *           Events are retrieved in blocks of 5. We keep retrieving
 *           blocks until we have all the events. If after 50 events we
 *           still have more to retrieve we consider this an error and stop
 *           so we're not stuck here forever. There is no known condition
 *           currently that would trigger >5 concurrent events, much less 50.
 *
 ************/
static int swimcu_process_events(struct swimcu *swimcu)
{
	enum mci_protocol_status_code_e p_code;

	struct mci_event_s events[MCI_EVENT_LIST_SIZE_MAX];
	int count = MCI_EVENT_LIST_SIZE_MAX;
	int query_count = 1;
	int i;

	do {
		memset(events, 0, sizeof(events));
		count = MCI_EVENT_LIST_SIZE_MAX;
		p_code = swimcu_event_query(swimcu, events, &count);
		swimcu_log(EVENT, "%s: %d events, query %d\n", __func__, count, query_count);

		if (p_code != MCI_PROTOCOL_STATUS_CODE_SUCCESS) {
			return -EIO;
		}
		/* handle the events */
		for (i = 0; i < count; i++) {
			if (events[i].type == MCI_PROTOCOL_EVENT_TYPE_GPIO) {
				swimcu_log(EVENT, "%s: GPIO callback for port %d pin %d value %d\n", __func__,
					events[i].data.gpio_irq.port, events[i].data.gpio_irq.pin, events[i].data.gpio_irq.level);
				swimcu_gpio_callback(swimcu, events[i].data.gpio_irq.port, events[i].data.gpio_irq.pin, events[i].data.gpio_irq.level);
			}
			else if (events[i].type == MCI_PROTOCOL_EVENT_TYPE_ADC) {
				swimcu_log(EVENT, "%s: ADC completed callback for channel %d: value=%d\n", __func__,
					events[i].data.adc.adch, events[i].data.adc.value);
			}
			else if (events[i].type == MCI_PROTOCOL_EVENT_TYPE_RESET) {
				swimcu_log(EVENT, "%s: MCU reset source 0x%x\n", __func__, events[i].data.reset.source);
				reset_recovery(swimcu);
				swimcu_set_reset_source(events[i].data.reset.source);
			}
			else if (events[i].type == MCI_PROTOCOL_EVENT_TYPE_WUSRC) {
				swimcu_log(EVENT, "%s: MCU wakeup source %d 0x%x\n", __func__, events[i].data.wusrc.type, events[i].data.wusrc.value);
				swimcu_set_wakeup_source(events[i].data.wusrc.type, events[i].data.wusrc.value);
			}
			else {
				pr_warn("%s: Unknown event[%d] type %d\n", __func__, i, events[i].type);
			}
		}

		if (count < MCI_EVENT_LIST_SIZE_MAX) {
			/* All events retrieved. Done. */
			break;
		}
		else {
			/* Possibly more events to retrieve */
			if (++query_count > 10) {
				/* MCU fault. MAX is arbitrary, but there's no known reason to exceed 1 */
				pr_err("%s: query max exceeded, %d\n", __func__, query_count);
				swimcu_set_fault_mask(SWIMCU_FAULT_EVENT_OFLOW);
				return -EREMOTEIO;
			}
			/* else retrieve another block of events */
		}
	} while (true);

	return 0;
}

/************
 *
 * Name:     swimcu_event_trigger
 *
 * Purpose:  notification callback on wake event
 *
 * Parms:    notifier_block - to retrieve swimcu driver data
 *           others unused
 *
 * Return:    0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from sierra_gpio_wake_n
 *
 ************/
static int swimcu_event_trigger (
	struct notifier_block *self,
	unsigned long event, void *unused)
{
	struct swimcu *swimcu = container_of(self, struct swimcu, nb);

	return swimcu_process_events(swimcu);
}

/************
 *
 * Name:     swimcu_event_init
 *
 * Purpose:  register notify block with sierra_gpio_wake_n driver
 *
 * Parms:    swimcu - contains the notifier block data
 *
 * Return:   nothing
 *
 * Abort:    none
 *
 * Notes:    called on device init (early)
 *
 */
static void swimcu_event_init (struct swimcu *swimcu)
{
	swimcu->nb.notifier_call = swimcu_event_trigger;
	sierra_gpio_wake_notifier_register(&swimcu->nb);
}

/************
 *
 * Name:     swimcu_client_dev_register
 *
 * Purpose:  Register a client device
 *
 * Parms:    swimcu - device block data
 *           name - device name
 *           pdev - device pointer
 *
 * Return:    0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    This is non-fatal since there is no need to fail the entire
 *           device init due to a single platform device failing.
 *
 */
static int swimcu_client_dev_register(
	struct swimcu *swimcu,
	const char *name,
	struct platform_device **pdev)
{
	int ret;

	*pdev = platform_device_alloc(name, -1);
	if (*pdev == NULL) {
		dev_err(swimcu->dev, "Failed to allocate %s\n", name);
		ret = -ENOMEM;
	}
	else {
		(*pdev)->dev.parent = swimcu->dev;
		platform_set_drvdata(*pdev, swimcu);
		ret = platform_device_add(*pdev);
		if (ret != 0) {
			dev_err(swimcu->dev, "Failed to register %s: %d\n", name, ret);
			platform_device_put(*pdev);
			*pdev = NULL;
		}
	}
	return ret;
}

/************
 *
 * Name:     swimcu_device_exit
 *
 * Purpose:  deinitialize the driver
 *
 * Parms:    swimcu - device data
 *
 * Return:   Nothing
 *
 * Abort:    none
 *
 * Notes:    called on device exit or init fail
 *
 */
void swimcu_device_exit(struct swimcu *swimcu)
{
	sierra_gpio_wake_notifier_unregister(&swimcu->nb);
	swimcu_pm_sysfs_deinit(swimcu);
	platform_device_unregister(swimcu->hwmon.pdev);
	platform_device_unregister(swimcu->gpio.pdev);
	swimcu_log(INIT, "%s\n", __func__);
}
EXPORT_SYMBOL_GPL(swimcu_device_exit);

/************
 *
 * Name:     swimcu_vbus_detect_disable
 *
 * Purpose:  Disable USB VBUS Detection
 *
 * Parms:    swimcu - pointer to device data structure
 *
 * Return:   Nothing
 *
 * Abort:    none
 *
 * Notes:    On WP76 DV5.2 HW, the VBUS input to MCU is floating, which
 *           may cause MCU to signal to MDM "USB not present", preventing
 *           USB enumeration.
 *
 *           By disabling the VBUS detect mechanism on MCU, the external
 *           pull-up on MCU output pull the singal to high, indicating to
 *           MDM the USB is always connected.
 */
static void swimcu_vbus_detect_disable(
	struct swimcu *swimcu)
{
	struct mci_mcu_pin_state_s pin_state = {
		.mux = MCI_MCU_PIN_FUNCTION_DISABLED,
		.dir = MCI_MCU_PIN_DIRECTION_INPUT,
		.level = MCI_MCU_PIN_LEVEL_LOW,
		.params.input.pe = false,
		.params.input.ps = MCI_MCU_PIN_PULL_UP,
		.params.input.pfe = false,
		.params.input.irqc_type = MCI_PIN_IRQ_DISABLED,
	};
	(void) swimcu_pin_config_set(swimcu, 0, 4, &pin_state);
	(void) swimcu_pin_config_set(swimcu, 0, 7, &pin_state);
}

/************
 * Name:     swimcu_device_init
 *
 * Purpose:  initialize the driver
 *
 * Parms:    swimcu - contains the pdata for init
 *
 * Return:    0 if successful
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called on device init (early)
 *
 */
int swimcu_device_init(struct swimcu *swimcu)
{
	int ret;
	struct swimcu_platform_data *pdata = dev_get_platdata(swimcu->dev);
	enum bshwtype hwtype;
	uint8_t hwrev;

	if (NULL == pdata) {
		ret = -EINVAL;
		pr_err("%s: no pdata, aborting\n", __func__);
		return ret;
	}
	swimcu_log(INIT, "%s: start 0x%x\n", __func__, swimcu->driver_init_mask);

	if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_EVENT)) {
		mutex_init(&swimcu->mcu_transaction_mutex);
		swimcu_event_init(swimcu);
		swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_EVENT;
	}

	/* Even though the MCU may not be functional now, we must still create the sysfs
	hwmon entry here to ensure it is always at hwmon1.
	Otherwise, if the MCU comes up later, it would be put at the end, at hwmon7 */
	swimcu->adc_init_mask = 0;
	if(pdata->nr_adc > 0) {
		if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_ADC)) {
			mutex_init(&swimcu->adc_mutex);

			if (pdata->nr_adc > SWIMCU_NUM_ADC)
				pdata->nr_adc = SWIMCU_NUM_ADC;

			ret = swimcu_client_dev_register(swimcu, "swimcu-hwmon",
				&(swimcu->hwmon.pdev));
			if (ret != 0) {
				dev_err(swimcu->dev, "hwmon client register failed: %d\n", ret);
				ret = 0; /* non-fatal */
			}
			else {
				swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_ADC;
			}
		}
	}

	if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != swimcu_ping(swimcu)) {
		swimcu_log(INIT, "%s: no response, aborting\n", __func__);
		ret = 0; /* this is not necessarily an error. MCI firmware update procedure will take over */
		goto exit;
	}

	swimcu_log(INIT, "%s: mcufw ver=%d.%03d target=%d opt=0x%X\n", __func__,
			swimcu->version_major, swimcu->version_minor,
			swimcu->target_dev_id, swimcu->opt_func_mask);

	if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_PING)) {
		/* first communication with MCU since statup */
		swimcu_gpio_retrieve(swimcu); /* get gpio config from MCU */
	}

	swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_PING;

	/* Disable MCU VBUS detect mechanism on MCU for WP76 DV5.2 HW. */
	hwtype = bs_hwtype_get();
	hwrev = bs_hwrev_get();
	if ((hwrev == BS_HW_ID_DV_5_2) &&
		((hwtype == BSWP7601) || (hwtype == BSWP7601_1) ||
		(hwtype == BSWP7603) || (hwtype == BSWP7603_1)))
	{
		swimcu_log(INIT, "%s: Disable MCU USB VBUS Detection for DV5.2\n", __func__);
		swimcu_vbus_detect_disable(swimcu);
	}

	if (0 != swimcu_pm_sysfs_opt_update(swimcu))
	{
		dev_err(swimcu->dev, "Cannot update optional sysfs\n");
		goto exit;
	}

	if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_FW)) {
		if(pdata->func_flags & SWIMCU_FUNC_FLAG_FWUPD) {
			ret = swimcu_pm_sysfs_init(swimcu, SWIMCU_FUNC_FLAG_FWUPD);
			if (ret != 0) {
				dev_err(swimcu->dev, "FW sysfs init failed: %d\n", ret);
				goto exit;
			}
		}
		swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_FW;
	}

	if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_PM)) {
		if(pdata->func_flags & SWIMCU_FUNC_FLAG_PM) {
			ret = swimcu_pm_sysfs_init(swimcu, SWIMCU_FUNC_FLAG_PM);
			if (ret != 0) {
				dev_err(swimcu->dev, "PM sysfs init failed: %d\n", ret);
				goto exit;
			}
		}
		swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_PM;
	}

	if(pdata->nr_gpio > 0) {
		if (!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_GPIO)) {
			ret = swimcu_client_dev_register(swimcu, "swimcu-gpio",
				&(swimcu->gpio.pdev));
			if (ret != 0) {
				dev_err(swimcu->dev, "gpio client register failed: %d\n", ret);
				ret = 0; /* non-fatal */
			}
			else {
				swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_GPIO;
			}
		}
	}

	if(!(swimcu->driver_init_mask & SWIMCU_DRIVER_INIT_REBOOT)) {
		swimcu->reboot_nb.notifier_call = pm_reboot_call;
		ret = register_reboot_notifier(&(swimcu->reboot_nb));
		if (ret) {
			pr_err("%s: Failed to register reboot notifier\n", __func__);
			goto exit;
		}
		swimcu->driver_init_mask |= SWIMCU_DRIVER_INIT_REBOOT;
	}

	if(pdata->func_flags & SWIMCU_FUNC_FLAG_EVENT) {
		ret = swimcu_process_events(swimcu);
		if (ret != 0) {
			dev_err(swimcu->dev, "process events failed: %d\n", ret);
			goto exit;
		}
	}

	swimcu_log(INIT, "%s: success 0x%x\n", __func__, swimcu->driver_init_mask);
	return 0;

exit:
	swimcu_log(INIT, "%s: abort 0x%x\n", __func__, swimcu->driver_init_mask);
	return ret;
}
EXPORT_SYMBOL_GPL(swimcu_device_init);

MODULE_DESCRIPTION("Sierra Wireless MCU core driver");
MODULE_LICENSE("GPL");
