/*
 * ddbridge-io.h: Digital Devices bridge I/O functions
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
 * along with this program; if not, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _DDBRIDGE_IO_H_
#define _DDBRIDGE_IO_H_

u32 ddblreadl(struct ddb_link *link, u32 adr);
void ddblwritel(struct ddb_link *link, u32 val, u32 adr);
u32 ddbreadl(struct ddb *dev, u32 adr);
void ddbwritel(struct ddb *dev, u32 val, u32 adr);
void gtlcpyto(struct ddb *dev, u32 adr, const u8 *buf,
	      unsigned int count);
void gtlcpyfrom(struct ddb *dev, u8 *buf, u32 adr, long count);
void ddbcpyto(struct ddb *dev, u32 adr, void *src, long count);
void ddbcpyfrom(struct ddb *dev, void *dst, u32 adr, long count);

static inline void ddbwriteb0(struct ddb *dev, u32 val, u32 adr)
{
	writeb(val, dev->regs + adr);
}

static inline u32 ddbreadb0(struct ddb *dev, u32 adr)
{
	return readb(dev->regs + adr);
}

static inline void ddbwritel0(struct ddb *dev, u32 val, u32 adr)
{
	writel(val, dev->regs + adr);
}

static inline u32 ddbreadl0(struct ddb *dev, u32 adr)
{
	return readl(dev->regs + adr);
}

static inline void ddblwritel0(struct ddb_link *link, u32 val, u32 adr)
{
	writel(val, link->dev->regs + adr);
}

static inline u32 ddblreadl0(struct ddb_link *link, u32 adr)
{
	return readl(link->dev->regs + adr);
}

#if 0
static inline void gtlw(struct ddb_link *link)
{
	u32 count = 0;
	static u32 max;

	while (1 & ddblreadl0(link, link->regs + 0x10)) {
		if (++count == 1024) {
			pr_info("LTO\n");
			break;
		}
	}
	if (count > max) {
		max = count;
		pr_info("TO=%u\n", max);
	}
	if (ddblreadl0(link, link->regs + 0x10) & 0x8000)
		pr_err("link error\n");
}
#else
static inline void gtlw(struct ddb_link *link)
{
	while (1 & ddblreadl0(link, link->regs + 0x10))
		;
}
#endif

#define ddbmemset(_dev, _adr, _val, _count) \
	memset_io(((_dev)->regs + (_adr)), (_val), (_count))

#endif
