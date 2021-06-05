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

void print_temp(struct mci_result *res)
{
	printf("Die temperature = %u\n", res->sx8_bist.temperature);
}

int temp_info(int dev)
{
	struct ddb_mci_msg msg = {
		.link = 0,
		.cmd.command = SX8_CMD_GETBIST,
	};
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}
	if (msg.res.status & 0x80) {
		printf("MCI errror %02x\n", msg.res.status);
		return ret;
	}
		
	print_temp(&msg.res);
	return ret;
}


void print_info(struct mci_result *res, uint8_t demod)
{
	if (res->status == MCI_DEMOD_STOPPED) {
		printf("Demod %u stopped\n", demod);
		return;
	}
	
	printf("Demod %u:\n", demod);
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
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}

	print_info(&msg.res, demod);
	return ret;
}

static int get_id(int fd, int link, struct ddb_id *id)
{
	struct ddb_reg ddbreg;

	if (link == 0) {
		if (ioctl(fd, IOCTL_DDB_ID, id) < 0)
			return -1;
		return 0;
	}
        ddbreg.reg = 8 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->vendor = ddbreg.val;
	id->device = ddbreg.val >> 16;

        ddbreg.reg = 12 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->subvendor = ddbreg.val;
	id->subdevice = ddbreg.val >> 16;

        ddbreg.reg = 0 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->hw = ddbreg.val;

	ddbreg.reg = 4 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->regmap = ddbreg.val;
	return 0;
}

static char *id2name(uint16_t id)
{
	switch (id) {
	case 0x222:
		return "MOD";
	case 0x0009:
		return "MAX SX8";
	case 0x000b:
		return "MAX SX8 Basic";
	case 0x000a:
		return "MAX M4";
	default:
		return " ";
	}
}

static int card_info(int ddbnum, int demod)
{
	char ddbname[80];
	struct ddb_id ddbid;
	int ddb, ret, link, links = 1, i;
	struct ddb_id id;
	
	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0)
		return -3;

	for (link = 0; link < links; link++) {
		ret = get_id(ddb, link, &id);
		if (ret < 0)
			goto out;
		if (!link) {
			switch (id.device) {
			case 0x20:
				links = 4;
				break;
			case 0x300:
			case 0x301:
			case 0x307:
				links = 2;
				break;
			
			default:
				break;
			}
		}
		printf("\n\nCard %s link %u id %04x (%s):\n",
		       ddbname, link, id.device, id2name(id.device));
		printf("HW %08x REGMAP %08x FW %u.%u\n",
		       id.hw, id.regmap, (id.hw & 0xff0000) >> 16, (id.hw & 0xffff));
		switch (id.device) {
		case 0x0009:
			if (demod >= 0)
				mci_info(ddb, demod);
			else {
				for (i = 0; i < 8; i++)
					mci_info(ddb, i);
			}
			temp_info(ddb);
			break;
		case 0x000a:
			if (demod >= 0)
				mci_info(ddb, demod);
			else {
				for (i = 0; i < 4; i++)
					mci_info(ddb, i);
			}
			break;
		default:
			break;
		}
	}
	
out:
	close(ddb);
	return ret;
}

int main(int argc, char*argv[])
{
	int fd = -1, all = 1, i, ret = 0;
	char fn[128];
	int32_t device = -1, demod = -1;
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"device", required_argument, 0, 'd'},
			{"demod", required_argument, 0, 'n'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, "ad:n:",
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
		case 'a':
			all = 1;
			break;
		default:
			break;
		}
	}
	if (optind < argc) {
		printf("too many arguments\n");
		exit(1);
	}
	if (device >=0)
		ret = card_info(device, demod);
	else
		for (i = 0; i < 100; i++) {
			ret = card_info(i, -1);
			
			if (ret == -3)     /* could not open, no more cards! */
				break; 
			if (ret < 0)
				return i; /* fatal error */ 
		}
}
