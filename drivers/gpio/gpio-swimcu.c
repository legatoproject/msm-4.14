/*
 * gpiolib support for Sierra Wireless WP76xx MCU
 *
 * adapted from:
 * gpiolib support for Wolfson WM835x PMICs
 *
 * Copyright 2016 Sierra Wireless
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/irq.h>

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/gpio.h>
#include <linux/mfd/swimcu/mcidefs.h>
#include "gpiolib.h"

struct swimcu_gpio_data {
	struct swimcu *swimcu;
	struct gpio_chip gpio_chip;
};

enum mci_pin_irqc_type_e gpio_irq_cfg[SWIMCU_NUM_GPIO_IRQ] = {MCI_PIN_IRQ_DISABLED};

static inline struct swimcu *to_swimcu(struct gpio_chip *chip)
{
	struct swimcu_gpio_data *swimcu_gpio = container_of(chip, struct swimcu_gpio_data, gpio_chip);
	return swimcu_gpio->swimcu;
}

static int swimcu_gpio_set_direction_in(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_set(swimcu, SWIMCU_GPIO_SET_DIR, gpio, 0);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d IN\n", __func__, gpio);
	}

	return ret;
}

static int swimcu_gpio_set_direction_out(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_set(swimcu, SWIMCU_GPIO_SET_DIR, gpio, 1+value);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d OUT\n", __func__, gpio);
	}

	return ret;
}

static int swimcu_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int value;
	int ret;

	ret = swimcu_gpio_get(swimcu, SWIMCU_GPIO_GET_VAL, gpio, &value);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
		return ret;
	}

	swimcu_log(GPIO, "%s: gpio%d get level %d\n", __func__, gpio, value);

	return value;
}

static void swimcu_gpio_set_value(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_set(swimcu, SWIMCU_GPIO_SET_VAL, gpio, value);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d = %d\n", __func__, gpio, value);
	}
}

static int swimcu_gpio_set_pull_up(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_set(swimcu, SWIMCU_GPIO_SET_PULL, gpio, 1);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d UP\n", __func__, gpio);
	}

	return ret;
}

static int swimcu_gpio_set_pull_down(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_set(swimcu, SWIMCU_GPIO_SET_PULL, gpio, 0);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d DOWN\n", __func__, gpio);
	}

	return ret;
}

static int swimcu_gpio_request(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_open(swimcu, gpio);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d\n", __func__, gpio);
	}

	return ret;
}

static void swimcu_gpio_free(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret;

	ret = swimcu_gpio_close(swimcu, gpio);

	if (ret < 0) {
		pr_err("%s: gpio%d error ret=%d\n", __func__, gpio, ret);
	}
	else {
		swimcu_log(GPIO, "%s: gpio%d\n", __func__, gpio);
	}
}

static int swimcu_to_irq(struct gpio_chip *chip, unsigned gpio)
{
	struct swimcu *swimcu = to_swimcu(chip);
	int ret = -1;
	enum swimcu_gpio_irq_index swimcu_irq = swimcu_get_irq_from_gpio(gpio);

	if ((swimcu->gpio_irq_base > 0) && (swimcu_irq != SWIMCU_GPIO_NO_IRQ)) {
		ret = swimcu->gpio_irq_base + swimcu_irq;
	}
	return ret;
}

static struct gpio_chip template_chip = {
	.label			= "swimcu",
	.owner			= THIS_MODULE,
	.request    		= swimcu_gpio_request,
	.free       		= swimcu_gpio_free,
	.direction_input	= swimcu_gpio_set_direction_in,
	.get			= swimcu_gpio_get_value,
	.direction_output	= swimcu_gpio_set_direction_out,
	.set			= swimcu_gpio_set_value,
	.pull_up    		= swimcu_gpio_set_pull_up,
	.pull_down  		= swimcu_gpio_set_pull_down,
	.to_irq		  	= swimcu_to_irq,
	.can_sleep		= true,
};

void swimcu_gpio_work(struct swimcu *swimcu, enum swimcu_gpio_irq_index irq)
{
	int gpio = swimcu_get_gpio_from_irq(irq);
	int result;

	if (irq < 0 || irq >= SWIMCU_NUM_GPIO_IRQ) {
		pr_err("%s: Invalid IRQ: %d\n", __func__, irq);
		return;
	}

	handle_nested_irq(swimcu->gpio_irq_base + irq);
	result = swimcu_gpio_set_trigger(gpio, gpio_irq_cfg[irq]);
	if (result < 0) {
		pr_err("%s: gpio%d error result=%d\n", __func__, gpio, result);
	}
	else {
		/* need to refresh gpio configs to apply trigger settings */
		swimcu_gpio_refresh(swimcu);
	}
}

static inline int sys_irq_to_swimcu_irq(struct swimcu *swimcu, int irq)
{
	return irq - swimcu->gpio_irq_base;
}

static void swimcu_irq_lock(struct irq_data *data)
{
	struct swimcu *swimcu = irq_data_get_irq_chip_data(data);

	mutex_lock(&swimcu->gpio_irq_lock);
}

static void swimcu_irq_sync_unlock(struct irq_data *data)
{
	struct swimcu *swimcu = irq_data_get_irq_chip_data(data);

	mutex_unlock(&swimcu->gpio_irq_lock);
	swimcu_gpio_refresh(swimcu);
}

static void swimcu_irq_disable(struct irq_data *data)
{
	struct swimcu *swimcu = irq_data_get_irq_chip_data(data);
	int swimcu_irq = sys_irq_to_swimcu_irq(swimcu, data->irq);
	int gpio = swimcu_get_gpio_from_irq(swimcu_irq);
	enum mci_pin_irqc_type_e irq_type;
	int result;

	/* We can't directly mask interrupts on the MCU,
	* so save the current trigger config then set it to disabled */
	irq_type = swimcu_gpio_get_trigger(gpio);
	result = swimcu_gpio_set_trigger(gpio, MCI_PIN_IRQ_DISABLED);
	if(result < 0) {
		pr_err("%s: gpio%d error result=%d\n", __func__, gpio, result);
	}
	else {
		gpio_irq_cfg[swimcu_irq] = irq_type;
	}
}

static void swimcu_irq_enable(struct irq_data *data)
{
	struct swimcu *swimcu = irq_data_get_irq_chip_data(data);
	int swimcu_irq = sys_irq_to_swimcu_irq(swimcu, data->irq);
	int gpio = swimcu_get_gpio_from_irq(swimcu_irq);
	int result;

	/* restore saved trigger config */
	result = swimcu_gpio_set_trigger(gpio, gpio_irq_cfg[swimcu_irq]);
	if (result < 0) {
		pr_err("%s: gpio%d error result=%d\n", __func__, gpio, result);
	}
}

static int swimcu_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct swimcu *swimcu = irq_data_get_irq_chip_data(data);
	int swimcu_irq = sys_irq_to_swimcu_irq(swimcu, data->irq);
	int gpio = swimcu_get_gpio_from_irq(swimcu_irq);
	enum mci_pin_irqc_type_e irq_type;
	int result;

	switch (type)
	{
		case IRQ_TYPE_LEVEL_LOW:
			irq_type = MCI_PIN_IRQ_LOGIC_ZERO;
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			irq_type = MCI_PIN_IRQ_LOGIC_ONE;
			break;
		case IRQ_TYPE_EDGE_BOTH:
			irq_type = MCI_PIN_IRQ_EITHER_EDGE;
			break;
		case IRQ_TYPE_EDGE_RISING:
			irq_type = MCI_PIN_IRQ_RISING_EDGE;
			break;
		case IRQ_TYPE_EDGE_FALLING:
			irq_type = MCI_PIN_IRQ_FALLING_EDGE;
			break;
		default:
			irq_type = MCI_PIN_IRQ_DISABLED;
	}

	result = swimcu_gpio_set_trigger(gpio, irq_type);
	if(result < 0) {
		pr_err("%s: gpio%d error result=%d\n", __func__, gpio, result);
	}
	else {
		gpio_irq_cfg[swimcu_irq] = irq_type;
	}
	return result;
}

static struct irq_chip swimcu_irq_chip = {
	.name			= "swimcu-irq",
	.irq_bus_lock		= swimcu_irq_lock,
	.irq_bus_sync_unlock	= swimcu_irq_sync_unlock,
	.irq_disable		= swimcu_irq_disable,
	.irq_enable		= swimcu_irq_enable,
	.irq_set_type		= swimcu_irq_set_type,
};

void swimcu_irq_init(struct swimcu *swimcu, int irq_base)
{
	int i;

	mutex_init(&swimcu->gpio_irq_lock);
	swimcu->gpio_irq_base = irq_alloc_descs(-1, irq_base, SWIMCU_NUM_GPIO_IRQ, -1);
	if (swimcu->gpio_irq_base < 0)
	{
		dev_warn(swimcu->dev, "Allocating irqs failed with %d\n",
			swimcu->gpio_irq_base);
		return;
	}

	/* register with genirq */
	for (i = swimcu->gpio_irq_base; i < SWIMCU_NUM_GPIO_IRQ + swimcu->gpio_irq_base; i++)
	{
		irq_set_chip_data(i, swimcu);
		irq_set_chip_and_handler(i, &swimcu_irq_chip, handle_simple_irq);
		irq_set_nested_thread(i, 1);

		/* ARM needs us to explicitly flag the IRQ as valid
		 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
		/* set_irq_flags(i, IRQF_VALID) -> irq_clear_status_flags(i, IRQ_NOREQUEST)
		 * https://patchwork.kernel.org/patch/6772731/
		 */
		irq_clear_status_flags(i, IRQ_NOREQUEST);
#else
		irq_set_noprobe(i);
#endif
	}
}

static int swimcu_gpio_probe(struct platform_device *pdev)
{
	struct swimcu *swimcu = dev_get_drvdata(pdev->dev.parent);
	struct swimcu_platform_data *pdata = dev_get_platdata(swimcu->dev);
	struct swimcu_gpio_data *swimcu_gpio;
	int ret;

	if ((pdata != NULL) && (pdata->nr_gpio > 0)) {
		swimcu_log(GPIO, "%s: start, base %d, nr %d\n", __func__, pdata->gpio_base, pdata->nr_gpio);
	}
	else {
		pr_err("%s: no gpio\n", __func__);
		return -ENODEV;
	}

	swimcu_gpio = devm_kzalloc(&pdev->dev, sizeof(*swimcu_gpio),
				   GFP_KERNEL);
	if (swimcu_gpio == NULL)
		return -ENOMEM;

	swimcu_gpio->swimcu = swimcu;
	swimcu_gpio->gpio_chip = template_chip;
	swimcu_gpio->gpio_chip.ngpio = pdata->nr_gpio;
	swimcu_gpio->gpio_chip.parent = &pdev->dev;
	swimcu_gpio->gpio_chip.base = pdata->gpio_base;

	ret = gpiochip_add(&swimcu_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n", ret);
		return ret;
	}

	swimcu_irq_init(swimcu, pdata->gpio_irq_base);
	platform_set_drvdata(pdev, swimcu_gpio);

	return ret;
}

void swimcu_gpio_irq_exit(struct swimcu* swimcu)
{
	irq_free_descs(swimcu->gpio_irq_base, SWIMCU_NUM_GPIO_IRQ);
}

static int swimcu_gpio_remove(struct platform_device *pdev)
{
	struct swimcu_gpio_data *swimcu_gpio = platform_get_drvdata(pdev);

	swimcu_gpio_irq_exit(swimcu_gpio->swimcu);
	(void) gpiochip_remove(&swimcu_gpio->gpio_chip);
	return 0;
}

static struct platform_driver swimcu_gpio_driver = {
	.driver.name	= "swimcu-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= swimcu_gpio_probe,
	.remove		= swimcu_gpio_remove,
};

static int __init swimcu_gpio_init(void)
{
	swimcu_log(GPIO, "%s: start\n", __func__);
	return platform_driver_register(&swimcu_gpio_driver);
}
subsys_initcall(swimcu_gpio_init);

static void __exit swimcu_gpio_exit(void)
{
	platform_driver_unregister(&swimcu_gpio_driver);
}
module_exit(swimcu_gpio_exit);

MODULE_DESCRIPTION("GPIO interface for Sierra Wireless MCU");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:swimcu-gpio");
