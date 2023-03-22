/*
 * ddbridge-sx8.c: Digital Devices MAX SX8 driver
 *
 * Copyright (C) 2018 Digital Devices GmbH
 *                    Marcus Metzler <mocm@metzlerbros.de>
 *                    Ralph Metzler <rjkm@metzlerbros.de>
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
#include "ddbridge-mci.h"

static int default_mod = 3;
module_param(default_mod, int, 0444);
MODULE_PARM_DESC(default_mod, "default modulations enabled, default is  3 (1 = QPSK, 2 = 8PSK, 4 = 16APSK, ...)");

static int direct_mode;
module_param(direct_mode, int, 0444);
MODULE_PARM_DESC(direct_mode, "Ignore LDPC limits and assign high speed demods according to needed symbolrate.");

static u32 sx8_tuner_flags;
module_param(sx8_tuner_flags, int, 0664);
MODULE_PARM_DESC(sx8_tuner_flags, "Change SX8 tuner flags.");

static u32 sx8_tuner_gain;
module_param(sx8_tuner_gain, int, 0664);
MODULE_PARM_DESC(sx8_tuner_gain, "Change SX8 tuner gain.");

static const u32 MCLK = (1550000000 / 12);

/* Add 2MBit/s overhead allowance (minimum factor is 90/32400 for QPSK w/o Pilots) */
static const u32 MAX_LDPC_BITRATE = (720000000 + 2000000);
static const u32 MAX_DEMOD_LDPC_BITRATE = (1550000000 / 6);

#define SX8_TUNER_NUM 4
#define SX8_DEMOD_NUM 8
#define SX8_DEMOD_NONE 0xff

struct sx8_base {
	struct mci_base      mci_base;

	u8                   tuner_use_count[SX8_TUNER_NUM];

	u32                  used_ldpc_bitrate[SX8_DEMOD_NUM];
	u8                   demod_in_use[SX8_DEMOD_NUM];
	u32                  iq_mode;
};

struct sx8 {
	struct mci           mci;
	struct mutex         lock;

	int                  first_time_lock;
	int                  started;
	int                  iq_started;
};

static const u8 dvbs2_bits_per_symbol[] = {
	0, 0, 0, 0,
        /* S2 QPSK */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	/* S2 8PSK */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3,
	/* S2 16APSK */
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	/* S2 32APSK */
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5,	     
	
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	3, 0, 4, 0,
	2, 2, 2,                               2, 2, 2,                                  // S2X QPSK
	3, 3, 3, 3, 3,                         3, 3, 3, 3, 3,                            // S2X 8PSK
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,    // S2X 16APSK
	5, 5, 5, 5, 5,                         5, 5, 5, 5, 5,                            // S2X 32APSK
	6, 6, 6, 6, 6, 6, 6, 6,                6, 6, 6, 6, 6, 6, 6, 6,                   // S2X 64APSK
	7, 7,                                  7, 7,                                     // S2X 128APSK
	8, 8, 8, 8, 8, 8,                      8, 8, 8, 8, 8, 8,                         // S2X 256APSK

	2, 2, 2, 2, 2, 2,                      2, 2, 2, 2, 2, 2,                         // S2X QPSK
	3, 3, 3, 3,                            3, 3, 3, 3,                               // S2X 8PSK
	4, 4, 4, 4, 4,                         4, 4, 4, 4, 4,                            // S2X 16APSK
	5, 5,                                  5, 5,                                     // S2X 32APSK

	3, 4, 5, 6, 8, 10,
};


static void release(struct dvb_frontend *fe)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;

	mci_base->count--;
	if (mci_base->count == 0) {
		list_del(&mci_base->mci_list);
		kfree(mci_base);
	}
	kfree(state);
}

static int ddb_mci_tsconfig(struct mci *state, u32 config)
{
	struct ddb_link *link = state->base->link;

	if (link->ids.device != 0x0009  && link->ids.device != 0x000b)
		return -EINVAL;
	ddblwritel(link, config, SX8_TSCONFIG);
	return 0;
}

static int read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	int stat = 0;
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;
	struct sx8_base *sx8_base = (struct sx8_base *) mci_base;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct mci_result res;

	*status = 0x00;
	mutex_lock(&state->lock);
	if (!state->started && !state->iq_started)
		goto unlock;
	stat = ddb_mci_get_status(&state->mci, &res);
	if (stat)
		goto unlock;
	ddb_mci_get_info(&state->mci);
	if (stat)
		goto unlock;
	if (res.status == MCI_DEMOD_LOCKED || res.status == SX8_DEMOD_IQ_MODE) {
		*status = FE_HAS_LOCK | FE_HAS_SYNC | FE_HAS_VITERBI |
			FE_HAS_CARRIER | FE_HAS_SIGNAL;
		if (res.status == MCI_DEMOD_LOCKED) {
			mutex_lock(&mci_base->tuner_lock);
			if (state->first_time_lock && state->started) {
				if (state->mci.signal_info.dvbs2_signal_info.standard == 2) {
					sx8_base->used_ldpc_bitrate[state->mci.nr] =
						p->symbol_rate *
						dvbs2_bits_per_symbol[
							state->mci.signal_info.
							dvbs2_signal_info.pls_code];
				} else
					sx8_base->used_ldpc_bitrate[state->mci.nr] = 0;
				state->first_time_lock = 0;
			}
			mutex_unlock(&mci_base->tuner_lock);
		}
	} else if (res.status == MCI_DEMOD_TIMEOUT)
		*status = FE_TIMEDOUT;
	else if (res.status >= SX8_DEMOD_WAIT_MATYPE)
		*status = FE_HAS_SYNC | FE_HAS_VITERBI | FE_HAS_CARRIER | FE_HAS_SIGNAL;
unlock:
	mutex_unlock(&state->lock);
	return stat;
}

static int mci_set_tuner(struct dvb_frontend *fe, u32 tuner, u32 on,
			 u8 flags, u8 gain)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.tuner = state->mci.tuner;
	cmd.command = on ? SX8_CMD_INPUT_ENABLE : SX8_CMD_INPUT_DISABLE;
	cmd.sx8_input_enable.flags = flags;
	cmd.sx8_input_enable.rf_gain = gain;
	return ddb_mci_cmd(&state->mci, &cmd, NULL);
}

static int stop_iq(struct dvb_frontend *fe)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;
	struct sx8_base *sx8_base = (struct sx8_base *) mci_base;
	struct mci_command cmd;
	u32 input = state->mci.tuner;

	if (!state->iq_started)
		return -1;
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = SX8_CMD_STOP_IQ;
	cmd.demod = state->mci.demod;
	ddb_mci_cmd(&state->mci, &cmd, NULL);
	ddb_mci_tsconfig(&state->mci, SX8_TSCONFIG_MODE_NORMAL);

	mutex_lock(&mci_base->tuner_lock);
	sx8_base->tuner_use_count[input]--;
        if (!sx8_base->tuner_use_count[input])
		mci_set_tuner(fe, input, 0, 0, 0);
	if (state->mci.demod != SX8_DEMOD_NONE) {
		sx8_base->demod_in_use[state->mci.demod] = 0;
		state->mci.demod = SX8_DEMOD_NONE;
	}
	sx8_base->used_ldpc_bitrate[state->mci.nr] = 0;	
	mutex_unlock(&mci_base->tuner_lock);
	sx8_base->iq_mode = 0;
	state->iq_started = 0;
	return 0;
}

static int stop(struct dvb_frontend *fe)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;
	struct sx8_base *sx8_base = (struct sx8_base *) mci_base;
	struct mci_command cmd;
	u32 input;

	input = state->mci.tuner;
	if (!state->started)
		return -1;
	if (state->mci.demod != SX8_DEMOD_NONE) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.command = MCI_CMD_STOP;
		cmd.demod = state->mci.demod;
		ddb_mci_cmd(&state->mci, &cmd, NULL);
		if (sx8_base->iq_mode) {
			cmd.command = SX8_CMD_DISABLE_IQOUTPUT;
			cmd.demod = state->mci.demod;
			cmd.output = 0;
			ddb_mci_cmd(&state->mci, &cmd, NULL);
			ddb_mci_tsconfig(&state->mci, SX8_TSCONFIG_MODE_NORMAL);
		}
	}
	mutex_lock(&mci_base->tuner_lock);
	sx8_base->tuner_use_count[input]--;
        if (!sx8_base->tuner_use_count[input])
		mci_set_tuner(fe, input, 0, 0, 0);
	if (state->mci.demod != SX8_DEMOD_NONE) {
		sx8_base->demod_in_use[state->mci.demod] = 0;
		state->mci.demod = SX8_DEMOD_NONE;
	}
	sx8_base->used_ldpc_bitrate[state->mci.nr] = 0;	
	sx8_base->iq_mode = 0;
	state->started = 0;
	mutex_unlock(&mci_base->tuner_lock);
	return 0;
}

static const u8 ro_lut[8] = {
	8 | SX8_ROLLOFF_35, 8 | SX8_ROLLOFF_20, 8 | SX8_ROLLOFF_25, 0,
	8 | SX8_ROLLOFF_15, 8 | SX8_ROLLOFF_10, 8 | SX8_ROLLOFF_05, 0,
};

static int start(struct dvb_frontend *fe, u32 flags, u32 modmask, u32 ts_config)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;
	struct sx8_base *sx8_base = (struct sx8_base *) mci_base;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	static const u32 MAX_DEMOD_LDPC_BITRATE = (1550000000 / 6);
	u32 used_ldpc_bitrate = 0, free_ldpc_bitrate;
	u32 used_demods = 0;
	struct mci_command cmd;
	u32 input = state->mci.tuner;
	u32 bits_per_symbol = 0;
	int i = -1, stat = 0;
	struct ddb_link *link = state->mci.base->link;

	if (link->ids.device == 0x000b) {
		/* Mask out higher modulations and MIS for Basic
		   or search command will fail */
		modmask &= 3;
		p->stream_id = NO_STREAM_ID_FILTER;
	}
	if (p->symbol_rate >= MCLK / 2)
		flags &= ~1;
	if ((flags & 3) == 0)
		return -EINVAL;

	if (flags & 2) {
		u32 tmp = modmask;
		
		bits_per_symbol = 1;
		while (tmp & 1) {
			tmp >>= 1;
			bits_per_symbol++;
		}
	}
	mutex_lock(&mci_base->tuner_lock);
	if (sx8_base->iq_mode) {
		stat = -EBUSY;
		goto unlock;
	}

	if (direct_mode) {
		if (p->symbol_rate >= MCLK / 2) {
			if (state->mci.nr < 4)
				i = state->mci.nr;
		} else {
			i = state->mci.nr;
		}
	} else {
		for (i = 0; i < SX8_DEMOD_NUM; i++) {
			used_ldpc_bitrate += sx8_base->used_ldpc_bitrate[i];
			if (sx8_base->demod_in_use[i])
				used_demods++;
		}
		if ((used_ldpc_bitrate >= MAX_LDPC_BITRATE)  ||
		    ((ts_config & SX8_TSCONFIG_MODE_MASK) >
		     SX8_TSCONFIG_MODE_NORMAL && used_demods > 0)) {
			stat = -EBUSY;
			goto unlock;
		}
		free_ldpc_bitrate = MAX_LDPC_BITRATE - used_ldpc_bitrate;
		if (free_ldpc_bitrate > MAX_DEMOD_LDPC_BITRATE) 
			free_ldpc_bitrate = MAX_DEMOD_LDPC_BITRATE;

		while (p->symbol_rate * bits_per_symbol > free_ldpc_bitrate)
			bits_per_symbol--;
		if (bits_per_symbol < 2) {
			stat = -EBUSY;
			goto unlock;
		}
		modmask &= ((1 << (bits_per_symbol - 1)) - 1);
		if (((flags & 0x02) != 0) && (modmask == 0)) {
			stat = -EBUSY;
			goto unlock;
		}
		
		i = (p->symbol_rate > MCLK / 2) ? 3 : 7;
		while (i >= 0 && sx8_base->demod_in_use[i])
			i--;
	}
	if (i < 0) { 
		stat = -EBUSY;
		goto unlock;
	}
        sx8_base->demod_in_use[i] = 1;
	sx8_base->used_ldpc_bitrate[state->mci.nr] = p->symbol_rate * bits_per_symbol;
        state->mci.demod = i;

        if (!sx8_base->tuner_use_count[input])
		mci_set_tuner(fe, input, 1, sx8_tuner_flags, sx8_tuner_gain);
	sx8_base->tuner_use_count[input]++;
	sx8_base->iq_mode = (ts_config > 1);
unlock:
	mutex_unlock(&mci_base->tuner_lock);
	if (stat)
		return stat;
	memset(&cmd, 0, sizeof(cmd));
	if (sx8_base->iq_mode) {
		cmd.command = SX8_CMD_ENABLE_IQOUTPUT;
		cmd.demod = state->mci.demod;
		cmd.output = p->stream_id & 0x0f;
		ddb_mci_cmd(&state->mci, &cmd, NULL);
		ddb_mci_tsconfig(&state->mci, ts_config);
	}
	if (p->stream_id != NO_STREAM_ID_FILTER && !(p->stream_id & 0xf0000000))
		flags |= 0x80;
	//printk("bw %u\n", p->bandwidth_hz);
	if (p->bandwidth_hz && (p->bandwidth_hz < 20000)) {
		flags |= 0x40;
		/* +/- range, so multiply bandwidth_hz (actually in kHz) by 500 */
		cmd.dvbs2_search.frequency_range = p->bandwidth_hz * 500;
		//printk("range %u\n", cmd.dvbs2_search.frequency_range);
	}
	cmd.command = MCI_CMD_SEARCH_DVBS;
	cmd.dvbs2_search.flags = flags;
	cmd.dvbs2_search.s2_modulation_mask = modmask;
	cmd.dvbs2_search.rsvd1 = ro_lut[p->rolloff & 7];
	cmd.dvbs2_search.retry = 2;
	cmd.dvbs2_search.frequency = p->frequency * 1000;
	cmd.dvbs2_search.symbol_rate = p->symbol_rate;
	cmd.dvbs2_search.scrambling_sequence_index =
		p->scrambling_sequence_index | 0x80000000;
	cmd.dvbs2_search.input_stream_id = p->stream_id;
	cmd.tuner = state->mci.tuner;
	cmd.demod = state->mci.demod;
	cmd.output = state->mci.nr;
	if ((p->stream_id != NO_STREAM_ID_FILTER)  &&
	    (p->stream_id & 0x80000000))
		cmd.output |= 0x80;
	stat = ddb_mci_cmd(&state->mci, &cmd, NULL);
	state->started = 1;
	state->first_time_lock = 1;
	state->mci.signal_info.status = MCI_DEMOD_WAIT_SIGNAL;
	if (stat)
		stop(fe);
	return stat;
}


static int start_iq(struct dvb_frontend *fe, u32 flags,
		    u32 ts_config)
{
	struct sx8 *state = fe->demodulator_priv;
	struct mci_base *mci_base = state->mci.base;
	struct sx8_base *sx8_base = (struct sx8_base *) mci_base;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 used_demods = 0;
	struct mci_command cmd;
	u32 input = state->mci.tuner;
	int i, stat = 0;

	mutex_lock(&mci_base->tuner_lock);
	if (!state->iq_started) {
		if (sx8_base->iq_mode) {
			stat = -EBUSY;
			goto unlock;
		}
		for (i = 0; i < SX8_DEMOD_NUM; i++)
			if (sx8_base->demod_in_use[i])
				used_demods++;
		if (used_demods > 0) {
			stat = -EBUSY;
			goto unlock;
		}
		state->mci.demod = 0;
		sx8_base->tuner_use_count[input]++;
		sx8_base->iq_mode = 2;
		mci_set_tuner(fe, input, 1, flags & 0xff, 0x40);
	} else {
		if ((state->iq_started & 0x07) != state->mci.nr) {
			stat = -EBUSY;
			goto unlock;
		}
	}
unlock:
	mutex_unlock(&mci_base->tuner_lock);
	if (stat)
		return stat;
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = SX8_CMD_START_IQ;
	cmd.sx8_start_iq.flags = (flags >> 16) & 0xff;
	cmd.sx8_start_iq.roll_off = 5;
	//cmd.sx8_start_iq.roll_off = ro_lut[p->rolloff & 7];
	cmd.sx8_start_iq.frequency = p->frequency * 1000;
	cmd.sx8_start_iq.symbol_rate = p->symbol_rate;
	cmd.sx8_start_iq.gain = (flags >> 8) & 0xff;
	cmd.tuner = state->mci.tuner;
	cmd.demod = state->mci.demod;
	stat = ddb_mci_cmd(&state->mci, &cmd, NULL);
	state->iq_started = 8 | state->mci.nr;
	state->first_time_lock = 1;
	state->mci.signal_info.status = MCI_DEMOD_WAIT_SIGNAL;
	if (stat)
		stop_iq(fe);
	ddb_mci_tsconfig(&state->mci, ts_config);
	return stat;
}

static int set_lna(struct dvb_frontend *fe)
{
	printk("set_lna\n");
	return 0;
}

static int set_parameters(struct dvb_frontend *fe)
{
	int stat = 0;
	struct sx8 *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 ts_config = SX8_TSCONFIG_MODE_NORMAL, iq_mode = 0, isi, ts_mode = 0;

	isi = p->stream_id;
	if (isi != NO_STREAM_ID_FILTER) {
		iq_mode = (isi & 0x30000000) >> 28;
		ts_mode = (isi & 0x03000000) >> 24;
	}
	state->mci.input->con = ts_mode << 8;
	if (iq_mode)
		ts_config = (SX8_TSCONFIG_TSHEADER | SX8_TSCONFIG_MODE_IQ);
	mutex_lock(&state->lock);
	stop(fe);
	if (iq_mode < 2) {
		u32 mask;

		stop_iq(fe);
		switch (p->modulation) {
		case APSK_256:
			mask = 0x7f;
			break;
		case APSK_128:
			mask = 0x3f;
			break;
		case APSK_64:
			mask = 0x1f;
			break;
		case APSK_32:
			mask = 0x0f;
			break;
		case APSK_16:
			mask = 0x07;
			break;
		default:
			mask = default_mod;
			break;
		}
		stat = start(fe, 3, mask, ts_config);
	} else {
		stat = start_iq(fe, isi & 0xffffff, ts_config);
	}
	mutex_unlock(&state->lock);
	return stat;
}

static int tune(struct dvb_frontend *fe, bool re_tune,
		unsigned int mode_flags,
		unsigned int *delay, enum fe_status *status)
{
	int r;

	if (re_tune) {
		r = set_parameters(fe);
		if (r)
			return r;
	}
	r = read_status(fe, status);
	if (r)
		return r;

	if (*status & FE_HAS_LOCK)
		return 0;
	*delay = HZ / 10;
	return 0;
}

static enum dvbfe_algo get_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

static int set_input(struct dvb_frontend *fe, int input)
{
	struct sx8 *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	if (input >= SX8_TUNER_NUM)
		return -EINVAL;
	if (state->mci.tuner == input)
		return 0;
	mutex_lock(&state->lock);
	stop_iq(fe);
	stop(fe);
	state->mci.tuner = input;
#ifndef KERNEL_DVB_CORE
	p->input = input;
#endif
	mutex_unlock(&state->lock);
	return 0;
}

static int sleep(struct dvb_frontend *fe)
{
	struct sx8 *state = fe->demodulator_priv;

	mutex_lock(&state->lock);
	stop_iq(fe);
	stop(fe);
	mutex_unlock(&state->lock);
	return 0;
}

static int get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *p)
{
	struct sx8 *state = fe->demodulator_priv;

	ddb_mci_proc_info(&state->mci, p);
	return 0;
}

static struct dvb_frontend_ops sx8_ops = {
	.delsys = { SYS_DVBS, SYS_DVBS2 },
	.info = {
		.name = "DVB-S/S2X",
		.frequency_min_hz	= 950000000,
		.frequency_max_hz	= 2150000000,
		.frequency_stepsize_hz	= 0,
		.frequency_tolerance_hz	= 0,
		.symbol_rate_min	= 100000,
		.symbol_rate_max	= 100000000,
		.caps			= FE_CAN_INVERSION_AUTO |
					  FE_CAN_FEC_AUTO       |
					  FE_CAN_QPSK           |
					  FE_CAN_2G_MODULATION  |
					  FE_CAN_MULTISTREAM,
	},
	.get_frontend_algo              = get_algo,
	.get_frontend                   = get_frontend,
	.tune                           = tune,
	.release                        = release,
	.read_status                    = read_status,
#ifndef KERNEL_DVB_CORE
	.xbar   = { 4, 0, 8 }, /* tuner_max, demod id, demod_max */
	.set_input                      = set_input,
#endif
	.set_lna                        = set_lna,
	.sleep                          = sleep,
};

static int init(struct mci *mci)
{
	struct sx8 *state = (struct sx8 *) mci;

	state->mci.demod = SX8_DEMOD_NONE;
#ifndef KERNEL_DVB_CORE
	mci->fe.ops.xbar[1] = mci->nr;
	mci->fe.dtv_property_cache.input = mci->tuner;
#endif
	mutex_init(&state->lock);
	return 0;
}

static int base_init(struct mci_base *mci_base)
{
	//struct sx8_base *base = (struct sx8_base *) mci_base;

	return 0;
}

static struct mci_cfg ddb_max_sx8_cfg = {
	.type = 0,
	.fe_ops = &sx8_ops,
	.base_size = sizeof(struct sx8_base),
	.state_size = sizeof(struct sx8),
	.init = init,
	.base_init = base_init,
};

struct dvb_frontend *ddb_sx8_attach(struct ddb_input *input, int nr, int tuner,
				    int (**fn_set_input)(struct dvb_frontend *fe, int input))
{
	*fn_set_input = set_input;
	return ddb_mci_attach(input, &ddb_max_sx8_cfg, nr, tuner);
}
