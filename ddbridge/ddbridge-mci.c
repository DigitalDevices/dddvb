/*
 * ddbridge-mci.c: Digital Devices microcode interface
 *
 * Copyright (C) 2017-2018 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
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

static LIST_HEAD(mci_list);

static int mci_reset(struct mci *state)
{
	struct ddb_link *link = state->base->link;
	u32 status = 0;
	u32 timeout = 40;

	ddblwritel(link, MCI_CONTROL_RESET, MCI_CONTROL);
	ddblwritel(link, 0, MCI_CONTROL + 4); /* 1= no internal init */
	msleep(300);
	ddblwritel(link, 0, MCI_CONTROL);

	while(1) {
		status = ddblreadl(link, MCI_CONTROL);
		if ((status & MCI_CONTROL_READY) == MCI_CONTROL_READY)
			break;
		if (--timeout == 0)
			break;
		msleep(50);
	}
	if ((status & MCI_CONTROL_READY) == 0 )
		return -1;
	if (link->ids.device == 0x0009)
		ddblwritel(link, SX8_TSCONFIG_MODE_NORMAL, SX8_TSCONFIG);
	return 0;
}

int ddb_mci_config(struct mci *state, u32 config)
{
	struct ddb_link *link = state->base->link;

	if (link->ids.device != 0x0009)
		return -EINVAL;
	ddblwritel(link, config, SX8_TSCONFIG);
	return 0;
}


static int ddb_mci_cmd_raw_unlocked(struct mci *state,
				    u32 *cmd, u32 cmd_len,
				    u32 *res, u32 res_len)
{
	struct ddb_link *link = state->base->link;
	u32 i, val;
	unsigned long stat;
	
	val = ddblreadl(link, MCI_CONTROL);
	if (val & (MCI_CONTROL_RESET | MCI_CONTROL_START_COMMAND))
		return -EIO;
	if (cmd && cmd_len)
		for (i = 0; i < cmd_len; i++)
			ddblwritel(link, cmd[i], MCI_COMMAND + i * 4);
	val |= (MCI_CONTROL_START_COMMAND | MCI_CONTROL_ENABLE_DONE_INTERRUPT);
	ddblwritel(link, val, MCI_CONTROL);
	
	stat = wait_for_completion_timeout(&state->base->completion, HZ);
	if (stat == 0) {
		u32 istat = ddblreadl(link, INTERRUPT_STATUS);

		printk("MCI timeout\n");
		val = ddblreadl(link, MCI_CONTROL);
		if (val == 0xffffffff)
			printk("Lost PCIe link!\n");
		else {
			printk("DDBridge IRS %08x\n", istat);
			if (istat & 1) 
				ddblwritel(link, istat & 1, INTERRUPT_ACK);
		}
		return -EIO;
	}
	if (res && res_len)
		for (i = 0; i < res_len; i++)
			res[i] = ddblreadl(link, MCI_RESULT + i * 4);
	return 0;
}

int ddb_mci_cmd_unlocked(struct mci *state,
			 struct mci_command *command,
			 struct mci_result *result)
{
	u32 *cmd = (u32 *) command;
	u32 *res = (u32 *) result;
	
	return ddb_mci_cmd_raw_unlocked(state, cmd, sizeof(*command)/sizeof(u32),
					res, sizeof(*result)/sizeof(u32));
}

int ddb_mci_cmd(struct mci *state,
		struct mci_command *command,
		struct mci_result *result)
{
	int stat;
	
	mutex_lock(&state->base->mci_lock);
	stat = ddb_mci_cmd_raw_unlocked(state,
				 (u32 *)command, sizeof(*command)/sizeof(u32),
				 (u32 *)result, sizeof(*result)/sizeof(u32));
	mutex_unlock(&state->base->mci_lock);
	return stat;
}

int ddb_mci_cmd_raw(struct mci *state,
		    struct mci_command *command, u32 command_len,
		    struct mci_result *result, u32 result_len)
{
	int stat;
	
	mutex_lock(&state->base->mci_lock);
	stat = ddb_mci_cmd_raw_unlocked(state,
					(u32 *)command, command_len,
					(u32 *)result, result_len);
	mutex_unlock(&state->base->mci_lock);
	return stat;
}

static int ddb_mci_get_iq(struct mci *mci, u32 demod, s16 *i, s16 *q)
{
	int stat;
	struct mci_command cmd;
	struct mci_result res;

	memset(&cmd, 0, sizeof(cmd));
	memset(&res, 0, sizeof(res));
	cmd.command = MCI_CMD_GET_IQSYMBOL;
	cmd.demod = demod;
	stat = ddb_mci_cmd(mci, &cmd, &res);
	if (!stat) {
		*i = res.iq_symbol.i;
		*q = res.iq_symbol.q;
	}
	return stat;
}

int ddb_mci_get_status(struct mci *mci, struct mci_result *res)
{
	struct mci_command cmd;

	cmd.command = MCI_CMD_GETSTATUS;
	cmd.demod = mci->demod;
	return ddb_mci_cmd_raw(mci, &cmd, 1, res, 1);
}

int ddb_mci_get_snr(struct dvb_frontend *fe)
{
	struct mci *mci = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;

	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	p->cnr.stat[0].svalue = (s64) mci->
		signal_info.dvbs2_signal_info.signal_to_noise * 10;
	return 0;
}

int ddb_mci_get_strength(struct dvb_frontend *fe)
{
	struct mci *mci = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	s32 str;

	str = mci->signal_info.dvbs2_signal_info.channel_power * 10;
	p->strength.len = 1;
	p->strength.stat[0].scale = FE_SCALE_DECIBEL;
	p->strength.stat[0].svalue = str;
	return 0;
}

int ddb_mci_get_info(struct mci *mci)
{
	int stat;
	struct mci_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = MCI_CMD_GETSIGNALINFO;
	cmd.demod = mci->demod;
	stat = ddb_mci_cmd(mci, &cmd, &mci->signal_info);
	return stat;
}

/****************************************************************************/
/****************************************************************************/

void ddb_mci_proc_info(struct mci *mci, struct dtv_frontend_properties *p)
{
	const enum fe_modulation modcod2mod[0x20] = {
		QPSK, QPSK, QPSK, QPSK,
		QPSK, QPSK, QPSK, QPSK,
		QPSK, QPSK, QPSK, QPSK,
		PSK_8, PSK_8, PSK_8, PSK_8,
		PSK_8, PSK_8, APSK_16, APSK_16,
		APSK_16, APSK_16, APSK_16, APSK_16,
		APSK_32, APSK_32, APSK_32, APSK_32,
		APSK_32,
	};
	const enum fe_code_rate modcod2fec[0x20] = {
		FEC_NONE, FEC_1_4, FEC_1_3, FEC_2_5,
		FEC_1_2, FEC_3_5, FEC_2_3, FEC_3_4,
		FEC_4_5, FEC_5_6, FEC_8_9, FEC_9_10,
		FEC_3_5, FEC_2_3, FEC_3_4, FEC_5_6,
		FEC_8_9, FEC_9_10, FEC_2_3, FEC_3_4,
		FEC_4_5, FEC_5_6, FEC_8_9, FEC_9_10,
		FEC_3_4, FEC_4_5, FEC_5_6, FEC_8_9,
		FEC_9_10, FEC_NONE, FEC_NONE, FEC_NONE,
	};
	const enum fe_code_rate dvbs_fec_lut[8] = {
		FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6,
		FEC_NONE, FEC_7_8, FEC_NONE, FEC_NONE,
	};
	const enum fe_rolloff ro_lut[8] = {
		ROLLOFF_35, ROLLOFF_25, ROLLOFF_20, ROLLOFF_10,
		ROLLOFF_5, ROLLOFF_15, ROLLOFF_35, ROLLOFF_35
	};
	
	p->frequency =
		mci->signal_info.dvbs2_signal_info.frequency;
	switch (p->delivery_system) {
	default:
	case SYS_DVBS:
	case SYS_DVBS2:
	{
		u32 pls_code =
			mci->signal_info.dvbs2_signal_info.pls_code;
		
		p->frequency =
			mci->signal_info.dvbs2_signal_info.frequency / 1000;
		p->delivery_system =
			(mci->signal_info.dvbs2_signal_info.standard == 2)  ?
			SYS_DVBS2 : SYS_DVBS;
		if (mci->signal_info.dvbs2_signal_info.standard == 2) {
			u32 modcod = (0x7c & pls_code) >> 2;
			
			p->delivery_system = SYS_DVBS2;
			p->rolloff =
				ro_lut[mci->signal_info.
				       dvbs2_signal_info.roll_off & 7];
			p->pilot = (pls_code & 1) ? PILOT_ON : PILOT_OFF;
			p->fec_inner = modcod2fec[modcod];
			p->modulation = modcod2mod[modcod];
			p->transmission_mode = pls_code;
		} else {
			p->delivery_system = SYS_DVBS;
			p->rolloff = ROLLOFF_35;
			p->pilot = PILOT_OFF;
			p->fec_inner = dvbs_fec_lut[pls_code & 7];
			p->modulation = QPSK;
		}
		break;
	}
	case SYS_DVBC_ANNEX_A:
		break;
	case SYS_DVBT:
		break;
	case SYS_DVBT2:
		break;
	case SYS_DVBC2:
		break;
	case SYS_ISDBT:
		break;
	}
	p->pre_bit_error.len = 1;
	p->pre_bit_error.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_error.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_numerator;

	p->pre_bit_count.len = 1;
	p->pre_bit_count.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_count.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_denominator;

	p->block_error.len = 1;
	p->block_error.stat[0].scale = FE_SCALE_COUNTER;
	p->block_error.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.packet_errors;
	p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	p->cnr.stat[0].svalue = (s64) mci->
		signal_info.dvbs2_signal_info.signal_to_noise * 10;

	p->strength.len = 1;
	p->strength.stat[0].scale = FE_SCALE_DECIBEL;
	p->strength.stat[0].svalue =
		mci->signal_info.dvbs2_signal_info.channel_power * 10;
}

static void mci_handler(void *priv)
{
	struct mci_base *base = (struct mci_base *)priv;

	complete(&base->completion);
}

static struct mci_base *match_base(void *key)
{
	struct mci_base *p;

	list_for_each_entry(p, &mci_list, mci_list)
		if (p->key == key)
			return p;
	return NULL;
}

static int probe(struct mci *state)
{
	mci_reset(state);
	return 0;
}

struct dvb_frontend *ddb_mci_attach(struct ddb_input *input, struct mci_cfg *cfg, int nr, int tuner)
{
	struct ddb_port *port = input->port;
	struct ddb *dev = port->dev;
	struct ddb_link *link = &dev->link[port->lnr];
	struct mci_base *base;
	struct mci *state;
	void *key = cfg->type ? (void *) port : (void *) link;

	state = kzalloc(cfg->state_size, GFP_KERNEL);
	if (!state)
		return NULL;

	base = match_base(key);
	if (base) {
		base->count++;
		state->base = base;
	} else {
		base = kzalloc(cfg->base_size, GFP_KERNEL);
		if (!base)
			goto fail;
		base->key = key;
		base->count = 1;
		base->link = link;
		mutex_init(&base->mci_lock);
		mutex_init(&base->tuner_lock);
		ddb_irq_set(dev, link->nr, 0, mci_handler, base);
		init_completion(&base->completion);
		state->base = base;
		if (probe(state) < 0) {
			kfree(base);
			goto fail;
		}
		list_add(&base->mci_list, &mci_list);
		if (cfg->base_init)
			cfg->base_init(base);
	}
	memcpy(&state->fe.ops, cfg->fe_ops, sizeof(struct dvb_frontend_ops));
	state->fe.demodulator_priv = state;
	state->nr = nr;
	state->demod = nr;
	state->tuner = tuner;
	if (cfg->init)
		cfg->init(state);
	return &state->fe;
fail:
	kfree(state);
	return NULL;
}

