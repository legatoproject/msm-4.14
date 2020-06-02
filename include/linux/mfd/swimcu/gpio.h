/*
 * gpio.h  --  GPIO Driver for Sierra Wireless WP76xx MCU
 *
 * adapted from:
 * gpio.h  --  GPIO Driver for Wolfson WM8350 PMIC
 *
 * Copyright (c) 2016 Sierra Wireless, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_SWIMCU_GPIO_H_
#define __LINUX_MFD_SWIMCU_GPIO_H_

#include <linux/platform_device.h>

#define SWIMCU_GPIO_NOOP	0
#define SWIMCU_GPIO_GET_DIR	1
#define SWIMCU_GPIO_SET_DIR	2
#define SWIMCU_GPIO_GET_VAL	3
#define SWIMCU_GPIO_SET_VAL	4
#define SWIMCU_GPIO_SET_PULL	5
#define SWIMCU_GPIO_SET_EDGE	6

/*
 * GPIO Port index.
 * Supported GPIOs ordered according to external GPIO mapping.
 * PTA12 is reserved for ADC.
 */
enum swimcu_gpio_index
{
	SWIMCU_GPIO_FIRST = 0,
	SWIMCU_GPIO_PTA0 = SWIMCU_GPIO_FIRST, /* GPIO36 */
	SWIMCU_GPIO_PTA2, /* GPIO37 */
	SWIMCU_GPIO_PTB0, /* GPIO38 */
	SWIMCU_GPIO_PTA6, /* GPIO40 */
	SWIMCU_GPIO_PTA5, /* GPIO41 */
	SWIMCU_GPIO_LAST = SWIMCU_GPIO_PTA5,
	SWIMCU_NUM_GPIO = SWIMCU_GPIO_LAST+1,
	SWIMCU_GPIO_INVALID = SWIMCU_NUM_GPIO,
};

enum swimcu_gpio_irq_index
{
	SWIMCU_GPIO_NO_IRQ = -1,  /* GPIO does not support IRQ */
	SWIMCU_GPIO_PTA0_IRQ = 0, /* GPIO36 */
	SWIMCU_GPIO_PTB0_IRQ,     /* GPIO38 */
	SWIMCU_NUM_GPIO_IRQ
};

/*
 * MCU GPIO map port/pin to gpio index.
 */
enum swimcu_gpio_index swimcu_get_gpio_from_port_pin(int port, int pin);

enum swimcu_gpio_index swimcu_get_gpio_from_irq(enum swimcu_gpio_irq_index irq);

enum swimcu_gpio_irq_index swimcu_get_irq_from_gpio(enum swimcu_gpio_index gpio);

struct swimcu;

struct swimcu_gpio {
	struct platform_device *pdev;
};

void swimcu_gpio_callback(struct swimcu *swimcu, int port, int pin, int level);

int swimcu_gpio_open(struct swimcu *swimcu, int gpio);

int swimcu_gpio_close(struct swimcu *swimcu, int gpio);

void swimcu_gpio_refresh( struct swimcu *swimcu );

void swimcu_gpio_retrieve( struct swimcu *swimcu );

int swimcu_gpio_get(struct swimcu *swimcu, int action, int gpio, int *value);

int swimcu_gpio_set(struct swimcu *swimcu, int action, int gpio, int value);

#ifndef CONFIG_MSM_SWI_QEMU
void swimcu_gpio_work(struct swimcu *swimcu, enum swimcu_gpio_irq_index irq);
#else
/* This function doesn't exist for QEMU - just fake it. */
static inline void swimcu_gpio_work(struct swimcu *swimcu, enum swimcu_gpio_irq_index irq) {}
#endif

#endif
