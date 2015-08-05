/*
 * stv0367.h
 *
 * Driver for ST STV0367 DVB-T & DVB-C demodulator IC.
 *
 * Copyright (C) ST Microelectronics.
 * Copyright (C) 2010,2011 NetUP Inc.
 * Copyright (C) 2010,2011 Igor M. Liplianin <liplianin@netup.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef STV0367_H
#define STV0367_H

#include <linux/dvb/frontend.h>
#include "dvb_frontend.h"

enum stv0367_ts_mode {
	STV0367_OUTPUTMODE_DEFAULT,
	STV0367_SERIAL_PUNCT_CLOCK,
	STV0367_SERIAL_CONT_CLOCK,
	STV0367_PARALLEL_PUNCT_CLOCK,
	STV0367_DVBCI_CLOCK
};

enum stv0367_clk_pol {
	STV0367_CLOCKPOLARITY_DEFAULT,
	STV0367_RISINGEDGE_CLOCK,
	STV0367_FALLINGEDGE_CLOCK
};

struct stv0367_config {
	u8 demod_address;
	u32 xtal;
	u32 if_khz;/*4500*/
	int if_iq_mode;
	int ts_mode;
	int clk_pol;
};

enum stv0367_ter_if_iq_mode {
	FE_TER_NORMAL_IF_TUNER = 0,
	FE_TER_LONGPATH_IF_TUNER = 1,
	FE_TER_IQ_TUNER = 2

};



#if defined(CONFIG_DVB_STV0367) || (defined(CONFIG_DVB_STV0367_MODULE) \
							&& defined(MODULE))
extern struct
dvb_frontend *stv0367ter_attach(const struct stv0367_config *config,
					struct i2c_adapter *i2c);
extern struct
dvb_frontend *stv0367cab_attach(const struct stv0367_config *config,
					struct i2c_adapter *i2c);
#else
static inline struct
dvb_frontend *stv0367ter_attach(const struct stv0367_config *config,
					struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct
dvb_frontend *stv0367cab_attach(const struct stv0367_config *config,
					struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
