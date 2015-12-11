/*
 * tda18212: Driver for the TDA18212 tuner
 *
 * Copyright (C) 2011-2013 Digital Devices GmbH
 *
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

#ifndef CHK_ERROR
#define CHK_ERROR(s) if ((status = s) < 0) break
#endif

#define MASTER_PSM_AGC1     0
#define MASTER_AGC1_6_15dB  1

#define SLAVE_PSM_AGC1      1
#define SLAVE_AGC1_6_15dB   0

/* 0 = 2 Vpp ... 2 = 1 Vpp,   7 = 0.5 Vpp */
#define IF_LEVEL_DVBC    2
#define IF_LEVEL_DVBT    2

enum {
	ID_1                = 0x00,
	ID_2                = 0x01,
	ID_3                = 0x02,
	THERMO_1,
	THERMO_2,
	POWER_STATE_1,
	POWER_STATE_2,
	INPUT_POWER_LEVEL,
	IRQ_STATUS,
	IRQ_ENABLE,
	IRQ_CLEAR,
	IRQ_SET,
	AGC1_1,
	AGC2_1,
	AGCK_1,
	RF_AGC_1,
	IR_MIXER_1          = 0x10,
	AGC5_1,
	IF_AGC,
	IF_1,
	REFERENCE,
	IF_FREQUENCY_1,
	RF_FREQUENCY_1,
	RF_FREQUENCY_2,
	RF_FREQUENCY_3,
	MSM_1,
	MSM_2,
	PSM_1,
	DCC_1,
	FLO_MAX,
	IR_CAL_1,
	IR_CAL_2,
	IR_CAL_3            = 0x20,
	IR_CAL_4,
	VSYNC_MGT,
	IR_MIXER_2,
	AGC1_2,
	AGC5_2,
	RF_CAL_1,
	RF_CAL_2,
	RF_CAL_3,
	RF_CAL_4,
	RF_CAL_5,
	RF_CAL_6,
	RF_FILTER_1,
	RF_FILTER_2,
	RF_FILTER_3,
	RF_BAND_PASS_FILTER,
	CP_CURRENT          = 0x30,
	AGC_DET_OUT         = 0x31,
	RF_AGC_GAIN_1       = 0x32,
	RF_AGC_GAIN_2       = 0x33,
	IF_AGC_GAIN         = 0x34,
	POWER_1             = 0x35,
	POWER_2             = 0x36,
	MISC_1,
	RFCAL_LOG_1,
	RFCAL_LOG_2,
	RFCAL_LOG_3,
	RFCAL_LOG_4,
	RFCAL_LOG_5,
	RFCAL_LOG_6,
	RFCAL_LOG_7,
	RFCAL_LOG_8,
	RFCAL_LOG_9         = 0x40,
	RFCAL_LOG_10        = 0x41,
	RFCAL_LOG_11        = 0x42,
	RFCAL_LOG_12        = 0x43,
	REG_MAX,
};

enum HF_Standard {
	HF_None = 0, HF_B, HF_DK, HF_G, HF_I, HF_L, HF_L1, HF_MN, HF_FM_Radio,
	HF_AnalogMax, HF_DVBT_6MHZ, HF_DVBT_7MHZ, HF_DVBT_8MHZ,
	HF_DVBT, HF_ATSC,  HF_DVBC_6MHZ,  HF_DVBC_7MHZ,
	HF_DVBC_8MHZ, HF_DVBC
};

struct SStandardParams {
	s32   m_IFFrequency;
	u32   m_BandWidth;
	u8    m_IF_1;     /* FF IF_HP_fc:2 IF_Notch:1 LP_FC_Offset:2 LP_FC:3 */
	u8    m_IR_MIXER_2;   /* 03 :6 HI_Pass:1 DC_Notch:1 */
	u8    m_AGC1_1;       /* 0F :4 AGC1_Top:4 */
	u8    m_AGC2_1;       /* 0F :4 AGC2_Top:4 */
	/*EF RF_AGC_Adapt:1 RF_AGC_Adapt_Top:2 :1 RF_Atten_3dB:1 RF_AGC_Top:3 */
	u8    m_RF_AGC_1_Low;
	/*EF RF_AGC_Adapt:1 RF_AGC_Adapt_Top:2 :1 RF_Atten_3dB:1 RF_AGC_Top:3 */
	u8    m_RF_AGC_1_High;
	u8    m_IR_MIXER_1;   /* 0F :4 IR_mixer_Top:4 */
	u8    m_AGC5_1;       /* 1F :3 AGC5_Ana AGC5_Top:4 */
	u8    m_AGCK_1;       /* 0F :4 AGCK_Step:2 AGCK_Mode:2 */
	u8    m_PSM_1;        /* 20 :2 PSM_StoB:1 :5 */
	bool  m_AGC1_Freeze;
	bool  m_LTO_STO_immune;
};

#if 0
static struct SStandardParams
m_StandardTable[HF_DVBC_8MHZ - HF_DVBT_6MHZ + 1] = {
	{ 3250000, 6000000, 0x20, 0x03, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_6MHZ */
	{ 3500000, 7000000, 0x31, 0x01, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_7MHZ */
	{ 4000000, 8000000, 0x22, 0x01, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_8MHZ */
	{ 0000000,       0, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, false, false },   /* HF_DVBT (Unused) */
	{ 3250000, 6000000, 0x20, 0x03, 0x0A, 0x07, 0x6D,
	  0x6D, 0x0E, 0x0E, 0x02, 0x20, false, false },    /* HF_ATSC */
	{ 3600000, 6000000, 0x10, 0x01, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },    /* HF_DVBC_6MHZ */
	{ 5000000, 7000000, 0x93, 0x03, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },
	/* HF_DVBC_7MHZ (not documented by NXP, use same settings as 8 MHZ) */
	{ 5000000, 8000000, 0x43, 0x03, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },    /* HF_DVBC_8MHZ */
};
#else
static struct SStandardParams
m_StandardTable[HF_DVBC_8MHZ - HF_DVBT_6MHZ + 1] = {
	{ 4000000, 6000000, 0x41, 0x03, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_6MHZ */
	{ 4500000, 7000000, 0x42, 0x03, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_7MHZ */
	{ 5000000, 8000000, 0x43, 0x03, 0x00, 0x07, 0x2B,
	  0x2C, 0x0B, 0x0B, 0x02, 0x20, false, false },    /* HF_DVBT_8MHZ */
	/* ------------------------------ */
	{ 0000000,       0, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, false, false },    /* HF_DVBT (Unused)*/
	{ 3250000, 6000000, 0x20, 0x03, 0x0A, 0x07, 0x6D,
	  0x6D, 0x0E, 0x0E, 0x02, 0x20, false, false },    /* HF_ATSC */
	{ 3600000, 6000000, 0x10, 0x01, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },    /* HF_DVBC_6MHZ */
	{ 5000000, 7000000, 0x93, 0x03, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },
	/* HF_DVBC_7MHZ (not documented by NXP, use same settings as 8 MHZ) */
	{ 5000000, 8000000, 0x43, 0x03, 0x00, 0x07, 0x83,
	  0x83, 0x0B, 0x0B, 0x02, 0x00, true , true  },    /* HF_DVBC_8MHZ */
};
#endif

struct tda_state {
	struct i2c_adapter *i2c;
	u8 adr;

	enum HF_Standard m_Standard;
	u32   m_Frequency;
	u32   IF;

	bool    m_isMaster;
	bool    m_bPowerMeasurement;
	bool    m_bLTEnable;
	bool    m_bEnableFreeze;

	u16   m_ID;

	s32    m_SettlingTime;

	u8    m_IFLevelDVBC;
	u8    m_IFLevelDVBT;
	u8    Regs[REG_MAX];
	u8    m_LastPowerLevel;
};

static int i2c_readn(struct i2c_adapter *adapter, u8 adr, u8 *data, int len)
{
	struct i2c_msg msgs[1] = {{.addr = adr,  .flags = I2C_M_RD,
				   .buf  = data, .len   = len} };
	return (i2c_transfer(adapter, msgs, 1) == 1) ? 0 : -1;
}

static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	struct i2c_msg msgs[2] = { { .addr = adr, .flags = 0,
				     .buf = msg, .len = len},
				   { .addr = adr, .flags = I2C_M_RD,
				     .buf = answ, .len = alen } };
	if (i2c_transfer(adap, msgs, 2) != 2) {
		pr_err("tda18212dd: i2c_read error\n");
		return -1;
	}
	return 0;
}

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	if (i2c_transfer(adap, &msg, 1) != 1) {
		pr_err("tda18212: i2c_write error\n");
		return -1;
	}
	return 0;
}

static int write_regs(struct tda_state *state,
		      u8 SubAddr, u8 *Regs, u16 nRegs)
{
	u8 data[REG_MAX + 1];

	data[0] = SubAddr;
	memcpy(data + 1, Regs, nRegs);
	return i2c_write(state->i2c, state->adr, data, nRegs + 1);
}

static int write_reg(struct tda_state *state, u8 SubAddr, u8 Reg)
{
	u8 msg[2] = {SubAddr, Reg};

	return i2c_write(state->i2c, state->adr, msg, 2);
}

static int Read(struct tda_state *state, u8 *Regs)
{
	return i2c_readn(state->i2c, state->adr, Regs, REG_MAX);
}

static int update_regs(struct tda_state *state, u8 RegFrom, u8 RegTo)
{
	return write_regs(state, RegFrom,
			  &state->Regs[RegFrom], RegTo-RegFrom + 1);
}

static int update_reg(struct tda_state *state, u8 Reg)
{
	return write_reg(state, Reg, state->Regs[Reg]);
}


static int read_regs(struct tda_state *state,
		    u8 SubAddr, u8 *Regs, u16 nRegs)
{
	return i2c_read(state->i2c, state->adr,
			&SubAddr, 1, Regs, nRegs);
}

static int read_reg(struct tda_state *state,
		   u8 SubAddr, u8 *Reg)
{
	return i2c_read(state->i2c, state->adr,
			&SubAddr, 1, Reg, 1);
}

static int read_reg1(struct tda_state *state, u8 Reg)
{
	return read_reg(state, Reg, &state->Regs[Reg]);
}

static void init_state(struct tda_state *state)
{
	u32   ulIFLevelDVBC = IF_LEVEL_DVBC;
	u32   ulIFLevelDVBT = IF_LEVEL_DVBT;
	u32   ulPowerMeasurement = 1;
	u32   ulLTEnable = 1;
	u32   ulEnableFreeze = 0;

	state->m_Frequency    = 0;
	state->m_isMaster = true;
	state->m_ID = 0;
	state->m_LastPowerLevel = 0xFF;
	state->m_IFLevelDVBC = (ulIFLevelDVBC & 0x07);
	state->m_IFLevelDVBT = (ulIFLevelDVBT & 0x07);
	state->m_bPowerMeasurement = (ulPowerMeasurement != 0);
	state->m_bLTEnable = (ulLTEnable != 0);
	state->m_bEnableFreeze = (ulEnableFreeze != 0);
}

static int StartCalibration(struct tda_state *state)
{
	int  status = 0;
	do {
		state->Regs[POWER_2] &= ~0x02; /* RSSI CK = 31.25 kHz */
		CHK_ERROR(update_reg(state, POWER_2));

		/* AGC1 Do Step = 2 */
		state->Regs[AGC1_2] = (state->Regs[AGC1_2] & ~0x60) | 0x40;
		CHK_ERROR(update_reg(state, AGC1_2));        /* AGC */

		/* AGC2 Do Step = 1 */
		state->Regs[RF_FILTER_3] =
			(state->Regs[RF_FILTER_3] & ~0xC0) | 0x40;
		CHK_ERROR(update_reg(state, RF_FILTER_3));

		/* AGCs Assym Up Step = 3   // Datasheet sets all bits to 1! */
		state->Regs[AGCK_1] |= 0xC0;
		CHK_ERROR(update_reg(state, AGCK_1));

		/* AGCs Assym Do Step = 2 */
		state->Regs[AGC5_1] = (state->Regs[AGC5_1] & ~0x60) | 0x40;
		CHK_ERROR(update_reg(state, AGC5_1));

		state->Regs[IRQ_CLEAR] |= 0x80; /* Reset IRQ */
		CHK_ERROR(update_reg(state, IRQ_CLEAR));

		state->Regs[MSM_1] = 0x3B; /* Set Calibration */
		state->Regs[MSM_2] = 0x01; /* Start MSM */
		CHK_ERROR(update_regs(state, MSM_1, MSM_2));
		state->Regs[MSM_2] = 0x00;
	} while (0);
	return status;
}

static int FinishCalibration(struct tda_state *state)
{
	int status = 0;
	u8 RFCal_Log[12];

	do {
		u8 IRQ = 0;
		int Timeout = 150; /* 1.5 s */
		while (true) {
			CHK_ERROR(read_reg(state, IRQ_STATUS, &IRQ));
			if ((IRQ & 0x80) != 0)
				break;
			Timeout -= 1;
			if (Timeout == 0) {
				status = -1;
				break;
			}
			usleep_range(10000, 12000);
		}
		CHK_ERROR(status);

		state->Regs[FLO_MAX] = 0x0A;
		CHK_ERROR(update_reg(state, FLO_MAX));

		state->Regs[AGC1_1] &= ~0xC0;
		if (state->m_bLTEnable)
			state->Regs[AGC1_1] |= 0x80;    /* LTEnable */

		state->Regs[AGC1_1] |= (state->m_isMaster ?
					MASTER_AGC1_6_15dB :
					SLAVE_AGC1_6_15dB) << 6;
		CHK_ERROR(update_reg(state, AGC1_1));

		state->Regs[PSM_1] &= ~0xC0;
		state->Regs[PSM_1] |= (state->m_isMaster ?
				       MASTER_PSM_AGC1 : SLAVE_PSM_AGC1) << 6;
		CHK_ERROR(update_reg(state, PSM_1));

		state->Regs[REFERENCE] |= 0x03; /* XTOUT = 3 */
		CHK_ERROR(update_reg(state, REFERENCE));

		CHK_ERROR(read_regs(state, RFCAL_LOG_1,
				    RFCal_Log, sizeof(RFCal_Log)));
	} while (0);
	return status;
}

static int PowerOn(struct tda_state *state)
{
	state->Regs[POWER_STATE_2] &= ~0x0F;
	update_reg(state, POWER_STATE_2);
	/* Digital clock source = Sigma Delta */
	state->Regs[REFERENCE] |= 0x40;
	update_reg(state, REFERENCE);
	return 0;
}

static int Standby(struct tda_state *state)
{
	int status = 0;

	do {
		/* Digital clock source = Quarz */
		state->Regs[REFERENCE] &= ~0x40;
		CHK_ERROR(update_reg(state, REFERENCE));

		state->Regs[POWER_STATE_2] &= ~0x0F;
		state->Regs[POWER_STATE_2] |= state->m_isMaster ? 0x08 : 0x0E;
		CHK_ERROR(update_reg(state, POWER_STATE_2));
	} while (0);
	return status;
}

static int attach_init(struct tda_state *state)
{
	int stat = 0;
	u8 Id[2];
	u8 PowerState = 0x00;

	state->m_Standard = HF_None;

	/* first read after cold reset sometimes fails on some cards,
	   try twice */
	stat = read_regs(state, ID_1, Id, sizeof(Id));
	stat = read_regs(state, ID_1, Id, sizeof(Id));
	if (stat < 0)
		return -1;

	state->m_ID = ((Id[0] & 0x7F) << 8) | Id[1];
	state->m_isMaster = ((Id[0] & 0x80) != 0);
	if (!state->m_isMaster)
		state->m_bLTEnable = false;

	/*pr_info("tda18212dd: ChipID %04x %s\n", state->m_ID,
	  state->m_isMaster ? "master" : "slave");*/

	if (state->m_ID != 18212)
		return -1;

	stat = read_reg(state, POWER_STATE_1 , &PowerState);
	if (stat < 0)
		return stat;

	/*pr_info("tda18212dd: PowerState %02x\n", PowerState);*/

	if (state->m_isMaster) {
		if (PowerState & 0x02) {
			/* msleep for XTAL Calibration
			   (on a PC this should be long done) */
			u8 IRQStatus = 0;
			int Timeout = 10;

			while (Timeout > 0) {
				read_reg(state, IRQ_STATUS, &IRQStatus);
				if (IRQStatus & 0x20)
					break;
				Timeout -= 1;
				usleep_range(10000, 12000);
			}
			if ((IRQStatus & 0x20) == 0)
				stat = -ETIMEDOUT;
		}
	} else {
		write_reg(state, FLO_MAX, 0x00);
		write_reg(state, CP_CURRENT, 0x68);
	}
	Read(state, state->Regs);

	PowerOn(state);
	StartCalibration(state);
	FinishCalibration(state);
	Standby(state);

#if 0
	{
		u8 RFCal_Log[12];

		read_regs(state, RFCAL_LOG_1, RFCal_Log, sizeof(RFCal_Log));
		pr_info("RFCal Log: %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x\n",
			RFCal_Log[0], RFCal_Log[1],
			RFCal_Log[2], RFCal_Log[3],
			RFCal_Log[4], RFCal_Log[5],
			RFCal_Log[6], RFCal_Log[7],
			RFCal_Log[8], RFCal_Log[9],
			RFCal_Log[10], RFCal_Log[11]);
	}
#endif
	return stat;
}

static int PowerMeasurement(struct tda_state *state, u8 *pPowerLevel)
{
	int status = 0;

	do {
		u8 IRQ = 0;
		int Timeout = 70; /* 700 ms */

		state->Regs[IRQ_CLEAR] |= 0x80; /* Reset IRQ */
		CHK_ERROR(update_reg(state, IRQ_CLEAR));

		state->Regs[MSM_1] = 0x80; /* power measurement */
		state->Regs[MSM_2] = 0x01; /* Start MSM */
		CHK_ERROR(update_regs(state, MSM_1, MSM_2));
		state->Regs[MSM_2] = 0x00;

		while (true) {
			CHK_ERROR(read_reg(state, IRQ_STATUS, &IRQ));
			if ((IRQ & 0x80) != 0)
				break;
			Timeout -= 1;
			if (Timeout == 0) {
				status = -1;
				break;
			}
			usleep_range(10000, 12000);
		}
		CHK_ERROR(status);

		CHK_ERROR(read_reg1(state, INPUT_POWER_LEVEL));
		*pPowerLevel = state->Regs[INPUT_POWER_LEVEL] & 0x7F;

		if (*pPowerLevel > 110)
			*pPowerLevel = 110;
	} while (0);
	/* pr_info("PL %d\n", *pPowerLevel); */
	return status;
}

static int SetFrequency(struct tda_state *state, u32 Frequency,
			enum HF_Standard Standard)
{
	int status = 0;
	struct SStandardParams *StandardParams;
	u32   f = Frequency / 1000;
	u8 IRQ = 0;
	int Timeout = 25; /* 250 ms */
	u32 fRatio = Frequency / 16000000;
	u32 fDelta = Frequency - fRatio * 16000000;

	if (Standard < HF_DVBT_6MHZ || Standard > HF_DVBC_8MHZ)
		return -EINVAL;
	StandardParams = &m_StandardTable[Standard - HF_DVBT_6MHZ];

	if (StandardParams->m_IFFrequency == 0)
		return -EINVAL;
	state->m_Standard = HF_None;
	state->m_Frequency = 0;

	do {
		/* IF Level */
		state->Regs[IF_AGC] = (Standard >= HF_DVBC_6MHZ) ?
			state->m_IFLevelDVBC : state->m_IFLevelDVBT;
		CHK_ERROR(update_reg(state, IF_AGC));

		/* Standard setup */
		state->Regs[IF_1] = StandardParams->m_IF_1;
		CHK_ERROR(update_reg(state, IF_1));

		state->Regs[IR_MIXER_2] = (state->Regs[IR_MIXER_2] & ~0x03) |
			StandardParams->m_IR_MIXER_2;
		CHK_ERROR(update_reg(state, IR_MIXER_2));

		state->Regs[AGC1_1] = (state->Regs[AGC1_1] & ~0x0F) |
			StandardParams->m_AGC1_1;
		CHK_ERROR(update_reg(state, AGC1_1));

		state->Regs[AGC2_1] = (state->Regs[AGC2_1] & ~0x0F) |
			StandardParams->m_AGC2_1;
		CHK_ERROR(update_reg(state, AGC2_1));

		state->Regs[RF_AGC_1] &= ~0xEF;
		if (Frequency < 291000000)
			state->Regs[RF_AGC_1] |= StandardParams->m_RF_AGC_1_Low;
		else
			state->Regs[RF_AGC_1] |=
				StandardParams->m_RF_AGC_1_High;
		CHK_ERROR(update_reg(state, RF_AGC_1));

		state->Regs[IR_MIXER_1] =
			(state->Regs[IR_MIXER_1] & ~0x0F) |
			StandardParams->m_IR_MIXER_1;
		CHK_ERROR(update_reg(state, IR_MIXER_1));

		state->Regs[AGC5_1] = (state->Regs[AGC5_1] & ~0x1F) |
			StandardParams->m_AGC5_1;
		CHK_ERROR(update_reg(state, AGC5_1));

		state->Regs[AGCK_1] = (state->Regs[AGCK_1] & ~0x0F) |
			StandardParams->m_AGCK_1;
		CHK_ERROR(update_reg(state, AGCK_1));

		state->Regs[PSM_1] = (state->Regs[PSM_1] & ~0x20) |
			StandardParams->m_PSM_1;
		CHK_ERROR(update_reg(state, PSM_1));

		state->Regs[IF_FREQUENCY_1] = (StandardParams->m_IFFrequency /
					       50000);
		CHK_ERROR(update_reg(state, IF_FREQUENCY_1));

		if (state->m_isMaster && StandardParams->m_LTO_STO_immune) {
			u8 tmp;
			u8 RF_Filter_Gain;

			CHK_ERROR(read_reg(state, RF_AGC_GAIN_1, &tmp));
			RF_Filter_Gain = (tmp & 0x30) >> 4;

			state->Regs[RF_FILTER_1] =
				(state->Regs[RF_FILTER_1] & ~0x0C) |
				(RF_Filter_Gain << 2);
			CHK_ERROR(update_reg(state, RF_FILTER_1));

			state->Regs[RF_FILTER_1] |= 0x10;    /* Force */
			CHK_ERROR(update_reg(state, RF_FILTER_1));

			while (RF_Filter_Gain != 0) {
				RF_Filter_Gain -= 1;
				state->Regs[RF_FILTER_1] =
					(state->Regs[RF_FILTER_1] & ~0x0C) |
					(RF_Filter_Gain << 2);
				CHK_ERROR(update_reg(state, RF_FILTER_1));
				usleep_range(10000, 12000);
			}
			CHK_ERROR(status);

			state->Regs[RF_AGC_1] |=  0x08;
			CHK_ERROR(update_reg(state, RF_AGC_1));
		}

		state->Regs[IRQ_CLEAR] |= 0x80; /* Reset IRQ */
		CHK_ERROR(update_reg(state, IRQ_CLEAR));

		CHK_ERROR(PowerOn(state));

		state->Regs[RF_FREQUENCY_1] = ((f >> 16) & 0xFF);
		state->Regs[RF_FREQUENCY_2] = ((f >>  8) & 0xFF);
		state->Regs[RF_FREQUENCY_3] = (f & 0xFF);
		CHK_ERROR(update_regs(state, RF_FREQUENCY_1, RF_FREQUENCY_3));

		state->Regs[MSM_1] = 0x41; /* Tune */
		state->Regs[MSM_2] = 0x01; /* Start MSM */
		CHK_ERROR(update_regs(state, MSM_1, MSM_2));
		state->Regs[MSM_2] = 0x00;

		while (true) {
			CHK_ERROR(read_reg(state, IRQ_STATUS, &IRQ));
			if ((IRQ & 0x80) != 0)
				break;
			Timeout -= 1;
			if (Timeout == 0) {
				status = -1;
				break;
			}
			usleep_range(10000, 12000);
		}
		CHK_ERROR(status);

		if (state->m_isMaster && StandardParams->m_LTO_STO_immune) {
			state->Regs[RF_AGC_1] &=  ~0x08;
			CHK_ERROR(update_reg(state, RF_AGC_1));

			msleep(50);

			state->Regs[RF_FILTER_1] &= ~0x10;  /* remove force */
			CHK_ERROR(update_reg(state, RF_FILTER_1));
		}

		/* Spur reduction */

		if (Frequency < 72000000)
			state->Regs[REFERENCE] |= 0x40;  /* Set digital clock */
		else if (Frequency < 104000000)
			state->Regs[REFERENCE] &= ~0x40; /*Clear digital clock*/
		else if (Frequency < 120000000)
			state->Regs[REFERENCE] |= 0x40;  /* Set digital clock */
		else {
			if (fDelta <= 8000000) {
				/* Clear or set digital clock */
				if (fRatio & 1)
					state->Regs[REFERENCE] &= ~0x40;
				else
					state->Regs[REFERENCE] |= 0x40;
			} else {
				/* Set or clear digital clock */
				if (fRatio & 1)
					state->Regs[REFERENCE] |= 0x40;
				else
					state->Regs[REFERENCE] &= ~0x40;
			}

		}
		CHK_ERROR(update_reg(state, REFERENCE));

		if (StandardParams->m_AGC1_Freeze && state->m_bEnableFreeze) {
			u8 tmp;
			int AGC1GainMin = 0;
			int nSteps = 10;
			int Step  = 0;

			CHK_ERROR(read_reg(state, AGC1_2, &tmp));
			if ((tmp & 0x80) == 0) {
				state->Regs[AGC1_2] |= 0x80;    /* Loop off */
				CHK_ERROR(update_reg(state, AGC1_2));
				state->Regs[AGC1_2] |= 0x10;   /* Force gain */
				CHK_ERROR(update_reg(state, AGC1_2));
			}
			/* Adapt */
			if (state->Regs[AGC1_1] & 0x40) { /* AGC1_6_15dB set */
				AGC1GainMin = 6;
				nSteps = 4;
			}
			while (Step < nSteps) {
				int Down = 0;
				int Up = 0, i;
				u8 AGC1_Gain;

				Step = Step + 1;

				for (i = 0; i < 40; i += 1) {
					CHK_ERROR(read_reg(state, AGC_DET_OUT,
							   &tmp));
					Up   += (tmp & 0x02) ?  1 : -4;
					Down += (tmp & 0x01) ? 14 : -1;
					usleep_range(1000, 2000);
				}
				CHK_ERROR(status);
				AGC1_Gain = (state->Regs[AGC1_2] & 0x0F);
				if (Up >= 15 && AGC1_Gain != 9) {
					state->Regs[AGC1_2] =
						(state->Regs[AGC1_2] & ~0x0F) |
						(AGC1_Gain + 1);
					CHK_ERROR(update_reg(state, AGC1_2));
				} else if (Down >= 10 &&
					   AGC1_Gain != AGC1GainMin) {
					state->Regs[AGC1_2] =
						(state->Regs[AGC1_2] & ~0x0F) |
						(AGC1_Gain - 1);
					CHK_ERROR(update_reg(state, AGC1_2));
				} else
					Step = nSteps;
			}
		} else {
			state->Regs[AGC1_2] &= ~0x10;       /* unforce gain */
			CHK_ERROR(update_reg(state, AGC1_2));
			state->Regs[AGC1_2] &= ~0x80;         /* Loop on */
			CHK_ERROR(update_reg(state, AGC1_2));
		}

		state->m_Standard = Standard;
		state->m_Frequency = Frequency;

		if (state->m_bPowerMeasurement)
			PowerMeasurement(state, &state->m_LastPowerLevel);
	} while (0);

	return status;
}

static int sleep(struct dvb_frontend *fe)
{
	struct tda_state *state = fe->tuner_priv;

	Standby(state);
	write_reg(state, THERMO_2, 0x01);
	read_reg1(state, THERMO_1);
	write_reg(state, THERMO_2, 0x00);
	printk("sleep: temp = %u\n", state->Regs[THERMO_1]);
	return 0;
}

static int init(struct dvb_frontend *fe)
{
	/* struct tda_state *state = fe->tuner_priv; */
	return 0;
}

static int release(struct dvb_frontend *fe)
{
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
	return 0;
}

static int set_params(struct dvb_frontend *fe)
{
	struct tda_state *state = fe->tuner_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	int status = 0;
	int Standard;
	u32 bw;

	bw = (p->bandwidth_hz + 999999) / 1000000;
	state->m_Frequency = p->frequency;
	/*pr_info("tuner bw=%u  freq=%u\n", bw, state->m_Frequency);*/
	if (p->delivery_system == SYS_DVBT ||
	    p->delivery_system == SYS_DVBT2 ||
	    p->delivery_system == SYS_ISDBT ||
	    p->delivery_system == SYS_DVBC2) {
		switch (bw) {
		case 6:
			Standard = HF_DVBT_6MHZ;
			break;
		case 7:
			Standard = HF_DVBT_7MHZ;
			break;
		default:
		case 8:
			Standard = HF_DVBT_8MHZ;
			break;
		}
	} else if (p->delivery_system == SYS_DVBC_ANNEX_A) {
		switch (bw) {
		case 6:
			Standard = HF_DVBC_6MHZ;
			break;
		case 7:
			Standard = HF_DVBC_7MHZ;
			break;
		default:
		case 8:
			Standard = HF_DVBC_8MHZ;
			break;
		}
	} else
		return -EINVAL;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	SetFrequency(state, state->m_Frequency, Standard);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	return status;
}

static int get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct tda_state *state = fe->tuner_priv;

	*frequency = state->IF;
	return 0;
}

static int get_rf_strength(struct dvb_frontend *fe, u16 *st)
{
	struct tda_state *state = fe->tuner_priv;

	*st = state->m_LastPowerLevel;
	return 0;
}

static int get_if(struct dvb_frontend *fe, u32 *frequency)
{
	struct tda_state *state = fe->tuner_priv;

	state->IF = 0;
	if (state->m_Standard < HF_DVBT_6MHZ ||
	    state->m_Standard > HF_DVBC_8MHZ)
		return 0;
	state->IF = m_StandardTable[state->m_Standard -
				    HF_DVBT_6MHZ].m_IFFrequency;
	*frequency = state->IF;
	return 0;
}

static int get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	/* struct tda_state *state = fe->tuner_priv; */
	/* *bandwidth = priv->bandwidth; */
	return 0;
}


static struct dvb_tuner_ops tuner_ops = {
	.info = {
		.name = "NXP TDA18212",
		.frequency_min  =  47125000,
		.frequency_max  = 865000000,
		.frequency_step =     62500
	},
	.init              = init,
	.sleep             = sleep,
	.set_params        = set_params,
	.release           = release,
	.get_frequency     = get_frequency,
	.get_if_frequency  = get_if,
	.get_bandwidth     = get_bandwidth,
	.get_rf_strength   = get_rf_strength,
};

struct dvb_frontend *tda18212dd_attach(struct dvb_frontend *fe,
				       struct i2c_adapter *i2c, u8 adr)
{
	struct tda_state *state;
	int stat;

	state = kzalloc(sizeof(struct tda_state), GFP_KERNEL);
	if (!state)
		return NULL;
	state->adr = adr;
	state->i2c = i2c;
	memcpy(&fe->ops.tuner_ops, &tuner_ops, sizeof(struct dvb_tuner_ops));
	init_state(state);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	stat = attach_init(state);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
	if (stat < 0) {
		kfree(state);
		return 0;
	}
	fe->tuner_priv = state;
	return fe;
}
EXPORT_SYMBOL_GPL(tda18212dd_attach);

MODULE_DESCRIPTION("TDA18212 driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL");

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
