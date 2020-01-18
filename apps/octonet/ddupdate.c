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


static int update_flash(struct ddflash *ddf)
{
	char *fname;
	int res, stat = 0;
	char *name;
	
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
		fname = ddf->fname;
		if (!fname)
			fname = devid2fname(ddf->id.device, &name);
		if (name)
			printf("Card: %s\n", name);
		if ((res = update_image(ddf, fname, 0x10000, 0x100000, 1, 0)) == 1)
			stat |= 1;
		return stat;
	}
	return stat;
}

static int ddupdate(struct ddflash *ddf)
{
	int ret;
	
	if (verbose >= 2)
		printf("Detect flash type\n");
	ret = flash_detect(ddf);
	if (ret < 0)
		return ret;
	ret = update_flash(ddf);

	if (ddf->buffer)
		free(ddf->buffer);

	return ret;
}

static int proc_card(int ddbnum, char *fname)
{
	struct ddflash ddf;
	char ddbname[80];
	struct ddb_id ddbid;
	int ddb, ret, link, links;
	
	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	if (verbose >= 2)
		printf("Update card %s\n", ddbname);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0)
		return -3;
	ddf.fd = ddb;
	ddf.link = 0;
	ddf.fname = fname;
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
				links = 1;
				break;
			
			default:
				break;
			}
		}

		//printf("%08x %08x\n", ddf.id.device, ddf.id.subdevice);
		if (ddf.id.device) {
			ret = ddupdate(&ddf);
			if (ret < 0)
				break;
		}
	}
	
out:
	close(ddb);
	return ret;
}


int main(int argc, char **argv)
{
	int ddbnum = 0, all = 0, i, force = 0;
	char *fname;
	
        while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"help", no_argument , NULL, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"n:havfb:",
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
		case 'v':
			verbose++;
			break;
		case 'h':
			printf("ddupdate [OPTION]\n\n"
			       "-n N\n  only update card N (default with N=0)\n\n"
			       "-a \n   update all cards\n\n"
			       "-b file\n  fpga image file override (not if -a is used)\n\n"
			       "-v \n   more verbose (up to -v -v -v)\n\n"
				);
			break;
		default:
			break;

		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}

	if (!all)
		return proc_card(ddbnum, fname);

	for (i = 0; i < 20; i++) {
		int ret = proc_card(i, 0);
		
		if (ret == -3)     /* could not open, no more cards! */
			break; 
		if (ret < 0)
			return i; /* fatal error */ 
		if (verbose >= 1)
			printf("card %d up to date\n", i);
	}
	return 0; 
}
