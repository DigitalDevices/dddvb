/*
 * Driver for the Maxlinear MX58x family of tuners/demods
 *
 * Copyright (C) 2014-2015 Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         developed for Digital Devices GmbH
 *
 * based on code:
 * Copyright (c) 2011-2013 MaxLinear, Inc. All rights reserved
 * which was released under GPL V2
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
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
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <asm/div64.h>
#include <asm/unaligned.h>

#include "dvb_frontend.h"
#include "mxl5xx.h"
#include "mxl5xx_regs.h"
#include "mxl5xx_defs.h"

#define BYTE0(v) ((v >>  0) & 0xff)
#define BYTE1(v) ((v >>  8) & 0xff)
#define BYTE2(v) ((v >> 16) & 0xff)
#define BYTE3(v) ((v >> 24) & 0xff)


LIST_HEAD(mxllist);

struct mxl_base {
	struct list_head     mxllist;
	struct list_head     mxls;

	u8                   adr;
	struct i2c_adapter  *i2c;

	u32                  count;
	u32                  type;
	u32                  sku_type;
	u32                  chipversion;
	u32                  clock;
	u32                  fwversion;
	
	u8                  *ts_map;
	u8                   can_clkout;
	u8                   chan_bond;
	u8                   demod_num;
	u8                   tuner_num;

	unsigned long        next_tune;
	
	struct mutex         i2c_lock;
	struct mutex         status_lock;
	struct mutex         tune_lock;
		
	u8                   buf[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

	u32                  cmd_size;
	u8                   cmd_data[MAX_CMD_DATA];
};

struct mxl {
	struct list_head     mxl;
	
	struct mxl_base     *base;
	struct dvb_frontend  fe;
	u32                  demod;
	u32                  tuner;
	u32                  tuner_in_use;

	unsigned long        tune_time;
};

static void le32_to_cpusn(u32 *data, u32 size)
{
	u32 i;

	for (i = 0; i < size; data++, i += 4)
		le32_to_cpus(data);
}

static void flip_data_in_dword(u32 size, u8 *d)
{
	u32 i;
	u8 t;

	for (i = 0; i < size; i += 4) {
		t = d[i + 3]; d[i + 3] = d[i]; d[i] = t;
		t = d[i + 2]; d[i + 2] = d[i + 1]; d[i + 1] = t;
	}
}


static void convert_endian(u8 flag, u32 size, u8 *d)
{
	u32 i;

	if (!flag)
		return;
	for (i = 0; i < (size & ~3); i += 4) {
		d[i + 0] ^= d[i + 3];
		d[i + 3] ^= d[i + 0];
		d[i + 0] ^= d[i + 3];

		d[i + 1] ^= d[i + 2];
		d[i + 2] ^= d[i + 1];
		d[i + 1] ^= d[i + 2];
	}

	switch (size & 3) {
	case 0: case 1: /* do nothing */ break;
	case 2:
		d[i + 0] ^= d[i + 1];
		d[i + 1] ^= d[i + 0];
		d[i + 0] ^= d[i + 1];
		break;
		
	case 3:
		d[i + 0] ^= d[i + 2];
		d[i + 2] ^= d[i + 0];
		d[i + 0] ^= d[i + 2];
		break;
	}
	    
}

static int i2c_write(struct i2c_adapter *adap, u8 adr,
			    u8 *data, u32 len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	return (i2c_transfer(adap, &msg, 1) == 1) ? 0 : -1;
}

static int i2c_read(struct i2c_adapter *adap, u8 adr,
			   u8 *data, u32 len)
{
	struct i2c_msg msg = {.addr = adr, .flags = I2C_M_RD,
			      .buf = data, .len = len};

	return (i2c_transfer(adap, &msg, 1) == 1) ? 0 : -1;
}

static int i2cread(struct mxl *state, u8 *data, int len)
{
	return i2c_read(state->base->i2c, state->base->adr, data, len);
}

static int i2cwrite(struct mxl *state, u8 *data, int len)
{
	return i2c_write(state->base->i2c, state->base->adr, data, len);
}

static int read_register_unlocked(struct mxl *state, u32 reg, u32 *val)
{
	int stat;
	u8 data[MXL_HYDRA_REG_SIZE_IN_BYTES + MXL_HYDRA_I2C_HDR_SIZE] = {
		MXL_HYDRA_PLID_REG_READ, 0x04,
		GET_BYTE(reg, 0), GET_BYTE(reg, 1),
		GET_BYTE(reg, 2), GET_BYTE(reg, 3),
	};

	stat = i2cwrite(state, data,
			MXL_HYDRA_REG_SIZE_IN_BYTES + MXL_HYDRA_I2C_HDR_SIZE);
	if (stat)
		pr_err("i2c read error 1\n");
	if (!stat)
		stat = i2cread(state, (u8 *) val, MXL_HYDRA_REG_SIZE_IN_BYTES);
	le32_to_cpus(val);
	if (stat)
		pr_err("i2c read error 2\n");
	return stat;
}


#define DMA_I2C_INTERRUPT_ADDR 0x8000011C
#define DMA_INTR_PROT_WR_CMP 0x08

static int send_command(struct mxl *state, u32 size, u8 *buf)
{
	int stat;
	u32 val, count = 10;
	
	mutex_lock(&state->base->i2c_lock);
	if (state->base->fwversion > 0x02010109)  {
		read_register_unlocked(state, DMA_I2C_INTERRUPT_ADDR, &val);
		if (DMA_INTR_PROT_WR_CMP & val)
			pr_info("mxl5xx: send_command busy\n");
		while ((DMA_INTR_PROT_WR_CMP & val) && --count) {
			mutex_unlock(&state->base->i2c_lock);
			usleep_range(1000, 2000);
			mutex_lock(&state->base->i2c_lock);
			read_register_unlocked(state, DMA_I2C_INTERRUPT_ADDR, &val);
		}
		if (!count) {
			pr_info("mxl5xx: send_command busy\n");
			return -EBUSY;
		}
	}
	stat = i2cwrite(state, buf, size);
	mutex_unlock(&state->base->i2c_lock);
	return stat;
}

static int write_register(struct mxl *state, u32 reg, u32 val)
{
	int stat;
	u8 data[MXL_HYDRA_REG_WRITE_LEN] = {
		MXL_HYDRA_PLID_REG_WRITE, 0x08,
		BYTE0(reg), BYTE1(reg), BYTE2(reg), BYTE3(reg),
		BYTE0(val), BYTE1(val), BYTE2(val), BYTE3(val),
	};
	mutex_lock(&state->base->i2c_lock);
	stat = i2cwrite(state, data, sizeof(data));
	mutex_unlock(&state->base->i2c_lock);
	if (stat)
		pr_err("i2c write error\n");
	return stat;
}

static int write_register_block(struct mxl *state, u32 reg, u32 size, u8 *data)
{
	int stat;
	u8 *buf = state->base->buf;

	mutex_lock(&state->base->i2c_lock);

	buf[0] = MXL_HYDRA_PLID_REG_WRITE;
	buf[1] = size + 4;
	buf[2] = GET_BYTE(reg, 0);
	buf[3] = GET_BYTE(reg, 1);
	buf[4] = GET_BYTE(reg, 2);
	buf[5] = GET_BYTE(reg, 3);
	memcpy(&buf[6], data, size);

	convert_endian(MXL_ENABLE_BIG_ENDIAN, size, &buf[6]);
	stat = i2cwrite(state, buf,
			MXL_HYDRA_I2C_HDR_SIZE +
			MXL_HYDRA_REG_SIZE_IN_BYTES + size);
	mutex_unlock(&state->base->i2c_lock);
	return stat;
}

static int write_firmware_block(struct mxl *state,
				u32 reg, u32 size, u8 *regDataPtr)
{
	int stat;
	u8 *buf = state->base->buf;

	mutex_lock(&state->base->i2c_lock);
	buf[0] = MXL_HYDRA_PLID_REG_WRITE;
	buf[1] = size + 4;
	buf[2] = GET_BYTE(reg, 0);
	buf[3] = GET_BYTE(reg, 1);
	buf[4] = GET_BYTE(reg, 2);
	buf[5] = GET_BYTE(reg, 3);
	memcpy(&buf[6], regDataPtr, size);
	stat = i2cwrite(state, buf,
			MXL_HYDRA_I2C_HDR_SIZE +
			MXL_HYDRA_REG_SIZE_IN_BYTES + size);
	mutex_unlock(&state->base->i2c_lock);
	if (stat)
		pr_err("fw block write failed\n");
	return stat;
}

static int read_register(struct mxl *state, u32 reg, u32 *val)
{
	int stat;
	u8 data[MXL_HYDRA_REG_SIZE_IN_BYTES + MXL_HYDRA_I2C_HDR_SIZE] = {
		MXL_HYDRA_PLID_REG_READ, 0x04,
		GET_BYTE(reg, 0), GET_BYTE(reg, 1),
		GET_BYTE(reg, 2), GET_BYTE(reg, 3),
	};

	mutex_lock(&state->base->i2c_lock);
	stat = i2cwrite(state, data,
			MXL_HYDRA_REG_SIZE_IN_BYTES + MXL_HYDRA_I2C_HDR_SIZE);
	if (stat)
		pr_err("i2c read error 1\n");
	if (!stat)
		stat = i2cread(state, (u8 *) val, MXL_HYDRA_REG_SIZE_IN_BYTES);
	mutex_unlock(&state->base->i2c_lock);
	le32_to_cpus(val);
	if (stat)
		pr_err("i2c read error 2\n");
	return stat;
}

static int read_register_block(struct mxl *state, u32 reg, u32 size, u8 *data)
{
	int stat;
	u8 *buf = state->base->buf;

	mutex_lock(&state->base->i2c_lock);

	buf[0] = MXL_HYDRA_PLID_REG_READ;
	buf[1] = size + 4;
	buf[2] = GET_BYTE(reg, 0);
	buf[3] = GET_BYTE(reg, 1);
	buf[4] = GET_BYTE(reg, 2);
	buf[5] = GET_BYTE(reg, 3);
	stat = i2cwrite(state, buf,
			MXL_HYDRA_I2C_HDR_SIZE + MXL_HYDRA_REG_SIZE_IN_BYTES);
	if (!stat) {
		stat = i2cread(state, data, size);
		convert_endian(MXL_ENABLE_BIG_ENDIAN, size, data);
	}
	mutex_unlock(&state->base->i2c_lock);
	return stat;
}

static int read_by_mnemonic(struct mxl *state,
			    u32 reg, u8 lsbloc, u8 numofbits, u32 *val)
{
	u32 data = 0, mask = 0;
	int stat;

	stat = read_register(state, reg, &data);
	if (stat)
		return stat;
	mask = MXL_GET_REG_MASK_32(lsbloc, numofbits);
	data &= mask;
	data >>= lsbloc;
	*val = data;
	return 0;
}


static int update_by_mnemonic(struct mxl *state,
			      u32 reg, u8 lsbloc, u8 numofbits, u32 val)
{
	u32 data, mask;
	int stat;

	stat = read_register(state, reg, &data);
	if (stat)
		return stat;
	mask = MXL_GET_REG_MASK_32(lsbloc, numofbits);
	data = (data & ~mask) | ((val << lsbloc) & mask);
	stat = write_register(state, reg, data);
	return stat;
}

static void extract_from_mnemonic(u32 regAddr, u8 lsbPos, u8 width,
				  u32 *toAddr, u8 *toLsbPos, u8 *toWidth)
{
	if (toAddr)
		*toAddr = regAddr;
	if (toLsbPos)
		*toLsbPos = lsbPos;
	if (toWidth)
		*toWidth = width;
}

static int firmware_is_alive(struct mxl *state)
{
	u32 hb0, hb1;

	if (read_register(state, HYDRA_HEAR_BEAT, &hb0))
		return 0;
	msleep(20);
	if (read_register(state, HYDRA_HEAR_BEAT, &hb1))
		return 0;
	if (hb1 == hb0)
		return 0;
	return 1;
}

static int init(struct dvb_frontend *fe)
{
	return 0;
}

static void release(struct dvb_frontend *fe)
{
	struct mxl *state = fe->demodulator_priv;

	list_del(&state->mxl);
	/* Release one frontend, two more shall take its place! */
	state->base->count--;
	if (state->base->count == 0) {
		list_del(&state->base->mxllist);
		kfree(state->base);
	}
	kfree(state);
}

static int get_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

/*
static int cfg_scrambler(struct mxl *state)
{
	u8 buf[26] = {
		MXL_HYDRA_PLID_CMD_WRITE, 24,
		0, MXL_HYDRA_DEMOD_SCRAMBLE_CODE_CMD, 0, 0,
		state->demod, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 1, 0, 0, 0,
	};

	return send_command(state, sizeof(buf), buf);
}

*/

static int CfgDemodAbortTune(struct mxl *state)
{
	MXL_HYDRA_DEMOD_ABORT_TUNE_T abortTuneCmd;
	u8 cmdSize = sizeof(abortTuneCmd);
	u8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
	
	abortTuneCmd.demodId = state->demod;
	BUILD_HYDRA_CMD(MXL_HYDRA_ABORT_TUNE_CMD, MXL_CMD_WRITE, cmdSize, &abortTuneCmd, cmdBuff);
	return send_command(state, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
}

static int send_master_cmd(struct dvb_frontend *fe,
			   struct dvb_diseqc_master_cmd *cmd)
{
	/*struct mxl *state = fe->demodulator_priv;*/

	return 0; /*CfgDemodAbortTune(state);*/
}

static int set_parameters(struct dvb_frontend *fe)
{
	struct mxl *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	MXL_HYDRA_DEMOD_PARAM_T demodChanCfg;
	u8 cmdSize = sizeof(demodChanCfg);
	u8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
	u32 srange = 10;
	int stat;
	
	if (p->frequency < 950000 || p->frequency > 2150000)
		return -EINVAL;
	if (p->symbol_rate < 1000000 || p->symbol_rate > 45000000)
		return -EINVAL;
	
	//CfgDemodAbortTune(state);

	switch (p->delivery_system) {
	case SYS_DSS:
		demodChanCfg.standard = MXL_HYDRA_DSS;
		demodChanCfg.rollOff = MXL_HYDRA_ROLLOFF_AUTO;
		break;
	case SYS_DVBS:
		srange = p->symbol_rate / 1000000;
		if (srange > 10)
			srange = 10;
		demodChanCfg.standard = MXL_HYDRA_DVBS;
		demodChanCfg.rollOff = MXL_HYDRA_ROLLOFF_0_35;
		demodChanCfg.modulationScheme = MXL_HYDRA_MOD_QPSK;
		demodChanCfg.pilots = MXL_HYDRA_PILOTS_OFF;
		break;
	case SYS_DVBS2:
		demodChanCfg.standard = MXL_HYDRA_DVBS2;
		demodChanCfg.rollOff = MXL_HYDRA_ROLLOFF_AUTO;
		demodChanCfg.modulationScheme = MXL_HYDRA_MOD_AUTO;
		demodChanCfg.pilots = MXL_HYDRA_PILOTS_AUTO;
		//cfg_scrambler(state);
		break;
	default:
		return -EINVAL;
	}
	demodChanCfg.tunerIndex = state->tuner;
	demodChanCfg.demodIndex = state->demod;
	demodChanCfg.frequencyInHz = p->frequency * 1000;
	demodChanCfg.symbolRateInHz = p->symbol_rate;
	demodChanCfg.maxCarrierOffsetInMHz = srange;
	demodChanCfg.spectrumInversion = MXL_HYDRA_SPECTRUM_AUTO;
	demodChanCfg.fecCodeRate = MXL_HYDRA_FEC_AUTO;

	mutex_lock(&state->base->tune_lock);
	if (time_after(jiffies + msecs_to_jiffies(200), state->base->next_tune))
		while (time_before(jiffies, state->base->next_tune))
			msleep(10);
	state->base->next_tune = jiffies + msecs_to_jiffies(100);
	state->tuner_in_use = state->tuner;
	BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_SET_PARAM_CMD, MXL_CMD_WRITE,
			cmdSize, &demodChanCfg, cmdBuff);
	stat = send_command(state, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
	mutex_unlock(&state->base->tune_lock);
	return stat;
}

static int read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct mxl *state = fe->demodulator_priv;

	int stat;
	u32 regData = 0;

	mutex_lock(&state->base->status_lock);
	HYDRA_DEMOD_STATUS_LOCK(state, state->demod);
	stat = read_register(state, (HYDRA_DMD_LOCK_STATUS_ADDR_OFFSET +
				     HYDRA_DMD_STATUS_OFFSET(state->demod)),
			     &regData);
	HYDRA_DEMOD_STATUS_UNLOCK(state, state->demod);
	mutex_unlock(&state->base->status_lock);

	*status = (regData == 1) ? 0x1f : 0;

	return stat;
}

static int tune(struct dvb_frontend *fe, bool re_tune,
		unsigned int mode_flags,
		unsigned int *delay, fe_status_t *status)
{
	struct mxl *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	int r = 0;

	*delay = HZ / 2;
	if (re_tune) {
		r = set_parameters(fe);
		if (r)
			return r;
		state->tune_time = jiffies;
		return 0;
	}
	if (*status & FE_HAS_LOCK)
		return 0;

	r = read_status(fe, status);
	if (r)
		return r;

#if 0
	if (*status & FE_HAS_LOCK)
		return 0;

	if (p->delivery_system == SYS_DVBS)
		p->delivery_system = SYS_DVBS2;
	else
		p->delivery_system = SYS_DVBS;
	set_parameters(fe);
#endif
	return 0;
}

static int enable_tuner(struct mxl *state, u32 tuner, u32 enable);

static int sleep(struct dvb_frontend *fe)
{
	struct mxl *state = fe->demodulator_priv;
	struct mxl *p;
	
	CfgDemodAbortTune(state);
	if (state->tuner_in_use != 0xffffffff) {
		mutex_lock(&state->base->tune_lock);
		state->tuner_in_use = 0xffffffff;
		list_for_each_entry(p, &state->base->mxls, mxl) {
			if (p->tuner_in_use == state->tuner)
				break;
		}
		if (&p->mxl == &state->base->mxls)
			enable_tuner(state, state->tuner, 0);
		mutex_unlock(&state->base->tune_lock);
	}
	return 0;
}

static int read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct mxl *state = fe->demodulator_priv;
	int stat;
	u32 regData = 0;

	mutex_lock(&state->base->status_lock);
	HYDRA_DEMOD_STATUS_LOCK(state, state->demod);
	stat = read_register(state, (HYDRA_DMD_SNR_ADDR_OFFSET +
				     HYDRA_DMD_STATUS_OFFSET(state->demod)),
			     &regData);
	HYDRA_DEMOD_STATUS_UNLOCK(state, state->demod);
	mutex_unlock(&state->base->status_lock);
	*snr = (s16) (regData & 0xFFFF);
	return stat;
}

static int read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;

	return 0;
}

static int read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct mxl *state = fe->demodulator_priv;
	int stat;
	u32 regData = 0;

	mutex_lock(&state->base->status_lock);
	HYDRA_DEMOD_STATUS_LOCK(state, state->demod);
	stat = read_register(state, (HYDRA_DMD_STATUS_INPUT_POWER_ADDR +
				     HYDRA_DMD_STATUS_OFFSET(state->demod)),
			     &regData);
	HYDRA_DEMOD_STATUS_UNLOCK(state, state->demod);
	mutex_unlock(&state->base->status_lock);
	*strength = (u16) (regData & 0xFFFF);
	return stat;
}

static int read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	return 0;
}

static int get_frontend(struct dvb_frontend *fe)
{
	//struct mxl *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	switch (p->delivery_system) {
	case SYS_DSS:
		break;
	case SYS_DVBS:
		break;
	case SYS_DVBS2:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int set_input(struct dvb_frontend *fe, int input)
{
	struct mxl *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	state->tuner = p->input = input;
	return 0;
}

static struct dvb_frontend_ops mxl_ops = {
	.delsys = { SYS_DVBS, SYS_DVBS2, SYS_DSS },
	.xbar   = { 4, 0, 8 }, /* tuner_max, demod id, demod_max */
	.info = {
		.name			= "MXL5XX",
		.frequency_min		= 950000,
		.frequency_max		= 2150000,
		.frequency_stepsize	= 0,
		.frequency_tolerance	= 0,
		.symbol_rate_min	= 1000000,
		.symbol_rate_max	= 45000000,
		.caps			= FE_CAN_INVERSION_AUTO |
					  FE_CAN_FEC_AUTO       |
					  FE_CAN_QPSK           |
					  FE_CAN_2G_MODULATION
	},
	.init				= init,
	.release                        = release,
	.get_frontend_algo              = get_algo,
	.tune                           = tune,
	.read_status			= read_status,
	.sleep				= sleep,
	.read_snr			= read_snr,
	.read_ber			= read_ber,
	.read_signal_strength		= read_signal_strength,
	.read_ucblocks			= read_ucblocks,
	.get_frontend                   = get_frontend,
	.set_input                      = set_input,
	.diseqc_send_master_cmd		= send_master_cmd,
};

static struct mxl_base *match_base(struct i2c_adapter  *i2c, u8 adr)
{
	struct mxl_base *p;

	list_for_each_entry(p, &mxllist, mxllist)
		if (p->i2c == i2c && p->adr == adr)
			return p;
	return NULL;
}

static void cfg_dev_xtal(struct mxl *state, u32 freq, u32 cap, u32 enable)
{
	if (state->base->can_clkout || !enable)  
		SET_REG_FIELD_DATA(AFE_REG_D2A_XTAL_EN_CLKOUT_1P8, enable);
	
	if (freq == 24000000)
		write_register(state, HYDRA_CRYSTAL_SETTING, 0);
	else
		write_register(state, HYDRA_CRYSTAL_SETTING, 1);

	write_register(state, HYDRA_CRYSTAL_CAP, cap);
}

static u32 get_big_endian(u8 numOfBits, const u8 buf[])
{
	u32 retValue = 0;

	switch (numOfBits) {
	case 24:
		retValue = (((u32) buf[0]) << 16) |
			(((u32) buf[1]) << 8) | buf[2];
		break;
	case 32:
		retValue = (((u32) buf[0]) << 24) |
			(((u32) buf[1]) << 16) |
			(((u32) buf[2]) << 8) | buf[3];
		break;
	default:
		break;
	}

	return retValue;
}

static int write_fw_segment(struct mxl *state,
			    u32 MemAddr, u32 totalSize, u8 *dataPtr)
{
	int status;
	u32 dataCount = 0;
	u32 size = 0;
	u32 origSize = 0;
	u8 *wBufPtr = NULL;
	u32 blockSize = ((MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH -
			  (MXL_HYDRA_I2C_HDR_SIZE + MXL_HYDRA_REG_SIZE_IN_BYTES)) / 4) * 4;
	u8 wMsgBuffer[MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH -
		      (MXL_HYDRA_I2C_HDR_SIZE + MXL_HYDRA_REG_SIZE_IN_BYTES)];

	do {
		size = origSize = (((u32)(dataCount + blockSize)) > totalSize) ?
			(totalSize - dataCount) : blockSize;
		
		if (origSize & 3) 
			size = (origSize + 4) & ~3;
		wBufPtr = &wMsgBuffer[0];
		memset((void *) wBufPtr, 0, size);
		memcpy((void *) wBufPtr, (void *) dataPtr, origSize);
		//flip_data_in_dword(size, wBufPtr);
		convert_endian(1, size, wBufPtr);
		status  = write_firmware_block(state, MemAddr, size, wBufPtr);
		if (status)
			return status;
		dataCount += size;
		MemAddr   += size;
		dataPtr   += size;
	} while (dataCount < totalSize);

	return status;
}

static int do_firmware_download(struct mxl *state, u8 *mbinBufferPtr, u32 mbinBufferSize)
				
{
	int status;
	u32 index = 0;
	u32 segLength = 0;
	u32 segAddress = 0;
	MBIN_FILE_T *mbinPtr  = (MBIN_FILE_T *)mbinBufferPtr;
	MBIN_SEGMENT_T *segmentPtr;
	MXL_BOOL_E xcpuFwFlag = MXL_FALSE;
  
	if (mbinPtr->header.id != MBIN_FILE_HEADER_ID) {
		pr_err("%s: Invalid file header ID (%c)\n",
		       __func__, mbinPtr->header.id);
		return -EINVAL;
	}
	status = write_register(state, FW_DL_SIGN_ADDR, 0);
	if (status)
		return status;
	segmentPtr = (MBIN_SEGMENT_T *) (&mbinPtr->data[0]);
	for (index = 0; index < mbinPtr->header.numSegments; index++) {
		if (segmentPtr->header.id != MBIN_SEGMENT_HEADER_ID) {
			pr_err("%s: Invalid segment header ID (%c)\n",
			       __func__, segmentPtr->header.id);
			return -EINVAL;
		}
		segLength  = get_big_endian(24, &(segmentPtr->header.len24[0]));
		segAddress = get_big_endian(32, &(segmentPtr->header.address[0]));
		
		if (state->base->type == MXL_HYDRA_DEVICE_568) {
			if ((((segAddress & 0x90760000) == 0x90760000) ||
			     ((segAddress & 0x90740000) == 0x90740000)) &&
			    (xcpuFwFlag == MXL_FALSE)) {
				SET_REG_FIELD_DATA(PRCM_PRCM_CPU_SOFT_RST_N, 1);
				msleep(200);
				write_register(state, 0x90720000, 0);
				msleep(10);
				xcpuFwFlag = MXL_TRUE;
			}
			status = write_fw_segment(state, segAddress,
						  segLength, (u8 *) segmentPtr->data);
		} else {
			if (((segAddress & 0x90760000) != 0x90760000) &&
			    ((segAddress & 0x90740000) != 0x90740000))
				status = write_fw_segment(state, segAddress,
							  segLength, (u8 *) segmentPtr->data);
		}
		if (status)
			return status;
		segmentPtr = (MBIN_SEGMENT_T *)
			&(segmentPtr->data[((segLength + 3) / 4) * 4]);
	}
	return status;
}

static int check_fw(u8 *mbin, u32 mbin_len)
{
	MBIN_FILE_HEADER_T *fh = (MBIN_FILE_HEADER_T *) mbin;
	u32 flen = (fh->imageSize24[0] << 16) |
		(fh->imageSize24[1] <<  8) | fh->imageSize24[2];
	u8 *fw, cs = 0;
	u32 i;
	
	if (fh->id != 'M' || fh->fmtVersion != '1' || flen > 0x3FFF0) {
		pr_info("mxl5xx: Invalid FW Header\n");
		return -1;
	}
	fw = mbin + sizeof(MBIN_FILE_HEADER_T);
	for (i = 0; i < flen; i += 1)
		cs += fw[i];
	if (cs != fh->imageChecksum) {
		pr_info("mxl5xx: Invalid FW Checksum\n");
		return -1;
	}
	return 0;
}

static int firmware_download(struct mxl *state, u8 *mbin, u32 mbin_len)
{
	int status;
	u32 regData = 0;
	MXL_HYDRA_SKU_COMMAND_T devSkuCfg;
	u8 cmdSize = sizeof(MXL_HYDRA_SKU_COMMAND_T);
	u8 cmdBuff[sizeof(MXL_HYDRA_SKU_COMMAND_T) + 6];

	if (check_fw(mbin, mbin_len))
		return -1;
	
	/* put CPU into reset */
	status = SET_REG_FIELD_DATA(PRCM_PRCM_CPU_SOFT_RST_N, 0);
	if (status)
		return status;
	usleep_range(1000, 2000);

	/* Reset TX FIFO's, BBAND, XBAR */
	status = write_register(state, HYDRA_RESET_TRANSPORT_FIFO_REG,
				HYDRA_RESET_TRANSPORT_FIFO_DATA);
	if (status)
		return status;
	status = write_register(state, HYDRA_RESET_BBAND_REG,
				HYDRA_RESET_BBAND_DATA);
	if (status)
		return status;
	status = write_register(state, HYDRA_RESET_XBAR_REG,
				HYDRA_RESET_XBAR_DATA);
	if (status)
		return status;

	/* Disable clock to Baseband, Wideband, SerDes, Alias ext & Transport modules */
	status = write_register(state, HYDRA_MODULES_CLK_2_REG, HYDRA_DISABLE_CLK_2);
	if (status)
		return status;
	/* Clear Software & Host interrupt status - (Clear on read) */
	status = read_register(state, HYDRA_PRCM_ROOT_CLK_REG, &regData);
	if (status)
		return status;
	status = do_firmware_download(state, mbin, mbin_len);
	if (status)
		return status;
	
	if (state->base->type == MXL_HYDRA_DEVICE_568) {
		msleep(10);
		
		// bring XCPU out of reset
		status = write_register(state, 0x90720000, 1);
		if (status)
			return status;
		msleep(500);
		
		// Enable XCPU UART message processing in MCPU
		status = write_register(state, 0x9076B510, 1);
		if (status)
			return status;
	} else {
		/* Bring CPU out of reset */
		status = SET_REG_FIELD_DATA(PRCM_PRCM_CPU_SOFT_RST_N, 1);
		if (status)
			return status;
		/* Wait until FW boots */
		msleep(150);
	}

	/* Initilize XPT XBAR */
	status = write_register(state, XPT_DMD0_BASEADDR, 0x76543210);
	if (status)
		return status;
	
	if (!firmware_is_alive(state))
		return -1;

	pr_info("mxl5xx: Hydra FW alive. Hail!\n");

	/* sometimes register values are wrong shortly after first heart beats */
	msleep(50);

	devSkuCfg.skuType = state->base->sku_type;
	BUILD_HYDRA_CMD(MXL_HYDRA_DEV_CFG_SKU_CMD, MXL_CMD_WRITE,
			cmdSize, &devSkuCfg, cmdBuff);
	status = send_command(state, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

	return status;
}

static int cfg_ts_pad_mux(struct mxl *state, MXL_BOOL_E enableSerialTS)
{
	int status = 0;
	u32 padMuxValue = 0;

	if (enableSerialTS == MXL_TRUE) {
		padMuxValue = 0;
		if ((state->base->type == MXL_HYDRA_DEVICE_541) ||
		    (state->base->type == MXL_HYDRA_DEVICE_541S))
			padMuxValue = 2;
	} else {
		if ((state->base->type == MXL_HYDRA_DEVICE_581) ||
		    (state->base->type == MXL_HYDRA_DEVICE_581S))
			padMuxValue = 2;
		else
			padMuxValue = 3;
	}

	switch (state->base->type) {
	case MXL_HYDRA_DEVICE_561:
	case MXL_HYDRA_DEVICE_581:
	case MXL_HYDRA_DEVICE_541:
	case MXL_HYDRA_DEVICE_541S:
	case MXL_HYDRA_DEVICE_561S:
	case MXL_HYDRA_DEVICE_581S:
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_15_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_20_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_21_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_22_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_23_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_24_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_25_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_26_PINMUX_SEL, padMuxValue);
		break;

	case MXL_HYDRA_DEVICE_544:
	case MXL_HYDRA_DEVICE_542:
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_01_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_02_PINMUX_SEL, 0);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_03_PINMUX_SEL, 0);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_04_PINMUX_SEL, 0);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_08_PINMUX_SEL, 0);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_27_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_28_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_29_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_30_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_32_PINMUX_SEL, 1);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_33_PINMUX_SEL, 1);
		if (enableSerialTS == MXL_ENABLE) {
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_09_PINMUX_SEL, 0);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_10_PINMUX_SEL, 0);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_11_PINMUX_SEL, 0);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_12_PINMUX_SEL, 0);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_13_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_15_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_20_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_21_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_22_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_23_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_24_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_25_PINMUX_SEL, 2);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_26_PINMUX_SEL, 2);
		} else {
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_09_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_10_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_11_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_12_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_13_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_15_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_20_PINMUX_SEL, 3);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_21_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_22_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_23_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_24_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_25_PINMUX_SEL, 1);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_26_PINMUX_SEL, 1);
		}
		break;
		
	case MXL_HYDRA_DEVICE_568:
		if (enableSerialTS == MXL_FALSE) {
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_02_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_03_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_04_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_05_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_06_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_07_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_08_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_09_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_10_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_11_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_12_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_13_PINMUX_SEL, 5);
			
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_20_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_21_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_22_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_23_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_24_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_25_PINMUX_SEL, padMuxValue);
			
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_26_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_27_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_28_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_29_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_30_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_31_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_32_PINMUX_SEL, 5);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_33_PINMUX_SEL, 5);
		} else {
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_09_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_10_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_11_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_12_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_13_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_15_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, padMuxValue);
			status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, padMuxValue);
		}
		break;
		
		
	case MXL_HYDRA_DEVICE_584:
	default:
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_09_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_10_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_11_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_12_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_13_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_14_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_15_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_16_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_17_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_18_PINMUX_SEL, padMuxValue);
		status |= SET_REG_FIELD_DATA(PAD_MUX_DIGIO_19_PINMUX_SEL, padMuxValue);
		break;
	}
	return status;
}


static int set_drive_strength(struct mxl *state,
			      MXL_HYDRA_TS_DRIVE_STRENGTH_E tsDriveStrength)
{
	int stat = 0;
	u32 val;

	read_register(state, 0x90000194, &val);
	pr_info("mxl5xx: DIGIO = %08x\n", val);
	pr_info("mxl5xx: set drive_strength = %u\n", tsDriveStrength);
	
	
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_00, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_05, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_06, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_11, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_12, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_13, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_14, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_16, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_17, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_18, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_22, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_23, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_24, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_25, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_29, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_30, tsDriveStrength);
	stat |= SET_REG_FIELD_DATA(PAD_MUX_PAD_DRV_DIGIO_31, tsDriveStrength);

	return stat;
}


static int enable_tuner(struct mxl *state, u32 tuner, u32 enable)
{
	int stat = 0;
	MxL_HYDRA_TUNER_CMD ctrlTunerCmd;
	u8 cmdSize = sizeof(ctrlTunerCmd);
	u8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
	u32 val, count = 10;
	
	ctrlTunerCmd.tunerId = tuner;
	ctrlTunerCmd.enable = enable;
	BUILD_HYDRA_CMD(MXL_HYDRA_TUNER_ACTIVATE_CMD, MXL_CMD_WRITE,
			cmdSize, &ctrlTunerCmd, cmdBuff);
	stat = send_command(state, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
	if (stat)
		return stat;
#if 1
	read_register(state, HYDRA_TUNER_ENABLE_COMPLETE, &val);
	while (--count && ((val >> tuner) & 1) != enable) {
		msleep(20);
		read_register(state, HYDRA_TUNER_ENABLE_COMPLETE, &val);
	}
	if (!count)
		return -1;
	read_register(state, HYDRA_TUNER_ENABLE_COMPLETE, &val);
	pr_info("mxl5xx: tuner %u ready = %u\n", tuner , (val >> tuner) & 1);
#endif
	
	return 0;
}


static int config_ts(struct mxl *state, MXL_HYDRA_DEMOD_ID_E demodId,
		     MXL_HYDRA_MPEGOUT_PARAM_T *mpegOutParamPtr)
{
	int status = 0;
	u32 ncoCountMin = 0;
	u32 clkType = 0;
	
	MXL_REG_FIELD_T xpt_sync_polarity[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_SYNC_POLARITY0}, {XPT_SYNC_POLARITY1},
		{XPT_SYNC_POLARITY2}, {XPT_SYNC_POLARITY3},
		{XPT_SYNC_POLARITY4}, {XPT_SYNC_POLARITY5},
		{XPT_SYNC_POLARITY6}, {XPT_SYNC_POLARITY7} };
	MXL_REG_FIELD_T xpt_clock_polarity[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_CLOCK_POLARITY0}, {XPT_CLOCK_POLARITY1},
		{XPT_CLOCK_POLARITY2}, {XPT_CLOCK_POLARITY3},
		{XPT_CLOCK_POLARITY4}, {XPT_CLOCK_POLARITY5},
		{XPT_CLOCK_POLARITY6}, {XPT_CLOCK_POLARITY7} };
	MXL_REG_FIELD_T xpt_valid_polarity[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_VALID_POLARITY0}, {XPT_VALID_POLARITY1},
		{XPT_VALID_POLARITY2}, {XPT_VALID_POLARITY3},
		{XPT_VALID_POLARITY4}, {XPT_VALID_POLARITY5},
		{XPT_VALID_POLARITY6}, {XPT_VALID_POLARITY7} };
	MXL_REG_FIELD_T xpt_ts_clock_phase[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_TS_CLK_PHASE0}, {XPT_TS_CLK_PHASE1},
		{XPT_TS_CLK_PHASE2}, {XPT_TS_CLK_PHASE3},
		{XPT_TS_CLK_PHASE4}, {XPT_TS_CLK_PHASE5},
		{XPT_TS_CLK_PHASE6}, {XPT_TS_CLK_PHASE7} };
	MXL_REG_FIELD_T xpt_lsb_first[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_LSB_FIRST0}, {XPT_LSB_FIRST1}, {XPT_LSB_FIRST2}, {XPT_LSB_FIRST3},
		{XPT_LSB_FIRST4}, {XPT_LSB_FIRST5}, {XPT_LSB_FIRST6}, {XPT_LSB_FIRST7} };
	MXL_REG_FIELD_T xpt_sync_byte[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_SYNC_FULL_BYTE0}, {XPT_SYNC_FULL_BYTE1},
		{XPT_SYNC_FULL_BYTE2}, {XPT_SYNC_FULL_BYTE3},
		{XPT_SYNC_FULL_BYTE4}, {XPT_SYNC_FULL_BYTE5},
		{XPT_SYNC_FULL_BYTE6}, {XPT_SYNC_FULL_BYTE7} };
	MXL_REG_FIELD_T xpt_enable_output[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_ENABLE_OUTPUT0}, {XPT_ENABLE_OUTPUT1},
		{XPT_ENABLE_OUTPUT2}, {XPT_ENABLE_OUTPUT3},
		{XPT_ENABLE_OUTPUT4}, {XPT_ENABLE_OUTPUT5},
		{XPT_ENABLE_OUTPUT6}, {XPT_ENABLE_OUTPUT7} };
	MXL_REG_FIELD_T xpt_err_replace_sync[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_ERROR_REPLACE_SYNC0}, {XPT_ERROR_REPLACE_SYNC1},
		{XPT_ERROR_REPLACE_SYNC2}, {XPT_ERROR_REPLACE_SYNC3},
		{XPT_ERROR_REPLACE_SYNC4}, {XPT_ERROR_REPLACE_SYNC5},
		{XPT_ERROR_REPLACE_SYNC6}, {XPT_ERROR_REPLACE_SYNC7} };
	MXL_REG_FIELD_T xpt_err_replace_valid[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_ERROR_REPLACE_VALID0}, {XPT_ERROR_REPLACE_VALID1},
		{XPT_ERROR_REPLACE_VALID2}, {XPT_ERROR_REPLACE_VALID3},
		{XPT_ERROR_REPLACE_VALID4}, {XPT_ERROR_REPLACE_VALID5},
		{XPT_ERROR_REPLACE_VALID6}, {XPT_ERROR_REPLACE_VALID7} };
	MXL_REG_FIELD_T xpt_continuous_clock[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_TS_CLK_OUT_EN0}, {XPT_TS_CLK_OUT_EN1},
		{XPT_TS_CLK_OUT_EN2}, {XPT_TS_CLK_OUT_EN3},
		{XPT_TS_CLK_OUT_EN4}, {XPT_TS_CLK_OUT_EN5},
		{XPT_TS_CLK_OUT_EN6}, {XPT_TS_CLK_OUT_EN7} };
	MXL_REG_FIELD_T xpt_nco_clock_rate[MXL_HYDRA_DEMOD_MAX] = {
		{XPT_NCO_COUNT_MIN0}, {XPT_NCO_COUNT_MIN1},
		{XPT_NCO_COUNT_MIN2}, {XPT_NCO_COUNT_MIN3},
		{XPT_NCO_COUNT_MIN4}, {XPT_NCO_COUNT_MIN5},
		{XPT_NCO_COUNT_MIN6}, {XPT_NCO_COUNT_MIN7} };
	
	MXL_REG_FIELD_T mxl561_xpt_ts_sync[MXL_HYDRA_DEMOD_ID_6] = {
		{PAD_MUX_DIGIO_25_PINMUX_SEL}, {PAD_MUX_DIGIO_20_PINMUX_SEL},
		{PAD_MUX_DIGIO_17_PINMUX_SEL}, {PAD_MUX_DIGIO_11_PINMUX_SEL},
		{PAD_MUX_DIGIO_08_PINMUX_SEL}, {PAD_MUX_DIGIO_03_PINMUX_SEL} };
	MXL_REG_FIELD_T mxl561_xpt_ts_valid[MXL_HYDRA_DEMOD_ID_6] = {
		{PAD_MUX_DIGIO_26_PINMUX_SEL}, {PAD_MUX_DIGIO_19_PINMUX_SEL},
		{PAD_MUX_DIGIO_18_PINMUX_SEL}, {PAD_MUX_DIGIO_10_PINMUX_SEL},
		{PAD_MUX_DIGIO_09_PINMUX_SEL}, {PAD_MUX_DIGIO_02_PINMUX_SEL} };

	demodId = state->base->ts_map[demodId];
	
	if (MXL_ENABLE == mpegOutParamPtr->enable) {
		if (mpegOutParamPtr->mpegMode == MXL_HYDRA_MPEG_MODE_PARALLEL)	{
#if 0
			for (i = MXL_HYDRA_DEMOD_ID_0; i < MXL_HYDRA_DEMOD_MAX; i++) {
				mxlStatus |= MxLWare_Hydra_UpdateByMnemonic(devId,
									    xpt_enable_output[i].regAddr,
									    xpt_enable_output[i].lsbPos,
									    xpt_enable_output[i].numOfBits,
									    0);
				
			}
			cfg_ts_pad_mux(state, MXL_FALSE);

			mpegOutParamPtr->lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
			mpegOutParamPtr->mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BYTE;
			
			// remove output FIFO
			mxlStatus |= SET_REG_FIELD_DATA(devId, PRCM_PRCM_XPT_PARALLEL_FIFO_RST_N, 0);
			mxlStatus |= SET_REG_FIELD_DATA(devId, PRCM_PRCM_XPT_PARALLEL_FIFO_RST_N, 1);
			
			// Enable parallel mode
			mxlStatus |= SET_REG_FIELD_DATA(devId, XPT_ENABLE_PARALLEL_OUTPUT, MXL_TRUE);
#endif
		} else {
			cfg_ts_pad_mux(state, MXL_TRUE);
			SET_REG_FIELD_DATA(XPT_ENABLE_PARALLEL_OUTPUT, MXL_FALSE);
		}
	}
	
	ncoCountMin = (u32)(MXL_HYDRA_NCO_CLK/mpegOutParamPtr->maxMpegClkRate);

	if (state->base->chipversion >= 2) {
		status |= update_by_mnemonic(state,
					     xpt_nco_clock_rate[demodId].regAddr,         // Reg Addr
					     xpt_nco_clock_rate[demodId].lsbPos,          // LSB pos
					     xpt_nco_clock_rate[demodId].numOfBits,       // Num of bits
					     ncoCountMin);              // Data
	} else
		SET_REG_FIELD_DATA(XPT_NCO_COUNT_MIN, ncoCountMin);
	
	if (mpegOutParamPtr->mpegClkType == MXL_HYDRA_MPEG_CLK_CONTINUOUS)
		clkType = 1;

	if (mpegOutParamPtr->mpegMode < MXL_HYDRA_MPEG_MODE_PARALLEL) {
		status  |= update_by_mnemonic(state,
					      xpt_continuous_clock[demodId].regAddr,
					      xpt_continuous_clock[demodId].lsbPos,
					      xpt_continuous_clock[demodId].numOfBits,
					      clkType);
	} else
		SET_REG_FIELD_DATA(XPT_TS_CLK_OUT_EN_PARALLEL, clkType);

	status |= update_by_mnemonic(state,
				     xpt_sync_polarity[demodId].regAddr,
				     xpt_sync_polarity[demodId].lsbPos,
				     xpt_sync_polarity[demodId].numOfBits,
				     mpegOutParamPtr->mpegSyncPol);

	status |= update_by_mnemonic(state,
				     xpt_valid_polarity[demodId].regAddr,
				     xpt_valid_polarity[demodId].lsbPos,
				     xpt_valid_polarity[demodId].numOfBits,
				     mpegOutParamPtr->mpegValidPol);

	status |= update_by_mnemonic(state,
				     xpt_clock_polarity[demodId].regAddr,
				     xpt_clock_polarity[demodId].lsbPos,
				     xpt_clock_polarity[demodId].numOfBits,
				     mpegOutParamPtr->mpegClkPol);

	status |= update_by_mnemonic(state,
				     xpt_sync_byte[demodId].regAddr,
				     xpt_sync_byte[demodId].lsbPos,
				     xpt_sync_byte[demodId].numOfBits,
				     mpegOutParamPtr->mpegSyncPulseWidth);

	status |= update_by_mnemonic(state,
				     xpt_ts_clock_phase[demodId].regAddr,
				     xpt_ts_clock_phase[demodId].lsbPos,
				     xpt_ts_clock_phase[demodId].numOfBits,
				     mpegOutParamPtr->mpegClkPhase);

	status |= update_by_mnemonic(state,
				     xpt_lsb_first[demodId].regAddr,
				     xpt_lsb_first[demodId].lsbPos,
				     xpt_lsb_first[demodId].numOfBits,
				     mpegOutParamPtr->lsbOrMsbFirst);

	switch (mpegOutParamPtr->mpegErrorIndication) {
	case MXL_HYDRA_MPEG_ERR_REPLACE_SYNC:
		status |= update_by_mnemonic(state,
					     xpt_err_replace_sync[demodId].regAddr,
					     xpt_err_replace_sync[demodId].lsbPos,
					     xpt_err_replace_sync[demodId].numOfBits,
					     MXL_TRUE);
		status |= update_by_mnemonic(state,
					     xpt_err_replace_valid[demodId].regAddr,
					     xpt_err_replace_valid[demodId].lsbPos,
					     xpt_err_replace_valid[demodId].numOfBits,
					     MXL_FALSE);
		break;

	case MXL_HYDRA_MPEG_ERR_REPLACE_VALID:
		status |= update_by_mnemonic(state,
					     xpt_err_replace_sync[demodId].regAddr,
					     xpt_err_replace_sync[demodId].lsbPos,
					     xpt_err_replace_sync[demodId].numOfBits,
					     MXL_FALSE);

		status |= update_by_mnemonic(state,
					     xpt_err_replace_valid[demodId].regAddr,
					     xpt_err_replace_valid[demodId].lsbPos,
					     xpt_err_replace_valid[demodId].numOfBits,
					     MXL_TRUE);
		break;

	case MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED:
	default:
		status |= update_by_mnemonic(state,
					     xpt_err_replace_sync[demodId].regAddr,
					     xpt_err_replace_sync[demodId].lsbPos,
					     xpt_err_replace_sync[demodId].numOfBits,
					     MXL_FALSE);

		status |= update_by_mnemonic(state,
					     xpt_err_replace_valid[demodId].regAddr,
					     xpt_err_replace_valid[demodId].lsbPos,
					     xpt_err_replace_valid[demodId].numOfBits,
					     MXL_FALSE);

		break;

	}

	if (mpegOutParamPtr->mpegMode != MXL_HYDRA_MPEG_MODE_PARALLEL) {
		status |= update_by_mnemonic(state,
					     xpt_enable_output[demodId].regAddr,
					     xpt_enable_output[demodId].lsbPos,
					     xpt_enable_output[demodId].numOfBits,
					     mpegOutParamPtr->enable);
	}
	return status;
}

static int config_mux(struct mxl *state)
{
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT0, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT1, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT2, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT3, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT4, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT5, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT6, 0);
	SET_REG_FIELD_DATA(XPT_ENABLE_OUTPUT7, 0);
	SET_REG_FIELD_DATA(XPT_STREAM_MUXMODE0, 1);
	SET_REG_FIELD_DATA(XPT_STREAM_MUXMODE1, 1);
	return 0;
}

static int config_dis(struct mxl *state, u32 id)
{
	MXL_HYDRA_DISEQC_ID_E diseqcId = id;
	MXL_HYDRA_DISEQC_OPMODE_E opMode = MXL_HYDRA_DISEQC_ENVELOPE_MODE;
	MXL_HYDRA_DISEQC_VER_E version = MXL_HYDRA_DISEQC_1_X;
	MXL_HYDRA_DISEQC_CARRIER_FREQ_E carrierFreqInHz =
		MXL_HYDRA_DISEQC_CARRIER_FREQ_22KHZ;
	MXL58x_DSQ_OP_MODE_T diseqcMsg;
	u8 cmdSize = sizeof(diseqcMsg);
	u8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

	diseqcMsg.diseqcId = diseqcId;
	diseqcMsg.centerFreq = carrierFreqInHz;
	diseqcMsg.version = version;
	diseqcMsg.opMode = opMode;

	BUILD_HYDRA_CMD(MXL_HYDRA_DISEQC_CFG_MSG_CMD,
			MXL_CMD_WRITE, cmdSize, &diseqcMsg, cmdBuff);
	return send_command(state, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
}

static int load_fw(struct mxl *state, struct mxl5xx_cfg *cfg)
{
	int stat = 0;
	u8 *buf;
	
	if (cfg->fw)
		return firmware_download(state, cfg->fw, cfg->fw_len);
	
	if (!cfg->fw_read)
		return -1;

	buf = vmalloc(0x40000);
	if (!buf)
		return -ENOMEM;
	
	cfg->fw_read(cfg->fw_priv, buf, 0x40000);
	stat = firmware_download(state, buf, 0x40000);
	vfree(buf);

	return stat;
}

static int validate_sku(struct mxl *state)
{
	u32 padMuxBond, prcmChipId, prcmSoCId;
	int status;
	u32 type = state->base->type;
	
	status = GET_REG_FIELD_DATA(PAD_MUX_BOND_OPTION, &padMuxBond);
	status |= GET_REG_FIELD_DATA(PRCM_PRCM_CHIP_ID, &prcmChipId);
	status |= GET_REG_FIELD_DATA(PRCM_AFE_SOC_ID, &prcmSoCId);
	if (status)
		return -1;

	pr_info("mx5xx: padMuxBond=%08x, prcmChipId=%08x, prcmSoCId=%08x\n",
		padMuxBond, prcmChipId, prcmSoCId);
	
	if (prcmChipId != 0x560) {
		switch (padMuxBond) {
		case MXL_HYDRA_SKU_ID_581:
			if (type == MXL_HYDRA_DEVICE_581)
				return 0;
			if (type == MXL_HYDRA_DEVICE_581S) {
				state->base->type = MXL_HYDRA_DEVICE_581;
				return 0;
			}
			break;
		case MXL_HYDRA_SKU_ID_584:
			if (type == MXL_HYDRA_DEVICE_584)
				return 0;
			break;
		case MXL_HYDRA_SKU_ID_544:
			if (type == MXL_HYDRA_DEVICE_544)
				return 0;
			if (type == MXL_HYDRA_DEVICE_542)
				return 0;
			break;
		case MXL_HYDRA_SKU_ID_582:
			if (type == MXL_HYDRA_DEVICE_582)
				return 0;
			break;
		default:
			return -1;
		}
	} else {
		
	}
	return -1;
}

static int get_fwinfo(struct mxl *state)
{
	int status;
	u32 val = 0;

	status = GET_REG_FIELD_DATA(PAD_MUX_BOND_OPTION, &val);
	if (status)
		return status;
	pr_info("mxl5xx: chipID=%08x\n", val);

	status = GET_REG_FIELD_DATA(PRCM_AFE_CHIP_MMSK_VER, &val);
	if (status)
		return status;
	pr_info("mxl5xx: chipVer=%08x\n", val);

	status = read_register(state, HYDRA_FIRMWARE_VERSION, &val);
	if (status)
		return status;
	pr_info("mx5xx: FWVer=%08x\n", val);

	state->base->fwversion = val;
	return status;
}


static u8 tsMap1_to_1[MXL_HYDRA_DEMOD_MAX] =
{
	MXL_HYDRA_DEMOD_ID_0,
	MXL_HYDRA_DEMOD_ID_1,
	MXL_HYDRA_DEMOD_ID_2,
	MXL_HYDRA_DEMOD_ID_3,
	MXL_HYDRA_DEMOD_ID_4,
	MXL_HYDRA_DEMOD_ID_5,
	MXL_HYDRA_DEMOD_ID_6,
	MXL_HYDRA_DEMOD_ID_7,
};

static u8 tsMap54x[MXL_HYDRA_DEMOD_MAX] =
{
	MXL_HYDRA_DEMOD_ID_2,
	MXL_HYDRA_DEMOD_ID_3,
	MXL_HYDRA_DEMOD_ID_4,
	MXL_HYDRA_DEMOD_ID_5,
	MXL_HYDRA_DEMOD_MAX,
	MXL_HYDRA_DEMOD_MAX,
	MXL_HYDRA_DEMOD_MAX,
	MXL_HYDRA_DEMOD_MAX,
};

static int probe(struct mxl *state, struct mxl5xx_cfg *cfg)
{
	u32 chipver;
	int fw, status, j;
	MXL_HYDRA_MPEGOUT_PARAM_T mpegInterfaceCfg;

	state->base->ts_map = tsMap1_to_1;
	
	switch (state->base->type) {
	case MXL_HYDRA_DEVICE_581:
	case MXL_HYDRA_DEVICE_581S:
		state->base->can_clkout = 1;
		state->base->demod_num = 8;
		state->base->tuner_num = 1;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_581;
		break;
	case MXL_HYDRA_DEVICE_582:
		state->base->can_clkout = 1;
		state->base->demod_num = 8;
		state->base->tuner_num = 3;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_582;
		break;
	case MXL_HYDRA_DEVICE_585:
		state->base->can_clkout = 0;
		state->base->demod_num = 8;
		state->base->tuner_num = 4;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_585;
		break;
	case MXL_HYDRA_DEVICE_544:
		state->base->can_clkout = 0;
		state->base->demod_num = 4;
		state->base->tuner_num = 4;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_544;
		state->base->ts_map = tsMap54x;
		break;
	case MXL_HYDRA_DEVICE_541:
	case MXL_HYDRA_DEVICE_541S:
		state->base->can_clkout = 0;
		state->base->demod_num = 4;
		state->base->tuner_num = 1;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_541;
		state->base->ts_map = tsMap54x;
		break;
	case MXL_HYDRA_DEVICE_561:
	case MXL_HYDRA_DEVICE_561S:
		state->base->can_clkout = 0;
		state->base->demod_num = 6;
		state->base->tuner_num = 1;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_561;
		break;
	case MXL_HYDRA_DEVICE_568:
		state->base->can_clkout = 0;
		state->base->demod_num = 8;
		state->base->tuner_num = 1;
		state->base->chan_bond = 1;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_568;
		break;
	case MXL_HYDRA_DEVICE_542:
		state->base->can_clkout = 1;
		state->base->demod_num = 4;
		state->base->tuner_num = 3;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_542;
		state->base->ts_map = tsMap54x;
		break;
	case MXL_HYDRA_DEVICE_TEST:
	case MXL_HYDRA_DEVICE_584:
	default:
		state->base->can_clkout = 0;
		state->base->demod_num = 8;
		state->base->tuner_num = 4;
		state->base->sku_type = MXL_HYDRA_SKU_TYPE_584;
		break;
	}

	status = validate_sku(state);
	if (status)
		return status;

	SET_REG_FIELD_DATA(PRCM_AFE_REG_CLOCK_ENABLE, 1);
	SET_REG_FIELD_DATA(PRCM_PRCM_AFE_REG_SOFT_RST_N, 1);
	status = GET_REG_FIELD_DATA(PRCM_CHIP_VERSION, &chipver);
	if (status)
		state->base->chipversion = 0;
	else
		state->base->chipversion = (chipver == 2) ? 2 : 1;
	pr_info("mxl5xx: Hydra chip version %u\n", state->base->chipversion);

	cfg_dev_xtal(state, cfg->clk, cfg->cap, 0);
		
	fw = firmware_is_alive(state);
	if (!fw) {
		status = load_fw(state, cfg);
		if (status)
			return status;
	}
	get_fwinfo(state);
	
#if 0
	config_dis(state, 0);
	config_dis(state, 1);
	config_dis(state, 2);
	config_dis(state, 3);
#endif
	
	config_mux(state);
	mpegInterfaceCfg.enable = MXL_ENABLE;
	mpegInterfaceCfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	/*  supports only (0-104&139)MHz */
	if (cfg->ts_clk)
		mpegInterfaceCfg.maxMpegClkRate = cfg->ts_clk;
	else
		mpegInterfaceCfg.maxMpegClkRate = 69;//139;
	mpegInterfaceCfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_0_DEG;
	mpegInterfaceCfg.mpegClkPol = MXL_HYDRA_MPEG_CLK_IN_PHASE;
	/* MXL_HYDRA_MPEG_CLK_GAPPED; */
	mpegInterfaceCfg.mpegClkType = MXL_HYDRA_MPEG_CLK_CONTINUOUS;
	mpegInterfaceCfg.mpegErrorIndication =
		MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;
	mpegInterfaceCfg.mpegMode = MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE;
	mpegInterfaceCfg.mpegSyncPol  = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	mpegInterfaceCfg.mpegSyncPulseWidth  = MXL_HYDRA_MPEG_SYNC_WIDTH_BIT;
	mpegInterfaceCfg.mpegValidPol  = MXL_HYDRA_MPEG_ACTIVE_HIGH;

	
	for (j = 0; j < state->base->demod_num; j++) {
		status = config_ts(state, (MXL_HYDRA_DEMOD_ID_E) j,
				   &mpegInterfaceCfg);
		if (status)
			return status;
	}
#if 0
	for (j = 0; j < state->base->tuner_num; j++)
		enable_tuner(state, j, 1);
#endif
	set_drive_strength(state, 1);
	return 0;
}

struct dvb_frontend *mxl5xx_attach(struct i2c_adapter *i2c,
				   struct mxl5xx_cfg *cfg,
				   u32 demod, u32 tuner)
{
	struct mxl *state;
	struct mxl_base *base;

	state = kzalloc(sizeof(struct mxl), GFP_KERNEL);
	if (!state)
		return NULL;

	state->demod = demod;
	state->tuner = tuner;
	state->tuner_in_use = 0xffffffff;

	base = match_base(i2c, cfg->adr);
	if (base) {
		base->count++;
		if (base->count > base->demod_num)
			goto fail;
		state->base = base;
	} else {
		base = kzalloc(sizeof(struct mxl_base), GFP_KERNEL);
		if (!base)
			goto fail;
		base->i2c = i2c;
		base->adr = cfg->adr;
		base->type = cfg->type;
		base->count = 1;
		mutex_init(&base->i2c_lock);
		mutex_init(&base->status_lock);
		mutex_init(&base->tune_lock);
		INIT_LIST_HEAD(&base->mxls);
		
		state->base = base;
		if (probe(state, cfg) < 0) {
			kfree(base);
			goto fail;
		}
		list_add(&base->mxllist, &mxllist);
	}
	state->fe.ops               = mxl_ops;
	state->fe.ops.xbar[1]       = demod;
	state->fe.demodulator_priv  = state;
	state->fe.dtv_property_cache.input = tuner;
	list_add(&state->mxl, &base->mxls);
	return &state->fe;

fail:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL_GPL(mxl5xx_attach);


MODULE_DESCRIPTION("MXL5XX driver");
MODULE_AUTHOR("Ralph and Marcus Metzler, Metzler Brothers Systementwicklung GbR");
MODULE_LICENSE("GPL");
