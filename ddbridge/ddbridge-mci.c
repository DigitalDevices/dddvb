// SPDX-License-Identifier: GPL-2.0
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

static int mci_reset(struct ddb_link *link)
{
	const struct ddb_regmap *regmap = link->info->regmap;
	u32 control;
	u32 status = 0;
	u32 timeout = 40;
	union {
		u32 u[4];
		char s[16];
	} version;
	u32 vaddr;
	
	if (!regmap || !regmap->mci)
		return -EINVAL;
	control = regmap->mci->base;
	vaddr = regmap->mci_buf->base + 0xf0;

	if ((link->info->type == DDB_OCTOPUS_MCI) &&
	    (ddblreadl(link, control) & MCI_CONTROL_START_COMMAND)) {
		ddblwritel(link, MCI_CONTROL_RESET, control);
		ddblwritel(link, 0, control + 4); /* 1= no internal init */
		msleep(300);
	}
	ddblwritel(link, 0, control);
	while (1) {
		status = ddblreadl(link, control);
		if ((status & MCI_CONTROL_READY) == MCI_CONTROL_READY)
			break;
		if (--timeout == 0)
			break;
		msleep(50);
	}
	dev_info(link->dev->dev, "MCI control port @ %08x\n", control);

	if ((status & MCI_CONTROL_READY) == 0) {
		dev_err(link->dev->dev, "MCI init failed!\n");
		return -1;
	}
	version.u[0] = ddblreadl(link, vaddr);
	version.u[1] = ddblreadl(link, vaddr + 4);
	version.u[2] = ddblreadl(link, vaddr + 8);
	version.u[3] = ddblreadl(link, vaddr + 12);
	dev_info(link->dev->dev, "MCI port OK, init time %u msecs\n", (40 - timeout) * 50);
	dev_info(link->dev->dev, "MCI firmware version %s.%d\n", version.s, version.s[15]);
	return 0;
}

static int ddb_mci_cmd_raw_unlocked(struct ddb_link *link,
				    u32 *cmd, u32 cmd_len,
				    u32 *res, u32 res_len)
{
	const struct ddb_regmap *regmap = link->info->regmap;
	u32 control, command, result;
	u32 i, val;
	unsigned long stat;

	if (!regmap || ! regmap->mci)
		return -EINVAL;
	control = regmap->mci->base;
	command = regmap->mci_buf->base;
	result = command + MCI_COMMAND_SIZE;
	val = ddblreadl(link, control);
	if (val & (MCI_CONTROL_RESET | MCI_CONTROL_START_COMMAND))
		return -EIO;
	if (cmd && cmd_len)
		for (i = 0; i < cmd_len; i++)
			ddblwritel(link, cmd[i], command + i * 4);
	val |= (MCI_CONTROL_START_COMMAND |
		MCI_CONTROL_ENABLE_DONE_INTERRUPT);
	ddblwritel(link, val, control);

	stat = wait_for_completion_timeout(&link->mci_completion, HZ);
	if (stat == 0) {
		u32 istat = ddblreadl(link, INTERRUPT_STATUS);

		dev_err(link->dev->dev, "MCI timeout\n");
		val = ddblreadl(link, control);
		if (val == 0xffffffff) {
			dev_err(link->dev->dev,
				"Lost PCIe link!\n");
			return -EIO;
		} else {
			dev_err(link->dev->dev,
				"DDBridge IRS %08x link %u\n",
				istat, link->nr);
			if (istat & 1)
				ddblwritel(link, istat, INTERRUPT_ACK);
			if (link->nr)
				ddbwritel(link->dev,
					  0xffffff, INTERRUPT_ACK);
		}
	}
	//print_hex_dump(KERN_INFO, "MCI", DUMP_PREFIX_OFFSET, 16, 1, cmd, cmd_len, false);
	if (res && res_len)
		for (i = 0; i < res_len; i++)
			res[i] = ddblreadl(link, result + i * 4);
	return 0;
}

int ddb_mci_cmd_link(struct ddb_link *link,
		     struct mci_command *command,
		     struct mci_result *result)
{
	struct mci_result res;
	int stat;

	if (!result)
		result = &res;
	mutex_lock(&link->mci_lock);
	stat = ddb_mci_cmd_raw_unlocked(link,
					(u32 *)command,
					sizeof(*command)/sizeof(u32),
					(u32 *)result,
					sizeof(*result)/sizeof(u32));
	mutex_unlock(&link->mci_lock);
	if (command && result && (result->status & 0x80))
		dev_warn(link->dev->dev,
			 "mci_command 0x%02x, error=0x%02x\n",
			 command->command, result->status);
	return stat;
}

static void mci_handler(void *priv)
{
	struct ddb_link *link = (struct ddb_link *) priv;

	complete(&link->mci_completion);
}

int mci_init(struct ddb_link *link)
{
	int result;
	
	mutex_init(&link->mci_lock);
	init_completion(&link->mci_completion);
	result = mci_reset(link);
	if (result < 0)
		return result;
	if (link->ids.device == 0x0009  || link->ids.device == 0x000b)
		ddblwritel(link, SX8_TSCONFIG_MODE_NORMAL, SX8_TSCONFIG);
	
	ddb_irq_set(link->dev, link->nr,
		    link->info->regmap->irq_base_mci,
		    mci_handler, link);
	link->mci_ok = 1;
	return result;
}

int mci_cmd_val(struct ddb_link *link, uint32_t cmd, uint32_t val)
{
	struct mci_result result;
	struct mci_command command;

	command.command_word = cmd;
	command.params[0] = val;
	return ddb_mci_cmd_link(link, &command, &result);
}

/****************************************************************************/
/****************************************************************************/

int ddb_mci_cmd(struct mci *state,
		struct mci_command *command,
		struct mci_result *result)
{
	return ddb_mci_cmd_link(state->base->link, command, result);
}


int ddb_mci_cmd_raw(struct mci *state,
		    struct mci_command *command, u32 command_len,
		    struct mci_result *result, u32 result_len)
{
	struct ddb_link *link = state->base->link;
	int stat;

	mutex_lock(&link->mci_lock);
	stat = ddb_mci_cmd_raw_unlocked(link,
					(u32 *)command, command_len,
					(u32 *)result, result_len);
	mutex_unlock(&link->mci_lock);
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
	p->cnr.stat[0].svalue =
		(s64) mci->signal_info.dvbs2_signal_info.signal_to_noise * 10;
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
		FEC_7_8, FEC_7_8, FEC_NONE, FEC_NONE,
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
		p->inversion = (mci->signal_info.dvbs2_signal_info.roll_off & 0x80) ?
			INVERSION_ON : INVERSION_OFF;
		if (mci->signal_info.dvbs2_signal_info.standard == 2) {
			u32 modcod;

			p->delivery_system = SYS_DVBS2;
			p->transmission_mode = pls_code;
			p->rolloff =
				ro_lut[mci->signal_info.dvbs2_signal_info.roll_off & 7];
			p->pilot = (pls_code & 1) ? PILOT_ON : PILOT_OFF;
			if (pls_code & 0x80) {
				/* no suitable values defined in Linux DVB API yet */
				/* modcod = (0x7f & pls_code) >> 1; */
				p->fec_inner = FEC_NONE;
				p->modulation = 0;
				if (pls_code >= 250)
					p->pilot = PILOT_ON;
			} else {
				modcod = (0x7c & pls_code) >> 2;
				p->fec_inner = modcod2fec[modcod];
				p->modulation = modcod2mod[modcod];
			}
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
	/* post is correct, we cannot provide both pre and post at the same time  */
	/* set pre and post the same for now */
	p->pre_bit_error.len = 1;
	p->pre_bit_error.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_error.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_numerator;

	p->pre_bit_count.len = 1;
	p->pre_bit_count.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_count.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_denominator;

	p->post_bit_error.len = 1;
	p->post_bit_error.stat[0].scale = FE_SCALE_COUNTER;
	p->post_bit_error.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_numerator;

	p->post_bit_count.len = 1;
	p->post_bit_count.stat[0].scale = FE_SCALE_COUNTER;
	p->post_bit_count.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.ber_denominator;

	p->block_error.len = 1;
	p->block_error.stat[0].scale = FE_SCALE_COUNTER;
	p->block_error.stat[0].uvalue =
		mci->signal_info.dvbs2_signal_info.packet_errors;
	p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;

	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	p->cnr.stat[0].svalue = (s64)
		mci->signal_info.dvbs2_signal_info.signal_to_noise * 10;

	p->strength.len = 1;
	p->strength.stat[0].scale = FE_SCALE_DECIBEL;
	p->strength.stat[0].svalue = (s64)
		mci->signal_info.dvbs2_signal_info.channel_power * 10;
}

static struct mci_base *match_base(void *key)
{
	struct mci_base *p;

	list_for_each_entry(p, &mci_list, mci_list)
		if (p->key == key)
			return p;
	return NULL;
}

struct dvb_frontend *ddb_mci_attach(struct ddb_input *input,
				    struct mci_cfg *cfg, int nr, int tuner)
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
		link->mci_base = base;
		mutex_init(&base->tuner_lock);
		state->base = base;

		if (!link->mci_ok) {
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
	state->input = input;
	if (cfg->init)
		cfg->init(state);
	return &state->fe;
fail:
	kfree(state);
	return NULL;
}

