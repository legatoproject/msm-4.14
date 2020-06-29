/*
 * wm8944.h -- WM8944 MFD internals
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 * based on wm8994.h
 *     Copyright 2011 Wolfson Microelectronics PLC.
 *     Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM8944_H
#define __MFD_WM8944_H

#include <linux/regmap.h>

extern struct regmap_config wm8944_regmap_config;
extern struct regmap_config wm8944_base_regmap_config;

#endif
