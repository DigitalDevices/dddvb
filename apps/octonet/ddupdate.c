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

static int verbose = 0;

static int yesno()
{
	char c;

	printf("\n\nNew firmware available\nReally flash now? y/n\n");
	fflush(0);
	c = getchar();
	if (c!='y') {
		printf("\nFlashing aborted.\n\n");
		return 0;
	}
	printf("\nStarting to flash\n\n");
	return 1;
}

static int update_flash(struct ddflash *ddf)
{
	char *fname, *default_fname;
	int res, stat = 0;
	char *name = 0, *dname;
	uint32_t imgadr = 0x10000;
	
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
	{
		uint32_t val;
		if (!readreg(ddf->fd, (ddf->link << 28) | 0x10, &val)) {
			//printf("reg0x10=%08x\n", val);
			if ((val >> 24) == 5)
				imgadr = 0;
		}
	}
		fname = ddf->fname;
		default_fname = devid2fname(ddf->id.device, &name);
		if (!fname)
			fname = default_fname;
		if (name)
			printf("Card:     %s\n", name);
		if (ddf->flash_name)
			printf("Flash:    %s\n", ddf->flash_name);
		printf("Version:  %08x\n", ddf->id.hw);
		printf("REGMAP :  %08x\n", ddf->id.regmap);
		printf("Address:  %08x\n", imgadr);
		if ((res = update_image(ddf, fname, imgadr, ddf->size / 2, 1, 0)) == 1)
			stat |= 1;
		return stat;
	}
	return stat;
}

static int update_link(struct ddflash *ddf)
{
	int ret;
	
	ret = flash_detect(ddf);
	if (ret < 0)
		return ret;
	ret = update_flash(ddf);

	if (ddf->buffer)
		free(ddf->buffer);

	return ret;
}

static int update_card(int ddbnum, char *fname, int force)
{
	struct ddflash ddf;
	char ddbname[80];
	struct ddb_id ddbid;
	int ddb, ret, link, links;
	
	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0)
		return -3;
	ddf.fd = ddb;
	ddf.link = 0;
	ddf.fname = fname;
	ddf.force = force;
	links = 1;

	for (link = 0; link < links; link++) {
		ddf.link = link;
		if (verbose >= 2)
			printf("Get id card %u link %u\n", ddbnum, link);
		ret = get_id(&ddf);
		if (ret < 0)
			goto out;
		if (!link) {
			switch (ddf.id.device) {
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
		//printf("%08x %08x\n", ddf.id.device, ddf.id.subdevice);
		if (ddf.id.device) {
			printf("\n\nUpdate card %s link %u:\n", ddbname, link);
			ret = update_link(&ddf);
			//if (ret < 0)
			//	break;
		}
	}
	
out:
	close(ddb);
	return ret;
}

static int usage()
{
	printf("ddupdate [OPTION]\n\n"
	       "-n N\n  only update card N (default with N=0)\n\n"
	       "-a \n   update all cards\n\n"
	       "-b file\n  fpga image file override (ignored if -a is used)\n\n"
	       "-f  \n  force  update\n\n"
	       "-v \n   more verbose (up to -v -v -v)\n\n"
		);
}

int main(int argc, char **argv)
{
	int ddbnum = -1, all = 0, i, force = 0, reboot_len = -1;
	char *fname = 0;
	int ret;
	
        while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"reboot", optional_argument , NULL, 'r'},
			{"help", no_argument , NULL, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"n:havfb:r::",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
		case 'b':
			fname = optarg;
			break;
		case 'n':
			ddbnum = strtol(optarg, NULL, 0);
			break;
		case 'a':
			all = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			if (optarg)
				reboot_len = strtol(optarg, NULL, 0);
			else 
				reboot_len = 40;
			if (!reboot_len)
				reboot(40);
			break;
		case 'h':
			usage();
			return 0;
		default:
			break;

		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}
	if (!all && (ddbnum < 0)) {
		printf("Select card number or all cards\n\n");
		usage();
		return -1;
	}
		
	if (!all)
		ret = update_card(ddbnum, fname, force);
	else
		for (i = 0; i < 100; i++) {
			ret = update_card(i, 0, 0);
			
			if (ret == -3)     /* could not open, no more cards! */
				break; 
			if (ret < 0)
				return i; /* fatal error */ 
			if (verbose >= 1)
				printf("card %d up to date\n", i);
		}
	if (reboot_len > 0)
		reboot(reboot_len);
	return 0; 
}
