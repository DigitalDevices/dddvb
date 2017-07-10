/*
 * ddbridge-io.h: Digital Devices bridge I/O (inline) functions
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DDBRIDGE_IO_H__
#define __DDBRIDGE_IO_H__

#include <linux/io.h>

#include "ddbridge.h"

/******************************************************************************/

static inline void ddbwriteb(struct ddb *dev, u32 val, u32 adr)
{
	writeb(val, (char *) (dev->regs + (adr)));
}

static inline u32 ddbreadb(struct ddb *dev, u32 adr)
{
	return readb((char *) (dev->regs + (adr)));
}

static inline void ddbwritel0(struct ddb_link *link, u32 val, u32 adr)
{
	writel(val, (char *) (link->dev->regs + (adr)));
}

static inline u32 ddbreadl0(struct ddb_link *link, u32 adr)
{
	return readl((char *) (link->dev->regs + (adr)));
}

static inline void gtlw(struct ddb_link *link)
{
	while (1 & ddbreadl0(link, link->regs + 0x10))
		;
}

/******************************************************************************/

u32 ddblreadl(struct ddb_link *link, u32 adr);
void ddblwritel(struct ddb_link *link, u32 val, u32 adr);
u32 ddbreadl(struct ddb *dev, u32 adr);
void ddbwritel(struct ddb *dev, u32 val, u32 adr);
void ddbcpyto(struct ddb *dev, u32 adr, void *src, long count);
void ddbcpyfrom(struct ddb *dev, void *dst, u32 adr, long count);

/******************************************************************************/

#define ddbmemset(_dev, _adr, _val, _count) \
	memset_io((char *) (_dev->regs + (_adr)), (_val), (_count))

#endif /* __DDBRIDGE_IO_H__ */
