#ifndef _TDA18271C2DD_H_
#define _TDA18271C2DD_H_

#include <linux/types.h>
#include <linux/i2c.h>

#if defined(CONFIG_DVB_TDA18271C2DD) ||			\
	(defined(CONFIG_DVB_TDA18271C2DD_MODULE)	\
	 && defined(MODULE))
struct dvb_frontend *tda18271c2dd_attach(struct dvb_frontend *fe,
					 struct i2c_adapter *i2c, u8 adr);
#else
static inline struct dvb_frontend *tda18271c2dd_attach(struct dvb_frontend *fe,
					 struct i2c_adapter *i2c, u8 adr)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
