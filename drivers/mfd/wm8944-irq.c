/*
 * wm8944-irq.c  --  Interrupt controller support for Wolfson WM8944
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 * based on wm8994-irq.c
 *    Copyright 2010 Wolfson Microelectronics PLC.
 *    Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>

#include <linux/mfd/wm8944/core.h>
#include <linux/mfd/wm8944/registers.h>

#include <linux/delay.h>

static struct regmap_irq wm8944_irqs[] = {
	[WM8944_IRQ_LDO_UV] = {
		.mask = WM8944_LDO_UV_EINT,
	},
	[WM8944_IRQ_GP1] = {
		.mask = WM8944_GP1_EINT,
	},
	[WM8944_IRQ_GP2] = {
		.mask = WM8944_GP2_EINT,
	},
	[WM8944_IRQ_TEMP] = {
		.mask = WM8944_TEMP_EINT,
	},
};

static struct regmap_irq_chip wm8944_irq_chip = {
	.name = "wm8944",
	.irqs = wm8944_irqs,
	.num_irqs = ARRAY_SIZE(wm8944_irqs),

	.num_regs = 1,
	.status_base = WM8944_SYSIT,
	.mask_base = WM8944_SYSITMSK,
	.ack_base = 0,
};

int wm8944_irq_init(struct wm8944 *wm8944)
{
	int ret;

	if (!wm8944->irq) {
		dev_warn(wm8944->dev, "No interrupt specified, no interrupts\n");
		wm8944->irq_base = 0;
		return 0;
	}

	if (!wm8944->irq_base) {
		dev_err(wm8944->dev, "No interrupt base specified, no interrupts\n");
		return 0;
	}

	ret = regmap_add_irq_chip(wm8944->regmap, wm8944->irq,
				  IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				  wm8944->irq_base, &wm8944_irq_chip,
				  &wm8944->irq_data);

	if (ret != 0) {
		dev_err(wm8944->dev, "Failed to register IRQ chip: %d\n", ret);
		return ret;
	}

	/* Enable top level interrupt if it was masked */
	wm8944_reg_write(wm8944, WM8944_IRQCFG, 0);

	return 0;
}

void wm8944_irq_exit(struct wm8944 *wm8944)
{
	regmap_del_irq_chip(wm8944->irq, wm8944->irq_data);
}
