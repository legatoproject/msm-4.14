/*
 * swimcu-i2c.c  --  Generic I2C driver for sierra mcu
 *
 * Copyright (c) 2016 Sierra Wireless, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/mfd/swimcu/core.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <mach/swimcu.h>

#ifdef CONFIG_OF
static struct swimcu_platform_data * swimcu_populate_dt_pdata(struct device *dev)
{
	struct swimcu_platform_data *pdata;
	int ret = 0;
	u32 nr_gpio = 0;
	u32 adc_base = 0;
	u32 nr_adc = 0;
	u16 func_flags = 0;
	u8 func_fwupd_en = 0;
	u8 func_event_en = 0;
	u8 func_pm_en = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}

	pdata->gpio_base= SWIMCU_GPIO_BASE;
	dev_dbg(dev, "swimcu,gpio-base is %d", pdata->gpio_base);

	ret = of_property_read_u32(dev->of_node,
														"swimcu,nr-gpio",
														&nr_gpio);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,nr-gpio",
			dev->of_node->full_name);
		ret = -EINVAL;
		goto err;
	}
	dev_dbg(dev, "Parse %s propety in node %s is %u",
		"swimcu,nr-gpio",
		dev->of_node->full_name,
		nr_gpio);
	pdata->nr_gpio= nr_gpio;

	ret = of_property_read_u32(dev->of_node,
				"swimcu,adc-base",
				&adc_base);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,adc-base",
			dev->of_node->full_name);
		ret = -EINVAL;
		goto err;
	}
	dev_dbg(dev, "Parse %s propety in node %s is %u",
		"swimcu,adc-base",
		dev->of_node->full_name,
		adc_base);
	pdata->adc_base= adc_base;

	ret = of_property_read_u32(dev->of_node,
				"swimcu,nr-adc",
				&nr_adc);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,nr-adc",
			dev->of_node->full_name);
		ret = -EINVAL;
		goto err;
	}
	dev_dbg(dev, "Parse %s propety in node %s is %u",
		"swimcu,nr-adc",
		dev->of_node->full_name,
		nr_adc);
	pdata->nr_adc= nr_adc;

	ret = of_property_read_u8(dev->of_node,
				"swimcu,func-fwupd-en",
				&func_fwupd_en);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,func-fwupd-en",
			dev->of_node->full_name);
	} else if ( !!func_fwupd_en ) {
		func_flags |= SWIMCU_FUNC_FLAG_FWUPD;
	}

	ret = of_property_read_u8(dev->of_node,
				"swimcu,func-pm-en",
				&func_pm_en);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,func-pm-en",
			dev->of_node->full_name);
	} else if ( !!func_pm_en ) {
		func_flags |= SWIMCU_FUNC_FLAG_PM;
	}

	ret = of_property_read_u8(dev->of_node,
				"swimcu,func-event-en",
				&func_event_en);
	if (ret) {
		dev_err(dev, "Looking up %s property in\n"
			"node %s failed",
			"swimcu,func-event-en",
			dev->of_node->full_name);
	} else if ( !!func_event_en ) {
		func_flags |= SWIMCU_FUNC_FLAG_EVENT;
	}

	dev_dbg(dev, "Parse %s propety in node %s is %u",
		"swimcu,func_flags",
		dev->of_node->full_name,
		func_flags);
	pdata->func_flags = func_flags;

	return pdata;

err:
	devm_kfree(dev, pdata);
	return NULL;
}
#else
static struct swimcu_platform_data * swimcu_populate_dt_pdata(struct device *dev)
{
	return NULL;
}
#endif

static int swimcu_i2c_probe(struct i2c_client *i2c,
							const struct i2c_device_id *id)
{
	struct swimcu *swimcu;
	struct swimcu_platform_data *pdata;
	int ret = 0;

	swimcu_log(INIT, "%s: start %lu\n", __func__, id->driver_data);
	swimcu = devm_kzalloc(&i2c->dev, sizeof(struct swimcu), GFP_KERNEL);
	if (swimcu == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, swimcu);

	pdata = swimcu_populate_dt_pdata(&i2c->dev);
	if (!pdata) {
		dev_err(&i2c->dev,
			"%s: Fail to obtain pdata from device tree\n",
			__func__);
		ret = -EINVAL;
		goto fail;
	}

	i2c->dev.platform_data = (struct platform_data *) pdata;

	swimcu->dev = &i2c->dev;
	swimcu->client = i2c;
	swimcu->i2c_driver_id = id->driver_data;

	mutex_init(&swimcu->calibrate_mutex);
	swimcu->calibrate_mcu_time = 1;
	swimcu->calibrate_mdm_time = 1;

	return swimcu_device_init(swimcu);

fail:
	return ret;
}

static int swimcu_i2c_remove(struct i2c_client *i2c)
{
	struct swimcu *swimcu = i2c_get_clientdata(i2c);

	swimcu_device_exit(swimcu);

	return 0;
}

static const struct i2c_device_id swimcu_i2c_id[] = {
				{ "mkl03", SWIMCU_APPL_I2C_ID },
				{ }
};
MODULE_DEVICE_TABLE(i2c, swimcu_i2c_id);


static struct i2c_driver swimcu_appl_i2c_driver = {
	.driver = {
		.name = "swimcu",
		.owner = THIS_MODULE,
	},
	.probe = swimcu_i2c_probe,
	.remove = swimcu_i2c_remove,
	.id_table = swimcu_i2c_id,
};

static int __init swimcu_i2c_init(void)
{
	swimcu_log(INIT, "%s: start\n", __func__);

	return i2c_add_driver(&swimcu_appl_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(swimcu_i2c_init);

static void __exit swimcu_i2c_exit(void)
{
	i2c_del_driver(&swimcu_appl_i2c_driver);
}
module_exit(swimcu_i2c_exit);

MODULE_DESCRIPTION("I2C support for the Sierra Wireless MCU");
MODULE_LICENSE("GPL");
