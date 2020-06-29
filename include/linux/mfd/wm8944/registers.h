/*
 * include/linux/mfd/wm8944/registers.h -- Register definitions for WM8944
 *
 * Copyright 2015 Sierra Wireless
 *
 * Author: Jean Michel Chauvet <jchauvet@sierrawireless.com>,
 *         Gaetan Perrier <gperrier@sierrawireless.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM8944_REGISTERS_H__
#define __MFD_WM8944_REGISTERS_H__

/* WM8944 register space */
#define WM8944_SOFTRESET        0x00           /* Reset all registers to their default state */
#define WM8944_CHIPVERSION      0x01           /* Chip Revision (RO) */
#define WM8944_POWER1           0x02           /* Power 1 */
#define WM8944_POWER2           0x03           /* Power 2 */
#define WM8944_IFACE            0x04           /* Interface control */
#define WM8944_COMPANDINGCTL    0x05           /* Companding Control */
#define WM8944_CLOCK            0x06           /* Clock control */
#define WM8944_ADDCNTRL         0x07           /* Auto Increment Control */
#define WM8944_FLLCTL1          0x08           /* FLL control 1 */
#define WM8944_FLLCTL2          0x09           /* FLL control 2 */
#define WM8944_FLLCTL3          0x0A           /* FLL control 3 */
#define WM8944_GPIOCFG          0x0B           /* GPIO Config */
#define WM8944_GPIO1CTL         0x0D           /* GPIO1 Control */
#define WM8944_GPIO2CTL         0x0E           /* GPIO2 Control */
#define WM8944_SYSIT            0x10           /* System Interrupts (RO)*/
#define WM8944_STATUSFLAG       0x11           /* Status Flags (RO)*/
#define WM8944_IRQCFG           0x12           /* IRQ Config */
#define WM8944_SYSITMSK         0x13           /* System Interrupts Mask */
#define WM8944_CTLINT           0x14           /* Control Interface */
#define WM8944_DACCTL1          0x15           /* DAC Control 1 */
#define WM8944_DACCTL2          0x16           /* DAC Control 2 */
#define WM8944_DACVOL           0x17           /* DAC Volume (digital) */
#define WM8944_ADCCTL1          0x19           /* ADC Control 1 */
#define WM8944_ADCCTL2          0x1A           /* ADC Control 2 */
#define WM8944_LADCVOL          0x1B           /* Left ADC Volume (digital) */
#define WM8944_RADCVOL          0x1C           /* Right ADC Volume (digital) */
#define WM8944_DRCCTL1          0x1D           /* DRC Control 1 */
#define WM8944_DRCCTL2          0x1E           /* DRC Control 2 */
#define WM8944_DRCCTL3          0x1F           /* DRC Control 3 */
#define WM8944_DRCCTL4          0x20           /* DRC Control 4 */
#define WM8944_DRCCTL5          0x21           /* DRC Control 5 */
#define WM8944_DRCCTL6          0x22           /* DRC Control 6 */
#define WM8944_DRCCTL7          0x23           /* DRC Control 7 */
#define WM8944_DRCSTATUS        0x24           /* DRC Status (RO) */
#define WM8944_BEEPCTL1         0x25           /* Beep Control 1 */
#define WM8944_VIDEOBUF         0x26           /* Video Buffer */
#define WM8944_INPUTCTL         0x27           /* Input Control */
#define WM8944_INPUTPGAGAINCTL  0x28           /* Input PGA Gain Control */
#define WM8944_OUTPUTCTL        0x2A           /* Output Control */
#define WM8944_SPEAKMIXCTL1     0x2B           /* Speaker mixer Control 1 */
#define WM8944_SPEAKMIXCTL2     0x2C           /* Speaker mixer Control 2 */
#define WM8944_SPEAKMIXCTL3     0x2D           /* Speaker mixer Control 3 */
#define WM8944_SPEAKMIXCTL4     0x2E           /* Speaker mixer Control 4 */
#define WM8944_SPEAKVOL         0x2F           /* Speaker Volume Control */
#define WM8944_LINEMIXCTL1      0x31           /* Liner mixer Control 1 */
#define WM8944_LINEMIXCTL2      0x33           /* Liner L mixer Control 2 */
#define WM8944_LDO              0x35           /* LDO */
#define WM8944_BANDGAP          0x36           /* BandGap */
#define WM8944_SECFG            0x40           /* SE Config Selection */
#define WM8944_SE1LHPFCFG       0x41           /* SE1_LHPF_CONFIG */
#define WM8944_SE1LHPFL         0x42           /* SE1_LHPF_L */
#define WM8944_SE1LHPFR         0x43           /* SE1_LHPF_R */
#define WM8944_SE1L3DCFG        0x44           /* SE1_3D_CONFIG */
#define WM8944_SE1L3DL          0x45           /* SE1_3D_L */
#define WM8944_SE1L3DR          0x46           /* SE1_3D_R */
#define WM8944_NOTCHCFG         0x47           /* SE1_NOTCH_CONFIG */
#define WM8944_NOTCHA10         0x48           /* SE1_NOTCH_A10 */
#define WM8944_NOTCHA11         0x49           /* SE1_NOTCH_A11 */
#define WM8944_NOTCHA20         0x4A           /* SE1_NOTCH_A20 */
#define WM8944_NOTCHA21         0x4B           /* SE1_NOTCH_A21 */
#define WM8944_NOTCHA30         0x4C           /* SE1_NOTCH_A30 */
#define WM8944_NOTCHA31         0x4D           /* SE1_NOTCH_A31 */
#define WM8944_NOTCHA40         0x4E           /* SE1_NOTCH_A40 */
#define WM8944_NOTCHA41         0x4F           /* SE1_NOTCH_A41 */
#define WM8944_NOTCHA50         0x50           /* SE1_NOTCH_A50 */
#define WM8944_NOTCHA51         0x51           /* SE1_NOTCH_A51 */
#define WM8944_NOTCHM10         0x52           /* SE1_NOTCH_M10 */
#define WM8944_NOTCHM11         0x53           /* SE1_NOTCH_M11 */
#define WM8944_NOTCHM20         0x54           /* SE1_NOTCH_M20 */
#define WM8944_NOTCHM21         0x55           /* SE1_NOTCH_M21 */
#define WM8944_NOTCHM30         0x56           /* SE1_NOTCH_M30 */
#define WM8944_NOTCHM31         0x57           /* SE1_NOTCH_M31 */
#define WM8944_NOTCHM40         0x58           /* SE1_NOTCH_M40 */
#define WM8944_NOTCHM41         0x59           /* SE1_NOTCH_M41 */
#define WM8944_NOTCHM50         0x5A           /* SE1_NOTCH_M50 */
#define WM8944_NOTCHM51         0x5B           /* SE1_NOTCH_M51 */
#define WM8944_SE1DF1CFG        0x5C           /* SE1_DF1_CONFIG */
#define WM8944_SE1DF1L0         0x5D           /* SE1_DF1_L0 */
#define WM8944_SE1DF1L1         0x5E           /* SE1_DF1_L1 */
#define WM8944_SE1DF1L2         0x5F           /* SE1_DF1_L2 */
#define WM8944_SE1DF1R0         0x60           /* SE1_DF1_R0 */
#define WM8944_SE1DF1R1         0x61           /* SE1_DF1_R1 */
#define WM8944_SE1DF1R2         0x62           /* SE1_DF1_R2 */
#define WM8944_SE2HPFCFG        0x63           /* SE2_HPF_CONFIG */
#define WM8944_SE2RETCFG        0x64           /* SE2_RETUNE_CONFIG */
#define WM8944_SE2RETC0         0x65           /* SE2_RETUNE_C0 */
#define WM8944_SE2RETC1         0x66           /* SE2_RETUNE_C1 */
#define WM8944_SE2RETC2         0x67           /* SE2_RETUNE_C2 */
#define WM8944_SE2RETC3         0x68           /* SE2_RETUNE_C3 */
#define WM8944_SE2RETC4         0x69           /* SE2_RETUNE_C4 */
#define WM8944_SE2RETC5         0x6A           /* SE2_RETUNE_C5 */
#define WM8944_SE2RETC6         0x6B           /* SE2_RETUNE_C6 */
#define WM8944_SE2RETC7         0x6C           /* SE2_RETUNE_C7 */
#define WM8944_SE2RETC8         0x6D           /* SE2_RETUNE_C8 */
#define WM8944_SE2RETC9         0x6E           /* SE2_RETUNE_C9 */
#define WM8944_SE2RETC10        0x6F           /* SE2_RETUNE_C10 */
#define WM8944_SE2RETC11        0x70           /* SE2_RETUNE_C11 */
#define WM8944_SE2RETC12        0x71           /* SE2_RETUNE_C12 */
#define WM8944_SE2RETC13        0x72           /* SE2_RETUNE_C13 */
#define WM8944_SE2RETC14        0x73           /* SE2_RETUNE_C14 */
#define WM8944_SE2RETC15        0x74           /* SE2_RETUNE_C15 */
#define WM8944_SE2RETC16        0x75           /* SE2_RETUNE_C16 */
#define WM8944_SE2RETC17        0x76           /* SE2_RETUNE_C17 */
#define WM8944_SE2RETC18        0x77           /* SE2_RETUNE_C18 */
#define WM8944_SE2RETC19        0x78           /* SE2_RETUNE_C19 */
#define WM8944_SE2RETC20        0x79           /* SE2_RETUNE_C20 */
#define WM8944_SE2RETC21        0x7A           /* SE2_RETUNE_C21 */
#define WM8944_SE2RETC22        0x7B           /* SE2_RETUNE_C22 */
#define WM8944_SE2RETC23        0x7C           /* SE2_RETUNE_C23 */
#define WM8944_SE2RETC24        0x7D           /* SE2_RETUNE_C24 */
#define WM8944_SE2RETC25        0x7E           /* SE2_RETUNE_C25 */
#define WM8944_SE2RETC26        0x7F           /* SE2_RETUNE_C26 */
#define WM8944_SE2RETC27        0x80           /* SE2_RETUNE_C27 */
#define WM8944_SE2RETC28        0x81           /* SE2_RETUNE_C28 */
#define WM8944_SE2RETC29        0x82           /* SE2_RETUNE_C29 */
#define WM8944_SE2RETC30        0x83           /* SE2_RETUNE_C30 */
#define WM8944_SE2RETC31        0x84           /* SE2_RETUNE_C31 */
#define WM8944_SE25BEQCFG       0x85           /* SE2_5BEQ_CONFIG */
#define WM8944_SE25BEQL10G      0x86           /* SE2_5BEQ_L10G */
#define WM8944_SE25BEQL32G      0x87           /* SE2_5BEQ_L32G */
#define WM8944_SE25BEQL4G       0x88           /* SE2_5BEQ_L4G */
#define WM8944_SE25BEQL0P       0x89           /* SE2_5BEQ_L0P */
#define WM8944_SE25BEQL0A       0x8A           /* SE2_5BEQ_L0A */
#define WM8944_SE25BEQL0B       0x8B           /* SE2_5BEQ_L0B */
#define WM8944_SE25BEQL1P       0x8C           /* SE2_5BEQ_L1P */
#define WM8944_SE25BEQL1A       0x8D           /* SE2_5BEQ_L1A */
#define WM8944_SE25BEQL1B       0x8E           /* SE2_5BEQ_L1B */
#define WM8944_SE25BEQL1C       0x8F           /* SE2_5BEQ_L1C */
#define WM8944_SE25BEQL2P       0x90           /* SE2_5BEQ_L2P */
#define WM8944_SE25BEQL2A       0x91           /* SE2_5BEQ_L2A */
#define WM8944_SE25BEQL2B       0x92           /* SE2_5BEQ_L2B */
#define WM8944_SE25BEQL2C       0x93           /* SE2_5BEQ_L2C */
#define WM8944_SE25BEQL3P       0x94           /* SE2_5BEQ_L3P */
#define WM8944_SE25BEQL3A       0x95           /* SE2_5BEQ_L3A */
#define WM8944_SE25BEQL3B       0x96           /* SE2_5BEQ_L3B */
#define WM8944_SE25BEQL3C       0x97           /* SE2_5BEQ_L3C */
#define WM8944_SE25BEQL4P       0x98           /* SE2_5BEQ_L4P */
#define WM8944_SE25BEQL4A       0x99           /* SE2_5BEQ_L4A */
#define WM8944_SE25BEQL4B       0x9A           /* SE2_5BEQ_L4B */
#define WM8944_SE25BEQR10G      0x9B           /* SE2_5BEQ_R10G */
#define WM8944_SE25BEQR32G      0x9C           /* SE2_5BEQ_R32G */
#define WM8944_SE25BEQR4G       0x9D           /* SE2_5BEQ_R4G */
#define WM8944_SE25BEQR0P       0x9E           /* SE2_5BEQ_R0P */
#define WM8944_SE25BEQR0A       0x9F           /* SE2_5BEQ_R0A */
#define WM8944_SE25BEQR0B       0xA0           /* SE2_5BEQ_R0B */
#define WM8944_SE25BEQR1P       0xA1           /* SE2_5BEQ_R1P */
#define WM8944_SE25BEQR1A       0xA2           /* SE2_5BEQ_R1A */
#define WM8944_SE25BEQR1B       0xA3           /* SE2_5BEQ_R1B */
#define WM8944_SE25BEQR1C       0xA4           /* SE2_5BEQ_R1C */
#define WM8944_SE25BEQR2P       0xA5           /* SE2_5BEQ_R2P */
#define WM8944_SE25BEQR2A       0xA6           /* SE2_5BEQ_R2A */
#define WM8944_SE25BEQR2B       0xA7           /* SE2_5BEQ_R2B */
#define WM8944_SE25BEQR2C       0xA8           /* SE2_5BEQ_R2C */
#define WM8944_SE25BEQR3P       0xA9           /* SE2_5BEQ_R3P */
#define WM8944_SE25BEQR3A       0xAA           /* SE2_5BEQ_R3A */
#define WM8944_SE25BEQR3B       0xAB           /* SE2_5BEQ_R3B */
#define WM8944_SE25BEQR3C       0xAC           /* SE2_5BEQ_R3C */
#define WM8944_SE25BEQR4P       0xAD           /* SE2_5BEQ_R4P */
#define WM8944_SE25BEQR4A       0xAE           /* SE2_5BEQ_R4A */
#define WM8944_SE25BEQR4B       0xAF           /* SE2_5BEQ_R4B */


#define WM8944_MAX_REGISTER     0xAF


/* Clock divider Id's */
#define WM8944_BCLKDIV          0
#define WM8944_MCLKDIV          1
#define WM8944_FLLCLKDIV        2
#define WM8944_FOCLKDIV         3

/* MCLK clock dividers */
#define WM8944_MCLKDIV_1        0
#define WM8944_MCLKDIV_1_5      1
#define WM8944_MCLKDIV_2        2
#define WM8944_MCLKDIV_3        3
#define WM8944_MCLKDIV_4        4
#define WM8944_MCLKDIV_6        5
#define WM8944_MCLKDIV_8        6
#define WM8944_MCLKDIV_12       7

/* BCLK clock dividers */
#define WM8944_BCLKDIV_1        0
#define WM8944_BCLKDIV_2        1
#define WM8944_BCLKDIV_4        2
#define WM8944_BCLKDIV_8        3
#define WM8944_BCLKDIV_16       4
#define WM8944_BCLKDIV_32       5

/* PLL Out Dividers */
#define WM8944_OPCLKDIV_1       0
#define WM8944_OPCLKDIV_2       1
#define WM8944_OPCLKDIV_3       2
#define WM8944_OPCLKDIV_4       3

/*
 * Register values.
 */
/*
 * Field Definitions.
 */

/*
 * R0 (0x0) - Software Reset/Chip ID
 */
#define WM8944_CHIP_ID                          0x6264  /* CHIP_ID */

/*
 * R2 (0x2) - Power Management1
 */

#define WM8944_INPGA_ENA                        0x1000  /* INPGA_ENA */
#define WM8944_INPGA_ENA_MASK                   0x1000  /* INPGA_ENA */
#define WM8944_INPGA_ENA_SHIFT                      12  /* INPGA_ENA */
#define WM8944_INPGA_ENA_WIDTH                       1  /* INPGA_ENA */

#define WM8944_ADCR_ENA                         0x0800  /* ADCR_ENA */
#define WM8944_ADCR_ENA_MASK                    0x0800  /* ADCR_ENA */
#define WM8944_ADCR_ENA_SHIFT                       11  /* ADCR_ENA */
#define WM8944_ADCR_ENA_WIDTH                        1  /* ADCR_ENA */

#define WM8944_ADCL_ENA                         0x0400  /* ADCL_ENA */
#define WM8944_ADCL_ENA_MASK                    0x0400  /* ADCL_ENA */
#define WM8944_ADCL_ENA_SHIFT                       10  /* ADCL_ENA */
#define WM8944_ADCL_ENA_WIDTH                        1  /* ADCL_ENA */

#define WM8944_MICB_ENA                         0x0010  /* MICB_ENA */
#define WM8944_MICB_ENA_MASK                    0x0010  /* MICB_ENA */
#define WM8944_MICB_ENA_SHIFT                        4  /* MICB_ENA */
#define WM8944_MICB_ENA_WIDTH                        1  /* MICB_ENA */

#define WM8944_BIAS_ENA                         0x0008  /* BIAS_ENA */
#define WM8944_BIAS_ENA_MASK                    0x0008  /* BIAS_ENA */
#define WM8944_BIAS_ENA_SHIFT                        3  /* BIAS_ENA */
#define WM8944_BIAS_ENA_WIDTH                        1  /* BIAS_ENA */

#define WM8944_VMID_BUF_ENA                     0x0004  /* VMID_BUF_ENA */
#define WM8944_VMID_BUF_ENA_MASK                0x0004  /* VMID_BUF_ENA */
#define WM8944_VMID_BUF_ENA_SHIFT                    2  /* VMID_BUF_ENA */
#define WM8944_VMID_BUF_ENA_WIDTH                    1  /* VMID_BUF_ENA */

#define WM8944_VMID_SEL_DIS                     0x0000  /* VMID_SEL */
#define WM8944_VMID_SEL_2x50K                   0x0001  /* VMID_SEL */
#define WM8944_VMID_SEL_2x250K                  0x0002  /* VMID_SEL */
#define WM8944_VMID_SEL_2x5K                    0x0003  /* VMID_SEL */
#define WM8944_VMID_SEL_MASK                    0x0003  /* VMID_SEL */
#define WM8944_VMID_SEL_SHIFT                        0  /* VMID_SEL */
#define WM8944_VMID_SEL_WIDTH                        2  /* VMID_SEL */


/*
 * R2 (0x3) - Power Management2
 */

#define WM8944_OUT_ENA                          0x4000  /* OUT_ENA */
#define WM8944_OUT_ENA_MASK                     0x4000  /* OUT_ENA */
#define WM8944_OUT_ENA_SHIFT                        14  /* OUT_ENA */
#define WM8944_OUT_ENA_WIDTH                         1  /* OUT_ENA */

#define WM8944_SPK_PGA_ENA                      0x1000  /* SPK_PGA_ENA */
#define WM8944_SPK_PGA_ENA_MASK                 0x1000  /* SPK_PGA_ENA */
#define WM8944_SPK_PGA_ENA_SHIFT                    12  /* SPK_PGA_ENA */
#define WM8944_SPK_PGA_ENA_WIDTH                     1  /* SPK_PGA_ENA */

#define WM8944_SPKN_SPKVDD_ENA                  0x0800  /* SPKN_SPKVDD_ENA */
#define WM8944_SPKN_SPKVDD_ENA_MASK             0x0800  /* SPKN_SPKVDD_ENA */
#define WM8944_SPKN_SPKVDD_ENA_SHIFT                11  /* SPKN_SPKVDD_ENA */
#define WM8944_SPKN_SPKVDD_ENA_WIDTH                 1  /* SPKN_SPKVDD_ENA */

#define WM8944_SPKP_SPKVDD_ENA                  0x0400  /* SPKP_SPKVDD_ENA */
#define WM8944_SPKP_SPKVDD_ENA_MASK             0x0400  /* SPKP_SPKVDD_ENA */
#define WM8944_SPKP_SPKVDD_ENA_SHIFT                10  /* SPKP_SPKVDD_ENA */
#define WM8944_SPKP_SPKVDD_ENA_WIDTH                 1  /* SPKP_SPKVDD_ENA */

#define WM8944_SPKN_OP_MUTE                     0x0200  /* SPKN_OP_MUTE */
#define WM8944_SPKN_OP_MUTE_MASK                0x0200  /* SPKN_OP_MUTE */
#define WM8944_SPKN_OP_MUTE_SHIFT                    9  /* SPKN_OP_MUTE */
#define WM8944_SPKN_OP_MUTE_WIDTH                    1  /* SPKN_OP_MUTE */

#define WM8944_SPKP_OP_MUTE                     0x0100  /* SPKP_OP_MUTE */
#define WM8944_SPKP_OP_MUTE_MASK                0x0100  /* SPKP_OP_MUTE */
#define WM8944_SPKP_OP_MUTE_SHIFT                    8  /* SPKP_OP_MUTE */
#define WM8944_SPKP_OP_MUTE_WIDTH                    1  /* SPKP_OP_MUTE */

#define WM8944_SPKN_OP_ENA                      0x0080  /* SPKN_OP_ENA */
#define WM8944_SPKN_OP_ENA_MASK                 0x0080  /* SPKN_OP_ENA */
#define WM8944_SPKN_OP_ENA_SHIFT                     7  /* SPKN_OP_ENA */
#define WM8944_SPKN_OP_ENA_WIDTH                     1  /* SPKN_OP_ENA */

#define WM8944_SPKP_OP_ENA                      0x0040  /* SPKP_OP_ENA */
#define WM8944_SPKP_OP_ENA_MASK                 0x0040  /* SPKP_OP_ENA */
#define WM8944_SPKP_OP_ENA_SHIFT                     6  /* SPKP_OP_ENA */
#define WM8944_SPKP_OP_ENA_WIDTH                     1  /* SPKP_OP_ENA */

#define WM8944_SPK_MIX_MUTE                     0x0010  /* SPK_MIX_MUTE */
#define WM8944_SPK_MIX_MUTE_MASK                0x0010  /* SPK_MIX_MUTE */
#define WM8944_SPK_MIX_MUTE_SHIFT                    4  /* SPK_MIX_MUTE */
#define WM8944_SPK_MIX_MUTE_WIDTH                    1  /* SPK_MIX_MUTE */

#define WM8944_SPK_MIX_ENA                      0x0004  /* SPK_MIX_ENA */
#define WM8944_SPK_MIX_ENA_MASK                 0x0004  /* SPK_MIX_ENA */
#define WM8944_SPK_MIX_ENA_SHIFT                     2  /* SPK_MIX_ENA */
#define WM8944_SPK_MIX_ENA_WIDTH                     1  /* SPK_MIX_ENA */

#define WM8944_DAC_ENA                          0x0001  /* DAC_ENA */
#define WM8944_DAC_ENA_MASK                     0x0001  /* DAC_ENA */
#define WM8944_DAC_ENA_SHIFT                         0  /* DAC_ENA */
#define WM8944_DAC_ENA_WIDTH                         1  /* DAC_ENA */

/*
 * R4 (0x4) - Audio interface
 */

#define WM8944_DACDATA_PU                       0xC000  /* DACDATA_PU */
#define WM8944_DACDATA_PU_MASK                  0xC000  /* DACDATA_PU */
#define WM8944_DACDATA_PU_SHIFT                     14  /* DACDATA_PU */
#define WM8944_DACDATA_PU_WIDTH                      2  /* DACDATA_PU */

#define WM8944_FRAME_PU                         0x3000  /* FRAME_PU */
#define WM8944_FRAME_PU_MASK                    0x3000  /* FRAME_PU */
#define WM8944_FRAME_PU_SHIFT                       12  /* FRAME_PU */
#define WM8944_FRAME_PU_WIDTH                        2  /* FRAME_PU */

#define WM8944_BLCK_PU                          0x0C00  /* BLCK_PU */
#define WM8944_BLCK_PU_MASK                     0x0C00  /* BLCK_PU */
#define WM8944_BLCK_PU_SHIFT                        10  /* BLCK_PU */
#define WM8944_BLCK_PU_WIDTH                         2  /* BLCK_PU */

/*
 * R6 (0x6) - Clock
 */

#define WM8944_MLCK_PU                          0x6000  /* MLCK_PU */
#define WM8944_MLCK_PU_MASK                     0x6000  /* MLCK_PU */
#define WM8944_MLCK_PU_SHIFT                        13  /* MLCK_PU */
#define WM8944_MLCK_PU_WIDTH                         2  /* MLCK_PU */

#define WM8944_SYSCLK_ENA                       0x0200  /* SYSCLK_ENA */
#define WM8944_SYSCLK_ENA_MASK                  0x0200  /* SYSCLK_ENA */
#define WM8944_SYSCLK_ENA_SHIFT                      9  /* SYSCLK_ENA */
#define WM8944_SYSCLK_ENA_WIDTH                      1  /* SYSCLK_ENA */

#define WM8944_SYSCLK_SRC                       0x0100  /* SYSCLK_SRC */
#define WM8944_SYSCLK_SRC_MASK                  0x0100  /* SYSCLK_SRC */
#define WM8944_SYSCLK_SRC_SHIFT                      8  /* SYSCLK_SRC */
#define WM8944_SYSCLK_SRC_WIDTH                      1  /* SYSCLK_SRC */

/*
 * R7 (0x7) - Clock
 */

#define WM8944_VMID_FAST_START                  0x0800  /* VMID_FAST_START */
#define WM8944_VMID_FAST_START_MASK             0x0800  /* VMID_FAST_START */
#define WM8944_VMID_FAST_START_SHIFT                11  /* VMID_FAST_START */
#define WM8944_VMID_FAST_START_WIDTH                 1  /* VMID_FAST_START */

#define WM8944_STARTUP_BIAS_ENA                 0x0100  /* STARTUP_BIAS_ENA */
#define WM8944_STARTUP_BIAS_ENA_MASK            0x0100  /* STARTUP_BIAS_ENA */
#define WM8944_STARTUP_BIAS_ENA_SHIFT                8  /* STARTUP_BIAS_ENA */
#define WM8944_STARTUP_BIAS_ENA_WIDTH                1  /* STARTUP_BIAS_ENA */

#define WM8944_BIAS_SRC_STARTUP                 0x0080  /* BIAS_SRC */
#define WM8944_BIAS_SRC_MASTER                  0x0000  /* BIAS_SRC */
#define WM8944_BIAS_SRC_MASK                    0x0080  /* BIAS_SRC */
#define WM8944_BIAS_SRC_SHIFT                        7  /* BIAS_SRC */
#define WM8944_BIAS_SRC_WIDTH                        1  /* BIAS_SRC */

#define WM8944_VMID_RAMP                        0x0060  /* VMID_RAMP */
#define WM8944_VMID_RAMP_MASK                   0x0060  /* VMID_RAMP */
#define WM8944_VMID_RAMP_SHIFT                       5  /* VMID_RAMP */
#define WM8944_VMID_RAMP_WIDTH                       2  /* VMID_RAMP */

#define WM8944_VMID_ENA                         0x0010  /* VMID_ENA */
#define WM8944_VMID_ENA_MASK                    0x0010  /* VMID_ENA */
#define WM8944_VMID_ENA_SHIFT                        4  /* VMID_ENA */
#define WM8944_VMID_ENA_WIDTH                        1  /* VMID_ENA */

/*
 * R13 (0xD) - GPIO1 control
 */

#define WM8944_GP1_PU                           0x6000  /* GP1_PU */
#define WM8944_GP1_PU_MASK                      0x6000  /* GP1_PU */
#define WM8944_GP1_PU_SHIFT                         13  /* GP1_PU */
#define WM8944_GP1_PU_WIDTH                          2  /* GP1_PU */

/*
 * R14 (0xE) - GPIO2 control
 */

#define WM8944_GP2_PU                           0x6000  /* GP2_PU */
#define WM8944_GP2_PU_MASK                      0x6000  /* GP2_PU */
#define WM8944_GP2_PU_SHIFT                         13  /* GP2_PU */
#define WM8944_GP2_PU_WIDTH                          2  /* GP2_PU */

/*
 * R16 (0x10) - Interrupt Status 1
 */
#define WM8944_TEMP_EINT                        0x8000  /* TEMP_EINT */
#define WM8944_TEMP_EINT_MASK                   0x8000  /* TEMP_EINT */
#define WM8944_TEMP_EINT_SHIFT                      15  /* TEMP_EINT */
#define WM8944_TEMP_EINT_WIDTH                       1  /* TEMP_EINT */

#define WM8944_GP2_EINT                         0x2000  /* GP2_EINT */
#define WM8944_GP2_EINT_MASK                    0x2000  /* GP2_EINT */
#define WM8944_GP2_EINT_SHIFT                       13  /* GP2_EINT */
#define WM8944_GP2_EINT_WIDTH                        1  /* GP2_EINT */

#define WM8944_GP1_EINT                         0x1000  /* GP1_EINT */
#define WM8944_GP1_EINT_MASK                    0x1000  /* GP1_EINT */
#define WM8944_GP1_EINT_SHIFT                       12  /* GP1_EINT */
#define WM8944_GP1_EINT_WIDTH                        1  /* GP1_EINT */

#define WM8944_LDO_UV_EINT                      0x0001  /* LDO_UV_EINT */
#define WM8944_LDO_UV_EINT_MASK                 0x0001  /* LDO_UV_EINT */
#define WM8944_LDO_UV_EINT_SHIFT                     0  /* LDO_UV_EINT */
#define WM8944_LDO_UV_EINT_WIDTH                     1  /* LDO_UV_EINT */

/*
 * R17 (0x11) - Status flags
 */
#define WM8944_TEMP_STS_EINT                    0x8000  /* TEMP_STS_EINT */
#define WM8944_TEMP_STS_EINT_MASK               0x8000  /* TEMP_STS_EINT */
#define WM8944_TEMP_STS_EINT_SHIFT                  15  /* TEMP_STS_EINT */
#define WM8944_TEMP_STS_EINT_WIDTH                   1  /* TEMP_STS_EINT */

#define WM8944_LDO_UV_STS_EINT                  0x0001  /* LDO_UV_STS_EINT */
#define WM8944_LDO_UV_STS_EINT_MASK             0x0001  /* LDO_UV_STS_EINT */
#define WM8944_LDO_UV_STS_EINT_SHIFT                 0  /* LDO_UV_STS_EINT */
#define WM8944_LDO_UV_STS_EINT_WIDTH                 1  /* LDO_UV_STS_EINT */

/*
 * R23 (0x17) - DAC Digital Vol
 */
#define WM8944_DAC_MUTE                         0x0100  /* DAC_MUTE */
#define WM8944_DAC_MUTE_MASK                    0x0100  /* DAC_MUTE */
#define WM8944_DAC_MUTE_SHIFT                        8  /* DAC_MUTE */
#define WM8944_DAC_MUTE_WIDTH                        1  /* DAC_MUTE */

#define WM8944_DAC_VOL_MASK                     0x00FF  /* DAC_VOL */
#define WM8944_DAC_VOL_SHIFT                         0  /* DAC_VOL */
#define WM8944_DAC_VOL_WIDTH                         8  /* DAC_VOL */

/*
 * R25 (0x19) - ADC control 1
 */
#define WM8944_ADC_MUTE_ALL                     0x0100  /* ADC_MUTE_ALL */
#define WM8944_ADC_MUTE_ALL_MASK                0x0100  /* ADC_MUTE_ALL */
#define WM8944_ADC_MUTE_ALL_SHIFT                    8  /* ADC_MUTE_ALL */
#define WM8944_ADC_MUTE_ALL_WIDTH                    1  /* ADC_MUTE_ALL */

#define WM8944_ADCR_DAT_INV                     0x0002  /* ADCR_DAT_INV */
#define WM8944_ADCR_DAT_INV_MASK                0x0002  /* ADCR_DAT_INV */
#define WM8944_ADCR_DAT_INV_SHIFT                    1  /* ADCR_DAT_INV */
#define WM8944_ADCR_DAT_INV_WIDTH                    1  /* ADCR_DAT_INV */

#define WM8944_ADCL_DAT_INV                     0x0001  /* ADCL_DAT_INV */
#define WM8944_ADCL_DAT_INV_MASK                0x0001  /* ADCL_DAT_INV */
#define WM8944_ADCL_DAT_INV_SHIFT                    0  /* ADCL_DAT_INV */
#define WM8944_ADCL_DAT_INV_WIDTH                    1  /* ADCL_DAT_INV */

/*
 * R39 (0x27) - Input ctrl
 */
#define WM8944_MICB_LVL                         0x0040  /* MICB_LVL */
#define WM8944_MICB_LVL_MASK                    0x0040  /* MICB_LVL */
#define WM8944_MICB_LVL_SHIFT                        6  /* MICB_LVL */
#define WM8944_MICB_LVL_WIDTH                        1  /* MICB_LVL */

/*
 * R40 (0x28) - Input PGA gain ctrl
 */
#define WM8944_INPGA_ZC                         0x0080  /* INPGA_ZC */
#define WM8944_INPGA_ZC_MASK                    0x0080  /* INPGA_ZC */
#define WM8944_INPGA_ZC_SHIFT                        7  /* INPGA_ZC */
#define WM8944_INPGA_ZC_WIDTH                        1  /* INPGA_ZC */

#define WM8944_INPGA_MUTE                       0x0040  /* INPGA_MUTE */
#define WM8944_INPGA_MUTE_MASK                  0x0040  /* INPGA_MUTE */
#define WM8944_INPGA_MUTE_SHIFT                      6  /* INPGA_MUTE */
#define WM8944_INPGA_MUTE_WIDTH                      1  /* INPGA_MUTE */

#define WM8944_INPGA_VOL_MASK                   0x003F  /* INPGA_VOL */
#define WM8944_INPGA_VOL_SHIFT                       0  /* INPGA_VOL */
#define WM8944_INPGA_VOL_WIDTH                       6  /* INPGA_VOL */

/*
 * R42 (0x2A) - Output Ctrl
 */
#define WM8944_SPKN_VMID_OP_ENA                 0x2000  /* SPKN_VMID_OP_ENA */
#define WM8944_SPKN_VMID_OP_ENA_MASK            0x2000  /* SPKN_VMID_OP_ENA */
#define WM8944_SPKN_VMID_OP_ENA_SHIFT               13  /* SPKN_VMID_OP_ENA */
#define WM8944_SPKN_VMID_OP_ENA_WIDTH                1  /* SPKN_VMID_OP_ENA */

#define WM8944_SPKP_VMID_OP_ENA                 0x1000  /* SPKP_VMID_OP_ENA */
#define WM8944_SPKP_VMID_OP_ENA_MASK            0x1000  /* SPKP_VMID_OP_ENA */
#define WM8944_SPKP_VMID_OP_ENA_SHIFT               12  /* SPKP_VMID_OP_ENA */
#define WM8944_SPKP_VMID_OP_ENA_WIDTH                1  /* SPKP_VMID_OP_ENA */

#define WM8944_LINE_VMID_OP_ENA                 0x0400  /* LINE_VMID_OP_ENA */
#define WM8944_LINE_VMID_OP_ENA_MASK            0x0400  /* LINE_VMID_OP_ENA */
#define WM8944_LINE_VMID_OP_ENA_SHIFT               10  /* LINE_VMID_OP_ENA */
#define WM8944_LINE_VMID_OP_ENA_WIDTH                1  /* LINE_VMID_OP_ENA */

#define WM8944_LINE_MUTE                        0x0100  /* LINE_MUTE */
#define WM8944_LINE_MUTE_MASK                   0x0100  /* LINE_MUTE */
#define WM8944_LINE_MUTE_SHIFT                       8  /* LINE_MUTE */
#define WM8944_LINE_MUTE_WIDTH                       1  /* LINE_MUTE */

#define WM8944_SPKN_DISCH                       0x0080  /* SPKN_DISCH */
#define WM8944_SPKN_DISCH_MASK                  0x0080  /* SPKN_DISCH */
#define WM8944_SPKN_DISCH_SHIFT                      7  /* SPKN_DISCH */
#define WM8944_SPKN_DISCH_WIDTH                      1  /* SPKN_DISCH */

#define WM8944_SPKP_DISCH                       0x0040  /* SPKP_DISCH */
#define WM8944_SPKP_DISCH_MASK                  0x0040  /* SPKP_DISCH */
#define WM8944_SPKP_DISCH_SHIFT                      6  /* SPKP_DISCH */
#define WM8944_SPKP_DISCH_WIDTH                      1  /* SPKP_DISCH */

#define WM8944_LINE_DISCH                       0x0010  /* LINE_DISCH */
#define WM8944_LINE_DISCH_MASK                  0x0010  /* LINE_DISCH */
#define WM8944_LINE_DISCH_SHIFT                      4  /* LINE_DISCH */
#define WM8944_LINE_DISCH_WIDTH                      1  /* LINE_DISCH */

#define WM8944_SPK_VROI                         0x0002  /* SPK_VROI */
#define WM8944_SPK_VROI_MASK                    0x0002  /* SPK_VROI */
#define WM8944_SPK_VROI_SHIFT                        1  /* SPK_VROI */
#define WM8944_SPK_VROI_WIDTH                        1  /* SPK_VROI */

#define WM8944_LINE_VROI                        0x0001  /* LINE_VROI */
#define WM8944_LINE_VROI_MASK                   0x0001  /* LINE_VROI */
#define WM8944_LINE_VROI_SHIFT                       0  /* LINE_VROI */
#define WM8944_LINE_VROI_WIDTH                       1  /* LINE_VROI */

/*
 * R47 (0x2F) - SPK Volume ctrl
 */
#define WM8944_SPK_ZC                           0x0080  /* SPK_ZC */
#define WM8944_SPK_ZC_MASK                      0x0080  /* SPK_ZC */
#define WM8944_SPK_ZC_SHIFT                          7  /* SPK_ZC */
#define WM8944_SPK_ZC_WIDTH                          1  /* SPK_ZC */

#define WM8944_SPK_PGA_MUTE                     0x0040  /* SPK_PGA_MUTE */
#define WM8944_SPK_PGA_MUTE_MASK                0x0040  /* SPK_PGA_MUTE */
#define WM8944_SPK_PGA_MUTE_SHIFT                    6  /* SPK_PGA_MUTE */
#define WM8944_SPK_PGA_MUTE_WIDTH                    1  /* SPK_PGA_MUTE */

#define WM8944_SPK_VOL_MASK                     0x003F  /* SPK_VOL */
#define WM8944_SPK_VOL_SHIFT                         0  /* SPK_VOL */
#define WM8944_SPK_VOL_WIDTH                         6  /* SPK_VOL */

/*
 * R53 (0x35) - LDO
 */

#define WM8944_LDO_ENA                          0x8000  /* LDO_ENA */
#define WM8944_LDO_ENA_MASK                     0x8000  /* LDO_ENA */
#define WM8944_LDO_ENA_SHIFT                        15  /* LDO_ENA */
#define WM8944_LDO_ENA_WIDTH                         1  /* LDO_ENA */

#define WM8944_LDO_REF_SEL_FAST                 0x4000  /* LDO_REF_SEL_FAST */
#define WM8944_LDO_REF_SEL_NORMAL               0x0000  /* LDO_REF_SEL_FAST */
#define WM8944_LDO_REF_SEL_FAST_MASK            0x4000  /* LDO_REF_SEL_FAST */
#define WM8944_LDO_REF_SEL_FAST_SHIFT               14  /* LDO_REF_SEL_FAST */
#define WM8944_LDO_REF_SEL_FAST_WIDTH                1  /* LDO_REF_SEL_FAST */

#define WM8944_LDO_OPFLT                        0x1000  /* LDO_OPFLT */
#define WM8944_LDO_OPFLT_MASK                   0x1000  /* LDO_OPFLT */
#define WM8944_LDO_OPFLT_SHIFT                      12  /* LDO_OPFLT */
#define WM8944_LDO_OPFLT_WIDTH                       1  /* LDO_OPFLT */

#define WM8944_LDO_BIAS_SRC_STARTUP             0x0020  /* LDO_BIAS_SRC */
#define WM8944_LDO_BIAS_SRC_MASTER              0x0000  /* LDO_BIAS_SRC */
#define WM8944_LDO_BIAS_SRC                     0x0020  /* LDO_BIAS_SRC */
#define WM8944_LDO_BIAS_SRC_MASK                0x0020  /* LDO_BIAS_SRC */
#define WM8944_LDO_BIAS_SRC_SHIFT                    5  /* LDO_BIAS_SRC */
#define WM8944_LDO_BIAS_SRC_WIDTH                    1  /* LDO_BIAS_SRC */


#endif
