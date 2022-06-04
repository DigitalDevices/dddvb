/*
 * ddbridge-modulator.c: Digital Devices modulator cards
 *
 * Copyright (C) 2010-2018 Digital Devices GmbH
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         Ralph Metzler <rjkm@metzlerbros.de>
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
 * along with this program; if not, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */

#include "ddbridge.h"
#include "ddbridge-io.h"
#include "ddbridge-ioctl.h"

#ifdef KERNEL_DVB_CORE
#include "../include/linux/dvb/mod.h"
#else
#include <linux/dvb/mod.h>
#endif
#include <linux/gcd.h>

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

inline s64 ConvertPCR(s64 a)
{
	s32 ext;
	s64 b;

	b = div_s64_rem(a, 300 << 22, &ext);

	return (b << 31) | ext;

}

inline s64 NegConvertPCR(s64 a)
{
	s32 ext;
	s64 b;

	b = -div_s64_rem(a, 300 << 22, &ext);

	if (ext != 0) {
		ext = (300 << 22) - ext;
		b -= 1;
	}
	return (b << 31) | ext;
}

inline s64 RoundPCR(s64 a)
{
	s64 b = a + (HW_LSB_MASK>>1);

	return b & ~(HW_LSB_MASK - 1);
}

inline s64 RoundPCRUp(s64 a)
{
	s64 b = a + (HW_LSB_MASK - 1);

	return b & ~(HW_LSB_MASK - 1);
}

inline s64 RoundPCRDown(s64 a)
{
	return a & ~(HW_LSB_MASK - 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int mod_SendChannelCommand(struct ddb *dev, u32 Channel, u32 Command)
{
	u32 ControlReg = ddbreadl(dev, CHANNEL_CONTROL(Channel));

	ControlReg = (ControlReg & ~CHANNEL_CONTROL_CMD_MASK)|Command;
	ddbwritel(dev, ControlReg, CHANNEL_CONTROL(Channel));
	while (1) {
		ControlReg = ddbreadl(dev, CHANNEL_CONTROL(Channel));
		if (ControlReg == 0xFFFFFFFF)
			return -EIO;
		if ((ControlReg & CHANNEL_CONTROL_CMD_STATUS) == 0)
			break;
	}
	if (ControlReg & CHANNEL_CONTROL_ERROR_CMD)
		return -EINVAL;
	return 0;
}

static int mod_busy(struct ddb *dev, int chan)
{
	u32 creg;

	while (1) {
		creg = ddbreadl(dev, CHANNEL_CONTROL(chan));
		if (creg == 0xffffffff)
			return -EFAULT;
		if ((creg & CHANNEL_CONTROL_BUSY) == 0)
			break;
	}
	return 0;
}

void ddbridge_mod_output_stop(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;
	struct ddb_mod *mod = &dev->mod[output->nr];

	mod->State = CM_IDLE;
	mod->Control &= 0xfffffff0;
	if (dev->link[0].info->version == 2)
		mod_SendChannelCommand(dev, output->nr,
				       CHANNEL_CONTROL_CMD_FREE);
	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(output->nr));
#if 0
	udelay(10);
	ddbwritel(dev, CHANNEL_CONTROL_RESET, CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));
#endif
	mod_busy(dev, output->nr);
	dev_info(dev->dev, "mod_output_stop %d.%d\n", dev->nr, output->nr);
}

static void mod_set_incs(struct ddb_output *output)
{
	s64 pcr;
	struct ddb *dev = output->port->dev;
	struct ddb_mod *mod = &dev->mod[output->nr];

	pcr = ConvertPCR(mod->PCRIncrement);
	ddbwritel(dev,	pcr & 0xffffffff,
		  CHANNEL_PCR_ADJUST_OUTL(output->nr));
	ddbwritel(dev,	(pcr >> 32) & 0xffffffff,
		  CHANNEL_PCR_ADJUST_OUTH(output->nr));
	mod_busy(dev, output->nr);

	pcr = NegConvertPCR(mod->PCRDecrement);
	ddbwritel(dev,	pcr & 0xffffffff,
		  CHANNEL_PCR_ADJUST_INL(output->nr));
	ddbwritel(dev,	(pcr >> 32) & 0xffffffff,
		  CHANNEL_PCR_ADJUST_INH(output->nr));
	mod_busy(dev, output->nr);

}

static void mod_set_rateinc(struct ddb *dev, u32 chan)
{
	ddbwritel(dev, dev->mod[chan].rate_inc, CHANNEL_RATE_INCR(chan));
	mod_busy(dev, chan);
}

static void mod_calc_rateinc(struct ddb_mod *mod)
{
	u32 ri;

	if (mod->ibitrate != 0) {
		u64 d = mod->obitrate - mod->ibitrate;

		d = div64_u64(d, mod->obitrate >> 24);
		if (d > 0xfffffe)
			ri = 0xfffffe;
		else
			ri = d;
	} else
		ri = 0;
	mod->rate_inc = ri;
	dev_info(mod->port->dev->dev,
		 "ibr=%llu, obr=%llu, ri=0x%06x\n",
		 mod->ibitrate >> 32, mod->obitrate >> 32, ri);
}

static int mod_calc_obitrate(struct ddb_mod *mod)
{
	u64 ofac;

	ofac = (((u64) mod->symbolrate) << 32) * 188;
	ofac = div_u64(ofac, 204);
	mod->obitrate = ofac * (mod->modulation + 3);
	return 0;
}

static int mod_set_symbolrate(struct ddb_mod *mod, u32 srate)
{
	struct ddb *dev = mod->port->dev;

	if (srate < 1000000)
		return -EINVAL;
	if (dev->link[0].info->version < 2) {
		if (srate != 6900000)
			return -EINVAL;
	} else {
		if (srate > 7100000)
			return -EINVAL;
	}
	mod->symbolrate = srate;
	mod_calc_obitrate(mod);
	return 0;
}

static u32 qamtab[6] = { 0x000, 0x600, 0x601, 0x602, 0x903, 0x604 };

static int mod_set_modulation(struct ddb_mod *mod,
			      enum fe_modulation modulation)
{
	struct ddb *dev = mod->port->dev;

	if (modulation > QAM_256 || modulation < QAM_16)
		return -EINVAL;
	mod->modulation = modulation;
	if (dev->link[0].info->version < 2)
		ddbwritel(dev, qamtab[modulation],
			  CHANNEL_SETTINGS(mod->port->nr));
	mod_calc_obitrate(mod);
	return 0;
}

static int mod_set_frequency(struct ddb_mod *mod, u32 frequency)
{
	u32 freq = frequency / 1000000;

	if (frequency % 1000000)
		return -EINVAL;
	if ((freq - 114) % 8)
		return -EINVAL;
	if ((freq < 114) || (freq > 874))
		return -EINVAL;
	mod->frequency = frequency;
	return 0;
}

static int mod_set_ibitrate(struct ddb_mod *mod, u64 ibitrate)
{
	if (ibitrate > mod->obitrate)
		return -EINVAL;
	mod->ibitrate = ibitrate;
	mod_calc_rateinc(mod);
	return 0;
}

/* Calculating KF, LF from Symbolrate
 *
 *  Symbolrate is usually calculated as (M/N) * 10.24 MS/s
 *
 *   Common Values for M,N
 *     J.83 Annex A,
 *     Euro Docsis  6.952 MS/s : M =   869, N =  1280
 *                  6.900 MS/s : M =   345, N =   512
 *                  6.875 MS/s : M =  1375, N =  2048
 *                  6.111 MS/s : M =  6111, N = 10240
 *     J.83 Annex B **
 *      QAM64       5.056941   : M =   401, N =   812
 *      QAM256      5.360537   : M =    78, N =   149
 *     J.83 Annex C **
 *                  5.309734   : M =  1889, N =  3643
 *
 *    For the present hardware
 *      KF' = 256 * M
 *      LF' = 225 * N
 *       or
 *      KF' = Symbolrate in Hz
 *      LF' = 9000000
 *
 *      KF  = KF' / gcd(KF',LF')
 *      LF  = LF' / gcd(KF',LF')
 * Note: LF must not be a power of 2.
 *       Maximum value for KF,LF = 13421727 ( 0x7FFFFFF )
 *    ** using these M,N values will result in a small err (<5ppm)
 *       calculating KF,LF directly gives the exact normative result
 *       but with rather large KF,LF values
 */

int ddbridge_mod_output_start(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;
	u32 Channel = output->nr;
	struct ddb_mod *mod = &dev->mod[output->nr];
	u32 Symbolrate = mod->symbolrate;

	if (dev->link[0].info->version < 16)
		mod_calc_rateinc(mod);

	mod->LastInPacketCount = 0;
	mod->LastOutPacketCount = 0;
	mod->InOverflowPacketCount = 0;
	mod->OutOverflowPacketCount = 0;
	mod->LastInPackets = 0;
	mod->LastOutPackets = 0;
	mod->LastPCRAdjust = 0;
	mod->PCRRunningCorr = 0;
	/* we interrupt every 0x80000=524288 packets */
	mod->MinInputPackets = 524288 / 2;
	mod->PCRIncrement = 0;
	mod->PCRDecrement = 0;

	mod->State = CM_STARTUP;
	mod->StateCounter = CM_STARTUP_DELAY;

	if (dev->link[0].info->version >= 16)
		mod->Control = 0xfffffff0 &
			ddbreadl(dev, CHANNEL_CONTROL(output->nr));
	else
		mod->Control = 0;
	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, mod->Control | CHANNEL_CONTROL_RESET,
		  CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(output->nr));
	switch (dev->link[0].info->version) {
	case 2:
	{
		u32 Output = (mod->frequency - 114000000) / 8000000;
		u32 KF = Symbolrate;
		u32 LF = 9000000UL;
		u32 d = gcd(KF, LF);
		u32 checkLF;
#if 0
		if (dev->link[0].ids.revision == 1) {
			mod->Control |= CHANNEL_CONTROL_ENABLE_DVB;
				return -EINVAL;
			break;
		}
#endif
		ddbwritel(dev, mod->modulation - 1, CHANNEL_SETTINGS(Channel));
		ddbwritel(dev, Output, CHANNEL_SETTINGS2(Channel));

		KF = KF / d;
		LF = LF / d;

		while ((KF > KFLF_MAX) || (LF > KFLF_MAX)) {
			KF >>= 1;
			LF >>= 1;
		}

		checkLF = LF;
		while ((checkLF & 1) == 0)
			checkLF >>= 1;
		if (checkLF <= 1)
			return -EINVAL;

		dev_info(dev->dev, "KF=%u LF=%u Output=%u mod=%u\n",
			 KF, LF, Output, mod->modulation);
		ddbwritel(dev, KF, CHANNEL_KF(Channel));
		ddbwritel(dev, LF, CHANNEL_LF(Channel));

		if (mod_SendChannelCommand(dev, Channel,
					   CHANNEL_CONTROL_CMD_SETUP))
			return -EINVAL;
		mod->Control |= CHANNEL_CONTROL_ENABLE_DVB;
		break;
	}
	case 0:
	case 1:
		/* QAM: 600 601 602 903 604 = 16 32 64 128 256 */
		/* ddbwritel(dev, 0x604, CHANNEL_SETTINGS(output->nr)); */
		ddbwritel(dev, qamtab[mod->modulation],
			  CHANNEL_SETTINGS(output->nr));
		mod->Control |= (CHANNEL_CONTROL_ENABLE_IQ |
				 CHANNEL_CONTROL_ENABLE_DVB);
		break;
	default:
		mod->Control |= (CHANNEL_CONTROL_ENABLE_IQ |
				 CHANNEL_CONTROL_ENABLE_DVB);
		break;
	}
	if (dev->link[0].info->version < 16) {
		mod_set_rateinc(dev, output->nr);
		mod_set_incs(output);
	}
	mod->Control |= CHANNEL_CONTROL_ENABLE_SOURCE;

	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(output->nr));
	if (dev->link[0].info->version == 2)
		if (mod_SendChannelCommand(dev, Channel,
					   CHANNEL_CONTROL_CMD_UNMUTE))
			return -EINVAL;
	dev_info(dev->dev, "mod_output_start %d.%d ctrl=%08x\n",
		 dev->nr, output->nr, mod->Control);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int mod_write_max2871(struct ddb *dev, u32 val)
{
	ddbwritel(dev, val, MAX2871_OUTDATA);
	ddbwritel(dev, MAX2871_CONTROL_CE | MAX2871_CONTROL_WRITE,
		  MAX2871_CONTROL);
	while (1) {
		u32 ControlReg = ddbreadl(dev, MAX2871_CONTROL);

		if (ControlReg == 0xFFFFFFFF)
			return -EIO;
		if ((ControlReg & MAX2871_CONTROL_WRITE) == 0)
			break;
	}
	return 0;
}

static u32 max2871_fsm[6] = {
	0x00730040, 0x600080A1, 0x510061C2, 0x010000CB, 0x6199003C, 0x60440005,
};

static u32 max2871_sdr[6] = {
	0x007A8098, 0x600080C9, 0x510061C2, 0x010000CB, 0x6199003C, 0x60440005
};

static void lostlock_handler(void *data)
{
	struct ddb *dev = (struct ddb *)data;

	dev_err(dev->dev, "Lost PLL lock!\n");
	ddbwritel(dev, 0, RF_VGA);
	udelay(10);
	ddbwritel(dev, 31, RF_ATTENUATOR);
}

static int mod_setup_max2871(struct ddb *dev, u32 *reg)
{
	int status = 0;
	int i, j;
	u32 val;

	ddbwritel(dev, MAX2871_CONTROL_CE, MAX2871_CONTROL);
	msleep(30);
	for (i = 0; i < 2; i++) {
		for (j = 5; j >= 0; j--) {
			val = reg[j];

			if (j == 4)
				val &= 0xFFFFFEDF;
			status = mod_write_max2871(dev, val);
			if (status)
				break;
			msleep(30);
		}
	}
	if (status == 0) {
		u32 ControlReg;

		if (reg[3] & (1 << 24))
			msleep(100);
		ControlReg = ddbreadl(dev, MAX2871_CONTROL);

		if ((ControlReg & MAX2871_CONTROL_LOCK) == 0)
			status = -EIO;
		status = mod_write_max2871(dev, reg[4]);
		ddbwritel(dev,
			  MAX2871_CONTROL_CE | MAX2871_CONTROL_LOSTLOCK,
			  MAX2871_CONTROL);
		if (dev->link[0].info->lostlock_irq)
			ddb_irq_set(dev, 0, dev->link[0].info->lostlock_irq,
				    lostlock_handler, dev);
	}
	return status;
}

static int mod_fsm_setup(struct ddb *dev, u32 MaxUsedChannels)
{
	int status = 0;
	u32 Capacity;
	u32 tmp = ddbreadl(dev, FSM_STATUS);

	status = mod_setup_max2871(dev, max2871_fsm);
	if (status)
		return status;
	ddbwritel(dev, FSM_CMD_RESET, FSM_CONTROL);
	msleep(20);

	tmp = ddbreadl(dev, FSM_STATUS);
	if ((tmp & FSM_STATUS_READY) == 0)
		return -1;

	Capacity = ddbreadl(dev, FSM_CAPACITY);
	if (((tmp & FSM_STATUS_QAMREADY) != 0) &&
	    ((Capacity & FSM_CAPACITY_INUSE) != 0))
		return -EBUSY;

	ddbwritel(dev, FSM_CMD_SETUP, FSM_CONTROL);
	msleep(20);
	tmp = ddbreadl(dev, FSM_STATUS);

	if ((tmp & FSM_STATUS_QAMREADY) == 0)
		return -1;

	if (MaxUsedChannels == 0)
		MaxUsedChannels = (Capacity & FSM_CAPACITY_CUR) >> 16;

	if (MaxUsedChannels <= 1)
		ddbwritel(dev, FSM_GAIN_N1, FSM_GAIN);
	else if (MaxUsedChannels <= 2)
		ddbwritel(dev, FSM_GAIN_N2, FSM_GAIN);
	else if (MaxUsedChannels <= 4)
		ddbwritel(dev, FSM_GAIN_N4, FSM_GAIN);
	else if (MaxUsedChannels <= 8)
		ddbwritel(dev, FSM_GAIN_N8, FSM_GAIN);
	else if (MaxUsedChannels <= 16)
		ddbwritel(dev, FSM_GAIN_N16, FSM_GAIN);
	else if (MaxUsedChannels <= 24)
		ddbwritel(dev, FSM_GAIN_N24, FSM_GAIN);
	else
		ddbwritel(dev, FSM_GAIN_N96, FSM_GAIN);

	ddbwritel(dev, FSM_CONTROL_ENABLE, FSM_CONTROL);
	return status;
}

static int mod_set_vga(struct ddb *dev, u32 gain)
{
	if (gain > 255)
		return -EINVAL;
	ddbwritel(dev, gain, RF_VGA);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void mod_write_dac_register(struct ddb *dev, u8 Index, u8 Value)
{
	u32 RegValue = 0;

	ddbwritel(dev, Value, DAC_WRITE_DATA);
	ddbwritel(dev, DAC_CONTROL_STARTIO | Index, DAC_CONTROL);
	do {
		RegValue = ddbreadl(dev, DAC_CONTROL);
	} while ((RegValue & DAC_CONTROL_STARTIO) != 0);
}

static void mod_write_dac_register2(struct ddb *dev, u8 Index, u16 Value)
{
	u32 RegValue = 0;

	ddbwritel(dev, Value, DAC_WRITE_DATA);
	ddbwritel(dev, DAC_CONTROL_STARTIO | 0x20 | Index, DAC_CONTROL);
	do {
		RegValue = ddbreadl(dev, DAC_CONTROL);
	} while ((RegValue & DAC_CONTROL_STARTIO) != 0);
}

static int mod_read_dac_register(struct ddb *dev, u8 Index, u8 *pValue)
{
	u32 RegValue = 0;

	ddbwritel(dev, DAC_CONTROL_STARTIO | 0x80 | Index, DAC_CONTROL);
	do {
		RegValue = ddbreadl(dev, DAC_CONTROL);
	} while ((RegValue & DAC_CONTROL_STARTIO) != 0);

	RegValue = ddbreadl(dev, DAC_READ_DATA);
	*pValue = (u8) RegValue;
	return 0;
}

static void mod_set_up_converter_vco1(struct ddb *dev, u32 Value)
{
	u32 RegValue = 0;

	/* Extra delay before writing N divider */
	if ((Value & 0x03) == 0x02)
		msleep(50);
	do {
		RegValue = ddbreadl(dev, VCO1_CONTROL);
	} while ((RegValue & VCO1_CONTROL_WRITE) != 0);

	if ((RegValue & VCO1_CONTROL_CE) == 0) {
		RegValue |= VCO1_CONTROL_CE;
		ddbwritel(dev, RegValue, VCO1_CONTROL);
		msleep(20);
	}

	ddbwritel(dev, Value, VCO1_DATA);
	ddbwritel(dev, RegValue | VCO1_CONTROL_WRITE, VCO1_CONTROL);
}

static void mod_set_up_converter_vco2(struct ddb *dev, u32 Value)
{
	u32 RegValue = 0;

	/* Extra delay before writing N divider */
	if ((Value & 0x03) == 0x02)
		msleep(50);
	do {
		RegValue = ddbreadl(dev, VCO2_CONTROL);
	} while ((RegValue & VCO2_CONTROL_WRITE) != 0);

	if ((RegValue & VCO2_CONTROL_CE) == 0) {
		RegValue |= VCO2_CONTROL_CE;
		ddbwritel(dev, RegValue, VCO2_CONTROL);
		msleep(20);
	}

	ddbwritel(dev, Value, VCO2_DATA);
	ddbwritel(dev, RegValue | VCO2_CONTROL_WRITE, VCO2_CONTROL);
}

static void mod_set_down_converter_vco(struct ddb *dev, u32 Value)
{
	u32 RegValue = 0;

	do {
		RegValue = ddbreadl(dev, VCO3_CONTROL);
	} while ((RegValue & VCO3_CONTROL_WRITE) != 0);

	if ((RegValue & VCO3_CONTROL_CE) == 0) {
		RegValue |= VCO3_CONTROL_CE;
		ddbwritel(dev, RegValue, VCO3_CONTROL);
		msleep(20);
	}
	ddbwritel(dev, Value, VCO3_DATA);
	ddbwritel(dev, RegValue | VCO3_CONTROL_WRITE, VCO3_CONTROL);
}

static int mod_set_attenuator(struct ddb *dev, u32 Value)
{
	if (Value > 31)
		return -EINVAL;
	if (dev->link[0].ids.revision == 1) {
		struct ddb_link *link = &dev->link[0];
		struct mci_result res;
		struct mci_command cmd = {
			.mod_command = MOD_SETUP_OUTPUT,
			.mod_channel = 0,
			.mod_stream = 0,
			.mod_setup_output = {
				.connector = MOD_CONNECTOR_F,
				.num_channels = 24,
				.unit = MOD_UNIT_DBUV,
				.channel_power = 9000 - Value * 100,
			},
		};
		if (!link->mci_ok) {
			return -EFAULT;
		}
		return ddb_mci_cmd_link(link, &cmd, &res);
	} else
		ddbwritel(dev, Value, RF_ATTENUATOR);
	return 0;
}

static int mod_set_sdr_attenuator(struct ddb *dev, u32 value)
{
	u32 control;
	
	if (value > 31)
		return -EINVAL;
	control = ddbreadl(dev, SDR_CONTROL);
	if (control & 0x01000000) {
		ddbwritel(dev, 0x03, SDR_CONTROL);
	} else {
		ddbwritel(dev, value, RF_ATTENUATOR);
	}
	return 0;
}

static int mod_set_sdr_gain(struct ddb *dev, u32 gain)
{
	u32 control = ddbreadl(dev, SDR_CONTROL);

	if (control & 0x01000000) {
		if (gain > 511)
			return -EINVAL;
		ddbwritel(dev, 0x03, SDR_CONTROL);
		ddbwritel(dev, gain, SDR_GAIN_F);
		if (gain > 255)
			gain = 255;
		ddbwritel(dev, gain, SDR_GAIN_SMA);
	} else {
		if (gain > 255)
			return -EINVAL;
		ddbwritel(dev, gain, SDR_GAIN_F);
	}
	return 0;
}

static void mod_si598_readreg(struct ddb *dev, u8 index, u8 *val)
{
	ddbwritel(dev, index, CLOCKGEN_INDEX);
	ddbwritel(dev, 1, CLOCKGEN_CONTROL);
	usleep_range(5000, 6000);
	*val = ddbreadl(dev, CLOCKGEN_READDATA);
}

static void mod_si598_writereg(struct ddb *dev, u8 index, u8 val)
{
	ddbwritel(dev, index, CLOCKGEN_INDEX);
	ddbwritel(dev, val, CLOCKGEN_WRITEDATA);
	ddbwritel(dev, 3, CLOCKGEN_CONTROL);
	usleep_range(5000, 6000);
}

static int mod_set_si598(struct ddb *dev, u32 freq)
{
	u8 Data[6];
	u64 fDCO = 0;
	u64 RFreq = 0;
	u32 fOut = 10000000;
	u64 m_fXtal = 0;
	u32 N = 0;
	u64 HSDiv = 0;

	u32 fxtal;
	u64 MinDiv, MaxDiv, Div;
	u64 RF;

	if (freq < 10000000 || freq > 525000000)
		return -EINVAL;
	mod_si598_writereg(dev, 137, 0x10);

	if (m_fXtal == 0) {
		mod_si598_writereg(dev, 135, 0x01);
		mod_si598_readreg(dev, 7, &Data[0]);
		mod_si598_readreg(dev, 8, &Data[1]);
		mod_si598_readreg(dev, 9, &Data[2]);
		mod_si598_readreg(dev, 10, &Data[3]);
		mod_si598_readreg(dev, 11, &Data[4]);
		mod_si598_readreg(dev, 12, &Data[5]);

		dev_info(dev->dev, "Data = %02x %02x %02x %02x %02x %02x\n",
			 Data[0], Data[1], Data[2], Data[3], Data[4], Data[5]);
		RFreq = (((u64)Data[1] & 0x3F) << 32) | ((u64)Data[2] << 24) |
			((u64)Data[3] << 16) | ((u64)Data[4] << 8) |
			((u64)Data[5]);
		if (RFreq == 0)
			return -EINVAL;
		HSDiv = ((Data[0] & 0xE0) >> 5) + 4;
		if (HSDiv == 8 || HSDiv == 10)
			return -EINVAL;
		N = (((u32)(Data[0] & 0x1F) << 2) |
		     ((u32)(Data[1] & 0xE0) >> 6)) + 1;
		fDCO = fOut * (u64)(HSDiv * N);
		m_fXtal = fDCO << 28;
		dev_info(dev->dev, "fxtal %016llx  rfreq %016llx\n",
			 m_fXtal, RFreq);

		m_fXtal += RFreq >> 1;
		m_fXtal = div64_u64(m_fXtal, RFreq);

		dev_info(dev->dev, "fOut = %d fXtal = %d fDCO = %d HDIV = %2d, N = %3d\n",
			 (u32) fOut, (u32) m_fXtal, (u32) fDCO, (u32) HSDiv, N);
	}

	fOut = freq;
	MinDiv = 4850000000ULL; do_div(MinDiv, freq); MinDiv += 1;
	MaxDiv = 5670000000ULL; do_div(MaxDiv, freq);
	Div    = 5260000000ULL; do_div(Div, freq);

	if (Div < MinDiv)
		Div = Div + 1;
	dev_info(dev->dev, "fOut = %u MinDiv = %llu MaxDiv = %llu StartDiv = %llu\n",
		 fOut, MinDiv, MaxDiv, Div);

	if (Div <= 11) {
		N = 1;
		HSDiv = Div;
	} else {
		int retry = 100;

		while (retry > 0) {
			N = 0;
			HSDiv = Div;
			while ((HSDiv > 11) /*|| ((HSDiv * N) != Div)*/) {
				N = N + 2;
				HSDiv = Div;
				do_div(HSDiv, N);
				if (N > 128)
					break;
			}
			dev_info(dev->dev, "%3d: %llu %llu %llu %u\n",
				 retry, Div, HSDiv * N, HSDiv, N);
			if (HSDiv * N < MinDiv)
				Div = Div + 2;
			else if (HSDiv * N > MaxDiv)
				Div = Div - 2;
			else
				break;
			retry = retry - 1;
		}
		if (retry == 0) {
			dev_err(dev->dev, "FAIL\n");
			return -EINVAL;
		}
	}

	if (HSDiv == 8 || HSDiv == 10)	{
		HSDiv = HSDiv >> 1;
		N = N * 2;
	}

	if (HSDiv < 4)
		return -EINVAL;


	fDCO = (u64)fOut * (u64)N * (u64)HSDiv;
	dev_info(dev->dev, "fdco %16llx\n", fDCO);
	RFreq = fDCO<<28;
	dev_info(dev->dev, "%16llx %16llx\n", fDCO, RFreq);

	fxtal = m_fXtal;
	do_div(RFreq, fxtal);
	dev_info(dev->dev, "%16llx %d\n", RFreq, fxtal);
	RF = RFreq;

	dev_info(dev->dev, "fOut = %u fXtal = %llu fDCO = %llu HSDIV = %llu, N = %u, RFreq = %llu\n",
		 fOut, m_fXtal, fDCO, HSDiv, N, RFreq);

	Data[0] = (u8)(((HSDiv - 4) << 5) | ((N - 1) >> 2));
	Data[1] = (u8)((((N - 1) & 0x03) << 6) | ((RF >> 32) & 0x3F));
	Data[2] = (u8)((RF >> 24) & 0xFF);
	Data[3] = (u8)((RF >> 16) & 0xFF);
	Data[4] = (u8)((RF >>  8) & 0xFF);
	Data[5] = (u8)((RF)       & 0xFF);

	dev_info(dev->dev, "Data = %02x %02x %02x %02x %02x %02x\n",
		 Data[0], Data[1], Data[2], Data[3], Data[4], Data[5]);
	mod_si598_writereg(dev, 7, Data[0]);
	mod_si598_writereg(dev, 8, Data[1]);
	mod_si598_writereg(dev, 9, Data[2]);
	mod_si598_writereg(dev, 10, Data[3]);
	mod_si598_writereg(dev, 11, Data[4]);
	mod_si598_writereg(dev, 12, Data[5]);

	mod_si598_writereg(dev, 137, 0x00);
	mod_si598_writereg(dev, 135, 0x40);
	return 0;
}


static void mod_bypass_equalizer(struct ddb *dev, int bypass)
{
	u32  RegValue;

	RegValue = ddbreadl(dev, IQOUTPUT_CONTROL);
	RegValue &= ~IQOUTPUT_CONTROL_BYPASS_EQUALIZER;
	RegValue |= (bypass ? IQOUTPUT_CONTROL_BYPASS_EQUALIZER : 0x00);
	ddbwritel(dev, RegValue, IQOUTPUT_CONTROL);
}

static int mod_set_equalizer(struct ddb *dev, u32 Num, s16 *cTable)
{
	u32 i, adr = IQOUTPUT_EQUALIZER_0;

	if (Num > 11)
		return -EINVAL;

	for (i = 0; i < 11 - Num; i += 1) {
		ddbwritel(dev, 0, adr);
		adr += 4;
	}
	for (i = 0; i < Num; i += 1) {
		ddbwritel(dev, (u32) cTable[i], adr);
		adr += 4;
	}
	return 0;
}

#if 0
static void mod_peak(struct ddb *dev, u32 Time, s16 *pIPeak, s16 *pQPeak)
{
	u32 val;

	val = ddbreadl(dev, IQOUTPUT_CONTROL);
	val &= ~(IQOUTPUT_CONTROL_ENABLE_PEAK | IQOUTPUT_CONTROL_RESET_PEAK);
	ddbwritel(dev, val, IQOUTPUT_CONTROL);
	ddbwritel(dev, val | IQOUTPUT_CONTROL_RESET_PEAK, IQOUTPUT_CONTROL);
	msleep(20);
	ddbwritel(dev, val, IQOUTPUT_CONTROL);
	ddbwritel(dev, val | IQOUTPUT_CONTROL_ENABLE_PEAK, IQOUTPUT_CONTROL);
	msleep(Time);
	ddbwritel(dev, val, IQOUTPUT_CONTROL);
	val = ddbreadl(dev, IQOUTPUT_PEAK_DETECTOR);

	*pIPeak = val & 0xffff;
	*pQPeak = (val >> 16) & 0xffff;
}
#endif

static int mod_init_dac_input(struct ddb *dev)
{
	u8 Set = 0;
	u8 Hld = 0;
	u8 Sample = 0;

	u8 Seek = 0;
	u8 ReadSeek = 0;

	u8 SetTable[32];
	u8 HldTable[32];
	u8 SeekTable[32];

	u8 Sample1 = 0xFF;
	u8 Sample2 = 0xFF;

	u8 SelectSample = 0xFF;
	u8 DiffMin = 0xFF;

	for (Sample = 0; Sample < 32; Sample++) {
		Set = 0;
		Hld = 0;

		mod_write_dac_register(dev, 0x04, Set << 4 | Hld);
		mod_write_dac_register(dev, 0x05, Sample);
		mod_read_dac_register(dev, 0x06, &ReadSeek);
		Seek = ReadSeek & 0x01;
		SeekTable[Sample] = Seek;

		HldTable[Sample] = 15;

		for (Hld = 1; Hld < 16; Hld += 1) {
			mod_write_dac_register(dev, 0x04, Set << 4 | Hld);
			mod_read_dac_register(dev, 0x06, &ReadSeek);

			if ((ReadSeek & 0x01) != Seek) {
				HldTable[Sample] = Hld;
				break;
			}
		}

		Hld = 0;
		SetTable[Sample] = 15;
		for (Set = 1; Set < 16; Set += 1) {
			mod_write_dac_register(dev, 0x04, Set << 4 | Hld);
			mod_read_dac_register(dev, 0x06, &ReadSeek);

			if ((ReadSeek & 0x01) != Seek) {
				SetTable[Sample] = Set;
				break;
			}
		}
	}

	Seek = 1;
	for (Sample = 0; Sample < 32; Sample += 1) {
		/* printk(" %2d: %d %2d %2d\n",
		 * Sample, SeekTable[Sample], SetTable[Sample],
		 * HldTable[Sample]);
		 */

		if (Sample1 == 0xFF && SeekTable[Sample] == 1 && Seek == 0)
			Sample1 = Sample;
		if (Sample1 != 0xFF && Sample2 == 0xFF &&
		    SeekTable[Sample] == 0 && Seek == 1)
			Sample2 = Sample;
		Seek = SeekTable[Sample];
	}

	if (Sample1 == 0xFF || Sample2 == 0xFF) {
		dev_err(dev->dev, "No valid window found\n");
		return -EINVAL;
	}

	dev_err(dev->dev, "Window = %d - %d\n", Sample1, Sample2);

	for (Sample = Sample1; Sample < Sample2; Sample += 1) {
		if (SetTable[Sample] < HldTable[Sample]) {
			if (HldTable[Sample] - SetTable[Sample] < DiffMin) {
				DiffMin = HldTable[Sample] - SetTable[Sample];
				SelectSample = Sample;
			}
		}
	}

	dev_info(dev->dev, "Select Sample %d\n", SelectSample);

	if (SelectSample == 0xFF) {
		dev_err(dev->dev, "No valid sample found\n");
		return -EINVAL;
	}

	if (HldTable[SelectSample] + SetTable[SelectSample] < 8) {
		dev_err(dev->dev, "Too high jitter\n");
		return -EINVAL;
	}

	mod_write_dac_register(dev, 0x04, 0x00);
	mod_write_dac_register(dev, 0x05, (SelectSample - 1) & 0x1F);
	mod_read_dac_register(dev, 0x06, &Seek);
	mod_write_dac_register(dev, 0x05, (SelectSample + 1) & 0x1F);
	mod_read_dac_register(dev, 0x06, &ReadSeek);
	Seek &= ReadSeek;

	mod_write_dac_register(dev, 0x05, SelectSample);
	mod_read_dac_register(dev, 0x06, &ReadSeek);
	Seek &= ReadSeek;
	if ((Seek & 0x01) == 0) {
		dev_err(dev->dev, "Insufficient timing margin\n");
		return -EINVAL;
	}
	dev_info(dev->dev, "Done\n");
	return 0;
}

static void mod_set_up1(struct ddb *dev, u32 Frequency, u32 Ref, u32 Ext)
{
	u32 RDiv = Ext / Ref;

	Frequency = Frequency / Ref;
	mod_set_up_converter_vco1(dev, 0x360001 | (RDiv << 2));
	mod_set_up_converter_vco1(dev, 0x0ff128);
	mod_set_up_converter_vco1(dev, 0x02 | (Frequency << 8));
}

static void mod_set_up2(struct ddb *dev, u32 Frequency, u32 Ref, u32 Ext)
{
	u32 Rdiv = Ext / Ref;
	u32 PreScale = 8;

	Frequency = Frequency / Ref;
	mod_set_up_converter_vco2(dev, 0x360001 | (Rdiv << 2));
	mod_set_up_converter_vco2(dev, 0x0fc128 |
				  (((PreScale - 8) / 8) << 22));
	mod_set_up_converter_vco2(dev, 0x02 | ((Frequency / PreScale) << 8)
				  | (Frequency & (PreScale - 1)) << 2);
}

static int mod_set_down(struct ddb *dev, u32 Frequency, u32 Ref, u32 Ext)
{
	u32 BandSelect = Ref * 8;
	u32 RefMul = 1;
	u32 RefDiv2 = 1;
	u32 RefDiv = Ext * RefMul / (Ref * RefDiv2);

	if (Frequency < 2200 || Frequency > 4000)
		return -EINVAL;

	Frequency = Frequency / Ref;

	mod_set_down_converter_vco(dev, 0x0080003C |
				   ((BandSelect & 0xFF) << 12));
	mod_set_down_converter_vco(dev, 0x00000003);
	mod_set_down_converter_vco(dev, 0x18001E42 | ((RefMul-1) << 25) |
				   ((RefDiv2-1) << 24) | (RefDiv << 14));
	mod_set_down_converter_vco(dev, 0x08008021);
	mod_set_down_converter_vco(dev, Frequency << 15);
	return 0;
}

static int mod_set_dac_clock(struct ddb *dev, u32 Frequency)
{
	int hr, i;

	if (Frequency) {
		ddbwritel(dev, DAC_CONTROL_RESET, DAC_CONTROL);
		msleep(20);
		if (mod_set_si598(dev, Frequency)) {
			dev_err(dev->dev, "mod_set_si598 failed\n");
			return -1;
		}
		msleep(50);
		ddbwritel(dev, 0x000, DAC_CONTROL);
		msleep(20);
		mod_write_dac_register(dev, 0, 0x02);
	}

	for (i = 0; i < 10; i++) {
		hr = mod_init_dac_input(dev);
		if (hr == 0)
			break;
		msleep(100);
	}
	dev_info(dev->dev, "%s OK\n", __func__);
	return hr;
}

static void mod_set_dac_current(struct ddb *dev, u32 Current1, u32 Current2)
{
	mod_write_dac_register2(dev, 0x0b, Current1 & 0x3ff);
	mod_write_dac_register2(dev, 0x0f, Current2 & 0x3ff);
}

static void mod_output_enable(struct ddb *dev, int enable)
{

	u32  RegValue;

	RegValue = ddbreadl(dev, IQOUTPUT_CONTROL);
	RegValue &= ~(IQOUTPUT_CONTROL_ENABLE | IQOUTPUT_CONTROL_RESET);
	ddbwritel(dev, RegValue, IQOUTPUT_CONTROL);

	if (enable) {
		ddbwritel(dev, RegValue | IQOUTPUT_CONTROL_RESET,
			  IQOUTPUT_CONTROL);
		msleep(20);
		ddbwritel(dev, RegValue, IQOUTPUT_CONTROL);
		ddbwritel(dev, RegValue | IQOUTPUT_CONTROL_ENABLE,
			  IQOUTPUT_CONTROL);
	}
}

static int mod_set_iq(struct ddb *dev, u32 steps, u32 chan, u32 freq)
{
	u32 i, j, k, fac = 8;
	u32 s1 = 22, s2 = 33;
	u64 amp = (1ULL << 17) - 1ULL;
	u64 s = 0, c = (amp << s1), ss;
	u64 frq = 0xC90FDAA22168C235ULL; /* PI << 62 */
	u32 *iqtab;
	u32 iqtabadr;
	u32 regval;

	iqtab = kmalloc((steps + 1) * 4, GFP_KERNEL);
	if (!iqtab)
		return -ENOMEM;
	frq = div64_u64(frq, steps * fac) >> (61 - s2);

	/* create sine table */
	for (i = 0; i <= steps * fac / 4; i++) {
		if (!(i & (fac - 1))) {
			j = i / fac;
			ss = s >> s1;
			/* round? ss = ((s >> (s1 - 1)) + 1) >> 1; */
			iqtab[j] = iqtab[steps / 2 - j] = ss;
			iqtab[steps / 2 + j] = iqtab[steps - j] = -ss;
		}
		c -= ((s * frq) >> s2);
		s += ((c * frq) >> s2);
	}
	iqtabadr = chan << 16;
	ddbwritel(dev, chan & 0x0f, MODULATOR_IQTABLE_INDEX);
	for (i = j = 0, k = steps / 4; i < steps; i++) {
		ddbwritel(dev, (iqtabadr + i) | MODULATOR_IQTABLE_INDEX_SEL_I,
			  MODULATOR_IQTABLE_INDEX);
		ddbwritel(dev, iqtab[j], MODULATOR_IQTABLE_DATA);
		regval = ddbreadl(dev, MODULATOR_CONTROL);
		ddbwritel(dev, (iqtabadr + i) | MODULATOR_IQTABLE_INDEX_SEL_Q,
			  MODULATOR_IQTABLE_INDEX);
		ddbwritel(dev, iqtab[k], MODULATOR_IQTABLE_DATA);
		regval = ddbreadl(dev, MODULATOR_CONTROL);
		j += freq;
		j %= steps;
		k += freq;
		k %= steps;
	}
	ddbwritel(dev, steps - 1, MODULATOR_IQTABLE_END);
	kfree(iqtab);
	return 0;
}

u32 eqtab[] = {
	0x0000FFDB, 0x00000121, 0x0000FF0A, 0x000003D7,
	0x000001C4, 0x000005A5, 0x000009CC, 0x0000F50D,
	0x00001B23, 0x0000EEB7, 0x00006A28
};

static void mod_set_channelsumshift(struct ddb *dev, u32 shift)
{
	ddbwritel(dev, (shift & 3) << 2, MODULATOR_CONTROL);
}

static void mod_pre_eq_gain(struct ddb *dev, u16 gain)
{
	ddbwritel(dev, gain, IQOUTPUT_PRESCALER);
}

static void mod_post_eq_gain(struct ddb *dev, u16 igain, u16 qgain)
{
	ddbwritel(dev, ((u32)qgain << 16) | igain, IQOUTPUT_POSTSCALER);
}

static int set_base_frequency(struct ddb *dev, u32 freq)
{
	u32 Ext = 40;
	u32 UP1Frequency = 290;
	u32 UP2Frequency = 1896;
	u32 down, freq10;

	dev_info(dev->dev, "set base to %u\n", freq);
	dev->mod_base.frequency = freq;
	freq /= 1000000;
	freq10 = dev->mod_base.flat_start + 4;
	down = freq + 9 * 8 + freq10 + UP1Frequency + UP2Frequency;

	if ((freq10 + 9 * 8) > (dev->mod_base.flat_end - 4)) {
		dev_err(dev->dev, "Frequency out of range %d\n", freq10);
		return -EINVAL;
	}
	if (down % 8) {
		dev_err(dev->dev, "Invalid Frequency %d\n", down);
		return -EINVAL;
	}
	return mod_set_down(dev, down, 8, Ext);
}

static int mod_init_1(struct ddb *dev, u32 Frequency)
{
	int stat = 0;
	u8 *buffer;
	struct DDMOD_FLASH *flash;
	u32 Ext = 40;
	u32 UP1Frequency = 290;
	u32 UP2Frequency = 1896;
	u32 DownFrequency;
	u32 FrequencyCH10;
	u32 iqfreq, iqsteps, i;

	buffer = kmalloc(4096, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	flash = (struct DDMOD_FLASH *) buffer;

	ddbridge_flashread(dev, 0, buffer, DDMOD_FLASH_START, 4096);

	if (flash->Magic != DDMOD_FLASH_MAGIC && flash->Magic != 1) {
		stat = -EINVAL;
		goto fail;
	}
	dev_info(dev->dev, "srate = %d\n", flash->DataSet[0].Symbolrate * 1000);

	mod_output_enable(dev, 0);
	stat = mod_set_dac_clock(dev, flash->DataSet[0].DACFrequency * 1000);
	if (stat < 0) {
		dev_err(dev->dev, "setting DAC clock failed\n");
		goto fail;
	}
	mod_set_dac_current(dev, 512, 512);

	ddbwritel(dev, flash->DataSet[0].Control2, IQOUTPUT_CONTROL2);

	mod_set_up1(dev, UP1Frequency, 5, Ext);
	mod_set_up2(dev, UP2Frequency, 8, Ext);

	dev->mod_base.flat_start = flash->DataSet[0].FlatStart;
	dev->mod_base.flat_end = flash->DataSet[0].FlatEnd;

	Frequency /= 1000000;
	FrequencyCH10 = flash->DataSet[0].FlatStart + 4;
	DownFrequency = Frequency + 9 * 8 + FrequencyCH10 +
		UP1Frequency + UP2Frequency;
	dev_info(dev->dev, "CH10 = %d, Down = %d\n",
		 FrequencyCH10, DownFrequency);

	if ((FrequencyCH10 + 9 * 8) > (flash->DataSet[0].FlatEnd - 4)) {
		dev_err(dev->dev, "Frequency out of range %d\n", FrequencyCH10);
		stat = -EINVAL;
		goto fail;
	}

	if (DownFrequency % 8 != 0) {
		dev_err(dev->dev, "Invalid Frequency %d\n", DownFrequency);
		stat = -EINVAL;
		goto fail;
	}

	mod_set_down(dev, DownFrequency, 8, Ext);

	for (i = 0; i < 10; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		mod->port = &dev->port[i];
		ddbwritel(dev, 0, CHANNEL_CONTROL(i));

		iqfreq = flash->DataSet[0].FrequencyFactor *
			(FrequencyCH10 + (9 - i) * 8);
		iqfreq += (dev->link[0].ids.hwid == 0x0203dd01) ? 22 : 0;
		iqsteps = flash->DataSet[0].IQTableLength;
		mod_set_iq(dev, iqsteps, i, iqfreq);
		mod_set_modulation(mod, QAM_256);
		mod_set_symbolrate(mod, 6900000);
	}

	mod_bypass_equalizer(dev, 1);
	mod_set_equalizer(dev, 11, flash->DataSet[0].EQTap);
	mod_bypass_equalizer(dev, 0);
	mod_post_eq_gain(dev, flash->DataSet[0].PostScaleI,
			 flash->DataSet[0].PostScaleQ);
	mod_pre_eq_gain(dev, flash->DataSet[0].PreScale);
	/*mod_pre_eq_gain(dev, 0x0680);*/
	dev_info(dev->dev, "prescaler %04x\n", flash->DataSet[0].PreScale);
	mod_set_channelsumshift(dev, 2);
	mod_output_enable(dev, 1);

	/*mod_set_attenuator(dev, 10);*/
fail:
	kfree(buffer);
	return stat;
}

#define PACKET_CLOCKS  (27000000ULL*1504)
#define FACTOR         (1ULL << 22)

/*
 * double Increment =  FACTOR*PACKET_CLOCKS/double(m_OutputBitrate);
 * double Decrement =  FACTOR*PACKET_CLOCKS/double(m_InputBitrate);
 * 27000000 * 1504 * 2^22 / (6900000 * 188 / 204) = 26785190066.1
 */

void ddbridge_mod_rate_handler(void *data)
{
	struct ddb_output *output = (struct ddb_output *) data;
	struct ddb_dma *dma = output->dma;
	struct ddb *dev = output->port->dev;
	struct ddb_mod *mod = &dev->mod[output->nr];

	u32 chan = output->nr;
	u32 OutPacketCount;
	u32 InPacketCount;
	u64 OutPackets, InPackets;
	s64 PCRAdjust;
	u32 PCRAdjustExt, PCRAdjustExtFrac, InPacketDiff, OutPacketDiff;
	s32 PCRCorr;

	s64 pcr;
	s64 PCRIncrementDiff;
	s64 PCRIncrement;
	u64 mul;

	if (!mod->pcr_correction)
		return;
	spin_lock(&dma->lock);
	ddbwritel(dev, mod->Control | CHANNEL_CONTROL_FREEZE_STATUS,
		  CHANNEL_CONTROL(output->nr));

	OutPacketCount = ddbreadl(dev, CHANNEL_PKT_COUNT_OUT(chan));
	if (OutPacketCount < mod->LastOutPacketCount)
		mod->OutOverflowPacketCount += 1;
	mod->LastOutPacketCount = OutPacketCount;

	InPacketCount = ddbreadl(dev, CHANNEL_PKT_COUNT_IN(chan));
	if (InPacketCount < mod->LastInPacketCount)
		mod->InOverflowPacketCount += 1;
	mod->LastInPacketCount = InPacketCount;

	OutPackets = ((u64) (mod->OutOverflowPacketCount) << 20) |
		OutPacketCount;
	InPackets = ((u64) (mod->InOverflowPacketCount) << 20) |
		InPacketCount;

	PCRAdjust = (s64) ((u64) ddbreadl(dev,
					  CHANNEL_PCR_ADJUST_ACCUL(chan)) |
			   (((u64) ddbreadl(dev,
					    CHANNEL_PCR_ADJUST_ACCUH(chan))
			     << 32)));
	PCRAdjustExt = (u32)((PCRAdjust & 0x7FFFFFFF) >> 22);
	PCRAdjustExtFrac = (u32)((PCRAdjust & 0x003FFFFF) >> 12);
	PCRAdjust >>= 31;
	InPacketDiff = (u32) (InPackets - mod->LastInPackets);
	OutPacketDiff = (u32) (OutPackets - mod->LastOutPackets);
	PCRCorr = 0;

	switch (mod->State) {
	case CM_STARTUP:
		if (mod->StateCounter) {
			if (mod->StateCounter == 1) {
				if (mod->ibitrate == 0) {
					mul = (0x1000000 *
					       (u64) (OutPacketDiff -
						      InPacketDiff -
						      InPacketDiff/1000));
					if (OutPacketDiff)
						mod->rate_inc =
							div_u64(mul,
								OutPacketDiff);
					else
						mod->rate_inc = 0;
					mod_set_rateinc(dev, output->nr);
					mod->PCRIncrement =
						div_u64(26785190066ULL,
							mod->modulation + 3);
					if (InPacketDiff)
						mod->PCRDecrement =
							div_u64(mod->PCRIncrement *
								(u64)
								OutPacketDiff,
								InPacketDiff);
					else
						mod->PCRDecrement = 0;
					mod_set_incs(output);
				} else {
					mod->PCRIncrement =
						div_u64(26785190066ULL,
							mod->modulation + 3);
					mod->PCRDecrement =
						div_u64(FACTOR*PACKET_CLOCKS,
							mod->ibitrate >> 32);
					mod_set_incs(output);
				}
			}
			mod->StateCounter--;
			break;
		} else if (InPacketDiff >= mod->MinInputPackets) {
			mod->State = CM_ADJUST;
			mod->Control |= CHANNEL_CONTROL_ENABLE_PCRADJUST;
			mod->InPacketsSum = 0;
			mod->OutPacketsSum = 0;
			mod->PCRAdjustSum = 0;
			mod->StateCounter = CM_AVERAGE;
		}
		break;

	case CM_ADJUST:
		if (InPacketDiff < mod->MinInputPackets) {
			dev_info(dev->dev, "PCR Adjust reset  IN: %u  Min: %u\n",
				 InPacketDiff, mod->MinInputPackets);
			mod->InPacketsSum = 0;
			mod->OutPacketsSum = 0;
			mod->PCRAdjustSum = 0;
			mod->StateCounter = CM_AVERAGE;
			ddbwritel(dev,
				  (mod->Control |
				   CHANNEL_CONTROL_FREEZE_STATUS) &
				  ~CHANNEL_CONTROL_ENABLE_PCRADJUST,
				  CHANNEL_CONTROL(chan));
			break;
		}

		mod->PCRAdjustSum += (s32) PCRAdjust;
		mod->InPacketsSum += InPacketDiff;
		mod->OutPacketsSum += OutPacketDiff;
		if (mod->StateCounter--)
			break;

		if (mod->OutPacketsSum)
			PCRIncrement = div_s64((s64)mod->InPacketsSum *
					       (s64)mod->PCRDecrement +
					       (s64)(mod->OutPacketsSum >> 1),
					       mod->OutPacketsSum);
		else
			PCRIncrement = 0;

		if (mod->PCRAdjustSum > 0)
			PCRIncrement = RoundPCRDown(PCRIncrement);
		else
			PCRIncrement = RoundPCRUp(PCRIncrement);

		PCRIncrementDiff = PCRIncrement - mod->PCRIncrement;
		if (PCRIncrementDiff > HW_LSB_MASK)
			PCRIncrementDiff = HW_LSB_MASK;
		if (PCRIncrementDiff < -HW_LSB_MASK)
			PCRIncrementDiff = -HW_LSB_MASK;

		mod->PCRIncrement += PCRIncrementDiff;
		pcr = ConvertPCR(mod->PCRIncrement);
		dev_info(dev->dev, "outl %016llx\n", pcr);
		ddbwritel(dev,	pcr & 0xffffffff,
			  CHANNEL_PCR_ADJUST_OUTL(output->nr));
		ddbwritel(dev,	(pcr >> 32) & 0xffffffff,
			  CHANNEL_PCR_ADJUST_OUTH(output->nr));
		mod_busy(dev, chan);

		PCRCorr = (s32) (PCRIncrementDiff >> HW_LSB_SHIFT);
		mod->PCRRunningCorr += PCRCorr;

		mod->InPacketsSum = 0;
		mod->OutPacketsSum = 0;
		mod->PCRAdjustSum = 0;
		mod->StateCounter = CM_AVERAGE;
		break;

	default:
		break;
	}
	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(chan));

	mod->LastInPackets = InPackets;
	mod->LastOutPackets = OutPackets;
	mod->LastPCRAdjust = (s32) PCRAdjust;

	spin_unlock(&dma->lock);

	dev_info(dev->dev, "chan %d out %016llx in %016llx indiff %08x\n",
		 chan, OutPackets, InPackets, InPacketDiff);
	dev_info(dev->dev, "cnt  %d pcra %016llx pcraext %08x pcraextfrac %08x pcrcorr %08x pcri %016llx\n",
		 mod->StateCounter, PCRAdjust, PCRAdjustExt,
		 PCRAdjustExtFrac, PCRCorr, mod->PCRIncrement);
}

static int mod3_set_base_frequency(struct ddb *dev, u32 frequency)
{
	u64 tmp;

	if (frequency % 1000)
		return -EINVAL;
	if (frequency < 114000000)
		return -EINVAL;
	if (frequency > 1874000000)
		return -EINVAL;
	dev->mod_base.frequency = frequency;
	tmp = frequency;
	tmp <<= 33;
	tmp = div64_s64(tmp, 4915200000);
	dev_info(dev->dev, "set base frequency = %u  regs = 0x%08llx\n", frequency, tmp);
	ddbwritel(dev, (u32) tmp, RFDAC_FCW);
	return 0;
}

static void mod3_set_cfcw(struct ddb_mod *mod, u32 f)
{
	struct ddb *dev = mod->port->dev;
	s32 freq = f;
	s32 dcf = dev->mod_base.frequency;
	s64 tmp, srdac = 245760000;
	u32 cfcw;

	tmp = ((s64) (freq - dcf)) << 32;
	tmp = div64_s64(tmp, srdac);
	cfcw = (u32) tmp;
	dev_info(dev->dev, "f=%u cfcw = %08x dcf = %08x, nr = %u\n",
		 f, cfcw, dcf, mod->port->nr);
	ddbwritel(dev, cfcw, SDR_CHANNEL_CFCW(mod->port->nr));
}

static int mod3_set_frequency(struct ddb_mod *mod, u32 frequency)
{
#if 0
	struct ddb *dev = mod->port->dev;

	if (frequency % 1000)
		return -EINVAL;
	if ((frequency < 114000000) || (frequency > 874000000))
		return -EINVAL;
	if (frequency > dev->mod_base.frequency)
		if (frequency - dev->mod_base.frequency > 100000000)
			return -EINVAL;
	else
		if (dev->mod_base.frequency - frequency > 100000000)
			return -EINVAL;
#endif
	mod3_set_cfcw(mod, frequency);
	return 0;
}

static int mod3_set_ari(struct ddb_mod *mod, u32 rate)
{
	ddbwritel(mod->port->dev, rate, SDR_CHANNEL_ARICW(mod->port->nr));
	return 0;
}


static int mod3_set_sample_rate(struct ddb_mod *mod, u32 rate)
{
	struct ddb *dev = mod->port->dev;
	u32 cic, inc, bypass = 0;

	switch (rate) {
		/* 2^31 * freq*4*cic / 245.76Mhz */
	case SYS_DVBT_6:
		inc = 0x72492492;
		cic = 8;
		break;
	case SYS_DVBT_7:
		inc = 1957341867;
		cic = 7;
		break;
	case SYS_DVBT_8:
		//rate = 8126984;
		inc = 1917396114;
		cic = 6;
		break;
	case SYS_DVBC_6900:
		inc = 0x73000000; //1929379840;
		cic = 8;
		break;
	case 9:
		inc = 0x47e00000; //1929379840;
		cic = 10;
		bypass = 2;
		break;

	case SYS_J83B_64_6: /* 5056941  */
		inc = 0x695a5a1d;
		cic = 10;
		break;
	case SYS_J83B_256_6: /* 5360537  */
		inc = 0x6fad87da;
		cic = 10;
		break;
		
	case SYS_ISDBT_6:
		inc = 0x7684BD82; //1988410754;
		cic = 7;
		break;
	case SYS_DVB_22:
		inc = 0x72955555; // 1922389333;
		cic = 5;
		bypass = 2;
		break;
	case SYS_DVB_24:
		inc = 0x7d000000;
		cic = 5;
		bypass = 2;
		break;
	case SYS_DVB_30:
		inc = 0x7d000000;
		cic = 4;
		bypass = 2;
		break;
	case SYS_ISDBS_2886:
		inc = 0x78400000;
		cic = 4;
		bypass = 2;
		break;
	default:
	{
		u64 a;
				
		if (rate < 1000000)
			return -EINVAL;
		if (rate > 30720000)
			return -EINVAL;

		bypass = 2;
		if (rate > 24576000)
			cic = 4;
		else if (rate > 20480000)
			cic = 5;
		else if (rate > 17554286)
			cic = 6;
		else if (rate > 15360000)
			cic = 7;
		else
			cic = 8;
		a = (1ULL << 31) * rate * 2 * cic;
		inc = div_s64(a, 245760000);
		break;
	}
	}
	dev_info(dev->dev, "inc = %08x, cic = %u, bypass = %u\n", inc, cic, bypass);
	ddbwritel(mod->port->dev, inc, SDR_CHANNEL_ARICW(mod->port->nr));
	ddbwritel(mod->port->dev, (cic << 8) | (bypass << 4),
		  SDR_CHANNEL_CONFIG(mod->port->nr));
	return 0;
}


static int mod3_prop_proc(struct ddb_mod *mod, struct dtv_property *tvp)
{
	switch (tvp->cmd) {
	case MODULATOR_OUTPUT_ARI:
		return mod3_set_ari(mod, tvp->u.data);

	case MODULATOR_OUTPUT_RATE:
		return mod3_set_sample_rate(mod, tvp->u.data);

	case MODULATOR_FREQUENCY:
		return mod3_set_frequency(mod, tvp->u.data);

	case MODULATOR_BASE_FREQUENCY:
		return mod3_set_base_frequency(mod->port->dev, tvp->u.data);

	case MODULATOR_ATTENUATOR:
		return mod_set_sdr_attenuator(mod->port->dev, tvp->u.data);

	case MODULATOR_GAIN:
		return mod_set_sdr_gain(mod->port->dev, tvp->u.data);

	}
	return -EINVAL;
}

static int mod_prop_proc(struct ddb_mod *mod, struct dtv_property *tvp)
{
	if (mod->port->dev->link[0].info->version >= 16)
		return mod3_prop_proc(mod, tvp);
	switch (tvp->cmd) {
	case MODULATOR_SYMBOL_RATE:
		return mod_set_symbolrate(mod, tvp->u.data);

	case MODULATOR_MODULATION:
		return mod_set_modulation(mod, tvp->u.data);

	case MODULATOR_FREQUENCY:
		return mod_set_frequency(mod, tvp->u.data);

	case MODULATOR_ATTENUATOR:
		return mod_set_attenuator(mod->port->dev, tvp->u.data);

	case MODULATOR_INPUT_BITRATE:
#ifdef KERNEL_DVB_CORE
		return mod_set_ibitrate(mod, *(u64 *) &tvp->u.buffer.data[0]);
#else
		return mod_set_ibitrate(mod, tvp->u.data64);
#endif

	case MODULATOR_GAIN:
		if (mod->port->dev->link[0].info->version == 2)
			return mod_set_vga(mod->port->dev, tvp->u.data);
		return -EINVAL;

	case MODULATOR_RESET:
		if (mod->port->dev->link[0].info->version == 2)
			return mod_fsm_setup(mod->port->dev,0 );
		return -EINVAL;

	case MODULATOR_STATUS:
		if (mod->port->dev->link[0].info->version != 2)
			return -EINVAL;
		if (tvp->u.data & 2)
			ddbwritel(mod->port->dev,
				  MAX2871_CONTROL_CE |
				  MAX2871_CONTROL_LOSTLOCK |
				  MAX2871_CONTROL_ENABLE_LOSTLOCK_EVENT,
				  MAX2871_CONTROL);
		return 0;
	}
	return 0;
}

static int mod_prop_get3(struct ddb_mod *mod, struct dtv_property *tvp)
{
	struct ddb *dev = mod->port->dev;

	switch (tvp->cmd) {
	case MODULATOR_INFO:
		tvp->u.data = dev->link[0].info->version;
		return 0;
	case MODULATOR_GAIN:
		tvp->u.data = 0xff & ddbreadl(dev, RF_VGA);
		return 0;
	default:
		return -1;
	}
}

static int mod_prop_get(struct ddb_mod *mod, struct dtv_property *tvp)
{
	struct ddb *dev = mod->port->dev;

	if (mod->port->dev->link[0].info->version >= 16)
		return mod_prop_get3(mod, tvp);
	if (mod->port->dev->link[0].info->version != 2)
		return -1;
	switch (tvp->cmd) {
	case MODULATOR_INFO:
		tvp->u.data = 2;
		return 0;

	case MODULATOR_GAIN:
		tvp->u.data = 0xff & ddbreadl(dev, RF_VGA);
		return 0;

	case MODULATOR_ATTENUATOR:
		tvp->u.data = 0x1f & ddbreadl(dev, RF_ATTENUATOR);
		return 0;

	case MODULATOR_STATUS:
	{
		u32 status = 0, val;

		val = ddbreadl(dev, MAX2871_CONTROL);
		if (!(val & MAX2871_CONTROL_LOCK))
			status |= 1;
		if (val & MAX2871_CONTROL_LOSTLOCK)
			status |= 2;
		ddbwritel(dev, val, MAX2871_CONTROL);

		val = ddbreadl(dev, TEMPMON_CONTROL);
		if (val & TEMPMON_CONTROL_OVERTEMP)
			status |= 4;
		val = ddbreadl(dev, HARDWARE_VERSION);
		if (val == 0xffffffff)
			status |= 8;
		tvp->u.data = status;
		return 0;
	}
	default:
		return -1;
	}
}

int ddbridge_mod_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb *dev = output->port->dev;
	struct ddb_mod *mod = &dev->mod[output->nr];
	struct dtv_property *tvp = NULL;
	struct dtv_properties *tvps =
		(struct dtv_properties __user *) parg;
	int i, ret = 0;

	if (dev->link[0].info->version >= 16 &&
	    (cmd != FE_SET_PROPERTY && cmd != IOCTL_DDB_MCI_CMD))
		return -EINVAL;
	mutex_lock(&dev->ioctl_mutex);
	switch (cmd) {
	case FE_SET_PROPERTY:
		if ((tvps->num == 0) || (tvps->num > DTV_IOCTL_MAX_MSGS)) {
			ret = -EINVAL;
			break;
		}
		tvp = memdup_user(tvps->props, tvps->num * sizeof(*tvp));
		if (IS_ERR(tvp)) {
			ret = PTR_ERR(tvp);
			break;
		}
		for (i = 0; i < tvps->num; i++) {
			ret = mod_prop_proc(mod, tvp + i);
			if (ret < 0)
				break;
			(tvp + i)->result = ret;
		}
		kfree(tvp);
		break;

	case FE_GET_PROPERTY:
		if ((tvps->num == 0) || (tvps->num > DTV_IOCTL_MAX_MSGS)) {
			ret = -EINVAL;
			break;
		}
		tvp = memdup_user(tvps->props, tvps->num * sizeof(*tvp));
		if (IS_ERR(tvp)) {
			ret = PTR_ERR(tvp);
			break;
		}
		for (i = 0; i < tvps->num; i++) {
			ret = mod_prop_get(mod, tvp + i);
			if (ret < 0)
				break;
			(tvp + i)->result = ret;
		}
		if ((ret >= 0) &&
		    copy_to_user((void __user *)tvps->props, tvp,
				 tvps->num * sizeof(struct dtv_property)))
			ret = -EFAULT;
		kfree(tvp);
		break;

	case DVB_MOD_SET:
	{
		struct dvb_mod_params *mp = parg;

		if (dev->link[0].info->version < 2) {
			if (mp->base_frequency != dev->mod_base.frequency)
				if (set_base_frequency(dev, mp->base_frequency)) {
					ret = -EINVAL;
					break;
				}
		} else {
			int i, streams = dev->link[0].info->port_num;

			dev->mod_base.frequency = mp->base_frequency;
			for (i = 0; i < streams; i++) {
				struct ddb_mod *mod = &dev->mod[i];

				mod->port = &dev->port[i];
				mod_set_modulation(mod, QAM_256);
				mod_set_symbolrate(mod, 6900000);
				mod_set_frequency(mod, dev->mod_base.frequency +
						  i * 8000000);
			}
		}
		mod_set_attenuator(dev, mp->attenuator);
		break;
	}
	case DVB_MOD_CHANNEL_SET:
	{
		struct dvb_mod_channel_params *cp = parg;
		struct ddb_mod *mod = &dev->mod[output->nr];
		int res;

		res = mod_set_modulation(mod, cp->modulation);
		if (res) {
			ret = res;
			break;
		}
		res = mod_set_ibitrate(mod, cp->input_bitrate);
		if (res) {
			ret = res;
			break;
		}
		mod->pcr_correction = cp->pcr_correction;
		break;
	}
	case IOCTL_DDB_MCI_CMD:
	{
		struct ddb_mci_msg *msg = 
			(struct ddb_mci_msg __user *) parg;
		struct ddb_link *link;

		if (dev->link[0].ids.revision != 1)
			break;

 		if (msg->link > 3) {
			ret = -EFAULT;
			break;
		}
		link = &dev->link[msg->link];
		if (!link->mci_ok) {
			ret = -EFAULT;
			break;
		}
		ret = ddb_mci_cmd_link(link, &msg->cmd, &msg->res);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&dev->ioctl_mutex);
	return ret;
}

static int mod_init_2_1(struct ddb *dev, u32 Frequency)
{
	int i, streams = dev->link[0].info->port_num;

	dev->mod_base.frequency = Frequency;
	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];
		mod->port = &dev->port[i];
	}
	return 0;
}

static int mod_init_2(struct ddb *dev, u32 Frequency)
{
	int i, status, streams = dev->link[0].info->port_num;

	dev->mod_base.frequency = Frequency;

	status = mod_fsm_setup(dev, 0);
	if (status) {
		dev_err(dev->dev, "FSM setup failed!\n");
		return -1;
	}
	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		mod->port = &dev->port[i];
		mod_set_modulation(mod, QAM_256);
		mod_set_symbolrate(mod, 6900000);
		mod_set_frequency(mod, dev->mod_base.frequency + i * 8000000);
	}
	if (streams <= 8)
		mod_set_vga(dev, RF_VGA_GAIN_N8);
	else if (streams <= 16)
		mod_set_vga(dev, RF_VGA_GAIN_N16);
	else
		mod_set_vga(dev, RF_VGA_GAIN_N24);

	udelay(10);
	mod_set_attenuator(dev, 0);
	return 0;
}


/****************************************************************************/

static u32 vsb13500[64] = {
	0x0000000E, 0x00010004, 0x00020003, 0x00030009,
	0x0004FFFA, 0x00050002, 0x0006FFF8, 0x0007FFF0,
	0x00080000, 0x0009FFEA, 0x000A0001, 0x000B0003,
	0x000CFFF9, 0x000D0025,	0x000E0004, 0x000F001F,
	0x00100023, 0x0011FFEE, 0x00120020, 0x0013FFD0,
	0x0014FFD5, 0x0015FFED, 0x0016FF8B, 0x0017000B,
	0x0018FFC8, 0x0019FFF1, 0x001A009E, 0x001BFFEF,
	0x001C013B, 0x001D00CB, 0x001E0031, 0x001F05F6,

	0x0040FFFF, 0x00410004, 0x0042FFF8, 0x0043FFFE,
	0x0044FFFA, 0x0045FFF3, 0x00460003, 0x0047FFF4,
	0x00480005, 0x0049000D, 0x004A0000, 0x004B0022,
	0x004C0005, 0x004D000D, 0x004E0013, 0x004FFFDF,
	0x00500007, 0x0051FFD4, 0x0052FFD2, 0x0053FFFD,
	0x0054FFB7, 0x00550021, 0x00560009, 0x00570010,
	0x00580097, 0x00590003, 0x005A009D, 0x005B004F,
	0x005CFF89, 0x005D0097, 0x005EFD42, 0x005FFCBE
};

static u32 stage2[16] = {
	0x0080FFFF, 0x00810000, 0x00820005, 0x00830000,
	0x0084FFF0, 0x00850000, 0x00860029, 0x00870000,
	0x0088FFA2, 0x0089FFFF, 0x008A00C9, 0x008B000C,
	0x008CFE49, 0x008DFF9B, 0x008E04D4, 0x008F07FF
};

static void mod_set_sdr_table(struct ddb_mod *mod, u32 *tab, u32 len)
{
	struct ddb *dev = mod->port->dev;
	u32 i;

	for (i = 0; i < len; i++)
		ddbwritel(dev, tab[i], SDR_CHANNEL_SETFIR(mod->port->nr));
}

static int rfdac_init(struct ddb *dev)
{
	int i;
	u32 tmp;

	ddbwritel(dev, RFDAC_CMD_POWERDOWN, RFDAC_CONTROL);
	for (i = 0; i < 10; i++) {
		msleep(20);
		tmp = ddbreadl(dev, RFDAC_CONTROL);
		if ((tmp & RFDAC_CMD_STATUS) == 0x00)
			break;
	}
	if (tmp & 0x80)
		return -1;
	//dev_info(dev->dev, "sync %d:%08x\n", i, tmp);
	ddbwritel(dev, RFDAC_CMD_RESET, RFDAC_CONTROL);
	for (i = 0; i < 10; i++) {
		msleep(20);
		tmp = ddbreadl(dev, RFDAC_CONTROL);
		if ((tmp & RFDAC_CMD_STATUS) == 0x00)
			break;
	}
	if (tmp & 0x80)
		return -1;
	//dev_info(dev->dev, "sync %d:%08x\n", i, tmp);
	ddbwritel(dev, RFDAC_CMD_SETUP, RFDAC_CONTROL);
	for (i = 0; i < 10; i++) {
		msleep(20);
		tmp = ddbreadl(dev, RFDAC_CONTROL);
		if ((tmp & RFDAC_CMD_STATUS) == 0x00)
			break;
	}
	if (tmp & 0x80)
		return -1;
	//dev_info(dev->dev, "sync %d:%08x\n", i, tmp);
	ddbwritel(dev, 0x01, JESD204B_BASE);
	for (i = 0; i < 400; i++) {
		msleep(20);
		tmp = ddbreadl(dev, JESD204B_BASE);
		if ((tmp & 0xc0000000) == 0xc0000000)
			break;
	}
	//dev_info(dev->dev, "sync %d:%08x\n", i, tmp);
	if ((tmp & 0xc0000000) != 0xc0000000)
		return -1;
	return 0;
}

static int mod_init_3(struct ddb *dev, u32 Frequency)
{
	int streams = dev->link[0].info->port_num;
	int i, ret = 0;

	ret = mod_setup_max2871(dev, max2871_sdr);
	if (ret)
		dev_err(dev->dev, "PLL setup failed\n");
	ret = rfdac_init(dev);
	if (ret)
		ret = rfdac_init(dev);
	if (ret)
		dev_err(dev->dev, "RFDAC setup failed\n");

	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		mod->port = &dev->port[i];
		mod_set_sdr_table(mod, vsb13500, 64);
		mod_set_sdr_table(mod, stage2, 16);
	}
	ddbwritel(dev, 0x1800, 0x244);
	ddbwritel(dev, 0x01, 0x240);

	mod3_set_base_frequency(dev, 602000000);
	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		ddbwritel(dev, 0x00, SDR_CHANNEL_CONTROL(i));
		ddbwritel(dev, 0x06, SDR_CHANNEL_CONFIG(i));
		ddbwritel(dev, 0x70800000, SDR_CHANNEL_ARICW(i));
		mod3_set_frequency(mod, Frequency + 7000000 * i);

		ddbwritel(dev, 0x00011f80, SDR_CHANNEL_RGAIN(i));
		ddbwritel(dev, 0x00002000, SDR_CHANNEL_FM1GAIN(i));
		ddbwritel(dev, 0x00001000, SDR_CHANNEL_FM2GAIN(i));
	}
	mod_set_sdr_attenuator(dev, 0);
	mod_set_sdr_gain(dev, 64);
	return ret;
}


static int mod_init_sdr_iq(struct ddb *dev)
{
	int streams = dev->link[0].info->port_num;
	int i, ret = 0;

	ret = mod_setup_max2871(dev, max2871_sdr);
	if (ret)
		dev_err(dev->dev, "PLL setup failed\n");
	ret = rfdac_init(dev);
	if (ret)
		ret = rfdac_init(dev);
	if (ret)
		dev_err(dev->dev, "RFDAC setup failed\n");
	
	ddbwritel(dev, 0x01, 0x240);


	//mod3_set_base_frequency(dev, 602000000);
	dev->mod_base.frequency = 570000000;
	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		mod->port = &dev->port[i];
		if (dev->link[0].ids.revision != 1)
			ddbwritel(dev, 0x00, SDR_CHANNEL_CONTROL(i));
	}
	if (dev->link[0].ids.revision == 1)
		return ret;
	mod_set_sdr_attenuator(dev, 0);
	udelay(10);
	mod_set_sdr_gain(dev, 120);
	return ret;
}

int ddbridge_mod_init(struct ddb *dev)
{
	dev_info(dev->dev, "Revision: %u\n", dev->link[0].ids.revision);
	if (dev->link[0].ids.revision == 1) {
		switch (dev->link[0].info->version) {
		case 0:
		case 1:
			return mod_init_1(dev, 722000000);
		case 2: /* FSM */
			return mod_init_2(dev, 114000000);
		case 16: /* PAL */
			return mod_init_3(dev, 503250000);
		case 17: /* raw IQ */
		case 18: /* IQ+FFT */
			return mod_init_sdr_iq(dev);
		default:
			return -1;
		}
	}
	switch (dev->link[0].info->version) {
	case 0:
	case 1:
		return mod_init_1(dev, 722000000);
	case 2: /* FSM */
		return mod_init_2(dev, 114000000);
	case 16: /* PAL */
		return mod_init_3(dev, 503250000);
	case 17: /* raw IQ */
		return mod_init_sdr_iq(dev);
	case 18: /* IQ+FFT */
		return mod_init_sdr_iq(dev);
	default:
		return -1;
	}
}
