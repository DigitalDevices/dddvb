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
		return -1;
	}
	return 0;
}

int main()
{
	int fd;
	struct dvb_mod_params mp;
	struct dvb_mod_channel_params mc;

	fd = open("/dev/dvb/adapter0/mod0", O_RDONLY);

	set_property(fd, MODULATOR_MODULATION, QAM_256);
	set_property(fd, MODULATOR_SYMBOL_RATE, 6900000);
	set_property(fd, MODULATOR_FREQUENCY, 114000000);
	close(fd);
}

