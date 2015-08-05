/*
 * Driver for the ST LNBH25
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


#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "dvb_frontend.h"
#include "lnbh25.h"

struct lnbh25 {
	struct i2c_adapter	*i2c;
	u8			 adr;
	u8                       reg[4];
	u8                       boost;
};

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	if (i2c_transfer(adap, &msg, 1) != 1) {
		pr_err("lnbh25: i2c_write error\n");
		return -1;
	}
	return 0;
}

static int lnbh25_write_regs(struct lnbh25 *lnbh, int reg, int len)
{
	u8 d[5];

	memcpy(&d[1], &lnbh->reg[reg], len);
	d[0] = reg + 2;
	return i2c_write(lnbh->i2c, lnbh->adr, d, len + 1);
}

static int lnbh25_set_voltage(struct dvb_frontend *fe,
			      fe_sec_voltage_t voltage)
{
	struct lnbh25 *lnbh = (struct lnbh25 *) fe->sec_priv;
	u8 oldreg0 = lnbh->reg[0];

	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		lnbh->reg[0] = 0x00;
		lnbh->reg[1] &= ~0x01;    /* Disable Tone */
		lnbh->reg[2] = 0x00;
		return lnbh25_write_regs(lnbh, 0, 3);
	case SEC_VOLTAGE_13:
		lnbh->reg[0] = lnbh->boost + 1;
		break;
	case SEC_VOLTAGE_18:
		lnbh->reg[0] = lnbh->boost + 8;
		break;
	default:
		return -EINVAL;
	};

	if (lnbh->reg[0] == 0x00) {
		lnbh->reg[2] = 4;
		lnbh25_write_regs(lnbh, 2, 2);
	} else if (lnbh->reg[2] != 0x00) {
		lnbh->reg[2] = 0;
		lnbh25_write_regs(lnbh, 2, 2);
	}
	lnbh->reg[1] |= 0x01;
	lnbh25_write_regs(lnbh, 0, 3);
	if (oldreg0 == 0) 
		msleep(100);
	return 0;
}

static int lnbh25_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	struct lnbh25 *lnbh = (struct lnbh25 *) fe->sec_priv;

	lnbh->boost = arg ? 3 : 0;

	return 0;
}

static int lnbh25_set_tone(struct dvb_frontend *fe,
			   fe_sec_tone_mode_t tone)
{
	/* struct lnbh25 *lnbh = (struct lnbh25 *) fe->sec_priv; */

	return 0;
}

static int lnbh25_init(struct lnbh25 *lnbh)
{
	return lnbh25_write_regs(lnbh, 0, 2);
}

static void lnbh25_release(struct dvb_frontend *fe)
{
	kfree(fe->sec_priv);
	fe->sec_priv = NULL;
}

struct dvb_frontend *lnbh25_attach(struct dvb_frontend *fe,
				   struct i2c_adapter *i2c,
				   u8 adr)
{
	struct lnbh25 *lnbh = kzalloc(sizeof(struct lnbh25), GFP_KERNEL);
	if (!lnbh)
		return NULL;

	lnbh->i2c = i2c;
	lnbh->adr = adr;
	lnbh->boost = 3;

	if (lnbh25_init(lnbh)) {
		kfree(lnbh);
		return NULL;
	}
	fe->sec_priv = lnbh;
	fe->ops.set_voltage = lnbh25_set_voltage;
	fe->ops.enable_high_lnb_voltage = lnbh25_enable_high_lnb_voltage;
	fe->ops.release_sec = lnbh25_release;

	pr_info("LNB25 on %02x\n", lnbh->adr);

	return fe;
}
EXPORT_SYMBOL(lnbh25_attach);

MODULE_DESCRIPTION("LNBH25");
MODULE_AUTHOR("Ralph Metzler");
MODULE_LICENSE("GPL");
