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
#include <linux/sierra_serial.h>

typedef struct uart_function_triplet_ {
	enum bs_uart_line_e line;
	enum bs_uart_type_e speed;
	enum bs_uart_func_e func;
} uart_function_triplet_t;

/* Attention! the below mapping should be unambiguous. A given service
 * on a given line should map only to a single speed.
 */
uart_function_triplet_t valid_triplets[] = {
	/* valid functions for UART1, high-speed */
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_AT},
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_NMEA},
	{BS_UART1_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_APP},
	/* valid functions for UART1, low-speed */
	{BS_UART1_LINE, BS_UART_TYPE_LS, BS_UART_FUNC_DM},
	{BS_UART1_LINE, BS_UART_TYPE_LS, BS_UART_FUNC_CONSOLE},
	/* valid functions for UART2, high-speed */
	{BS_UART2_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_AT},
	{BS_UART2_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_NMEA},
	{BS_UART2_LINE, BS_UART_TYPE_HS, BS_UART_FUNC_APP},
	/* valid functions for UART2, low-speed */
	{BS_UART2_LINE, BS_UART_TYPE_LS, BS_UART_FUNC_DM},
	{BS_UART2_LINE, BS_UART_TYPE_LS, BS_UART_FUNC_CONSOLE},
};
#define TRIPLET_MATCH(triplet, l, s, f) \
	((triplet)->line == l && \
	(triplet)->speed == s && \
	(triplet)->func == f)

#define ALIAS_STEM_LS "serial" /* Low speed UARTs are aliased "serial<N>" */
#define ALIAS_STEM_HS "uart" /* High-speed UARTs are aliased "uart<N>" */

static enum bs_uart_func_e assign_function(struct device *dev)
{
	int i;
	enum bs_uart_func_e f;
	enum bs_uart_type_e speed;
	int line;
	bool found = false;

	line = of_alias_get_id(dev->of_node, ALIAS_STEM_LS);
	if (0 > line) {
		/* Not low-speed. Check for high-speed. */
		line = of_alias_get_id(dev->of_node, ALIAS_STEM_HS);
		if (unlikely(0 > line))
			dev_err(dev, "No alias for this UART!\n");
		speed = BS_UART_TYPE_HS;
	} else {
		speed = BS_UART_TYPE_LS;
	}

	if (unlikely(0 > line) || (line >= BS_UART_LINE_MAX)) {
		dev_err(dev, "Failed to retrieve line from uart dev. line=%d.\n", line);
		return BS_UART_FUNC_INVALID;
	}

	/* Read and check UART configuration */
	f = bs_uart_fun_get(line);
	if (f <= BS_UART_FUNC_DISABLED || f >= BS_UART_FUNC_MAX) {
		/* UART is either disabled or with bad configuration; Assign  DISABLED.	*/
		pr_info("%s:%d is disabled.\n", dev_name(dev), line);
		return BS_UART_FUNC_DISABLED;
	}

	/* Check if configured function is allowed for line/speed */
	for (i = 0; i < ARRAY_SIZE(valid_triplets); i++) {
		if (TRIPLET_MATCH(&(valid_triplets[i]), line, speed, f)) {
			if (unlikely(found)){
				/* Ambiugous mappings are not allowed. */
				dev_err(dev, "Bug! UART service %s is mapped to more than one speed.\n", bs_uart_func_name(f));
				return BS_UART_FUNC_INVALID;
			}
			else {
				found = true;
			}
		}
	}

	if  (found) {
		/* Allowed- Use function */
		pr_info("%s:%d is reserved for %s.\n", dev_name(dev), line, bs_uart_func_name(f));
		return f;
	}
	else {
		/* Not allowed. Return INVALID.*/
		pr_info("Function, %s, is not valid on %s:%d.\n",
				bs_uart_func_name(f), dev_name(dev), line);
		return BS_UART_FUNC_INVALID;
	}

}

static ssize_t uart_config_show(struct device *dev,
				struct device_attribute *a, char *buf)
{
	enum bs_uart_func_e f;

	f = assign_function(dev);

	/* Write function string to buffer */
	strcpy(buf, bs_uart_func_name(f));
	return strlen(buf);
}

static DEVICE_ATTR(config, S_IRUSR| S_IRGRP| S_IROTH, uart_config_show, NULL);

int uart_create_sysfs_config(struct device *dev)
{
	enum bs_uart_func_e f;

	f = assign_function(dev);
	if ((f == BS_UART_FUNC_DISABLED) || (f == BS_UART_FUNC_INVALID))
	{
		dev_err(dev, "Disabled \n");
		return -1;
	}

        if (0 > device_create_file(dev, &dev_attr_config))
	{
		dev_err(dev, "Cannot create sysfs config file\n");
		return -1;
	}

	return 0;
}

bool uart_is_function_console(struct device *dev)
{
	return BS_UART_FUNC_CONSOLE == assign_function(dev);
}
