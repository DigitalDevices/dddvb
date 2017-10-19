#include <errno.h>
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
		fprintf(stderr, "FE_SET_PROPERTY returned %d\n", errno);
		perror("FE_SET_PROPERTY");
		return -1;
	}
	return 0;
}

int main(int argc, char ** argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s /dev/dvb/adapterX/modY freq[MHz] bitrate[kbit/s]\n", argv[0]);
		exit(1);
	}
	int fd;
	struct dvb_mod_params mp;
	struct dvb_mod_channel_params mc;

	fd = open(argv[1], O_RDONLY);

	set_property(fd, MODULATOR_MODULATION, QAM_256);
	set_property(fd, MODULATOR_SYMBOL_RATE, 6900000);
	set_property(fd, MODULATOR_FREQUENCY, atoi(argv[2])*1000000);
	set_property(fd, MODULATOR_INPUT_BITRATE, atoi(argv[3])*1000);
	close(fd);
}

