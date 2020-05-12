/* arch/arm/mach-msm/swimcu.h
 *
 * Copyright (C) 2016 Sierra Wireless, Inc
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

#ifndef __ARCH_ARM_MACH_MSM_SWIMCU_H
#define __ARCH_ARM_MACH_MSM_SWIMCU_H


/* Macro assumes SWIMCU GPIOs & ADCs start at 0 */
#define SWIMCU_GPIO_BASE                200
#define SWIMCU_GPIO_TO_SYS(mcu_gpio)    (mcu_gpio + SWIMCU_GPIO_BASE)
#define SWIMCU_NR_GPIOS                 8
#define SWIMCU_IS_GPIO(gpio)            ((gpio >= SWIMCU_GPIO_BASE) && (gpio < SWIMCU_GPIO_BASE + SWIMCU_NR_GPIOS))
#define SWIMCU_ADC_BASE                 2
#define SWIMCU_ADC_TO_SYS(mcu_adc)      (mcu_adc + SWIMCU_ADC_BASE)
#define SWIMCU_NR_ADCS                  2

#endif /*__ARCH_ARM_MACH_MSM_SWIMCU_H*/
