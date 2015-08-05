#ifndef _MXL5XX_H_
#define _MXL5XX_H_

#include <linux/types.h>
#include <linux/i2c.h>

struct mxl5xx_cfg {
	u8   adr;
	u8   type;
	u32  cap;
	u32  clk;
	u32  ts_clk;
	
	u8  *fw;
	u32  fw_len;

	int (*fw_read)(void *priv, u8 *buf, u32 len);
	void *fw_priv;
};

#if defined(CONFIG_DVB_MXL5XX) || \
	(defined(CONFIG_DVB_MXL5XX_MODULE) && defined(MODULE))

extern struct dvb_frontend *mxl5xx_attach(struct i2c_adapter *i2c,
					  struct mxl5xx_cfg *cfg,
					  u32 demod, u32 tuner);
#else

static inline struct dvb_frontend *mxl5xx_attach(struct i2c_adapter *i2c,
						 struct mxl5xx_cfg *cfg,
						 u32 demod, u32 tuner)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}

#endif

#endif
