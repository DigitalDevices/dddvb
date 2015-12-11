/*
 * Driver for the ST STV0910 DVB-S/S2 demodulator.
 *
 * Copyright (C) 2014-2015 Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         developed for Digital Devices GmbH
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
#include "stv0910.h"
#include "stv0910_regs.h"


#define TUNING_DELAY    200
#define BER_SRC_S    0x20
#define BER_SRC_S2   0x20

LIST_HEAD(stvlist);

enum ReceiveMode { Mode_None, Mode_DVBS, Mode_DVBS2, Mode_Auto };


enum DVBS2_FECType { DVBS2_64K, DVBS2_16K };

enum DVBS2_ModCod {
	DVBS2_DUMMY_PLF, DVBS2_QPSK_1_4, DVBS2_QPSK_1_3, DVBS2_QPSK_2_5,
	DVBS2_QPSK_1_2, DVBS2_QPSK_3_5, DVBS2_QPSK_2_3,	DVBS2_QPSK_3_4,
	DVBS2_QPSK_4_5,	DVBS2_QPSK_5_6,	DVBS2_QPSK_8_9,	DVBS2_QPSK_9_10,
	DVBS2_8PSK_3_5,	DVBS2_8PSK_2_3,	DVBS2_8PSK_3_4,	DVBS2_8PSK_5_6,
	DVBS2_8PSK_8_9,	DVBS2_8PSK_9_10, DVBS2_16APSK_2_3, DVBS2_16APSK_3_4,
	DVBS2_16APSK_4_5, DVBS2_16APSK_5_6, DVBS2_16APSK_8_9, DVBS2_16APSK_9_10,
	DVBS2_32APSK_3_4, DVBS2_32APSK_4_5, DVBS2_32APSK_5_6, DVBS2_32APSK_8_9,
	DVBS2_32APSK_9_10
};

enum FE_STV0910_ModCod {
	FE_DUMMY_PLF, FE_QPSK_14, FE_QPSK_13, FE_QPSK_25,
	FE_QPSK_12, FE_QPSK_35, FE_QPSK_23, FE_QPSK_34,
	FE_QPSK_45, FE_QPSK_56, FE_QPSK_89, FE_QPSK_910,
	FE_8PSK_35, FE_8PSK_23, FE_8PSK_34, FE_8PSK_56,
	FE_8PSK_89, FE_8PSK_910, FE_16APSK_23, FE_16APSK_34,
	FE_16APSK_45, FE_16APSK_56, FE_16APSK_89, FE_16APSK_910,
	FE_32APSK_34, FE_32APSK_45, FE_32APSK_56, FE_32APSK_89,
	FE_32APSK_910
};

enum FE_STV0910_RollOff { FE_SAT_35, FE_SAT_25, FE_SAT_20, FE_SAT_15 };

static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = (u64)a * (u64)b;
	do_div(tmp64, c);

	return (u32) tmp64;
}

struct stv_base {
	struct list_head     stvlist;

	u8                   adr;
	struct i2c_adapter  *i2c;
	struct mutex         i2c_lock;
	struct mutex         reg_lock;
	int                  count;

	u32                  extclk;
	u32                  mclk;
};

struct stv {
	struct stv_base     *base;
	struct dvb_frontend  fe;
	int                  nr;
	u16                  regoff;
	u8                   i2crpt;
	u8                   tscfgh;
	u8                   tsgeneral;
	u8                   tsspeed;
	unsigned long        tune_time;

	s32                  SearchRange;
	u32                  Started;
	u32                  DemodLockTime;
	enum ReceiveMode     ReceiveMode;
	u32                  DemodTimeout;
	u32                  FecTimeout;
	u32                  FirstTimeLock;
	u8                   DEMOD;
	u32                  SymbolRate;

	u8                      LastViterbiRate;
	enum fe_code_rate       PunctureRate;
	enum FE_STV0910_ModCod  ModCod;
	enum DVBS2_FECType      FECType;
	u32                     Pilots;
	enum FE_STV0910_RollOff FERollOff;

	u32   LastBERNumerator;
	u32   LastBERDenominator;
	u8    BERScale;
};

struct SInitTable {
	u16  Address;
	u8   Data;
};

struct SLookupSNTable {
	s16  SignalToNoise;
	u16  RefValue;
};

static inline int i2c_write(struct i2c_adapter *adap, u8 adr,
			    u8 *data, int len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	return (i2c_transfer(adap, &msg, 1) == 1) ? 0 : -1;
}

static int i2c_write_reg16(struct i2c_adapter *adap, u8 adr, u16 reg, u8 val)
{
	u8 msg[3] = {reg >> 8, reg & 0xff, val};

	return i2c_write(adap, adr, msg, 3);
}

static int write_reg(struct stv *state, u16 reg, u8 val)
{
	return i2c_write_reg16(state->base->i2c, state->base->adr, reg, val);
}

static inline int i2c_read_reg16(struct i2c_adapter *adapter, u8 adr,
				 u16 reg, u8 *val)
{
	u8 msg[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msgs[2] = {{.addr = adr, .flags = 0,
				   .buf  = msg, .len   = 2},
				  {.addr = adr, .flags = I2C_M_RD,
				   .buf  = val, .len   = 1 } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int read_reg(struct stv *state, u16 reg, u8 *val)
{
	return i2c_read_reg16(state->base->i2c, state->base->adr, reg, val);
}


static inline int i2c_read_regs16(struct i2c_adapter *adapter, u8 adr,
				  u16 reg, u8 *val, int len)
{
	u8 msg[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msgs[2] = {{.addr = adr, .flags = 0,
				   .buf  = msg, .len   = 2},
				  {.addr = adr, .flags = I2C_M_RD,
				   .buf  = val, .len   = len } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int read_regs(struct stv *state, u16 reg, u8 *val, int len)
{
	return i2c_read_regs16(state->base->i2c, state->base->adr,
			       reg, val, len);
}

struct SLookupSNTable S1_SN_Lookup[] = {
	{   0,    9242  },  /*C/N=  0dB*/
	{  05,    9105  },  /*C/N=0.5dB*/
	{  10,    8950  },  /*C/N=1.0dB*/
	{  15,    8780  },  /*C/N=1.5dB*/
	{  20,    8566  },  /*C/N=2.0dB*/
	{  25,    8366  },  /*C/N=2.5dB*/
	{  30,    8146  },  /*C/N=3.0dB*/
	{  35,    7908  },  /*C/N=3.5dB*/
	{  40,    7666  },  /*C/N=4.0dB*/
	{  45,    7405  },  /*C/N=4.5dB*/
	{  50,    7136  },  /*C/N=5.0dB*/
	{  55,    6861  },  /*C/N=5.5dB*/
	{  60,    6576  },  /*C/N=6.0dB*/
	{  65,    6330  },  /*C/N=6.5dB*/
	{  70,    6048  },  /*C/N=7.0dB*/
	{  75,    5768  },  /*C/N=7.5dB*/
	{  80,    5492  },  /*C/N=8.0dB*/
	{  85,    5224  },  /*C/N=8.5dB*/
	{  90,    4959  },  /*C/N=9.0dB*/
	{  95,    4709  },  /*C/N=9.5dB*/
	{  100,   4467  },  /*C/N=10.0dB*/
	{  105,   4236  },  /*C/N=10.5dB*/
	{  110,   4013  },  /*C/N=11.0dB*/
	{  115,   3800  },  /*C/N=11.5dB*/
	{  120,   3598  },  /*C/N=12.0dB*/
	{  125,   3406  },  /*C/N=12.5dB*/
	{  130,   3225  },  /*C/N=13.0dB*/
	{  135,   3052  },  /*C/N=13.5dB*/
	{  140,   2889  },  /*C/N=14.0dB*/
	{  145,   2733  },  /*C/N=14.5dB*/
	{  150,   2587  },  /*C/N=15.0dB*/
	{  160,   2318  },  /*C/N=16.0dB*/
	{  170,   2077  },  /*C/N=17.0dB*/
	{  180,   1862  },  /*C/N=18.0dB*/
	{  190,   1670  },  /*C/N=19.0dB*/
	{  200,   1499  },  /*C/N=20.0dB*/
	{  210,   1347  },  /*C/N=21.0dB*/
	{  220,   1213  },  /*C/N=22.0dB*/
	{  230,   1095  },  /*C/N=23.0dB*/
	{  240,    992  },  /*C/N=24.0dB*/
	{  250,    900  },  /*C/N=25.0dB*/
	{  260,    826  },  /*C/N=26.0dB*/
	{  270,    758  },  /*C/N=27.0dB*/
	{  280,    702  },  /*C/N=28.0dB*/
	{  290,    653  },  /*C/N=29.0dB*/
	{  300,    613  },  /*C/N=30.0dB*/
	{  310,    579  },  /*C/N=31.0dB*/
	{  320,    550  },  /*C/N=32.0dB*/
	{  330,    526  },  /*C/N=33.0dB*/
	{  350,    490  },  /*C/N=33.0dB*/
	{  400,    445  },  /*C/N=40.0dB*/
	{  450,    430  },  /*C/N=45.0dB*/
	{  500,    426  },  /*C/N=50.0dB*/
	{  510,    425  }   /*C/N=51.0dB*/
};

struct SLookupSNTable S2_SN_Lookup[] = {
	{  -30,  13950  },  /*C/N=-2.5dB*/
	{  -25,  13580  },  /*C/N=-2.5dB*/
	{  -20,  13150  },  /*C/N=-2.0dB*/
	{  -15,  12760  },  /*C/N=-1.5dB*/
	{  -10,  12345  },  /*C/N=-1.0dB*/
	{  -05,  11900  },  /*C/N=-0.5dB*/
	{    0,  11520  },  /*C/N=   0dB*/
	{   05,  11080  },  /*C/N= 0.5dB*/
	{   10,  10630  },  /*C/N= 1.0dB*/
	{   15,  10210  },  /*C/N= 1.5dB*/
	{   20,   9790  },  /*C/N= 2.0dB*/
	{   25,   9390  },  /*C/N= 2.5dB*/
	{   30,   8970  },  /*C/N= 3.0dB*/
	{   35,   8575  },  /*C/N= 3.5dB*/
	{   40,   8180  },  /*C/N= 4.0dB*/
	{   45,   7800  },  /*C/N= 4.5dB*/
	{   50,   7430  },  /*C/N= 5.0dB*/
	{   55,   7080  },  /*C/N= 5.5dB*/
	{   60,   6720  },  /*C/N= 6.0dB*/
	{   65,   6320  },  /*C/N= 6.5dB*/
	{   70,   6060  },  /*C/N= 7.0dB*/
	{   75,   5760  },  /*C/N= 7.5dB*/
	{   80,   5480  },  /*C/N= 8.0dB*/
	{   85,   5200  },  /*C/N= 8.5dB*/
	{   90,   4930  },  /*C/N= 9.0dB*/
	{   95,   4680  },  /*C/N= 9.5dB*/
	{  100,   4425  },  /*C/N=10.0dB*/
	{  105,   4210  },  /*C/N=10.5dB*/
	{  110,   3980  },  /*C/N=11.0dB*/
	{  115,   3765  },  /*C/N=11.5dB*/
	{  120,   3570  },  /*C/N=12.0dB*/
	{  125,   3315  },  /*C/N=12.5dB*/
	{  130,   3140  },  /*C/N=13.0dB*/
	{  135,   2980  },  /*C/N=13.5dB*/
	{  140,   2820  },  /*C/N=14.0dB*/
	{  145,   2670  },  /*C/N=14.5dB*/
	{  150,   2535  },  /*C/N=15.0dB*/
	{  160,   2270  },  /*C/N=16.0dB*/
	{  170,   2035  },  /*C/N=17.0dB*/
	{  180,   1825  },  /*C/N=18.0dB*/
	{  190,   1650  },  /*C/N=19.0dB*/
	{  200,   1485  },  /*C/N=20.0dB*/
	{  210,   1340  },  /*C/N=21.0dB*/
	{  220,   1212  },  /*C/N=22.0dB*/
	{  230,   1100  },  /*C/N=23.0dB*/
	{  240,   1000  },  /*C/N=24.0dB*/
	{  250,    910  },  /*C/N=25.0dB*/
	{  260,    836  },  /*C/N=26.0dB*/
	{  270,    772  },  /*C/N=27.0dB*/
	{  280,    718  },  /*C/N=28.0dB*/
	{  290,    671  },  /*C/N=29.0dB*/
	{  300,    635  },  /*C/N=30.0dB*/
	{  310,    602  },  /*C/N=31.0dB*/
	{  320,    575  },  /*C/N=32.0dB*/
	{  330,    550  },  /*C/N=33.0dB*/
	{  350,    517  },  /*C/N=35.0dB*/
	{  400,    480  },  /*C/N=40.0dB*/
	{  450,    466  },  /*C/N=45.0dB*/
	{  500,    464  },  /*C/N=50.0dB*/
	{  510,    463  },  /*C/N=51.0dB*/
};

/*********************************************************************
Tracking carrier loop carrier QPSK 1/4 to 8PSK 9/10 long Frame
*********************************************************************/
static u8 S2CarLoop[] =	{
	/* Modcod  2MPon 2MPoff 5MPon 5MPoff 10MPon 10MPoff
	   20MPon 20MPoff 30MPon 30MPoff*/
	/* FE_QPSK_14  */
	0x0C,  0x3C,  0x0B,  0x3C,  0x2A,  0x2C,  0x2A,  0x1C,  0x3A,  0x3B,
	/* FE_QPSK_13  */
	0x0C,  0x3C,  0x0B,  0x3C,  0x2A,  0x2C,  0x3A,  0x0C,  0x3A,  0x2B,
	/* FE_QPSK_25  */
	0x1C,  0x3C,  0x1B,  0x3C,  0x3A,  0x1C,  0x3A,  0x3B,  0x3A,  0x2B,
	/* FE_QPSK_12  */
	0x0C,  0x1C,  0x2B,  0x1C,  0x0B,  0x2C,  0x0B,  0x0C,  0x2A,  0x2B,
	/* FE_QPSK_35  */
	0x1C,  0x1C,  0x2B,  0x1C,  0x0B,  0x2C,  0x0B,  0x0C,  0x2A,  0x2B,
	/* FE_QPSK_23  */
	0x2C,  0x2C,  0x2B,  0x1C,  0x0B,  0x2C,  0x0B,  0x0C,  0x2A,  0x2B,
	/* FE_QPSK_34  */
	0x3C,  0x2C,  0x3B,  0x2C,  0x1B,  0x1C,  0x1B,  0x3B,  0x3A,  0x1B,
	/* FE_QPSK_45  */
	0x0D,  0x3C,  0x3B,  0x2C,  0x1B,  0x1C,  0x1B,  0x3B,  0x3A,  0x1B,
	/* FE_QPSK_56  */
	0x1D,  0x3C,  0x0C,  0x2C,  0x2B,  0x1C,  0x1B,  0x3B,  0x0B,  0x1B,
	/* FE_QPSK_89  */
	0x3D,  0x0D,  0x0C,  0x2C,  0x2B,  0x0C,  0x2B,  0x2B,  0x0B,  0x0B,
	/* FE_QPSK_910 */
	0x1E,  0x0D,  0x1C,  0x2C,  0x3B,  0x0C,  0x2B,  0x2B,  0x1B,  0x0B,
	/* FE_8PSK_35  */
	0x28,  0x09,  0x28,  0x09,  0x28,  0x09,  0x28,  0x08,  0x28,  0x27,
	/* FE_8PSK_23  */
	0x19,  0x29,  0x19,  0x29,  0x19,  0x29,  0x38,  0x19,  0x28,  0x09,
	/* FE_8PSK_34  */
	0x1A,  0x0B,  0x1A,  0x3A,  0x0A,  0x2A,  0x39,  0x2A,  0x39,  0x1A,
	/* FE_8PSK_56  */
	0x2B,  0x2B,  0x1B,  0x1B,  0x0B,  0x1B,  0x1A,  0x0B,  0x1A,  0x1A,
	/* FE_8PSK_89  */
	0x0C,  0x0C,  0x3B,  0x3B,  0x1B,  0x1B,  0x2A,  0x0B,  0x2A,  0x2A,
	/* FE_8PSK_910 */
	0x0C,  0x1C,  0x0C,  0x3B,  0x2B,  0x1B,  0x3A,  0x0B,  0x2A,  0x2A,

	/**********************************************************************
	Tracking carrier loop carrier 16APSK 2/3 to 32APSK 9/10 long Frame
	**********************************************************************/
	/*Modcod 2MPon  2MPoff 5MPon 5MPoff 10MPon 10MPoff 20MPon
	  20MPoff 30MPon 30MPoff*/
	/* FE_16APSK_23  */
	0x0A,  0x0A,  0x0A,  0x0A,  0x1A,  0x0A,  0x39,  0x0A,  0x29,  0x0A,
	/* FE_16APSK_34  */
	0x0A,  0x0A,  0x0A,  0x0A,  0x0B,  0x0A,  0x2A,  0x0A,  0x1A,  0x0A,
	/* FE_16APSK_45  */
	0x0A,  0x0A,  0x0A,  0x0A,  0x1B,  0x0A,  0x3A,  0x0A,  0x2A,  0x0A,
	/* FE_16APSK_56  */
	0x0A,  0x0A,  0x0A,  0x0A,  0x1B,  0x0A,  0x3A,  0x0A,  0x2A,  0x0A,
	/* FE_16APSK_89  */
	0x0A,  0x0A,  0x0A,  0x0A,  0x2B,  0x0A,  0x0B,  0x0A,  0x3A,  0x0A,
	/* FE_16APSK_910 */
	0x0A,  0x0A,  0x0A,  0x0A,  0x2B,  0x0A,  0x0B,  0x0A,  0x3A,  0x0A,
	/* FE_32APSK_34  */
	0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,
	/* FE_32APSK_45  */
	0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,
	/* FE_32APSK_56  */
	0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,
	/* FE_32APSK_89  */
	0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,
	/* FE_32APSK_910 */
	0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,  0x09,
};

static u8 get_optim_cloop(struct stv *state,
			  enum FE_STV0910_ModCod ModCod, u32 Pilots)
{
	int i = 0;
	if (ModCod >= FE_32APSK_910)
		i = ((int)FE_32APSK_910 - (int)FE_QPSK_14) * 10;
	else if (ModCod >= FE_QPSK_14)
		i = ((int)ModCod - (int)FE_QPSK_14) * 10;

	if (state->SymbolRate <= 3000000)
		i += 0;
	else if (state->SymbolRate <=  7000000)
		i += 2;
	else if (state->SymbolRate <= 15000000)
		i += 4;
	else if (state->SymbolRate <= 25000000)
		i += 6;
	else
		i += 8;

	if (!Pilots)
		i += 1;

	return S2CarLoop[i];
}

static int GetCurSymbolRate(struct stv *state, u32 *pSymbolRate)
{
	int status = 0;
	u8 SymbFreq0;
	u8 SymbFreq1;
	u8 SymbFreq2;
	u8 SymbFreq3;
	u8 TimOffs0;
	u8 TimOffs1;
	u8 TimOffs2;
	u32 SymbolRate;
	s32 TimingOffset;

	*pSymbolRate = 0;
	if (!state->Started)
		return status;

	read_reg(state, RSTV0910_P2_SFR3 + state->regoff, &SymbFreq3);
	read_reg(state, RSTV0910_P2_SFR2 + state->regoff, &SymbFreq2);
	read_reg(state, RSTV0910_P2_SFR1 + state->regoff, &SymbFreq1);
	read_reg(state, RSTV0910_P2_SFR0 + state->regoff, &SymbFreq0);
	read_reg(state, RSTV0910_P2_TMGREG2 + state->regoff, &TimOffs2);
	read_reg(state, RSTV0910_P2_TMGREG1 + state->regoff, &TimOffs1);
	read_reg(state, RSTV0910_P2_TMGREG0 + state->regoff, &TimOffs0);

	SymbolRate = ((u32) SymbFreq3 << 24) | ((u32) SymbFreq2 << 16) |
		((u32) SymbFreq1 << 8) | (u32) SymbFreq0;
	TimingOffset = ((u32) TimOffs2 << 16) | ((u32) TimOffs1 << 8) |
		(u32) TimOffs0;

	if ((TimingOffset & (1<<23)) != 0)
		TimingOffset |= 0xFF000000; /* Sign extent */

	SymbolRate = (u32) (((u64) SymbolRate * state->base->mclk) >> 32);
	TimingOffset = (s32) (((s64) SymbolRate * (s64) TimingOffset) >> 29);

	*pSymbolRate = SymbolRate + TimingOffset;

	return 0;
}

static int GetSignalParameters(struct stv *state)
{
	if (!state->Started)
		return -1;

	if (state->ReceiveMode == Mode_DVBS2) {
		u8 tmp;
		u8 rolloff;

		read_reg(state, RSTV0910_P2_DMDMODCOD + state->regoff, &tmp);
		state->ModCod = (enum FE_STV0910_ModCod) ((tmp & 0x7c) >> 2);
		state->Pilots = (tmp & 0x01) != 0;
		state->FECType = (enum DVBS2_FECType) ((tmp & 0x02) >> 1);

		read_reg(state, RSTV0910_P2_TMGOBS + state->regoff, &rolloff);
		rolloff = rolloff >> 6;
		state->FERollOff = (enum FE_STV0910_RollOff) rolloff;

	} else if (state->ReceiveMode == Mode_DVBS) {
		/* todo */
	}
	return 0;
}

static int TrackingOptimization(struct stv *state)
{
	u32 SymbolRate = 0;
	u8 tmp;

	GetCurSymbolRate(state, &SymbolRate);
	read_reg(state, RSTV0910_P2_DMDCFGMD + state->regoff, &tmp);
	tmp &= ~0xC0;

	switch (state->ReceiveMode) {
	case Mode_DVBS:
		tmp |= 0x40; break;
	case Mode_DVBS2:
		tmp |= 0x80; break;
	default:
		tmp |= 0xC0; break;
	}
	write_reg(state, RSTV0910_P2_DMDCFGMD + state->regoff, tmp);

	if (state->ReceiveMode == Mode_DVBS2) {
		/* force to PRE BCH Rate */
		write_reg(state, RSTV0910_P2_ERRCTRL1 + state->regoff,
			  BER_SRC_S2 | state->BERScale);

		if (state->FECType == DVBS2_64K) {
			u8 aclc = get_optim_cloop(state, state->ModCod,
						  state->Pilots);

			if (state->ModCod <= FE_QPSK_910) {
				write_reg(state, RSTV0910_P2_ACLC2S2Q +
					  state->regoff, aclc);
			} else if (state->ModCod <= FE_8PSK_910) {
				write_reg(state, RSTV0910_P2_ACLC2S2Q +
					  state->regoff, 0x2a);
				write_reg(state, RSTV0910_P2_ACLC2S28 +
					  state->regoff, aclc);
			} else if (state->ModCod <= FE_16APSK_910) {
				write_reg(state, RSTV0910_P2_ACLC2S2Q +
					  state->regoff, 0x2a);
				write_reg(state, RSTV0910_P2_ACLC2S216A +
					  state->regoff, aclc);
			} else if (state->ModCod <= FE_32APSK_910) {
				write_reg(state, RSTV0910_P2_ACLC2S2Q +
					  state->regoff, 0x2a);
				write_reg(state, RSTV0910_P2_ACLC2S232A +
					  state->regoff, aclc);
			}
		}
	}
	if (state->ReceiveMode == Mode_DVBS) {
		u8 tmp;

		read_reg(state, RSTV0910_P2_VITCURPUN + state->regoff, &tmp);
		state->PunctureRate = FEC_NONE;
		switch (tmp & 0x1F) {
		case 0x0d:
			state->PunctureRate = FEC_1_2;
			break;
		case 0x12:
			state->PunctureRate = FEC_2_3;
			break;
		case 0x15:
			state->PunctureRate = FEC_3_4;
			break;
		case 0x18:
			state->PunctureRate = FEC_5_6;
			break;
		case 0x1A:
			state->PunctureRate = FEC_7_8;
			break;
		}
	}
	return 0;
}

static int GetSignalToNoise(struct stv *state, s32 *SignalToNoise)
{
	int i;
	u8 Data0;
	u8 Data1;
	u16 Data;
	int nLookup;
	struct SLookupSNTable *Lookup;

	*SignalToNoise = 0;

	if (!state->Started)
		return 0;

	if (state->ReceiveMode == Mode_DVBS2) {
		read_reg(state, RSTV0910_P2_NNOSPLHT1 + state->regoff, &Data1);
		read_reg(state, RSTV0910_P2_NNOSPLHT0 + state->regoff, &Data0);
		nLookup = ARRAY_SIZE(S2_SN_Lookup);
		Lookup = S2_SN_Lookup;
	} else {
		read_reg(state, RSTV0910_P2_NNOSDATAT1 + state->regoff, &Data1);
		read_reg(state, RSTV0910_P2_NNOSDATAT0 + state->regoff, &Data0);
		nLookup = ARRAY_SIZE(S1_SN_Lookup);
		Lookup = S1_SN_Lookup;
	}
	Data = (((u16)Data1) << 8) | (u16) Data0;
	if (Data > Lookup[0].RefValue) {
		*SignalToNoise = Lookup[0].SignalToNoise;
	} else if (Data <= Lookup[nLookup-1].RefValue) {
		*SignalToNoise = Lookup[nLookup-1].SignalToNoise;
	} else {
		for (i = 0; i < nLookup - 1; i += 1) {
			if (Data <= Lookup[i].RefValue &&
			    Data > Lookup[i+1].RefValue) {
				*SignalToNoise =
					(s32)(Lookup[i].SignalToNoise) +
					((s32)(Data - Lookup[i].RefValue) *
					 (s32)(Lookup[i+1].SignalToNoise -
					       Lookup[i].SignalToNoise)) /
					((s32)(Lookup[i+1].RefValue) -
					  (s32)(Lookup[i].RefValue));
				break;
			}
		}
	}
	return 0;
}

static int GetBitErrorRateS(struct stv *state, u32 *BERNumerator,
			    u32 *BERDenominator)
{
	u8 Regs[3];

	int status = read_regs(state, RSTV0910_P2_ERRCNT12 + state->regoff,
			       Regs, 3);

	if (status)
		return -1;

	if ((Regs[0] & 0x80) == 0) {
		state->LastBERDenominator = 1 << ((state->BERScale * 2) +
						  10 + 3);
		state->LastBERNumerator = ((u32) (Regs[0] & 0x7F) << 16) |
			((u32) Regs[1] << 8) | Regs[2];
		if (state->LastBERNumerator < 256 && state->BERScale < 6) {
			state->BERScale += 1;
			status = write_reg(state, RSTV0910_P2_ERRCTRL1 +
					   state->regoff,
					   0x20 | state->BERScale);
		} else if (state->LastBERNumerator > 1024 &&
			   state->BERScale > 2) {
			state->BERScale -= 1;
			status = write_reg(state, RSTV0910_P2_ERRCTRL1 +
					   state->regoff, 0x20 |
					   state->BERScale);
		}
	}
	*BERNumerator = state->LastBERNumerator;
	*BERDenominator = state->LastBERDenominator;
	return 0;
}

static u32 DVBS2_nBCH(enum DVBS2_ModCod ModCod, enum DVBS2_FECType FECType)
{
	static u32 nBCH[][2] = {
		{16200,  3240}, /* QPSK_1_4, */
		{21600,  5400}, /* QPSK_1_3, */
		{25920,  6480}, /* QPSK_2_5, */
		{32400,  7200}, /* QPSK_1_2, */
		{38880,  9720}, /* QPSK_3_5, */
		{43200, 10800}, /* QPSK_2_3, */
		{48600, 11880}, /* QPSK_3_4, */
		{51840, 12600}, /* QPSK_4_5, */
		{54000, 13320}, /* QPSK_5_6, */
		{57600, 14400}, /* QPSK_8_9, */
		{58320, 16000}, /* QPSK_9_10, */
		{43200,  9720}, /* 8PSK_3_5, */
		{48600, 10800}, /* 8PSK_2_3, */
		{51840, 11880}, /* 8PSK_3_4, */
		{54000, 13320}, /* 8PSK_5_6, */
		{57600, 14400}, /* 8PSK_8_9, */
		{58320, 16000}, /* 8PSK_9_10, */
		{43200, 10800}, /* 16APSK_2_3, */
		{48600, 11880}, /* 16APSK_3_4, */
		{51840, 12600}, /* 16APSK_4_5, */
		{54000, 13320}, /* 16APSK_5_6, */
		{57600, 14400}, /* 16APSK_8_9, */
		{58320, 16000}, /* 16APSK_9_10 */
		{48600, 11880}, /* 32APSK_3_4, */
		{51840, 12600}, /* 32APSK_4_5, */
		{54000, 13320}, /* 32APSK_5_6, */
		{57600, 14400}, /* 32APSK_8_9, */
		{58320, 16000}, /* 32APSK_9_10 */
	};

	if (ModCod >= DVBS2_QPSK_1_4 &&
	    ModCod <= DVBS2_32APSK_9_10 && FECType <= DVBS2_16K)
		return nBCH[FECType][ModCod];
	return 64800;
}

static int GetBitErrorRateS2(struct stv *state, u32 *BERNumerator,
			     u32 *BERDenominator)
{
	u8 Regs[3];

	int status = read_regs(state, RSTV0910_P2_ERRCNT12 + state->regoff,
			       Regs, 3);

	if (status)
		return -1;

	if ((Regs[0] & 0x80) == 0) {
		state->LastBERDenominator =
			DVBS2_nBCH((enum DVBS2_ModCod) state->ModCod,
				   state->FECType) <<
			(state->BERScale * 2);
		state->LastBERNumerator = (((u32) Regs[0] & 0x7F) << 16) |
			((u32) Regs[1] << 8) | Regs[2];
		if (state->LastBERNumerator < 256 && state->BERScale < 6) {
			state->BERScale += 1;
			write_reg(state, RSTV0910_P2_ERRCTRL1 + state->regoff,
				  0x20 | state->BERScale);
		} else if (state->LastBERNumerator > 1024 &&
			   state->BERScale > 2) {
			state->BERScale -= 1;
			write_reg(state, RSTV0910_P2_ERRCTRL1 + state->regoff,
				  0x20 | state->BERScale);
		}
	}
	*BERNumerator = state->LastBERNumerator;
	*BERDenominator = state->LastBERDenominator;
	return status;
}

static int GetBitErrorRate(struct stv *state, u32 *BERNumerator,
			   u32 *BERDenominator)
{
	*BERNumerator = 0;
	*BERDenominator = 1;

	switch (state->ReceiveMode) {
	case Mode_DVBS:
		return GetBitErrorRateS(state, BERNumerator, BERDenominator);
		break;
	case Mode_DVBS2:
		return GetBitErrorRateS2(state, BERNumerator, BERDenominator);
	default:
		break;
	}
	return 0;
}

static int init(struct dvb_frontend *fe)
{
	return 0;
}

static int set_mclock(struct stv *state, u32 MasterClock)
{
	u32 idf = 1;
	u32 odf = 4;
	u32 quartz = state->base->extclk / 1000000;
	u32 Fphi = MasterClock / 1000000;
	u32 ndiv = (Fphi * odf * idf) / quartz;
	u32 cp = 7;
	u32 fvco;

	if (ndiv >= 7 && ndiv <= 71)
		cp = 7;
	else if (ndiv >=  72 && ndiv <=  79)
		cp = 8;
	else if (ndiv >=  80 && ndiv <=  87)
		cp = 9;
	else if (ndiv >=  88 && ndiv <=  95)
		cp = 10;
	else if (ndiv >=  96 && ndiv <= 103)
		cp = 11;
	else if (ndiv >= 104 && ndiv <= 111)
		cp = 12;
	else if (ndiv >= 112 && ndiv <= 119)
		cp = 13;
	else if (ndiv >= 120 && ndiv <= 127)
		cp = 14;
	else if (ndiv >= 128 && ndiv <= 135)
		cp = 15;
	else if (ndiv >= 136 && ndiv <= 143)
		cp = 16;
	else if (ndiv >= 144 && ndiv <= 151)
		cp = 17;
	else if (ndiv >= 152 && ndiv <= 159)
		cp = 18;
	else if (ndiv >= 160 && ndiv <= 167)
		cp = 19;
	else if (ndiv >= 168 && ndiv <= 175)
		cp = 20;
	else if (ndiv >= 176 && ndiv <= 183)
		cp = 21;
	else if (ndiv >= 184 && ndiv <= 191)
		cp = 22;
	else if (ndiv >= 192 && ndiv <= 199)
		cp = 23;
	else if (ndiv >= 200 && ndiv <= 207)
		cp = 24;
	else if (ndiv >= 208 && ndiv <= 215)
		cp = 25;
	else if (ndiv >= 216 && ndiv <= 223)
		cp = 26;
	else if (ndiv >= 224 && ndiv <= 225)
		cp = 27;

	write_reg(state, RSTV0910_NCOARSE, (cp << 3) | idf);
	write_reg(state, RSTV0910_NCOARSE2, odf);
	write_reg(state, RSTV0910_NCOARSE1, ndiv);

	fvco = (quartz * 2 * ndiv) / idf;
	state->base->mclk = fvco / (2 * odf) * 1000000;

	/*pr_info("ndiv = %d, MasterClock = %d\n", ndiv, state->base->mclk);*/
	return 0;
}

static int Stop(struct stv *state)
{
	if (state->Started) {
		u8 tmp;

		write_reg(state, RSTV0910_P2_TSCFGH + state->regoff,
			  state->tscfgh | 0x01);
		read_reg(state, RSTV0910_P2_PDELCTRL1 + state->regoff, &tmp);
		tmp &= ~0x01; /*release reset DVBS2 packet delin*/
		write_reg(state, RSTV0910_P2_PDELCTRL1 + state->regoff, tmp);
		/* Blind optim*/
		write_reg(state, RSTV0910_P2_AGC2O + state->regoff, 0x5B);
		/* Stop the demod */
		write_reg(state, RSTV0910_P2_DMDISTATE + state->regoff, 0x5c);
		state->Started = 0;
	}
	state->ReceiveMode = Mode_None;
	return 0;
}


static int Start(struct stv *state, struct dtv_frontend_properties *p)
{
	s32 Freq;
	u8  regDMDCFGMD;
	u16 symb;

	if (p->symbol_rate < 100000 || p->symbol_rate > 70000000)
		return -EINVAL;

	state->ReceiveMode = Mode_None;
	state->DemodLockTime = 0;

	/* Demod Stop*/
	if (state->Started)
		write_reg(state, RSTV0910_P2_DMDISTATE + state->regoff, 0x5C);

	if (p->symbol_rate <= 1000000) {  /*SR <=1Msps*/
		state->DemodTimeout = 3000;
		state->FecTimeout = 2000;
	} else if (p->symbol_rate <= 2000000) {  /*1Msps < SR <=2Msps*/
		state->DemodTimeout = 2500;
		state->FecTimeout = 1300;
	} else if (p->symbol_rate <= 5000000) {  /*2Msps< SR <=5Msps*/
		state->DemodTimeout = 1000;
	    state->FecTimeout = 650;
	} else if (p->symbol_rate <= 10000000) {  /*5Msps< SR <=10Msps*/
		state->DemodTimeout = 700;
		state->FecTimeout = 350;
	} else if (p->symbol_rate < 20000000) {  /*10Msps< SR <=20Msps*/
		state->DemodTimeout = 400;
		state->FecTimeout = 200;
	} else {  /*SR >=20Msps*/
		state->DemodTimeout = 300;
		state->FecTimeout = 200;
	}

	/* Set the Init Symbol rate*/
	symb = MulDiv32(p->symbol_rate, 65536, state->base->mclk);
	write_reg(state, RSTV0910_P2_SFRINIT1 + state->regoff,
		  ((symb >> 8) & 0x7F));
	write_reg(state, RSTV0910_P2_SFRINIT0 + state->regoff, (symb & 0xFF));

	/*pr_info("symb = %u\n", symb);*/

	state->DEMOD |= 0x80;
	write_reg(state, RSTV0910_P2_DEMOD + state->regoff, state->DEMOD);

	/* FE_STV0910_SetSearchStandard */
	read_reg(state, RSTV0910_P2_DMDCFGMD + state->regoff, &regDMDCFGMD);
	write_reg(state, RSTV0910_P2_DMDCFGMD + state->regoff,
		  regDMDCFGMD |= 0xC0);

	/* Disable DSS */
	write_reg(state, RSTV0910_P2_FECM  + state->regoff, 0x00);
	write_reg(state, RSTV0910_P2_PRVIT + state->regoff, 0x2F);

	/* 8PSK 3/5, 8PSK 2/3 Poff tracking optimization WA*/
	write_reg(state, RSTV0910_P2_ACLC2S2Q + state->regoff, 0x0B);
	write_reg(state, RSTV0910_P2_ACLC2S28 + state->regoff, 0x0A);
	write_reg(state, RSTV0910_P2_BCLC2S2Q + state->regoff, 0x84);
	write_reg(state, RSTV0910_P2_BCLC2S28 + state->regoff, 0x84);
	write_reg(state, RSTV0910_P2_CARHDR + state->regoff, 0x1C);
	/* Reset demod */
	write_reg(state, RSTV0910_P2_DMDISTATE + state->regoff, 0x1F);

	write_reg(state, RSTV0910_P2_CARCFG + state->regoff, 0x46);

	Freq = (state->SearchRange / 2000) + 600;
	if (p->symbol_rate <= 5000000)
		Freq -= (600 + 80);
	Freq = (Freq << 16) / (state->base->mclk / 1000);

	write_reg(state, RSTV0910_P2_CFRUP1 + state->regoff,
		  (Freq >> 8) & 0xff);
	write_reg(state, RSTV0910_P2_CFRUP0 + state->regoff, (Freq & 0xff));
	/*CFR Low Setting*/
	Freq = -Freq;
	write_reg(state, RSTV0910_P2_CFRLOW1 + state->regoff,
		  (Freq >> 8) & 0xff);
	write_reg(state, RSTV0910_P2_CFRLOW0 + state->regoff, (Freq & 0xff));

	/* init the demod frequency offset to 0 */
	write_reg(state, RSTV0910_P2_CFRINIT1 + state->regoff, 0);
	write_reg(state, RSTV0910_P2_CFRINIT0 + state->regoff, 0);

	write_reg(state, RSTV0910_P2_DMDISTATE + state->regoff, 0x1F);
	/* Trigger acq */
	write_reg(state, RSTV0910_P2_DMDISTATE + state->regoff, 0x15);

	state->DemodLockTime += TUNING_DELAY;
	state->Started = 1;

	return 0;
}

static int init_diseqc(struct stv *state)
{
	u16 offs = state->nr ? 0x40 : 0;  /* Address offset */
	u8 Freq = ((state->base->mclk + 11000 * 32) / (22000 * 32));

	/* Disable receiver */
	write_reg(state, RSTV0910_P1_DISRXCFG + offs, 0x00);
	write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0xBA); /* Reset = 1 */
	write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3A); /* Reset = 0 */
	write_reg(state, RSTV0910_P1_DISTXF22 + offs, Freq);
	return 0;
}

static int probe(struct stv *state)
{
	u8 id;

	state->ReceiveMode = Mode_None;
	state->Started = 0;

	if (read_reg(state, RSTV0910_MID, &id) < 0)
		return -1;

	if (id != 0x51)
		return -EINVAL;
	/* pr_info("stv0910: found STV0910 id=0x%02x\n", id); */

	 /* Configure the I2C repeater to off */
	write_reg(state, RSTV0910_P1_I2CRPT, 0x24);
	/* Configure the I2C repeater to off */
	write_reg(state, RSTV0910_P2_I2CRPT, 0x24);
	/* Set the I2C to oversampling ratio */
	write_reg(state, RSTV0910_I2CCFG, 0x88);

	write_reg(state, RSTV0910_OUTCFG,    0x00);  /* OUTCFG */
	write_reg(state, RSTV0910_PADCFG,    0x05);  /* RF AGC Pads Dev = 05 */
	write_reg(state, RSTV0910_SYNTCTRL,  0x02);  /* SYNTCTRL */
	write_reg(state, RSTV0910_TSGENERAL, state->tsgeneral);  /* TSGENERAL */
	write_reg(state, RSTV0910_CFGEXT,    0x02);  /* CFGEXT */
	write_reg(state, RSTV0910_GENCFG,    0x15);  /* GENCFG */


	write_reg(state, RSTV0910_TSTRES0, 0x80); /* LDPC Reset */
	write_reg(state, RSTV0910_TSTRES0, 0x00);

	set_mclock(state, 135000000);

	/* TS output */
	write_reg(state, RSTV0910_P1_TSCFGH , state->tscfgh | 0x01);
	write_reg(state, RSTV0910_P1_TSCFGH , state->tscfgh);
	write_reg(state, RSTV0910_P1_TSCFGM , 0xC0);  /* Manual speed */
	write_reg(state, RSTV0910_P1_TSCFGL , 0x20);

	/* Speed = 67.5 MHz */
	write_reg(state, RSTV0910_P1_TSSPEED , state->tsspeed);

	write_reg(state, RSTV0910_P2_TSCFGH , state->tscfgh | 0x01);
	write_reg(state, RSTV0910_P2_TSCFGH , state->tscfgh);
	write_reg(state, RSTV0910_P2_TSCFGM , 0xC0);  /* Manual speed */
	write_reg(state, RSTV0910_P2_TSCFGL , 0x20);

	/* Speed = 67.5 MHz */
	write_reg(state, RSTV0910_P2_TSSPEED , state->tsspeed);

	/* Reset stream merger */
	write_reg(state, RSTV0910_P1_TSCFGH , state->tscfgh | 0x01);
	write_reg(state, RSTV0910_P2_TSCFGH , state->tscfgh | 0x01);
	write_reg(state, RSTV0910_P1_TSCFGH , state->tscfgh);
	write_reg(state, RSTV0910_P2_TSCFGH , state->tscfgh);

	write_reg(state, RSTV0910_P1_I2CRPT, state->i2crpt);
	write_reg(state, RSTV0910_P2_I2CRPT, state->i2crpt);

	init_diseqc(state);
	return 0;
}


static int gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct stv *state = fe->demodulator_priv;
	u8 i2crpt = state->i2crpt & ~0x86;

	if (enable)
		mutex_lock(&state->base->i2c_lock);

	if (enable)
		i2crpt |= 0x80;
	else
		i2crpt |= 0x02;

	if (write_reg(state, state->nr ? RSTV0910_P2_I2CRPT :
		      RSTV0910_P1_I2CRPT, i2crpt) < 0)
		return -EIO;

	state->i2crpt = i2crpt;

	if (!enable)
		mutex_unlock(&state->base->i2c_lock);
	return 0;
}

static void release(struct dvb_frontend *fe)
{
	struct stv *state = fe->demodulator_priv;

	state->base->count--;
	if (state->base->count == 0) {
		list_del(&state->base->stvlist);
		kfree(state->base);
	}
	kfree(state);
}

static int set_parameters(struct dvb_frontend *fe)
{
	int stat = 0;
	struct stv *state = fe->demodulator_priv;
	u32 IF;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	Stop(state);
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe);
	if (fe->ops.tuner_ops.get_if_frequency)
		fe->ops.tuner_ops.get_if_frequency(fe, &IF);
	state->SymbolRate = p->symbol_rate;
	stat = Start(state, p);
	return stat;
}


static int read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct stv *state = fe->demodulator_priv;
	u8 DmdState = 0;
	u8 DStatus  = 0;
	enum ReceiveMode CurReceiveMode = Mode_None;
	u32 FECLock = 0;

	read_reg(state, RSTV0910_P2_DMDSTATE + state->regoff, &DmdState);

	if (DmdState & 0x40) {
		read_reg(state, RSTV0910_P2_DSTATUS + state->regoff, &DStatus);
		if (DStatus & 0x08)
			CurReceiveMode = (DmdState & 0x20) ?
				Mode_DVBS : Mode_DVBS2;
	}
	if (CurReceiveMode == Mode_None) {
		*status = 0;
		return 0;
	}

	*status |= 0x0f;
	if (state->ReceiveMode == Mode_None) {
		state->ReceiveMode = CurReceiveMode;
		state->DemodLockTime = jiffies;
		state->FirstTimeLock = 0;

		write_reg(state, RSTV0910_P2_TSCFGH + state->regoff,
			  state->tscfgh);
		usleep_range(3000, 4000);
		write_reg(state, RSTV0910_P2_TSCFGH + state->regoff,
			  state->tscfgh | 0x01);
		write_reg(state, RSTV0910_P2_TSCFGH + state->regoff,
			  state->tscfgh);
	}
	if (DmdState & 0x40) {
		if (state->ReceiveMode == Mode_DVBS2) {
			u8 PDELStatus;
			read_reg(state,
				 RSTV0910_P2_PDELSTATUS1 + state->regoff,
				 &PDELStatus);
			FECLock = (PDELStatus & 0x02) != 0;
		} else {
			u8 VStatus;
			read_reg(state,
				 RSTV0910_P2_VSTATUSVIT + state->regoff,
				 &VStatus);
			FECLock = (VStatus & 0x08) != 0;
		}
	}

	if (!FECLock)
		return 0;

	*status |= 0x10;

	if (state->FirstTimeLock) {
		u8 tmp;

		state->FirstTimeLock = 0;
		GetSignalParameters(state);

		if (state->ReceiveMode == Mode_DVBS2) {
			/* FSTV0910_P2_MANUALSX_ROLLOFF,
			   FSTV0910_P2_MANUALS2_ROLLOFF = 0 */
			state->DEMOD &= ~0x84;
			write_reg(state, RSTV0910_P2_DEMOD + state->regoff,
				  state->DEMOD);
			read_reg(state, RSTV0910_P2_PDELCTRL2 + state->regoff,
				 &tmp);
			/*reset DVBS2 packet delinator error counter */
			tmp |= 0x40;
			write_reg(state, RSTV0910_P2_PDELCTRL2 + state->regoff,
				  tmp);
			/*reset DVBS2 packet delinator error counter */
			tmp &= ~0x40;
			write_reg(state, RSTV0910_P2_PDELCTRL2 + state->regoff,
				  tmp);

			state->BERScale = 2;
			state->LastBERNumerator = 0;
			state->LastBERDenominator = 1;
			/* force to PRE BCH Rate */
			write_reg(state, RSTV0910_P2_ERRCTRL1 + state->regoff,
				  BER_SRC_S2 | state->BERScale);
		} else {
			state->BERScale = 2;
			state->LastBERNumerator = 0;
			state->LastBERDenominator = 1;
			/* force to PRE RS Rate */
			write_reg(state, RSTV0910_P2_ERRCTRL1 + state->regoff,
				  BER_SRC_S | state->BERScale);
		}
		/*Reset the Total packet counter */
		write_reg(state, RSTV0910_P2_FBERCPT4 + state->regoff, 0x00);
		/*Reset the packet Error counter2 (and Set it to
		  infinit error count mode )*/
		write_reg(state, RSTV0910_P2_ERRCTRL2 + state->regoff, 0xc1);

		TrackingOptimization(state);
	}
	return 0;
}

static int tune(struct dvb_frontend *fe, bool re_tune,
		unsigned int mode_flags,
		unsigned int *delay, fe_status_t *status)
{
	struct stv *state = fe->demodulator_priv;
	int r;

	if (re_tune) {
		r = set_parameters(fe);
		if (r)
			return r;
		state->tune_time = jiffies;
	}
	if (*status & FE_HAS_LOCK)
		return 0;
	*delay = HZ;

	r = read_status(fe, status);
	if (r)
		return r;
	return 0;
}


static int get_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

static int set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone)
{
	struct stv *state = fe->demodulator_priv;
	u16 offs = state->nr ? 0x40 : 0;

	switch (tone) {
	case SEC_TONE_ON:
		return write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x38);
	case SEC_TONE_OFF:
		return write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3a);
	default:
		break;
	}
	return -EINVAL;
}

static int wait_dis(struct stv *state, u8 flag, u8 val)
{
	int i;
	u8 stat;
	u16 offs = state->nr ? 0x40 : 0;

	for (i = 0; i < 10; i++) {
		read_reg(state, RSTV0910_P1_DISTXSTATUS + offs, &stat);
		if ((stat & flag) == val)
			return 0;
		msleep(10);
	}
	return -1;
}

static int send_master_cmd(struct dvb_frontend *fe,
			   struct dvb_diseqc_master_cmd *cmd)
{
	struct stv *state = fe->demodulator_priv;
	u16 offs = state->nr ? 0x40 : 0;
	int i;

	/*pr_info("master_cmd %02x %02x %02x %02x\n",
	  cmd->msg[0],  cmd->msg[1],  cmd->msg[2],  cmd->msg[3]);*/
	write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3E);
	for (i = 0; i < cmd->msg_len; i++) {
		wait_dis(state, 0x40, 0x00);
		write_reg(state, RSTV0910_P1_DISTXFIFO + offs, cmd->msg[i]);
	}
	write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3A);
	wait_dis(state, 0x20, 0x20);
	return 0;
}

static int recv_slave_reply(struct dvb_frontend *fe,
			    struct dvb_diseqc_slave_reply *reply)
{
	return 0;
}

static int send_burst(struct dvb_frontend *fe, fe_sec_mini_cmd_t burst)
{
#if 0
	struct stv *state = fe->demodulator_priv;
	u16 offs = state->nr ? 0x40 : 0;
	u8 value;

	if (burst == SEC_MINI_A) {
		write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3F);
		value = 0x00;
	} else {
		write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3E);
		value = 0xFF;
	}
	wait_dis(state, 0x40, 0x00);
	write_reg(state, RSTV0910_P1_DISTXFIFO + offs, value);
	write_reg(state, RSTV0910_P1_DISTXCFG + offs, 0x3A);
	wait_dis(state, 0x20, 0x20);
#endif
	return 0;
}

static int sleep(struct dvb_frontend *fe)
{
	struct stv *state = fe->demodulator_priv;

	Stop(state);
	return 0;
}

static int read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct stv *state = fe->demodulator_priv;
	s32 SNR;

	*snr = 0;
	if (GetSignalToNoise(state, &SNR))
		return -EIO;
	*snr = SNR;
	return 0;
}

static int read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct stv *state = fe->demodulator_priv;
	u32 n, d;

	GetBitErrorRate(state, &n, &d);
	if (d) 
		*ber = n / d;
	else
		*ber = 0;
	return 0;
}

static int read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct stv *state = fe->demodulator_priv;
	u8 Agc1, Agc0;

	read_reg(state, RSTV0910_P2_AGCIQIN1 + state->regoff, &Agc1);
	read_reg(state, RSTV0910_P2_AGCIQIN0 + state->regoff, &Agc0);

	*strength = ((255 - Agc1) * 3300) / 256;
	return 0;
}

static int read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	/* struct stv *state = fe->demodulator_priv; */


	return 0;
}

static struct dvb_frontend_ops stv0910_ops = {
	.delsys = { SYS_DVBS, SYS_DVBS2, SYS_DSS },
	.info = {
		.name			= "STV0910",
		.frequency_min		= 950000,
		.frequency_max		= 2150000,
		.frequency_stepsize	= 0,
		.frequency_tolerance	= 0,
		.symbol_rate_min	= 1000000,
		.symbol_rate_max	= 70000000,
		.caps			= FE_CAN_INVERSION_AUTO |
					  FE_CAN_FEC_AUTO       |
					  FE_CAN_QPSK           |
					  FE_CAN_2G_MODULATION
	},
	.init				= init,
	.sleep				= sleep,
	.release                        = release,
	.i2c_gate_ctrl                  = gate_ctrl,
	.get_frontend_algo              = get_algo,
	.tune                           = tune,
	.read_status			= read_status,
	.set_tone			= set_tone,

	.diseqc_send_master_cmd		= send_master_cmd,
	.diseqc_send_burst		= send_burst,
	.diseqc_recv_slave_reply	= recv_slave_reply,

	.read_snr			= read_snr,
	.read_ber			= read_ber,
	.read_signal_strength		= read_signal_strength,
	.read_ucblocks			= read_ucblocks,
};

static struct stv_base *match_base(struct i2c_adapter  *i2c, u8 adr)
{
	struct stv_base *p;

	list_for_each_entry(p, &stvlist, stvlist)
		if (p->i2c == i2c && p->adr == adr)
			return p;
	return NULL;
}

struct dvb_frontend *stv0910_attach(struct i2c_adapter *i2c,
				    struct stv0910_cfg *cfg,
				    int nr)
{
	struct stv *state;
	struct stv_base *base;

	state = kzalloc(sizeof(struct stv), GFP_KERNEL);
	if (!state)
		return NULL;

	state->tscfgh = 0x20 | (cfg->parallel ? 0 : 0x40);
	state->tsgeneral = (cfg->parallel == 2) ? 0x02 : 0x00;
	state->i2crpt = 0x0A | ((cfg->rptlvl & 0x07) << 4);
	state->tsspeed = 0x40;
	state->nr = nr;
	state->regoff = state->nr ? 0 : 0x200;
	state->SearchRange = 16000000;
	state->DEMOD = 0x10;     /* Inversion : Auto with reset to 0 */
	state->ReceiveMode   = Mode_None;

	base = match_base(i2c, cfg->adr);
	if (base) {
		base->count++;
		state->base = base;
	} else {
		base = kzalloc(sizeof(struct stv_base), GFP_KERNEL);
		if (!base)
			goto fail;
		base->i2c = i2c;
		base->adr = cfg->adr;
		base->count = 1;
		base->extclk = cfg->clk ? cfg->clk : 30000000;

		mutex_init(&base->i2c_lock);
		mutex_init(&base->reg_lock);
		state->base = base;
		if (probe(state) < 0) {
			kfree(base);
			goto fail;
		}
		list_add(&base->stvlist, &stvlist);
	}
	state->fe.ops               = stv0910_ops;
	state->fe.demodulator_priv  = state;
	state->nr = nr;

	return &state->fe;

fail:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL_GPL(stv0910_attach);

MODULE_DESCRIPTION("STV0910 driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL");
