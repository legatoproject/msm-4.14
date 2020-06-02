/*
 * drivers/hwmon/swimcu-hwmon.c - Sierra Wireless WP76xx MCU ADC.
 *
 * adapted from:
 * drivers/hwmon/wm8350-hwmon.c - Wolfson Microelectronics WM8350 PMIC
 *                                  hardware monitoring features.
 *
 * Copyright (C) 2016 Sierra Wireless
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include <linux/mfd/swimcu/core.h>

static ssize_t show_name(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "swimcu\n");
}

static ssize_t show_voltage(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct swimcu *swimcu = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	int val;

	val = swimcu_read_adc(swimcu, channel);

	return sprintf(buf, "%d\n", val);
}

static ssize_t show_label(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct swimcu *swimcu = dev_get_drvdata(dev);
	struct swimcu_platform_data *pdata = dev_get_platdata(swimcu->dev);
	int channel = to_sensor_dev_attr(attr)->index;

	if (pdata != NULL)
		channel += pdata->adc_base;

	return sprintf(buf, "adc%d\n", channel);
}

#define SWIMCU_NAMED_VOLTAGE(id, name) \
	static SENSOR_DEVICE_ATTR(in##id##_mv, S_IRUGO, show_voltage,\
				  NULL, name);		\
	static SENSOR_DEVICE_ATTR(in##id##_label, S_IRUGO, show_label,	\
				  NULL, name)

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

SWIMCU_NAMED_VOLTAGE(0, (int)SWIMCU_ADC_PTA12);
SWIMCU_NAMED_VOLTAGE(1, (int)SWIMCU_ADC_PTB1);

static struct attribute *swimcu_attributes[] = {
	&dev_attr_name.attr,

	&sensor_dev_attr_in0_mv.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,

	&sensor_dev_attr_in1_mv.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,

	NULL,
};

static const struct attribute_group swimcu_attr_group = {
	.attrs	= swimcu_attributes,
};

static int swimcu_hwmon_probe(struct platform_device *pdev)
{
	struct swimcu *swimcu = platform_get_drvdata(pdev);
	struct swimcu_platform_data *pdata = dev_get_platdata(swimcu->dev);
	int ret;

	swimcu_log(INIT, "%s: start\n", __func__);

	if (pdata->nr_adc > 0) {
		if (pdata->nr_adc > SWIMCU_NUM_ADC)
			pdata->nr_adc = SWIMCU_NUM_ADC;
		swimcu_attributes[2* pdata->nr_adc + 1] = NULL;

		ret = sysfs_create_group(&pdev->dev.kobj, &swimcu_attr_group);
		if (ret)
			goto err;

		swimcu->hwmon.classdev = hwmon_device_register(&pdev->dev);
		if (IS_ERR(swimcu->hwmon.classdev)) {
			ret = PTR_ERR(swimcu->hwmon.classdev);
			goto err_group;
		}
	}

	swimcu_log(INIT, "%s: success num %d\n", __func__, pdata->nr_adc);
	return 0;

err_group:
	sysfs_remove_group(&pdev->dev.kobj, &swimcu_attr_group);
err:
	swimcu_log(INIT, "%s: fail %d\n", __func__, ret);
	return ret;
}

static int swimcu_hwmon_remove(struct platform_device *pdev)
{
	struct swimcu *swimcu = platform_get_drvdata(pdev);

	hwmon_device_unregister(swimcu->hwmon.classdev);
	sysfs_remove_group(&pdev->dev.kobj, &swimcu_attr_group);

	return 0;
}

static struct platform_driver swimcu_hwmon_driver = {
	.probe = swimcu_hwmon_probe,
	.remove = swimcu_hwmon_remove,
	.driver = {
		.name = "swimcu-hwmon",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(swimcu_hwmon_driver);

MODULE_DESCRIPTION("SWIMCU Hardware Monitoring");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:swimcu-hwmon");
