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
		fprintf(stderr, "Usage: %s /dev/dvb/adapterX/modY freq[MHz] bitrate[kbit/s] [symbolrate[ksyms/s]]\n", argv[0]);
		exit(1);
	}
	int fd;
	unsigned int freq = atoi(argv[2])*1000000;
	unsigned int bitrate = atoi(argv[3])*1000;
	unsigned int symbol_rate = 6900000;
	struct dvb_mod_params mp;
	struct dvb_mod_channel_params mc;

	if (argc == 4) symbol_rate = atoi(argv[3]);
	if (symbol_rate <= 7100) symbol_rate *= 1000;

	fd = open(argv[1], O_RDONLY);

	set_property(fd, MODULATOR_MODULATION, QAM_256);

	fprintf(stderr, "Symbol rate: %u\n", symbol_rate);
	set_property(fd, MODULATOR_SYMBOL_RATE, symbol_rate);

	fprintf(stderr, "Frequency: %u\n", freq);
	set_property(fd, MODULATOR_FREQUENCY, freq);

	fprintf(stderr, "Bitrate: %u\n", bitrate);
	set_property(fd, MODULATOR_INPUT_BITRATE, bitrate);

	close(fd);
}

