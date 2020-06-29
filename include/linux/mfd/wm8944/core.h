/*
 * include/linux/mfd/wm8944/core.h -- Core interface for WM8944
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 * Based on include/linux/mfd/wm8994/core.h
 *     Copyright 2009 Wolfson Microelectronics PLC.
 *     Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM8944_CORE_H__
#define __MFD_WM8944_CORE_H__

#include <linux/mutex.h>
#include <linux/interrupt.h>


struct regulator_dev;
struct regulator_bulk_data;
struct regmap;

#define WM8944_NUM_GPIO_REGS  2
#define WM8944_NUM_LDO_REGS   1
#define WM8944_NUM_IRQ_REGS   1

#define WM8944_IRQ_LDO_UV     0
#define WM8944_IRQ_GP1       12
#define WM8944_IRQ_GP2       13
#define WM8944_IRQ_TEMP      15
#define WM8944_IRQ_RANGE     WM8944_IRQ_TEMP + 1

#define WM8944_INTERFACE_TYPE_UNKNOWN  -1
#define WM8944_INTERFACE_TYPE_NONE      0
#define WM8944_INTERFACE_TYPE_I2C       1

struct wm8944 {
	//struct mutex irq_lock;

	int revision;

	struct device *dev;
	struct regmap *regmap;

	int gpio_base;
	int irq_base;

	int irq;
	struct regmap_irq_chip_data *irq_data;

	/* Used over suspend/resume */
	bool suspended;

	struct regulator_dev *dbvdd;
	int num_supplies;
	struct regulator_bulk_data *supplies;
};

/* Device I/O API */
int wm8944_reg_read  (struct wm8944 *wm8944, unsigned short reg);
int wm8944_reg_write (struct wm8944 *wm8944, unsigned short reg,
		      unsigned short val);
int wm8944_set_bits  (struct wm8944 *wm8944, unsigned short reg,
		      unsigned short mask, unsigned short val);
int wm8944_bulk_read (struct wm8944 *wm8944, unsigned short reg,
		      int count, u16 *buf);
int wm8944_bulk_write(struct wm8944 *wm8944, unsigned short reg,
		      int count, const u16 *buf);


/* Helper to save on boilerplate */
static inline int wm8944_request_irq(struct wm8944 *wm8944, int irq,
				     irq_handler_t handler, const char *name,
				     void *data)
{
	if (!wm8944->irq_base)
		return -EINVAL;
	return request_threaded_irq(wm8944->irq_base + irq, NULL, handler,
				    IRQF_TRIGGER_RISING, name,
				    data);
}
static inline void wm8944_free_irq(struct wm8944 *wm8944, int irq, void *data)
{
	if (!wm8944->irq_base)
		return;
	free_irq(wm8944->irq_base + irq, data);
}

int wm8944_irq_init(struct wm8944 *wm8944);
void wm8944_irq_exit(struct wm8944 *wm8944);
int wm8944_get_intf_type(void);

#endif
