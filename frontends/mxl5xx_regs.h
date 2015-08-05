/*
* Copyright (c) 2011-2013 MaxLinear, Inc. All rights reserved
*
* License type: GPLv2
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*
* This program may alternatively be licensed under a proprietary license from
* MaxLinear, Inc.
*
* See terms and conditions defined in file 'LICENSE.txt', which is part of this
* source code package.
*/

#ifndef __MXL58X_REGISTERS_H__
#define __MXL58X_REGISTERS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HYDRA_INTR_STATUS_REG               0x80030008
#define HYDRA_INTR_MASK_REG                 0x8003000C

#define HYDRA_CRYSTAL_SETTING               0x3FFFC5F0 // 0 - 24 MHz & 1 - 27 MHz
#define HYDRA_CRYSTAL_CAP                   0x3FFFEDA4 // 0 - 24 MHz & 1 - 27 MHz

#define HYDRA_CPU_RESET_REG                 0x8003003C
#define HYDRA_CPU_RESET_DATA                0x00000400

#define HYDRA_RESET_TRANSPORT_FIFO_REG      0x80030028
#define HYDRA_RESET_TRANSPORT_FIFO_DATA     0x00000000

#define HYDRA_RESET_BBAND_REG               0x80030024
#define HYDRA_RESET_BBAND_DATA              0x00000000

#define HYDRA_RESET_XBAR_REG                0x80030020
#define HYDRA_RESET_XBAR_DATA               0x00000000

#define HYDRA_MODULES_CLK_1_REG             0x80030014
#define HYDRA_DISABLE_CLK_1                 0x00000000

#define HYDRA_MODULES_CLK_2_REG             0x8003001C
#define HYDRA_DISABLE_CLK_2                 0x0000000B

#define HYDRA_PRCM_ROOT_CLK_REG             0x80030018
#define HYDRA_PRCM_ROOT_CLK_DISABLE         0x00000000

#define HYDRA_CPU_RESET_CHECK_REG           0x80030008
#define HYDRA_CPU_RESET_CHECK_OFFSET        0x40000000  //  <bit 30>

#define HYDRA_SKU_ID_REG                    0x90000190

#define FW_DL_SIGN_ADDR                     0x3FFFEAE0

// Register to check if FW is running or not
#define HYDRA_HEAR_BEAT                     0x3FFFEDDC

// Firmware version
#define HYDRA_FIRMWARE_VERSION              0x3FFFEDB8
#define HYDRA_FW_RC_VERSION                 0x3FFFCFAC

// Firmware patch version
#define HYDRA_FIRMWARE_PATCH_VERSION        0x3FFFEDC2

// SOC operating temperature in C
#define HYDRA_TEMPARATURE                   0x3FFFEDB4

// Demod & Tuner status registers
// Demod 0 status base address
#define HYDRA_DEMOD_0_BASE_ADDR             0x3FFFC64C

// Tuner 0 status base address
#define HYDRA_TUNER_0_BASE_ADDR             0x3FFFCE4C

#define POWER_FROM_ADCRSSI_READBACK         0x3FFFEB6C

// Macros to determine base address of respective demod or tuner
#define HYDRA_DMD_STATUS_OFFSET(demodID)        ((demodID) * 0x100)
#define HYDRA_TUNER_STATUS_OFFSET(tunerID)      ((tunerID) * 0x40)

// Demod status address offset from respective demod's base address
#define HYDRA_DMD_AGC_DIG_LEVEL_ADDR_OFFSET               0x3FFFC64C
#define HYDRA_DMD_LOCK_STATUS_ADDR_OFFSET                 0x3FFFC650
#define HYDRA_DMD_ACQ_STATUS_ADDR_OFFSET                  0x3FFFC654

#define HYDRA_DMD_STANDARD_ADDR_OFFSET                    0x3FFFC658
#define HYDRA_DMD_SPECTRUM_INVERSION_ADDR_OFFSET          0x3FFFC65C
#define HYDRA_DMD_SPECTRUM_ROLL_OFF_ADDR_OFFSET           0x3FFFC660
#define HYDRA_DMD_SYMBOL_RATE_ADDR_OFFSET                 0x3FFFC664
#define HYDRA_DMD_MODULATION_SCHEME_ADDR_OFFSET           0x3FFFC668
#define HYDRA_DMD_FEC_CODE_RATE_ADDR_OFFSET               0x3FFFC66C

#define HYDRA_DMD_SNR_ADDR_OFFSET                         0x3FFFC670
#define HYDRA_DMD_FREQ_OFFSET_ADDR_OFFSET                 0x3FFFC674
#define HYDRA_DMD_CTL_FREQ_OFFSET_ADDR_OFFSET             0x3FFFC678
#define HYDRA_DMD_STR_FREQ_OFFSET_ADDR_OFFSET             0x3FFFC67C
#define HYDRA_DMD_FTL_FREQ_OFFSET_ADDR_OFFSET             0x3FFFC680
#define HYDRA_DMD_STR_NBC_SYNC_LOCK_ADDR_OFFSET           0x3FFFC684
#define HYDRA_DMD_CYCLE_SLIP_COUNT_ADDR_OFFSET            0x3FFFC688

#define HYDRA_DMD_DISPLAY_I_ADDR_OFFSET                   0x3FFFC68C
#define HYDRA_DMD_DISPLAY_Q_ADDR_OFFSET                   0x3FFFC68E

#define HYDRA_DMD_DVBS2_CRC_ERRORS_ADDR_OFFSET            0x3FFFC690
#define HYDRA_DMD_DVBS2_PER_COUNT_ADDR_OFFSET             0x3FFFC694
#define HYDRA_DMD_DVBS2_PER_WINDOW_ADDR_OFFSET            0x3FFFC698

#define HYDRA_DMD_DVBS_CORR_RS_ERRORS_ADDR_OFFSET         0x3FFFC69C
#define HYDRA_DMD_DVBS_UNCORR_RS_ERRORS_ADDR_OFFSET       0x3FFFC6A0
#define HYDRA_DMD_DVBS_BER_COUNT_ADDR_OFFSET              0x3FFFC6A4
#define HYDRA_DMD_DVBS_BER_WINDOW_ADDR_OFFSET             0x3FFFC6A8

// Debug-purpose DVB-S DMD 0
#define HYDRA_DMD_DVBS_1ST_CORR_RS_ERRORS_ADDR_OFFSET     0x3FFFC6C8  // corrected RS Errors: 1st iteration
#define HYDRA_DMD_DVBS_1ST_UNCORR_RS_ERRORS_ADDR_OFFSET   0x3FFFC6CC  // uncorrected RS Errors: 1st iteration
#define HYDRA_DMD_DVBS_BER_COUNT_1ST_ADDR_OFFSET          0x3FFFC6D0
#define HYDRA_DMD_DVBS_BER_WINDOW_1ST_ADDR_OFFSET         0x3FFFC6D4

#define HYDRA_DMD_TUNER_ID_ADDR_OFFSET                    0x3FFFC6AC
#define HYDRA_DMD_DVBS2_PILOT_ON_OFF_ADDR_OFFSET          0x3FFFC6B0
#define HYDRA_DMD_FREQ_SEARCH_RANGE_KHZ_ADDR_OFFSET       0x3FFFC6B4
#define HYDRA_DMD_STATUS_LOCK_ADDR_OFFSET                 0x3FFFC6B8
#define HYDRA_DMD_STATUS_CENTER_FREQ_IN_KHZ_ADDR          0x3FFFC704
#define HYDRA_DMD_STATUS_INPUT_POWER_ADDR                 0x3FFFC708

// DVB-S new scaled_BER_count for a new BER API, see HYDRA-1343 "DVB-S post viterbi information"
#define DMD0_STATUS_DVBS_1ST_SCALED_BER_COUNT_ADDR        0x3FFFC710 // DMD 0: 1st iteration BER count scaled by HYDRA_BER_COUNT_SCALING_FACTOR
#define DMD0_STATUS_DVBS_SCALED_BER_COUNT_ADDR            0x3FFFC714 // DMD 0: 2nd iteration BER count scaled by HYDRA_BER_COUNT_SCALING_FACTOR

#define DMD0_SPECTRUM_MIN_GAIN_STATUS                     0x3FFFC73C
#define DMD0_SPECTRUM_MIN_GAIN_WB_SAGC_VALUE              0x3FFFC740
#define DMD0_SPECTRUM_ MIN_GAIN_NB_SAGC_VALUE             0x3FFFC744

#define HYDRA_DMD_STATUS_END_ADDR_OFFSET                  0x3FFFC748

// Tuner status address offset from respective tuners's base address
#define HYDRA_TUNER_DEMOD_ID_ADDR_OFFSET                  0x3FFFCE4C
#define HYDRA_TUNER_AGC_LOCK_OFFSET                       0x3FFFCE50
#define HYDRA_TUNER_SPECTRUM_STATUS_OFFSET                0x3FFFCE54
#define HYDRA_TUNER_SPECTRUM_BIN_SIZE_OFFSET              0x3FFFCE58
#define HYDRA_TUNER_SPECTRUM_ADDRESS_OFFSET               0x3FFFCE5C
#define HYDRA_TUNER_ENABLE_COMPLETE                       0x3FFFEB78

#define HYDRA_DEMOD_STATUS_LOCK(devId, demodId)   write_register(devId, (HYDRA_DMD_STATUS_LOCK_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)), MXL_YES)
#define HYDRA_DEMOD_STATUS_UNLOCK(devId, demodId) write_register(devId, (HYDRA_DMD_STATUS_LOCK_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)), MXL_NO)

#define HYDRA_TUNER_STATUS_LOCK(devId,tunerId)   MxLWare_HYDRA_WriteRegister(devId,(HYDRA_TUNER_STATUS_LOCK_ADDR_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)), MXL_YES)
#define HYDRA_TUNER_STATUS_UNLOCK(devId,tunerId) MxLWare_HYDRA_WriteRegister(devId,(HYDRA_TUNER_STATUS_LOCK_ADDR_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)), MXL_NO)

#define HYDRA_VERSION                                     0x3FFFEDB8
#define HYDRA_DEMOD0_VERSION                              0x3FFFEDBC
#define HYDRA_DEMOD1_VERSION                              0x3FFFEDC0
#define HYDRA_DEMOD2_VERSION                              0x3FFFEDC4
#define HYDRA_DEMOD3_VERSION                              0x3FFFEDC8
#define HYDRA_DEMOD4_VERSION                              0x3FFFEDCC
#define HYDRA_DEMOD5_VERSION                              0x3FFFEDD0
#define HYDRA_DEMOD6_VERSION                              0x3FFFEDD4
#define HYDRA_DEMOD7_VERSION                              0x3FFFEDD8
#define HYDRA_HEAR_BEAT                                   0x3FFFEDDC
#define HYDRA_SKU_MGMT                                    0x3FFFEBC0

#define MXL_HYDRA_FPGA_A_ADDRESS                          0x91C00000
#define MXL_HYDRA_FPGA_B_ADDRESS                          0x91D00000

// TS control base address
#define HYDRA_TS_CTRL_BASE_ADDR                           0x90700000

#define MPEG_MUX_MODE_SLICE0_REG            HYDRA_TS_CTRL_BASE_ADDR + 0x08
#define MPEG_MUX_MODE_SLICE0_OFFSET         (0),(2)

#define MPEG_MUX_MODE_SLICE1_REG            HYDRA_TS_CTRL_BASE_ADDR + 0x08
#define MPEG_MUX_MODE_SLICE1_OFFSET         (2),(2)

#define PID_BANK_SEL_SLICE0_REG             HYDRA_TS_CTRL_BASE_ADDR + 0x190
#define PID_BANK_SEL_SLICE1_REG             HYDRA_TS_CTRL_BASE_ADDR + 0x1B0

#define SW_REGULAR_PID_SW_BANK_OFFSET       0,1
#define SW_FIXED_PID_SW_BANK_OFFSET         1,1

#define HW_REGULAR_PID_BANK_OFFSET          8,4
#define HW_FIXED_PID_BANK_OFFSET            4,4

#define MPEG_CLK_GATED_REG                  HYDRA_TS_CTRL_BASE_ADDR + 0x20
#define MPEG_CLK_GATED_OFFSET               0,1

#define MPEG_CLK_ALWAYS_ON_REG              HYDRA_TS_CTRL_BASE_ADDR + 0x1D4
#define MPEG_CLK_ALWAYS_ON_OFFSET           0,1

#define HYDRA_REGULAR_PID_BANK_A_REG        HYDRA_TS_CTRL_BASE_ADDR + 0x190
#define HYDRA_REGULAR_PID_BAN K_A_OFFSET     0,1

#define HYDRA_FIXED_PID_BANK_A_REG          HYDRA_TS_CTRL_BASE_ADDR + 0x190
#define HYDRA_FIXED_PID_BANK_A_OFFSET       1,1

#define HYDRA_REGULAR_PID_BANK_B_REG        HYDRA_TS_CTRL_BASE_ADDR + 0x1B0
#define HYDRA_REGULAR_PID_BANK_B_OFFSET     0,1

#define HYDRA_FIXED_PID_BANK_B_REG          HYDRA_TS_CTRL_BASE_ADDR + 0x1B0
#define HYDRA_FIXED_PID_BANK_B_OFFSET       1,1

#define FIXED_PID_TBL_REG_ADDRESS_0         HYDRA_TS_CTRL_BASE_ADDR + 0x9000
#define FIXED_PID_TBL_REG_ADDRESS_1         HYDRA_TS_CTRL_BASE_ADDR + 0x9100
#define FIXED_PID_TBL_REG_ADDRESS_2         HYDRA_TS_CTRL_BASE_ADDR + 0x9200
#define FIXED_PID_TBL_REG_ADDRESS_3         HYDRA_TS_CTRL_BASE_ADDR + 0x9300

#define FIXED_PID_TBL_REG_ADDRESS_4         HYDRA_TS_CTRL_BASE_ADDR + 0xB000
#define FIXED_PID_TBL_REG_ADDRESS_5         HYDRA_TS_CTRL_BASE_ADDR + 0xB100
#define FIXED_PID_TBL_REG_ADDRESS_6         HYDRA_TS_CTRL_BASE_ADDR + 0xB200
#define FIXED_PID_TBL_REG_ADDRESS_7         HYDRA_TS_CTRL_BASE_ADDR + 0xB300

#define REGULAR_PID_TBL_REG_ADDRESS_0       HYDRA_TS_CTRL_BASE_ADDR + 0x8000
#define REGULAR_PID_TBL_REG_ADDRESS_1       HYDRA_TS_CTRL_BASE_ADDR + 0x8200
#define REGULAR_PID_TBL_REG_ADDRESS_2       HYDRA_TS_CTRL_BASE_ADDR + 0x8400
#define REGULAR_PID_TBL_REG_ADDRESS_3       HYDRA_TS_CTRL_BASE_ADDR + 0x8600

#define REGULAR_PID_TBL_REG_ADDRESS_4       HYDRA_TS_CTRL_BASE_ADDR + 0xA000
#define REGULAR_PID_TBL_REG_ADDRESS_5       HYDRA_TS_CTRL_BASE_ADDR + 0xA200
#define REGULAR_PID_TBL_REG_ADDRESS_6       HYDRA_TS_CTRL_BASE_ADDR + 0xA400
#define REGULAR_PID_TBL_REG_ADDRESS_7       HYDRA_TS_CTRL_BASE_ADDR + 0xA600

#define PID_VALID_OFFSET                    0,1
#define PID_DROP_OFFSET                     1,1
#define PID_REMAP_ENABLE_OFFSET             2,1
#define PID_VALUE_OFFSET                    4,13
#define PID_MASK_OFFSET                     19,13

#define REGULAR_PID_REMAP_VALUE_OFFSET      0,13
#define FIXED_PID_REMAP_VALUE_OFFSET        0,16
#define PID_DEMODID_OFFSET                  16,3


///////////////////////////////////////////////

#if 0
#define AFE_REG_D2A_TA_ADC_CLK_OUT_FLIP 0x90200004,12,1
#define AFE_REG_D2A_TA_RFFE_LNACAPLOAD_1P8 0x90200028,24,4
#define AFE_REG_D2A_TA_RFFE_RF1_EN_1P8 0x90200028,5,1
#define AFE_REG_D2A_TA_RFFE_SPARE_1P8 0x90200028,8,8
#define AFE_REG_D2A_TB_ADC_CLK_OUT_FLIP 0x9020000C,23,1
#define AFE_REG_D2A_TB_RFFE_LNACAPLOAD_1P8 0x90200030,16,4
#define AFE_REG_D2A_TB_RFFE_RF1_EN_1P8 0x9020002C,21,1
#define AFE_REG_D2A_TB_RFFE_SPARE_1P8 0x90200030,0,8
#define AFE_REG_D2A_TC_ADC_CLK_OUT_FLIP 0x90200018,7,1
#define AFE_REG_D2A_TC_RFFE_LNACAPLOAD_1P8 0x90200038,2,4
#define AFE_REG_D2A_TC_RFFE_RF1_EN_1P8 0x90200034,14,1
#define AFE_REG_D2A_TC_RFFE_SPARE_1P8 0x90200034,17,8
#define AFE_REG_D2A_TD_ADC_CLK_OUT_FLIP 0x90200020,18,1
#define AFE_REG_D2A_TD_RFFE_LNACAPLOAD_1P8 0x9020003C,17,4
#define AFE_REG_D2A_TD_RFFE_RF1_EN_1P8 0x90200038,29,1
#define AFE_REG_D2A_TD_RFFE_SPARE_1P8 0x9020003C,1,8
#endif
#define AFE_REG_D2A_XTAL_EN_CLKOUT_1P8 0x90200054,23,1

#define   PAD_MUX_TS0_IN_CLK_PINMUX_SEL                          0x90000018,0,3
#define   PAD_MUX_TS0_IN_DATA_PINMUX_SEL                         0x90000018,4,3
#define   PAD_MUX_TS1_IN_CLK_PINMUX_SEL                          0x90000018,8,3
#define   PAD_MUX_TS1_IN_DATA_PINMUX_SEL                         0x90000018,12,3
#define   PAD_MUX_TS2_IN_CLK_PINMUX_SEL                          0x90000018,16,3
#define   PAD_MUX_TS2_IN_DATA_PINMUX_SEL                         0x90000018,20,3
#define   PAD_MUX_TS3_IN_CLK_PINMUX_SEL                          0x90000018,24,3
#define   PAD_MUX_TS3_IN_DATA_PINMUX_SEL                         0x90000018,28,3

#define   PAD_MUX_GPIO_00_SYNC_BASEADDR                          0x90000188
#define   PAD_MUX_GPIO_01_SYNC_IN                                PAD_MUX_GPIO_00_SYNC_BASEADDR,1,1

#define PRCM_AFE_SOC_ID 0x80030004,24,8

#define PAD_MUX_UART_RX_C_PINMUX_BASEADDR 0x9000001C
#define PAD_MUX_UART_RX_C_PINMUX_SEL PAD_MUX_UART_RX_C_PINMUX_BASEADDR,0,3
#define PAD_MUX_UART_RX_D_PINMUX_SEL PAD_MUX_UART_RX_C_PINMUX_BASEADDR,4,3
#define PAD_MUX_BOND_OPTION 0x90000190,0,3
#define PAD_MUX_DIGIO_01_PINMUX_SEL 0x9000016C,4,3
#define PAD_MUX_DIGIO_02_PINMUX_SEL 0x9000016C,8,3
#define PAD_MUX_DIGIO_03_PINMUX_SEL 0x9000016C,12,3
#define PAD_MUX_DIGIO_04_PINMUX_SEL 0x9000016C,16,3
#define PAD_MUX_DIGIO_05_PINMUX_SEL 0x9000016C,20,3
#define PAD_MUX_DIGIO_06_PINMUX_SEL 0x9000016C,24,3
#define PAD_MUX_DIGIO_07_PINMUX_SEL 0x9000016C,28,3
#define PAD_MUX_DIGIO_08_PINMUX_SEL 0x90000170,0,3
#define PAD_MUX_DIGIO_09_PINMUX_SEL 0x90000170,4,3
#define PAD_MUX_DIGIO_10_PINMUX_SEL 0x90000170,8,3
#define PAD_MUX_DIGIO_11_PINMUX_SEL 0x90000170,12,3
#define PAD_MUX_DIGIO_12_PINMUX_SEL 0x90000170,16,3
#define PAD_MUX_DIGIO_13_PINMUX_SEL 0x90000170,20,3
#define PAD_MUX_DIGIO_14_PINMUX_SEL 0x90000170,24,3
#define PAD_MUX_DIGIO_15_PINMUX_SEL 0x90000170,28,3
#define PAD_MUX_DIGIO_16_PINMUX_SEL 0x90000174,0,3
#define PAD_MUX_DIGIO_17_PINMUX_SEL 0x90000174,4,3
#define PAD_MUX_DIGIO_18_PINMUX_SEL 0x90000174,8,3
#define PAD_MUX_DIGIO_19_PINMUX_SEL 0x90000174,12,3
#define PAD_MUX_DIGIO_20_PINMUX_SEL 0x90000174,16,3
#define PAD_MUX_DIGIO_21_PINMUX_SEL 0x90000174,20,3
#define PAD_MUX_DIGIO_22_PINMUX_SEL 0x90000174,24,3
#define PAD_MUX_DIGIO_23_PINMUX_SEL 0x90000174,28,3
#define PAD_MUX_DIGIO_24_PINMUX_SEL 0x90000178,0,3
#define PAD_MUX_DIGIO_25_PINMUX_SEL 0x90000178,4,3
#define PAD_MUX_DIGIO_26_PINMUX_SEL 0x90000178,8,3
#define PAD_MUX_DIGIO_27_PINMUX_SEL 0x90000178,12,3
#define PAD_MUX_DIGIO_28_PINMUX_SEL 0x90000178,16,3
#define PAD_MUX_DIGIO_29_PINMUX_SEL 0x90000178,20,3
#define PAD_MUX_DIGIO_30_PINMUX_SEL 0x90000178,24,3
#define PAD_MUX_DIGIO_31_PINMUX_SEL 0x90000178,28,3
#define PAD_MUX_DIGIO_32_PINMUX_SEL 0x9000017C,0,3
#define PAD_MUX_DIGIO_33_PINMUX_SEL 0x9000017C,4,3
#define PAD_MUX_DIGIO_34_PINMUX_SEL 0x9000017C,8,3
#define PAD_MUX_EJTAG_TCK_PINMUX_SEL 0x90000020,0,3
#define PAD_MUX_EJTAG_TDI_PINMUX_SEL 0x90000020,8,3
#define PAD_MUX_EJTAG_TMS_PINMUX_SEL 0x90000020,4,3
#define PAD_MUX_EJTAG_TRSTN_PINMUX_SEL 0x90000020,12,3
#define PAD_MUX_PAD_DRV_DIGIO_00 0x90000194,0,3
#define PAD_MUX_PAD_DRV_DIGIO_05 0x90000194,20,3
#define PAD_MUX_PAD_DRV_DIGIO_06 0x90000194,24,3
#define PAD_MUX_PAD_DRV_DIGIO_11 0x90000198,12,3
#define PAD_MUX_PAD_DRV_DIGIO_12 0x90000198,16,3
#define PAD_MUX_PAD_DRV_DIGIO_13 0x90000198,20,3
#define PAD_MUX_PAD_DRV_DIGIO_14 0x90000198,24,3
#define PAD_MUX_PAD_DRV_DIGIO_16 0x9000019C,0,3
#define PAD_MUX_PAD_DRV_DIGIO_17 0x9000019C,4,3
#define PAD_MUX_PAD_DRV_DIGIO_18 0x9000019C,8,3
#define PAD_MUX_PAD_DRV_DIGIO_22 0x9000019C,24,3
#define PAD_MUX_PAD_DRV_DIGIO_23 0x9000019C,28,3
#define PAD_MUX_PAD_DRV_DIGIO_24 0x900001A0,0,3
#define PAD_MUX_PAD_DRV_DIGIO_25 0x900001A0,4,3
#define PAD_MUX_PAD_DRV_DIGIO_29 0x900001A0,20,3
#define PAD_MUX_PAD_DRV_DIGIO_30 0x900001A0,24,3
#define PAD_MUX_PAD_DRV_DIGIO_31 0x900001A0,28,3
#define PRCM_AFE_REG_CLOCK_ENABLE 0x80030014,9,1
#define PRCM_CHIP_VERSION 0x80030000,12,4
#define PRCM_AFE_CHIP_MMSK_VER 0x80030004,8,8
#define PRCM_PRCM_AFE_REG_SOFT_RST_N 0x8003003C,12,1
#define PRCM_PRCM_CPU_SOFT_RST_N 0x8003003C,0,1
#define PRCM_PRCM_DIGRF_APB_DATA_BB0 0x80030074,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB1 0x80030078,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB2 0x8003007C,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB3 0x80030080,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB4 0x80030084,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB5 0x80030088,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB6 0x8003008C,0,20
#define PRCM_PRCM_DIGRF_APB_DATA_BB7 0x80030090,0,20
#define PRCM_PRCM_DIGRF_CAPT_DONE 0x80030070,24,8
#define PRCM_PRCM_DIGRF_START_CAPT 0x80030064,2,1
#define PRCM_PRCM_PAD_MUX_SOFT_RST_N 0x8003003C,11,1
#define PRCM_PRCM_XPT_PARALLEL_FIFO_RST_N 0x80030028,20,1
#define XPT_APPEND_BYTES0 0x90700008,4,2
#define XPT_APPEND_BYTES1 0x90700008,6,2
#define XPT_CLOCK_POLARITY0 0x90700010,16,1
#define XPT_CLOCK_POLARITY1 0x90700010,17,1
#define XPT_CLOCK_POLARITY2 0x90700010,18,1
#define XPT_CLOCK_POLARITY3 0x90700010,19,1
#define XPT_CLOCK_POLARITY4 0x90700010,20,1
#define XPT_CLOCK_POLARITY5 0x90700010,21,1
#define XPT_CLOCK_POLARITY6 0x90700010,22,1
#define XPT_CLOCK_POLARITY7 0x90700010,23,1
#define XPT_DSS_DVB_ENCAP_EN0 0x90700000,16,1
#define XPT_DSS_DVB_ENCAP_EN1 0x90700000,17,1
#define XPT_DSS_DVB_ENCAP_EN2 0x90700000,18,1
#define XPT_DSS_DVB_ENCAP_EN3 0x90700000,19,1
#define XPT_DSS_DVB_ENCAP_EN4 0x90700000,20,1
#define XPT_DSS_DVB_ENCAP_EN5 0x90700000,21,1
#define XPT_DSS_DVB_ENCAP_EN6 0x90700000,22,1
#define XPT_DSS_DVB_ENCAP_EN7 0x90700000,23,1
#define XPT_DVB_MATCH_BYTE 0x9070017C,16,8
#define XPT_DVB_PACKET_SIZE0 0x90700180,0,8
#define XPT_DVB_PACKET_SIZE1 0x90700180,8,8
#define XPT_DVB_PACKET_SIZE2 0x90700180,16,8
#define XPT_DVB_PACKET_SIZE3 0x90700180,24,8
#define XPT_ENABLE_DVB_INPUT0 0x90700178,0,1
#define XPT_ENABLE_DVB_INPUT1 0x90700178,1,1
#define XPT_ENABLE_DVB_INPUT2 0x90700178,2,1
#define XPT_ENABLE_DVB_INPUT3 0x90700178,3,1
#define XPT_ENABLE_INPUT0 0x90700000,0,1
#define XPT_ENABLE_INPUT1 0x90700000,1,1
#define XPT_ENABLE_INPUT2 0x90700000,2,1
#define XPT_ENABLE_INPUT3 0x90700000,3,1
#define XPT_ENABLE_INPUT4 0x90700000,4,1
#define XPT_ENABLE_INPUT5 0x90700000,5,1
#define XPT_ENABLE_INPUT6 0x90700000,6,1
#define XPT_ENABLE_INPUT7 0x90700000,7,1
#define XPT_ENABLE_OUTPUT0 0x9070000C,0,1
#define XPT_ENABLE_OUTPUT1 0x9070000C,1,1
#define XPT_ENABLE_OUTPUT2 0x9070000C,2,1
#define XPT_ENABLE_OUTPUT3 0x9070000C,3,1
#define XPT_ENABLE_OUTPUT4 0x9070000C,4,1
#define XPT_ENABLE_OUTPUT5 0x9070000C,5,1
#define XPT_ENABLE_OUTPUT6 0x9070000C,6,1
#define XPT_ENABLE_OUTPUT7 0x9070000C,7,1
#define XPT_ENABLE_PARALLEL_OUTPUT 0x90700010,27,1
#define XPT_ENABLE_PCR_COUNT 0x90700184,1,1
#define XPT_ERROR_REPLACE_SYNC0 0x9070000C,24,1
#define XPT_ERROR_REPLACE_SYNC1 0x9070000C,25,1
#define XPT_ERROR_REPLACE_SYNC2 0x9070000C,26,1
#define XPT_ERROR_REPLACE_SYNC3 0x9070000C,27,1
#define XPT_ERROR_REPLACE_SYNC4 0x9070000C,28,1
#define XPT_ERROR_REPLACE_SYNC5 0x9070000C,29,1
#define XPT_ERROR_REPLACE_SYNC6 0x9070000C,30,1
#define XPT_ERROR_REPLACE_SYNC7 0x9070000C,31,1
#define XPT_ERROR_REPLACE_VALID0 0x90700014,8,1
#define XPT_ERROR_REPLACE_VALID1 0x90700014,9,1
#define XPT_ERROR_REPLACE_VALID2 0x90700014,10,1
#define XPT_ERROR_REPLACE_VALID3 0x90700014,11,1
#define XPT_ERROR_REPLACE_VALID4 0x90700014,12,1
#define XPT_ERROR_REPLACE_VALID5 0x90700014,13,1
#define XPT_ERROR_REPLACE_VALID6 0x90700014,14,1
#define XPT_ERROR_REPLACE_VALID7 0x90700014,15,1
#define XPT_INP0_MERGE_HDR0 0x90700058,0,32
#define XPT_INP0_MERGE_HDR1 0x9070005C,0,32
#define XPT_INP0_MERGE_HDR2 0x90700060,0,32
#define XPT_INP1_MERGE_HDR0 0x90700064,0,32
#define XPT_INP1_MERGE_HDR1 0x90700068,0,32
#define XPT_INP1_MERGE_HDR2 0x9070006C,0,32
#define XPT_INP2_MERGE_HDR0 0x90700070,0,32
#define XPT_INP2_MERGE_HDR1 0x90700074,0,32
#define XPT_INP2_MERGE_HDR2 0x90700078,0,32
#define XPT_INP3_MERGE_HDR0 0x9070007C,0,32
#define XPT_INP3_MERGE_HDR1 0x90700080,0,32
#define XPT_INP3_MERGE_HDR2 0x90700084,0,32
#define XPT_INP4_MERGE_HDR0 0x90700088,0,32
#define XPT_INP4_MERGE_HDR1 0x9070008C,0,32
#define XPT_INP4_MERGE_HDR2 0x90700090,0,32
#define XPT_INP5_MERGE_HDR0 0x90700094,0,32
#define XPT_INP5_MERGE_HDR1 0x90700098,0,32
#define XPT_INP5_MERGE_HDR2 0x9070009C,0,32
#define XPT_INP6_MERGE_HDR0 0x907000A0,0,32
#define XPT_INP6_MERGE_HDR1 0x907000A4,0,32
#define XPT_INP6_MERGE_HDR2 0x907000A8,0,32
#define XPT_INP7_MERGE_HDR0 0x907000AC,0,32
#define XPT_INP7_MERGE_HDR1 0x907000B0,0,32
#define XPT_INP7_MERGE_HDR2 0x907000B4,0,32
#define XPT_INP_MODE_DSS0 0x90700000,8,1
#define XPT_INP_MODE_DSS1 0x90700000,9,1
#define XPT_INP_MODE_DSS2 0x90700000,10,1
#define XPT_INP_MODE_DSS3 0x90700000,11,1
#define XPT_INP_MODE_DSS4 0x90700000,12,1
#define XPT_INP_MODE_DSS5 0x90700000,13,1
#define XPT_INP_MODE_DSS6 0x90700000,14,1
#define XPT_INP_MODE_DSS7 0x90700000,15,1
#define XPT_KNOWN_PID_MUX_SELECT0 0x90700190,8,4
#define XPT_KNOWN_PID_MUX_SELECT1 0x907001B0,8,4
#define XPT_LSB_FIRST0 0x9070000C,16,1
#define XPT_LSB_FIRST1 0x9070000C,17,1
#define XPT_LSB_FIRST2 0x9070000C,18,1
#define XPT_LSB_FIRST3 0x9070000C,19,1
#define XPT_LSB_FIRST4 0x9070000C,20,1
#define XPT_LSB_FIRST5 0x9070000C,21,1
#define XPT_LSB_FIRST6 0x9070000C,22,1
#define XPT_LSB_FIRST7 0x9070000C,23,1
#define XPT_MODE_27MHZ 0x90700184,0,1
#define XPT_NCO_COUNT_MIN 0x90700044,16,8
#define XPT_OUTPUT_MODE_DSS0 0x9070000C,8,1
#define XPT_OUTPUT_MODE_DSS1 0x9070000C,9,1
#define XPT_OUTPUT_MODE_DSS2 0x9070000C,10,1
#define XPT_OUTPUT_MODE_DSS3 0x9070000C,11,1
#define XPT_OUTPUT_MODE_DSS4 0x9070000C,12,1
#define XPT_OUTPUT_MODE_DSS5 0x9070000C,13,1
#define XPT_OUTPUT_MODE_DSS6 0x9070000C,14,1
#define XPT_OUTPUT_MODE_DSS7 0x9070000C,15,1
#define XPT_OUTPUT_MODE_MUXGATING0 0x90700020,0,1
#define XPT_OUTPUT_MODE_MUXGATING1 0x90700020,1,1
#define XPT_OUTPUT_MODE_MUXGATING2 0x90700020,2,1
#define XPT_OUTPUT_MODE_MUXGATING3 0x90700020,3,1
#define XPT_OUTPUT_MODE_MUXGATING4 0x90700020,4,1
#define XPT_OUTPUT_MODE_MUXGATING5 0x90700020,5,1
#define XPT_OUTPUT_MODE_MUXGATING6 0x90700020,6,1
#define XPT_OUTPUT_MODE_MUXGATING7 0x90700020,7,1
#define XPT_OUTPUT_MUXSELECT0 0x9070001C,0,3
#define XPT_OUTPUT_MUXSELECT1 0x9070001C,4,3
#define XPT_OUTPUT_MUXSELECT2 0x9070001C,8,3
#define XPT_OUTPUT_MUXSELECT3 0x9070001C,12,3
#define XPT_OUTPUT_MUXSELECT4 0x9070001C,16,3
#define XPT_OUTPUT_MUXSELECT5 0x9070001C,20,3
#define XPT_PCR_RTS_CORRECTION_ENABLE 0x90700008,14,1
#define XPT_PID_DEFAULT_DROP0 0x90700190,12,1
#define XPT_PID_DEFAULT_DROP1 0x90700190,13,1
#define XPT_PID_DEFAULT_DROP2 0x90700190,14,1
#define XPT_PID_DEFAULT_DROP3 0x90700190,15,1
#define XPT_PID_DEFAULT_DROP4 0x907001B0,12,1
#define XPT_PID_DEFAULT_DROP5 0x907001B0,13,1
#define XPT_PID_DEFAULT_DROP6 0x907001B0,14,1
#define XPT_PID_DEFAULT_DROP7 0x907001B0,15,1
#define XPT_PID_MUX_SELECT0 0x90700190,4,4
#define XPT_PID_MUX_SELECT1 0x907001B0,4,4
#define XPT_STREAM_MUXMODE0 0x90700008,0,2
#define XPT_STREAM_MUXMODE1 0x90700008,2,2
#define XPT_SYNC_FULL_BYTE0 0x90700010,0,1
#define XPT_SYNC_FULL_BYTE1 0x90700010,1,1
#define XPT_SYNC_FULL_BYTE2 0x90700010,2,1
#define XPT_SYNC_FULL_BYTE3 0x90700010,3,1
#define XPT_SYNC_FULL_BYTE4 0x90700010,4,1
#define XPT_SYNC_FULL_BYTE5 0x90700010,5,1
#define XPT_SYNC_FULL_BYTE6 0x90700010,6,1
#define XPT_SYNC_FULL_BYTE7 0x90700010,7,1
#define XPT_SYNC_LOCK_THRESHOLD 0x9070017C,0,8
#define XPT_SYNC_MISS_THRESHOLD 0x9070017C,8,8
#define XPT_SYNC_POLARITY0 0x90700010,8,1
#define XPT_SYNC_POLARITY1 0x90700010,9,1
#define XPT_SYNC_POLARITY2 0x90700010,10,1
#define XPT_SYNC_POLARITY3 0x90700010,11,1
#define XPT_SYNC_POLARITY4 0x90700010,12,1
#define XPT_SYNC_POLARITY5 0x90700010,13,1
#define XPT_SYNC_POLARITY6 0x90700010,14,1
#define XPT_SYNC_POLARITY7 0x90700010,15,1
#define XPT_TS_CLK_OUT_EN0 0x907001D4,0,1
#define XPT_TS_CLK_OUT_EN1 0x907001D4,1,1
#define XPT_TS_CLK_OUT_EN2 0x907001D4,2,1
#define XPT_TS_CLK_OUT_EN3 0x907001D4,3,1
#define XPT_TS_CLK_OUT_EN4 0x907001D4,4,1
#define XPT_TS_CLK_OUT_EN5 0x907001D4,5,1
#define XPT_TS_CLK_OUT_EN6 0x907001D4,6,1
#define XPT_TS_CLK_OUT_EN7 0x907001D4,7,1
#define XPT_TS_CLK_OUT_EN_PARALLEL 0x907001D4,8,1
#define XPT_TS_CLK_PHASE0 0x90700018,0,3
#define XPT_TS_CLK_PHASE1 0x90700018,4,3
#define XPT_TS_CLK_PHASE2 0x90700018,8,3
#define XPT_TS_CLK_PHASE3 0x90700018,12,3
#define XPT_TS_CLK_PHASE4 0x90700018,16,3
#define XPT_TS_CLK_PHASE5 0x90700018,20,3
#define XPT_TS_CLK_PHASE6 0x90700018,24,3
#define XPT_TS_CLK_PHASE7 0x90700018,28,3
#define XPT_VALID_POLARITY0 0x90700014,0,1
#define XPT_VALID_POLARITY1 0x90700014,1,1
#define XPT_VALID_POLARITY2 0x90700014,2,1
#define XPT_VALID_POLARITY3 0x90700014,3,1
#define XPT_VALID_POLARITY4 0x90700014,4,1
#define XPT_VALID_POLARITY5 0x90700014,5,1
#define XPT_VALID_POLARITY6 0x90700014,6,1
#define XPT_VALID_POLARITY7 0x90700014,7,1
#define XPT_ZERO_FILL_COUNT 0x90700008,8,6

#define   XPT_PACKET_GAP_MIN_BASEADDR                            0x90700044
#define   XPT_PACKET_GAP_MIN_TIMER                               XPT_PACKET_GAP_MIN_BASEADDR,0,16
#define   XPT_NCO_COUNT_MIN0 XPT_PACKET_GAP_MIN_BASEADDR,16,8
#define   XPT_NCO_COUNT_BASEADDR                                 0x90700238
#define   XPT_NCO_COUNT_MIN1 XPT_NCO_COUNT_BASEADDR,0,8
#define   XPT_NCO_COUNT_MIN2 XPT_NCO_COUNT_BASEADDR,8,8
#define   XPT_NCO_COUNT_MIN3 XPT_NCO_COUNT_BASEADDR,16,8
#define   XPT_NCO_COUNT_MIN4 XPT_NCO_COUNT_BASEADDR,24,8

#define   XPT_NCO_COUNT_BASEADDR1                                0x9070023C
#define   XPT_NCO_COUNT_MIN5 XPT_NCO_COUNT_BASEADDR1,0,8
#define   XPT_NCO_COUNT_MIN6 XPT_NCO_COUNT_BASEADDR1,8,8
#define   XPT_NCO_COUNT_MIN7 XPT_NCO_COUNT_BASEADDR1,16,8

// V2 DigRF status register
#define   BB0_DIGRF_CAPT_DONE                                    0x908000CC,0,1
#define   PRCM_PRCM_CHIP_ID                                      0x80030000,0,12

#define   XPT_PID_BASEADDR                                       0x90708000
#define   XPT_PID_VALID0                                         XPT_PID_BASEADDR,0,1
#define   XPT_PID_DROP0                                          XPT_PID_BASEADDR,1,1
#define   XPT_PID_REMAP0                                         XPT_PID_BASEADDR,2,1
#define   XPT_PID_VALUE0                                         XPT_PID_BASEADDR,4,13
#define   XPT_PID_MASK0                                          XPT_PID_BASEADDR,19,13

#define   XPT_PID_REMAP_BASEADDR                                 0x90708004
#define   XPT_PID_REMAP_VALUE0                                   XPT_PID_REMAP_BASEADDR,0,13
#define   XPT_PID_PORT_ID0                                       XPT_PID_REMAP_BASEADDR,16,3

#define   XPT_KNOWN_PID_BASEADDR                                 0x90709000
#define   XPT_KNOWN_PID_VALID0                                   XPT_KNOWN_PID_BASEADDR,0,1
#define   XPT_KNOWN_PID_DROP0                                    XPT_KNOWN_PID_BASEADDR,1,1
#define   XPT_KNOWN_PID_REMAP0                                   XPT_KNOWN_PID_BASEADDR,2,1
#define   XPT_KNOWN_PID_REMAP_VALUE0                             XPT_KNOWN_PID_BASEADDR,16,13

#define   XPT_PID_BASEADDR1                                      0x9070A000
#define   XPT_PID_VALID1                                         XPT_PID_BASEADDR1,0,1
#define   XPT_PID_DROP1                                          XPT_PID_BASEADDR1,1,1
#define   XPT_PID_REMAP1                                         XPT_PID_BASEADDR1,2,1
#define   XPT_PID_VALUE1                                         XPT_PID_BASEADDR1,4,13
#define   XPT_PID_MASK1                                          XPT_PID_BASEADDR1,19,13

#define   XPT_PID_REMAP_BASEADDR1                                0x9070A004
#define   XPT_PID_REMAP_VALUE1                                   XPT_PID_REMAP_BASEADDR1,0,13

#define   XPT_KNOWN_PID_BASEADDR1                                0x9070B000
#define   XPT_KNOWN_PID_VALID1                                   XPT_KNOWN_PID_BASEADDR1,0,1
#define   XPT_KNOWN_PID_DROP1                                    XPT_KNOWN_PID_BASEADDR1,1,1
#define   XPT_KNOWN_PID_REMAP1                                   XPT_KNOWN_PID_BASEADDR1,2,1
#define   XPT_KNOWN_PID_REMAP_VALUE1                             XPT_KNOWN_PID_BASEADDR1,16,13

#define   XPT_BERT_LOCK_BASEADDR                                 0x907000B8
#define   XPT_BERT_LOCK_THRESHOLD                                XPT_BERT_LOCK_BASEADDR,0,8
#define   XPT_BERT_LOCK_WINDOW                                   XPT_BERT_LOCK_BASEADDR,8,8

#define   XPT_BERT_BASEADDR                                      0x907000BC
#define   XPT_BERT_ENABLE0                                       XPT_BERT_BASEADDR,0,1
#define   XPT_BERT_ENABLE1                                       XPT_BERT_BASEADDR,1,1
#define   XPT_BERT_ENABLE2                                       XPT_BERT_BASEADDR,2,1
#define   XPT_BERT_ENABLE3                                       XPT_BERT_BASEADDR,3,1
#define   XPT_BERT_ENABLE4                                       XPT_BERT_BASEADDR,4,1
#define   XPT_BERT_ENABLE5                                       XPT_BERT_BASEADDR,5,1
#define   XPT_BERT_ENABLE6                                       XPT_BERT_BASEADDR,6,1
#define   XPT_BERT_ENABLE7                                       XPT_BERT_BASEADDR,7,1
#define   XPT_BERT_SEQUENCE_PN23_0                               XPT_BERT_BASEADDR,8,1
#define   XPT_BERT_SEQUENCE_PN23_1                               XPT_BERT_BASEADDR,9,1
#define   XPT_BERT_SEQUENCE_PN23_2                               XPT_BERT_BASEADDR,10,1
#define   XPT_BERT_SEQUENCE_PN23_3                               XPT_BERT_BASEADDR,11,1
#define   XPT_BERT_SEQUENCE_PN23_4                               XPT_BERT_BASEADDR,12,1
#define   XPT_BERT_SEQUENCE_PN23_5                               XPT_BERT_BASEADDR,13,1
#define   XPT_BERT_SEQUENCE_PN23_6                               XPT_BERT_BASEADDR,14,1
#define   XPT_BERT_SEQUENCE_PN23_7                               XPT_BERT_BASEADDR,15,1
#define   XPT_LOCK_RESYNC0                                       XPT_BERT_BASEADDR,16,1
#define   XPT_LOCK_RESYNC1                                       XPT_BERT_BASEADDR,17,1
#define   XPT_LOCK_RESYNC2                                       XPT_BERT_BASEADDR,18,1
#define   XPT_LOCK_RESYNC3                                       XPT_BERT_BASEADDR,19,1
#define   XPT_LOCK_RESYNC4                                       XPT_BERT_BASEADDR,20,1
#define   XPT_LOCK_RESYNC5                                       XPT_BERT_BASEADDR,21,1
#define   XPT_LOCK_RESYNC6                                       XPT_BERT_BASEADDR,22,1
#define   XPT_LOCK_RESYNC7                                       XPT_BERT_BASEADDR,23,1
#define   XPT_BERT_DATA_POLARITY0                                XPT_BERT_BASEADDR,24,1
#define   XPT_BERT_DATA_POLARITY1                                XPT_BERT_BASEADDR,25,1
#define   XPT_BERT_DATA_POLARITY2                                XPT_BERT_BASEADDR,26,1
#define   XPT_BERT_DATA_POLARITY3                                XPT_BERT_BASEADDR,27,1
#define   XPT_BERT_DATA_POLARITY4                                XPT_BERT_BASEADDR,28,1
#define   XPT_BERT_DATA_POLARITY5                                XPT_BERT_BASEADDR,29,1
#define   XPT_BERT_DATA_POLARITY6                                XPT_BERT_BASEADDR,30,1
#define   XPT_BERT_DATA_POLARITY7                                XPT_BERT_BASEADDR,31,1

#define   XPT_BERT_INVERT_BASEADDR                               0x907000C0
#define   XPT_BERT_INVERT_DATA0                                  XPT_BERT_INVERT_BASEADDR,0,1
#define   XPT_BERT_INVERT_DATA1                                  XPT_BERT_INVERT_BASEADDR,1,1
#define   XPT_BERT_INVERT_DATA2                                  XPT_BERT_INVERT_BASEADDR,2,1
#define   XPT_BERT_INVERT_DATA3                                  XPT_BERT_INVERT_BASEADDR,3,1
#define   XPT_BERT_INVERT_DATA4                                  XPT_BERT_INVERT_BASEADDR,4,1
#define   XPT_BERT_INVERT_DATA5                                  XPT_BERT_INVERT_BASEADDR,5,1
#define   XPT_BERT_INVERT_DATA6                                  XPT_BERT_INVERT_BASEADDR,6,1
#define   XPT_BERT_INVERT_DATA7                                  XPT_BERT_INVERT_BASEADDR,7,1
#define   XPT_BERT_INVERT_SEQUENCE0                              XPT_BERT_INVERT_BASEADDR,8,1
#define   XPT_BERT_INVERT_SEQUENCE1                              XPT_BERT_INVERT_BASEADDR,9,1
#define   XPT_BERT_INVERT_SEQUENCE2                              XPT_BERT_INVERT_BASEADDR,10,1
#define   XPT_BERT_INVERT_SEQUENCE3                              XPT_BERT_INVERT_BASEADDR,11,1
#define   XPT_BERT_INVERT_SEQUENCE4                              XPT_BERT_INVERT_BASEADDR,12,1
#define   XPT_BERT_INVERT_SEQUENCE5                              XPT_BERT_INVERT_BASEADDR,13,1
#define   XPT_BERT_INVERT_SEQUENCE6                              XPT_BERT_INVERT_BASEADDR,14,1
#define   XPT_BERT_INVERT_SEQUENCE7                              XPT_BERT_INVERT_BASEADDR,15,1
#define   XPT_BERT_OUTPUT_POLARITY0                              XPT_BERT_INVERT_BASEADDR,16,1
#define   XPT_BERT_OUTPUT_POLARITY1                              XPT_BERT_INVERT_BASEADDR,17,1
#define   XPT_BERT_OUTPUT_POLARITY2                              XPT_BERT_INVERT_BASEADDR,18,1
#define   XPT_BERT_OUTPUT_POLARITY3                              XPT_BERT_INVERT_BASEADDR,19,1
#define   XPT_BERT_OUTPUT_POLARITY4                              XPT_BERT_INVERT_BASEADDR,20,1
#define   XPT_BERT_OUTPUT_POLARITY5                              XPT_BERT_INVERT_BASEADDR,21,1
#define   XPT_BERT_OUTPUT_POLARITY6                              XPT_BERT_INVERT_BASEADDR,22,1
#define   XPT_BERT_OUTPUT_POLARITY7                              XPT_BERT_INVERT_BASEADDR,23,1

#define   XPT_BERT_HEADER_BASEADDR                               0x907000C4
#define   XPT_BERT_HEADER_MODE0                                  XPT_BERT_HEADER_BASEADDR,0,2
#define   XPT_BERT_HEADER_MODE1                                  XPT_BERT_HEADER_BASEADDR,2,2
#define   XPT_BERT_HEADER_MODE2                                  XPT_BERT_HEADER_BASEADDR,4,2
#define   XPT_BERT_HEADER_MODE3                                  XPT_BERT_HEADER_BASEADDR,6,2
#define   XPT_BERT_HEADER_MODE4                                  XPT_BERT_HEADER_BASEADDR,8,2
#define   XPT_BERT_HEADER_MODE5                                  XPT_BERT_HEADER_BASEADDR,10,2
#define   XPT_BERT_HEADER_MODE6                                  XPT_BERT_HEADER_BASEADDR,12,2
#define   XPT_BERT_HEADER_MODE7                                  XPT_BERT_HEADER_BASEADDR,14,2

#define   XPT_BERT_BASEADDR1                                     0x907000C8
#define   XPT_BERT_LOCKED0                                       XPT_BERT_BASEADDR1,0,1
#define   XPT_BERT_LOCKED1                                       XPT_BERT_BASEADDR1,1,1
#define   XPT_BERT_LOCKED2                                       XPT_BERT_BASEADDR1,2,1
#define   XPT_BERT_LOCKED3                                       XPT_BERT_BASEADDR1,3,1
#define   XPT_BERT_LOCKED4                                       XPT_BERT_BASEADDR1,4,1
#define   XPT_BERT_LOCKED5                                       XPT_BERT_BASEADDR1,5,1
#define   XPT_BERT_LOCKED6                                       XPT_BERT_BASEADDR1,6,1
#define   XPT_BERT_LOCKED7                                       XPT_BERT_BASEADDR1,7,1
#define   XPT_BERT_BIT_COUNT_SAT0                                XPT_BERT_BASEADDR1,8,1
#define   XPT_BERT_BIT_COUNT_SAT1                                XPT_BERT_BASEADDR1,9,1
#define   XPT_BERT_BIT_COUNT_SAT2                                XPT_BERT_BASEADDR1,10,1
#define   XPT_BERT_BIT_COUNT_SAT3                                XPT_BERT_BASEADDR1,11,1
#define   XPT_BERT_BIT_COUNT_SAT4                                XPT_BERT_BASEADDR1,12,1
#define   XPT_BERT_BIT_COUNT_SAT5                                XPT_BERT_BASEADDR1,13,1
#define   XPT_BERT_BIT_COUNT_SAT6                                XPT_BERT_BASEADDR1,14,1
#define   XPT_BERT_BIT_COUNT_SAT7                                XPT_BERT_BASEADDR1,15,1

#define   XPT_BERT_BIT_COUNT0_BASEADDR                           0x907000CC
#define   XPT_BERT_BIT_COUNT0_LO                                 XPT_BERT_BIT_COUNT0_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT0_BASEADDR1                          0x907000D0
#define   XPT_BERT_BIT_COUNT0_HI                                 XPT_BERT_BIT_COUNT0_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT1_BASEADDR                           0x907000D4
#define   XPT_BERT_BIT_COUNT1_LO                                 XPT_BERT_BIT_COUNT1_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT1_BASEADDR1                          0x907000D8
#define   XPT_BERT_BIT_COUNT1_HI                                 XPT_BERT_BIT_COUNT1_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT2_BASEADDR                           0x907000DC
#define   XPT_BERT_BIT_COUNT2_LO                                 XPT_BERT_BIT_COUNT2_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT2_BASEADDR1                          0x907000E0
#define   XPT_BERT_BIT_COUNT2_HI                                 XPT_BERT_BIT_COUNT2_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT3_BASEADDR                           0x907000E4
#define   XPT_BERT_BIT_COUNT3_LO                                 XPT_BERT_BIT_COUNT3_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT3_BASEADDR1                          0x907000E8
#define   XPT_BERT_BIT_COUNT3_HI                                 XPT_BERT_BIT_COUNT3_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT4_BASEADDR                           0x907000EC
#define   XPT_BERT_BIT_COUNT4_LO                                 XPT_BERT_BIT_COUNT4_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT4_BASEADDR1                          0x907000F0
#define   XPT_BERT_BIT_COUNT4_HI                                 XPT_BERT_BIT_COUNT4_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT5_BASEADDR                           0x907000F4
#define   XPT_BERT_BIT_COUNT5_LO                                 XPT_BERT_BIT_COUNT5_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT5_BASEADDR1                          0x907000F8
#define   XPT_BERT_BIT_COUNT5_HI                                 XPT_BERT_BIT_COUNT5_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT6_BASEADDR                           0x907000FC
#define   XPT_BERT_BIT_COUNT6_LO                                 XPT_BERT_BIT_COUNT6_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT6_BASEADDR1                          0x90700100
#define   XPT_BERT_BIT_COUNT6_HI                                 XPT_BERT_BIT_COUNT6_BASEADDR1,0,18

#define   XPT_BERT_BIT_COUNT7_BASEADDR                           0x90700104
#define   XPT_BERT_BIT_COUNT7_LO                                 XPT_BERT_BIT_COUNT7_BASEADDR,0,32

#define   XPT_BERT_BIT_COUNT7_BASEADDR1                          0x90700108
#define   XPT_BERT_BIT_COUNT7_HI                                 XPT_BERT_BIT_COUNT7_BASEADDR1,0,18

#define   XPT_BERT_ERR_COUNT0_BASEADDR                           0x9070010C
#define   XPT_BERT_ERR_COUNT0_LO                                 XPT_BERT_ERR_COUNT0_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT0_BASEADDR1                          0x90700110
#define   XPT_BERT_ERR_COUNT0_HI                                 XPT_BERT_ERR_COUNT0_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT1_BASEADDR                           0x90700114
#define   XPT_BERT_ERR_COUNT1_LO                                 XPT_BERT_ERR_COUNT1_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT1_BASEADDR1                          0x90700118
#define   XPT_BERT_ERR_COUNT1_HI                                 XPT_BERT_ERR_COUNT1_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT2_BASEADDR                           0x9070011C
#define   XPT_BERT_ERR_COUNT2_LO                                 XPT_BERT_ERR_COUNT2_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT2_BASEADDR1                          0x90700120
#define   XPT_BERT_ERR_COUNT2_HI                                 XPT_BERT_ERR_COUNT2_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT3_BASEADDR                           0x90700124
#define   XPT_BERT_ERR_COUNT3_LO                                 XPT_BERT_ERR_COUNT3_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT3_BASEADDR1                          0x90700128
#define   XPT_BERT_ERR_COUNT3_HI                                 XPT_BERT_ERR_COUNT3_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT4_BASEADDR                           0x9070012C
#define   XPT_BERT_ERR_COUNT4_LO                                 XPT_BERT_ERR_COUNT4_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT4_BASEADDR1                          0x90700130
#define   XPT_BERT_ERR_COUNT4_HI                                 XPT_BERT_ERR_COUNT4_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT5_BASEADDR                           0x90700134
#define   XPT_BERT_ERR_COUNT5_LO                                 XPT_BERT_ERR_COUNT5_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT5_BASEADDR1                          0x90700138
#define   XPT_BERT_ERR_COUNT5_HI                                 XPT_BERT_ERR_COUNT5_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT6_BASEADDR                           0x9070013C
#define   XPT_BERT_ERR_COUNT6_LO                                 XPT_BERT_ERR_COUNT6_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT6_BASEADDR1                          0x90700140
#define   XPT_BERT_ERR_COUNT6_HI                                 XPT_BERT_ERR_COUNT6_BASEADDR1,0,8

#define   XPT_BERT_ERR_COUNT7_BASEADDR                           0x90700144
#define   XPT_BERT_ERR_COUNT7_LO                                 XPT_BERT_ERR_COUNT7_BASEADDR,0,32

#define   XPT_BERT_ERR_COUNT7_BASEADDR1                          0x90700148
#define   XPT_BERT_ERR_COUNT7_HI                                 XPT_BERT_ERR_COUNT7_BASEADDR1,0,8

#define   XPT_BERT_ERROR_BASEADDR                                0x9070014C
#define   XPT_BERT_ERROR_INSERT                                  XPT_BERT_ERROR_BASEADDR,0,24

#define   XPT_BERT_ANALYZER_BASEADDR                             0x90700150
#define   XPT_BERT_ANALYZER_ENABLE                               XPT_BERT_ANALYZER_BASEADDR,0,1
#define   XPT_BERT_ANALYZER_PORT                                 XPT_BERT_ANALYZER_BASEADDR,4,3
#define   XPT_BERT_ANALYZER_ERR_THRES                            XPT_BERT_ANALYZER_BASEADDR,15,17

#define   XPT_BERT_ANALYZER_BASEADDR1                            0x90700154
#define   XPT_BERT_ANALYZER_START                                XPT_BERT_ANALYZER_BASEADDR1,0,32

#define   XPT_BERT_ANALYZER_BASEADDR2                            0x90700158
#define   XPT_BERT_ANALYZER_TSTAMP0                              XPT_BERT_ANALYZER_BASEADDR2,0,32

#define   XPT_BERT_ANALYZER_BASEADDR3                            0x9070015C
#define   XPT_BERT_ANALYZER_TSTAMP1                              XPT_BERT_ANALYZER_BASEADDR3,0,32

#define   XPT_BERT_ANALYZER_BASEADDR4                            0x90700160
#define   XPT_BERT_ANALYZER_TSTAMP2                              XPT_BERT_ANALYZER_BASEADDR4,0,32

#define   XPT_BERT_ANALYZER_BASEADDR5                            0x90700164
#define   XPT_BERT_ANALYZER_TSTAMP3                              XPT_BERT_ANALYZER_BASEADDR5,0,32

#define   XPT_BERT_ANALYZER_BASEADDR6                            0x90700168
#define   XPT_BERT_ANALYZER_TSTAMP4                              XPT_BERT_ANALYZER_BASEADDR6,0,32

#define   XPT_BERT_ANALYZER_BASEADDR7                            0x9070016C
#define   XPT_BERT_ANALYZER_TSTAMP5                              XPT_BERT_ANALYZER_BASEADDR7,0,32

#define   XPT_BERT_ANALYZER_BASEADDR8                            0x90700170
#define   XPT_BERT_ANALYZER_TSTAMP6                              XPT_BERT_ANALYZER_BASEADDR8,0,32

#define   XPT_BERT_ANALYZER_BASEADDR9                            0x90700174
#define   XPT_BERT_ANALYZER_TSTAMP7                              XPT_BERT_ANALYZER_BASEADDR9,0,32

#define   XPT_DMD0_BASEADDR                                      0x9070024C
#define   XPT_DMD0_SEL                                           XPT_DMD0_BASEADDR,0,3
#define   XPT_DMD1_SEL                                           XPT_DMD0_BASEADDR,4,3
#define   XPT_DMD2_SEL                                           XPT_DMD0_BASEADDR,8,3
#define   XPT_DMD3_SEL                                           XPT_DMD0_BASEADDR,12,3
#define   XPT_DMD4_SEL                                           XPT_DMD0_BASEADDR,16,3
#define   XPT_DMD5_SEL                                           XPT_DMD0_BASEADDR,20,3
#define   XPT_DMD6_SEL                                           XPT_DMD0_BASEADDR,24,3
#define   XPT_DMD7_SEL                                           XPT_DMD0_BASEADDR,28,3

// V2 AGC Gain Freeze & step
#define   DBG_ENABLE_DISABLE_AGC                                 (0x3FFFCF60) // 1: DISABLE, 0:ENABLE
#define   WB_DFE0_DFE_FB_RF1_BASEADDR                            0x903004A4
#define   WB_DFE0_DFE_FB_RF1_BO                                  WB_DFE0_DFE_FB_RF1_BASEADDR,0,3
#define   WB_DFE0_DFE_FB_RF2_BO                                  WB_DFE0_DFE_FB_RF1_BASEADDR,4,4
#define   WB_DFE0_DFE_FB_LNA_BO                                  WB_DFE0_DFE_FB_RF1_BASEADDR,8,2

#define   WB_DFE1_DFE_FB_RF1_BASEADDR                            0x904004A4
#define   WB_DFE1_DFE_FB_RF1_BO                                  WB_DFE1_DFE_FB_RF1_BASEADDR,0,3
#define   WB_DFE1_DFE_FB_RF2_BO                                  WB_DFE1_DFE_FB_RF1_BASEADDR,4,4
#define   WB_DFE1_DFE_FB_LNA_BO                                  WB_DFE1_DFE_FB_RF1_BASEADDR,8,2

#define   WB_DFE2_DFE_FB_RF1_BASEADDR                            0x905004A4
#define   WB_DFE2_DFE_FB_RF1_BO                                  WB_DFE2_DFE_FB_RF1_BASEADDR,0,3
#define   WB_DFE2_DFE_FB_RF2_BO                                  WB_DFE2_DFE_FB_RF1_BASEADDR,4,4
#define   WB_DFE2_DFE_FB_LNA_BO                                  WB_DFE2_DFE_FB_RF1_BASEADDR,8,2

#define   WB_DFE3_DFE_FB_RF1_BASEADDR                            0x906004A4
#define   WB_DFE3_DFE_FB_RF1_BO                                  WB_DFE3_DFE_FB_RF1_BASEADDR,0,3
#define   WB_DFE3_DFE_FB_RF2_BO                                  WB_DFE3_DFE_FB_RF1_BASEADDR,4,4
#define   WB_DFE3_DFE_FB_LNA_BO                                  WB_DFE3_DFE_FB_RF1_BASEADDR,8,2

#define   AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR                0x90200104
#define   AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_2                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,0,1
#define   AFE_REG_D2A_TA_RFFE_RF1_BO_1P8_3                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,1,1
#define   AFE_REG_D2A_TB_RFFE_LNA_BO_1P8_2                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,2,1
#define   AFE_REG_D2A_TB_RFFE_RF1_BO_1P8_3                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,3,1
#define   AFE_REG_D2A_TC_RFFE_LNA_BO_1P8_2                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,4,1
#define   AFE_REG_D2A_TC_RFFE_RF1_BO_1P8_3                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,5,1
#define   AFE_REG_D2A_TD_RFFE_LNA_BO_1P8_2                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,6,1
#define   AFE_REG_D2A_TD_RFFE_RF1_BO_1P8_3                       AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR,7,1

#define   AFE_REG_AFE_REG_SPARE_BASEADDR                         0x902000A0
#define   AFE_REG_D2A_TA_RFFE_RF1_CAP_1P8                        AFE_REG_AFE_REG_SPARE_BASEADDR,13,5

#define   AFE_REG_AFE_REG_SPARE_BASEADDR1                        0x902000B4
#define   AFE_REG_D2A_TB_RFFE_RF1_CAP_1P8                        AFE_REG_AFE_REG_SPARE_BASEADDR1,13,5

#define   AFE_REG_AFE_REG_SPARE_BASEADDR2                        0x902000C4
#define   AFE_REG_D2A_TC_RFFE_RF1_CAP_1P8                        AFE_REG_AFE_REG_SPARE_BASEADDR2,13,5

#define   AFE_REG_AFE_REG_SPARE_BASEADDR3                        0x902000D4
#define   AFE_REG_D2A_TD_RFFE_RF1_CAP_1P8                        AFE_REG_AFE_REG_SPARE_BASEADDR3,13,5

#define   WB_DFE0_DFE_FB_AGC_BASEADDR                            0x90300498
#define   WB_DFE0_DFE_FB_AGC_APPLY                               WB_DFE0_DFE_FB_AGC_BASEADDR,0,1

#define   WB_DFE1_DFE_FB_AGC_BASEADDR                            0x90400498
#define   WB_DFE1_DFE_FB_AGC_APPLY                               WB_DFE1_DFE_FB_AGC_BASEADDR,0,1

#define   WB_DFE2_DFE_FB_AGC_BASEADDR                            0x90500498
#define   WB_DFE2_DFE_FB_AGC_APPLY                               WB_DFE2_DFE_FB_AGC_BASEADDR,0,1

#define   WB_DFE3_DFE_FB_AGC_BASEADDR                            0x90600498
#define   WB_DFE3_DFE_FB_AGC_APPLY                               WB_DFE3_DFE_FB_AGC_BASEADDR,0,1

#define   WDT_WD_INT_BASEADDR                                    0x8002000C
#define   WDT_WD_INT_STATUS                                      WDT_WD_INT_BASEADDR,0,1

#define   FSK_TX_FTM_BASEADDR                                    0x80090000
#define   FSK_TX_FTM_OE                                          FSK_TX_FTM_BASEADDR,12,1
#define   FSK_TX_FTM_TX_EN                                       FSK_TX_FTM_BASEADDR,10,1
#define   FSK_TX_FTM_FORCE_CARRIER_ON                            FSK_TX_FTM_BASEADDR,1,1
#define   FSK_TX_FTM_FORCE_MARK_SPACE                            FSK_TX_FTM_BASEADDR,0,1

#define   FSK_TX_FTM_TX_CNT_BASEADDR                             0x80090018
#define   FSK_TX_FTM_TX_CNT_INT                                  FSK_TX_FTM_TX_CNT_BASEADDR,8,4
#define   FSK_TX_FTM_TX_INT_EN                                   FSK_TX_FTM_TX_CNT_BASEADDR,4,1
#define   FSK_TX_FTM_TX_INT_SRC_SEL                              FSK_TX_FTM_TX_CNT_BASEADDR,0,2

#define   AFE_REG_D2A_FSK_BIAS_BASEADDR                          0x90200040
#define   AFE_REG_D2A_FSK_BIAS_EN                                AFE_REG_D2A_FSK_BIAS_BASEADDR,0,1
#define   AFE_REG_D2A_FSK_TEST_EN                                AFE_REG_D2A_FSK_BIAS_BASEADDR,10,1
#define   AFE_REG_D2A_FSK_TEST_MODE                              AFE_REG_D2A_FSK_BIAS_BASEADDR,11,4
#define   AFE_REG_D2A_FSK_TERM_INT_EN                            AFE_REG_D2A_FSK_BIAS_BASEADDR,15,1
#define   AFE_REG_D2A_FSK_RESETB_1P8                             AFE_REG_D2A_FSK_BIAS_BASEADDR,16,1
#define   AFE_REG_D2A_FSK_REG_EN_1P8                             AFE_REG_D2A_FSK_BIAS_BASEADDR,17,1
#define   AFE_REG_D2A_FSK_REG_EN_LKG_1P8                         AFE_REG_D2A_FSK_BIAS_BASEADDR,18,1
#define   AFE_REG_D2A_FSK_REG_AMP_1P8                            AFE_REG_D2A_FSK_BIAS_BASEADDR,19,3
#define   AFE_REG_D2A_FSK_REG_TEST_CTRL_1P8                      AFE_REG_D2A_FSK_BIAS_BASEADDR,22,2
#define   AFE_REG_D2A_DSQ_RX_MODE                                AFE_REG_D2A_FSK_BIAS_BASEADDR,24,1
#define   AFE_REG_D2A_DSQ_RX_EN                                  AFE_REG_D2A_FSK_BIAS_BASEADDR,25,1
#define   AFE_REG_D2A_DSQ_HYST                                   AFE_REG_D2A_FSK_BIAS_BASEADDR,26,2
#define   AFE_REG_D2A_DSQ_RESETB_1P8                             AFE_REG_D2A_FSK_BIAS_BASEADDR,28,1
#define   AFE_REG_D2A_FSK_CLKRX_ENA                              AFE_REG_D2A_FSK_BIAS_BASEADDR,29,1

#define   DMD_TEI_BASEADDR                                       0x3FFFEBE0
#define   DMD_TEI_ENA                                            DMD_TEI_BASEADDR,0,1

#define   xpt_shm_input_control0  0x90700270,0,8
#define   xpt_shm_input_control1  0x90700270,8,8
#define   xpt_shm_input_control2  0x90700270,16,8
#define   xpt_shm_input_control3  0x90700270,24,8
#define   xpt_shm_input_control4  0x90700274,0,8
#define   xpt_shm_input_control5  0x90700274,8,8
#define   xpt_shm_input_control6  0x90700274,16,8
#define   xpt_shm_input_control7  0x90700274,24,8


#define   xpt_shm_output_control0  0x90700278,0,8
#define   xpt_shm_output_control1  0x90700278,8,8
#define   xpt_shm_output_control2  0x90700278,16,8
#define   xpt_shm_output_control3  0x90700278,24,8
#define   xpt_shm_output_control4  0x9070027C,0,8
#define   xpt_shm_output_control5  0x9070027C,8,8
#define   xpt_shm_output_control6  0x9070027C,16,8
#define   xpt_shm_output_control7  0x9070027C,24,8

#define   xpt_mode_27mhz           0x90700184,0,1
#define   xpt_enable_pcr_count     0x90700184,1,1

#define   xcpu_ctrl_003c_reg       0x9072003C,0,4

#ifdef __cplusplus
}
#endif

#endif //__MXL58X_REGISTERS_H__
