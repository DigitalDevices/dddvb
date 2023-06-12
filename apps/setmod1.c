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
#include <pthread.h>

#include <linux/dvb/mod.h>

int main()
{
	int fd;
	struct dvb_mod_params mp;
	struct dvb_mod_channel_params mc;

	fd = open("/dev/dvb/adapter1/mod0", O_RDONLY);

	mp.base_frequency = 722000000;
	mp.attenuator = 0;
	ioctl(fd, DVB_MOD_SET, &mp);

	mc.modulation = QAM_256;
	mc.input_bitrate = 40000000ULL << 32;
	mc.pcr_correction = 0;
	ioctl(fd, DVB_MOD_CHANNEL_SET, &mc);
	close(fd);
}

