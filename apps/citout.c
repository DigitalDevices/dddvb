#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>

#define TSBUFSIZE (100*188)

void citest()
{
        uint8_t *buf;
	uint8_t id;
	int i, nts;
	int len;
	int ts0=open("/dev/dvb/adapter0/dvr0", O_RDONLY);
	int ts1=open("/dev/dvb/adapter4/sec0", O_WRONLY);
	int demux0=open("/dev/dvb/adapter0/demux0", O_RDWR);

	struct dmx_pes_filter_params pesFilterParams; 
	
	pesFilterParams.input = DMX_IN_FRONTEND; 
	pesFilterParams.output = DMX_OUT_TS_TAP; 
	pesFilterParams.pes_type = DMX_PES_OTHER; 
	pesFilterParams.flags = DMX_IMMEDIATE_START;
  
	pesFilterParams.pid = 8192;
	if (ioctl(demux0, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
	        printf("Could not set PES filter\n"); 
		return;
	}
	buf=(uint8_t *)malloc(TSBUFSIZE);

	while(1) {
		len=read(ts0, buf, TSBUFSIZE);
		if (len<=0)
			break;
		if (buf[0]!=0x47)
			printf("oops\n");
		write(ts1, buf, len);
	}
}

int main()
{
	citest();
}

