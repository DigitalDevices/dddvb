#include "libdddvb.h"
#include "dddvb.h"
#include "tools.h"
#include "debug.h"

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/net.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DTV_SCRAMBLING_SEQUENCE_INDEX 70
#define DTV_INPUT                     71
#define SYS_DVBC2                    19

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))



/******************************************************************************/


static int set_property(int fd, uint32_t cmd, uint32_t data)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	p.u.data = data;
	ret = ioctl(fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}


static int get_property(int fd, uint32_t cmd, uint32_t *data)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	ret = ioctl(fd, FE_GET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_GET_PROPERTY returned %d\n", ret);
		return -1;
	}
	*data = p.u.data;
	return 0;
}

static int get_stat(int fd, uint32_t cmd, struct dtv_fe_stats *stats)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = 1;
	c.props = &p;
	ret = ioctl(fd, FE_GET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_GET_PROPERTY returned %d\n", ret);
		return -1;
	}
	memcpy(stats, &p.u.st, sizeof(struct dtv_fe_stats));
	return 0;
}

static int get_stat_num(int fd, uint32_t cmd, struct dtv_fe_stats *stats, int num)
{
	struct dtv_property p;
	struct dtv_properties c;
	int ret;

	p.cmd = cmd;
	c.num = num;
	c.props = &p;
	ret = ioctl(fd, FE_GET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_GET_PROPERTY returned %d\n", ret);
		return -1;
	}
	memcpy(stats, &p.u.st, num*sizeof(struct dtv_fe_stats));
	return 0;
}



static int set_fe_input(struct dddvb_fe *fe, uint32_t fr,
			uint32_t sr, fe_delivery_system_t ds,
			uint32_t input)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_DELIVERY_SYSTEM, .u.data = ds },
		{ .cmd = DTV_FREQUENCY, .u.data = fr },
		{ .cmd = DTV_INVERSION, .u.data = INVERSION_AUTO },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = sr },
		{ .cmd = DTV_INNER_FEC, .u.data = FEC_AUTO },
		{ .cmd = DTV_ROLLOFF, .u.data = ROLLOFF_AUTO },
	};		
	struct dtv_properties c;
	int ret;
	int fd = fe->fd;
	
	if (fe->param.param[PARAM_FEC] != DDDVB_UNDEF)
		p[5].u.data = fe->param.param[PARAM_FEC];
	
	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	if (input != DDDVB_UNDEF)
		set_property(fd, DTV_INPUT, input);
	//fprintf(stderr, "bw =%u\n", fe->param.param[PARAM_BW_HZ]);
	if (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF)
		set_property(fd, DTV_BANDWIDTH_HZ, fe->param.param[PARAM_BW_HZ]);
	if (fe->param.param[PARAM_ISI] != DDDVB_UNDEF)
		set_property(fd, DTV_STREAM_ID, fe->param.param[PARAM_ISI]);
	if (fe->param.param[PARAM_SSI] != DDDVB_UNDEF)
		set_property(fd, DTV_SCRAMBLING_SEQUENCE_INDEX,
			     fe->param.param[PARAM_SSI]);
	if (fe->param.param[PARAM_MTYPE] != DDDVB_UNDEF)
		set_property(fd, DTV_MODULATION, fe->param.param[PARAM_MTYPE]);
	set_property(fd, DTV_TUNE, 0);
	return 0;
}

static void diseqc_send_msg(int fd, fe_sec_voltage_t v, 
			    struct dvb_diseqc_master_cmd *cmd,
			    fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b, 
			    int wait)
{
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15 * 1000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(wait * 1000);
	usleep(15 * 1000);
	if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
		perror("FE_DISEQC_SEND_BURST failed");
	usleep(15 * 1000);
	if (ioctl(fd, FE_SET_TONE, t) == -1)
		perror("FE_SET_TONE failed");
}

static int diseqc(int fd, int sat, int hor, int band)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00},
		.msg_len = 4
	};

	hor &= 1;
	cmd.msg[3] = 0xf0 | ( ((sat << 2) & 0x0c) | (band ? 1 : 0) | (hor ? 2 : 0));
	
	diseqc_send_msg(fd, hor ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
			&cmd, band ? SEC_TONE_ON : SEC_TONE_OFF,
			(sat & 1) ? SEC_MINI_B : SEC_MINI_A, 0);
	dbgprintf(DEBUG_DVB, "MS %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3]);
	return 0;
}

static int set_vol_tone(int fd, int hor, int band)
{
	if (ioctl(fd, FE_SET_TONE, band ? SEC_TONE_ON : SEC_TONE_OFF))
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, hor ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13) == -1)
		perror("FE_SET_VOLTAGE failed");
	dbgprintf(DEBUG_DVB, "set_vol_tone hor=%u, band=%u\n", hor, band);
}

static int set_en50494(struct dddvb_fe *fe, uint32_t freq_khz, uint32_t sr, 
		       int sat, int hor, int band, 
		       uint32_t slot, uint32_t ubfreq,
		       fe_delivery_system_t ds)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0xe0, 0x11, 0x5a, 0x00, 0x00},
		.msg_len = 5
	};
	uint16_t t;
	uint32_t input = 3 & (sat >> 6);
	int fd = fe->fd;
	uint32_t freq = (freq_khz + 2000) / 4000;
	int32_t fdiff = freq_khz - freq * 1000;

	t = (freq_khz / 1000 + ubfreq + 2) / 4 - 350;
 	hor &= 1;

	cmd.msg[3] = ((t & 0x0300) >> 8) | 
		(slot << 5) | ((sat & 0x3f) ? 0x10 : 0) | (band ? 4 : 0) | (hor ? 8 : 0);
	cmd.msg[4] = t & 0xff;

	set_property(fd, DTV_INPUT, input);
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(15000);
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
		perror("FE_SET_VOLTAGE failed");

	set_fe_input(fe, ubfreq * 1000, sr, ds, input);
	dbgprintf(DEBUG_DVB, "EN50494 %02x %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3], cmd.msg[4]);
}

static int set_en50607(struct dddvb_fe *fe, uint32_t freq_khz, uint32_t sr,
		       int sat, int hor, int band, 
		       uint32_t slot, uint32_t ubfreq,
		       fe_delivery_system_t ds)
{
	struct dvb_diseqc_master_cmd cmd = {
		.msg = {0x70, 0x00, 0x00, 0x00, 0x00},
		.msg_len = 4
	};
	uint32_t freq = (freq_khz + 500) / 1000;
	int32_t fdiff = freq_khz - freq * 1000;
	uint32_t t = freq - 100;
	uint32_t input = 3 & (sat >> 6);
	int fd = fe->fd;
	
	dbgprintf(DEBUG_DVB, "input = %u, sat = %u\n", input, sat&0x3f);
	hor &= 1;
	cmd.msg[1] = slot << 3;
	cmd.msg[1] |= ((t >> 8) & 0x07);
	cmd.msg[2] = (t & 0xff);
	cmd.msg[3] = ((sat & 0x3f) << 2) | (hor ? 2 : 0) | (band ? 1 : 0);

	set_property(fd, DTV_INPUT, input);
	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
		perror("FE_SET_TONE failed");
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18) == -1)
		perror("FE_SET_VOLTAGE failed");
	usleep(15000);
	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
		perror("FE_DISEQC_SEND_MASTER_CMD failed");
	usleep(15000);
	if (ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
		perror("FE_SET_VOLTAGE failed");

	set_fe_input(fe, ubfreq * 1000 + fdiff, sr, ds, input);
	dbgprintf(DEBUG_DVB, "EN50607 %02x %02x %02x %02x\n", 
		  cmd.msg[0], cmd.msg[1], cmd.msg[2], cmd.msg[3]);
	dbgprintf(DEBUG_DVB, "EN50607 freq %u ubfreq %u fdiff %d sr %u hor %u\n", 
		  freq, ubfreq, fdiff, sr, hor);
}




static int tune_sat(struct dddvb_fe *fe)
{
	uint32_t freq, hi = 0, src, lnb = 0, lnbc = 0, lofs;
	fe_delivery_system_t ds = fe->param.param[PARAM_MSYS];
	
	freq = fe->param.param[PARAM_FREQ];
	dbgprintf(DEBUG_DVB, "tune_sat freq=%u\n", freq);

	if (fe->param.param[PARAM_SRC] != DDDVB_UNDEF)
		lnb = fe->param.param[PARAM_SRC];

	lnbc = lnb & (DDDVB_MAX_SOURCE - 1);
	lofs = fe->lofs[lnbc];
#if 0
	if (freq < 5000000) { //3400 - 4200 ->5150
		lofs = 5150000;
		if (freq > lofs)
			freq -= lofs;
		else
			freq = lofs - freq;
	} else 	if (freq > 19700000 && freq < 22000000) { //19700-22000 ->21200
		lofs = 21200000;
		if (freq > lofs)
			freq -= lofs;
		else
			freq = lofs - freq;
	} 
#endif
	if (freq > 3000000) {
		if (lofs)
			hi = (freq > lofs) ? 1 : 0;
		if (lofs > 10000000) {
			if (hi)
				freq -= fe->lof2[lnbc];
			else
				freq -= fe->lof1[lnbc];
		} else {
			if (hi)
				freq = fe->lof2[lnbc] - freq;
			else
				freq = fe->lof1[lnbc] - freq;
		}
	}
	dbgprintf(DEBUG_DVB, "tune_sat IF=%u\n", freq);
	if (fe->first) {
		fe->first = 0;
		dbgprintf(DEBUG_DVB, "pre voltage %d\n", fe->prev_delay[lnbc]);
		if (ioctl(fe->fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) == -1)
			perror("FE_SET_VOLTAGE failed");
		usleep(fe->prev_delay[lnbc]);
	}
	dbgprintf(DEBUG_DVB, "scif_type = %u\n", fe->scif_type);
	if (fe->scif_type == 1) { 
		pthread_mutex_lock(&fe->dd->uni_lock);
		set_en50494(fe, freq, fe->param.param[PARAM_SR],
			    lnb, fe->param.param[PARAM_POL], hi,
			    fe->scif_slot, fe->scif_freq, ds);
		pthread_mutex_unlock(&fe->dd->uni_lock);
	} else if (fe->scif_type == 2) {
		pthread_mutex_lock(&fe->dd->uni_lock);
		set_en50607(fe, freq, fe->param.param[PARAM_SR],
			    lnb, fe->param.param[PARAM_POL], hi,
			    fe->scif_slot, fe->scif_freq, ds);
		pthread_mutex_unlock(&fe->dd->uni_lock);
	} else {
		uint32_t input = fe->param.param[PARAM_SRC];

		if (input != DDDVB_UNDEF) {
			input = 3 & (input >> 6);
			dbgprintf(DEBUG_DVB, "input = %u\n", input);
			set_property(fe->fd, DTV_INPUT, input);
		}
		if (fe->scif_type == 3)
			set_vol_tone(fe->fd, fe->param.param[PARAM_POL], hi);
		else
			diseqc(fe->fd, lnb, fe->param.param[PARAM_POL], hi);
		set_fe_input(fe, freq, fe->param.param[PARAM_SR], ds, input);
	}
}

static int tune_c(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000},
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 8000000 },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = fe->param.param[PARAM_SR] },
		{ .cmd = DTV_INNER_FEC, .u.data = (fe->param.param[PARAM_FEC] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_FEC] : FEC_AUTO },
		{ .cmd = DTV_MODULATION,
		  .u.data = (fe->param.param[PARAM_MTYPE] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_MTYPE] : QAM_AUTO },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	dbgprintf(DEBUG_DVB, "tune_c()\n");
	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_A);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_j83b(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000},
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 6000000 },
		{ .cmd = DTV_SYMBOL_RATE, .u.data = (fe->param.param[PARAM_SR] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_SR] : 5056941},
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	dbgprintf(DEBUG_DVB, "tune_j83b()\n");
	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC_ANNEX_B);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_terr(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 8000000 },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}
#if 0
static int tune_terr(struct dddvb_fe *fe)
{
	uint32_t freq;
	enum fe_bandwidth bw;
	struct dvb_frontend_parameters p = {
		.frequency = fe->param.param[PARAM_FREQ] * 1000,
		.inversion = INVERSION_AUTO,
		.u.ofdm.code_rate_HP = FEC_AUTO,
		.u.ofdm.code_rate_LP = FEC_AUTO,
		.u.ofdm.constellation = fe->param.param[PARAM_MTYPE],
		.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO,
		.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO,
		.u.ofdm.hierarchy_information = HIERARCHY_AUTO,
		.u.ofdm.bandwidth = (fe->param.param[PARAM_BW] != DDDVB_UNDEF) ? 
		                   (fe->param.param[PARAM_BW]) : BANDWIDTH_AUTO,
	};
	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT);
	if (ioctl(fe->fd, FE_SET_FRONTEND, &p) == -1) {
		perror("FE_SET_FRONTEND error");
		return -1;
	}
	return 0;
}
#endif

static int tune_c2(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 8000000 },
		{ .cmd = DTV_STREAM_ID, .u.data = fe->param.param[PARAM_PLP] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBC2);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_terr2(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 8000000 },
		{ .cmd = DTV_STREAM_ID, .u.data = fe->param.param[PARAM_PLP] },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_DVBT2);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_isdbt(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ] * 1000 },
		{ .cmd = DTV_BANDWIDTH_HZ, .u.data = (fe->param.param[PARAM_BW_HZ] != DDDVB_UNDEF) ?
		  fe->param.param[PARAM_BW_HZ] : 6000000 },
		{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_ISDBT);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	return 0;
}

static int tune_isdbs(struct dddvb_fe *fe)
{
	struct dtv_property p[] = {
		{ .cmd = DTV_CLEAR },
		{ .cmd = DTV_FREQUENCY, .u.data = fe->param.param[PARAM_FREQ]},
		//{ .cmd = DTV_SYMBOL_RATE, .u.data = fe->param.param[PARAM_SR] },
		//{ .cmd = DTV_TUNE },
	};		
	struct dtv_properties c;
	int ret;

	set_property(fe->fd, DTV_DELIVERY_SYSTEM, SYS_ISDBS);

	c.num = ARRAY_SIZE(p);
	c.props = p;
	ret = ioctl(fe->fd, FE_SET_PROPERTY, &c);
	if (ret < 0) {
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", ret);
		return -1;
	}
	if (fe->param.param[PARAM_ISI] != DDDVB_UNDEF)
		set_property(fe->fd, DTV_STREAM_ID, fe->param.param[PARAM_ISI]);
	set_property(fe->fd, DTV_TUNE, 0);
	return 0;
}

static int tune(struct dddvb_fe *fe)
{
	int ret;

	dbgprintf(DEBUG_DVB, "tune()\n");
	switch (fe->param.param[PARAM_MSYS]) {
	case SYS_DVBS:
	case SYS_DVBS2:
		ret = tune_sat(fe);
		break;
	case SYS_DVBC_ANNEX_A:
		ret = tune_c(fe);
		break;
	case SYS_DVBC_ANNEX_B:
		ret = tune_j83b(fe);
		break;
	case SYS_DVBT:
		ret = tune_terr(fe);
		break;
	case SYS_DVBT2:
		ret = tune_terr2(fe);
		break;
	case SYS_DVBC2:
		ret = tune_c2(fe);
		break;
	case SYS_ISDBT:
		ret = tune_isdbt(fe);
		break;
	case SYS_ISDBS:
		ret = tune_isdbs(fe);
		break;
	default:
		break;
	}
	return ret;
}

int open_dmx(struct dddvb_fe *fe)
{
	char fname[80];
	struct dmx_pes_filter_params pesFilterParams; 
	
	sprintf(fname, "/dev/dvb/adapter%u/demux%u", fe->anum, fe->fnum); 

	fe->dmx = open(fname, O_RDWR);
	if (fe->dmx < 0) 
		return -1;

	pesFilterParams.input = DMX_IN_FRONTEND; 
	pesFilterParams.output = DMX_OUT_TS_TAP; 
	pesFilterParams.pes_type = DMX_PES_OTHER; 
	pesFilterParams.flags = DMX_IMMEDIATE_START;
  	pesFilterParams.pid = 0x2000;

	if (ioctl(fe->dmx, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
		return -1;
	return 0;
}

static int open_fe(struct dddvb_fe *fe)
{
	char fname[80];

	sprintf(fname, "/dev/dvb/adapter%d/frontend%d", fe->anum, fe->fnum); 
	fe->fd = open(fname, O_RDWR);
	if (fe->fd < 0) 
		return -1;
	return 0;
}


#include "dvb_quality.c"

static void get_stats(struct dddvb_fe *fe)
{
	uint16_t sig = 0, snr = 0;
	fe_status_t stat;
	int64_t str, cnr;
	int64_t val;
	uint64_t uval;
	struct dtv_fe_stats st;
	
	ioctl(fe->fd, FE_READ_STATUS, &stat);
	fe->stat = stat;
	fe->lock = (stat == 0x1f) ? 1 : 0;
	calc_lq(fe);
	if (!get_stat(fe->fd, DTV_STAT_SIGNAL_STRENGTH, &st)) {

		fe->strength = str = st.stat[0].svalue;
		dbgprintf(DEBUG_DVB, "fe%d: str=%lld.%03llddB\n",
			  fe->nr, str/1000, abs(str%1000));
	}
	if (!get_stat(fe->fd, DTV_STAT_CNR, &st)) {
		fe->cnr = cnr = st.stat[0].svalue;
		dbgprintf(DEBUG_DVB, "fe%d: cnr=%lld.%03llddB\n",
			  fe->nr, cnr/1000, abs(cnr%1000));
	}
	if (!get_stat(fe->fd, DTV_STAT_PRE_TOTAL_BIT_COUNT, &st) &&
	    (st.stat[0].scale == FE_SCALE_COUNTER)) {
		uval = st.stat[0].uvalue;
		dbgprintf(DEBUG_DVB, "fe%d: pre_total_bit_count = %08x\n",
			  fe->nr, (uint32_t)uval);
	}
	if (!get_stat(fe->fd, DTV_STAT_PRE_ERROR_BIT_COUNT, &st) &&
	    (st.stat[0].scale == FE_SCALE_COUNTER)) {
		uval = st.stat[0].uvalue;
		dbgprintf(DEBUG_DVB, "fe%d: pre_error_bit_count = %llu\n",
			  fe->nr, uval);
	}
	if (!get_stat(fe->fd, DTV_STAT_ERROR_BLOCK_COUNT, &st) &&
	    (st.stat[0].scale == FE_SCALE_COUNTER)) {
		uval = st.stat[0].uvalue;
		dbgprintf(DEBUG_DVB, "fe%d: error_block_count = %llu\n",
			  fe->nr, uval);
	}
	if (!get_stat(fe->fd, DTV_STAT_TOTAL_BLOCK_COUNT, &st) &&
	    (st.stat[0].scale == FE_SCALE_COUNTER)) {
		uval = st.stat[0].uvalue;
		dbgprintf(DEBUG_DVB, "fe%d: total_block_count = %llu\n",
			  fe->nr, uval);
	}
}

void dddvb_fe_handle(struct dddvb_fe *fe)
{
	uint32_t newtune, count = 0, max, nolock = 0;
	int ret;

	
	if (fe->dd->get_ts)
		open_dmx(fe);
	while (fe->state == 1) {
		pthread_mutex_lock(&fe->mutex);
		newtune = fe->n_tune;
		if (newtune == 1) {
			fe->n_tune = 0;
			if (!memcmp(fe->param.param, fe->n_param.param, sizeof(fe->param.param))) {
				dbgprintf(DEBUG_DVB, "same params\n");
				fe->tune = 2;
				count = 0;
				nolock = 10;
				max = 2;
			} else { 
				memcpy(fe->param.param, fe->n_param.param, sizeof(fe->param.param));
				fe->tune = 1;
			}
		}
		pthread_mutex_unlock(&fe->mutex);
		
		switch (fe->tune) {
		case 1:
			dbgprintf(DEBUG_DVB, "fe %d tune\n", fe->nr);
			tune(fe);
			nolock = 0;
			count = 0;
			max = 2;
			dbgprintf(DEBUG_DVB, "fe %d tune done\n", fe->nr);
			fe->tune = 2;
			break;
		case 2: 
			count++;
			if (count < max) 
				break;
			count = 0;
			get_stats(fe);
			if (fe->lock) {
				max = 20;
				nolock = 0;
			} else {
				max = 1;
				nolock++;
				if (nolock > 10)
					fe->tune = 1;
			}
			break;

		default:
			break;
		}
		if (fe->state != 1)
			break;
		usleep(50000);
	}
	close(fe->fd);
	if (fe->dmx > 0)
		close(fe->dmx);
	fe->fd = -1;
	fe->dmx = -1;
	fe->stat = fe->lock = fe->level = fe->quality = 0;
	fe->state = 0;
	dbgprintf(DEBUG_DVB, "fe %d done\n", fe->nr);
}

int dddvb_fe_start(struct dddvb_fe *fe)
{
	fe->dmx = -1;
	fe->tune = 0;
	dddvb_param_init(&fe->param);
	fe->first = 1;
	if (open_fe(fe))
		return -1;
	return pthread_create(&fe->pt, NULL, (void *) dddvb_fe_handle, fe); 
}

int dddvb_fe_tune(struct dddvb_fe *fe, struct dddvb_params *p)
{
	int ret = 0;

	dbgprintf(DEBUG_DVB, "dvb_tune\n");
	pthread_mutex_lock(&fe->mutex);
	memcpy(fe->n_param.param, p->param, sizeof(fe->n_param.param));
	fe->n_tune = 1;
	pthread_mutex_unlock(&fe->mutex);
	while(fe->n_tune) usleep(10000);
	while(fe->tune != 2) usleep(10000);
	return ret;
}

int dddvb_fe_get(struct dddvb_fe *fe, struct dddvb_params *p)
{
	int ret = 0;

	dbgprintf(DEBUG_DVB, "fe_get\n");
	pthread_mutex_lock(&fe->mutex);
	memcpy(p->param, fe->n_param.param, sizeof(fe->n_param.param));
	pthread_mutex_unlock(&fe->mutex);
	return ret;
}

static int dddvb_fe_init(struct dddvb *dd, int a, int f, int fd)
{
	struct dtv_properties dps;
	struct dtv_property dp[10];
	struct dddvb_fe *fe;
	int r;
	uint32_t i, ds;

	fe = &dd->dvbfe[dd->dvbfe_num];

	r = snprintf(fe->name, sizeof(fe->name), "/dev/dvb/adapter%d/frontend%d", a, f);
	if (r >= sizeof(fe->name))
	    return -1;
	dbgprintf(DEBUG_DVB, "fe_init a=%d f=%d  name=%s\n", a, f, fe->name);
	
	dps.num = 1;
	dps.props = dp;
	dp[0].cmd = DTV_ENUM_DELSYS;
	r = ioctl(fd, FE_GET_PROPERTY, &dps);
	if (r < 0)
		return -1;
	for (i = 0; i < dp[0].u.buffer.len; i++) {
		ds = dp[0].u.buffer.data[i];
		dbgprintf(DEBUG_DVB, "delivery system %d\n", ds);
		fe->type |= (1UL << ds);
	}
	dbgprintf(DEBUG_DVB, "fe %d type = %08x\n", dd->dvbfe_num, fe->type);
	if (!fe->type)
		return -1;

	fe->dd = dd;
	fe->anum = a;
	fe->fnum = f;
	fe->nr = dd->dvbfe_num;
	
	dps.num = 1;
	dps.props = dp;
	dp[0].cmd = DTV_INPUT;
	r = ioctl(fd, FE_GET_PROPERTY, &dps);
	if (r < 0)
		return -1;
#if 0
	for (i = 0; i < dp[0].u.buffer.len; i++) {
		fe->input[i] = dp[0].u.buffer.data[i];
		//dbgprintf(DEBUG_DVB, "input prop %u = %u\n", i, fe->input[i]);
	}
	if (fe->input[3]) {
		dd->has_feswitch = 1;
		if (!dd->scif_type && !msmode) {
			if (fe->input[2] >= fe->input[1]) {
				fe->type = 0;
				return -1;
			}
		}
	}
#endif
	if (fe->type & (1UL << SYS_DVBS2))
		dd->dvbs2num++;
	if (fe->type & (1UL << SYS_DVBT2))
		dd->dvbt2num++;
	else if (fe->type & (1UL << SYS_DVBT))
		dd->dvbtnum++;
	if (fe->type & (1UL << SYS_DVBC2))
		dd->dvbc2num++;
	else if (fe->type & (1UL << SYS_DVBC_ANNEX_A))
		dd->dvbcnum++;

	dd->dvbfe_num++;
	pthread_mutex_init(&fe->mutex, 0);
	return 0;
}

static int scan_dvbfe(struct dddvb *dd)
{
	int a, f, fd;
	char fname[80];

	for (a = 0; a < 16; a++) {
		for (f = 0; f < 24; f++) {
			sprintf(fname, "/dev/dvb/adapter%d/frontend%d", a, f); 
			fd = open(fname, O_RDONLY);
			if (fd >= 0) {
				dddvb_fe_init(dd, a, f, fd);
				close(fd);
			}
		}
	}
	dbgprintf(DEBUG_DVB, "Found %d frontends\n", dd->dvbfe_num);
}

void scif_config(struct dddvb *dd, char *name, char *val)
{
	if (!name || !val)
		return;
	
	if (!strncasecmp(name, "type", 4) &&
	    val[0] >= 0x30 && val[0] <= 0x32) {
		dd->scif_type = val[0] - 0x30;
		dbgprintf(DEBUG_DVB, "setting type = %d\n", dd->scif_type);
	}
	if (!strncasecmp(name, "tuner", 5) &&
	    name[5] >= 0x31 && name[5] <= 0x39) {
		int fe = strtol(name + 5, NULL, 10 );
		if (fe >= 0 && fe < DDDVB_MAX_DVB_FE) {
			char *end;
			unsigned long int nr = strtoul(val, &end, 10), freq = 0;

			if (*end == ',') {
				val = end + 1;
				freq = strtoul(val, &end, 10);
				if (val == end)
					return;
			}
			fe--;
			if (nr == 0)
				dd->dvbfe[fe].scif_type = 0;
			else {
				dd->dvbfe[fe].scif_slot = nr - 1;
				dd->dvbfe[fe].scif_freq = freq;
				dd->dvbfe[fe].scif_type = dd->scif_type;
			}
			dbgprintf(DEBUG_DVB, "fe%d: type=%d, slot=%d, freq=%d\n", fe,
				  dd->dvbfe[fe].scif_type,
				  dd->dvbfe[fe].scif_slot,
				  dd->dvbfe[fe].scif_freq);
		}
	}
}


void set_lnb(struct dddvb *dd, int tuner, 
	     uint32_t source, uint32_t lof1, uint32_t lof2, uint32_t lofs) 
{
	int i, j;
	int i1 = 0, i2 = DDDVB_MAX_DVB_FE;
	int j1 = 0, j2 = DDDVB_MAX_SOURCE;
	
	if (tuner > DDDVB_MAX_DVB_FE) 
		return;
	if (source > DDDVB_MAX_SOURCE) 
		return;
	
	if (tuner) {
		i1 = tuner - 1; 
		i2 = i1 + 1;
	}
	if (source) {
		j1 = source - 1;
		j2 = j1 + 1;
	}
	for (i = i1; i < i2; i++) {
		struct dddvb_fe *fe = &dd->dvbfe[i];
		for (j = j1; j < j2; j++) {
			dbgprintf(DEBUG_DVB, "setting %d %d %u %u %u\n", 
				  i, j, lof1, lof2, lofs);
			fe->lof1[j] = lof1; 
			fe->lof2[j] = lof2; 
			fe->lofs[j] = lofs; 
			fe->prev_delay[j] = 250000;
		}
	}
}

void lnb_config(struct dddvb *dd, char *name, char *val)
{
	static int lnb = -1;
	static uint32_t lof1, lof2, lofs, tuner, source;
	char *end;

	if (!name || !val) {
		if (lnb >= 0)
			set_lnb(dd, tuner, source, 
				lof1 * 1000, lof2 * 1000, lofs * 1000);
		lnb++;
		tuner = source = lof1 = lof2 = lofs = 0;
		return;
	}
	if (!strcasecmp(name, "tuner")) {
		tuner = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "source")) {
		source = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lof1")) {
		lof1 = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lof2")) {
		lof2 = strtoul(val, &end, 10);
	} else if (!strcasecmp(name, "lofs")) {
		lofs = strtoul(val, &end, 10);
	}
}

void ca_config(struct dddvb *dd, char *name, char *val)
{
	if (!name || !val)
		return;
	char *p = strpbrk(val, "\r\n");
	if (p)
		*p = 0;
	if (!strcasecmp(name, "family")) {
		if (!strcasecmp(val, "tcp")) {
			dd->cam_family = 1;
		} else if (!strcasecmp(val, "unix")) {
			dd->cam_family = 2;
		}
		return;
	}
	if (!strcasecmp(name, "proto")) {
		dd->cam_proto = strtoul(val, NULL, 10);
		return;
	}
	if (!strcasecmp(name, "port")) {
		dd->cam_port = strtoul(val, NULL, 10);
		return;
	}
}

int dddvb_dvb_init(struct dddvb *dd)
{
	pthread_mutex_init(&dd->uni_lock, 0);
	scan_dvbfe(dd);
	parse_config(dd, "", "scif", &scif_config);
	set_lnb(dd, 0, 0, 9750000, 10600000, 11700000);
	parse_config(dd, "", "LNB", &lnb_config);
	parse_config(dd, "", "CA", &ca_config);
	{
		if (dd->cam_family == 0)
			dd->cam_family = 1;
		if (dd->cam_proto == 0) {
			switch (dd->cam_family) {
				case 1:
					dd->cam_proto = 1;
					break;
				case 2:
					dd->cam_proto = 2;
					break;
			}
		}
		if (dd->cam_port == 0)
			dd->cam_port = 8888;
	}
	scan_dvbca(dd);
}


int dddvb_dvb_exit(struct dddvb *dd)
{


}

