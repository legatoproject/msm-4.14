/*
 * swimcu-gpio.c  --  Device access for Sierra Wireless MCU GPIO.
 *
 * adapted from:
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
#include <linux/errno.h>
#include <linux/string.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/gpio.h>
#include <linux/mfd/swimcu/mciprotocol.h>
#include <linux/mfd/swimcu/mcidefs.h>

/* Local cache for MCU GPIO pin configuration */
static struct mci_mcu_pin_state_s swimcu_gpio_cfg[SWIMCU_NUM_GPIO] = {0};

/* Mutexes to protect swimcu_gpio_cfg cache consistency */
static struct mutex swimcu_gpio_cfg_mutex[SWIMCU_NUM_GPIO];

/* local gpio indexed mapping table of MCU port, pin and IRQ support info */
static const struct {
	int port;
	int pin;
	enum swimcu_gpio_irq_index irq;
} swimcu_gpio_map[SWIMCU_NUM_GPIO] = {
	{ 0, 0, SWIMCU_GPIO_PTA0_IRQ}, /* GPIO 36 = PTA0, with IRQ support */
	{ 0, 2, SWIMCU_GPIO_NO_IRQ},   /* GPIO 37 = PTA2 */
	{ 1, 0, SWIMCU_GPIO_PTB0_IRQ}, /* GPIO 38 = PTB0, with IRQ support */
	{ 0, 6, SWIMCU_GPIO_NO_IRQ},   /* GPIO 40 = PTA6 */
	{ 0, 5, SWIMCU_GPIO_NO_IRQ},   /* GPIO 41 = PTA5 */
};

/* MCU disables an IRQ after it is triggerred. If a READ operation occurs before
*  the IRQ event is delivered and handled, swimcu_gpio_cfg cache will be updated
*  with IRQ type as DISABLED. swimcu_gpio_irq_cfg saves the user configuration
*  of IRQ type, which gurantees the MCU GPIO configuration can be restored with
*  the latest IRQ type configured by user after the IRQ event is successfully
*  processed.
*/
static struct swimcu_gpio_irq_cfg gpio_irq_cfg[SWIMCU_NUM_GPIO_IRQ] = {0};

/* Pointer for the GPIO IRQ handler provided by the user (GPIO-SWIMCU) */
bool (*swimcu_gpio_irqp)(struct swimcu*, enum swimcu_gpio_irq_index) = NULL;

/************
 *
 * Name:     swimcu_get_gpio_from_irq
 *
 * Purpose:  return index into swimcu_gpio_map given irq number
 *
 * Parms:    irq - the valid irq number
 *
 * Return:   gpio - a valid gpio index on success
 *           SWIMCU_GPIO_INVALID (SWIMCU_NUM_GPIO) on failure
 *
 * Abort:    none
 *
 ************/
enum swimcu_gpio_index swimcu_get_gpio_from_irq(enum swimcu_gpio_irq_index irq)
{
	enum swimcu_gpio_index gpio;

	for (gpio = SWIMCU_GPIO_FIRST; gpio < SWIMCU_NUM_GPIO; gpio++) {
		if (irq == swimcu_gpio_map[gpio].irq) {
			break;
		}
	}
	return gpio;
}

/************
 *
 * Name:     swimcu_get_irq_from_gpio
 *
 * Purpose:  return irq number given gpio index
 *
 * Parms:    gpio - the valid gpio index
 *
 * Return:   irq - the valid irq on success
 *           -1 if pin does not support interrupts
 *
 * Abort:    none
 *
 ************/
inline enum swimcu_gpio_irq_index swimcu_get_irq_from_gpio(
	enum swimcu_gpio_index gpio)
{
	return swimcu_gpio_map[gpio].irq;
}

/************
 *
 * Name:     swimcu_get_gpio_from_port_pin
 *
 * Purpose:  return index into swimcu_gpio_map given port and pin
 *
 * Parms:    port - 0 or 1 corresponding to PTA or PTB
 *           pin - 0 to 7 for pin # on that port
 *
 * Return:   gpio - a valid gpio index on success
 *           SWIMCU_GPIO_INVALID (SWIMCU_NUM_GPIO) on failure
 *
 * Abort:    none
 *
 ************/
enum swimcu_gpio_index swimcu_get_gpio_from_port_pin(int port, int pin)
{
	enum swimcu_gpio_index gpio;

	for (gpio = SWIMCU_GPIO_FIRST; gpio < SWIMCU_NUM_GPIO; gpio++) {
		if((port == swimcu_gpio_map[gpio].port) &&
			(pin == swimcu_gpio_map[gpio].pin))
			break;
	}
	return gpio;
}

/************
 *
 * Name:     swimcu_gpio_irq_support_check
 *
 * Purpose:  check whether IRQ is supported for a particular gpio
 *
 * Parms:    gpio - a valid gpio index
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Note:     Called with the GPIO configuration cache locked.
 *
 * Abort:    none
 *
 ************/
int swimcu_gpio_irq_support_check(enum swimcu_gpio_index gpio)
{
	if ((gpio < SWIMCU_GPIO_FIRST) || (gpio > SWIMCU_GPIO_LAST))
	{
		pr_err("%s: GPIO %d not supported\n", __func__, gpio);
		return -EINVAL;
	}

	if (swimcu_gpio_map[gpio].irq == SWIMCU_GPIO_NO_IRQ)
	{
		pr_err("%s: GPIO %d does not support IRQ\n", __func__, gpio);
		return -EPERM;
	}

	if (swimcu_gpio_cfg[gpio].dir != MCI_MCU_PIN_DIRECTION_INPUT)
	{
		pr_err("%s: GPIO %d not configured as input\n", __func__, gpio);
		return -EPERM;
	}

	return 0;
}

/************
 *
 * Name:     swimcu_gpio_irq_cfg_set
 *
 * Purpose:  set the irq trigger for a particular gpio
 *
 * Parms:    irq  - SWIMCU GPIO IRQ index
 *           irq_cfg - MCU IRQ config (enable/disable, rising, falling, etc)
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Note:     Caller responsible for mapping the external interrupt type to
 *           MCU IRQ control type.
 *
 * Abort:    none
 *
 ************/
int swimcu_gpio_irq_cfg_set(enum swimcu_gpio_irq_index irq,
		struct swimcu_gpio_irq_cfg *irq_cfg)
{
	if(!irq_cfg)
	{
		pr_err("%s: irq_cfg null",__func__);
		return -EINVAL;
	}

	if (irq <= SWIMCU_GPIO_NO_IRQ && irq >= SWIMCU_NUM_GPIO_IRQ)
	{
		pr_err("%s: Invalid IRQ %d\n", __func__, irq);
		return -EPERM;
	}

	gpio_irq_cfg[irq] = *irq_cfg;
	return 0;
}

/************
 *
 * Name:     swimcu_gpio_irq_cfg_get
 *
 * Purpose:  get the irq trigger for a particular gpio
 *
 * Parms:    irq - GPIO IRQ index
 *           irq_cfg - IRQ config (enable/disable, rising, falling, etc)
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 ************/
int swimcu_gpio_irq_cfg_get(enum swimcu_gpio_irq_index irq,
		struct swimcu_gpio_irq_cfg *irq_cfg)
{
	int ret = -EINVAL;

	if(!irq_cfg)
	{
		pr_err("%s: irq_cfg null",__func__);
		return ret;
	}

	if ((irq > SWIMCU_GPIO_NO_IRQ || irq < SWIMCU_NUM_GPIO_IRQ))
	{
		*irq_cfg = gpio_irq_cfg[irq];
		ret = 0;
	}

	return ret;
}

/************
 *
 * Name:     _swimcu_gpio_get (internal use)
 *           swimcu_gpio_get  (external interface wrapper)
 *
 * Purpose:  get the gpio property
 *
 * Parms:    swimcu - pointer to the device data struct
 *           action - get specific attribute from a MCU GPIO pin.
 *           gpio   - a valid gpio index
 *           valuep - pointer to storage for the returns value
 *        For _swimcu_gpio_get() only
 *           caller_locked - Boolean flag indicates caller has locked cache
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from gpio driver
 *
 ************/
static int _swimcu_gpio_get(
	struct swimcu *swimcu,
	int action,
	int gpio,
	int *valuep,
	bool caller_locked)
{
	enum mci_protocol_status_code_e s_code;

	if (gpio > SWIMCU_GPIO_LAST)
	{
		pr_err("%s: invalid gpio %d \n",__func__, gpio);
		return -EINVAL;
	}

	if (!valuep && action != SWIMCU_GPIO_NOOP)
	{
		pr_err ("%s: no storage for returned value\n", __func__);
		return -EINVAL;
	}

	/* validate the action */
	switch (action)
	{
		case SWIMCU_GPIO_GET_EDGE:

			if (swimcu_gpio_map[gpio].irq == SWIMCU_GPIO_NO_IRQ)
			{
				pr_err("%s: unsupported action %d \n",__func__, action);
				return -EINVAL;
			}
			break;

		case SWIMCU_GPIO_GET_DIR:  /* use cached value; do nothing */

			break;

		case SWIMCU_GPIO_GET_PULL:   /* use cached value; do nothing */

			break;
		case SWIMCU_GPIO_GET_VAL:   /* use current value on MCU */
		case SWIMCU_GPIO_NOOP:      /* refresh the cached states from MCU */

			/* retrieve current state of the MCU pin */
			if (!caller_locked)
			{
				mutex_lock(&swimcu_gpio_cfg_mutex[gpio]);
			}
			s_code = swimcu_pin_states_get(swimcu,
			swimcu_gpio_map[gpio].port, swimcu_gpio_map[gpio].pin, &(swimcu_gpio_cfg[gpio]));
			if (!caller_locked)
			{
				mutex_unlock(&swimcu_gpio_cfg_mutex[gpio]);
			}

			if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code)
			{
				pr_err("%s: failed to access MCU gpio %d (status=%d)\n",__func__, gpio, s_code);
				return -EIO;
			}
			break;

		default:
			pr_err("%s: unsupported action %d \n",__func__, action);
			return -EINVAL;
	}

	/* retrive the value of a specific attribute */
	switch (action)
	{
		case SWIMCU_GPIO_GET_DIR:

			*valuep = (swimcu_gpio_cfg[gpio].dir == MCI_MCU_PIN_DIRECTION_INPUT) ? 0 : 1;
			break;

		case SWIMCU_GPIO_GET_VAL:

			*valuep = (swimcu_gpio_cfg[gpio].level == MCI_MCU_PIN_LEVEL_LOW) ? 0 : 1;
			break;

		case SWIMCU_GPIO_GET_PULL:

  			if (swimcu_gpio_cfg[gpio].dir != MCI_MCU_PIN_DIRECTION_INPUT)
			{
				pr_err ("%s: illegal operation to get PULL for output pin %d\n", __func__, gpio);
				return -EPERM;
			}

			if (!swimcu_gpio_cfg[gpio].params.input.pe)
			{
				*valuep = MCI_MCU_PIN_PULL_NONE;
			}
			else
			{
				*valuep = (swimcu_gpio_cfg[gpio].params.input.ps == MCI_MCU_PIN_PULL_DOWN) ? 0 : 1;
			}
			break;

		case SWIMCU_GPIO_GET_EDGE:

			*valuep = swimcu_gpio_cfg[gpio].params.input.irqc_type;
			break;

		case SWIMCU_GPIO_NOOP:    /* do nothing */

			break;

		default:

			return -EPERM;
	}

	return 0;
}

/* External interface wrapper */
int swimcu_gpio_get(struct swimcu *swimcu, int action, int gpio, int *valuep)
{
	return _swimcu_gpio_get(swimcu, action, gpio, valuep, false);
}

/************
 *
 * Name:     _swimcu_gpio_set (for internal use only)
 *           swimcu_gpio_set  (external interface wrapper)
 *
 * Purpose:  set the gpio property
 *
 * Parms:    swimcu - device data struct
 *           action - property to set
 *           gpio   - a valid gpio index
 *           value  - value to set (depends on action)
 *        For _swimcu_gpio_set() only:
 *           caller_locked - Boolean flag indicates caller has locked cache
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from gpio driver
 *
 ************/
static int _swimcu_gpio_set(struct swimcu *swimcu,
	int action,
	int gpio,
	int value,
	bool caller_locked)
{
	enum mci_protocol_status_code_e s_code;
	struct mci_mcu_pin_state_s *pin_statep;
	struct mci_mcu_pin_state_s backup_pin_state;
	enum mci_mcu_pin_direction_e dir;
	enum mci_mcu_pin_level_e level;
	bool config_changed = false;
	int ret;

	swimcu_log(GPIO, "%s: gpio=%d, action=%d, value=%d\n", __func__, action, gpio, value);

	if ((gpio < SWIMCU_GPIO_FIRST) || (gpio > SWIMCU_GPIO_LAST))
	{
		pr_err("%s: Invalid GPIO %d\n", __func__, gpio);
		return -EINVAL;
	}

	pin_statep = &(swimcu_gpio_cfg[gpio]);

	/* configure the gpio regardless of the pin export status in sysfs */
	if (action != SWIMCU_GPIO_NOOP && pin_statep->mux != MCI_MCU_PIN_FUNCTION_GPIO)
        {
		pr_info("%s: setting MCU pin as GPIO %d\n", __func__, gpio);
		ret = swimcu_gpio_open(swimcu, gpio);
		if (ret)
		{
			pr_err("%s: failed to set MCU pin as GPIO %d err=%d\n", __func__, gpio, ret);
			return ret;
		}
	}

	/* Lock the cached config as attributes may change in the following process.
	*  If attributes change, make a backup copy for recovery in case of failure,
	*  Operations are performed with the cached copy on the bet that the operations
	*  will success most probably.
	*/
	if (!caller_locked)
	{
		mutex_lock(&swimcu_gpio_cfg_mutex[gpio]);
	}
	ret = 0;
	switch (action)
	{
		case SWIMCU_GPIO_SET_DIR:

			swimcu_log(GPIO, "%s: SET DIR %d, port %d, pin%d\n",
				__func__, value, swimcu_gpio_map[gpio].port, swimcu_gpio_map[gpio].pin);

			/* validate input: value: 0-input, 1-output,low, 2-output,high */
			if (value == 0)
			{
				dir = MCI_MCU_PIN_DIRECTION_INPUT;
				level = MCI_MCU_PIN_LEVEL_LOW;
			}
			else if (value == 1)
			{
				dir = MCI_MCU_PIN_DIRECTION_OUTPUT;
				level = MCI_MCU_PIN_LEVEL_LOW;
			}
			else if (value == 2)
			{
				dir = MCI_MCU_PIN_DIRECTION_OUTPUT;
				level = MCI_MCU_PIN_LEVEL_HIGH;
			}
			else
			{
				pr_err("%s: invalid input/output value %d (0~2))\n", __func__, value);
				ret = -EINVAL;
				break;
			}

			if (pin_statep->dir != dir)
			{
				swimcu_log(GPIO, "%s: configure changed: dir %d to %d\n", __func__, pin_statep->dir, dir);
				config_changed = true;
			}
			else if ((pin_statep->dir == MCI_MCU_PIN_DIRECTION_OUTPUT) &&
				(pin_statep->level != level))
			{
				config_changed = true;
				swimcu_log(GPIO, "%s: configure changed: ouput level %d to %d\n", __func__, pin_statep->level, level);
			}

			if (config_changed)
			{
				backup_pin_state = swimcu_gpio_cfg[gpio];
				pin_statep->dir = dir;
				pin_statep->level = level;
			}
			else
			{
				swimcu_log(GPIO, "%s: no change DIR %d\n", __func__, value);
			}

			break;

		case SWIMCU_GPIO_SET_VAL:

			/* for output pins only */
			if (pin_statep->dir != MCI_MCU_PIN_DIRECTION_OUTPUT)
			{
				pr_err("%s: illegal operation to set VAL for an input pin %d)\n", __func__, gpio);
				ret = -EPERM;
				break;
			}

			if (pin_statep->level != value)
			{
				swimcu_log(GPIO, "%s: output VAL change from %d to %d\n", __func__, pin_statep->level, value);
				backup_pin_state = swimcu_gpio_cfg[gpio];
				pin_statep->level = value;
				config_changed = true;
			}
			else
			{
				swimcu_log(GPIO, "%s: no change in output VAL %d\n", __func__, value);
			}
			break;

		case SWIMCU_GPIO_SET_PULL:

			if (pin_statep->dir != MCI_MCU_PIN_DIRECTION_INPUT) {
				pr_err ("%s: illegal operation to set PULL for output pin %d\n", __func__, gpio);
				ret = -EPERM;
				break;
			}
			if (value == MCI_MCU_PIN_PULL_NONE)
			{
				if (pin_statep->params.input.pe)
				{
					swimcu_log(GPIO, "%s: disable the pull on the pin %d\n", __func__, value);
					config_changed = true;
				}
			}
			/* enable the pull for all other states */
			else if (!pin_statep->params.input.pe)
			{
				swimcu_log(GPIO, "%s: change PULL OFF to %d\n", __func__, value);
				config_changed = true;
			}
			else if (pin_statep->params.input.ps != (enum mci_mcu_pin_pull_select_e) value)
			{
				swimcu_log(GPIO, "%s: change PULL %d to %d\n", __func__, pin_statep->params.input.ps, value);
				config_changed = true;
			}
			else
			{
				swimcu_log(GPIO, "%s: no change PULL %d\n", __func__, value);
			}

			if (config_changed)
			{
				backup_pin_state = swimcu_gpio_cfg[gpio];
				if(value == MCI_MCU_PIN_PULL_NONE)
				{
					pin_statep->params.input.pe = false;
				}
				else
				{
					pin_statep->params.input.ps = (enum mci_mcu_pin_pull_select_e) value;
					pin_statep->params.input.pe = true;
				}
			}
			break;

		case SWIMCU_GPIO_SET_EDGE:

			ret = swimcu_gpio_irq_support_check(gpio);
			if (0 != ret)
			{
				break;
			}

			if (swimcu_gpio_cfg[gpio].params.input.irqc_type != value)
			{
				swimcu_log(GPIO, "%s: change IRQ type from %d to %d\n",
					 __func__, swimcu_gpio_cfg[gpio].params.input.irqc_type, value);

				backup_pin_state = swimcu_gpio_cfg[gpio];
				swimcu_gpio_cfg[gpio].params.input.irqc_type = value;
				config_changed = true;
			}
			else
			{
				swimcu_log(GPIO, "%s: no change PULL %d\n", __func__, value);
			}

			break;

		case SWIMCU_GPIO_NOOP:

			swimcu_log(GPIO, "%s: refresh gpio %d\n", __func__, gpio);
			break;

		default:

			pr_err("%s: unknown action %d\n", __func__, action);
			ret = -EPERM;
	}

	if (ret)
	{
		if (!caller_locked)
		{
			mutex_unlock(&swimcu_gpio_cfg_mutex[gpio]);
		}
		return ret;
	}

	if (config_changed || (action == SWIMCU_GPIO_NOOP))
	{
		s_code = swimcu_pin_config_set(swimcu,
			swimcu_gpio_map[gpio].port, swimcu_gpio_map[gpio].pin, &(swimcu_gpio_cfg[gpio]));
		if (MCI_PROTOCOL_STATUS_CODE_SUCCESS != s_code)
		{
			pr_err ("%s: failed to configure MCU GPIO%d (status=%d)\n", __func__, gpio, s_code);

			/* recover the old configuration if any change has been made */
			if (config_changed)
			{
				swimcu_gpio_cfg[gpio].mux = MCI_MCU_PIN_FUNCTION_DISABLED;
			}
			ret = -EIO;
		}
	}

	if (!caller_locked)
	{
		mutex_unlock(&swimcu_gpio_cfg_mutex[gpio]);
	}
	return ret;
}

/* External interface wrapper */
int swimcu_gpio_set(struct swimcu *swimcu, int action, int gpio, int value)
{
	return _swimcu_gpio_set(swimcu, action, gpio, value, false);
}

/************
 *
 * Name:     swimcu_gpio_open
 *
 * Purpose:  initialize MCU pin as gpio
 *
 * Parms:    swimcu - device data struct
 *           gpio - a valid gpio index
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from gpio driver on export
 *
 ************/
int swimcu_gpio_open(struct swimcu *swimcu, int gpio)
{
	if (gpio > SWIMCU_GPIO_LAST) {
		return -EINVAL;
	}

	if (MCI_MCU_PIN_FUNCTION_GPIO == swimcu_gpio_cfg[gpio].mux)
	{
		swimcu_log(GPIO, "%s: gpio %d already opened\n", __func__, gpio);
		return 0;
	}

	/* default to GPIO input pin with no pull, interrupt disabled */
	swimcu_gpio_cfg[gpio].mux = MCI_MCU_PIN_FUNCTION_GPIO;
	swimcu_gpio_cfg[gpio].dir = MCI_MCU_PIN_DIRECTION_INPUT;
	swimcu_gpio_cfg[gpio].params.input.pe = false;
	swimcu_gpio_cfg[gpio].params.input.ps = MCI_MCU_PIN_PULL_DOWN;
	swimcu_gpio_cfg[gpio].params.input.irqc_type = MCI_PIN_IRQ_DISABLED;

	return swimcu_gpio_set(swimcu, SWIMCU_GPIO_NOOP, gpio, 0);
}

/************
 *
 * Name:     swimcu_gpio_refresh
 *
 * Purpose:  refresh MCU gpio configuration with currently cached settings
 *
 * Parms:    swimcu - device data struct
 *
 * Return:   nothing
 *
 * Abort:    none
 *
 * Notes:    called on MCU reset
 *
 ************/
void swimcu_gpio_refresh(struct swimcu *swimcu)
{
	enum swimcu_gpio_index gpio;

	swimcu_log(INIT, "%s\n", __func__);
	for (gpio = SWIMCU_GPIO_FIRST; gpio <= SWIMCU_GPIO_LAST; gpio++)
	{
		(void) swimcu_gpio_set(swimcu, SWIMCU_GPIO_NOOP, gpio, 0);
	}
}

/************
 *
 * Name:     swimcu_gpio_module_init
 *
 * Purpose:  Initialize the Sierra Wireless MCU GPIO "chip"
 *
 * Parms:    swimcu      - pointer to SWIMCU device data structure
 *           irq_handler - GPIO IRQ handler. IRQ is handled by GPIO-SWIMCU only,
 *                         Other callers must provide NULL value for the arg!!!
 *
 * Return:   none
 *
 * Notes:    User modules (e.g., SWIMCU-CORE, GPIO-SWIMCU) need to call this
 *           function before access the GPIO.
 *
 * Abort:    none
 *
 ************/
void swimcu_gpio_module_init(
	struct swimcu * swimcup,
	bool (*irq_handler)(struct swimcu *, enum swimcu_gpio_irq_index))
{
	static bool cache_initialized = false;
	enum swimcu_gpio_index gpio;

	swimcu_log(INIT, "%s handler=0x%x cache_init %d\n",
		__func__, (uint32_t)irq_handler, cache_initialized);

	if (!swimcu_gpio_irqp && irq_handler)
	{
		swimcu_gpio_irqp = irq_handler;
	}

	if (cache_initialized)
	{
		return;
	}
	cache_initialized = true;

	/* one-time retrieval of the MCU external pin configuration */
	for (gpio = SWIMCU_GPIO_FIRST; gpio < SWIMCU_NUM_GPIO; gpio++)
	{
		mutex_init(&swimcu_gpio_cfg_mutex[gpio]);
		(void) swimcu_gpio_get(swimcup, SWIMCU_GPIO_NOOP, gpio, 0);
	}
}

/************
 *
 * Name:     swimcu_gpio_close
 *
 * Purpose:  return MCU pin to uninitialized
 *
 * Parms:    swimcu - device data struct
 *           gpio - a valid gpio index
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 * Notes:    called from gpio driver on unexport
 *
 ************/
int swimcu_gpio_close( struct swimcu *swimcu, int gpio )
{
	if (gpio > SWIMCU_GPIO_LAST)
	{
		return -EINVAL;
	}

	swimcu_gpio_cfg[gpio].mux = MCI_MCU_PIN_FUNCTION_DISABLED;
	return swimcu_gpio_set(swimcu, SWIMCU_GPIO_NOOP, gpio, 0);
}

/************
 *
 * Name:     swimcu_gpio_irq_event_handle
 *
 * Purpose:  callback to handle MCU gpio irq event
 *
 * Parms:    swimcu - device data struct
 *           port - 0 or 1 corresponding to PTA or PTB
 *           pin - 0 to 7 for pin # on that port
 *           level - 0 or 1
 *
 * Return:   nothing
 *
 * Abort:    none
 *
 ************/
void swimcu_gpio_irq_event_handle(struct swimcu *swimcu, int port, int pin, int level)
{
	enum swimcu_gpio_index gpio = swimcu_get_gpio_from_port_pin(port, pin);
	enum swimcu_gpio_irq_index swimcu_irq = swimcu_get_irq_from_gpio(gpio);

	mutex_lock(&swimcu->gpio_irq_lock);

	/* MCU disabled IRQ on its triggering */
	swimcu_gpio_cfg[gpio].params.input.irqc_type = MCI_PIN_IRQ_DISABLED;
	swimcu_gpio_cfg[gpio].level = level;

	/* call registered IRQ handler to process */
	if (swimcu_gpio_irqp && swimcu_gpio_irqp(swimcu, swimcu_irq))
	{
		/* Re-enable user-configured IRQ if the interrupt is handled successfully */
		mutex_lock(&swimcu_gpio_cfg_mutex[gpio]);
		swimcu_gpio_cfg[gpio].params.input.irqc_type = gpio_irq_cfg[swimcu_irq].type;
		if (0 == _swimcu_gpio_set(swimcu, SWIMCU_GPIO_NOOP, gpio, 0, true))
		{
			pr_err("%s: Re-enabled irq %d type %X for MCU GPIO %d \n",
				__func__, swimcu_irq, gpio_irq_cfg[swimcu_irq].type, gpio);
		}
		mutex_unlock(&swimcu_gpio_cfg_mutex[gpio]);

	}
	else
	{
		/* Leave GPIO IRQ disabled until user re-configure the IRQ */
		pr_err("%s: failed to handle IRQ event for gpio%d\n", __func__, gpio);
	}

	mutex_unlock(&swimcu->gpio_irq_lock);
}

