/*
/* ddflash - Programmer for flash on Digital Devices devices
 *
 * Copyright (C) 2013 Digital Devices GmbH
 *                    Ralph Metzler <rmetzler@digitaldevices.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 3 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "flash.h"
#include "flash.c"

static int update_flash(struct ddflash *ddf)
{
	char *fname;
	int res, stat = 0;

	switch (ddf->id.device) {
	case 0x300:
	case 0x301:
	case 0x302:
	case 0x307:
		if ((res = update_image(ddf, "/boot/bs.img", 0x4000, 0x1000, 0, 0)) == 1)
			stat |= 4;
		if ((res = update_image(ddf, "/boot/uboot.img", 0xb0000, 0xb0000, 0, 0)) == 1)
			stat |= 2;
		if (fexists("/config/gtl.enabled")) {
			if ((res = update_image(ddf, "/config/fpga_gtl.img", 0x10000, 0xa0000, 1, 0)) == 1)
				stat |= 1;
			if (res == -1)
				if ((res = update_image(ddf, "/boot/fpga_gtl.img", 0x10000, 0xa0000, 1, 0)) == 1)
					stat |= 1;
		} else if (fexists("/config/gtl.disabled")) {
			if ((res = update_image(ddf, "/config/fpga.img", 0x10000, 0xa0000, 1, 0)) == 1)
				stat |= 1;
			if (res == -1)
				if ((res = update_image(ddf, "/boot/fpga.img", 0x10000, 0xa0000, 1, 0)) == 1)
					stat |= 1;
		} else {
			if (ddf->id.device == 0x0307) {
				if (res == -1)
					if ((res = update_image(ddf, "/config/fpga_gtl.img", 0x10000, 0xa0000, 1, 1)) == 1)
						stat |= 1;
				if (res == -1)
					if ((res = update_image(ddf, "/boot/fpga_gtl.img", 0x10000, 0xa0000, 1, 1)) == 1)
						stat |= 1;
			} else {
				if ((res = update_image(ddf, "/config/fpga.img", 0x10000, 0xa0000, 1, 1)) == 1)
					stat |= 1;
				if (res == -1)
					if ((res = update_image(ddf, "/boot/fpga.img", 0x10000, 0xa0000, 1, 1)) == 1)
						stat |= 1;
			}
		}
#if 1
		if ( (stat&1) && (ddf->id.hw & 0xffffff) <= 0x010001) {		
			if (ddf->id.device == 0x0307) {
				if ((res = update_image(ddf, "/config/fpga_gtl.img", 0x160000, 0x80000, 1, 0)) == 1)
					stat |= 1;
				if (res == -1)
					if ((res = update_image(ddf, "/boot/fpga_gtl.img", 0x160000, 0x80000, 1, 0)) == 1)
						stat |= 1;
			} else {
				if ((res = update_image(ddf, "/config/fpga.img", 0x160000, 0x80000, 1, 0)) == 1)
					stat |= 1;
				if (res == -1)
					if ((res = update_image(ddf, "/boot/fpga.img", 0x160000, 0x80000, 1, 0)) == 1)
						stat |= 1;
			
			}
		}
#endif

		break;
	case 0x320:
		//fname="/boot/DVBNetV1A_DD01_0300.bit";
		fname="/boot/fpga.img";
		if ((res = update_image(ddf, fname, 0x10000, 0x100000, 1, 0)) == 1)
			stat |= 1;
		return stat;
		break;
	case 0x322:
		//fname="/boot/DVBNetV1A_DD01_0300.bit";
		fname="/boot/fpga.img";
		if ((res = update_image(ddf, fname, 0x10000, 0x100000, 1, 0)) == 1)
			stat |= 1;
		return stat;
		break;
	default:
		return 0;
	}
	return stat;
}

int main(int argc, char **argv)
{
	struct ddflash ddf;
	char ddbname[80];
	uint8_t *buffer = 0;
	uint32_t FlashOffset = 0x10000;
	int i, err, res;
	int ddbnum = 0;

	uint32_t svid, jump, flash;

	memset(&ddf, 0, sizeof(ddf));

        while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"svid", required_argument, NULL, 's'},
			{"help", no_argument , NULL, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"d:n:s:o:l:dfhj",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
		case 's':
			svid = strtoul(optarg, NULL, 16);
			break;
		case 'o':
			FlashOffset = strtoul(optarg, NULL, 16);
			break;
		case 'n':
			ddbnum = strtol(optarg, NULL, 0);
			break;
		case 'j':
			jump = 1;
			break;
		case 'h':
		default:
			break;

		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}
	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	while ((ddf.fd = open(ddbname, O_RDWR)) < 0) {
		if (errno == EBUSY)
			usleep(100000);
		else {
			printf("Could not open device\n");
			return -1;
		}
	}
	ddf.link = 0;
	flash = flash_detect(&ddf);
	if (flash < 0)
		return -1;
	get_id(&ddf);

	res = update_flash(&ddf);

	if (ddf.buffer)
		free(ddf.buffer);
	if (res < 0)
		return res;
	if (res & 1)
		reboot(40);
	return res;
}
