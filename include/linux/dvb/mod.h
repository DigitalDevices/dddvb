#ifndef _UAPI_DVBMOD_H_
#define _UAPI_DVBMOD_H_

#include <linux/types.h>
#include "frontend.h"

struct dvb_mod_params {
	__u32 base_frequency;
	__u32 attenuator; 
};

struct dvb_mod_channel_params {
	enum fe_modulation modulation;
	__u64 input_bitrate;         /* 2^-32 Hz */
	int   pcr_correction; 
};


#define DVB_MOD_SET              _IOW('o', 208, struct dvb_mod_params)
#define DVB_MOD_CHANNEL_SET      _IOW('o', 209, struct dvb_mod_channel_params)

#define MOD_UNDEFINED		0
#define MOD_START		1
#define MOD_STOP		2
#define MOD_FREQUENCY		3
#define MOD_MODULATION		4   
#define MOD_SYMBOL_RATE		5   /* Hz */
#define MOD_ATTENUATOR          32  
#define MOD_INPUT_BITRATE       33  /* Hz */
#define MOD_PCR_MODE            34  /* 1=pcr correction enabled */


#endif /*_UAPI_DVBMOD_H_*/
