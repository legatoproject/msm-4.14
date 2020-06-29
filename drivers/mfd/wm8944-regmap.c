/*
 * wm8944-regmap.c  --  Register map data for WM8944 series devices
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 * based on wm8994-regmap.c
 *     Copyright 2011 Wolfson Microelectronics PLC.
 *     Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/mfd/wm8944/core.h>
#include <linux/mfd/wm8944/registers.h>
#include <linux/regmap.h>
#include <linux/device.h>

#include "wm8944.h"

static struct reg_default wm8944_defaults[] = {
	{0x01, 0x0001}, /* R1   - Chip Revision (RO) */
	{0x02, 0x0000}, /* R2   - Power 1 */
	{0x03, 0x0310}, /* R3   - Power 2 */
	{0x04, 0x020A}, /* R4   - Interface control */
	{0x05, 0x0000}, /* R5   - Companding Control */
	{0x06, 0x0106}, /* R6   - Clock control */
	{0x07, 0x800D}, /* R7   - Auto Increment Control */
	{0x08, 0x0102}, /* R8   - FLL control 1 */
	{0x09, 0x3127}, /* R9   - FLL control 2 */
	{0x0A, 0x0100}, /* R10  - FLL control 3 */
	{0x0B, 0x0000}, /* R11  - GPIO Config */
	{0x0D, 0x8000}, /* R13  - GPIO1 Control */
	{0x0E, 0xC000}, /* R14  - GPIO2 Control */
	{0x10, 0x0000}, /* R16  - System Interrupts (RO)*/
	{0x11, 0x0000}, /* R17  - Status Flags (RO)*/
	{0x12, 0x0001}, /* R18  - IRQ Config */
	{0x13, 0x0000}, /* R19  - System Interrupts Mask */
	{0x14, 0x0001}, /* R20  - Control Interface */
	{0x15, 0x0010}, /* R21  - DAC Control 1 */
	{0x16, 0x0010}, /* R22  - DAC Control 2 */
	{0x17, 0x01C0}, /* R23  - DAC Volume (digital) */
	{0x19, 0x0100}, /* R25  - ADC Control 1 */
	{0x1A, 0x0021}, /* R26  - ADC Control 2 */
	{0x1B, 0x00C0}, /* R27  - Left ADC Volume (digital) */
	{0x1C, 0x00C0}, /* R28  - Right ADC Volume (digital) */
	{0x1D, 0x000F}, /* R29  - DRC Control 1 */
	{0x1E, 0x0C25}, /* R30  - DRC Control 2 */
	{0x1F, 0x0342}, /* R31  - DRC Control 3 */
	{0x20, 0x0000}, /* R32  - DRC Control 4 */
	{0x21, 0x0003}, /* R33  - DRC Control 5 */
	{0x22, 0x0000}, /* R34  - DRC Control 6 */
	{0x23, 0x0000}, /* R35  - DRC Control 7 */
	{0x24, 0x0000}, /* R36  - DRC Status (RO) */
	{0x25, 0x0002}, /* R37  - Beep Control 1 */
	{0x26, 0x001C}, /* R38  - Video Buffer */
	{0x27, 0x0001}, /* R39  - Input Control */
	{0x28, 0x0050}, /* R40  - Input PGA Gain Control */
	{0x2A, 0x8100}, /* R42  - Output Control */
	{0x2B, 0x0000}, /* R43  - Speaker mixer Control 1 */
	{0x2C, 0x0000}, /* R44  - Speaker mixer Control 2 */
	{0x2D, 0x0000}, /* R45  - Speaker mixer Control 3 */
	{0x2E, 0x0000}, /* R46  - Speaker mixer Control 4 */
	{0x2F, 0x0079}, /* R47  - Speaker Volume Control */
	{0x31, 0x0000}, /* R49  - Liner mixer Control 1 */
	{0x33, 0x0000}, /* R51  - Liner L mixer Control 2 */
	{0x35, 0x0007}, /* R53  - LDO */
	{0x36, 0x000A}, /* R54  - BandGap */
	{0x40, 0x0000}, /* R64  - SE Config Selection */
	{0x41, 0x0000}, /* R65  - SE1_LHPF_CONFIG */
	{0x42, 0x0000}, /* R66  - SE1_LHPF_L */
	{0x43, 0x0000}, /* R67  - SE1_LHPF_R */
	{0x44, 0x0000}, /* R68  - SE1_3D_CONFIG */
	{0x45, 0x0408}, /* R69  - SE1_3D_L */
	{0x46, 0x0408}, /* R70  - SE1_3D_R */
	{0x47, 0x0000}, /* R71  - SE1_NOTCH_CONFIG */
	{0x48, 0x0000}, /* R72  - SE1_NOTCH_A10 */
	{0x49, 0x0000}, /* R73  - SE1_NOTCH_A11 */
	{0x4A, 0x0000}, /* R74  - SE1_NOTCH_A20 */
	{0x4B, 0x0000}, /* R75  - SE1_NOTCH_A21 */
	{0x4C, 0x0000}, /* R76  - SE1_NOTCH_A30 */
	{0x4D, 0x0000}, /* R77  - SE1_NOTCH_A31 */
	{0x4E, 0x0000}, /* R78  - SE1_NOTCH_A40 */
	{0x4F, 0x0000}, /* R79  - SE1_NOTCH_A41 */
	{0x50, 0x0000}, /* R80  - SE1_NOTCH_A50 */
	{0x51, 0x0000}, /* R81  - SE1_NOTCH_A51 */
	{0x52, 0x0000}, /* R82  - SE1_NOTCH_M10 */
	{0x53, 0x1000}, /* R83  - SE1_NOTCH_M11 */
	{0x54, 0x0000}, /* R84  - SE1_NOTCH_M20 */
	{0x55, 0x1000}, /* R85  - SE1_NOTCH_M21 */
	{0x56, 0x0000}, /* R86  - SE1_NOTCH_M30 */
	{0x57, 0x1000}, /* R87  - SE1_NOTCH_M31 */
	{0x58, 0x0000}, /* R88  - SE1_NOTCH_M40 */
	{0x59, 0x1000}, /* R89  - SE1_NOTCH_M41 */
	{0x5A, 0x0000}, /* R90  - SE1_NOTCH_M50 */
	{0x5B, 0x1000}, /* R91  - SE1_NOTCH_M51 */
	{0x5C, 0x0000}, /* R92  - SE1_DF1_CONFIG */
	{0x5D, 0x1000}, /* R93  - SE1_DF1_L0 */
	{0x5E, 0x0000}, /* R94  - SE1_DF1_L1 */
	{0x5F, 0x0000}, /* R95  - SE1_DF1_L2 */
	{0x60, 0x1000}, /* R96  - SE1_DF1_R0 */
	{0x61, 0x0000}, /* R97  - SE1_DF1_R1 */
	{0x62, 0x0000}, /* R98  - SE1_DF1_R2 */
	{0x63, 0x0000}, /* R99  - SE2_HPF_CONFIG */
	{0x64, 0x0000}, /* R100 - SE2_RETUNE_CONFIG */
	{0x65, 0x1000}, /* R101 - SE2_RETUNE_C0 */
	{0x66, 0x0000}, /* R102 - SE2_RETUNE_C1 */
	{0x67, 0x0000}, /* R103 - SE2_RETUNE_C2 */
	{0x68, 0x0000}, /* R104 - SE2_RETUNE_C3 */
	{0x69, 0x0000}, /* R105 - SE2_RETUNE_C4 */
	{0x6A, 0x0000}, /* R106 - SE2_RETUNE_C5 */
	{0x6B, 0x0000}, /* R107 - SE2_RETUNE_C6 */
	{0x6C, 0x0000}, /* R108 - SE2_RETUNE_C7 */
	{0x6D, 0x0000}, /* R109 - SE2_RETUNE_C8 */
	{0x6E, 0x0000}, /* R110 - SE2_RETUNE_C9 */
	{0x6F, 0x0000}, /* R111 - SE2_RETUNE_C10 */
	{0x70, 0x0000}, /* R112 - SE2_RETUNE_C11 */
	{0x71, 0x0000}, /* R113 - SE2_RETUNE_C12 */
	{0x72, 0x0000}, /* R114 - SE2_RETUNE_C13 */
	{0x73, 0x0000}, /* R115 - SE2_RETUNE_C14 */
	{0x74, 0x0000}, /* R116 - SE2_RETUNE_C15 */
	{0x75, 0x0000}, /* R117 - SE2_RETUNE_C16 */
	{0x76, 0x0000}, /* R118 - SE2_RETUNE_C17 */
	{0x77, 0x0000}, /* R119 - SE2_RETUNE_C18 */
	{0x78, 0x0000}, /* R120 - SE2_RETUNE_C19 */
	{0x79, 0x0000}, /* R121 - SE2_RETUNE_C20 */
	{0x7A, 0x0000}, /* R122 - SE2_RETUNE_C21 */
	{0x7B, 0x0000}, /* R123 - SE2_RETUNE_C22 */
	{0x7C, 0x0000}, /* R124 - SE2_RETUNE_C23 */
	{0x7D, 0x0000}, /* R125 - SE2_RETUNE_C24 */
	{0x7E, 0x0000}, /* R126 - SE2_RETUNE_C25 */
	{0x7F, 0x0000}, /* R127 - SE2_RETUNE_C26 */
	{0x80, 0x0000}, /* R128 - SE2_RETUNE_C27 */
	{0x81, 0x0000}, /* R129 - SE2_RETUNE_C28 */
	{0x82, 0x0000}, /* R130 - SE2_RETUNE_C29 */
	{0x83, 0x0000}, /* R131 - SE2_RETUNE_C30 */
	{0x84, 0x0000}, /* R132 - SE2_RETUNE_C31 */
	{0x85, 0x0000}, /* R133 - SE2_5BEQ_CONFIG */
	{0x86, 0x0C0C}, /* R134 - SE2_5BEQ_L10G */
	{0x87, 0x0C0C}, /* R135 - SE2_5BEQ_L32G */
	{0x88, 0x000C}, /* R136 - SE2_5BEQ_L4G */
	{0x89, 0x00D8}, /* R137 - SE2_5BEQ_L0P */
	{0x8A, 0x0FCA}, /* R138 - SE2_5BEQ_L0A */
	{0x8B, 0x0400}, /* R139 - SE2_5BEQ_L0B */
	{0x8C, 0x01C5}, /* R140 - SE2_5BEQ_L1P */
	{0x8D, 0x1EB5}, /* R141 - SE2_5BEQ_L1A */
	{0x8E, 0xF145}, /* R142 - SE2_5BEQ_L1B */
	{0x8F, 0x0B75}, /* R143 - SE2_5BEQ_L1C */
	{0x90, 0x0558}, /* R144 - SE2_5BEQ_L2P */
	{0x91, 0x1C58}, /* R145 - SE2_5BEQ_L2A */
	{0x92, 0xF373}, /* R146 - SE2_5BEQ_L2B */
	{0x93, 0x0A54}, /* R147 - SE2_5BEQ_L2C */
	{0x94, 0x1103}, /* R148 - SE2_5BEQ_L3P */
	{0x95, 0x168E}, /* R149 - SE2_5BEQ_L3A */
	{0x96, 0xF829}, /* R150 - SE2_5BEQ_L3B */
	{0x97, 0x07AD}, /* R151 - SE2_5BEQ_L3C */
	{0x98, 0x4000}, /* R152 - SE2_5BEQ_L4P */
	{0x99, 0x0564}, /* R153 - SE2_5BEQ_L4A */
	{0x9A, 0x0559}, /* R154 - SE2_5BEQ_L4B */
	{0x9B, 0x0C0C}, /* R155 - SE2_5BEQ_R10G */
	{0x9C, 0x0C0C}, /* R156 - SE2_5BEQ_R32G */
	{0x9D, 0x000C}, /* R157 - SE2_5BEQ_R4G */
	{0x9E, 0x00D8}, /* R158 - SE2_5BEQ_R0P */
	{0x9F, 0x0FCA}, /* R159 - SE2_5BEQ_R0A */
	{0xA0, 0x0400}, /* R160 - SE2_5BEQ_R0B */
	{0xA1, 0x01C5}, /* R161 - SE2_5BEQ_R1P */
	{0xA2, 0x1EB5}, /* R162 - SE2_5BEQ_R1A */
	{0xA3, 0xF145}, /* R163 - SE2_5BEQ_R1B */
	{0xA4, 0x0B75}, /* R164 - SE2_5BEQ_R1C */
	{0xA5, 0x0558}, /* R165 - SE2_5BEQ_R2P */
	{0xA6, 0x1C58}, /* R166 - SE2_5BEQ_R2A */
	{0xA7, 0xF373}, /* R167 - SE2_5BEQ_R2B */
	{0xA8, 0x0A54}, /* R168 - SE2_5BEQ_R2C */
	{0xA9, 0x1103}, /* R169 - SE2_5BEQ_R3P */
	{0xAA, 0x168E}, /* R170 - SE2_5BEQ_R3A */
	{0xAB, 0xF829}, /* R171 - SE2_5BEQ_R3B */
	{0xAC, 0x07AD}, /* R172 - SE2_5BEQ_R3C */
	{0xAD, 0x4000}, /* R173 - SE2_5BEQ_R4P */
	{0xAE, 0x0564}, /* R174 - SE2_5BEQ_R4A */
	{0xAF, 0x0559}, /* R175 - SE2_5BEQ_R4B */
};

static bool wm8944_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8944_SOFTRESET:
	case WM8944_CHIPVERSION:
	case WM8944_POWER1:
	case WM8944_POWER2:
	case WM8944_IFACE:
	case WM8944_COMPANDINGCTL:
	case WM8944_CLOCK:
	case WM8944_ADDCNTRL:
	case WM8944_FLLCTL1:
	case WM8944_FLLCTL2:
	case WM8944_FLLCTL3:
	case WM8944_GPIOCFG:
	case WM8944_GPIO1CTL:
	case WM8944_GPIO2CTL:
	case WM8944_SYSIT:
	case WM8944_STATUSFLAG:
	case WM8944_IRQCFG:
	case WM8944_SYSITMSK:
	case WM8944_CTLINT:
	case WM8944_DACCTL1:
	case WM8944_DACCTL2:
	case WM8944_DACVOL:
	case WM8944_ADCCTL1:
	case WM8944_ADCCTL2:
	case WM8944_LADCVOL:
	case WM8944_RADCVOL:
	case WM8944_DRCCTL1:
	case WM8944_DRCCTL2:
	case WM8944_DRCCTL3:
	case WM8944_DRCCTL4:
	case WM8944_DRCCTL5:
	case WM8944_DRCCTL6:
	case WM8944_DRCCTL7:
	case WM8944_DRCSTATUS:
	case WM8944_BEEPCTL1:
	case WM8944_VIDEOBUF:
	case WM8944_INPUTCTL:
	case WM8944_INPUTPGAGAINCTL:
	case WM8944_OUTPUTCTL:
	case WM8944_SPEAKMIXCTL1:
	case WM8944_SPEAKMIXCTL2:
	case WM8944_SPEAKMIXCTL3:
	case WM8944_SPEAKMIXCTL4:
	case WM8944_SPEAKVOL:
	case WM8944_LINEMIXCTL1:
	case WM8944_LINEMIXCTL2:
	case WM8944_LDO:
	case WM8944_BANDGAP:
	case WM8944_SECFG:
	case WM8944_SE1LHPFCFG:
	case WM8944_SE1LHPFL:
	case WM8944_SE1LHPFR:
	case WM8944_SE1L3DCFG:
	case WM8944_SE1L3DL:
	case WM8944_SE1L3DR:
	case WM8944_NOTCHCFG:
	case WM8944_NOTCHA10:
	case WM8944_NOTCHA11:
	case WM8944_NOTCHA20:
	case WM8944_NOTCHA21:
	case WM8944_NOTCHA30:
	case WM8944_NOTCHA31:
	case WM8944_NOTCHA40:
	case WM8944_NOTCHA41:
	case WM8944_NOTCHA50:
	case WM8944_NOTCHA51:
	case WM8944_NOTCHM10:
	case WM8944_NOTCHM11:
	case WM8944_NOTCHM20:
	case WM8944_NOTCHM21:
	case WM8944_NOTCHM30:
	case WM8944_NOTCHM31:
	case WM8944_NOTCHM40:
	case WM8944_NOTCHM41:
	case WM8944_NOTCHM50:
	case WM8944_NOTCHM51:
	case WM8944_SE1DF1CFG:
	case WM8944_SE1DF1L0:
	case WM8944_SE1DF1L1:
	case WM8944_SE1DF1L2:
	case WM8944_SE1DF1R0:
	case WM8944_SE1DF1R1:
	case WM8944_SE1DF1R2:
	case WM8944_SE2HPFCFG:
	case WM8944_SE2RETCFG:
	case WM8944_SE2RETC0:
	case WM8944_SE2RETC1:
	case WM8944_SE2RETC2:
	case WM8944_SE2RETC3:
	case WM8944_SE2RETC4:
	case WM8944_SE2RETC5:
	case WM8944_SE2RETC6:
	case WM8944_SE2RETC7:
	case WM8944_SE2RETC8:
	case WM8944_SE2RETC9:
	case WM8944_SE2RETC10:
	case WM8944_SE2RETC11:
	case WM8944_SE2RETC12:
	case WM8944_SE2RETC13:
	case WM8944_SE2RETC14:
	case WM8944_SE2RETC15:
	case WM8944_SE2RETC16:
	case WM8944_SE2RETC17:
	case WM8944_SE2RETC18:
	case WM8944_SE2RETC19:
	case WM8944_SE2RETC20:
	case WM8944_SE2RETC21:
	case WM8944_SE2RETC22:
	case WM8944_SE2RETC23:
	case WM8944_SE2RETC24:
	case WM8944_SE2RETC25:
	case WM8944_SE2RETC26:
	case WM8944_SE2RETC27:
	case WM8944_SE2RETC28:
	case WM8944_SE2RETC29:
	case WM8944_SE2RETC30:
	case WM8944_SE2RETC31:
	case WM8944_SE25BEQCFG:
	case WM8944_SE25BEQL10G:
	case WM8944_SE25BEQL32G:
	case WM8944_SE25BEQL4G:
	case WM8944_SE25BEQL0P:
	case WM8944_SE25BEQL0A:
	case WM8944_SE25BEQL0B:
	case WM8944_SE25BEQL1P:
	case WM8944_SE25BEQL1A:
	case WM8944_SE25BEQL1B:
	case WM8944_SE25BEQL1C:
	case WM8944_SE25BEQL2P:
	case WM8944_SE25BEQL2A:
	case WM8944_SE25BEQL2B:
	case WM8944_SE25BEQL2C:
	case WM8944_SE25BEQL3P:
	case WM8944_SE25BEQL3A:
	case WM8944_SE25BEQL3B:
	case WM8944_SE25BEQL3C:
	case WM8944_SE25BEQL4P:
	case WM8944_SE25BEQL4A:
	case WM8944_SE25BEQL4B:
	case WM8944_SE25BEQR10G:
	case WM8944_SE25BEQR32G:
	case WM8944_SE25BEQR4G:
	case WM8944_SE25BEQR0P:
	case WM8944_SE25BEQR0A:
	case WM8944_SE25BEQR0B:
	case WM8944_SE25BEQR1P:
	case WM8944_SE25BEQR1A:
	case WM8944_SE25BEQR1B:
	case WM8944_SE25BEQR1C:
	case WM8944_SE25BEQR2P:
	case WM8944_SE25BEQR2A:
	case WM8944_SE25BEQR2B:
	case WM8944_SE25BEQR2C:
	case WM8944_SE25BEQR3P:
	case WM8944_SE25BEQR3A:
	case WM8944_SE25BEQR3B:
	case WM8944_SE25BEQR3C:
	case WM8944_SE25BEQR4P:
	case WM8944_SE25BEQR4A:
	case WM8944_SE25BEQR4B:
		return true;
	default:
		return false;
	}
}


static bool wm8944_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8944_SOFTRESET:
	case WM8944_CHIPVERSION:
	case WM8944_DRCSTATUS:
	case WM8944_SYSIT:
	case WM8944_STATUSFLAG:
		return true;
	default:
		return false;
	}
}

static bool wm8944_writable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8944_CHIPVERSION:
	case WM8944_DRCSTATUS:
	case WM8944_SYSIT:
	case WM8944_STATUSFLAG:
		return false;
	default:
		return wm8944_readable_register(dev, reg);
	}
}

struct regmap_config wm8944_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,

	.cache_type = REGCACHE_RBTREE,

	.reg_defaults = wm8944_defaults,
	.num_reg_defaults = ARRAY_SIZE(wm8944_defaults),

	.max_register = WM8944_MAX_REGISTER,
	.volatile_reg = wm8944_volatile_register,
	.readable_reg = wm8944_readable_register,
	.writeable_reg = wm8944_writable_register,
};

struct regmap_config wm8944_base_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
};
