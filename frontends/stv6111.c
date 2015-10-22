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

#if 0
static int write_reg(struct stv *state, u8 reg, u8 val)
{
	u8 d[2] = {reg, val};

	return i2c_write(state->i2c, state->adr, d, 2);
}
#endif

static int read_reg(struct stv *state, u8 reg, u8 *val)
{
	return i2c_read(state->i2c, state->adr, &reg, 1, val, 1);
}

static int read_regs(struct stv *state, u8 reg, u8 *val, int len)
{
	return i2c_read(state->i2c, state->adr, &reg, 1, val, len);
}

static void dump_regs(struct stv *state)
{
	u8 d[11], *c = &state->reg[0];

	read_regs(state, 0, d, 11);
#if 0
	pr_info("stv6111_regs = %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x\n",
		d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
		d[8], d[9], d[10]);
	pr_info("reg[] =        %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x\n",
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10]);
#endif
}

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
	dump_regs(state);
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

	dump_regs(state);
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

static u32 AGC_Gain[] = {
	000, /* 0.0 */
	000, /* 0.1 */
	1000, /* 0.2 */
	2000, /* 0.3 */
	3000, /* 0.4 */
	4000, /* 0.5 */
	5000, /* 0.6 */
	6000, /* 0.7 */
	7000, /* 0.8 */
	14000, /* 0.9 */
	20000, /* 1.0 */
	27000, /* 1.1 */
	32000, /* 1.2 */
	37000, /* 1.3 */
	42000, /* 1.4 */
	47000, /* 1.5 */
	50000, /* 1.6 */
	53000, /* 1.7 */
	56000, /* 1.8 */
	58000, /* 1.9 */
	60000, /* 2.0 */
	62000, /* 2.1 */
	63000, /* 2.2 */
	64000, /* 2.3 */
	64500, /* 2.4 */
	65000, /* 2.5 */
	65500, /* 2.6 */
	66000, /* 2.7 */
	66500, /* 2.8 */
	67000, /* 2.9 */
};

static int get_rf_strength(struct dvb_frontend *fe, u16 *st)
{
	*st = 0;
#if 0
	struct stv *state = fe->tuner_priv;
	s32 Gain;
	u32 Index = RFAgc / 100;
	if (Index >= (sizeof(AGC_Gain) / sizeof(AGC_Gain[0]) - 1))
		Gain = AGC_Gain[sizeof(AGC_Gain) / sizeof(AGC_Gain[0]) - 1];
	else
		Gain = AGC_Gain[Index] +
			((AGC_Gain[Index+1] - AGC_Gain[Index]) *
			 (RFAgc % 100)) / 100;
	*st = Gain;
#endif
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
		return 0;
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
