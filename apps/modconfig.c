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

struct param_table_entry {
	int value;
	char* name;
};

struct param_table_entry mod_standard_table[] = {
	{ .name = "0", .value = MOD_STANDARD_GENERIC },
	{ .name = "GENERIC", .value = MOD_STANDARD_GENERIC },

	{ .name = "1", .value = MOD_STANDARD_DVBT_8 },
	{ .name = "DVBT_8", .value = MOD_STANDARD_DVBT_8 },
	{ .name = "DVBT2_8", .value = MOD_STANDARD_DVBT_8 },

	{ .name = "2", .value = MOD_STANDARD_DVBT_7 },
	{ .name = "DVBT_7", .value = MOD_STANDARD_DVBT_7 },
	{ .name = "DVBT2_7", .value = MOD_STANDARD_DVBT_7 },

	{ .name = "3", .value = MOD_STANDARD_DVBT_6 },
	{ .name = "DVBT_6", .value = MOD_STANDARD_DVBT_6 },
	{ .name = "DVBT2_6", .value = MOD_STANDARD_DVBT_6 },

	{ .name = "4", .value = MOD_STANDARD_DVBT_5 },
	{ .name = "DVBT_5", .value = MOD_STANDARD_DVBT_5 },
	{ .name = "DVBT2_5", .value = MOD_STANDARD_DVBT_5 },

	{ .name = "8", .value = MOD_STANDARD_DVBC_8 },
	{ .name = "DVBC_8", .value = MOD_STANDARD_DVBC_8 },

	{ .name = "9", .value = MOD_STANDARD_DVBC_7 },
	{ .name = "DVBC_7", .value = MOD_STANDARD_DVBC_7 },

	{ .name = "10", .value = MOD_STANDARD_DVBC_6 },
	{ .name = "DVBC_6", .value = MOD_STANDARD_DVBC_6 },

	{ .name = "11", .value = MOD_STANDARD_J83B_QAM64 },
	{ .name = "J83B_QAM64", .value = MOD_STANDARD_J83B_QAM64 },

	{ .name = "12", .value = MOD_STANDARD_J83B_QAM256 },
	{ .name = "J83B_QAM256", .value = MOD_STANDARD_J83B_QAM256 },

	{ .name = "13", .value = MOD_STANDARD_ISDBC_QAM64 },
	{ .name = "ISDBC_QAM64", .value = MOD_STANDARD_ISDBC_QAM64 },
	{ .name = "J83C_QAM64", .value = MOD_STANDARD_ISDBC_QAM64 },

	{ .name = "14", .value = MOD_STANDARD_ISDBC_QAM256 },
	{ .name = "ISDBC_QAM256", .value = MOD_STANDARD_ISDBC_QAM256 },
	{ .name = "J83C_QAM256", .value = MOD_STANDARD_ISDBC_QAM256 },

	{ .name = NULL, .value = 0 }
};

struct param_table_entry stream_format_table[] = {
	{ .name = "0", .value = MOD_FORMAT_DEFAULT },
	{ .name = "default", .value = MOD_FORMAT_DEFAULT },

	{ .name = "1", .value = MOD_FORMAT_IQ16 },
	{ .name = "IQ16", .value = MOD_FORMAT_IQ16 },

	{ .name = "2", .value = MOD_FORMAT_IQ8 },
	{ .name = "IQ8", .value = MOD_FORMAT_IQ8 },

	{ .name = "3", .value = MOD_FORMAT_IDX8 },
	{ .name = "IDX8", .value = MOD_FORMAT_IDX8 },

	{ .name = "4", .value = MOD_FORMAT_TS },
	{ .name = "TS", .value = MOD_FORMAT_TS },

	{ .name = NULL, .value = 0 }
};


struct param_table_entry guard_interval_table[] = {
	{ .name = "0", .value = MOD_DVBT_GI_1_32 },
	{ .name = "1/32", .value = MOD_DVBT_GI_1_32 },

	{ .name = "1", .value = MOD_DVBT_GI_1_16 },
	{ .name = "1/16", .value = MOD_DVBT_GI_1_16 },

	{ .name = "2", .value = MOD_DVBT_GI_1_8	},
	{ .name = "1/8", .value = MOD_DVBT_GI_1_8 },

	{ .name = "3", .value = MOD_DVBT_GI_1_4	},
	{ .name = "1/4", .value = MOD_DVBT_GI_1_4 },
	{ .name = NULL, .value = 0 }
};

struct param_table_entry puncture_rate_table[] = {
	{ .name = "1", .value = MOD_DVBT_PR_1_2 },
	{ .name = "1/2", .value = MOD_DVBT_PR_1_2 },

	{ .name = "2", .value = MOD_DVBT_PR_2_3 },
	{ .name = "2/3", .value = MOD_DVBT_PR_2_3 },

	{ .name = "3", .value = MOD_DVBT_PR_3_4 },
	{ .name = "3/4", .value = MOD_DVBT_PR_3_4 },

	{ .name = "5", .value = MOD_DVBT_PR_5_6 },
	{ .name = "5/6", .value = MOD_DVBT_PR_5_6 },

	{ .name = "7", .value = MOD_DVBT_PR_7_8 },
	{ .name = "7/8", .value = MOD_DVBT_PR_7_8 },

	{ .name = NULL, .value = 0 }
};

struct param_table_entry dvbt_constellation_table[] = {
	{ .name = "0", .value = MOD_DVBT_QPSK },
	{ .name = "qpsk", .value = MOD_DVBT_QPSK },

	{ .name = "1", .value = MOD_DVBT_16QAM },
	{ .name = "16qam", .value = MOD_DVBT_16QAM },
	{ .name = "qam16", .value = MOD_DVBT_16QAM },

	{ .name = "2", .value = MOD_DVBT_64QAM },
	{ .name = "64qam", .value = MOD_DVBT_64QAM },
	{ .name = "qam64", .value = MOD_DVBT_64QAM },

	{ .name = NULL, .value = 0 }
};

struct param_table_entry qam_modulation_table[] = {
	{ .name = "0", .value = MOD_QAM_DVBC_16 },
	{ .name = "qam_dvbc_16", .value = MOD_QAM_DVBC_16 },

	{ .name = "1", .value = MOD_QAM_DVBC_32 },
	{ .name = "qam_dvbc_32", .value = MOD_QAM_DVBC_32 },

	{ .name = "2", .value = MOD_QAM_DVBC_64 },
	{ .name = "qam_dvbc_64", .value = MOD_QAM_DVBC_64 },

	{ .name = "3", .value = MOD_QAM_DVBC_128 },
	{ .name = "qam_dvbc_128", .value = MOD_QAM_DVBC_128 },

	{ .name = "4", .value = MOD_QAM_DVBC_256 },
	{ .name = "qam_dvbc_256", .value = MOD_QAM_DVBC_256 },

	{ .name = "5", .value = MOD_QAM_J83B_64 },
	{ .name = "qam_j83b_64", .value = MOD_QAM_J83B_64 },

	{ .name = "6", .value = MOD_QAM_DVBC_256 },
	{ .name = "qam_j83b_256", .value = MOD_QAM_J83B_256 },

	{ .name = "7", .value = MOD_QAM_GENERIC },
	{ .name = "qam_generic", .value = MOD_QAM_GENERIC },

	{ .name = "8", .value = MOD_QAM_ISDBC_64 },
	{ .name = "qam_isdbc_64", .value = MOD_QAM_ISDBC_64 },

	{ .name = "9", .value = MOD_QAM_ISDBC_256 },
	{ .name = "qam_isdbc_256", .value = MOD_QAM_ISDBC_256 },

	{ .name = NULL, .value = 0 }
};

int parse_param(char *val, struct param_table_entry *table, int *value) {
	if (value) {
		*value = 0;
		if (table) {
			while (table->name) {
				if( !strcasecmp(val,table->name)) {
					*value = table->value;
					printf("%s=%u\n", val, *value);
					return 0;
				}
				table++;
			}
		}
	}
	printf("unknown value %s\n", val);
	return -1;
}

void dump(const uint8_t *b, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) { 
		for (i = 0; i < 16; i++)
			if (i + j < l)
				printf("%02x ", b[i]);
			else
				printf("   ");
		printf(" | ");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				putchar((b[i] > 31 && b[i] < 127) ? b[i] : '.');
		printf("\n");
	}
}

int mci_cmd(int dev, struct mci_command *cmd)
{
	int ret;
	struct ddb_mci_msg msg;
	uint8_t status;

	memset(&msg, 0, sizeof(msg));
	msg.link = 0;
	memcpy(&msg.cmd, cmd, sizeof(msg.cmd));
	//dump((uint8_t *) &msg.cmd, sizeof(msg.cmd));
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
		.standard = MOD_STANDARD_DVBC_8,
#if 0
		.ofdm = {
			.fft_size = 1,
			.guard_interval = 0,
		}
#endif
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
	} else if (!strcasecmp(par, "unit")) {
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
	int value;

	if (!par && !val) {
		mc->set_channels = 1;
		return;
	}
	if (!strcasecmp(par, "frequency")) {
		mc->channels.mod_setup_channels[0].frequency =	(uint32_t) (strtod(val, NULL) * 1000000.0);
		printf("frequency = %u\n", mc->channels.mod_setup_channels[0].frequency);
	} else if (!strcasecmp(par, "channels")) {
		mc->channels.mod_setup_channels[0].num_channels = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "standard")) {
		if (!parse_param(val,mod_standard_table, &value))
			mc->channels.mod_setup_channels[0].standard = value;
		printf("standard = %u\n", value);
	} else if (!strcasecmp(par, "offset")) {
		mc->channels.mod_setup_channels[0].offset = (uint32_t) (strtod(val, NULL) * 1000000.0);
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
	int value;

	if (!par && !val) {
		return;
	}
	if (!strcasecmp(par, "fft_size")) {
		mc->stream.mod_setup_stream.ofdm.fft_size = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "guard_interval")) {
		if (!parse_param(val, guard_interval_table, &value))
			mc->stream.mod_setup_stream.ofdm.guard_interval = value;
	} else if (!strcasecmp(par, "puncture_rate")) {
		if (!parse_param(val, puncture_rate_table, &value))
			mc->stream.mod_setup_stream.ofdm.puncture_rate = value;
	} else if (!strcasecmp(par, "constellation")) {
		if (!parse_param(val,dvbt_constellation_table,&value))
			mc->stream.mod_setup_stream.ofdm.constellation = value;
	} else if (!strcasecmp(par, "cell_identifier")) {
		mc->stream.mod_setup_stream.ofdm.cell_identifier = strtol(val, NULL, 0);
	} else if (!strcasecmp(par, "modulation")) {
		if (!parse_param(val, qam_modulation_table, &value))
			mc->stream.mod_setup_stream.qam.modulation = value;
	} else if (!strcasecmp(par, "rolloff")) {
		mc->stream.mod_setup_stream.qam.rolloff = strtol(val, NULL, 0);
	} else if (!strcasecmp(par, "standard")) {
		if (!parse_param(val,mod_standard_table,&value))
			mc->stream.mod_setup_stream.standard = value;
	} else if (!strcasecmp(par, "stream_format")) {
		if (!parse_param(val,stream_format_table,&value))
			mc->stream.mod_setup_stream.stream_format = value;
	} else if (!strcasecmp(par, "symbol_rate")) {
		mc->stream.mod_setup_stream.symbol_rate = (uint32_t) (strtod(val, NULL) * 1000000.0);
	} else if (!strcasecmp(par, "channel")) {
		mc->stream.mod_channel = strtol(val, NULL, 10);
	} else if (!strcasecmp(par, "stream")) {
		mc->stream.mod_stream = strtol(val, NULL, 10);
		printf("set stream %u to channel %u\n", mc->stream.mod_stream, mc->stream.mod_channel);
		mci_cmd(mc->fd, &mc->stream);
	} else
		printf("invalid streams parameter: %s = %s\n", par, val);
}

int mci_lic(int dev)
{
	struct ddb_mci_msg msg = {
		.cmd.command = CMD_EXPORT_LICENSE,
		.cmd.get_bb_header.select = 0,
	};
	struct mci_result *res = &msg.res;
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}
	if (res->bb_header.valid) {
		printf("MATYPE1: %02x\n", res->bb_header.matype_1);
		printf("MATYPE2: %02x\n", res->bb_header.matype_2);
	}
	dump(&res->license, sizeof(res->license));
	return ret;
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
	//snprintf(fn, 127, "/dev/ddbridge/card%u", device);
	snprintf(fn, 127, "/dev/dvb/adapter%u/mod0", device);
	fd = open(fn, O_RDWR);
	if (fd < 0) {
		dprintf(2, "Could not open %s\n", fn);
		return -1;
	}
	//mci_lic(fd);
	mc.fd = fd;
	parse(configname, "channels", (void *) &mc, channels_cb);
	if (mc.set_channels) {
		printf("setting channels.\n");
		mci_cmd(fd, &mc.channels);
	}
	parse(configname, "streams", (void *) &mc, streams_cb);
	parse(configname, "output", (void *) &mc, output_cb);
	if (mc.set_output) {
		printf("setting output.\n");
		mci_cmd(fd, &mc.output);
	}
}
