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

static int reboot(uint32_t off)
{
	FILE *f;
	uint32_t time;

	if ((f = fopen ("/sys/class/rtc/rtc0/since_epoch", "r")) == NULL)
		return -1;
	fscanf(f, "%u", &time);
	fclose(f);

	if ((f = fopen ("/sys/class/rtc/rtc0/wakealarm", "r+")) == NULL)
		return -1;
	fprintf(f, "%u", time + off);
	fclose(f);
	system("/sbin/poweroff");
	return 0;
}

struct ddflash {
	int fd;
	struct ddb_id id;
	uint32_t version;

	uint32_t flash_type;
	uint32_t sector_size;
	uint32_t size;

	uint32_t bufsize;
	uint32_t block_erase;

	uint8_t *buffer;
};

int flashwrite_pagemode(struct ddflash *ddf, int dev, uint32_t FlashOffset,
			uint8_t LockBits, uint32_t fw_off)
{
	int err = 0;
	uint8_t cmd[260];
	int i, j;
	uint32_t flen, blen;
	
	blen = flen = lseek(dev, 0, SEEK_END) - fw_off;
	if (blen % 0xff)
		blen = (blen + 0xff) & 0xffffff00; 
	printf("blen = %u, flen = %u\n", blen, flen);
	    
	do {
		cmd[0] = 0x50;  // EWSR
		err = flashio(ddf->fd, cmd, 1, NULL, 0);
		if (err < 0)
			break;
		
		cmd[0] = 0x01;  // WRSR
		cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = flashio(ddf->fd, cmd, 2, NULL, 0);
		if (err < 0)
			break;
		
		for (i = 0; i < flen; i += 4096) {
			if ((i & 0xFFFF) == 0)
				printf(" Erase    %08x\n", FlashOffset + i);
			
			cmd[0] = 0x06;  // WREN
			err = flashio(ddf->fd, cmd, 1, NULL, 0);
			if (err < 0)
				break;
			
			cmd[0] = 0x20;  // Sector erase ( 4Kb)
			cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
			cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
			cmd[3] = 0x00;
			err = flashio(ddf->fd, cmd, 4, NULL, 0);
			if (err < 0)
				break;

			while (1) {
				cmd[0] = 0x05;  // RDRS
				err = flashio(ddf->fd, cmd, 1, &cmd[0], 1);
				if (err < 0)
					break;
				if ((cmd[0] & 0x01) == 0)
					break;
			}
			if (err < 0)
				break;
			
		}
		if (err < 0)
			break;
		
		for (j = blen - 256; j >= 0; j -= 256 ) {
			uint32_t len = 256; 
			ssize_t rlen;
			
			if (lseek(dev, j + fw_off, SEEK_SET) < 0) {
				printf("seek error\n");
				return -1;
			}
			if (flen - j < 256) {
				len = flen - j;
				memset(ddf->buffer, 0xff, 256);
			}
			rlen = read(dev, ddf->buffer, len);
			if (rlen < 0 || rlen != len) {
				printf("file read error %d,%d at %u\n", rlen, errno, j);
				return -1;
			}
			printf ("write %u bytes at %08x\n", len, j);
			
			
			if ((j & 0xFFFF) == 0)
				printf(" Programm %08x\n", FlashOffset + j);
			
			cmd[0] = 0x06;  // WREN
			err = flashio(ddf->fd, cmd, 1, NULL, 0);
			if (err < 0)
				break;
			
			cmd[0] = 0x02;  // PP
			cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
			cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
			cmd[3] = 0x00;
			memcpy(&cmd[4], ddf->buffer, 256);
			err = flashio(ddf->fd, cmd, 260, NULL, 0);
			if (err < 0)
				break;
			
			while(1) {
				cmd[0] = 0x05;  // RDRS
				err = flashio(ddf->fd, cmd,1, &cmd[0], 1);
				if (err < 0)
					break;
				if ((cmd[0] & 0x01) == 0)
					break;
			}
			if (err < 0)
				break;
			
		}
		if (err < 0)
			break;
		
		cmd[0] = 0x50;  // EWSR
		err = flashio(ddf->fd, cmd, 1, NULL, 0);
		if (err < 0)
			break;
		
		cmd[0] = 0x01;  // WRSR
		cmd[1] = LockBits;  // BPx = 0, Lock all blocks
		err = flashio(ddf->fd, cmd, 2, NULL, 0);
	} while(0);
	return err;
}


static int flashwrite_SSTI(struct ddflash *ddf, int fs, uint32_t FlashOffset, uint32_t maxlen, uint32_t fw_off)
{
    int err = 0;
    uint8_t cmd[6];
    int i, j;
    uint32_t flen, blen;

    blen = flen = lseek(fs, 0, SEEK_END) - fw_off;
    if (blen % 0xfff)
	    blen = (blen + 0xfff) & 0xfffff000; 
    printf("blen = %u, flen = %u\n", blen, flen);
    do {
#if 1
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(ddf->fd, cmd, 1, NULL, 0);
	    if (err < 0) 
		    break;

	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(ddf->fd, cmd, 2, NULL, 0);
	    if (err < 0 )
		    break;
	    
	    for (i = 0; i < flen; i += 4096) {
		    if ((i & 0xFFFF) == 0 )
			    printf("Erase %08x\n", FlashOffset + i);
		    cmd[0] = 0x06;  // WREN
		    err = flashio(ddf->fd, cmd, 1, NULL, 0);
		    if (err < 0 )
			    break;
		    
		    cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    cmd[1] = (((FlashOffset + i ) >> 16) & 0xFF);
		    cmd[2] = (((FlashOffset + i ) >>  8) & 0xFF);
		    cmd[3] = 0x00;
		    err = flashio(ddf->fd,cmd,4,NULL,0);
		    if (err < 0 )
			    break;
		    
		    while(1) {
			    cmd[0] = 0x05;  // RDRS
			    err = flashio(ddf->fd,cmd,1,&cmd[0],1);
			    if (err < 0 ) break;
			    if ((cmd[0] & 0x01) == 0 ) break;
		    }
		    if (err < 0 ) break;
	    }
	    if (err < 0 ) 
		    break;
#endif
	    for (j = blen - 4096; j >= 0; j -= 4096 ) {
		    uint32_t len = 4096; 
		    ssize_t rlen;
		    
		    if (lseek(fs, j + fw_off, SEEK_SET) < 0) {
			    printf("seek error\n");
			    return -1;
		    }
		    if (flen - j < 4096) {
			    len = flen - j;
			    memset(ddf->buffer, 0xff, 4096);
		    }
   		    rlen = read(fs, ddf->buffer, len);
		    if (rlen < 0 || rlen != len) {
			    printf("file read error %d,%d at %u\n", rlen, errno, j);
			    return -1;
		    }
		    printf ("write %u bytes at %08x\n", len, j);

		    if ((j & 0xFFFF) == 0 )
			    printf(" Program  %08x\n",FlashOffset + j);
#if 1		    
		    for (i = 0; i < 4096; i += 2) {
			    if (i == 0) {
				    cmd[0] = 0x06;  // WREN
				    err = flashio(ddf->fd, cmd, 1, NULL, 0);
				    if (err < 0 ) 
					    break;
				    
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ((( FlashOffset + j ) >> 16) & 0xFF );
				    cmd[2] = ((( FlashOffset + j ) >>  8) & 0xFF );
				    cmd[3] = 0x00;
				    cmd[4] = ddf->buffer[i];
				    cmd[5] = ddf->buffer[i + 1];
				    err = flashio(ddf->fd,cmd,6,NULL,0);
			    } else {
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ddf->buffer[i];
				    cmd[2] = ddf->buffer[i + 1];
				    err = flashio(ddf->fd,cmd,3,NULL,0);
			    }
			    if (err < 0 ) 
				    break;
			    
			    while(1) {
				    cmd[0] = 0x05;  // RDRS
				    err = flashio(ddf->fd,cmd,1,&cmd[0],1);
				    if (err < 0 ) break;
				    if ((cmd[0] & 0x01) == 0 ) break;
			    }
			    if (err < 0 ) 
				    break;
		    }
		    if (err < 0)
			    break;
		    
		    cmd[0] = 0x04;  // WDIS
		    err = flashio(ddf->fd, cmd, 1, NULL, 0);
		    if (err < 0 ) 
			    break;
#endif
	    }
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(ddf->fd,cmd,1,NULL,0);
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(ddf->fd,cmd,2,NULL,0);
    } while(0);
    return err;
}


static int flashwrite(struct ddflash *ddf, int fs, uint32_t addr, uint32_t maxlen, uint32_t fw_off)
{
	switch (ddf->flash_type) {
        case SSTI_SST25VF016B: 
        case SSTI_SST25VF032B: 
		return flashwrite_SSTI(ddf, fs, addr, maxlen, fw_off);
        case SSTI_SST25VF064C:
		return flashwrite_pagemode(ddf, fs, addr, 0x3c, fw_off);
	case SPANSION_S25FL116K: 
	case SPANSION_S25FL132K: 
	case SPANSION_S25FL164K: 
		return flashwrite_pagemode(ddf, fs, addr, 0x1c, fw_off);
	}
	return -1;
}

static int flashcmp(struct ddflash *ddf, int fs, uint32_t addr, uint32_t maxlen, uint32_t fw_off)
{
	off_t off;
	uint32_t len;
	int i, j, rlen;
	uint8_t buf[256], buf2[256];
	int bl = sizeof(buf);
	
	off = lseek(fs, 0, SEEK_END);
	if (off < 0)
		return -1;
	len = off - fw_off;
	lseek(fs, fw_off, SEEK_SET);
	if (len > maxlen) {
		printf("file too big\n");
		return -1;
	}
	printf("flash file len %u, compare to %08x in flash\n", len, addr);
	for (j = 0; j < len; j += bl, addr += bl) {
		if (len - j < bl)
			bl = len - j;
		flashread(ddf->fd, buf, addr, bl);
		rlen = read(fs, buf2, bl);
		if (rlen < 0 || rlen != bl) {
			printf("read error\n");
			return -1;
		}
			
		if (memcmp(buf, buf2, bl)) {
			printf("flash differs at %08x (offset %u)\n", addr, j);
			dump(buf, 32);
			dump(buf2, 32);
			return addr;
		}
	}
	printf("flash same as file\n");
	return -2;
}


static int flash_detect(struct ddflash *ddf)
{
	uint8_t cmd = 0x9F;
	uint8_t id[3];
	
	int r = flashio(ddf->fd, &cmd, 1, id, 3);
	if (r < 0)
		return r;
	
	if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x41) {
		ddf->flash_type = SSTI_SST25VF016B; 
		printf("Flash: SSTI  SST25VF016B 16 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x200000; 
	} else if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x4A) {
		ddf->flash_type = SSTI_SST25VF032B; 
		printf("Flash: SSTI  SST25VF032B 32 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x400000; 
	} else if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x4B) {
		ddf->flash_type = SSTI_SST25VF064C; 
		printf("Flash: SSTI  SST25VF064C 64 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x800000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x15) {
		ddf->flash_type = SPANSION_S25FL116K;
		printf("Flash: SPANSION S25FL116K 16 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x200000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x16) {
		ddf->flash_type = SPANSION_S25FL132K;
		printf("Flash: SPANSION S25FL132K 32 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x400000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x17) {
		ddf->flash_type = SPANSION_S25FL164K;
		printf("Flash: SPANSION S25FL164K 64 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x800000; 
	} else if (id[0] == 0x1F && id[1] == 0x28) {
		ddf->flash_type = ATMEL_AT45DB642D; 
		printf("Flash: Atmel AT45DB642D  64 MBit\n");
		ddf->sector_size = 1024; 
		ddf->size = 0x800000; 
	} else {
		printf("Unknown Flash Flash ID = %02x %02x %02x\n", id[0], id[1], id[2]);
		return -1;
	}
	if (ddf->sector_size) {
		ddf->buffer = malloc(ddf->sector_size);
		printf("allocated buffer %08x@%08x\n", ddf->sector_size, (uint32_t) ddf->buffer);
		if (!ddf->buffer)
			return -1;
	}
	return 0;
}


static int get_id(struct ddflash *ddf) {
	uint8_t id[4];

	if (ioctl(ddf->fd, IOCTL_DDB_ID, &ddf->id) < 0)
		return -1;
#if 1
	printf("%04x %04x %04x %04x %08x %08x\n",
	       ddf->id.vendor, ddf->id.device,
	       ddf->id.subvendor, ddf->id.subdevice,
	       ddf->id.hw, ddf->id.regmap);
#endif	
	return 0;
}

static int check_fw(struct ddflash *ddf, char *fn, uint32_t *fw_off)
{
	int fd, fsize, ret = 0;
	off_t off;
	uint32_t p, i;
	uint8_t *buf;
	uint8_t hdr[256];
	unsigned int devid, version, length;
	unsigned int cid[8];
	int cids = 0;
	uint32_t maxlen = 1024 * 1024;
	
	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		printf("%s: not found\n", fn);
		return -1;
	}
	off = lseek(fd, 0, SEEK_END);
	if (off < 0)
		return -1;
	fsize = off;
	if (fsize > maxlen) {
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);	
	buf = malloc(fsize);
	if (!buf)
		return -1;
	read(fd, buf, fsize);
	close(fd);
	
	for (p = 0; p < fsize && buf[p]; p++) {
		char *key = &buf[p], *val = NULL;

		for (; p < fsize && buf[p] != 0x0a; p++) {
			if (buf[p] == ':') {
				buf[p] = 0;
				val = &buf[p + 1];
			}
		}
		if (val == NULL || p == fsize)
			break;
		buf[p] = 0;
		//printf("%-20s:%s\n", key, val);
		if (!strcasecmp(key, "Devid")) {
			sscanf(val, "%x", &devid);
		} else if (!strcasecmp(key, "Compat")) {
			cids = sscanf(val, "%x,%x,%x,%x,%x,%x,%x,%x",
				      &cid[0], &cid[1], &cid[2], &cid[3],
				      &cid[4], &cid[5], &cid[6], &cid[7]);
			if (cids < 1)
				break;
			for (i = 0; i < cids; i++) 
				if (cid[i] == ddf->id.device)
					break;
			if (i == cids) {
				printf("%s: no compatible id\n", fn);
				ret = -2; /* no compatible ID */
				goto out;
			}
		} else if (!strcasecmp(key, "Version")) {
			sscanf(val, "%x", &version);
		} else if (!strcasecmp(key, "Length")) {
			sscanf(val, "%u", &length);
		} 
	}
	p++;
	*fw_off = p;
	printf("devid = %04x\n", devid);
	printf("version = %08x  %08x\n", version, ddf->id.hw);
	printf("length = %u\n", length);
	printf("fsize = %u, p = %u, f-p = %u\n", fsize, p, fsize - p);
	if (devid == ddf->id.device) {
		if (version <= (ddf->id.hw & 0xffffff)) {
			printf("%s: old version\n", fn);
			ret = -3; /* same id but no newer version */
		}
	} else
		ret = 1;

out:
	free(buf);
	printf("check_fw = %d\n", ret);
	return ret;
	
}

static int update_image(struct ddflash *ddf, char *fn, 
			uint32_t adr, uint32_t len,
			int has_header, int no_change)
{
	int fs, res = 0;
	uint32_t fw_off = 0;

	printf("Check %s\n", fn);
	if (has_header) {
		int ck;
		
		ck = check_fw(ddf, fn, &fw_off);
		if (ck < 0)
			return ck;
		if (ck == 1 && no_change)
			return 0;
	}
	fs = open(fn, O_RDONLY);
	if (fs < 0 ) {
		printf("File %s not found \n", fn);
		return -1;
	}
	res = flashcmp(ddf, fs, adr, len, fw_off);
	if (res == -2) {
		printf("%s: same as flash\n", fn);
	}
	if (res < 0) 
		goto out;
	res = flashwrite(ddf, fs, adr, len, fw_off);
	if (res == 0)
		res = 1;
out:
	close(fs);
	return res;
}


static int fexists(char *fn)
{
	struct stat b;

	return (!stat(fn, &b));
}

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
			if ((res = update_image(ddf, "/config/fpga.img", 0x10000, 0xa0000, 1, 1)) == 1)
				stat |= 1;
			if (res == -1)
				if ((res = update_image(ddf, "/boot/fpga.img", 0x10000, 0xa0000, 1, 1)) == 1)
					stat |= 1;
			if (res == -1)
				if ((res = update_image(ddf, "/config/fpga_gtl.img", 0x10000, 0xa0000, 1, 1)) == 1)
					stat |= 1;
			if (res == -1)
				if ((res = update_image(ddf, "/boot/fpga_gtl.img", 0x10000, 0xa0000, 1, 1)) == 1)
					stat |= 1;
		}
		break;
	case 0x320:
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
