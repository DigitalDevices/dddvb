#ifndef _LIBDDDVB_H_
#define _LIBDDDVB_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/net.h>


#define DDDVB_UNDEF (~0U)

#define PARAM_STREAMID   0
#define PARAM_FE         1
#define PARAM_SRC        3
#define PARAM_FEC        4
#define PARAM_FREQ       5
#define PARAM_SR         6
#define PARAM_POL        7
#define PARAM_RO         8

#define PARAM_MSYS       9

#define PARAM_MTYPE     10
#define PARAM_PLTS      11
#define PARAM_BW        12
#define PARAM_BW_HZ     13
#define PARAM_TMODE     14
#define PARAM_GI        15
#define PARAM_PLP       16
#define PARAM_ISI       16
#define PARAM_PLS       17
#define PARAM_T2ID      17
#define PARAM_SM        18
#define PARAM_C2TFT     19
#define PARAM_DS        20
#define PARAM_SPECINV   21

#define PARAM_CI        27
#define PARAM_PMT       28
#define PARAM_PID       29
#define PARAM_APID      30
#define PARAM_DPID      31


#include "dddvb.h"

#if BUILDING_LIBDDDVB
#define LIBDDDVB_EXPORTED __attribute__((__visibility__("default")))
#else
#define LIBDDDVB_EXPORTED
#endif
	
LIBDDDVB_EXPORTED struct dddvb *dddvb_init(char *config, uint32_t flags);
LIBDDDVB_EXPORTED int dddvb_dvb_tune(struct dddvb_fe *fe, struct dddvb_params *p);
LIBDDDVB_EXPORTED struct dddvb_fe *dddvb_fe_alloc(struct dddvb *dd, uint32_t type);
LIBDDDVB_EXPORTED struct dddvb_fe *dddvb_fe_alloc_num(struct dddvb *dd, uint32_t type, uint32_t num);
LIBDDDVB_EXPORTED int dddvb_ca_write(struct dddvb *dd, uint32_t nr, uint8_t *buf, uint32_t len);
LIBDDDVB_EXPORTED int dddvb_ca_read(struct dddvb *dd, uint32_t nr, uint8_t *buf, uint32_t len);
LIBDDDVB_EXPORTED int dddvb_ca_set_pmts(struct dddvb *dd, uint32_t nr, uint8_t **pmts);


static inline void dddvb_set_frequency(struct dddvb_params *p, uint32_t freq) {
	p->param[PARAM_FREQ] = freq;
};

static inline void dddvb_set_bandwidth(struct dddvb_params *p, uint32_t bandw) {
	p->param[PARAM_BW_HZ] = bandw;
};

static inline void dddvb_set_symbol_rate(struct dddvb_params *p, uint32_t srate) {
	p->param[PARAM_SR] = srate;
};

static inline void dddvb_set_delsys(struct dddvb_params *p, enum fe_delivery_system delsys) {
	p->param[PARAM_MSYS] = delsys;
};

static inline void dddvb_set_polarization(struct dddvb_params *p, uint32_t pol) {
	p->param[PARAM_POL] = pol;
};

static inline void dddvb_set_src(struct dddvb_params *p, uint32_t src) {
	p->param[PARAM_SRC] = src;
};

static inline void dddvb_set_fec(struct dddvb_params *p, enum fe_code_rate fec) {
	p->param[PARAM_FEC] = fec;
};

static inline void dddvb_set_pls(struct dddvb_params *p, uint32_t pls) {
	p->param[PARAM_PLS] = pls;
};

static inline void dddvb_set_id(struct dddvb_params *p, uint32_t id) {
	p->param[PARAM_ISI] = id;
};

static inline uint32_t dddvb_get_stat(struct dddvb_fe *fe) {
	return fe->stat;
};

static inline int64_t dddvb_get_strength(struct dddvb_fe *fe) {
	return fe->strength;
};

static inline int64_t dddvb_get_cnr(struct dddvb_fe *fe) {
	return fe->cnr;
};

static inline int64_t dddvb_get_ber(struct dddvb_fe *fe) {
	return fe->ber;
};

static inline uint32_t dddvb_get_quality(struct dddvb_fe *fe) {
	return fe->quality;
};

static inline void dddvb_param_init(struct dddvb_params *p) {
	int i;

	for (i = 0; i < 32; i++) 
		p->param[i] = DDDVB_UNDEF;
};

#if 0
static inline int dddvb_ca_write(struct dddvb *dd, uint32_t nr, uint8_t *buf, uint32_t len) {
	return ca_write(dd, nr, buf, len);
};
#endif

#endif /* _LIBDDDVB_H_ */
