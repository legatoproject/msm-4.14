/************
 *
 * $Id$
 *
 * Filename:  pinctrl-sierra.c
 *
 * Purpose:   Complete probe and remove function for bitmask ownership.
 *
 * Copyright: (c) 2019 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>

#include "qcom/pinctrl-msm.h"
#include <linux/sierra_bsudefs.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include "../gpio/gpiolib.h"

int sierra_pinctrl_probe(struct platform_device *pdev)
{
	struct gpio_chip	*chip = msm_pinctrl_get_gpio_chip(pdev);
	int			bsfeature = -1;
	u64			bsgpiomask;
	char			*featurestr;
	struct device_node*	np = chip->of_node;
	int			i;
	int			nmap;
	int			igpio;
	int			ibit;

	bsfeature = sierra_smem_get_factory_mode() == 1 ? 2:
		(bs_support_get (3) ? 1 : 0);
	/* Assign product specific GPIO mapping */
	bsgpiomask = bsgetgpioflag();
	/*
	 * AR set the bit to 1 when the GPIO if not usable or allocatable to APP core
	 * WP set the bit to 1 when the GPIO is allocated to APP core
	 * Invert the bit mask in case of AR to have a compatibility between AR and WP
	 */
	switch (bsfeature) {
		case 0:
			featurestr = "ar";
			bsgpiomask = ~bsgpiomask;
			break;
		case 1:
			featurestr = "wp";
			break;
		case 2:
			featurestr = "mft";
			break;
	};
	dev_info(&pdev->dev, "Feature \"%s\" (%d)\n", featurestr, bsfeature);
	dev_info(&pdev->dev, "Cores GPIO mask 0x%llx\n", bsgpiomask);
	chip->mask = kzalloc((ARCH_NR_GPIOS + sizeof(u64)*8 - 1) / (sizeof(u64)*8), GFP_KERNEL);
	if (!chip->mask)
		return -ENOMEM;
	chip->mask[0] = bsgpiomask;
	chip->max_bit = -1;

	for (igpio = 0; igpio < chip->ngpio; igpio++) {
		struct gpio_desc	*desc;

		desc = gpio_to_desc(igpio);
		if (desc)
			desc->bit_in_mask = -1;
	}
	nmap = of_property_count_u32_elems(np, "gpio-bit-map");
	dev_err(&pdev->dev, "gpio-bit-map = %d\n", nmap);
	for (i = 0; i < nmap; i += 2) {
		struct gpio_desc	*desc;

		/*
		 * Read GPIO num and bit to test
		 */
		of_property_read_u32_index(np, "gpio-bit-map", i, &igpio);
		ibit = -1;
		of_property_read_u32_index(np, "gpio-bit-map", i+1, &ibit);
		desc = gpio_to_desc(igpio);
		if (desc && (ibit >= 0) && (ibit < chip->ngpio))
			desc->bit_in_mask = ibit;
		dev_dbg(&pdev->dev, "gpio-bit-map = <%d %d>\n", igpio, ibit);
	}
	chip->bitmask_valid = true;
	igpio = -1;
	if (0 == (nmap = of_property_read_u32_index(np, "gpio-RI", 0, &igpio))) {
		struct gpio_desc	*desc;

		desc = gpio_to_desc(igpio);
		if (desc) {
			set_bit(FLAG_RING_INDIC, &desc->flags);
			dev_info(&pdev->dev, "RI is GPIO %d\n", igpio);
		}
		else
			dev_err(&pdev->dev, "Invalid GPIO %d for RI\n", igpio);
	}

	return 0;
}

int sierra_pinctrl_remove(struct platform_device *pdev)
{
	struct gpio_chip	*chip = msm_pinctrl_get_gpio_chip(pdev);

	if (chip->bitmask_valid) {
		kfree(chip->mask);
		chip->mask = NULL;
		chip->bitmask_valid = false;
	}
	return 0;
}
