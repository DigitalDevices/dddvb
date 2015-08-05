/*
 * lnbh25.h
 */

#ifndef _LNBH25_H
#define _LNBH25_H

#include <linux/dvb/frontend.h>

#if defined(CONFIG_DVB_LNBH25) || \
	(defined(CONFIG_DVB_LNBH25_MODULE) && defined(MODULE))

extern struct dvb_frontend *lnbh25_attach(struct dvb_frontend *fe,
					  struct i2c_adapter *i2c,
					  u8 i2c_addr);
#else

static inline struct dvb_frontend *lnbh25_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 u8 i2c_addr)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}

#endif

#endif
