/*
 * wm8944-core.c  --  Device access for Wolfson WM8944
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 * Based on wm8994.c
 *     Copyright 2009 Wolfson Microelectronics PLC.
 *     Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
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
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#include <linux/mfd/wm8944/core.h>
#include <linux/mfd/wm8944/pdata.h>
#include <linux/mfd/wm8944/registers.h>

#include "wm8944.h"

#include <linux/sierra_bsudefs.h>

static int wm8944_intf = WM8944_INTERFACE_TYPE_UNKNOWN;

/**
 * @brief wm8944_get_intf_type: Get interface type
 *
 * @return  1:I2C, 0:None, -1:Unknown
 */
int wm8944_get_intf_type(void)
{
	return wm8944_intf;
}
EXPORT_SYMBOL_GPL(wm8944_get_intf_type);


/**
 * wm8944_reg_read: Read a single WM8944 register.
 *
 * @wm8944: Device to read from.
 * @reg: Register to read.
 */
int wm8944_reg_read(struct wm8944 *wm8944, unsigned short reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(wm8944->regmap, reg, &val);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(wm8944_reg_read);

/**
 * wm8944_bulk_read: Read multiple WM8944 registers
 *
 * @wm8944: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.  The data will be returned big endian.
 */
int wm8944_bulk_read(struct wm8944 *wm8944, unsigned short reg,
		     int count, u16 *buf)
{
	return regmap_bulk_read(wm8944->regmap, reg, buf, count);
}

/**
 * wm8944_reg_write: Write a single WM8944 register.
 *
 * @wm8944: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int wm8944_reg_write(struct wm8944 *wm8944, unsigned short reg,
		     unsigned short val)
{
	dev_dbg(wm8944->dev, "%s reg 0x%x val 0x%x\n", __func__, reg, val);
	return regmap_write(wm8944->regmap, reg, val);
}
EXPORT_SYMBOL_GPL(wm8944_reg_write);

/**
 * wm8944_bulk_write: Write multiple WM8944 registers
 *
 * @wm8944: Device to write to
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to write from.  Data must be big-endian formatted.
 */
int wm8944_bulk_write(struct wm8944 *wm8944, unsigned short reg,
		      int count, const u16 *buf)
{
	return regmap_raw_write(wm8944->regmap, reg, buf, count * sizeof(u16));
}
EXPORT_SYMBOL_GPL(wm8944_bulk_write);

/**
 * wm8944_set_bits: Set the value of a bitfield in a WM8944 register
 *
 * @wm8944: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int wm8944_set_bits(struct wm8944 *wm8944, unsigned short reg,
		    unsigned short mask, unsigned short val)
{
	return regmap_update_bits(wm8944->regmap, reg, mask, val);
}
EXPORT_SYMBOL_GPL(wm8944_set_bits);

static struct resource wm8944_codec_resources[] = {
	{
		.start = WM8944_IRQ_LDO_UV,
		.end   = WM8944_IRQ_TEMP,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource wm8944_gpio_resources[] = {
	{
		.start = WM8944_IRQ_GP1,
		.end   = WM8944_IRQ_GP2,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell wm8944_devs[] = {
	{
		.name = "wm8944-codec",
		.num_resources = ARRAY_SIZE(wm8944_codec_resources),
		.resources = wm8944_codec_resources,
	},

	{
		.name = "wm8944-gpio",
		.num_resources = ARRAY_SIZE(wm8944_gpio_resources),
		.resources = wm8944_gpio_resources,
		.pm_runtime_no_callbacks = true,
	},
};


#ifdef CONFIG_PM
static int wm8944_suspend(struct device *dev)
{
	struct wm8944 *wm8944 = dev_get_drvdata(dev);
	int ret;

	printk(KERN_DEBUG "wm8944_suspend - START\n");

	/* Don't actually go through with the suspend if the CODEC is
	 * still active (eg, for audio passthrough from CP. */
	ret = wm8944_reg_read(wm8944, WM8944_POWER1);
	if (ret < 0) {
		dev_err(dev, "Failed to read power status: %d\n", ret);
	} else if (ret & (WM8944_INPGA_ENA | WM8944_ADCR_ENA | WM8944_ADCL_ENA
			  | WM8944_MICB_ENA)) {
		dev_dbg(dev, "CODEC still active, ignoring suspend\n");
		return 0;
	}

	ret = wm8944_reg_read(wm8944, WM8944_POWER2);
	if (ret < 0) {
		dev_err(dev, "Failed to read power status: %d\n", ret);
	} else if (ret & (WM8944_SPK_PGA_ENA | WM8944_SPKN_SPKVDD_ENA |
			  WM8944_SPKP_SPKVDD_ENA | WM8944_SPKN_OP_ENA |
			  WM8944_SPKP_OP_ENA | WM8944_DAC_ENA |
			  WM8944_SPK_MIX_ENA | WM8944_DAC_ENA)) {
		dev_dbg(dev, "CODEC still active, ignoring suspend\n");
		return 0;
	}

	/* Explicitly put the device into reset in case regulators
	 * don't get disabled in order to ensure consistent restart.
	 */
	wm8944_reg_write(wm8944, WM8944_SOFTRESET, 0);

	regcache_cache_only(wm8944->regmap, true);
	regcache_mark_dirty(wm8944->regmap);

	wm8944->suspended = true;

	ret = regulator_bulk_disable(wm8944->num_supplies,
				     wm8944->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to disable supplies: %d\n", ret);
		return ret;
	}

	printk(KERN_DEBUG "wm8944_suspend - OK\n");
	return 0;
}

static int wm8944_resume(struct device *dev)
{
	struct wm8944 *wm8944 = dev_get_drvdata(dev);
	int ret;

	/* We may have lied to the PM core about suspending */
	if (!wm8944->suspended)
		return 0;

	ret = regulator_bulk_enable(wm8944->num_supplies,
				    wm8944->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	regcache_cache_only(wm8944->regmap, false);
	ret = regcache_sync(wm8944->regmap);
	if (ret != 0) {
		dev_err(dev, "Failed to restore register map: %d\n", ret);
		goto err_enable;
	}

	wm8944->suspended = false;

	return 0;

err_enable:
	regulator_bulk_disable(wm8944->num_supplies, wm8944->supplies);

	return ret;
}
#endif

#if 0 //jmcdef CONFIG_REGULATOR
static int wm8944_ldo_in_use(struct wm8944_pdata *pdata, int ldo)
{
	struct wm8944_ldo_pdata *ldo_pdata;

	if (!pdata)
		return 0;

	ldo_pdata = &pdata->ldo[ldo];

	if (!ldo_pdata->init_data)
		return 0;

	return ldo_pdata->init_data->num_consumer_supplies != 0;
}
#else
static int wm8944_ldo_in_use(struct wm8944_pdata *pdata, int ldo)
{
	return 0;
}
#endif


static struct wm8944_pdata* wm8944_populate_dt_pdata(struct device* dev)
{
	struct wm8944_pdata *pdata;
	struct device_node *np = dev->of_node;
	u32 vmid_mode;
	u32 dacvol;
	u32 speakvol;

	pdata = devm_kzalloc(dev, sizeof(struct wm8944_pdata), GFP_KERNEL);
	if(!pdata) {
		dev_err(dev, "no memory\n");
		return NULL;
	}

	of_property_read_u32_array(np, "wlf,gpio_defaults", pdata->gpio_defaults,
				   ARRAY_SIZE(pdata->gpio_defaults));

	pdata->line_vroi = of_property_read_bool(np, "wlf,line_vroi");

	pdata->spk_vroi = of_property_read_bool(np, "wlf,spk_vroi");

	pdata->micbias_lvl = of_property_read_bool(np, "wlf,micbias_lvl");

	pdata->spkmode_pu = of_property_read_bool(np, "wlf,spkmode_pu");

	pdata->idle_bias_off = of_property_read_bool(np, "wlf,idle_bias_off");

	if(!of_property_read_u32(np, "wlf,vmid_mode", &vmid_mode)) {
		pdata->vmid_mode = vmid_mode;
	}

	if(!of_property_read_u32(np, "wlf,dacvol", &dacvol)) {
		pdata->dacvol = dacvol;
	}

	if(!of_property_read_u32(np, "wlf,speakvol", &speakvol)) {
		pdata->speakvol = speakvol;
	}

	return pdata;
}

/*
 * Instantiate the generic non-control parts of the device.
 */
static int wm8944_device_init(struct wm8944 *wm8944, int irq)
{
	struct wm8944_pdata *pdata;
	struct regmap_config *regmap_config;
	const char *devname;
	int ret;
	int pulls = 0;

	printk(KERN_DEBUG "%s START\n",__func__);

	if(wm8944->dev->of_node) {
		pdata = wm8944_populate_dt_pdata(wm8944->dev);
		if(!pdata) {
			dev_err(wm8944->dev,
				"%s: failed to obtain pdata from device tree",
				__func__);
			ret = -EINVAL;
			goto err_enable;
		}
		wm8944->dev->platform_data = pdata;
	}

	dev_set_drvdata(wm8944->dev, wm8944);

	ret = wm8944_reg_read(wm8944, WM8944_SOFTRESET);
	if (ret < 0) {
		dev_err(wm8944->dev, "Failed to read ID register\n");
		goto err_enable;
	}

	if (ret != WM8944_CHIP_ID) {
		dev_err(wm8944->dev, "Bad chip ID (0x%x)\n", ret);
		goto err_enable;
	}

	devname = "WM8944";

	ret = wm8944_reg_read(wm8944, WM8944_CHIPVERSION);
	if (ret < 0) {
		dev_err(wm8944->dev, "Failed to read revision register: %d\n",
			ret);
		goto err_enable;
	}
	wm8944->revision = ret;

	if (wm8944->revision != 1) {
		dev_warn(wm8944->dev, "revision %c not fully supported\n",'A'
			 + wm8944->revision);
	}

	dev_info(wm8944->dev, "%s revision %c\n", devname, 'A'
		 + wm8944->revision);

	regmap_config = &wm8944_regmap_config;

	ret = regmap_reinit_cache(wm8944->regmap, regmap_config);
	if (ret != 0) {
		dev_err(wm8944->dev, "Failed to reinit register cache: %d\n",
			ret);
		return ret;
	}

	/* reset the codec to ensure that we're starting in stable state */
	wm8944_reg_write(wm8944, WM8944_SOFTRESET, 0);

	if (pdata) {
		int i;

		wm8944->irq_base = (int)pdata->irq_base;
		wm8944->gpio_base = (int)pdata->gpio_base;

		/* GPIO configuration is only applied if it's non-zero */
		for (i = 0; i < ARRAY_SIZE(pdata->gpio_defaults); i++) {
			if (pdata->gpio_defaults[i]) {
				wm8944_set_bits(wm8944, WM8944_GPIO1CTL + i,
						0xffff,
						pdata->gpio_defaults[i]);
			}
		}

		if (pdata->spkmode_pu)
			pulls |= WM8944_DACDATA_PU;
	}

	/* Disable unneeded pulls */
	wm8944_set_bits(wm8944, WM8944_IFACE, WM8944_DACDATA_PU |
			WM8944_FRAME_PU | WM8944_BLCK_PU, pulls);

	/* In some system designs where the regulators are not in use,
	 * we can achieve a small reduction in leakage currents by
	 * floating LDO outputs.  This bit makes no difference if the
	 * LDOs are enabled, it only affects cases where the LDOs were
	 * in operation and are then disabled.
	 */
	if (wm8944_ldo_in_use(pdata, 0))
		wm8944_set_bits(wm8944, WM8944_LDO, WM8944_LDO_OPFLT,
				WM8944_LDO_OPFLT);
	else
		wm8944_set_bits(wm8944, WM8944_LDO, WM8944_LDO_OPFLT, 0);

	wm8944_irq_init(wm8944);

	ret = mfd_add_devices(wm8944->dev, -1, wm8944_devs,
			      ARRAY_SIZE(wm8944_devs), NULL, 0, NULL);
	if (ret != 0) {
		dev_err(wm8944->dev, "Failed to add children: %d\n", ret);
		goto err_irq;
	}

	pm_runtime_enable(wm8944->dev);
	pm_runtime_idle(wm8944->dev);
	wm8944_intf = WM8944_INTERFACE_TYPE_I2C;
	printk(KERN_DEBUG "%s OK\n",__func__);
	return 0;

err_irq:
	wm8944_irq_exit(wm8944);
err_enable:
	regulator_bulk_disable(wm8944->num_supplies, wm8944->supplies);
	regulator_bulk_free(wm8944->num_supplies, wm8944->supplies);

	mfd_remove_devices(wm8944->dev);
	wm8944_intf = WM8944_INTERFACE_TYPE_NONE;
	printk(KERN_DEBUG "%s - Failed to add children: %d\n",__func__, ret);

	return ret;
}

static void wm8944_device_exit(struct wm8944 *wm8944)
{
	pm_runtime_disable(wm8944->dev);
	mfd_remove_devices(wm8944->dev);
	wm8944_irq_exit(wm8944);
	regulator_bulk_disable(wm8944->num_supplies, wm8944->supplies);
	regulator_bulk_free(wm8944->num_supplies, wm8944->supplies);
	printk(KERN_DEBUG "%s \n", __func__);
}

static int wm8944_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8944 *wm8944;
	int ret;

	printk(KERN_DEBUG "%s \n", __func__);

	wm8944 = devm_kzalloc(&i2c->dev, sizeof(struct wm8944), GFP_KERNEL);

	if (wm8944 == NULL) {
		printk(KERN_ERR "wm8944_i2c_probe failed\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, wm8944);
	wm8944->dev = &i2c->dev;
	wm8944->irq = i2c->irq;

	wm8944->regmap = devm_regmap_init_i2c(i2c, &wm8944_base_regmap_config);

	if (IS_ERR(wm8944->regmap)) {
		ret = PTR_ERR(wm8944->regmap);
		printk(KERN_ERR "Failed to allocate register map: %d\n", ret);
		return ret;
	}
	return wm8944_device_init(wm8944, i2c->irq);
}

static int wm8944_i2c_remove(struct i2c_client *i2c)
{
	struct wm8944 *wm8944 = i2c_get_clientdata(i2c);

	wm8944_device_exit(wm8944);

	return 0;
}

static const struct i2c_device_id wm8944_i2c_id[] = {
	{ "wm8944", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8944_i2c_id);

static UNIVERSAL_DEV_PM_OPS(wm8944_pm_ops, wm8944_suspend, wm8944_resume, NULL);

static struct i2c_driver wm8944_i2c_driver = {
	.driver = {
		.name = "wm8944",
		.owner = THIS_MODULE,
		.pm = &wm8944_pm_ops,
	},
	.probe = wm8944_i2c_probe,
	.remove = wm8944_i2c_remove,
	.id_table = wm8944_i2c_id,
};

static int __init wm8944_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&wm8944_i2c_driver);
	if (ret != 0)
	{
		pr_err("Failed to register wm8944 I2C driver: %d\n", ret);
	}
	return ret;
}
subsys_initcall(wm8944_i2c_init);

static void __exit wm8944_i2c_exit(void)
{

	if(bs_support_get(BSFEATURE_WM8944) == false)
		return;

	i2c_del_driver(&wm8944_i2c_driver);
}
module_exit(wm8944_i2c_exit);

MODULE_DESCRIPTION("Core support for the WM8944 audio CODEC");
