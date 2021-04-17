#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/types.h>
#include <getopt.h>
#include <ctype.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "../ddbridge/ddbridge-mci.h"
#include "../ddbridge/ddbridge-ioctl.h"

struct mconf {
	int set_output;
	int set_channels;
	int fd;
	
	struct mci_command channels;
	struct mci_command stream;
	struct mci_command output;
};

void strim(char *s)
{
	int l = strlen(s);

	while (l && isspace(s[l-1]))
		l--;
	s[l] = 0;
}

void parse(char *fname, char *sec, void *priv, void (*cb)(void *, char *, char *))
{
	char line[256], csec[80], par[80], val[80], *p;
	FILE *f;

	if ((f = fopen(fname, "r")) == NULL)
		return;
	while ((p = fgets(line, sizeof(line), f))) {
		if (*p == '\r' || *p == '\n' || *p == '#')
			continue;
		if (*p == '[') {
			if ((p = strtok(line + 1, "]")) == NULL)
				continue;
			strncpy(csec, p, sizeof(csec));
			if (!strcmp(sec, csec) && cb)
				cb(priv, NULL, NULL);
			continue;
		}
		if (!(p = strtok(line, "=")))
			continue;
		while (isspace(*p))
			p++;
		strncpy(par, p, sizeof(par));
		strim(par);
		if (!(p = strtok(NULL, "=")))
			continue;
		while (isspace(*p))
			p++;
		strncpy (val, p, sizeof(val));
		strim(val);
		if (!strcmp(sec, csec) && cb)
			cb(priv, par, val);
	}
	if (!strcmp(sec, csec) && cb)
		cb(priv, NULL, NULL);
	fclose(f);
}

int mci_cmd(int dev, struct mci_command *cmd)
{
	int ret;
	struct ddb_mci_msg msg;
	uint8_t status;

	msg.link = 0;
	memcpy(&msg.cmd, cmd, sizeof(msg.cmd));
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		dprintf(2, "mci_cmd error %d\n", errno);
		return ret;
	}
	status = msg.res.status;
	if (status == MCI_STATUS_OK)
		return ret;
	if (status == MCI_STATUS_UNSUPPORTED) {
		dprintf(2, "Unsupported MCI command\n");
		return ret;
	}
	if (status == MCI_STATUS_INVALID_PARAMETER) {
		dprintf(2, "Invalid MCI parameters\n");
		return ret;
	}
	return ret;
}

struct mci_command msg_channels = {
	.mod_command = MOD_SETUP_CHANNELS,	
	.mod_channel = 0,
	.mod_stream = 0,
	.mod_setup_channels[0] = {
		.flags = MOD_SETUP_FLAG_FIRST|MOD_SETUP_FLAG_LAST|MOD_SETUP_FLAG_VALID,
		.standard = MOD_STANDARD_DVBT_8,
		.num_channels = 25,
		.frequency = 474000000,
	},
};

struct mci_command msg_stream = {
	.mod_command = MOD_SETUP_STREAM,	
	.mod_channel = 1,
	.mod_stream = 0,
	.mod_setup_stream = {
		.standard = MOD_STANDARD_DVBT_8,
		.fft_size = 1,
		.guard_interval = 0,
	},
};

struct mci_command msg_output = {
	.mod_command = MOD_SETUP_OUTPUT,
	.mod_channel = 0,
	.mod_stream = 0,
	.mod_setup_output = {
		.connector = MOD_CONNECTOR_F,
		.num_channels = 16,
		.unit = MOD_UNIT_DBUV,
		.channel_power = 5000,
	},
};

void output_cb(void *priv, char *par, char *val)
{
	struct mconf *mc = (struct mconf *) priv;

	if (!par && !val) {
		mc->set_output = 1;
		return;
	}		
	if (!strcasecmp(par, "connector")) {
		if (!strcasecmp(val, "F")) {
			mc->output.mod_setup_output.connector = MOD_CONNECTOR_F;
		} else if (!strcasecmp(val, "SMA")) {
			mc->output.mod_setup_output.connector = MOD_CONNECTOR_SMA;
		} else if (!strcasecmp(val, "OFF")) {
			mc->output.mod_setup_output.connector = MOD_CONNECTOR_OFF;
		} else
			printf("invalid connector\n");
	} else if (!strcasecmp(par, "power")) {
		mc->output.mod_setup_output.channel_power = (int16_t) (strtod(val, NULL) * 100.0);
	} else if (!strcasecmp(par, "channels")) {
		mc->output.mod_setup_output.num_channels = strtol(val, NULL, 10);
	}else if (!strcasecmp(par, "unit")) {
		if (!strcasecmp(val, "DBUV")) {
			mc->output.mod_setup_output.unit = MOD_UNIT_DBUV;
		} else if (!strcasecmp(val, "DBM")) {
			mc->output.mod_setup_output.unit = MOD_UNIT_DBM;
		} else
			printf("invalid unit\n");
	} else
		printf("invalid output parameter: %s\n", par);
}

void channels_cb(void *priv, char *par, char *val)
{
	struct mconf *mc = (struct mconf *) priv;

	if (!par && !val) {
		mc->set_channels = 1;
		return;
	}		
	if (!strcasecmp(par, "frequency")) {
		mc->channels.mod_setup_channels[0].frequency =  (uint32_t) (strtod(val, NULL) * 1000000.0);
		printf("frequency = %u\n", mc->channels.mod_setup_channels[0].frequency);
	} else if (!strcasecmp(par, "channels")) {
		mc->channels.mod_setup_channels[0].num_channels = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "standard")) {
		mc->channels.mod_setup_channels[0].standard = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "offset")) {
		mc->channels.mod_setup_channels[0].offset =  (uint32_t) (strtod(val, NULL) * 1000000.0);
	} else if (!strcasecmp(par, "bandwidth")) {
		mc->channels.mod_setup_channels[0].bandwidth = (uint32_t) (strtod(val, NULL) * 1000000.0);
		mc->channels.mod_setup_channels[0].offset =
			mc->channels.mod_setup_channels[0].bandwidth / 2;
	} else
		printf("invalid channels parameter: %s\n", par);
}

void streams_cb(void *priv, char *par, char *val)
{
	struct mconf *mc = (struct mconf *) priv;

	if (!par && !val) {
		return;
	}		
	if (!strcasecmp(par, "fft_size")) {
		mc->stream.mod_setup_stream.fft_size = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "guard_interval")) {
		mc->stream.mod_setup_stream.guard_interval = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "puncture_rate")) {
		mc->stream.mod_setup_stream.puncture_rate = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "standard")) {
		mc->stream.mod_setup_stream.standard = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "stream_format")) {
		mc->stream.mod_setup_stream.stream_format = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "symbol_rate")) {
		mc->stream.mod_setup_stream.symbol_rate = (uint32_t) (strtod(val, NULL) * 1000000.0);
	} else if (!strcasecmp(par, "stream")) {
		mc->stream.mod_stream = strtol(val, NULL, 10);
		printf("set stream %u to channel %u\n", mc->stream.mod_stream, mc->stream.mod_channel);
		mci_cmd(mc->fd, &mc->stream);
	} else if (!strcasecmp(par, "channel")) {
		mc->stream.mod_channel = strtol(val, NULL, 10);
	} else
		printf("invalid streams parameter: %s\n", par);
}

int main(int argc, char*argv[])
{
	int fd = -1;
	char fn[128];
	uint32_t device = 0;
	uint32_t frequency = 0;
	char *configname = "modulator.conf";
	struct mconf mc;

	memset(&mc, 0, sizeof(mc));
	mc.channels = msg_channels;
	mc.stream = msg_stream;
	mc.output = msg_output;
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"device", required_argument, 0, 'd'},
			{"config", required_argument, 0, 'c'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, "d:c:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'd':
			device = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			configname = optarg;
			break;
		case 'h':
			dprintf(2, "modconfig [-d device_number] [-c config_file]\n");
			break;
		default:
			break;
		}
	}
	if (optind < argc) {
		printf("too many arguments\n");
		exit(1);
	}
	snprintf(fn, 127, "/dev/ddbridge/card%u", device);
	fd = open(fn, O_RDWR);
	if (fd < 0) {
		dprintf(2, "Could not open %s\n", fn);
		return -1;
	}
	mc.fd = fd;
	parse(configname, "channels", (void *) &mc, channels_cb);
	if (mc.set_channels)
		mci_cmd(fd, &mc.channels);
	parse(configname, "streams", (void *) &mc, streams_cb);
	parse(configname, "output", (void *) &mc, output_cb);
	if (mc.set_output)
		mci_cmd(fd, &mc.output);
}
