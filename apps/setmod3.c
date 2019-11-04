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
#include <getopt.h>

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



int main(int argc, char*argv[])
{
	int fd;
	struct dvb_mod_params mp;
	struct dvb_mod_channel_params mc;
	uint32_t data;
	int adapter = 0, channel = 0, gain = -1;
	int32_t base = -1, freq = -1, rate = -1;
	char mod_name[128];
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"adapter", required_argument, 0, 'a'},
			{"channel", required_argument, 0, 'c'},
			{"gain", required_argument, 0, 'g'},
			{"base", required_argument, 0, 'b'},
			{"frequency", required_argument, 0, 'f'},
			{"rate", required_argument, 0, 'r'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"a:c:g:b:f:r:",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
		case 'a':
			adapter = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			channel = strtoul(optarg, NULL, 0);
			break;
		case 'g':
			gain = strtoul(optarg, NULL, 0);
			break;
		case 'b':
			base = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			freq = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			if (!strcmp(optarg, "DVBT_8"))
				rate = SYS_DVBT_8;
			else if (!strcmp(optarg, "DVBT_7"))
				rate = SYS_DVBT_7;
			else if (!strcmp(optarg, "DVBT_6"))
				rate = SYS_DVBT_6;
			else if (!strcmp(optarg, "ISDBT_6"))
				rate = SYS_ISDBT_6;
			else rate = strtoul(optarg, NULL, 0);
			break;
		default:
			break;
		}
	}
	if (optind < argc) {
		printf("too man arguments\n");
		exit(1);
	}

	snprintf(mod_name, 127, "/dev/dvb/adapter%d/mod%d", adapter, channel);
	fd = open(mod_name, O_RDONLY);
	
	if (fd < 0)  {
		printf("Could not open modulator device.\n");
		exit(1);
	}
		
	
	/* gain 0-255 */
	//get_property(fd, MODULATOR_GAIN, &data);
	//printf("Modulator gain = %u\n", data);
	//set_property(fd, MODULATOR_GAIN, 100);

	//get_property(fd, MODULATOR_ATTENUATOR, &data);
	//printf("Modulator attenuator = %u\n", data);

	if (base > 0)
		set_property(fd, MODULATOR_BASE_FREQUENCY, base);
	if (freq > 0)
		set_property(fd, MODULATOR_FREQUENCY, freq);
	if (rate > 0)
		set_property(fd, MODULATOR_OUTPUT_RATE, rate);
	
	
	close(fd);
}

