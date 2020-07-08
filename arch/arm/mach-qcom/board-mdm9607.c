/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include "board-dt.h"

#if defined(CONFIG_SIERRA) && defined(CONFIG_WLAN_VENDOR_TI)
#include <linux/wl12xx.h>
#include <linux/gpio.h>
#include <linux/sierra_gpio.h>
#endif

static const char *mdm9607_dt_match[] __initconst = {
	"qcom,mdm9607",
	NULL
};

static void __init mdm9607_init(void)
{
	board_dt_populate(NULL);
}

#if defined(CONFIG_SIERRA) && defined(CONFIG_WLAN_VENDOR_TI)
/* sierra_gpio aliases, as defined in DTS; sans, "alias-" */
#define MSM_WIFI_IRQ_ALIAS_GPIO		"WIFI_IRQ"	/* IOT0_GPIO1 */
#define MSM_WLAN_EN_ALIAS_GPIO		"WLAN_EN"	/* IOT0_GPIO3 */

static void __init mdm9607_wl18xx_init(void)
{
	struct wl12xx_static_platform_data msm_wl12xx_pdata;
	int ret;
	struct gpio_desc *desc;

	memset(&msm_wl12xx_pdata, 0, sizeof(msm_wl12xx_pdata));

	if (gpio_alias_lookup(MSM_WLAN_EN_ALIAS_GPIO, &desc)) {
		pr_err("wl18xx: NO WLAN_EN gpio");
		goto fail;
	}
	msm_wl12xx_pdata.wlan_en = desc_to_gpio(desc);
	pr_info("wl12xx WLAN_EN GPIO: %d\n", msm_wl12xx_pdata.wlan_en);
	if (gpio_alias_lookup(MSM_WIFI_IRQ_ALIAS_GPIO, &desc)) {
		pr_err("wl18xx: NO WIFI_IRQ gpio");
		goto fail;
	}
	msm_wl12xx_pdata.irq = gpio_to_irq(desc_to_gpio(desc));
	pr_info("wl12xx IRQ: %d\n", msm_wl12xx_pdata.irq);
	if (msm_wl12xx_pdata.irq < 0)
		goto fail;

	msm_wl12xx_pdata.ref_clock_freq = 38400000;
	msm_wl12xx_pdata.tcxo_clock_freq = 19200000;

	ret = wl12xx_set_platform_data(&msm_wl12xx_pdata);
	if (ret < 0)
		goto fail;

	pr_info("wl18xx board initialization done\n");
	return;

fail:
	pr_err("%s: wl18xx board initialisation failed\n", __func__);
}
#endif


DT_MACHINE_START(MDM9607_DT,
	"Qualcomm Technologies, Inc. MDM 9607 (Flattened Device Tree)")
	.init_machine	= mdm9607_init,
	.dt_compat	= mdm9607_dt_match,
#if defined(CONFIG_SIERRA) && defined(CONFIG_WLAN_VENDOR_TI)
	.init_late      = mdm9607_wl18xx_init,
#endif
MACHINE_END
