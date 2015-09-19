/*
 * ddbridge-i2c.c: Digital Devices bridge i2c driver
 *
 * Copyright (C) 2010-2015 Digital Devices GmbH
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

static int i2c_io(struct i2c_adapter *adapter, u8 adr,
		  u8 *wbuf, u32 wlen, u8 *rbuf, u32 rlen)
{
	struct i2c_msg msgs[2] = {{.addr = adr,  .flags = 0,
				   .buf  = wbuf, .len   = wlen },
				  {.addr = adr,  .flags = I2C_M_RD,
				   .buf  = rbuf,  .len   = rlen } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len)
{
	struct i2c_msg msg = {.addr = adr, .flags = 0,
			      .buf = data, .len = len};

	return (i2c_transfer(adap, &msg, 1) == 1) ? 0 : -1;
}

static int i2c_read(struct i2c_adapter *adapter, u8 adr, u8 *val)
{
	struct i2c_msg msgs[1] = {{.addr = adr,  .flags = I2C_M_RD,
				   .buf  = val,  .len   = 1 } };
	return (i2c_transfer(adapter, msgs, 1) == 1) ? 0 : -1;
}

static int i2c_read_regs(struct i2c_adapter *adapter,
			 u8 adr, u8 reg, u8 *val, u8 len)
{
	struct i2c_msg msgs[2] = {{.addr = adr,  .flags = 0,
				   .buf  = &reg, .len   = 1 },
				  {.addr = adr,  .flags = I2C_M_RD,
				   .buf  = val,  .len   = len } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int i2c_read_regs16(struct i2c_adapter *adapter,
			   u8 adr, u16 reg, u8 *val, u8 len)
{
	u8 reg16[2] = { reg >> 8, reg };
	struct i2c_msg msgs[2] = {{.addr = adr,  .flags = 0,
				   .buf  = reg16, .len   = 2 },
				  {.addr = adr,  .flags = I2C_M_RD,
				   .buf  = val,  .len   = len } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int i2c_read_reg(struct i2c_adapter *adapter, u8 adr, u8 reg, u8 *val)
{
	struct i2c_msg msgs[2] = {{.addr = adr,  .flags = 0,
				   .buf  = &reg, .len   = 1},
				  {.addr = adr,  .flags = I2C_M_RD,
				   .buf  = val,  .len   = 1 } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int i2c_read_reg16(struct i2c_adapter *adapter, u8 adr,
			  u16 reg, u8 *val)
{
	u8 msg[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msgs[2] = {{.addr = adr, .flags = 0,
				   .buf  = msg, .len   = 2},
				  {.addr = adr, .flags = I2C_M_RD,
				   .buf  = val, .len   = 1 } };
	return (i2c_transfer(adapter, msgs, 2) == 2) ? 0 : -1;
}

static int i2c_write_reg16(struct i2c_adapter *adap, u8 adr,
			   u16 reg, u8 val)
{
	u8 msg[3] = {reg >> 8, reg & 0xff, val};

	return i2c_write(adap, adr, msg, 3);
}

static int i2c_write_reg(struct i2c_adapter *adap, u8 adr,
			 u8 reg, u8 val)
{
	u8 msg[2] = {reg, val};

	return i2c_write(adap, adr, msg, 2);
}

static int ddb_i2c_cmd(struct ddb_i2c *i2c, u32 adr, u32 cmd)
{
	struct ddb *dev = i2c->dev;
	unsigned long stat;
	u32 val;

	ddbwritel(dev, (adr << 9) | cmd, i2c->regs + I2C_COMMAND);
	stat = wait_for_completion_timeout(&i2c->completion, HZ);
	val = ddbreadl(dev, i2c->regs + I2C_COMMAND);
	if (stat == 0) {
		pr_err("DDBridge I2C timeout, card %d, port %d, link %u\n",
		       dev->nr, i2c->nr, i2c->link);
#if 1
		{ 
			u32 istat = ddbreadl(dev, INTERRUPT_STATUS);
			
			dev_err(dev->dev, "DDBridge IRS %08x\n", istat);
			if (i2c->link) {
				u32 listat = ddbreadl(dev, DDB_LINK_TAG(i2c->link) | INTERRUPT_STATUS);
				dev_err(dev->dev, "DDBridge link %u IRS %08x\n",
					i2c->link, listat);
			}
			if (istat & 1) {
				ddbwritel(dev, istat & 1, INTERRUPT_ACK);
			} else {
				u32 mon = ddbreadl(dev, i2c->regs + I2C_MONITOR);

				dev_err(dev->dev, "I2C cmd=%08x mon=%08x\n",
					val, mon);
			}
		}
#endif
		return -EIO;
	}
	if (val & 0x70000)
		return -EIO;
	return 0;
}

static int ddb_i2c_master_xfer(struct i2c_adapter *adapter,
			       struct i2c_msg msg[], int num)
{
	struct ddb_i2c *i2c = (struct ddb_i2c *) i2c_get_adapdata(adapter);
	struct ddb *dev = i2c->dev;
	u8 addr = 0;
	
	if (num != 1 && num != 2)
		return -EIO;
	addr = msg[0].addr;
	if (msg[0].len > i2c->bsize)
		return -EIO;
	if (num == 2 && msg[1].flags & I2C_M_RD &&
	    !(msg[0].flags & I2C_M_RD)) {
		if (msg[1].len > i2c->bsize)
			return -EIO;
		ddbcpyto(dev, i2c->wbuf, msg[0].buf, msg[0].len);
		ddbwritel(dev, msg[0].len | (msg[1].len << 16),
			  i2c->regs + I2C_TASKLENGTH);
		if (!ddb_i2c_cmd(i2c, addr, 1)) {
			ddbcpyfrom(dev, msg[1].buf,
				   i2c->rbuf,
				   msg[1].len);
			return num;
		}
	}
	if (num == 1 && !(msg[0].flags & I2C_M_RD)) {
		ddbcpyto(dev, i2c->wbuf, msg[0].buf, msg[0].len);
		ddbwritel(dev, msg[0].len, i2c->regs + I2C_TASKLENGTH);
		if (!ddb_i2c_cmd(i2c, addr, 2))
			return num;
	}
	if (num == 1 && (msg[0].flags & I2C_M_RD)) {
		ddbwritel(dev, msg[0].len << 16, i2c->regs + I2C_TASKLENGTH);
		if (!ddb_i2c_cmd(i2c, addr, 3)) {
			ddbcpyfrom(dev, msg[0].buf,
				   i2c->rbuf, msg[0].len);
			return num;
		}
	}
	return -EIO;
}

static u32 ddb_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

struct i2c_algorithm ddb_i2c_algo = {
	.master_xfer   = ddb_i2c_master_xfer,
	.functionality = ddb_i2c_functionality,
};

static void ddb_i2c_release(struct ddb *dev)
{
	int i;
	struct ddb_i2c *i2c;

	for (i = 0; i < dev->i2c_num; i++) {
		i2c = &dev->i2c[i];
		i2c_del_adapter(&i2c->adap);
	}
}

static void i2c_handler(unsigned long priv)
{
	struct ddb_i2c *i2c = (struct ddb_i2c *) priv;

	complete(&i2c->completion);
}

static int ddb_i2c_add(struct ddb *dev, struct ddb_i2c *i2c,
		       struct ddb_regmap *regmap, int link, int i, int num)
{
	struct i2c_adapter *adap;
	
	i2c->nr = i;
	i2c->dev = dev;
	i2c->link = link;
	i2c->bsize = regmap->i2c_buf->size;
	i2c->wbuf = DDB_LINK_TAG(link) | (regmap->i2c_buf->base + i2c->bsize * i);
	i2c->rbuf = i2c->wbuf;// + i2c->bsize / 2;
	i2c->regs = DDB_LINK_TAG(link) | (regmap->i2c->base + regmap->i2c->size * i);
	ddbwritel(dev, I2C_SPEED_100, i2c->regs + I2C_TIMING);
	ddbwritel(dev, ((i2c->rbuf & 0xffff) << 16) | (i2c->wbuf & 0xffff),
		  i2c->regs + I2C_TASKADDRESS);
	init_completion(&i2c->completion);
	
	adap = &i2c->adap;
	i2c_set_adapdata(adap, i2c);
#ifdef I2C_ADAP_CLASS_TV_DIGITAL
	adap->class = I2C_ADAP_CLASS_TV_DIGITAL|I2C_CLASS_TV_ANALOG;
#else
#ifdef I2C_CLASS_TV_ANALOG
	adap->class = I2C_CLASS_TV_ANALOG;
#endif
#endif
	strcpy(adap->name, "ddbridge");
	adap->algo = &ddb_i2c_algo;
	adap->algo_data = (void *)i2c;
	adap->dev.parent = dev->dev;
	return i2c_add_adapter(adap);
}

static int ddb_i2c_init(struct ddb *dev)
{
	int stat = 0;
	u32 i, j, num = 0, l;
	struct ddb_i2c *i2c;
	struct i2c_adapter *adap;
	struct ddb_regmap *regmap;
	
	for (l = 0; l < DDB_MAX_LINK; l++) {
		if (!dev->link[l].info)
			continue;
		regmap = dev->link[l].info->regmap;
		if (!regmap || !regmap->i2c)
			continue;
		for (i = 0; i < regmap->i2c->num; i++) {
			if (!(dev->link[l].info->i2c_mask & (1 << i)))
				continue;
			i2c = &dev->i2c[num];
			dev->handler_data[i + l * 32] = (unsigned long) i2c;
			dev->handler[i + l * 32] = i2c_handler;
			stat = ddb_i2c_add(dev, i2c, regmap, l, i, num);
			if (stat)
				break;
			num++;
		}
	}
	if (stat) {
		for (j = 0; j < num; j++) {
			i2c = &dev->i2c[j];
			adap = &i2c->adap;
			i2c_del_adapter(adap);
		}
	} else 
		dev->i2c_num = num;
	return stat;
}

