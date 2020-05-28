/* kernel/include/linux/sierra_bsuproto.h
 *
 * Copyright (C) 2013 Sierra Wireless, Inc
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

#ifndef SIERRA_BS_UPROTO_H
#define SIERRA_BS_UPROTO_H

extern uint64_t bsgetgpioflag(void);
extern bool bsgethsicflag(void);
extern enum bshwtype bs_hwtype_get(void);
extern enum bs_prod_family_e bs_prod_family_get (void);
extern bool bs_support_get (enum bsfeature feature);
extern int8_t bs_uart_fun_get (uint uart_num);
extern int8_t bsgetriowner(void);
extern uint8_t bs_hwrev_get(void);

extern int sierra_smem_get_factory_mode(void);
#endif /* SIERRA_BSUPROTO_H */
