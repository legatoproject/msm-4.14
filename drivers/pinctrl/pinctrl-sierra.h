/************
 *
 * $Id$
 *
 * Filename:  pinctrl-sierra.h
 *
 * Purpose:   Complete probe and remove function for bitmask ownership.
 *
 * Copyright: (c) 2019 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/
#include <linux/platform_device.h>

#ifdef CONFIG_SIERRA
extern int sierra_pinctrl_probe(struct platform_device *pdev);
extern int sierra_pinctrl_remove(struct platform_device *pdev);
#else
static inline sierra_pinctrl_probe(struct platform_device *pdev)
{
	return 0;
}

static inline sierra_pinctrl_remove(struct platform_device *pdev)
{
	return 0;
}
#endif
