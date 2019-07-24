#ifndef _DDDVB_H_
#define _DDDVB_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/net.h>


#include <libdvben50221/en50221_stdcam.h>

#define DDDVB_MAX_DVB_FE 256
#define DDDVB_MAX_DVB_CA 256

#define DDDVB_MAX_SOURCE 4

#define MAX_PMT         16


struct dddvb_params {
	uint32_t param[32];
	uint8_t  pid[1024];
};

struct dddvb_status {
	struct dddvb_params params;
	fe_status_t stat;
	int64_t strength;
	int64_t snr;
};

struct dddvb_fe {
	struct dddvb *dd;
	uint32_t state;
	pthread_t pt;
	pthread_mutex_t mutex;
	char name[120];
	
	uint32_t source;
#define DDDVB_FE_SOURCE_DEMOD   0
#define DDDVB_FE_SOURCE_SATIP   1
#define DDDVB_FE_SOURCE_RTP     2
#define DDDVB_FE_SOURCE_MCAST   3

	uint32_t modulation;
#define DDDVB_MODULATION_DVB_C  1
#define DDDVB_MODULATION_DVB_S  2
#define DDDVB_MODULATION_DVB_T  3

	uint32_t nr;
	uint32_t type;
	uint32_t anum;
	uint32_t fnum;
	
	uint32_t scif_type;
	uint32_t scif_slot;
	uint32_t scif_freq;
	uint32_t input;

	uint32_t lof1[DDDVB_MAX_SOURCE];
	uint32_t lof2[DDDVB_MAX_SOURCE];
	uint32_t lofs[DDDVB_MAX_SOURCE];
	uint32_t prev_delay[DDDVB_MAX_SOURCE];

	int fd;
	int dmx;

	fe_status_t stat;
	uint32_t level;
	uint32_t lock;
	uint32_t quality;
	int64_t strength;
	int64_t cnr;
	int64_t ber;
	int first;

	uint32_t tune;
	struct dddvb_params param;

	uint32_t n_tune;
	struct dddvb_params n_param;

	struct dddvb_status status;
};

struct dddvb_ca {
	struct dddvb *dd;
	struct osstrm *stream;
	int fd;
	int ci_rfd;
	int ci_wfd;
	uint32_t type;
	int anum;
	int fnum;
	int state;
	int nr;
	int input;

	pthread_t pt;
	pthread_t poll_pt;

	pthread_mutex_t mutex;

	struct en50221_transport_layer *tl;
	struct en50221_session_layer *sl;
	struct en50221_stdcam *stdcam;
	int resource_ready;
	int sentpmt;
	int moveca;
	int ca_pmt_version[MAX_PMT];
	int data_pmt_version;

	int setpmt;
	uint32_t pmt[MAX_PMT];
	uint32_t pmt_new[MAX_PMT];
	uint32_t pmt_old[MAX_PMT];

	int mmi_state;
	uint8_t mmi_buf[16];
	int mmi_bufp;
	int sock;
};
	
struct dddvb {
	pthread_mutex_t lock;
	pthread_mutex_t uni_lock;

	char config[80];
	
	uint32_t state;

	uint32_t dvbnum;
	uint32_t dvbtnum;
	uint32_t dvbs2num;
	uint32_t dvbt2num;
	uint32_t dvbcnum;
	uint32_t dvbc2num;

	uint32_t dvbfe_num;
	uint32_t scif_type;

	uint32_t dvbca_num;
	int exit;
	
	struct dddvb_fe dvbfe[DDDVB_MAX_DVB_FE];
	struct dddvb_ca dvbca[DDDVB_MAX_DVB_CA];
};

int dddvb_dvb_init(struct dddvb *dd);
int parse_config(struct dddvb *dd, char *name, char *sec,
		 void (*cb)(struct dddvb *, char *, char *) );
void dddvb_fe_handle(struct dddvb_fe *fe);
int dddvb_fe_tune(struct dddvb_fe *fe, struct dddvb_params *p);
int dddvb_fe_start(struct dddvb_fe *fe);
int scan_dvbca(struct dddvb *dd);


#endif /* _DDDVB_H_ */
