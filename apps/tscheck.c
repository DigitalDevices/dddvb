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

char line_start[16] = "";
char line_end[16]   = "\r";


uint32_t cc_errors = 0;
uint32_t packets = 0;
uint32_t payload_packets = 0;
uint32_t packet_errors = 0;

uint8_t cc[8192] = { 0 };

void proc_ts(int i, uint8_t *buf)
{
  uint16_t pid=0x1fff&((buf[1]<<8)|buf[2]);
  uint8_t ccin = buf[3] & 0x1F;
  
  if( buf[0] == 0x47 && (buf[1] & 0x80) == 0)
  {
    if( pid != 8191 )
    {
      if( ccin != 0 )
      {
        if( cc[pid] != 0 )
        {
          // TODO: 1 repetition allowed
          if( ( ccin & 0x10 ) != 0 && (((cc[pid] + 1) & 0x0F) != (ccin & 0x0F)) )  
            cc_errors += 1;
        }
        cc[pid] = ccin;
      }
      payload_packets += 1;
    }
  }
  else
    packet_errors += 1;
  
  if( (packets & 0x3FFF ) == 0)
  {
    printf("%s  Packets: %12u non null %12u, errors: %12u, CC errors: %12u%s", line_start, packets, payload_packets, packet_errors, cc_errors, line_end);
    fflush(stdout);
  }
  
  packets += 1;  
}

#define TSBUFSIZE (100*188)

void citest(char* n)
{
  uint8_t *buf;
	uint8_t id;
	int i, nts;
	int len;
	int ts=open(n, O_RDONLY);
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

int main(int argc, char* argv[])
{
  if( argc < 2 )
  {
    printf("tscheck <file>|<device> [<display line>]\n");
    exit(0);
  }
  if( argc > 2 )
  {
    int line = atoi(argv[2]);
    if( line >= 0 && line < 64 )
    {
      snprintf(line_start,sizeof(line_start)-1,"\0337\033[%d;0H",line);
      strncpy(line_end,"\0338",sizeof(line_end)-1);
    }
  }
	citest(argv[1]);
}

