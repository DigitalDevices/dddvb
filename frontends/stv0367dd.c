/*
 * stv0367dd: STV0367 DVB-C/T demodulator driver
 *
 * Copyright (C) 2011 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <asm/div64.h>

#include "dvb_frontend.h"
#include "stv0367dd.h"
#include "stv0367dd_regs.h"

enum omode { OM_NONE, OM_DVBT, OM_DVBC, OM_QAM_ITU_C };
enum {  QAM_MOD_QAM4 = 0,
	QAM_MOD_QAM16,
	QAM_MOD_QAM32,
	QAM_MOD_QAM64,
	QAM_MOD_QAM128,
	QAM_MOD_QAM256,
	QAM_MOD_QAM512,
	QAM_MOD_QAM1024
};

enum {QAM_SPECT_NORMAL, QAM_SPECT_INVERTED };

enum {
	QAM_FEC_A = 1,					/* J83 Annex A */
	QAM_FEC_B = (1<<1),				/* J83 Annex B */
	QAM_FEC_C = (1<<2)				/* J83 Annex C */
};

enum EDemodState { Off, QAMSet, OFDMSet, QAMStarted, OFDMStarted };

struct stv_state {
	struct dvb_frontend frontend;
	fe_modulation_t modulation;
	u32 symbol_rate;
	u32 bandwidth;
	struct device *dev;

	struct i2c_adapter *i2c;
	u8     adr;
	u8     cont_clock;
	void  *priv;

	struct mutex mutex;
	struct mutex ctlock;

	u32 master_clock;
	u32 adc_clock;
	u8 ID;
	u8 I2CRPT;
	u32 omode;
	u8  qam_inversion;

	s32 IF;

	s32    m_FECTimeOut;
	s32    m_DemodTimeOut;
	s32    m_SignalTimeOut;
	s32    m_DemodLockTime;
	s32    m_FFTTimeOut;
	s32    m_TSTimeOut;

	bool    m_bFirstTimeLock;

	u8    m_Save_QAM_AGC_CTL;

	enum EDemodState demod_state;

	u8    m_OFDM_FFTMode;          // 0 = 2k, 1 = 8k, 2 = 4k
	u8    m_OFDM_Modulation;   //
	u8    m_OFDM_FEC;          //
	u8    m_OFDM_Guard;

	u32   ucblocks;
	u32   ber;
};

struct init_table {
	u16  adr;
	u8   data;
};

struct init_table base_init[] = {
	{ R367_IOCFG0,     0x80 },
	{ R367_DAC0R,      0x00 },
	{ R367_IOCFG1,     0x00 },
	{ R367_DAC1R,      0x00 },
	{ R367_IOCFG2,     0x00 },
	{ R367_SDFR,       0x00 },
	{ R367_AUX_CLK,    0x00 },
	{ R367_FREESYS1,   0x00 },
	{ R367_FREESYS2,   0x00 },
	{ R367_FREESYS3,   0x00 },
	{ R367_GPIO_CFG,   0x55 },
	{ R367_GPIO_CMD,   0x01 },
	{ R367_TSTRES,     0x00 },
	{ R367_ANACTRL,    0x00 },
	{ R367_TSTBUS,     0x00 },
	{ R367_RF_AGC2,    0x20 },
	{ R367_ANADIGCTRL, 0x0b },
	{ R367_PLLMDIV,    0x01 },
	{ R367_PLLNDIV,    0x08 },
	{ R367_PLLSETUP,   0x18 },
	{ R367_DUAL_AD12,  0x04 },
	{ R367_TSTBIST,    0x00 },
	{ 0x0000,          0x00 }
};

struct init_table qam_init[] = {
	{ R367_QAM_CTRL_1,                  0x06 },// Orginal 0x04
	{ R367_QAM_CTRL_2,                  0x03 },
	{ R367_QAM_IT_STATUS1,              0x2b },
	{ R367_QAM_IT_STATUS2,              0x08 },
	{ R367_QAM_IT_EN1,                  0x00 },
	{ R367_QAM_IT_EN2,                  0x00 },
	{ R367_QAM_CTRL_STATUS,             0x04 },
	{ R367_QAM_TEST_CTL,                0x00 },
	{ R367_QAM_AGC_CTL,                 0x73 },
	{ R367_QAM_AGC_IF_CFG,              0x50 },
	{ R367_QAM_AGC_RF_CFG,              0x02 },// RF Freeze
	{ R367_QAM_AGC_PWM_CFG,             0x03 },
	{ R367_QAM_AGC_PWR_REF_L,           0x5a },
	{ R367_QAM_AGC_PWR_REF_H,           0x00 },
	{ R367_QAM_AGC_RF_TH_L,             0xff },
	{ R367_QAM_AGC_RF_TH_H,             0x07 },
	{ R367_QAM_AGC_IF_LTH_L,            0x00 },
	{ R367_QAM_AGC_IF_LTH_H,            0x08 },
	{ R367_QAM_AGC_IF_HTH_L,            0xff },
	{ R367_QAM_AGC_IF_HTH_H,            0x07 },
	{ R367_QAM_AGC_PWR_RD_L,            0xa0 },
	{ R367_QAM_AGC_PWR_RD_M,            0xe9 },
	{ R367_QAM_AGC_PWR_RD_H,            0x03 },
	{ R367_QAM_AGC_PWM_IFCMD_L,         0xe4 },
	{ R367_QAM_AGC_PWM_IFCMD_H,         0x00 },
	{ R367_QAM_AGC_PWM_RFCMD_L,         0xff },
	{ R367_QAM_AGC_PWM_RFCMD_H,         0x07 },
	{ R367_QAM_IQDEM_CFG,               0x01 },
	{ R367_QAM_MIX_NCO_LL,              0x22 },
	{ R367_QAM_MIX_NCO_HL,              0x96 },
	{ R367_QAM_MIX_NCO_HH,              0x55 },
	{ R367_QAM_SRC_NCO_LL,              0xff },
	{ R367_QAM_SRC_NCO_LH,              0x0c },
	{ R367_QAM_SRC_NCO_HL,              0xf5 },
	{ R367_QAM_SRC_NCO_HH,              0x20 },
	{ R367_QAM_IQDEM_GAIN_SRC_L,        0x06 },
	{ R367_QAM_IQDEM_GAIN_SRC_H,        0x01 },
	{ R367_QAM_IQDEM_DCRM_CFG_LL,       0xfe },
	{ R367_QAM_IQDEM_DCRM_CFG_LH,       0xff },
	{ R367_QAM_IQDEM_DCRM_CFG_HL,       0x0f },
	{ R367_QAM_IQDEM_DCRM_CFG_HH,       0x00 },
	{ R367_QAM_IQDEM_ADJ_COEFF0,        0x34 },
	{ R367_QAM_IQDEM_ADJ_COEFF1,        0xae },
	{ R367_QAM_IQDEM_ADJ_COEFF2,        0x46 },
	{ R367_QAM_IQDEM_ADJ_COEFF3,        0x77 },
	{ R367_QAM_IQDEM_ADJ_COEFF4,        0x96 },
	{ R367_QAM_IQDEM_ADJ_COEFF5,        0x69 },
	{ R367_QAM_IQDEM_ADJ_COEFF6,        0xc7 },
	{ R367_QAM_IQDEM_ADJ_COEFF7,        0x01 },
	{ R367_QAM_IQDEM_ADJ_EN,            0x04 },
	{ R367_QAM_IQDEM_ADJ_AGC_REF,       0x94 },
	{ R367_QAM_ALLPASSFILT1,            0xc9 },
	{ R367_QAM_ALLPASSFILT2,            0x2d },
	{ R367_QAM_ALLPASSFILT3,            0xa3 },
	{ R367_QAM_ALLPASSFILT4,            0xfb },
	{ R367_QAM_ALLPASSFILT5,            0xf6 },
	{ R367_QAM_ALLPASSFILT6,            0x45 },
	{ R367_QAM_ALLPASSFILT7,            0x6f },
	{ R367_QAM_ALLPASSFILT8,            0x7e },
	{ R367_QAM_ALLPASSFILT9,            0x05 },
	{ R367_QAM_ALLPASSFILT10,           0x0a },
	{ R367_QAM_ALLPASSFILT11,           0x51 },
	{ R367_QAM_TRL_AGC_CFG,             0x20 },
	{ R367_QAM_TRL_LPF_CFG,             0x28 },
	{ R367_QAM_TRL_LPF_ACQ_GAIN,        0x44 },
	{ R367_QAM_TRL_LPF_TRK_GAIN,        0x22 },
	{ R367_QAM_TRL_LPF_OUT_GAIN,        0x03 },
	{ R367_QAM_TRL_LOCKDET_LTH,         0x04 },
	{ R367_QAM_TRL_LOCKDET_HTH,         0x11 },
	{ R367_QAM_TRL_LOCKDET_TRGVAL,      0x20 },
	{ R367_QAM_IQ_QAM,			0x01 },
	{ R367_QAM_FSM_STATE,               0xa0 },
	{ R367_QAM_FSM_CTL,                 0x08 },
	{ R367_QAM_FSM_STS,                 0x0c },
	{ R367_QAM_FSM_SNR0_HTH,            0x00 },
	{ R367_QAM_FSM_SNR1_HTH,            0x00 },
	{ R367_QAM_FSM_SNR2_HTH,            0x00 },
	{ R367_QAM_FSM_SNR0_LTH,            0x00 },
	{ R367_QAM_FSM_SNR1_LTH,            0x00 },
	{ R367_QAM_FSM_EQA1_HTH,            0x00 },
	{ R367_QAM_FSM_TEMPO,               0x32 },
	{ R367_QAM_FSM_CONFIG,              0x03 },
	{ R367_QAM_EQU_I_TESTTAP_L,         0x11 },
	{ R367_QAM_EQU_I_TESTTAP_M,         0x00 },
	{ R367_QAM_EQU_I_TESTTAP_H,         0x00 },
	{ R367_QAM_EQU_TESTAP_CFG,          0x00 },
	{ R367_QAM_EQU_Q_TESTTAP_L,         0xff },
	{ R367_QAM_EQU_Q_TESTTAP_M,         0x00 },
	{ R367_QAM_EQU_Q_TESTTAP_H,         0x00 },
	{ R367_QAM_EQU_TAP_CTRL,            0x00 },
	{ R367_QAM_EQU_CTR_CRL_CONTROL_L,   0x11 },
	{ R367_QAM_EQU_CTR_CRL_CONTROL_H,   0x05 },
	{ R367_QAM_EQU_CTR_HIPOW_L,         0x00 },
	{ R367_QAM_EQU_CTR_HIPOW_H,         0x00 },
	{ R367_QAM_EQU_I_EQU_LO,            0xef },
	{ R367_QAM_EQU_I_EQU_HI,            0x00 },
	{ R367_QAM_EQU_Q_EQU_LO,            0xee },
	{ R367_QAM_EQU_Q_EQU_HI,            0x00 },
	{ R367_QAM_EQU_MAPPER,              0xc5 },
	{ R367_QAM_EQU_SWEEP_RATE,          0x80 },
	{ R367_QAM_EQU_SNR_LO,              0x64 },
	{ R367_QAM_EQU_SNR_HI,              0x03 },
	{ R367_QAM_EQU_GAMMA_LO,            0x00 },
	{ R367_QAM_EQU_GAMMA_HI,            0x00 },
	{ R367_QAM_EQU_ERR_GAIN,            0x36 },
	{ R367_QAM_EQU_RADIUS,              0xaa },
	{ R367_QAM_EQU_FFE_MAINTAP,         0x00 },
	{ R367_QAM_EQU_FFE_LEAKAGE,         0x63 },
	{ R367_QAM_EQU_FFE_MAINTAP_POS,     0xdf },
	{ R367_QAM_EQU_GAIN_WIDE,           0x88 },
	{ R367_QAM_EQU_GAIN_NARROW,         0x41 },
	{ R367_QAM_EQU_CTR_LPF_GAIN,        0xd1 },
	{ R367_QAM_EQU_CRL_LPF_GAIN,        0xa7 },
	{ R367_QAM_EQU_GLOBAL_GAIN,         0x06 },
	{ R367_QAM_EQU_CRL_LD_SEN,          0x85 },
	{ R367_QAM_EQU_CRL_LD_VAL,          0xe2 },
	{ R367_QAM_EQU_CRL_TFR,             0x20 },
	{ R367_QAM_EQU_CRL_BISTH_LO,        0x00 },
	{ R367_QAM_EQU_CRL_BISTH_HI,        0x00 },
	{ R367_QAM_EQU_SWEEP_RANGE_LO,      0x00 },
	{ R367_QAM_EQU_SWEEP_RANGE_HI,      0x00 },
	{ R367_QAM_EQU_CRL_LIMITER,         0x40 },
	{ R367_QAM_EQU_MODULUS_MAP,         0x90 },
	{ R367_QAM_EQU_PNT_GAIN,            0xa7 },
	{ R367_QAM_FEC_AC_CTR_0,            0x16 },
	{ R367_QAM_FEC_AC_CTR_1,            0x0b },
	{ R367_QAM_FEC_AC_CTR_2,            0x88 },
	{ R367_QAM_FEC_AC_CTR_3,            0x02 },
	{ R367_QAM_FEC_STATUS,              0x12 },
	{ R367_QAM_RS_COUNTER_0,            0x7d },
	{ R367_QAM_RS_COUNTER_1,            0xd0 },
	{ R367_QAM_RS_COUNTER_2,            0x19 },
	{ R367_QAM_RS_COUNTER_3,            0x0b },
	{ R367_QAM_RS_COUNTER_4,            0xa3 },
	{ R367_QAM_RS_COUNTER_5,            0x00 },
	{ R367_QAM_BERT_0,                  0x01 },
	{ R367_QAM_BERT_1,                  0x25 },
	{ R367_QAM_BERT_2,                  0x41 },
	{ R367_QAM_BERT_3,                  0x39 },
	{ R367_QAM_OUTFORMAT_0,             0xc2 },
	{ R367_QAM_OUTFORMAT_1,             0x22 },
	{ R367_QAM_SMOOTHER_2,              0x28 },
	{ R367_QAM_TSMF_CTRL_0,             0x01 },
	{ R367_QAM_TSMF_CTRL_1,             0xc6 },
	{ R367_QAM_TSMF_CTRL_3,             0x43 },
	{ R367_QAM_TS_ON_ID_0,              0x00 },
	{ R367_QAM_TS_ON_ID_1,              0x00 },
	{ R367_QAM_TS_ON_ID_2,              0x00 },
	{ R367_QAM_TS_ON_ID_3,              0x00 },
	{ R367_QAM_RE_STATUS_0,             0x00 },
	{ R367_QAM_RE_STATUS_1,             0x00 },
	{ R367_QAM_RE_STATUS_2,             0x00 },
	{ R367_QAM_RE_STATUS_3,             0x00 },
	{ R367_QAM_TS_STATUS_0,             0x00 },
	{ R367_QAM_TS_STATUS_1,             0x00 },
	{ R367_QAM_TS_STATUS_2,             0xa0 },
	{ R367_QAM_TS_STATUS_3,             0x00 },
	{ R367_QAM_T_O_ID_0,                0x00 },
	{ R367_QAM_T_O_ID_1,                0x00 },
	{ R367_QAM_T_O_ID_2,                0x00 },
	{ R367_QAM_T_O_ID_3,                0x00 },
	{ 0x0000, 0x00 } // EOT
};

struct init_table ofdm_init[] = {
	//{R367_OFDM_ID                   ,0x60},
	//{R367_OFDM_I2CRPT 				,0x22},
	//{R367_OFDM_TOPCTRL				,0x02},
	//{R367_OFDM_IOCFG0				,0x40},
	//{R367_OFDM_DAC0R				,0x00},
	//{R367_OFDM_IOCFG1				,0x00},
	//{R367_OFDM_DAC1R				,0x00},
	//{R367_OFDM_IOCFG2				,0x62},
	//{R367_OFDM_SDFR 				,0x00},
	//{R367_OFDM_STATUS				,0xf8},
	//{R367_OFDM_AUX_CLK				,0x0a},
	//{R367_OFDM_FREESYS1			,0x00},
	//{R367_OFDM_FREESYS2			,0x00},
	//{R367_OFDM_FREESYS3			,0x00},
	//{R367_OFDM_GPIO_CFG			,0x55},
	//{R367_OFDM_GPIO_CMD			,0x00},
	{R367_OFDM_AGC2MAX				,0xff},
	{R367_OFDM_AGC2MIN				,0x00},
	{R367_OFDM_AGC1MAX				,0xff},
	{R367_OFDM_AGC1MIN				,0x00},
	{R367_OFDM_AGCR					,0xbc},
	{R367_OFDM_AGC2TH				,0x00},
	//{R367_OFDM_AGC12C				,0x01}, //Note: This defines AGC pins, also needed for QAM
	{R367_OFDM_AGCCTRL1			,0x85},
	{R367_OFDM_AGCCTRL2			,0x1f},
	{R367_OFDM_AGC1VAL1			,0x00},
	{R367_OFDM_AGC1VAL2			,0x00},
	{R367_OFDM_AGC2VAL1			,0x6f},
	{R367_OFDM_AGC2VAL2			,0x05},
	{R367_OFDM_AGC2PGA				,0x00},
	{R367_OFDM_OVF_RATE1			,0x00},
	{R367_OFDM_OVF_RATE2			,0x00},
	{R367_OFDM_GAIN_SRC1			,0x2b},
	{R367_OFDM_GAIN_SRC2			,0x04},
	{R367_OFDM_INC_DEROT1			,0x55},
	{R367_OFDM_INC_DEROT2			,0x55},
	{R367_OFDM_PPM_CPAMP_DIR		,0x2c},
	{R367_OFDM_PPM_CPAMP_INV		,0x00},
	{R367_OFDM_FREESTFE_1			,0x00},
	{R367_OFDM_FREESTFE_2			,0x1c},
	{R367_OFDM_DCOFFSET			,0x00},
	{R367_OFDM_EN_PROCESS			,0x05},
	{R367_OFDM_SDI_SMOOTHER		,0x80},
	{R367_OFDM_FE_LOOP_OPEN		,0x1c},
	{R367_OFDM_FREQOFF1			,0x00},
	{R367_OFDM_FREQOFF2			,0x00},
	{R367_OFDM_FREQOFF3			,0x00},
	{R367_OFDM_TIMOFF1				,0x00},
	{R367_OFDM_TIMOFF2				,0x00},
	{R367_OFDM_EPQ					,0x02},
	{R367_OFDM_EPQAUTO				,0x01},
	{R367_OFDM_SYR_UPDATE			,0xf5},
	{R367_OFDM_CHPFREE						,0x00},
	{R367_OFDM_PPM_STATE_MAC		      ,0x23},
	{R367_OFDM_INR_THRESHOLD		      ,0xff},
	{R367_OFDM_EPQ_TPS_ID_CELL	      ,0xf9},
	{R367_OFDM_EPQ_CFG				      ,0x00},
	{R367_OFDM_EPQ_STATUS			      ,0x01},
	{R367_OFDM_AUTORELOCK			      ,0x81},
	{R367_OFDM_BER_THR_VMSB		      ,0x00},
	{R367_OFDM_BER_THR_MSB		      ,0x00},
	{R367_OFDM_BER_THR_LSB		      ,0x00},
	{R367_OFDM_CCD					      ,0x83},
	{R367_OFDM_SPECTR_CFG			      ,0x00},
	{R367_OFDM_CHC_DUMMY			      ,0x18},
	{R367_OFDM_INC_CTL				      ,0x88},
	{R367_OFDM_INCTHRES_COR1		      ,0xb4},
	{R367_OFDM_INCTHRES_COR2		      ,0x96},
	{R367_OFDM_INCTHRES_DET1		      ,0x0e},
	{R367_OFDM_INCTHRES_DET2		      ,0x11},
	{R367_OFDM_IIR_CELLNB				   ,0x8d},
	{R367_OFDM_IIRCX_COEFF1_MSB	      ,0x00},
	{R367_OFDM_IIRCX_COEFF1_LSB	      ,0x00},
	{R367_OFDM_IIRCX_COEFF2_MSB	      ,0x09},
	{R367_OFDM_IIRCX_COEFF2_LSB	      ,0x18},
	{R367_OFDM_IIRCX_COEFF3_MSB	      ,0x14},
	{R367_OFDM_IIRCX_COEFF3_LSB	      ,0x9c},
	{R367_OFDM_IIRCX_COEFF4_MSB	      ,0x00},
	{R367_OFDM_IIRCX_COEFF4_LSB	      ,0x00},
	{R367_OFDM_IIRCX_COEFF5_MSB	      ,0x36},
	{R367_OFDM_IIRCX_COEFF5_LSB			,0x42},
	{R367_OFDM_FEPATH_CFG			      ,0x00},
	{R367_OFDM_PMC1_FUNC			      ,0x65},
	{R367_OFDM_PMC1_FOR			      ,0x00},
	{R367_OFDM_PMC2_FUNC			      ,0x00},
	{R367_OFDM_STATUS_ERR_DA		      ,0xe0},
	{R367_OFDM_DIG_AGC_R			      ,0xfe},
	{R367_OFDM_COMAGC_TARMSB		      ,0x0b},
	{R367_OFDM_COM_AGC_TAR_ENMODE     ,0x41},
	{R367_OFDM_COM_AGC_CFG			   ,0x3e},
	{R367_OFDM_COM_AGC_GAIN1				,0x39},
	{R367_OFDM_AUT_AGC_TARGETMSB	   ,0x0b},
	{R367_OFDM_LOCK_DET_MSB			   ,0x01},
	{R367_OFDM_AGCTAR_LOCK_LSBS		   ,0x40},
	{R367_OFDM_AUT_GAIN_EN		      ,0xf4},
	{R367_OFDM_AUT_CFG				      ,0xf0},
	{R367_OFDM_LOCKN				      ,0x23},
	{R367_OFDM_INT_X_3				      ,0x00},
	{R367_OFDM_INT_X_2				      ,0x03},
	{R367_OFDM_INT_X_1				      ,0x8d},
	{R367_OFDM_INT_X_0				      ,0xa0},
	{R367_OFDM_MIN_ERRX_MSB		      ,0x00},
	{R367_OFDM_COR_CTL				      ,0x00},
	{R367_OFDM_COR_STAT			      ,0xf6},
	{R367_OFDM_COR_INTEN			      ,0x00},
	{R367_OFDM_COR_INTSTAT		      ,0x3f},
	{R367_OFDM_COR_MODEGUARD		      ,0x03},
	{R367_OFDM_AGC_CTL				      ,0x08},
	{R367_OFDM_AGC_MANUAL1		      ,0x00},
	{R367_OFDM_AGC_MANUAL2		      ,0x00},
	{R367_OFDM_AGC_TARG			      ,0x16},
	{R367_OFDM_AGC_GAIN1			      ,0x53},
	{R367_OFDM_AGC_GAIN2			      ,0x1d},
	{R367_OFDM_RESERVED_1			      ,0x00},
	{R367_OFDM_RESERVED_2			      ,0x00},
	{R367_OFDM_RESERVED_3			      ,0x00},
	{R367_OFDM_CAS_CTL				      ,0x44},
	{R367_OFDM_CAS_FREQ			      ,0xb3},
	{R367_OFDM_CAS_DAGCGAIN		      ,0x12},
	{R367_OFDM_SYR_CTL				      ,0x04},
	{R367_OFDM_SYR_STAT			      ,0x10},
	{R367_OFDM_SYR_NCO1			      ,0x00},
	{R367_OFDM_SYR_NCO2			      ,0x00},
	{R367_OFDM_SYR_OFFSET1		      ,0x00},
	{R367_OFDM_SYR_OFFSET2		      ,0x00},
	{R367_OFDM_FFT_CTL				      ,0x00},
	{R367_OFDM_SCR_CTL				      ,0x70},
	{R367_OFDM_PPM_CTL1			      ,0xf8},
	{R367_OFDM_TRL_CTL				      ,0xac},
	{R367_OFDM_TRL_NOMRATE1		      ,0x1e},
	{R367_OFDM_TRL_NOMRATE2		      ,0x58},
	{R367_OFDM_TRL_TIME1			      ,0x1d},
	{R367_OFDM_TRL_TIME2			      ,0xfc},
	{R367_OFDM_CRL_CTL				      ,0x24},
	{R367_OFDM_CRL_FREQ1			      ,0xad},
	{R367_OFDM_CRL_FREQ2			      ,0x9d},
	{R367_OFDM_CRL_FREQ3			      ,0xff},
	{R367_OFDM_CHC_CTL		       ,0x01},
	{R367_OFDM_CHC_SNR				      ,0xf0},
	{R367_OFDM_BDI_CTL				      ,0x00},
	{R367_OFDM_DMP_CTL				      ,0x00},
	{R367_OFDM_TPS_RCVD1			      ,0x30},
	{R367_OFDM_TPS_RCVD2			      ,0x02},
	{R367_OFDM_TPS_RCVD3			      ,0x01},
	{R367_OFDM_TPS_RCVD4			      ,0x00},
	{R367_OFDM_TPS_ID_CELL1		      ,0x00},
	{R367_OFDM_TPS_ID_CELL2		      ,0x00},
	{R367_OFDM_TPS_RCVD5_SET1	      ,0x02},
	{R367_OFDM_TPS_SET2			      ,0x02},
	{R367_OFDM_TPS_SET3			      ,0x01},
	{R367_OFDM_TPS_CTL				      ,0x00},
	{R367_OFDM_CTL_FFTOSNUM		      ,0x34},
	{R367_OFDM_TESTSELECT			      ,0x09},
	{R367_OFDM_MSC_REV 			      ,0x0a},
	{R367_OFDM_PIR_CTL 			      ,0x00},
	{R367_OFDM_SNR_CARRIER1 		      ,0xa1},
	{R367_OFDM_SNR_CARRIER2		      ,0x9a},
	{R367_OFDM_PPM_CPAMP			      ,0x2c},
	{R367_OFDM_TSM_AP0				      ,0x00},
	{R367_OFDM_TSM_AP1				      ,0x00},
	{R367_OFDM_TSM_AP2 			      ,0x00},
	{R367_OFDM_TSM_AP3				      ,0x00},
	{R367_OFDM_TSM_AP4				      ,0x00},
	{R367_OFDM_TSM_AP5				      ,0x00},
	{R367_OFDM_TSM_AP6				      ,0x00},
	{R367_OFDM_TSM_AP7				      ,0x00},
	//{R367_OFDM_TSTRES				 ,0x00},
	//{R367_OFDM_ANACTRL				 ,0x0D},/*caution PLL stopped, to be restarted at init!!!*/
	//{R367_OFDM_TSTBUS				      ,0x00},
	//{R367_OFDM_TSTRATE				      ,0x00},
	{R367_OFDM_CONSTMODE			      ,0x01},
	{R367_OFDM_CONSTCARR1			      ,0x00},
	{R367_OFDM_CONSTCARR2			      ,0x00},
	{R367_OFDM_ICONSTEL			      ,0x0a},
	{R367_OFDM_QCONSTEL			      ,0x15},
	{R367_OFDM_TSTBISTRES0		      ,0x00},
	{R367_OFDM_TSTBISTRES1		      ,0x00},
	{R367_OFDM_TSTBISTRES2		      ,0x28},
	{R367_OFDM_TSTBISTRES3		      ,0x00},
	//{R367_OFDM_RF_AGC1				      ,0xff},
	//{R367_OFDM_RF_AGC2				      ,0x83},
	//{R367_OFDM_ANADIGCTRL			      ,0x19},
	//{R367_OFDM_PLLMDIV				      ,0x0c},
	//{R367_OFDM_PLLNDIV				      ,0x55},
	//{R367_OFDM_PLLSETUP			      ,0x18},
	//{R367_OFDM_DUAL_AD12			      ,0x00},
	//{R367_OFDM_TSTBIST				      ,0x00},
	//{R367_OFDM_PAD_COMP_CTRL		      ,0x00},
	//{R367_OFDM_PAD_COMP_WR		      ,0x00},
	//{R367_OFDM_PAD_COMP_RD		      ,0xe0},
	{R367_OFDM_SYR_TARGET_FFTADJT_MSB	,0x00},
	{R367_OFDM_SYR_TARGET_FFTADJT_LSB ,0x00},
	{R367_OFDM_SYR_TARGET_CHCADJT_MSB ,0x00},
	{R367_OFDM_SYR_TARGET_CHCADJT_LSB ,0x00},
	{R367_OFDM_SYR_FLAG			 ,0x00},
	{R367_OFDM_CRL_TARGET1		 ,0x00},
	{R367_OFDM_CRL_TARGET2		 ,0x00},
	{R367_OFDM_CRL_TARGET3		 ,0x00},
	{R367_OFDM_CRL_TARGET4		 ,0x00},
	{R367_OFDM_CRL_FLAG			 ,0x00},
	{R367_OFDM_TRL_TARGET1		 ,0x00},
	{R367_OFDM_TRL_TARGET2		 ,0x00},
	{R367_OFDM_TRL_CHC				 ,0x00},
	{R367_OFDM_CHC_SNR_TARG		 ,0x00},
	{R367_OFDM_TOP_TRACK			      ,0x00},
	{R367_OFDM_TRACKER_FREE1		 ,0x00},
	{R367_OFDM_ERROR_CRL1			 ,0x00},
	{R367_OFDM_ERROR_CRL2			 ,0x00},
	{R367_OFDM_ERROR_CRL3			 ,0x00},
	{R367_OFDM_ERROR_CRL4			 ,0x00},
	{R367_OFDM_DEC_NCO1			 ,0x2c},
	{R367_OFDM_DEC_NCO2			 ,0x0f},
	{R367_OFDM_DEC_NCO3			 ,0x20},
	{R367_OFDM_SNR					 ,0xf1},
	{R367_OFDM_SYR_FFTADJ1		      ,0x00},
	{R367_OFDM_SYR_FFTADJ2		 ,0x00},
	{R367_OFDM_SYR_CHCADJ1		 ,0x00},
	{R367_OFDM_SYR_CHCADJ2		 ,0x00},
	{R367_OFDM_SYR_OFF				 ,0x00},
	{R367_OFDM_PPM_OFFSET1		      ,0x00},
	{R367_OFDM_PPM_OFFSET2		 ,0x03},
	{R367_OFDM_TRACKER_FREE2		 ,0x00},
	{R367_OFDM_DEBG_LT10			 ,0x00},
	{R367_OFDM_DEBG_LT11			 ,0x00},
	{R367_OFDM_DEBG_LT12			      ,0x00},
	{R367_OFDM_DEBG_LT13			 ,0x00},
	{R367_OFDM_DEBG_LT14			 ,0x00},
	{R367_OFDM_DEBG_LT15			 ,0x00},
	{R367_OFDM_DEBG_LT16			 ,0x00},
	{R367_OFDM_DEBG_LT17			 ,0x00},
	{R367_OFDM_DEBG_LT18			 ,0x00},
	{R367_OFDM_DEBG_LT19			 ,0x00},
	{R367_OFDM_DEBG_LT1A			 ,0x00},
	{R367_OFDM_DEBG_LT1B			 ,0x00},
	{R367_OFDM_DEBG_LT1C 			 ,0x00},
	{R367_OFDM_DEBG_LT1D 			 ,0x00},
	{R367_OFDM_DEBG_LT1E			 ,0x00},
	{R367_OFDM_DEBG_LT1F			 ,0x00},
	{R367_OFDM_RCCFGH				 ,0x00},
	{R367_OFDM_RCCFGM						,0x00},
	{R367_OFDM_RCCFGL						,0x00},
	{R367_OFDM_RCINSDELH					,0x00},
	{R367_OFDM_RCINSDELM			 ,0x00},
	{R367_OFDM_RCINSDELL			 ,0x00},
	{R367_OFDM_RCSTATUS			 ,0x00},
	{R367_OFDM_RCSPEED 			 ,0x6f},
	{R367_OFDM_RCDEBUGM			 ,0xe7},
	{R367_OFDM_RCDEBUGL			 ,0x9b},
	{R367_OFDM_RCOBSCFG			 ,0x00},
	{R367_OFDM_RCOBSM 				 ,0x00},
	{R367_OFDM_RCOBSL 				 ,0x00},
	{R367_OFDM_RCFECSPY			 ,0x00},
	{R367_OFDM_RCFSPYCFG 			 ,0x00},
	{R367_OFDM_RCFSPYDATA			 ,0x00},
	{R367_OFDM_RCFSPYOUT 			 ,0x00},
	{R367_OFDM_RCFSTATUS 			 ,0x00},
	{R367_OFDM_RCFGOODPACK		 ,0x00},
	{R367_OFDM_RCFPACKCNT 		 ,0x00},
	{R367_OFDM_RCFSPYMISC 		 ,0x00},
	{R367_OFDM_RCFBERCPT4 		 ,0x00},
	{R367_OFDM_RCFBERCPT3 		 ,0x00},
	{R367_OFDM_RCFBERCPT2 		 ,0x00},
	{R367_OFDM_RCFBERCPT1 		 ,0x00},
	{R367_OFDM_RCFBERCPT0 		 ,0x00},
	{R367_OFDM_RCFBERERR2 		 ,0x00},
	{R367_OFDM_RCFBERERR1 		 ,0x00},
	{R367_OFDM_RCFBERERR0 		 ,0x00},
	{R367_OFDM_RCFSTATESM 		 ,0x00},
	{R367_OFDM_RCFSTATESL 		 ,0x00},
	{R367_OFDM_RCFSPYBER  		 ,0x00},
	{R367_OFDM_RCFSPYDISTM		 ,0x00},
	{R367_OFDM_RCFSPYDISTL		 ,0x00},
	{R367_OFDM_RCFSPYOBS7 		 ,0x00},
	{R367_OFDM_RCFSPYOBS6 		      ,0x00},
	{R367_OFDM_RCFSPYOBS5 		 ,0x00},
	{R367_OFDM_RCFSPYOBS4 		 ,0x00},
	{R367_OFDM_RCFSPYOBS3 		 ,0x00},
	{R367_OFDM_RCFSPYOBS2 		 ,0x00},
	{R367_OFDM_RCFSPYOBS1 		 ,0x00},
	{R367_OFDM_RCFSPYOBS0			 ,0x00},
	//{R367_OFDM_TSGENERAL 			 ,0x00},
	//{R367_OFDM_RC1SPEED  			 ,0x6f},
	//{R367_OFDM_TSGSTATUS			 ,0x18},
	{R367_OFDM_FECM					 ,0x01},
	{R367_OFDM_VTH12				 ,0xff},
	{R367_OFDM_VTH23				 ,0xa1},
	{R367_OFDM_VTH34				 ,0x64},
	{R367_OFDM_VTH56				 ,0x40},
	{R367_OFDM_VTH67				 ,0x00},
	{R367_OFDM_VTH78				 ,0x2c},
	{R367_OFDM_VITCURPUN			 ,0x12},
	{R367_OFDM_VERROR				 ,0x01},
	{R367_OFDM_PRVIT				 ,0x3f},
	{R367_OFDM_VAVSRVIT			 ,0x00},
	{R367_OFDM_VSTATUSVIT			 ,0xbd},
	{R367_OFDM_VTHINUSE 			 ,0xa1},
	{R367_OFDM_KDIV12				 ,0x20},
	{R367_OFDM_KDIV23				 ,0x40},
	{R367_OFDM_KDIV34				 ,0x20},
	{R367_OFDM_KDIV56				 ,0x30},
	{R367_OFDM_KDIV67				 ,0x00},
	{R367_OFDM_KDIV78				 ,0x30},
	{R367_OFDM_SIGPOWER 			 ,0x54},
	{R367_OFDM_DEMAPVIT 			 ,0x40},
	{R367_OFDM_VITSCALE 			 ,0x00},
	{R367_OFDM_FFEC1PRG 			 ,0x00},
	{R367_OFDM_FVITCURPUN 		 ,0x12},
	{R367_OFDM_FVERROR 			 ,0x01},
	{R367_OFDM_FVSTATUSVIT		 ,0xbd},
	{R367_OFDM_DEBUG_LT1			 ,0x00},
	{R367_OFDM_DEBUG_LT2			 ,0x00},
	{R367_OFDM_DEBUG_LT3			 ,0x00},
	{R367_OFDM_TSTSFMET  			 ,0x00},
	{R367_OFDM_SELOUT				 ,0x00},
	{R367_OFDM_TSYNC				 ,0x00},
	{R367_OFDM_TSTERR				 ,0x00},
	{R367_OFDM_TSFSYNC   			 ,0x00},
	{R367_OFDM_TSTSFERR  			 ,0x00},
	{R367_OFDM_TSTTSSF1  			 ,0x01},
	{R367_OFDM_TSTTSSF2  			 ,0x1f},
	{R367_OFDM_TSTTSSF3  			 ,0x00},
	{R367_OFDM_TSTTS1   			 ,0x00},
	{R367_OFDM_TSTTS2   			      ,0x1f},
	{R367_OFDM_TSTTS3   			 ,0x01},
	{R367_OFDM_TSTTS4   			 ,0x00},
	{R367_OFDM_TSTTSRC  			 ,0x00},
	{R367_OFDM_TSTTSRS  			 ,0x00},
	{R367_OFDM_TSSTATEM			 ,0xb0},
	{R367_OFDM_TSSTATEL			 ,0x40},
	{R367_OFDM_TSCFGH  			 ,0x80},
	{R367_OFDM_TSCFGM  			 ,0x00},
	{R367_OFDM_TSCFGL  			 ,0x20},
	{R367_OFDM_TSSYNC  			 ,0x00},
	{R367_OFDM_TSINSDELH			 ,0x00},
	{R367_OFDM_TSINSDELM 			 ,0x00},
	{R367_OFDM_TSINSDELL			 ,0x00},
	{R367_OFDM_TSDIVN				 ,0x03},
	{R367_OFDM_TSDIVPM				 ,0x00},
	{R367_OFDM_TSDIVPL				 ,0x00},
	{R367_OFDM_TSDIVQM 			 ,0x00},
	{R367_OFDM_TSDIVQL				 ,0x00},
	{R367_OFDM_TSDILSTKM			 ,0x00},
	{R367_OFDM_TSDILSTKL			 ,0x00},
	{R367_OFDM_TSSPEED				 ,0x6f},
	{R367_OFDM_TSSTATUS			 ,0x81},
	{R367_OFDM_TSSTATUS2			 ,0x6a},
	{R367_OFDM_TSBITRATEM			 ,0x0f},
	{R367_OFDM_TSBITRATEL			 ,0xc6},
	{R367_OFDM_TSPACKLENM			 ,0x00},
	{R367_OFDM_TSPACKLENL			 ,0xfc},
	{R367_OFDM_TSBLOCLENM			 ,0x0a},
	{R367_OFDM_TSBLOCLENL			 ,0x80},
	{R367_OFDM_TSDLYH 				 ,0x90},
	{R367_OFDM_TSDLYM				 ,0x68},
	{R367_OFDM_TSDLYL				 ,0x01},
	{R367_OFDM_TSNPDAV				 ,0x00},
	{R367_OFDM_TSBUFSTATH 		 ,0x00},
	{R367_OFDM_TSBUFSTATM 		 ,0x00},
	{R367_OFDM_TSBUFSTATL			 ,0x00},
	{R367_OFDM_TSDEBUGM			 ,0xcf},
	{R367_OFDM_TSDEBUGL			 ,0x1e},
	{R367_OFDM_TSDLYSETH 			 ,0x00},
	{R367_OFDM_TSDLYSETM			 ,0x68},
	{R367_OFDM_TSDLYSETL			 ,0x00},
	{R367_OFDM_TSOBSCFG			 ,0x00},
	{R367_OFDM_TSOBSM 				 ,0x47},
	{R367_OFDM_TSOBSL				 ,0x1f},
	{R367_OFDM_ERRCTRL1			 ,0x95},
	{R367_OFDM_ERRCNT1H 			 ,0x80},
	{R367_OFDM_ERRCNT1M 			 ,0x00},
	{R367_OFDM_ERRCNT1L 			 ,0x00},
	{R367_OFDM_ERRCTRL2			 ,0x95},
	{R367_OFDM_ERRCNT2H			 ,0x00},
	{R367_OFDM_ERRCNT2M			 ,0x00},
	{R367_OFDM_ERRCNT2L			 ,0x00},
	{R367_OFDM_FECSPY 				 ,0x88},
	{R367_OFDM_FSPYCFG				 ,0x2c},
	{R367_OFDM_FSPYDATA			 ,0x3a},
	{R367_OFDM_FSPYOUT				 ,0x06},
	{R367_OFDM_FSTATUS				 ,0x61},
	{R367_OFDM_FGOODPACK			 ,0xff},
	{R367_OFDM_FPACKCNT			 ,0xff},
	{R367_OFDM_FSPYMISC 			 ,0x66},
	{R367_OFDM_FBERCPT4 			 ,0x00},
	{R367_OFDM_FBERCPT3			 ,0x00},
	{R367_OFDM_FBERCPT2			 ,0x36},
	{R367_OFDM_FBERCPT1			 ,0x36},
	{R367_OFDM_FBERCPT0 			 ,0x14},
	{R367_OFDM_FBERERR2			 ,0x00},
	{R367_OFDM_FBERERR1			 ,0x03},
	{R367_OFDM_FBERERR0			 ,0x28},
	{R367_OFDM_FSTATESM			 ,0x00},
	{R367_OFDM_FSTATESL			 ,0x02},
	{R367_OFDM_FSPYBER 			 ,0x00},
	{R367_OFDM_FSPYDISTM			 ,0x01},
	{R367_OFDM_FSPYDISTL			 ,0x9f},
	{R367_OFDM_FSPYOBS7 			 ,0xc9},
	{R367_OFDM_FSPYOBS6 			 ,0x99},
	{R367_OFDM_FSPYOBS5			 ,0x08},
	{R367_OFDM_FSPYOBS4			 ,0xec},
	{R367_OFDM_FSPYOBS3			 ,0x01},
	{R367_OFDM_FSPYOBS2			 ,0x0f},
	{R367_OFDM_FSPYOBS1			 ,0xf5},
	{R367_OFDM_FSPYOBS0			 ,0x08},
	{R367_OFDM_SFDEMAP 			 ,0x40},
	{R367_OFDM_SFERROR 			 ,0x00},
	{R367_OFDM_SFAVSR  			 ,0x30},
	{R367_OFDM_SFECSTATUS			 ,0xcc},
	{R367_OFDM_SFKDIV12			 ,0x20},
	{R367_OFDM_SFKDIV23			 ,0x40},
	{R367_OFDM_SFKDIV34			 ,0x20},
	{R367_OFDM_SFKDIV56			 ,0x20},
	{R367_OFDM_SFKDIV67			 ,0x00},
	{R367_OFDM_SFKDIV78			 ,0x20},
	{R367_OFDM_SFDILSTKM			 ,0x00},
	{R367_OFDM_SFDILSTKL 			 ,0x00},
	{R367_OFDM_SFSTATUS			 ,0xb5},
	{R367_OFDM_SFDLYH				 ,0x90},
	{R367_OFDM_SFDLYM				 ,0x60},
	{R367_OFDM_SFDLYL				 ,0x01},
	{R367_OFDM_SFDLYSETH			 ,0xc0},
	{R367_OFDM_SFDLYSETM			 ,0x60},
	{R367_OFDM_SFDLYSETL			 ,0x00},
	{R367_OFDM_SFOBSCFG 			 ,0x00},
	{R367_OFDM_SFOBSM 				 ,0x47},
	{R367_OFDM_SFOBSL				 ,0x05},
	{R367_OFDM_SFECINFO 			 ,0x40},
	{R367_OFDM_SFERRCTRL 			 ,0x74},
	{R367_OFDM_SFERRCNTH			 ,0x80},
	{R367_OFDM_SFERRCNTM 			 ,0x00},
	{R367_OFDM_SFERRCNTL			 ,0x00},
	{R367_OFDM_SYMBRATEM			 ,0x2f},
	{R367_OFDM_SYMBRATEL			      ,0x50},
	{R367_OFDM_SYMBSTATUS			 ,0x7f},
	{R367_OFDM_SYMBCFG 			 ,0x00},
	{R367_OFDM_SYMBFIFOM 			 ,0xf4},
	{R367_OFDM_SYMBFIFOL 			 ,0x0d},
	{R367_OFDM_SYMBOFFSM 			 ,0xf0},
	{R367_OFDM_SYMBOFFSL 			 ,0x2d},
	//{R367_OFDM_DEBUG_LT4			 ,0x00},
	//{R367_OFDM_DEBUG_LT5			 ,0x00},
	//{R367_OFDM_DEBUG_LT6			 ,0x00},
	//{R367_OFDM_DEBUG_LT7			 ,0x00},
	//{R367_OFDM_DEBUG_LT8			 ,0x00},
	//{R367_OFDM_DEBUG_LT9			 ,0x00},
	{ 0x0000, 0x00 } // EOT
};

static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = (u64)a * (u64)b;
	do_div(tmp64, c);

	return (u32) tmp64;
}

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	struct i2c_msg msg =
		{.addr = adr, .flags = 0, .buf = data, .len = len};

	if (i2c_transfer(adap, &msg, 1) != 1) {
		printk("stv0367: i2c_write error\n");
		return -1;
	}
	return 0;
}

#if 0
static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	struct i2c_msg msgs[2] = { { .addr = adr, .flags = 0,
				     .buf = msg, .len = len},
				   { .addr = adr, .flags = I2C_M_RD,
				     .buf = answ, .len = alen } };
	if (i2c_transfer(adap, msgs, 2) != 2) {
		printk("stv0367: i2c_read error\n");
		return -1;
	}
	return 0;
}
#endif

static int writereg(struct stv_state *state, u16 reg, u8 dat)
{
	u8 mm[3] = { (reg >> 8), reg & 0xff, dat };

	return i2c_write(state->i2c, state->adr, mm, 3);
}

static int readreg(struct stv_state *state, u16 reg, u8 *val)
{
	u8 msg[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msgs[2] = {{.addr = state->adr, .flags = 0,
				   .buf  = msg, .len   = 2},
				  {.addr = state->adr, .flags = I2C_M_RD,
				   .buf  = val, .len   = 1}};
	return (i2c_transfer(state->i2c, msgs, 2) == 2) ? 0 : -1;
}

static int readregs(struct stv_state *state, u16 reg, u8 *val, int count)
{
	u8 msg[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msgs[2] = {{.addr = state->adr, .flags = 0,
				   .buf  = msg, .len   = 2},
				  {.addr = state->adr, .flags = I2C_M_RD,
				   .buf  = val, .len   = count}};
	return (i2c_transfer(state->i2c, msgs, 2) == 2) ? 0 : -1;
}

static int write_init_table(struct stv_state *state, struct init_table *tab)
{
	while (1) {
		if (!tab->adr)
			break;
		if (writereg(state, tab->adr, tab->data) < 0)
			return -1;
		tab++;
	}
	return 0;
}

static int qam_set_modulation(struct stv_state *state)
{
	int stat = 0;

	switch(state->modulation) {
	case QAM_16:
		writereg(state, R367_QAM_EQU_MAPPER,state->qam_inversion | QAM_MOD_QAM16 );
		writereg(state, R367_QAM_AGC_PWR_REF_L,0x64);       /* Set analog AGC reference */
		writereg(state, R367_QAM_IQDEM_ADJ_AGC_REF,0x00);   /* Set digital AGC reference */
		writereg(state, R367_QAM_FSM_STATE,0x90);
		writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
		writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa7);
		writereg(state, R367_QAM_EQU_CRL_LD_SEN,0x95);
		writereg(state, R367_QAM_EQU_CRL_LIMITER,0x40);
		writereg(state, R367_QAM_EQU_PNT_GAIN,0x8a);
		break;
	case QAM_32:
		writereg(state, R367_QAM_EQU_MAPPER,state->qam_inversion | QAM_MOD_QAM32 );
		writereg(state, R367_QAM_AGC_PWR_REF_L,0x6e);       /* Set analog AGC reference */
		writereg(state, R367_QAM_IQDEM_ADJ_AGC_REF,0x00);   /* Set digital AGC reference */
		writereg(state, R367_QAM_FSM_STATE,0xb0);
		writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
		writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xb7);
		writereg(state, R367_QAM_EQU_CRL_LD_SEN,0x9d);
		writereg(state, R367_QAM_EQU_CRL_LIMITER,0x7f);
		writereg(state, R367_QAM_EQU_PNT_GAIN,0xa7);
		break;
	case QAM_64:
		writereg(state, R367_QAM_EQU_MAPPER,state->qam_inversion | QAM_MOD_QAM64 );
		writereg(state, R367_QAM_AGC_PWR_REF_L,0x5a);       /* Set analog AGC reference */
		writereg(state, R367_QAM_IQDEM_ADJ_AGC_REF,0x82);   /* Set digital AGC reference */
		if(state->symbol_rate>4500000)
		{
			writereg(state, R367_QAM_FSM_STATE,0xb0);
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa5);
		}
		else if(state->symbol_rate>2500000) // 25000000
		{
			writereg(state, R367_QAM_FSM_STATE,0xa0);
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa6);
		}
		else
		{
			writereg(state, R367_QAM_FSM_STATE,0xa0);
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xd1);
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa7);
		}
		writereg(state, R367_QAM_EQU_CRL_LD_SEN,0x95);
		writereg(state, R367_QAM_EQU_CRL_LIMITER,0x40);
		writereg(state, R367_QAM_EQU_PNT_GAIN,0x99);
		break;
	case QAM_128:
		writereg(state, R367_QAM_EQU_MAPPER,state->qam_inversion | QAM_MOD_QAM128 );
		writereg(state, R367_QAM_AGC_PWR_REF_L,0x76);       /* Set analog AGC reference */
		writereg(state, R367_QAM_IQDEM_ADJ_AGC_REF,0x00);	/* Set digital AGC reference */
		writereg(state, R367_QAM_FSM_STATE,0x90);
		writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xb1);
		if(state->symbol_rate>4500000) // 45000000
		{
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa7);
		}
		else if(state->symbol_rate>2500000) // 25000000
		{
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa6);
		}
		else
		{
			writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0x97);
		}
		writereg(state, R367_QAM_EQU_CRL_LD_SEN,0x8e);
		writereg(state, R367_QAM_EQU_CRL_LIMITER,0x7f);
		writereg(state, R367_QAM_EQU_PNT_GAIN,0xa7);
		break;
	case QAM_256:
		writereg(state, R367_QAM_EQU_MAPPER,state->qam_inversion | QAM_MOD_QAM256 );
		writereg(state, R367_QAM_AGC_PWR_REF_L,0x5a);     /* Set analog AGC reference */
		writereg(state, R367_QAM_IQDEM_ADJ_AGC_REF,0x94); /* Set digital AGC reference */
		writereg(state, R367_QAM_FSM_STATE,0xa0);
		if(state->symbol_rate>4500000) // 45000000
		{
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
		}
		else if(state->symbol_rate>2500000) // 25000000
		{
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xc1);
		}
		else
		{
			writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,0xd1);
		}
		writereg(state, R367_QAM_EQU_CRL_LPF_GAIN,0xa7);
		writereg(state, R367_QAM_EQU_CRL_LD_SEN,0x85);
		writereg(state, R367_QAM_EQU_CRL_LIMITER,0x40);
		writereg(state, R367_QAM_EQU_PNT_GAIN,0xa7);
		break;
	default:
		stat = -EINVAL;
		break;
	}
	return stat;
}


static int QAM_SetSymbolRate(struct stv_state *state)
{
	int status = 0;
	u32 sr = state->symbol_rate;
	u32 Corr = 0;
	u32 Temp, Temp1, AdpClk;

	switch(state->modulation) {
	default:
	case QAM_16:   Corr = 1032; break;
	case QAM_32:   Corr =  954; break;
	case QAM_64:   Corr =  983; break;
	case QAM_128:  Corr =  957; break;
	case QAM_256:  Corr =  948; break;
	}

	// Transfer ration
	Temp = (256*sr) / state->adc_clock;
	writereg(state, R367_QAM_EQU_CRL_TFR,(Temp));

	/* Symbol rate and SRC gain calculation */
	AdpClk = (state->master_clock) / 2000; /* TRL works at half the system clock */

	Temp = state->symbol_rate;
	Temp1 = sr;

	if(sr < 2097152)				/* 2097152 = 2^21 */
	{
		Temp  = ((((sr * 2048) / AdpClk) * 16384 ) / 125 ) * 8;
		Temp1 = (((((sr * 2048) / 439 ) * 256 ) / AdpClk ) * Corr * 9 ) / 10000000;
	}
	else if(sr < 4194304)			/* 4194304 = 2**22 */
	{
		Temp  = ((((sr * 1024) / AdpClk) * 16384 ) / 125 ) * 16;
		Temp1 = (((((sr * 1024) / 439 ) * 256 ) / AdpClk ) * Corr * 9 ) / 5000000;
	}
	else if(sr < 8388608)			/* 8388608 = 2**23 */
	{
		Temp  = ((((sr * 512) / AdpClk) * 16384 ) / 125 ) * 32;
		Temp1 = (((((sr * 512) / 439 ) * 256 ) / AdpClk ) * Corr * 9 ) / 2500000;
	}
	else
	{
		Temp  = ((((sr * 256) / AdpClk) * 16384 ) / 125 ) * 64;
		Temp1 = (((((sr * 256) / 439 ) * 256 ) / AdpClk ) * Corr * 9 ) / 1250000;
	}

	///* Filters' coefficients are calculated and written into registers only if the filters are enabled */
	//if (ChipGetField(hChip,F367qam_ADJ_EN)) // Is disabled from init!
	//{
	//    FE_367qam_SetIirAdjacentcoefficient(hChip, MasterClk_Hz, SymbolRate);
	//}
	///* AllPass filter is never used on this IC */
	//ChipSetField(hChip,F367qam_ALLPASSFILT_EN,0); // should be disabled from init!

	writereg(state, R367_QAM_SRC_NCO_LL,(Temp));
	writereg(state, R367_QAM_SRC_NCO_LH,(Temp>>8));
	writereg(state, R367_QAM_SRC_NCO_HL,(Temp>>16));
	writereg(state, R367_QAM_SRC_NCO_HH,(Temp>>24));

	writereg(state, R367_QAM_IQDEM_GAIN_SRC_L,(Temp1));
	writereg(state, R367_QAM_IQDEM_GAIN_SRC_H,(Temp1>>8));
	return status;
}


static int QAM_SetDerotFrequency(struct stv_state *state, u32 DerotFrequency)
{
	int status = 0;
	u32 Sampled_IF;

	do {
		//if (DerotFrequency < 1000000)
		//    DerotFrequency = state->adc_clock/4; /* ZIF operation */
		if (DerotFrequency > state->adc_clock)
			DerotFrequency = DerotFrequency - state->adc_clock;	// User Alias

		Sampled_IF = ((32768 * (DerotFrequency/1000)) / (state->adc_clock/1000)) * 256;
		if(Sampled_IF > 8388607)
			Sampled_IF = 8388607;

		writereg(state, R367_QAM_MIX_NCO_LL, (Sampled_IF));
		writereg(state, R367_QAM_MIX_NCO_HL, (Sampled_IF>>8));
		writereg(state, R367_QAM_MIX_NCO_HH, (Sampled_IF>>16));
	} while(0);

	return status;
}



static int QAM_Start(struct stv_state *state, s32 offsetFreq,s32 IntermediateFrequency)
{
	int status = 0;
	u32 AGCTimeOut = 25;
	u32 TRLTimeOut = 100000000 / state->symbol_rate;
	u32 CRLSymbols = 0;
	u32 EQLTimeOut = 100;
	u32 SearchRange = state->symbol_rate / 25;
	u32 CRLTimeOut;
	u8 Temp;

	if( state->demod_state != QAMSet ) {
		writereg(state, R367_DEBUG_LT4,0x00);
		writereg(state, R367_DEBUG_LT5,0x01);
		writereg(state, R367_DEBUG_LT6,0x06);// R367_QAM_CTRL_1
		writereg(state, R367_DEBUG_LT7,0x03);// R367_QAM_CTRL_2
		writereg(state, R367_DEBUG_LT8,0x00);
		writereg(state, R367_DEBUG_LT9,0x00);

		// Tuner Setup
		writereg(state, R367_ANADIGCTRL,0x8B); /* Buffer Q disabled, I Enabled, signed ADC */
		writereg(state, R367_DUAL_AD12,0x04); /* ADCQ disabled */

		// Clock setup
		writereg(state, R367_ANACTRL,0x0D); /* PLL bypassed and disabled */
		writereg(state, R367_TOPCTRL,0x10); // Set QAM

		writereg(state, R367_PLLMDIV,27); /* IC runs at 58 MHz with a 27 MHz crystal */
		writereg(state, R367_PLLNDIV,232);
		writereg(state, R367_PLLSETUP,0x18);  /* ADC clock is equal to system clock */

		msleep(50);
		writereg(state, R367_ANACTRL,0x00); /* PLL enabled and used */

		state->master_clock = 58000000;
		state->adc_clock = 58000000;

		state->demod_state = QAMSet;
	}

	state->m_bFirstTimeLock = true;
	state->m_DemodLockTime  = -1;

	qam_set_modulation(state);
	QAM_SetSymbolRate(state);

	// Will make problems on low symbol rates ( < 2500000 )

	switch(state->modulation) {
	default:
	case QAM_16:   CRLSymbols = 150000; break;
	case QAM_32:   CRLSymbols = 250000; break;
	case QAM_64:   CRLSymbols = 200000; break;
	case QAM_128:  CRLSymbols = 250000; break;
	case QAM_256:  CRLSymbols = 250000; break;
	}

	CRLTimeOut = (25 * CRLSymbols * (SearchRange/1000)) / (state->symbol_rate/1000);
	CRLTimeOut = (1000 * CRLTimeOut) / state->symbol_rate;
	if( CRLTimeOut < 50 ) CRLTimeOut = 50;

	state->m_FECTimeOut = 20;
	state->m_DemodTimeOut = AGCTimeOut + TRLTimeOut + CRLTimeOut + EQLTimeOut;
	state->m_SignalTimeOut = AGCTimeOut + TRLTimeOut;

	// QAM_AGC_ACCUMRSTSEL = 0;
	readreg(state, R367_QAM_AGC_CTL,&state->m_Save_QAM_AGC_CTL);
	writereg(state, R367_QAM_AGC_CTL,state->m_Save_QAM_AGC_CTL & ~0x0F);

	// QAM_MODULUSMAP_EN = 0
	readreg(state, R367_QAM_EQU_PNT_GAIN,&Temp);
	writereg(state, R367_QAM_EQU_PNT_GAIN,Temp & ~0x40);

	// QAM_SWEEP_EN = 0
	readreg(state, R367_QAM_EQU_CTR_LPF_GAIN,&Temp);
	writereg(state, R367_QAM_EQU_CTR_LPF_GAIN,Temp & ~0x08);

	QAM_SetDerotFrequency(state, IntermediateFrequency);

	// Release TRL
	writereg(state, R367_QAM_CTRL_1,0x00);

	state->IF = IntermediateFrequency;
	state->demod_state = QAMStarted;

	return status;
}

static int OFDM_Start(struct stv_state *state, s32 offsetFreq,s32 IntermediateFrequency)
{
	int status = 0;
	u8 GAIN_SRC1;
	u32 Derot;
	u8 SYR_CTL;
	u8 tmp1;
	u8 tmp2;

	if ( state->demod_state != OFDMSet ) {
		// QAM Disable
		writereg(state, R367_DEBUG_LT4, 0x00);
		writereg(state, R367_DEBUG_LT5, 0x00);
		writereg(state, R367_DEBUG_LT6, 0x00);// R367_QAM_CTRL_1
		writereg(state, R367_DEBUG_LT7, 0x00);// R367_QAM_CTRL_2
		writereg(state, R367_DEBUG_LT8, 0x00);
		writereg(state, R367_DEBUG_LT9, 0x00);

		// Tuner Setup
		writereg(state, R367_ANADIGCTRL, 0x89); /* Buffer Q disabled, I Enabled, unsigned ADC */
		writereg(state, R367_DUAL_AD12, 0x04); /* ADCQ disabled */

		// Clock setup
		writereg(state, R367_ANACTRL, 0x0D); /* PLL bypassed and disabled */
		writereg(state, R367_TOPCTRL, 0x00); // Set OFDM

		writereg(state, R367_PLLMDIV, 1); /* IC runs at 54 MHz with a 27 MHz crystal */
		writereg(state, R367_PLLNDIV, 8);
		writereg(state, R367_PLLSETUP, 0x18);  /* ADC clock is equal to system clock */

		msleep(50);
		writereg(state, R367_ANACTRL, 0x00); /* PLL enabled and used */

		state->master_clock = 54000000;
		state->adc_clock    = 54000000;

		state->demod_state = OFDMSet;
	}

	state->m_bFirstTimeLock = true;
	state->m_DemodLockTime  = -1;

	// Set inversion in GAIN_SRC1 (fixed from init)
	//  is in GAIN_SRC1, see below

	GAIN_SRC1 = 0xA0;
	// Bandwidth

	// Fixed values for 54 MHz
	switch(state->bandwidth) {
	case 0:
	case 8000000:
		// Normrate = 44384;
		writereg(state, R367_OFDM_TRL_CTL,0x14);
		writereg(state, R367_OFDM_TRL_NOMRATE1,0xB0);
		writereg(state, R367_OFDM_TRL_NOMRATE2,0x56);
		// Gain SRC = 2774
		writereg(state, R367_OFDM_GAIN_SRC1,0x0A | GAIN_SRC1);
		writereg(state, R367_OFDM_GAIN_SRC2,0xD6);
		break;
	case 7000000:
		// Normrate = 38836;
		writereg(state, R367_OFDM_TRL_CTL,0x14);
		writereg(state, R367_OFDM_TRL_NOMRATE1,0xDA);
		writereg(state, R367_OFDM_TRL_NOMRATE2,0x4B);
		// Gain SRC = 2427
		writereg(state, R367_OFDM_GAIN_SRC1,0x09 | GAIN_SRC1);
		writereg(state, R367_OFDM_GAIN_SRC2,0x7B);
		break;
	case 6000000:
		// Normrate = 33288;
		writereg(state, R367_OFDM_TRL_CTL,0x14);
		writereg(state, R367_OFDM_TRL_NOMRATE1,0x04);
		writereg(state, R367_OFDM_TRL_NOMRATE2,0x41);
		// Gain SRC = 2080
		writereg(state, R367_OFDM_GAIN_SRC1,0x08 | GAIN_SRC1);
		writereg(state, R367_OFDM_GAIN_SRC2,0x20);
		break;
	default:
		return -EINVAL;
		break;
	}

	Derot = ((IntermediateFrequency / 1000) * 65536) / (state->master_clock / 1000);

	writereg(state, R367_OFDM_INC_DEROT1,(Derot>>8));
	writereg(state, R367_OFDM_INC_DEROT2,(Derot));

	readreg(state, R367_OFDM_SYR_CTL,&SYR_CTL);
	SYR_CTL  &= ~0x78;
	writereg(state, R367_OFDM_SYR_CTL,SYR_CTL);    // EchoPos = 0


	writereg(state, R367_OFDM_COR_MODEGUARD,0x03); // Force = 0, Mode = 0, Guard = 3
	SYR_CTL &= 0x01;
	writereg(state, R367_OFDM_SYR_CTL,SYR_CTL);    // SYR_TR_DIS = 0

	msleep(5);

	writereg(state, R367_OFDM_COR_CTL,0x20);    // Start core

	// -- Begin M.V.
	// Reset FEC and Read Solomon
	readreg(state, R367_OFDM_SFDLYSETH,&tmp1);
	readreg(state, R367_TSGENERAL,&tmp2);
	writereg(state, R367_OFDM_SFDLYSETH,tmp1 | 0x08);
	writereg(state, R367_TSGENERAL,tmp2 | 0x01);
	// -- End M.V.

	state->m_SignalTimeOut = 200;
	state->IF = IntermediateFrequency;
	state->demod_state = OFDMStarted;
	state->m_DemodTimeOut = 0;
	state->m_FECTimeOut = 0;
	state->m_TSTimeOut = 0;

	return status;
}

#if 0
static int Stop(struct stv_state *state)
{
	int status = 0;

	switch(state->demod_state)
	{
	case QAMStarted:
		status = writereg(state, R367_QAM_CTRL_1,0x06);
		state->demod_state = QAMSet;
		break;
	case OFDMStarted:
		status = writereg(state, R367_OFDM_COR_CTL,0x00);
		state->demod_state = OFDMSet;
		break;
	default:
		break;
	}
	return status;
}
#endif

static s32 Log10x100(u32 x)
{
	static u32 LookupTable[100] = {
		101157945, 103514217, 105925373, 108392691, 110917482,
		113501082, 116144861, 118850223, 121618600, 124451461, // 800.5 - 809.5
		127350308, 130316678, 133352143, 136458314, 139636836,
		142889396, 146217717, 149623566, 153108746, 156675107, // 810.5 - 819.5
		160324539, 164058977, 167880402, 171790839, 175792361,
		179887092, 184077200, 188364909, 192752491, 197242274, // 820.5 - 829.5
		201836636, 206538016, 211348904, 216271852, 221309471,
		226464431, 231739465, 237137371, 242661010, 248313311, // 830.5 - 839.5
		254097271, 260015956, 266072506, 272270131, 278612117,
		285101827, 291742701, 298538262, 305492111, 312607937, // 840.5 - 849.5
		319889511, 327340695, 334965439, 342767787, 350751874,
		358921935, 367282300, 375837404, 384591782, 393550075, // 850.5 - 859.5
		402717034, 412097519, 421696503, 431519077, 441570447,
		451855944, 462381021, 473151259, 484172368, 495450191, // 860.5 - 869.5
		506990708, 518800039, 530884444, 543250331, 555904257,
		568852931, 582103218, 595662144, 609536897, 623734835, // 870.5 - 879.5
		638263486, 653130553, 668343918, 683911647, 699841996,
		716143410, 732824533, 749894209, 767361489, 785235635, // 880.5 - 889.5
		803526122, 822242650, 841395142, 860993752, 881048873,
		901571138, 922571427, 944060876, 966050879, 988553095, // 890.5 - 899.5
	};
	s32 y;
	int i;

	if (x == 0)
		return 0;
	y = 800;
	if (x >= 1000000000) {
		x /= 10;
		y += 100;
	}

	while (x < 100000000) {
		x *= 10;
		y -= 100;
	}
	i = 0;
	while (i < 100 && x > LookupTable[i])
		i += 1;
	y += i;
	return y;
}

static int QAM_GetSignalToNoise(struct stv_state *state, s32 *pSignalToNoise)
{
	u32 RegValAvg = 0;
	u8 RegVal[2];
	int status = 0, i;

	*pSignalToNoise = 0;
	for (i = 0; i < 10; i += 1 ) {
		readregs(state, R367_QAM_EQU_SNR_LO, RegVal, 2);
		RegValAvg += RegVal[0] + 256 * RegVal[1];
	}
	if (RegValAvg != 0) {
		s32 Power = 1;
		switch(state->modulation) {
		case QAM_16:
			Power = 20480;
			break;
		case QAM_32:
			Power = 23040;
			break;
		case QAM_64:
			Power = 21504;
			break;
		case QAM_128:
			Power = 23616; 
			break;
		case QAM_256:
			Power = 21760; 
			break;
		default:
			break;
		}
		*pSignalToNoise = Log10x100((Power * 320) / RegValAvg);
	} else {
		*pSignalToNoise = 380;
	}
	return status;
}

static int OFDM_GetSignalToNoise(struct stv_state *state, s32 *pSignalToNoise)
{
	u8 CHC_SNR = 0;

	int status = readreg(state, R367_OFDM_CHC_SNR, &CHC_SNR);
	if (status >= 0) {
		// Note: very unclear documentation on this.
		//   Datasheet states snr = CHC_SNR/4 dB  -> way to high values!
		//   Software  snr = ( 1000 * CHC_SNR ) / 8 / 32 / 10; -> to low values
		//   Comment in SW states this should be ( 1000 * CHC_SNR ) / 4 / 32 / 10; for the 367
		//   361/362 Datasheet: snr = CHC_SNR/8 dB -> this looks best
		*pSignalToNoise = ( (s32)CHC_SNR * 10) / 8;
	}
	//printk("SNR %d\n", *pSignalToNoise);
	return status;
}

#if 0
static int DVBC_GetQuality(struct stv_state *state, s32 SignalToNoise, s32 *pQuality)
{
	*pQuality = 100;
	return 0;
};

static int DVBT_GetQuality(struct stv_state *state, s32 SignalToNoise, s32 *pQuality)
{
	static s32 QE_SN[] = {
		51, // QPSK 1/2
		69, // QPSK 2/3
		79, // QPSK 3/4
		89, // QPSK 5/6
		97, // QPSK 7/8
		108, // 16-QAM 1/2
		131, // 16-QAM 2/3
		146, // 16-QAM 3/4
		156, // 16-QAM 5/6
		160, // 16-QAM 7/8
		165, // 64-QAM 1/2
		187, // 64-QAM 2/3
		202, // 64-QAM 3/4
		216, // 64-QAM 5/6
		225, // 64-QAM 7/8
	};
	u8 TPS_Received[2];
	int Constellation;
	int CodeRate;
	s32 SignalToNoiseRel, BERQuality;

	*pQuality = 0;
	readregs(state, R367_OFDM_TPS_RCVD2, TPS_Received, sizeof(TPS_Received));
	Constellation = TPS_Received[0] & 0x03;
	CodeRate = TPS_Received[1] & 0x07;

	if( Constellation > 2 || CodeRate > 5 )
		return -1;
	SignalToNoiseRel = SignalToNoise - QE_SN[Constellation * 5 + CodeRate];
	BERQuality = 100;

	if( SignalToNoiseRel < -70 )
		*pQuality = 0;
	else if( SignalToNoiseRel < 30 ) {
		*pQuality = ((SignalToNoiseRel + 70) * BERQuality)/100;
	} else
		*pQuality = BERQuality;
	return 0;
};

static s32 DVBCQuality(struct stv_state *state, s32 SignalToNoise)
{
	s32 SignalToNoiseRel = 0;
	s32 Quality = 0;
	s32 BERQuality = 100;

	switch(state->modulation) {
	case QAM_16:  SignalToNoiseRel = SignalToNoise - 200 ; break;
	case QAM_32:  SignalToNoiseRel = SignalToNoise - 230 ; break; // Not in NorDig
	case QAM_64:  SignalToNoiseRel = SignalToNoise - 260 ; break;
	case QAM_128: SignalToNoiseRel = SignalToNoise - 290 ; break;
	case QAM_256: SignalToNoiseRel = SignalToNoise - 320 ; break;
	}

	if( SignalToNoiseRel < -70 ) Quality = 0;
	else if( SignalToNoiseRel < 30 )
	{
		Quality = ((SignalToNoiseRel + 70) * BERQuality)/100;
	}
	else
	Quality = BERQuality;

	return Quality;
}

static int GetQuality(struct stv_state *state, s32 SignalToNoise, s32 *pQuality)
{
	*pQuality = 0;
	switch(state->demod_state)
	{
	case QAMStarted:
		*pQuality = DVBCQuality(state, SignalToNoise);
		break;
	case OFDMStarted:
		return DVBT_GetQuality(state, SignalToNoise, pQuality);
	}
	return 0;
};
#endif

static int attach_init(struct stv_state *state)
{
	int stat = 0;

	stat = readreg(state, R367_ID, &state->ID);
	if ( stat < 0 || state->ID != 0x60 )
		return -ENODEV;
	printk("stv0367 found\n");

	writereg(state, R367_TOPCTRL, 0x10);
	write_init_table(state, base_init);
	write_init_table(state, qam_init);

	writereg(state, R367_TOPCTRL, 0x00);
	write_init_table(state, ofdm_init);

	writereg(state, R367_OFDM_GAIN_SRC1, 0x2A);
	writereg(state, R367_OFDM_GAIN_SRC2, 0xD6);
	writereg(state, R367_OFDM_INC_DEROT1, 0x55);
	writereg(state, R367_OFDM_INC_DEROT2, 0x55);
	writereg(state, R367_OFDM_TRL_CTL, 0x14);
	writereg(state, R367_OFDM_TRL_NOMRATE1, 0xAE);
	writereg(state, R367_OFDM_TRL_NOMRATE2, 0x56);
	writereg(state, R367_OFDM_FEPATH_CFG, 0x0);

	// OFDM TS Setup

	writereg(state, R367_OFDM_TSCFGH, 0x70);
	writereg(state, R367_OFDM_TSCFGM, 0xC0);
	writereg(state, R367_OFDM_TSCFGL, 0x20);
	writereg(state, R367_OFDM_TSSPEED, 0x40);        // Fixed at 54 MHz
	//writereg(state, R367_TSTBUS, 0x80);      // Invert CLK

	writereg(state, R367_OFDM_TSCFGH, 0x71);
	
	if (state->cont_clock)
		writereg(state, R367_OFDM_TSCFGH, 0xf0);
	else
		writereg(state, R367_OFDM_TSCFGH, 0x70);

	writereg(state, R367_TOPCTRL, 0x10);

	// Also needed for QAM
	writereg(state, R367_OFDM_AGC12C, 0x01); // AGC Pin setup

	writereg(state, R367_OFDM_AGCCTRL1, 0x8A); //

	// QAM TS setup, note exact format also depends on descrambler settings
	writereg(state, R367_QAM_OUTFORMAT_0, 0x85); // Inverted Clock, Swap, serial
	// writereg(state, R367_QAM_OUTFORMAT_1, 0x00); //

	// Clock setup
	writereg(state, R367_ANACTRL, 0x0D); /* PLL bypassed and disabled */

	if( state->master_clock == 58000000 ) {
		writereg(state, R367_PLLMDIV,27); /* IC runs at 58 MHz with a 27 MHz crystal */
		writereg(state, R367_PLLNDIV,232);
	} else {
		writereg(state, R367_PLLMDIV,1); /* IC runs at 54 MHz with a 27 MHz crystal */
		writereg(state, R367_PLLNDIV,8);
	}
	writereg(state, R367_PLLSETUP, 0x18);  /* ADC clock is equal to system clock */

	// Tuner setup
	writereg(state, R367_ANADIGCTRL, 0x8b); /* Buffer Q disabled, I Enabled, signed ADC */
	writereg(state, R367_DUAL_AD12, 0x04); /* ADCQ disabled */

	writereg(state, R367_QAM_FSM_SNR2_HTH, 0x23); /* Improves the C/N lock limit */
	writereg(state, R367_QAM_IQ_QAM, 0x01); /* ZIF/IF Automatic mode */
	writereg(state, R367_QAM_EQU_FFE_LEAKAGE, 0x83); /* Improving burst noise performances */
	writereg(state, R367_QAM_IQDEM_ADJ_EN, 0x05); /* Improving ACI performances */

	writereg(state, R367_ANACTRL, 0x00); /* PLL enabled and used */

	writereg(state, R367_I2CRPT, state->I2CRPT);
	state->demod_state    = QAMSet;
	return stat;
}

static void release(struct dvb_frontend* fe)
{
	struct stv_state *state=fe->demodulator_priv;
	printk("%s\n", __FUNCTION__);
	kfree(state);
}

static int gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct stv_state *state = fe->demodulator_priv;
	u8 i2crpt = state->I2CRPT & ~0x80;

	if (enable)
		i2crpt |= 0x80;
	if (writereg(state, R367_I2CRPT, i2crpt) < 0)
		return -1;
	state->I2CRPT = i2crpt;
	return 0;
}

#if 0
static int c_track(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	return DVBFE_ALGO_SEARCH_AGAIN;
}
#endif

#if 0
int (*set_property)(struct dvb_frontend* fe, struct dtv_property* tvp);
int (*get_property)(struct dvb_frontend* fe, struct dtv_property* tvp);
#endif

static int ofdm_lock(struct stv_state *state)
{
	int status = 0;
	u8 OFDM_Status;
	s32 DemodTimeOut = 10;
	s32 FECTimeOut = 0;
	s32 TSTimeOut = 0;
	u8 CPAMPMin = 255;
	u8 CPAMPValue;
	u8 SYR_STAT;
	u8 FFTMode;
	u8 TSStatus;

	msleep(state->m_SignalTimeOut);
	readreg(state, R367_OFDM_STATUS,&OFDM_Status);

	if (!(OFDM_Status & 0x40))
		return -1;
	//printk("lock 1\n");

	readreg(state, R367_OFDM_SYR_STAT,&SYR_STAT);
	FFTMode = (SYR_STAT & 0x0C) >> 2;

	switch(FFTMode)
	{
	    case 0: // 2K
		DemodTimeOut = 10;
		FECTimeOut = 150;
		TSTimeOut = 125;
		CPAMPMin = 20;
		break;
	    case 1: // 8K
		DemodTimeOut = 55;
		FECTimeOut = 600;
		TSTimeOut = 500;
		CPAMPMin = 80;
		break;
	    case 2: // 4K
		DemodTimeOut = 40;
		FECTimeOut = 300;
		TSTimeOut = 250;
		CPAMPMin = 30;
		break;
	}
	state->m_OFDM_FFTMode = FFTMode;
	readreg(state, R367_OFDM_PPM_CPAMP_DIR,&CPAMPValue);
	msleep(DemodTimeOut);
	{
	    // Release FEC and Read Solomon Reset
	    u8 tmp1;
	    u8 tmp2;
	    readreg(state, R367_OFDM_SFDLYSETH,&tmp1);
	    readreg(state, R367_TSGENERAL,&tmp2);
	    writereg(state, R367_OFDM_SFDLYSETH,tmp1 & ~0x08);
	    writereg(state, R367_TSGENERAL,tmp2 & ~0x01);
	}
	msleep(FECTimeOut);
	if( (OFDM_Status & 0x98) != 0x98 )
		;//return -1;
	//printk("lock 2\n");

	{
	    u8 Guard = (SYR_STAT & 0x03);
	    if(Guard < 2)
	    {
		u8 tmp;
		readreg(state, R367_OFDM_SYR_CTL,&tmp);
		writereg(state, R367_OFDM_SYR_CTL,tmp & ~0x04); // Clear AUTO_LE_EN
		readreg(state, R367_OFDM_SYR_UPDATE,&tmp);
		writereg(state, R367_OFDM_SYR_UPDATE,tmp & ~0x10); // Clear SYR_FILTER
	    } else {
		u8 tmp;
		readreg(state, R367_OFDM_SYR_CTL,&tmp);
		writereg(state, R367_OFDM_SYR_CTL,tmp | 0x04); // Set AUTO_LE_EN
		readreg(state, R367_OFDM_SYR_UPDATE,&tmp);
		writereg(state, R367_OFDM_SYR_UPDATE,tmp | 0x10); // Set SYR_FILTER
	    }

	    // apply Sfec workaround if 8K 64QAM CR!=1/2
	    if( FFTMode == 1)
	    {
		u8 tmp[2];
		readregs(state, R367_OFDM_TPS_RCVD2, tmp, 2);
		if( ((tmp[0] & 0x03) == 0x02) && (( tmp[1] & 0x07 ) != 0) )
		{
		    writereg(state, R367_OFDM_SFDLYSETH,0xc0);
		    writereg(state, R367_OFDM_SFDLYSETM,0x60);
		    writereg(state, R367_OFDM_SFDLYSETL,0x00);
		}
		else
		{
		    writereg(state, R367_OFDM_SFDLYSETH,0x00);
		}
	    }
	}
	msleep(TSTimeOut);
	readreg(state, R367_OFDM_TSSTATUS,&TSStatus);
	if( (TSStatus & 0x80) != 0x80 )
		return -1;
	//printk("lock 3\n");
	return status;
}


static int set_parameters(struct dvb_frontend *fe)
{
	int stat;
	struct stv_state *state = fe->demodulator_priv;
	u32 OF = 0;
	u32 IF;

	switch (fe->dtv_property_cache.delivery_system) {
	case SYS_DVBC_ANNEX_A:
		state->omode = OM_DVBC;
		/* symbol rate 0 might cause an oops */
		if (fe->dtv_property_cache.symbol_rate == 0) {
			printk(KERN_ERR "stv0367dd: Invalid symbol rate\n");
			return -EINVAL;
		}
		break;
	case SYS_DVBT:
		state->omode = OM_DVBT;
		break;
	default:
		return -EINVAL;
	}
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe);
	state->modulation = fe->dtv_property_cache.modulation;
	state->symbol_rate = fe->dtv_property_cache.symbol_rate;
	state->bandwidth = fe->dtv_property_cache.bandwidth_hz;
	fe->ops.tuner_ops.get_if_frequency(fe, &IF);
	//fe->ops.tuner_ops.get_frequency(fe, &IF);

	switch(state->omode) {
	case OM_DVBT:
		stat = OFDM_Start(state, OF, IF);
		ofdm_lock(state);
		break;
	case OM_DVBC:
	case OM_QAM_ITU_C:
		stat = QAM_Start(state, OF, IF);
		break;
	default:
		stat = -EINVAL;
	}
	//printk("%s IF=%d OF=%d done\n", __FUNCTION__, IF, OF);
	return stat;
}

#if 0
static int c_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	//struct stv_state *state = fe->demodulator_priv;
	//printk("%s\n", __FUNCTION__);
	return 0;
}

static int OFDM_GetLockStatus(struct stv_state *state, LOCK_STATUS* pLockStatus, s32 Time)
{
	int status = STATUS_SUCCESS;
	u8 OFDM_Status;
	s32 DemodTimeOut = 0;
	s32 FECTimeOut = 0;
	s32 TSTimeOut = 0;
	u8 CPAMPMin = 255;
	u8 CPAMPValue;
	bool SYRLock;
	u8 SYR_STAT;
	u8 FFTMode;
	u8 TSStatus;

	readreg(state, R367_OFDM_STATUS,&OFDM_Status);

	SYRLock = (OFDM_Status & 0x40) != 0;

	if( Time > m_SignalTimeOut && !SYRLock )
	{
	    *pLockStatus = NEVER_LOCK;
	    break;
	}

	if( !SYRLock ) break;

	*pLockStatus = SIGNAL_PRESENT;

	// Check Mode

	readreg(state, R367_OFDM_SYR_STAT,&SYR_STAT);
	FFTMode = (SYR_STAT & 0x0C) >> 2;

	switch(FFTMode)
	{
	    case 0: // 2K
		DemodTimeOut = 10;
		FECTimeOut = 150;
		TSTimeOut = 125;
		CPAMPMin = 20;
		break;
	    case 1: // 8K
		DemodTimeOut = 55;
		FECTimeOut = 600;
		TSTimeOut = 500;
		CPAMPMin = 80;
		break;
	    case 2: // 4K
		DemodTimeOut = 40;
		FECTimeOut = 300;
		TSTimeOut = 250;
		CPAMPMin = 30;
		break;
	}

	m_OFDM_FFTMode = FFTMode;

	if( m_DemodTimeOut == 0 && m_bFirstTimeLock )
	{
	    m_DemodTimeOut = Time + DemodTimeOut;
	    //break;
	}

	readreg(state, R367_OFDM_PPM_CPAMP_DIR,&CPAMPValue);

	if( Time <= m_DemodTimeOut && CPAMPValue < CPAMPMin )
	{
	    break;
	}

	if( CPAMPValue < CPAMPMin && m_bFirstTimeLock )
	{
	    // initiate retry
	    *pLockStatus = NEVER_LOCK;
	    break;
	}

	if( CPAMPValue < CPAMPMin ) break;

	*pLockStatus = DEMOD_LOCK;

	if( m_FECTimeOut == 0 && m_bFirstTimeLock )
	{
	    // Release FEC and Read Solomon Reset
	    u8 tmp1;
	    u8 tmp2;
	    readreg(state, R367_OFDM_SFDLYSETH,&tmp1);
	    readreg(state, R367_TSGENERAL,&tmp2);
	    writereg(state, R367_OFDM_SFDLYSETH,tmp1 & ~0x08);
	    writereg(state, R367_TSGENERAL,tmp2 & ~0x01);

	    m_FECTimeOut = Time + FECTimeOut;
	}

	// Wait for TSP_LOCK, LK, PRF
	if( (OFDM_Status & 0x98) != 0x98 )
	{
	    if( Time > m_FECTimeOut ) *pLockStatus = NEVER_LOCK;
	    break;
	}

	if( m_bFirstTimeLock && m_TSTimeOut == 0)
	{
	    u8 Guard = (SYR_STAT & 0x03);
	    if(Guard < 2)
	    {
		u8 tmp;
		readreg(state, R367_OFDM_SYR_CTL,&tmp);
		writereg(state, R367_OFDM_SYR_CTL,tmp & ~0x04); // Clear AUTO_LE_EN
		readreg(state, R367_OFDM_SYR_UPDATE,&tmp);
		writereg(state, R367_OFDM_SYR_UPDATE,tmp & ~0x10); // Clear SYR_FILTER
	    } else {
		u8 tmp;
		readreg(state, R367_OFDM_SYR_CTL,&tmp);
		writereg(state, R367_OFDM_SYR_CTL,tmp | 0x04); // Set AUTO_LE_EN
		readreg(state, R367_OFDM_SYR_UPDATE,&tmp);
		writereg(state, R367_OFDM_SYR_UPDATE,tmp | 0x10); // Set SYR_FILTER
	    }

	    // apply Sfec workaround if 8K 64QAM CR!=1/2
	    if( FFTMode == 1)
	    {
		u8 tmp[2];
		readreg(state, R367_OFDM_TPS_RCVD2,tmp,2);
		if( ((tmp[0] & 0x03) == 0x02) && (( tmp[1] & 0x07 ) != 0) )
		{
		    writereg(state, R367_OFDM_SFDLYSETH,0xc0);
		    writereg(state, R367_OFDM_SFDLYSETM,0x60);
		    writereg(state, R367_OFDM_SFDLYSETL,0x00);
		}
		else
		{
		    writereg(state, R367_OFDM_SFDLYSETH,0x00);
		}
	    }

	    m_TSTimeOut = Time + TSTimeOut;
	}
	readreg(state, R367_OFDM_TSSTATUS,&TSStatus);
	if( (TSStatus & 0x80) != 0x80 )
	{
		if( Time > m_TSTimeOut ) *pLockStatus = NEVER_LOCK;
	    break;
	}
	*pLockStatus = MPEG_LOCK;
	m_bFirstTimeLock = false;
	return status;
}

#endif

static int read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct stv_state *state = fe->demodulator_priv;
	*status=0;

	switch(state->demod_state) {
	case QAMStarted:
	{
		u8 FEC_Lock;
		u8 QAM_Lock;

		readreg(state, R367_QAM_FSM_STS, &QAM_Lock);
		QAM_Lock &= 0x0F;
		if (QAM_Lock >10)
			*status|=0x07;
		readreg(state, R367_QAM_FEC_STATUS,&FEC_Lock);
		if (FEC_Lock&2)
			*status|=0x1f;
		if (state->m_bFirstTimeLock) {
			state->m_bFirstTimeLock = false;
			// QAM_AGC_ACCUMRSTSEL to Tracking;
			writereg(state, R367_QAM_AGC_CTL, state->m_Save_QAM_AGC_CTL);
		}
		break;
	}
	case OFDMStarted:
	{
		u8 OFDM_Status;
		u8 TSStatus;

		readreg(state, R367_OFDM_TSSTATUS, &TSStatus);

		readreg(state, R367_OFDM_STATUS, &OFDM_Status);
		if (OFDM_Status & 0x40)
			*status |= FE_HAS_SIGNAL;

		if ((OFDM_Status & 0x98) == 0x98)
			*status|=0x0f;

		if (TSStatus & 0x80)
			*status |= 0x1f;
		break;
	}
	default:
		break;
	}
	return 0;
}

static int read_ber_ter(struct dvb_frontend *fe, u32 *ber)
{
	struct stv_state *state = fe->demodulator_priv;
	u32 err;
	u8 cnth, cntm, cntl;

#if 1
	readreg(state, R367_OFDM_SFERRCNTH, &cnth);

	if (cnth & 0x80) {
		*ber = state->ber; 
		return 0;
	}

	readreg(state, R367_OFDM_SFERRCNTM, &cntm);
	readreg(state, R367_OFDM_SFERRCNTL, &cntl);

	err = ((cnth & 0x7f) << 16) | (cntm << 8) | cntl;
	
#if 0
	{
		u64 err64;
		err64 = (u64) err;
		err64 *= 1000000000ULL;
		err64 >>= 21;
		err = err64;
	}
#endif
#else
	readreg(state, R367_OFDM_ERRCNT1HM, &cnth);

#endif
	*ber = state->ber = err;
	return 0;
}

static int read_ber_cab(struct dvb_frontend *fe, u32 *ber)
{
	struct stv_state *state = fe->demodulator_priv;
	u32 err;
	u8 cntm, cntl, ctrl;

	readreg(state, R367_QAM_BERT_1, &ctrl);
	if (!(ctrl & 0x20)) {
		readreg(state, R367_QAM_BERT_2, &cntl);
		readreg(state, R367_QAM_BERT_3, &cntm);
		err = (cntm << 8) | cntl;
		//printk("err %04x\n", err);
		state->ber = err;
		writereg(state, R367_QAM_BERT_1, 0x27);
	}
	*ber = (u32) state->ber;
	return 0;
}

static int read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct stv_state *state = fe->demodulator_priv;

	if (state->demod_state == QAMStarted)
		return read_ber_cab(fe, ber);
	if (state->demod_state == OFDMStarted)
		return read_ber_ter(fe, ber);
	*ber = 0;
	return 0;
}

static int read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	if (fe->ops.tuner_ops.get_rf_strength)
		fe->ops.tuner_ops.get_rf_strength(fe, strength);
	else
		*strength = 0;
	return 0;
}

static int read_snr(struct dvb_frontend *fe, u16 *snr)
{
 	struct stv_state *state = fe->demodulator_priv;
	s32 snr2 = 0;

	switch(state->demod_state) {
	case QAMStarted:
		QAM_GetSignalToNoise(state, &snr2);
		break;
	case OFDMStarted:
		OFDM_GetSignalToNoise(state, &snr2);
		break;
	default:
		break;
	}
	*snr = snr2&0xffff;
	return 0;
}

static int read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct stv_state *state = fe->demodulator_priv;
	u8 errl, errm, errh;
	u8 val;

	switch(state->demod_state) {
	case QAMStarted:
		readreg(state, R367_QAM_RS_COUNTER_4, &errl);
		readreg(state, R367_QAM_RS_COUNTER_5, &errm);
		*ucblocks = (errm << 8) | errl;
		break;
	case OFDMStarted:
		readreg(state, R367_OFDM_SFERRCNTH, &val);
		if ((val & 0x80) == 0) {
			readreg(state, R367_OFDM_ERRCNT1H, &errh);
			readreg(state, R367_OFDM_ERRCNT1M, &errl);
			readreg(state, R367_OFDM_ERRCNT1L, &errm);
			state->ucblocks = (errh <<16) | (errm << 8) | errl;
		}
		*ucblocks = state->ucblocks;
		break;
	default:
		*ucblocks = 0;
		break;
	}
	return 0;
}

static int c_get_tune_settings(struct dvb_frontend *fe,
				    struct dvb_frontend_tune_settings *sets)
{
	sets->min_delay_ms=3000;
	sets->max_drift=0;
	sets->step_size=0;
	return 0;
}

static int get_tune_settings(struct dvb_frontend *fe,
			     struct dvb_frontend_tune_settings *sets)
{
	switch (fe->dtv_property_cache.delivery_system) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		return c_get_tune_settings(fe, sets);
	default:
		/* DVB-T: Use info.frequency_stepsize. */
		return -EINVAL;
	}
}

#if 0
static int t_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	//struct stv_state *state = fe->demodulator_priv;
	//printk("%s\n", __FUNCTION__);
	return 0;
}

static enum dvbfe_algo algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_CUSTOM;
}
#endif

static struct dvb_frontend_ops common_ops = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBT },
	.info = {
		.name = "STV0367 DVB-C DVB-T",
		.frequency_stepsize = 166667,	/* DVB-T only */
		.frequency_min = 47000000,	/* DVB-T: 47125000 */
		.frequency_max = 865000000,	/* DVB-C: 862000000 */
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = /* DVB-C */
			FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
			FE_CAN_QAM_128 | FE_CAN_QAM_256 |
			FE_CAN_FEC_AUTO |
			/* DVB-T */
			FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS
	},
	.release = release,
	.i2c_gate_ctrl = gate_ctrl,

	.get_tune_settings = get_tune_settings,

	.set_frontend = set_parameters,

	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
};


static void init_state(struct stv_state *state, struct stv0367_cfg *cfg)
{
	u32 ulENARPTLEVEL = 5;
	u32 ulQAMInversion = 2;
	state->omode = OM_NONE;
	state->adr = cfg->adr;
	state->cont_clock = cfg->cont_clock;

	mutex_init(&state->mutex);
	mutex_init(&state->ctlock);

	memcpy(&state->frontend.ops, &common_ops, sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;

	state->master_clock = 58000000;
	state->adc_clock = 58000000;
	state->I2CRPT = 0x08 | ((ulENARPTLEVEL & 0x07) << 4);
	state->qam_inversion = ((ulQAMInversion & 3) << 6 );
	state->demod_state   = Off;
}


struct dvb_frontend *stv0367_attach(struct i2c_adapter *i2c, struct stv0367_cfg *cfg,
				    struct dvb_frontend **fe_t)
{
	struct stv_state *state = NULL;

	state = kzalloc(sizeof(struct stv_state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->i2c = i2c;
	init_state(state, cfg);

	if (attach_init(state)<0)
		goto error;
	return &state->frontend;

error:
	printk("stv0367: not found\n");
	kfree(state);
	return NULL;
}


MODULE_DESCRIPTION("STV0367DD driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(stv0367_attach);



