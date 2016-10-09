/*
 * ddbridge.c: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2015 Digital Devices GmbH
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include "ddbridge.h"
#include "ddbridge-regs.h"

#include <linux/dvb/mod.h>

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

// Calculating KF, LF from Symbolrate
//
//  Symbolrate is usually calculated as (M/N) * 10.24 MS/s
//  
//   Common Values for M,N 
//     J.83 Annex A,
//     Euro Docsis  6.952 MS/s : M =   869, N =  1280
//                  6.900 MS/s : M =   345, N =   512
//                  6.875 MS/s : M =  1375, N =  2048
//                  6.111 MS/s : M =  6111, N = 10240
//     J.83 Annex B **
//      QAM64       5.056941   : M =   401, N =   812
//      QAM256      5.360537   : M =    78, N =   149
//     J.83 Annex C **
//                  5.309734   : M =  1889, N =  3643
//
//    For the present hardware 
//      KF' = 256 * M 
//      LF' = 225 * N 
//       or
//      KF' = Symbolrate in Hz
//      LF' = 9000000
//
//      KF  = KF' / gcd(KF',LF')
//      LF  = LF' / gcd(KF',LF')
// Note: LF must not be a power of 2.
//       Maximum value for KF,LF = 13421727 ( 0x7FFFFFF )
//    ** using these M,N values will result in a small err (<5ppm)
//       calculating KF,LF directly gives the exact normative result
//       but with rather large KF,LF values

static inline u32 gcd(u32 u,u32 v)
{
    int s = 0;
    while (((u|v)&1) == 0) {
	    s += 1;
	    u >>= 1;
	    v >>= 1;
    }
    while ((u&1) == 0)
	    u >>= 1;
    do {
	    while ( (v&1) == 0 ) v >>= 1;
	    if( u > v ) {
		    u32 t = v;
		    v = u;
		    u = t;
	    }
	    v = v - u;
    } while(v != 0);
    return u << s;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int mod_SendChannelCommand(struct ddb *dev, u32 Channel, u32 Command)
{
	u32 ControlReg = ddbreadl(dev, CHANNEL_CONTROL(Channel));
	
	ControlReg = (ControlReg & ~CHANNEL_CONTROL_CMD_MASK)|Command;
	ddbwritel(dev, ControlReg, CHANNEL_CONTROL(Channel));
	while(1) {
		ControlReg = ddbreadl(dev, CHANNEL_CONTROL(Channel));
		if (ControlReg == 0xFFFFFFFF)
			return -EIO;
		if((ControlReg & CHANNEL_CONTROL_CMD_STATUS) == 0)
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
	mod->Control = 0;
	if (dev->link[0].info->version == 2)
		mod_SendChannelCommand(dev, output->nr, CHANNEL_CONTROL_CMD_FREE);
	ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));
#if 0
	udelay(10);
	ddbwritel(dev, CHANNEL_CONTROL_RESET, CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));
#endif
	mod_busy(dev, output->nr);
	pr_info("DDBridge: mod_output_stop %d.%d\n", dev->nr, output->nr);
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
	
	pr_info("DDBridge: ibitrate %llu\n", mod->ibitrate);
	pr_info("DDBridge: obitrate %llu\n", mod->obitrate);
	
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
	pr_info("DDBridge: ibr=%llu, obr=%llu, ri=0x%06x\n",
		mod->ibitrate >> 32, mod->obitrate >> 32, ri);
}

static int mod_calc_obitrate(struct ddb_mod *mod)
{
	struct ddb *dev = mod->port->dev;
	u64 ofac; 

	ofac = (((u64) mod->symbolrate) << 32) * 188;
	ofac = div_u64(ofac, 204);
	mod->obitrate = ofac * (mod->modulation + 3);
	return 0;
}

static int mod_set_symbolrate(struct ddb_mod *mod, u32 srate)
{
	struct ddb *dev = mod->port->dev;
	u64 ofac; 

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

static int mod_set_modulation(struct ddb_mod *mod, enum fe_modulation modulation)
{
	struct ddb *dev = mod->port->dev;
	u64 ofac; 

	if (modulation > QAM_256 || modulation < QAM_16)
		return -EINVAL;
	mod->modulation = modulation;
	if (dev->link[0].info->version < 2) 
		ddbwritel(dev, qamtab[modulation], CHANNEL_SETTINGS(mod->nr));
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

int ddbridge_mod_output_start(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;
	u32 Channel = output->nr;
	struct ddb_mod *mod = &dev->mod[output->nr];
	u32 Symbolrate = mod->symbolrate;

	mod_calc_rateinc(mod);
	/*PCRIncrement = RoundPCR(PCRIncrement);*/
	/*PCRDecrement = RoundPCR(PCRDecrement);*/

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

	ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, CHANNEL_CONTROL_RESET, CHANNEL_CONTROL(output->nr));
	udelay(10);
	ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));

	pr_info("DDBridge: CHANNEL_BASE = %08x\n", CHANNEL_BASE);
	pr_info("DDBridge: CHANNEL_CONTROL = %08x\n", CHANNEL_CONTROL(Channel));
	if (dev->link[0].info->version == 2) {
		//u32 Output = ((dev->mod_base.frequency - 114000000)/8000000 + Channel) % 96;
		u32 Output = (mod->frequency - 114000000) / 8000000;
		u32 KF = Symbolrate;
		u32 LF = 9000000UL;
		u32 d = gcd(KF,LF);
		u32 checkLF;

		ddbwritel(dev, mod->modulation - 1, CHANNEL_SETTINGS(Channel));
		ddbwritel(dev, Output, CHANNEL_SETTINGS2(Channel));
		
		KF = KF / d;
		LF = LF / d;
		
		while( (KF > KFLF_MAX) || (LF > KFLF_MAX) ) {
			KF >>= 1;
			LF >>= 1;
		}
		
		checkLF = LF;
		while ((checkLF & 1) == 0)
			checkLF >>= 1;
		if (checkLF <= 1)
			return -EINVAL;

		pr_info("DDBridge: KF=%u LF=%u Output=%u mod=%u\n", KF, LF, Output, mod->modulation);
		ddbwritel(dev, KF, CHANNEL_KF(Channel));
		ddbwritel(dev, LF, CHANNEL_LF(Channel));
		
		if (mod_SendChannelCommand(dev, Channel, CHANNEL_CONTROL_CMD_SETUP))
			return -EINVAL;
		mod->Control = CHANNEL_CONTROL_ENABLE_DVB;
	} else {
		/* QAM: 600 601 602 903 604 = 16 32 64 128 256 */
		/* ddbwritel(dev, 0x604, CHANNEL_SETTINGS(output->nr)); */
		ddbwritel(dev, qamtab[mod->modulation], CHANNEL_SETTINGS(output->nr));
		mod->Control = (CHANNEL_CONTROL_ENABLE_IQ | CHANNEL_CONTROL_ENABLE_DVB);
	}
	mod_set_rateinc(dev, output->nr);
	mod_set_incs(output);

	mod->Control |= CHANNEL_CONTROL_ENABLE_SOURCE;

	ddbwritel(dev, mod->Control, CHANNEL_CONTROL(output->nr));
	if (dev->link[0].info->version == 2)
		if (mod_SendChannelCommand(dev, Channel, CHANNEL_CONTROL_CMD_UNMUTE))
			return -EINVAL;
	pr_info("DDBridge: mod_output_start %d.%d\n", dev->nr, output->nr);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int mod_write_max2871(struct ddb *dev, u32 val)
{
	ddbwritel(dev, val, MAX2871_OUTDATA);
	ddbwritel(dev, MAX2871_CONTROL_CE | MAX2871_CONTROL_WRITE, MAX2871_CONTROL);
	while(1) {
		u32 ControlReg = ddbreadl(dev, MAX2871_CONTROL);
		if (ControlReg == 0xFFFFFFFF)
			return -EIO;
		if ((ControlReg & MAX2871_CONTROL_WRITE) == 0)
			break;
	}
	return 0;
}

static int mod_setup_max2871(struct ddb *dev)
{
	int status = 0;
	int i;
	
	ddbwritel(dev, MAX2871_CONTROL_CE, MAX2871_CONTROL);
	for (i = 0; i < 2; i++) {
		status = mod_write_max2871(dev, 0x00440005);
		if (status)
			break;
		status = mod_write_max2871(dev, 0x6199003C);
		if (status)
			break;
		status = mod_write_max2871(dev, 0x000000CB);
		if (status)
			break;
		status = mod_write_max2871(dev, 0x510061C2);
		if (status)
			break;
		status = mod_write_max2871(dev, 0x600080A1);
		if (status)
			break;
		status = mod_write_max2871(dev, 0x00730040);
		if (status)
			break;
		msleep(30);
	} while(0);

	if (status == 0) {
		u32 ControlReg = ddbreadl(dev, MAX2871_CONTROL);
		
		if ((ControlReg & MAX2871_CONTROL_LOCK) == 0)
			status = -EIO;
	}

	return status;
}


static int mod_fsm_setup(struct ddb *dev, u32 FrequencyPlan, u32 MaxUsedChannels)
{
	int status = 0;
	u32 Capacity;
	u32 tmp = ddbreadl(dev, FSM_STATUS);
	
	if ((tmp & FSM_STATUS_READY) == 0) {
		status = mod_setup_max2871(dev);
		if (status)
			return status;
		ddbwritel(dev, FSM_CMD_RESET, FSM_CONTROL);
		msleep(10);
		
		tmp = ddbreadl(dev, FSM_STATUS);
		if ((tmp & FSM_STATUS_READY) == 0)
			return -1;
	}
	Capacity = ddbreadl(dev, FSM_CAPACITY);
	if (((tmp & FSM_STATUS_QAMREADY) != 0) &&
	    ((Capacity & FSM_CAPACITY_INUSE) != 0))
		return -EBUSY;
	
	ddbwritel(dev, FSM_CMD_SETUP, FSM_CONTROL);
	msleep(10);
	tmp = ddbreadl(dev, FSM_STATUS);
	
	if ((tmp & FSM_STATUS_QAMREADY) == 0)
		return -1;
	
	if (MaxUsedChannels == 0)
		MaxUsedChannels = (Capacity & FSM_CAPACITY_CUR) >> 16;

	pr_info("DDBridge: max used chan = %u\n", MaxUsedChannels);
	if (MaxUsedChannels <= 1 )
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

static int mod_set_vga(struct ddb *dev, u32 Gain)
{
	if( Gain > 255 )
		return -EINVAL;
	ddbwritel(dev, Gain, RF_VGA);
	return 0;
}

static int mod_get_vga(struct ddb *dev, u32 *pGain)
{
	*pGain = ddbreadl(dev, RF_VGA);
	return 0;
}
#if 0
static void TemperatureMonitorSetFan(struct ddb *dev)
{
	u32 tqam, pwm;
	
	if ((ddbreadl(dev, TEMPMON_CONTROL) & TEMPMON_CONTROL_OVERTEMP ) != 0) {
		pr_info("DDBridge: Over temperature condition\n");
		dev->OverTemperatureError = 1;
	}
	tqam  = (ddbreadl(dev, TEMPMON2_QAMCORE) >> 8) & 0xFF;
	if (tqam & 0x80)
		tqam = 0;
	
	pwm = (ddbreadl(dev, TEMPMON_FANCONTROL) >> 8) & 0x0F;
	if (pwm > 10)
		pwm = 10;   
	
	if (tqam >= dev->temp_tab[pwm]) {
		while( pwm < 10 && tqam >= dev->temp_tab[pwm + 1])
			pwm += 1;
	} else {
		while( pwm > 1 && tqam < dev->temp_tab[pwm - 2])
			pwm -= 1;
	}
	ddbwritel(dev, (pwm << 8), TEMPMON_FANCONTROL);
}


static void temp_handler(unsigned long data)
{
	struct ddb *dev = (struct ddb *) data;

	pr_info("DDBridge: temp_handler\n");

	spin_lock(&dev->temp_lock);
	TemperatureMonitorSetFan(dev);
	spin_unlock(&dev->temp_lock);
}

static int TemperatureMonitorInit(struct ddb *dev, int FirstTime) {
	int status = 0;
	
	spin_lock_irq(&dev->temp_lock);
	if (FirstTime) {
		static u8 TemperatureTable[11] = {30,35,40,45,50,55,60,65,70,75,80};
		
		memcpy(dev->temp_tab, TemperatureTable, sizeof(TemperatureTable));
	}
	dev->handler[0][8] = temp_handler;
	dev->handler_data[0][8] = (unsigned long) dev;
	ddbwritel(dev, (TEMPMON_CONTROL_OVERTEMP | TEMPMON_CONTROL_AUTOSCAN |
			TEMPMON_CONTROL_INTENABLE),
		  TEMPMON_CONTROL);
	ddbwritel(dev, (3 << 8), TEMPMON_FANCONTROL);
	
	dev->OverTemperatureError =
		((ddbreadl(dev, TEMPMON_CONTROL) & TEMPMON_CONTROL_OVERTEMP ) != 0);
	if (dev->OverTemperatureError)	{
		pr_info("DDBridge: Over temperature condition\n");
		status = -1;
	}
	TemperatureMonitorSetFan(dev);
	spin_unlock_irq(&dev->temp_lock);
	return status;
}
#endif
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
	ddbwritel(dev, Value, RF_ATTENUATOR);
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

		pr_info("DDBridge:  Data = %02x %02x %02x %02x %02x %02x\n",
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
		pr_info("DDBridge: fxtal %016llx  rfreq %016llx\n", m_fXtal, RFreq);

		m_fXtal += RFreq >> 1;
		m_fXtal = div64_u64(m_fXtal, RFreq);

		pr_info("DDBridge: fOut = %d fXtal = %d fDCO = %d HDIV = %2d, N = %3d\n",
			(u32) fOut, (u32) m_fXtal, (u32) fDCO, (u32) HSDiv, N);
	}

	fOut = freq;
	MinDiv = 4850000000ULL; do_div(MinDiv, freq); MinDiv += 1;
	MaxDiv = 5670000000ULL; do_div(MaxDiv, freq);
	Div    = 5260000000ULL; do_div(Div, freq);

	if (Div < MinDiv)
		Div = Div + 1;
	pr_info("DDBridge:  fOut = %u MinDiv = %llu MaxDiv = %llu StartDiv = %llu\n",
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
			pr_info("DDBridge:  %3d: %llu %llu %llu %u\n",
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
			pr_err("DDBridge:  FAIL\n");
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
	pr_info("DDBridge: fdco %16llx\n", fDCO);
	RFreq = fDCO<<28;
	pr_info("DDBridge: %16llx %16llx\n", fDCO, RFreq);

	fxtal = m_fXtal;
	do_div(RFreq, fxtal);
	pr_info("DDBridge: %16llx %d\n", RFreq, fxtal);
	RF = RFreq;

	pr_info("DDBridge: fOut = %u fXtal = %llu fDCO = %llu HSDIV = %llu, N = %u, RFreq = %llu\n",
		fOut, m_fXtal, fDCO, HSDiv, N, RFreq);

	Data[0] = (u8)(((HSDiv - 4) << 5) | ((N - 1) >> 2));
	Data[1] = (u8)((((N - 1) & 0x03) << 6) | ((RF >> 32) & 0x3F));
	Data[2] = (u8)((RF >> 24) & 0xFF);
	Data[3] = (u8)((RF >> 16) & 0xFF);
	Data[4] = (u8)((RF >>  8) & 0xFF);
	Data[5] = (u8)((RF)       & 0xFF);

	pr_info("DDBridge:  Data = %02x %02x %02x %02x %02x %02x\n",
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
		   Sample, SeekTable[Sample], SetTable[Sample],
		   HldTable[Sample]);
		*/

		if (Sample1 == 0xFF && SeekTable[Sample] == 1 && Seek == 0)
			Sample1 = Sample;
		if (Sample1 != 0xFF && Sample2 == 0xFF &&
		    SeekTable[Sample] == 0 && Seek == 1)
			Sample2 = Sample;
		Seek = SeekTable[Sample];
	}

	if (Sample1 == 0xFF || Sample2 == 0xFF) {
		pr_err("DDBridge:  No valid window found\n");
		return -EINVAL;
	}

	pr_err("DDBridge:  Window = %d - %d\n", Sample1, Sample2);

	for (Sample = Sample1; Sample < Sample2; Sample += 1) {
		if (SetTable[Sample] < HldTable[Sample]) {
			if (HldTable[Sample] - SetTable[Sample] < DiffMin) {
				DiffMin = HldTable[Sample] - SetTable[Sample];
				SelectSample = Sample;
			}
		}
	}

	pr_info("DDBridge: Select Sample %d\n", SelectSample);

	if (SelectSample == 0xFF) {
		pr_err("DDBridge: No valid sample found\n");
		return -EINVAL;
	}

	if (HldTable[SelectSample] + SetTable[SelectSample] < 8) {
		pr_err("DDBridge: Too high jitter\n");
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
		pr_err("DDBridge: Insufficient timing margin\n");
		return -EINVAL;
	}
	pr_info("DDBridge: Done\n");
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
			pr_err("DDBridge: mod_set_si598 failed\n");
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
	pr_info("DDBridge: mod_set_dac_clock OK\n");
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

	pr_info("DDBridge: set base to %u\n", freq);
	dev->mod_base.frequency = freq;
	freq /= 1000000;
	freq10 = dev->mod_base.flat_start + 4;
	down = freq + 9 * 8 + freq10 + UP1Frequency + UP2Frequency;

	if ((freq10 + 9 * 8) > (dev->mod_base.flat_end - 4)) {
		pr_err("DDBridge: Frequency out of range %d\n", freq10);
		return -EINVAL;
	}
	if (down % 8) {
		pr_err("DDBridge:  Invalid Frequency %d\n", down);
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
	pr_info("DDBridge: srate = %d\n", flash->DataSet[0].Symbolrate * 1000);

	mod_output_enable(dev, 0);
	stat = mod_set_dac_clock(dev, flash->DataSet[0].DACFrequency * 1000);
	if (stat < 0) {
		pr_err("DDBridge: setting DAC clock failed\n");
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
	pr_info("DDBridge: CH10 = %d, Down = %d\n", FrequencyCH10, DownFrequency);

	if ((FrequencyCH10 + 9 * 8) > (flash->DataSet[0].FlatEnd - 4)) {
		pr_err("DDBridge: Frequency out of range %d\n", FrequencyCH10);
		stat = -EINVAL;
		goto fail;
	}

	if (DownFrequency % 8 != 0) {
		pr_err("DDBridge:  Invalid Frequency %d\n", DownFrequency);
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
	pr_info("DDBridge: prescaler %04x\n", flash->DataSet[0].PreScale);
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
  double Increment =  FACTOR*PACKET_CLOCKS/double(m_OutputBitrate);
  double Decrement =  FACTOR*PACKET_CLOCKS/double(m_InputBitrate);
  27000000 * 1504 * 2^22 / (6900000 * 188 / 204) = 26785190066.1
*/

void ddbridge_mod_rate_handler(unsigned long data)
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
			pr_info("DDBridge: PCR Adjust reset  IN: %u  Min: %u\n",
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
		pr_info("DDBridge: outl %016llx\n", pcr);
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

	pr_info("DDBridge: chan %d out %016llx in %016llx indiff %08x\n",
		chan, OutPackets, InPackets, InPacketDiff);
	pr_info("DDBridge: cnt  %d pcra %016llx pcraext %08x pcraextfrac %08x pcrcorr %08x pcri %016llx\n",
		mod->StateCounter, PCRAdjust, PCRAdjustExt,
		PCRAdjustExtFrac, PCRCorr, mod->PCRIncrement);
}

static int mod_prop_proc(struct ddb_mod *mod, struct dtv_property *tvp)
{
	struct ddb *dev = mod->port->dev;

	switch(tvp->cmd) {
	case MODULATOR_SYMBOL_RATE:
		return mod_set_symbolrate(mod, tvp->u.data);

	case MODULATOR_MODULATION:
		return mod_set_modulation(mod, tvp->u.data);

	case MODULATOR_FREQUENCY:
		return mod_set_frequency(mod, tvp->u.data);

	case MODULATOR_ATTENUATOR:
		return mod_set_attenuator(mod->port->dev, tvp->u.data);

	case MODULATOR_INPUT_BITRATE:
		return mod_set_ibitrate(mod, tvp->u.data);
	}
	return 0;
}

int ddbridge_mod_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb *dev = output->port->dev;
	struct ddb_mod *mod = &dev->mod[output->nr];
	int ret = 0;

	switch (cmd) {
	case FE_SET_PROPERTY:
	{
		struct dtv_properties *tvps = (struct dtv_properties __user *) parg;
		struct dtv_property *tvp = NULL;
		int i;
		
		if ((tvps->num == 0) || (tvps->num > DTV_IOCTL_MAX_MSGS))
			return -EINVAL;
		
		tvp = kmalloc(tvps->num * sizeof(struct dtv_property), GFP_KERNEL);
		if (!tvp) {
			ret = -ENOMEM;
			goto out;
		}
		if (copy_from_user(tvp, tvps->props, tvps->num *
				   sizeof(struct dtv_property))) {
			ret = -EFAULT;
			goto out;
		}
		for (i = 0; i < tvps->num; i++) {
			if ((ret = mod_prop_proc(mod, tvp + i)) < 0)
				goto out;
			(tvp + i)->result = ret;
		}
	out:
		kfree(tvp);
		return ret;
	}
	case DVB_MOD_SET:
	{
		struct dvb_mod_params *mp = parg;

		if (dev->link[0].info->version < 2) {
			if (mp->base_frequency != dev->mod_base.frequency)
				if (set_base_frequency(dev, mp->base_frequency))
					return -EINVAL;
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
		if (res)
			return res;
		res = mod_set_ibitrate(mod, cp->input_bitrate);
		if (res)
			return res;
		mod->pcr_correction = cp->pcr_correction;
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mod_init_2(struct ddb *dev, u32 Frequency)
{
	int status, i;
	int streams = dev->link[0].info->port_num;

	dev->mod_base.frequency = Frequency;
	status = mod_fsm_setup(dev, 0, 0);

	for (i = 0; i < streams; i++) {
		struct ddb_mod *mod = &dev->mod[i];

		mod->port = &dev->port[i];
		mod_set_modulation(mod, QAM_256);
		mod_set_symbolrate(mod, 6900000);
		mod_set_frequency(mod, 114000000 + i * 8000000);
	}
	if (streams <= 8)
		mod_set_vga(dev, RF_VGA_GAIN_N8);
	else if (streams <= 16)
		mod_set_vga(dev, RF_VGA_GAIN_N16);
	else
		mod_set_vga(dev, RF_VGA_GAIN_N24);
	
	mod_set_attenuator(dev, 0);
	return 0;
}

int ddbridge_mod_init(struct ddb *dev)
{
	if (dev->link[0].info->version <= 1)
		return mod_init_1(dev, 722000000);
	if (dev->link[0].info->version == 2)
		return mod_init_2(dev, 114000000);
	return -1;
}
