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

void print_iq(struct mci_result *res, int fd)
{
	dprintf(fd, "%d,%d\n", res->iq_symbol.i, res->iq_symbol.q);
}

int get_iq(int dev, uint32_t link, uint8_t demod, int fd)
{
	struct ddb_mci_msg msg = {
		.link = link,
		.cmd.command = MCI_CMD_GET_IQSYMBOL,
		.cmd.demod = demod,
		.cmd.get_iq_symbol.tap = 0,
		.cmd.get_iq_symbol.point = 0,
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
		
	print_iq(&msg.res, fd);
	return ret;
}


#define SIZE_OF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

int main(int argc, char*argv[])
{
	char ddbname[80];
	int fd = -1, all = 1, i, ret = 0, ddb;
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
	sprintf(ddbname, "/dev/ddbridge/card%d", device);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0)
		return -3;
	for (i = 0; i < 20000; i++)
		get_iq(ddb, 0, demod, 1);
}
