#ifndef _STV0367DD_H_
#define _STV0367DD_H_

#include <linux/types.h>
#include <linux/i2c.h>

struct stv0367_cfg {
	u8  adr;
	u32 xtal;
	u8 parallel;
	u8 cont_clock;
};


extern struct dvb_frontend *stv0367_attach(struct i2c_adapter *i2c,
					   struct stv0367_cfg *cfg,
					   struct dvb_frontend **fe_t);
#endif
