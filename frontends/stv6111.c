/*
 * Driver for the ST STV6111 tuner
 *
 * Copyright (C) 2014 Digital Devices GmbH
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

static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = (u64)a * (u64)b;
	do_div(tmp64, c);

	return (u32) tmp64;
}


struct stv {
	struct i2c_adapter *i2c;
	u8 adr;

	u8 reg[11];
	u32 ref_freq;
	u32 Frequency;
};

static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	struct i2c_msg msgs[2] = { { .addr = adr, .flags = 0,
				     .buf = msg, .len = len},
				   { .addr = adr, .flags = I2C_M_RD,
				     .buf = answ, .len = alen } };
	if (i2c_transfer(adap, msgs, 2) != 2) {
		pr_err("stv6111: i2c_read error\n");
		return -1;
	}
	return 0;
}

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	if (i2c_transfer(adap, &msg, 1) != 1) {
		pr_err("stv6111: i2c_write error\n");
		return -1;
	}
	return 0;
}

static int write_regs(struct stv *state, int reg, int len)
{
	u8 d[12];

	memcpy(&d[1], &state->reg[reg], len);
	d[0] = reg;
	return i2c_write(state->i2c, state->adr, d, len + 1);
}

static int write_reg(struct stv *state, u8 reg, u8 val)
{
	u8 d[2] = {reg, val};

	return i2c_write(state->i2c, state->adr, d, 2);
}

static int read_reg(struct stv *state, u8 reg, u8 *val)
{
	return i2c_read(state->i2c, state->adr, &reg, 1, val, 1);
}

static int read_regs(struct stv *state, u8 reg, u8 *val, int len)
{
	return i2c_read(state->i2c, state->adr, &reg, 1, val, len);
}

#if 0
static void dump_regs(struct stv *state)
{
	u8 d[11], *c = &state->reg[0];

	read_regs(state, 0, d, 11);
	pr_info("stv6111_regs = %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x\n",
		d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
		d[8], d[9], d[10]);
	pr_info("reg[] =        %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x\n",
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10]);
}
#endif

static int wait_for_call_done(struct stv *state, u8 mask)
{
	int status = 0;
	u32 LockRetryCount = 10;

	while (LockRetryCount > 0) {
		u8 Status;

		status = read_reg(state, 9, &Status);
		if (status < 0)
			return status;

		if ((Status & mask) == 0)
			break;
		usleep_range(4000, 6000);
		LockRetryCount -= 1;

		status = -1;
	}
	return status;
}

static void init_state(struct stv *state)
{
	u32 clkdiv = 0;
	u32 agcmode = 0;
	u32 agcref = 2;
	u32 agcset = 0xffffffff;
	u32 bbmode = 0xffffffff;

	state->reg[0] = 0x08;
	state->reg[1] = 0x41;
	state->reg[2] = 0x8f;
	state->reg[3] = 0x00;
	state->reg[4] = 0xce;
	state->reg[5] = 0x54;
	state->reg[6] = 0x55;
	state->reg[7] = 0x45;
	state->reg[8] = 0x46;
	state->reg[9] = 0xbd;
	state->reg[10] = 0x11;

	state->ref_freq = 16000;


	if (clkdiv <= 3)
		state->reg[0x00] |= (clkdiv & 0x03);
	if (agcmode <= 3) {
		state->reg[0x03] |= (agcmode << 5);
		if (agcmode == 0x01)
			state->reg[0x01] |= 0x30;
	}
	if (bbmode <= 3)
		state->reg[0x01] = (state->reg[0x01] & ~0x30) | (bbmode << 4);
	if (agcref <= 7)
		state->reg[0x03] |= agcref;
	if (agcset <= 31)
		state->reg[0x02] = (state->reg[0x02] & ~0x1F) | agcset | 0x40;
}

static int attach_init(struct stv *state)
{
	if (write_regs(state, 0, 11))
		return -1;
#if 0
	dump_regs(state);
#endif
	return 0;
}

static int sleep(struct dvb_frontend *fe)
{
	/* struct tda_state *state = fe->tuner_priv; */

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

static int set_bandwidth(struct dvb_frontend *fe, u32 CutOffFrequency)
{
	struct stv *state = fe->tuner_priv;
	u32 index = (CutOffFrequency + 999999) / 1000000;

	if (index < 6)
		index = 6;
	if (index > 50)
		index = 50;
	if ((state->reg[0x08] & ~0xFC) == ((index-6) << 2))
		return 0;

	state->reg[0x08] = (state->reg[0x08] & ~0xFC) | ((index-6) << 2);
	state->reg[0x09] = (state->reg[0x09] & ~0x0C) | 0x08;
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	write_regs(state, 0x08, 2);
	wait_for_call_done(state, 0x08);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
	return 0;
}

static int set_lof(struct stv *state, u32 LocalFrequency, u32 CutOffFrequency)
{
	u32 index = (CutOffFrequency + 999999) / 1000000;
	u32 Frequency = (LocalFrequency + 500) / 1000;
	u32 p = 1, psel = 0, fvco, div, frac;
	u8 Icp, tmp;

	/* pr_info("F = %u, COF = %u\n", Frequency, CutOffFrequency); */
	if (index < 6)
		index = 6;
	if (index > 50)
		index = 50;

	if (Frequency <= 1300000) {
		p =  4;
		psel = 1;
	} else {
		p =  2;
		psel = 0;
	}
	fvco = Frequency * p;
	div = fvco / state->ref_freq;
	frac = fvco % state->ref_freq;
	frac = MulDiv32(frac, 0x40000, state->ref_freq);

	Icp = 0;
	if (fvco < 2700000)
		Icp = 0;
	else if (fvco < 2950000)
		Icp = 1;
	else if (fvco < 3300000)
		Icp = 2;
	else if (fvco < 3700000)
		Icp = 3;
	else if (fvco < 4200000)
		Icp = 5;
	else if (fvco < 4800000)
		Icp = 6;
	else
		Icp = 7;

	state->reg[0x02] |= 0x80;   /* LNA IIP3 Mode */

	state->reg[0x03] = (state->reg[0x03] & ~0x80) | (psel << 7);
	state->reg[0x04] = (div & 0xFF);
	state->reg[0x05] = (((div >> 8) & 0x01) | ((frac & 0x7F) << 1)) & 0xff;
	state->reg[0x06] = ((frac >> 7) & 0xFF);
	state->reg[0x07] = (state->reg[0x07] & ~0x07) | ((frac >> 15) & 0x07);
	state->reg[0x07] = (state->reg[0x07] & ~0xE0) | (Icp << 5);

	state->reg[0x08] = (state->reg[0x08] & ~0xFC) | ((index - 6) << 2);
	/* Start cal vco,CF */
	state->reg[0x09] = (state->reg[0x09] & ~0x0C) | 0x0C;
	write_regs(state, 2, 8);

	wait_for_call_done(state, 0x0C);

	usleep_range(10000, 12000);

	read_reg(state, 0x03, &tmp);
	if (tmp & 0x10)	{
		state->reg[0x02] &= ~0x80;   /* LNA NF Mode */
		write_regs(state, 2, 1);
	}
	read_reg(state, 0x08, &tmp);

	state->Frequency = Frequency;
	
#if 0
	dump_regs(state);
#endif
	return 0;
}

static int set_params(struct dvb_frontend *fe)
{
	struct stv *state = fe->tuner_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 freq, symb, cutoff;

	if (p->delivery_system != SYS_DVBS && p->delivery_system != SYS_DVBS2)
		return -EINVAL;

	freq = p->frequency * 1000;
	symb = p->symbol_rate;
	cutoff = 5000000 + MulDiv32(p->symbol_rate, 135, 200);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	set_lof(state, freq, cutoff);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);
	return 0;
}

static int get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	*frequency = 0;
	return 0;
}

struct SLookup {
	s16 Value;
	u16 RegValue;
};


static struct SLookup LNAGain_NF_LookUp[] = {
	/*Gain *100dB*/      /*Reg*/   
	{ 2572  , 0   },
	{ 2575  , 1   },
	{ 2580  , 2   },
	{ 2588  , 3   },
	{ 2596  , 4   },
	{ 2611  , 5   },
	{ 2633  , 6   },
	{ 2664  , 7   },
	{ 2701  , 8   },
	{ 2753  , 9   },
	{ 2816  , 10  },
	{ 2902  , 11  },
	{ 2995  , 12  },
	{ 3104  , 13  },
	{ 3215  , 14  },
	{ 3337  , 15  },
	{ 3492  , 16  },
	{ 3614  , 17  },
	{ 3731  , 18  },
	{ 3861  , 19  },
	{ 3988  , 20  },
	{ 4124  , 21  },
	{ 4253  , 22  },
	{ 4386  , 23  },
	{ 4505  , 24  },
	{ 4623  , 25  },
	{ 4726  , 26  },
	{ 4821  , 27  },
	{ 4903  , 28  },
	{ 4979  , 29  },
	{ 5045  , 30  },
	{ 5102  , 31  }
};
                                                
static struct SLookup LNAGain_IIP3_LookUp[] = {
	/*Gain *100dB*/   /*reg*/
	{ 1548 , 0   },
	{ 1552 , 1   },
	{ 1569 , 2   },
	{ 1565 , 3   },
	{ 1577 , 4   },
	{ 1594 , 5   },
	{ 1627 , 6   },
	{ 1656 , 7   },
	{ 1700 , 8   },
	{ 1748 , 9   },
	{ 1805 , 10  },
	{ 1896 , 11  },
	{ 1995 , 12  },
	{ 2113 , 13  },
	{ 2233 , 14  },
	{ 2366 , 15  },
	{ 2543 , 16  },
	{ 2687 , 17  },
	{ 2842 , 18  },
	{ 2999 , 19  },
	{ 3167 , 20  },
	{ 3342 , 21  },
	{ 3507 , 22  },
	{ 3679 , 23  },
	{ 3827 , 24  },
	{ 3970 , 25  },
	{ 4094 , 26  },
	{ 4210 , 27  },
	{ 4308 , 28  },
	{ 4396 , 29  },
	{ 4468 , 30  },
	{ 4535 , 31  }
};

static struct SLookup Gain_RFAGC_LookUp[] = {                
	/*Gain *100dB*/   /*reg*/
	{  4870  , 0x3000 },
	{  4850  , 0x3C00 },
	{  4800  , 0x4500 },
	{  4750  , 0x4800 },
	{  4700  , 0x4B00 },
	{  4650  , 0x4D00 },
	{  4600  , 0x4F00 },
	{  4550  , 0x5100 },
	{  4500  , 0x5200 },
	{  4420  , 0x5500 },
	{  4316  , 0x5800 },
	{  4200  , 0x5B00 },
	{  4119  , 0x5D00 },
	{  3999  , 0x6000 },
	{  3950  , 0x6100 },
	{  3876  , 0x6300 },
	{  3755  , 0x6600 },
	{  3641  , 0x6900 },
	{  3567  , 0x6B00 },
	{  3425  , 0x6F00 },
	{  3350  , 0x7100 },
	{  3236  , 0x7400 },
	{  3118  , 0x7700 },
	{  3004  , 0x7A00 },
	{  2917  , 0x7C00 },
	{  2776  , 0x7F00 },
	{  2635  , 0x8200 },
	{  2516  , 0x8500 },
	{  2406  , 0x8800 },
	{  2290  , 0x8B00 },
	{  2170  , 0x8E00 },
	{  2073  , 0x9100 },
	{  1949  , 0x9400 },
	{  1836  , 0x9700 },
	{  1712  , 0x9A00 },
	{  1631  , 0x9C00 },
	{  1515  , 0x9F00 },
	{  1400  , 0xA200 },
	{  1323  , 0xA400 },
	{  1203  , 0xA700 },
	{  1091  , 0xAA00 },
	{  1011  , 0xAC00 },
	{  904   , 0xAF00 },
	{  787   , 0xB200 },
	{  685   , 0xB500 },
	{  571   , 0xB800 },
	{  464   , 0xBB00 },
	{  374   , 0xBE00 },
	{  275   , 0xC200 },
	{  181   , 0xC600 },
	{  102   , 0xCC00 },
	{  49    , 0xD900 }
};

                                              
// This table is 6 dB too low comapred to the others (probably created with a different BB_MAG setting)
static struct SLookup Gain_Channel_AGC_NF_LookUp[] = {                
	/*Gain *100dB*/   /*reg*/     
	{  7082  ,  0x3000  },
	{  7052  ,  0x4000  },
	{  7007  ,  0x4600  },
	{  6954  ,  0x4A00  },
	{  6909  ,  0x4D00  },
	{  6833  ,  0x5100  },
	{  6753  ,  0x5400  },
	{  6659  ,  0x5700  },
	{  6561  ,  0x5A00  },
	{  6472  ,  0x5C00  },
	{  6366  ,  0x5F00  },
	{  6259  ,  0x6100  },
	{  6151  ,  0x6400  },
	{  6026  ,  0x6700  },
	{  5920  ,  0x6900  },
	{  5835  ,  0x6B00  },
	{  5770  ,  0x6C00  },
	{  5681  ,  0x6E00  },
	{  5596  ,  0x7000  },
	{  5503  ,  0x7200  },
	{  5429  ,  0x7300  },
	{  5319  ,  0x7500  },
	{  5220  ,  0x7700  },
	{  5111  ,  0x7900  },
	{  4983  ,  0x7B00  },
	{  4876  ,  0x7D00  },
	{  4755  ,  0x7F00  },
	{  4635  ,  0x8100  },
	{  4499  ,  0x8300  },
	{  4405  ,  0x8500  },
	{  4323  ,  0x8600  },
	{  4233  ,  0x8800  },
	{  4156  ,  0x8A00  },
	{  4038  ,  0x8C00  },
	{  3935  ,  0x8E00  },
	{  3823  ,  0x9000  },
	{  3712  ,  0x9200  },
	{  3601  ,  0x9500  },
	{  3511  ,  0x9700  },
	{  3413  ,  0x9900  },
	{  3309  ,  0x9B00  },
	{  3213  ,  0x9D00  },
	{  3088  ,  0x9F00  },
	{  2992  ,  0xA100  },
	{  2878  ,  0xA400  },
	{  2769  ,  0xA700  },
	{  2645  ,  0xAA00  },
	{  2538  ,  0xAD00  },
	{  2441  ,  0xB000  },
	{  2350  ,  0xB600  },
	{  2237  ,  0xBA00  },
	{  2137  ,  0xBF00  },
	{  2039  ,  0xC500  },
	{  1938  ,  0xDF00  },
	{  1927  ,  0xFF00  }
};

  
static struct SLookup Gain_Channel_AGC_IIP3_LookUp[] = {                
	/*Gain *100dB*/   /*reg*/     
	{ 7070 , 0x3000 },
	{ 7028 , 0x4000 },
	{ 7019 , 0x4600 },
	{ 6900 , 0x4A00 },
	{ 6811 , 0x4D00 },
	{ 6763 , 0x5100 },
	{ 6690 , 0x5400 },
	{ 6644 , 0x5700 },
	{ 6617 , 0x5A00 },
	{ 6598 , 0x5C00 },
	{ 6462 , 0x5F00 },
	{ 6348 , 0x6100 },
	{ 6197 , 0x6400 },
	{ 6154 , 0x6700 },
	{ 6098 , 0x6900 },
	{ 5893 , 0x6B00 },
	{ 5812 , 0x6C00 },
	{ 5773 , 0x6E00 },
	{ 5723 , 0x7000 },
	{ 5661 , 0x7200 },
	{ 5579 , 0x7300 },
	{ 5460 , 0x7500 },
	{ 5308 , 0x7700 },
	{ 5099 , 0x7900 },
	{ 4910 , 0x7B00 }, 
	{ 4800 , 0x7D00 }, 
	{ 4785 , 0x7F00 },
	{ 4635 , 0x8100 },
	{ 4466 , 0x8300 },
	{ 4314 , 0x8500 },
	{ 4295 , 0x8600 },
	{ 4144 , 0x8800 },
	{ 3920 , 0x8A00 },
	{ 3889 , 0x8C00 },
	{ 3771 , 0x8E00 },
	{ 3655 , 0x9000 },
	{ 3446 , 0x9200 },
	{ 3298 , 0x9500 },
	{ 3083 , 0x9700 },
	{ 3015 , 0x9900 },
	{ 2833 , 0x9B00 },
	{ 2746 , 0x9D00 },
	{ 2632 , 0x9F00 },
	{ 2598 , 0xA100 },
	{ 2480 , 0xA400 },
	{ 2236 , 0xA700 },
	{ 2171 , 0xAA00 },
	{ 2060 , 0xAD00 },
	{ 1999 , 0xB000 },
	{ 1974 , 0xB600 },
	{ 1820 , 0xBA00 },
	{ 1741 , 0xBF00 },
	{ 1655 , 0xC500 },
	{ 1444 , 0xDF00 },
	{ 1325 , 0xFF00 },
};


static s32 TableLookup(struct SLookup *Table, int TableSize, u16 RegValue)
{
	s32 Gain;
	s32 RegDiff;
	int imin = 0;
	int imax = TableSize - 1;
	int i;
	
	// Assumes Table[0].RegValue < Table[imax].RegValue 
	if( RegValue <= Table[0].RegValue )
		Gain = Table[0].Value;
	else if( RegValue >= Table[imax].RegValue )
		Gain = Table[imax].Value;
	else {
		while(imax-imin > 1) {
			i = (imax + imin) / 2;
			if ((Table[imin].RegValue <= RegValue) &&
			    (RegValue <= Table[i].RegValue) )
				imax = i;
			else
				imin = i;
		}
		RegDiff = Table[imax].RegValue - Table[imin].RegValue;
		Gain = Table[imin].Value;
		if (RegDiff != 0)
			Gain += ((s32) (RegValue - Table[imin].RegValue) *
			    (s32)(Table[imax].Value - Table[imin].Value))/(RegDiff);
	}
	return Gain;
}

static int get_rf_strength(struct dvb_frontend *fe, u16 *st)
{
	struct stv *state = fe->tuner_priv;
	u16 RFAgc = *st;
	s32 Gain;

	if ((state->reg[0x03] & 0x60) == 0 ) {
		// RF Mode
		// Read AGC ADC
		u8 Reg = 0;
		
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 1);
		write_reg(state, 0x02, state->reg[0x02] | 0x20); 
		read_reg(state, 2, &Reg);
		if( Reg & 0x20 )
			read_reg(state, 2, &Reg);
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 0);
		
		if((state->reg[0x02] & 0x80) == 0)
			// NF
			Gain = TableLookup(LNAGain_NF_LookUp,
					   ARRAY_SIZE(LNAGain_NF_LookUp), Reg & 0x1F);
		else
			// IIP3
			Gain = TableLookup(LNAGain_IIP3_LookUp,
					   ARRAY_SIZE(LNAGain_IIP3_LookUp), Reg & 0x1F);
		Gain += TableLookup(Gain_RFAGC_LookUp,
				    ARRAY_SIZE(Gain_RFAGC_LookUp), RFAgc);
		Gain -= 2400;
	} else {
		// Channel Mode
		if( (state->reg[0x02] & 0x80) == 0 ) {
			// NF
			Gain = TableLookup(Gain_Channel_AGC_NF_LookUp,
					   ARRAY_SIZE(Gain_Channel_AGC_NF_LookUp), RFAgc);
			Gain += 600;
		} else {
			// IIP3
			Gain = TableLookup(Gain_Channel_AGC_IIP3_LookUp,
					   ARRAY_SIZE(Gain_Channel_AGC_IIP3_LookUp), RFAgc);
		}
	}
	
	if (state->Frequency > 0)
		// Tilt correction ( 0.00016 dB/MHz )
		Gain -= ((((s32)(state->Frequency / 1000) - 1550) * 2) / 12);
	
	Gain +=  (s32)( (state->reg[0x01] & 0xC0 ) >> 6 ) * 600 - 1300;// + (BBGain * 10);
	
	if( Gain < 0 )
		Gain = 0;
	else if (Gain > 10000)
		Gain = 10000;
	
	*st = 10000 - Gain;

	return 0;
}

static int get_if(struct dvb_frontend *fe, u32 *frequency)
{
	*frequency = 0;
	return 0;
}

static int get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	return 0;
}

static struct dvb_tuner_ops tuner_ops = {
	.info = {
		.name = "STV6111",
		.frequency_min  =  950000,
		.frequency_max  = 2150000,
		.frequency_step =       0
	},
	.init              = init,
	.sleep             = sleep,
	.set_params        = set_params,
	.release           = release,
	.get_frequency     = get_frequency,
	.get_if_frequency  = get_if,
	.get_bandwidth     = get_bandwidth,
	.get_rf_strength   = get_rf_strength,
	.set_bandwidth     = set_bandwidth,
};

struct dvb_frontend *stv6111_attach(struct dvb_frontend *fe,
				    struct i2c_adapter *i2c, u8 adr)
{
	struct stv *state;
	int stat;

	state = kzalloc(sizeof(struct stv), GFP_KERNEL);
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
		return NULL;
	}
	fe->tuner_priv = state;
	return fe;
}
EXPORT_SYMBOL_GPL(stv6111_attach);

MODULE_DESCRIPTION("STV6111 driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL");

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
