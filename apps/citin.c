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

void proc_ts(int i, uint8_t *buf)
{
        uint16_t pid=0x1fff&((buf[1]<<8)|buf[2]);

	if (buf[3]&0xc0) /* only descrambled packets */
		return;
	/* only ORF */
	if (pid==160 || pid==161 || pid==1001||pid==13001 || pid==0)
		write(1, buf, 188);
}

#define TSBUFSIZE (100*188)

void citest()
{
        uint8_t *buf;
	uint8_t id;
	int i, nts;
	int len;
	int ts=open("/dev/dvb/adapter4/ci0", O_RDONLY);
	buf=(uint8_t *)malloc(TSBUFSIZE);

	
	while(1) {
		len=read(ts, buf, TSBUFSIZE);
		if (len<0) {
			continue;
		}
		if (buf[0]!=0x47) {
			read(ts, buf, 1);
			continue;
		}
		if (len%188) { /* should not happen */
			printf("blah\n");
			continue;
		}
		nts=len/188;
		for (i=0; i<nts; i++)
			proc_ts(i, buf+i*188);
	}
}

int main()
{
	citest();
}

