/*
/* flashprog - Programmer for flash on Digital Devices Octopus 
 *
 * Copyright (C) 2010-2011 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <errno.h>

#include "flash.h"
#include "flash.c"

void get_ddid(int ddb, struct ddb_id *ddbid) {
	uint8_t id[4];

	if (ioctl(ddb, IOCTL_DDB_ID, ddbid)>=0)
		return;
	memset(ddbid, 0, sizeof(*ddbid));
	flashread(ddb, linknr, id, 0, 4);
	printf("%02x %02x %02x %02x\n", 
	       id[0], id[1], id[2], id[3]);
	ddbid->subvendor=(id[0] << 8) | id[1];
	ddbid->subdevice=(id[2] << 8) | id[3];
}

int sure()
{
	char c;

	printf("\n\nWARNING! Flashing a new FPGA image might make your card unusable!\n");
	printf("\n\nWARNUNG! Das Flashen eines neuen FPGA-Images kann Ihre Karte unbrauchbar machen.\n");
	printf("\n\nAre you sure? y/n?");
	printf("\n\nSind Sie sicher? y/n?");
	fflush(0);
	c = getchar();
	if (c!='y') {
		printf("\nFlashing aborted.\n\n");
		return -1;
	}
	printf("\nStarting to flash\n\n");
	return 0;
}


int main(int argc, char **argv)
{
	char ddbname[80];
	char *flashname;
	int type = 0;
	struct ddb_id ddbid;
	uint8_t *buffer;
	int BufferSize = 0;
	int BlockErase = 0;
	uint32_t FlashOffset = 0x10000;
	int ddb;
	int i, err;
	uint32_t SectorSize=0;
	uint32_t FlashSize=0;
	int Flash;

	uint32_t svid=0, jump=0, dump=0;
	int bin;

	int ddbnum = 0;
	int force = 0;
	char *fname = NULL;

        while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"svid", required_argument, NULL, 's'},
			{"help", no_argument , NULL, 'h'},
			{"force", no_argument , NULL, 'f'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"d:n:s:o:l:dfhjb:",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
		case 'b':
			fname = optarg;
			break;
		case 'd':
			dump = strtoul(optarg, NULL, 16);
			break;
		case 's':
			svid = strtoul(optarg, NULL, 16);
			break;
		case 'o':
			FlashOffset = strtoul(optarg, NULL, 16);
			break;
		case 'n':
			ddbnum = strtol(optarg, NULL, 0);
			break;
		case 'l':
			linknr = strtol(optarg, NULL, 0);
			break;
		case 'f':
			force = 1;
			break;
		case 'j':
			jump = 1;
			break;
		case 'h':
		default:
			break;

		}
	}
	if (optind<argc) {
		printf("Warning: unused arguments\n");
	}

	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	ddb=open(ddbname, O_RDWR);
	if (ddb < 0) {
		printf("Could not open device\n");
		return -1;
	}
	Flash = flashdetect(ddb, &SectorSize, &FlashSize, &flashname);

	get_ddid(ddb, &ddbid);
#if 0
	printf("%04x %04x %04x %04x %08x %08x\n",
	       ddbid.vendor, ddbid.device,
	       ddbid.subvendor, ddbid.subdevice,
	       ddbid.hw, ddbid.regmap);
#endif

	if (dump) {
		flashdump(ddb, linknr, dump, 128);
		return 0;
	}

	if (!SectorSize)
		return 0;
	
	if (jump) {
		uint32_t Jump = 0x200000;
		
		BufferSize = SectorSize;
		FlashOffset = FlashSize - SectorSize;
		buffer = malloc(BufferSize);
		if (!buffer) {
			printf("out of memory\n");
			return 0;
		}
		memset(buffer, 0xFF, BufferSize);
		memset(&buffer[BufferSize - 256 + 0x10], 0x00, 16);

		buffer[BufferSize - 256 + 0x10] = 0xbd;
		buffer[BufferSize - 256 + 0x11] = 0xb3;
		buffer[BufferSize - 256 + 0x12] = 0xc4;
		buffer[BufferSize - 256 + 0x1a] = 0xfe;
		buffer[BufferSize - 256 + 0x1e] = 0x03;
		buffer[BufferSize - 256 + 0x1f] = ( ( Jump >> 16 ) & 0xFF );
		buffer[BufferSize - 256 + 0x20] = ( ( Jump >>  8 ) & 0xFF );
		buffer[BufferSize - 256 + 0x21] = ( ( Jump       ) & 0xFF );
	} else if (svid) {
		BufferSize = SectorSize;
		FlashOffset = 0;
		
		buffer = malloc(BufferSize);
		if (!buffer) {
			printf("out of memory\n");
			return 0;
		}
		memset(buffer,0xFF,BufferSize);
		
		buffer[0] = ((svid >> 24 ) & 0xFF);
		buffer[1] = ((svid >> 16 ) & 0xFF);
		buffer[2] = ((svid >>  8 ) & 0xFF);
		buffer[3] = ((svid       ) & 0xFF);
	} else {
		int fh, i;
		int fsize;
		char *name;

		if (!fname) 
			fname = devid2fname(ddbid.device, &name);
		if (name)
			printf("Card: %s\n", name);
		
		fh = open(fname, O_RDONLY);
		if (fh < 0 ) {
			printf("File %s not found \n", fname);
			return 0;
		}
		printf("Using bitstream %s\n", fname);

		fsize = lseek(fh,0,SEEK_END);
		if( fsize > FlashSize/2 - 0x10000 || fsize < SectorSize )
		{
			close(fh);
			printf("Invalid File Size \n");
			return 0;
		}
		
		if( Flash == ATMEL_AT45DB642D ) {
			BlockErase = fsize >= 8192;
			if( BlockErase )
				BufferSize = (fsize + 8191) & ~8191;
			else
				BufferSize = (fsize + 1023) & ~1023;
		} else {
			BufferSize = (fsize + SectorSize - 1 ) & ~(SectorSize - 1);
		}
		printf(" Size     %08x, target %08x\n", BufferSize, FlashOffset);
		
		buffer = malloc(BufferSize);

		if( buffer == NULL ) {
			close(fh);
			printf("out of memory\n");
			return 0;
		}
	
		memset(buffer, 0xFF, BufferSize);
		lseek(fh, 0, SEEK_SET);
		read(fh, buffer, fsize);
		close(fh);
		
		if (BufferSize >= 0x10000) {
			for(i = 0; i < 0x200; i += 1 ) {
				if ( *(uint16_t *) (&buffer[i]) == 0xFFFF )
					break;
				buffer[i] = 0xFF;
			}
		}
	}
	if (!force && sure()<0)
		return 0;
	switch(Flash) {
        case ATMEL_AT45DB642D:
		err = FlashWriteAtmel(ddb,FlashOffset,buffer,BufferSize);
		break;
        case SSTI_SST25VF016B: 
        case SSTI_SST25VF032B:
		err = FlashWriteSSTI_B(ddb,FlashOffset,buffer,BufferSize);
		break;
        case SSTI_SST25VF064C:
		err = FlashWritePageMode(ddb,FlashOffset,buffer,BufferSize,0x3C);
		break;
        case SPANSION_S25FL116K:
        case SPANSION_S25FL164K:
	case WINBOND_W25Q16JV:
	case WINBOND_W25Q32JV:
		err = FlashWritePageMode(ddb,FlashOffset,buffer,BufferSize,0x1C);
		break;            
	}
	
	if (err < 0) 
		printf("Programming Error\n");
	else
		printf("Programming Done\n");
	
	free(buffer);
	return 0;
}
