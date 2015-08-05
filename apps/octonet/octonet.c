#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include "ns.h"


struct dvb_ns_params nsp = {
	.dmac = { 0x00, 0x01, 0x2e, 0x3a, 0x66,0xfc },
	.smac = { 0x00, 0x12, 0x34, 0x56, 0x78,0x90 },
	.sip = { 192, 168, 2, 80 },
	.dip = { 192, 168, 2, 58 },
	.sport = 1234,
	.dport = 6670,
	.ssrc = { 0x91, 0x82, 0x73, 0x64 },
};

static int set(int fd)
{
	uint16_t pid = 0xa000;

	ioctl(fd, NS_SET_NET, &nsp);
	ioctl(fd, NS_START);
	ioctl(fd, NS_SET_PID, &pid);
	while(1);
	ioctl(fd, NS_STOP);
	return 0;
}

int main(int argc, char **argv)
{
	int ddbnum;
	int force;
	int ddb;
	char ddbname[80];

        while (1) {
                int oi = 0;
		int c;
                static struct option lopts[] = {
			{"help", no_argument , NULL, 'h'},
			{"force", no_argument , NULL, 'f'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"n:l:fh",
				lopts, &oi);
		if (c == -1)
			break;

		switch (c) {
		case 'm':
			
			break;
		case 'n':
			ddbnum = strtol(optarg, NULL, 0);
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
		default:
			break;

		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}
	sprintf(ddbname, "/dev/dvb/adapter0/ns%d", ddbnum);
	ddb=open(ddbname, O_RDWR);
	if (ddb < 0) {
		printf("Could not open device\n");
		return -1;
	}
}
