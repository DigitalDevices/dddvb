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

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "../ddbridge/ddbridge-mci.h"
#include "../ddbridge/ddbridge-ioctl.h"

char *Rolloff[8] = {
	"0.35",
	"0.25",
	"0.20",
	"0.10",
	"0.05",
	"0.15",
	"rsvd",
	"rsvd",
};

void print_info(struct mci_result *res)
{
	if (res->status == MCI_DEMOD_STOPPED) {
		printf("Demod stopped\n");
		return;
	}
	
	switch (res->mode) {
		case 0:
		case M4_MODE_DVBSX:
		
		if (res->dvbs2_signal_info.standard == 2) {
			printf("PLS-Code:      %u\n", res->dvbs2_signal_info.pls_code);
			printf("Roll-Off:      %s\n", Rolloff[res->dvbs2_signal_info.roll_off]);
			printf("Inversion:     %s\n", (res->dvbs2_signal_info.roll_off & 0x80) ? "on": "off");
			printf("Frequency:     %u Hz\n", res->dvbs2_signal_info.frequency);
			printf("Symbol Rate:   %u Symbols/s\n", res->dvbs2_signal_info.symbol_rate);
			printf("Channel Power: %.2f dBm\n", (float) res->dvbs2_signal_info.channel_power / 100);
			printf("Band Power:    %.2f dBm\n", (float) res->dvbs2_signal_info.band_power / 100);
			printf("SNR:           %.2f dB\n", (float) res->dvbs2_signal_info.signal_to_noise / 100);
			printf("Packet Errors: %u\n", res->dvbs2_signal_info.packet_errors);
			printf("BER Numerator: %u\n", res->dvbs2_signal_info.ber_numerator);
			printf("BER Denom.:    %u\n", res->dvbs2_signal_info.ber_denominator);
			printf("\n");
		} else {
			
		}
	}
}

int mci_info(int dev, uint8_t demod)
{
	struct ddb_mci_msg msg = {
		.link = 0,
		.cmd.command = MCI_CMD_GETSIGNALINFO,
		.cmd.demod = demod
	};
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("%d %d\n", ret, errno);
		return ret;
	}

	print_info(&msg.res);
	return ret;
}

int main(int argc, char*argv[])
{
	int fd = -1;
	char fn[128];
	uint32_t device = 0;
	uint8_t demod = 0;
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"device", required_argument, 0, 'd'},
			{"demod", required_argument, 0, 'n'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, "d:n:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'd':
			device = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			demod = strtoul(optarg, NULL, 0);
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
	if (fd < 0)
		return -1;
	mci_info(fd, demod);
}
