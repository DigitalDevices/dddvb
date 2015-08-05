#ifndef _STV6111_H_
#define _STV6111_H_
struct dvb_frontend *stv6111_attach(struct dvb_frontend *fe,
				    struct i2c_adapter *i2c, u8 adr);
#endif
