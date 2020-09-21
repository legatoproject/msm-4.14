/*
 * Copyright (C) 2017 Sierra Wireless, Inc
 * Author: Zoran Markovic <zmarkovic@sierrawireless.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/bug.h>
#include <linux/sierra_serial.h>

typedef struct uart_function_triplet_ {
	enum bs_uart_line_e line;
	enum bs_uart_type_e speed;
	enum bs_uart_func_e func;
} uart_function_triplet_t;

uart_function_triplet_t valid_triplets[] = {
	/* valid functions for UART1, high-speed */
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_AT},
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_NMEA},
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_APP},
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_DISABLED},
	/* valid functions for UART1, low-speed */
	{BS_UART1_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_AT},
	{BS_UART1_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_DM},
	/* valid functions for UART2, low-speed */
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_AT},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_DM},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_NMEA},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_APP},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_CONSOLE},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_DISABLED},
};
#define TRIPLET_MATCH(triplet, l, s, f) \
	((triplet)->line == l && \
	(triplet)->speed == s && \
	(triplet)->func == f)

#define LINE_AND_SPEED_MATCH(triplet, l, s) \
	((triplet)->line == l && \
	(triplet)->speed == s)

uart_function_triplet_t default_functions[] = {
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_INVALID},
	{BS_UART1_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_DISABLED},
	{BS_UART2_LINE, BS_UART_TYPE_HSL, BS_UART_FUNC_CONSOLE},
	{BS_UART2_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_DISABLED}
};

static inline enum bs_uart_func_e default_function(enum bs_uart_line_e line,
						enum bs_uart_type_e speed)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(default_functions) &&
	    !LINE_AND_SPEED_MATCH(&(default_functions[i]), line, speed); i++);
	BUG_ON(i >= ARRAY_SIZE(valid_triplets)); /* not found */

	return default_functions[i].func;
}

static enum bs_uart_func_e assign_function(enum bs_uart_line_e line,
					   enum bs_uart_type_e speed,
					   enum bs_uart_func_e f)
{
	int i;

	/* Check if configured function is allowed for line/speed */
	for (i = 0; i < ARRAY_SIZE(valid_triplets) &&
	    !TRIPLET_MATCH(&(valid_triplets[i]), line, speed, f); i++);

	if  (ARRAY_SIZE(valid_triplets) > i)
		/* Allowed, use function */
		return valid_triplets[i].func;
	else
		/* Not allowed, use default function */
		return default_function(line, speed);
}

static ssize_t uart_config_store(struct device *dev,
				 struct device_attribute *a, char *buf)
{
	enum bs_uart_func_e f;
	enum bs_uart_type_e speed;
	int line;

	/* Low speed UARTs are aliased "serial<N>" */
	line = of_alias_get_id(dev->of_node, "serial");
	if (0 > line) {
		/* Not low speed. High-speed UARTs are aliased "uart<N>" */
		line = of_alias_get_id(dev->of_node, "uart");
		BUG_ON(0 > line); /* No alias for this UART, fail */
		speed = BS_UART_TYPE_HS;
	} else {
		speed = BS_UART_TYPE_HSL;
	}

	/* Read and check UART configuration */
	f = bs_uart_fun_get(line);
	if (f < BS_UART_FUNC_DISABLED || f >= BS_UART_FUNC_MAX)
		f = BS_UART_FUNC_DISABLED;
	else
		f = assign_function(line, speed, f);

	/* Write function string to buffer */
	strcpy(buf, bs_uart_func_name(f));
	return strlen(buf);
}

static DEVICE_ATTR(config, S_IRUSR| S_IRGRP| S_IROTH, uart_config_store, NULL);

void uart_create_sysfs_config(struct device *dev)
{
	if (0 > device_create_file(dev, &dev_attr_config))
		dev_err(dev, "Cannot create sysfs config file\n");
}
