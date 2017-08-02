/*
 * ddbridge-io.c: Digital Devices bridge I/O functions
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
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

#include "ddbridge.h"
#include "ddbridge-io.h"

u32 ddblreadl(struct ddb_link *link, u32 adr)
{
	if (unlikely(link->nr)) {
		unsigned long flags;
		u32 val;

		spin_lock_irqsave(&link->lock, flags);
		gtlw(link);
		ddbwritel0(link, adr & 0xfffc, link->regs + 0x14);
		ddbwritel0(link, 3, link->regs + 0x10);
		gtlw(link);
		val = ddbreadl0(link, link->regs + 0x1c);
		spin_unlock_irqrestore(&link->lock, flags);
		return val;
	}
	return readl((char *) (link->dev->regs + (adr)));
}

void ddblwritel(struct ddb_link *link, u32 val, u32 adr)
{
	if (unlikely(link->nr)) {
		unsigned long flags;

		spin_lock_irqsave(&link->lock, flags);
		gtlw(link);
		ddbwritel0(link, 0xf0000 | (adr & 0xfffc), link->regs + 0x14);
		ddbwritel0(link, val, link->regs + 0x18);
		ddbwritel0(link, 1, link->regs + 0x10);
		spin_unlock_irqrestore(&link->lock, flags);
		return;
	}
	writel(val, (char *) (link->dev->regs + (adr)));
}

u32 ddbreadl(struct ddb *dev, u32 adr)
{
	if (unlikely(adr & 0xf0000000)) {
		unsigned long flags;
		u32 val, l = (adr >> DDB_LINK_SHIFT);
		struct ddb_link *link = &dev->link[l];

		spin_lock_irqsave(&link->lock, flags);
		gtlw(link);
		ddbwritel0(link, adr & 0xfffc, link->regs + 0x14);
		ddbwritel0(link, 3, link->regs + 0x10);
		gtlw(link);
		val = ddbreadl0(link, link->regs + 0x1c);
		spin_unlock_irqrestore(&link->lock, flags);
		return val;
	}
	return readl((char *) (dev->regs + (adr)));
}

void ddbwritel(struct ddb *dev, u32 val, u32 adr)
{
	if (unlikely(adr & 0xf0000000)) {
		unsigned long flags;
		u32 l = (adr >> DDB_LINK_SHIFT);
		struct ddb_link *link = &dev->link[l];

		spin_lock_irqsave(&link->lock, flags);
		gtlw(link);
		ddbwritel0(link, 0xf0000 | (adr & 0xfffc), link->regs + 0x14);
		ddbwritel0(link, val, link->regs + 0x18);
		ddbwritel0(link, 1, link->regs + 0x10);
		spin_unlock_irqrestore(&link->lock, flags);
		return;
	}
	writel(val, (char *) (dev->regs + (adr)));
}

void gtlcpyto(struct ddb *dev, u32 adr, const u8 *buf,
	      unsigned int count)
{
	u32 val = 0, p = adr;
	u32 aa = p & 3;

	if (aa) {
		while (p & 3 && count) {
			val >>= 8;
			val |= *buf << 24;
			p++;
			buf++;
			count--;
		}
		ddbwritel(dev, val, adr);
	}
	while (count >= 4) {
		val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
		ddbwritel(dev, val, p);
		p += 4;
		buf += 4;
		count -= 4;
	}
	if (count) {
		val = buf[0];
		if (count > 1)
			val |= buf[1] << 8;
		if (count > 2)
			val |= buf[2] << 16;
		ddbwritel(dev, val, p);
	}
}

void gtlcpyfrom(struct ddb *dev, u8 *buf, u32 adr, long count)
{
	u32 val = 0, p = adr;
	u32 a = p & 3;

	if (a) {
		val = ddbreadl(dev, p) >> (8 * a);
		while (p & 3 && count) {
			*buf = val & 0xff;
			val >>= 8;
			p++;
			buf++;
			count--;
		}
	}
	while (count >= 4) {
		val = ddbreadl(dev, p);
		buf[0] = val & 0xff;
		buf[1] = (val >> 8) & 0xff;
		buf[2] = (val >> 16) & 0xff;
		buf[3] = (val >> 24) & 0xff;
		p += 4;
		buf += 4;
		count -= 4;
	}
	if (count) {
		val = ddbreadl(dev, p);
		buf[0] = val & 0xff;
		if (count > 1)
			buf[1] = (val >> 8) & 0xff;
		if (count > 2)
			buf[2] = (val >> 16) & 0xff;
	}
}

void ddbcpyto(struct ddb *dev, u32 adr, void *src, long count)
{
	if (unlikely(adr & 0xf0000000))
		return gtlcpyto(dev, adr, src, count);
	return memcpy_toio((char *) (dev->regs + adr), src, count);
}

void ddbcpyfrom(struct ddb *dev, void *dst, u32 adr, long count)
{
	if (unlikely(adr & 0xf0000000))
		return gtlcpyfrom(dev, dst, adr, count);
	return memcpy_fromio(dst, (char *) (dev->regs + adr), count);
}



