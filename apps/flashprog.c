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

#define DDB_MAGIC 'd'

static uint32_t linknr = 0;

struct ddb_id {
	__u16 vendor;
	__u16 device;
	__u16 subvendor;
	__u16 subdevice;
	__u32 hw;
	__u32 regmap;
};

struct ddb_flashio {
	__u8 *write_buf;
	__u32 write_len;
	__u8 *read_buf;
	__u32 read_len;
	__u32 link;
};

#define IOCTL_DDB_FLASHIO  _IOWR(DDB_MAGIC, 0x00, struct ddb_flashio)
#define IOCTL_DDB_ID       _IOR(DDB_MAGIC, 0x03, struct ddb_id)


int flashio(int ddb, uint8_t *wbuf, uint32_t wlen, uint8_t *rbuf, uint32_t rlen)
{
	struct ddb_flashio fio = {
		.write_buf=wbuf,
		.write_len=wlen,
		.read_buf=rbuf,
		.read_len=rlen,
		.link=linknr,
	};
	
	return ioctl(ddb, IOCTL_DDB_FLASHIO, &fio);
}

enum {
	UNKNOWN_FLASH = 0,
	ATMEL_AT45DB642D = 1,
	SSTI_SST25VF016B = 2,
	SSTI_SST25VF032B = 3,
	SSTI_SST25VF064C = 4,
	SPANSION_S25FL116K = 5,
};


int flashread(int ddb, uint8_t *buf, uint32_t addr, uint32_t len)
{
	uint8_t cmd[4]= {0x03, (addr >> 16) & 0xff, 
			 (addr >> 8) & 0xff, addr & 0xff};
	
	return flashio(ddb, cmd, 4, buf, len);
}

int flashdump(int ddb, uint32_t addr, uint32_t len)
{
	int i, j;
	uint8_t buf[32];
	int bl = sizeof(buf);
	
	for (j=0; j<len; j+=bl, addr+=bl) {
		flashread(ddb, buf, addr, bl);
		for (i=0; i<bl; i++) {
			printf("%02x ", buf[i]);
		}
		printf("\n");
	}
}


int FlashDetect(int dev)
{
	uint8_t Cmd = 0x9F;
	uint8_t Id[3];
	
	int r = flashio(dev, &Cmd,1,Id,3);
	if (r < 0)
		return r;
	
	if (Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x41 )
		r = SSTI_SST25VF016B; 
	else if( Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x4A ) 
		r = SSTI_SST25VF032B; 
	else if( Id[0] == 0x1F && Id[1] == 0x28 )
		r = ATMEL_AT45DB642D; 
	else if( Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x4B )
		r = SSTI_SST25VF064C; 
	else if( Id[0] == 0x01 && Id[1] == 0x40 && Id[2] == 0x15 )
		r = SPANSION_S25FL116K; 
	else 
		r = UNKNOWN_FLASH;
	
	switch(r) {
        case UNKNOWN_FLASH: 
		printf("Unknown Flash Flash ID = %02x %02x %02x\n",Id[0],Id[1],Id[2]);
		break;
        case ATMEL_AT45DB642D:
		printf("Flash: Atmel AT45DB642D  64 MBit\n");
		break;
        case SSTI_SST25VF016B:
		printf("Flash: SSTI  SST25VF016B 16 MBit\n");
		break;
        case SSTI_SST25VF032B:
		printf("Flash: SSTI  SST25VF032B 32 MBit\n"); break;
        case SSTI_SST25VF064C:
		printf("Flash: SSTI  SST25VF064C 64 MBit\n"); break;
        case SPANSION_S25FL116K:
		printf("Flash: SPANSION S25FL116K 16 MBit\n"); break;
	}
	return r;
}


int FlashWriteAtmel(int dev,uint32_t FlashOffset, uint8_t *Buffer,int BufferSize)
{
    int err = 0;
    int BlockErase = BufferSize >= 8192;
    int i;
    
    if (BlockErase) {
	    for(i = 0; i < BufferSize; i += 8192 ) {
		    uint8_t Cmd[4];
		    if( (i & 0xFFFF) == 0 )
			    printf(" Erase    %08x\n",FlashOffset + i);
		    Cmd[0] = 0x50; // Block Erase
		    Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		    Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		    Cmd[3] = 0x00;
		    err = flashio(dev,Cmd,4,NULL,0);
		    if( err < 0 ) break;
		    
		    while( 1 )
		    {
			    Cmd[0] = 0xD7;  // Read Status register
			    err = flashio(dev,Cmd,1,&Cmd[0],1);
			    if( err < 0 ) break;
			    if( (Cmd[0] & 0x80) == 0x80 ) break;
		    }
	    }
    }
    
    for(i = 0; i < BufferSize; i += 1024 )
    {
        uint8_t Cmd[4 + 1024];
        if( (i & 0xFFFF) == 0 )
        {
            printf(" Program  %08x\n",FlashOffset + i);
        }
        Cmd[0] = 0x84; // Buffer 1
        Cmd[1] = 0x00;
        Cmd[2] = 0x00;
        Cmd[3] = 0x00;
        memcpy(&Cmd[4],&Buffer[i],1024);

        err = flashio(dev,Cmd,4 + 1024,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = BlockErase ? 0x88 : 0x83; // Buffer to Main Memory (with Erase)
        Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
        Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
        Cmd[3] = 0x00;

        err = flashio(dev,Cmd,4,NULL,0);
        if( err < 0 ) break;

        while( 1 )
        {
		Cmd[0] = 0xD7;  // Read Status register
		err = flashio(dev,Cmd,1,&Cmd[0],1);
            if( err < 0 ) break;
            if( (Cmd[0] & 0x80) == 0x80 ) break;
        }
        if( err < 0 ) break;
    }
    return err;
}


int FlashWritePageMode(int dev, uint32_t FlashOffset, uint8_t *Buffer, int BufferSize, uint8_t LockBits)
{
	int err = 0, i, j;
	uint8_t Cmd[260];
	
	if( (BufferSize % 4096) != 0 )
		return -1;   // Must be multiple of sector size
	
	do {
		Cmd[0] = 0x50;  // EWSR
		err = flashio(dev, Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = flashio(dev, Cmd,2,NULL,0);
		if( err < 0 ) break;
		
		for(i = 0; i < BufferSize; i += 4096 ) {
			if( (i & 0xFFFF) == 0 )	{
				printf(" Erase    %08x\n",FlashOffset + i);
			}
			
			Cmd[0] = 0x06;  // WREN
			err = flashio(dev, Cmd,1,NULL,0);
			if( err < 0 ) break;
			
			Cmd[0] = 0x20;  // Sector erase ( 4Kb)
			Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
			Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
			Cmd[3] = 0x00;
			err = flashio(dev, Cmd,4,NULL,0);
			if( err < 0 ) break;
			
			while(1)
			{
				Cmd[0] = 0x05;  // RDRS
				err = flashio(dev, Cmd,1,&Cmd[0],1);
				if( err < 0 ) break;
				if( (Cmd[0] & 0x01) == 0 ) break;
			}
			if( err < 0 ) break;
			
		}
		if( err < 0 ) break;
		
		
		for (j = BufferSize - 256; j >= 0; j -= 256 )
		{
			if( (j & 0xFFFF) == 0 )
			{
				printf(" Programm %08x\n",FlashOffset + j);
			}
			
			Cmd[0] = 0x06;  // WREN
			err = flashio(dev, Cmd,1,NULL,0);
			if( err < 0 ) break;
			
			Cmd[0] = 0x02;  // PP
			Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
			Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
			Cmd[3] = 0x00;
			memcpy(&Cmd[4],&Buffer[j],256);
			err = flashio(dev, Cmd,260,NULL,0);
			if( err < 0 ) break;
			
			while(1)
			{
				Cmd[0] = 0x05;  // RDRS
				err = flashio(dev, Cmd,1,&Cmd[0],1);
				if( err < 0 ) break;
				if( (Cmd[0] & 0x01) == 0 ) break;
			}
			if( err < 0 ) break;
			
		}
		if( err < 0 ) break;
		
		Cmd[0] = 0x50;  // EWSR
		err = flashio(dev, Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = LockBits;  // BPx = 0, Lock all blocks
		err = flashio(dev, Cmd,2,NULL,0);
		
	} while(0);
	return err;
}


int FlashWriteSSTI_B(int dev, uint32_t FlashOffset, uint8_t *Buffer, int BufferSize)
{
    int err = 0;
    uint8_t Cmd[6];
    int i, j;

    // Must be multiple of sector size
    if( (BufferSize % 4096) != 0 ) 
	    return -1;   
    
    do {
	    Cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,Cmd,1,NULL,0);
	    if( err < 0 ) 
		    break;

	    Cmd[0] = 0x01;  // WRSR
	    Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(dev,Cmd,2,NULL,0);
	    if( err < 0 )
		    break;
	    
	    for(i = 0; i < BufferSize; i += 4096 ) {
		    if( (i & 0xFFFF) == 0 )
			    printf(" Erase    %08x\n",FlashOffset + i);
		    Cmd[0] = 0x06;  // WREN
		    err = flashio(dev,Cmd,1,NULL,0);
		    if( err < 0 )
			    break;
		    
		    Cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		    Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		    Cmd[3] = 0x00;
		    err = flashio(dev,Cmd,4,NULL,0);
		    if( err < 0 )
			    break;
		    
		    while(1) {
			    Cmd[0] = 0x05;  // RDRS
			    err = flashio(dev,Cmd,1,&Cmd[0],1);
			    if( err < 0 ) break;
			    if( (Cmd[0] & 0x01) == 0 ) break;
		    }
		    if( err < 0 ) break;
	    }
	    if( err < 0 ) 
		    break;
	    for(j = BufferSize - 4096; j >= 0; j -= 4096 ) {
		    if( (j & 0xFFFF) == 0 )
			    printf(" Program  %08x\n",FlashOffset + j);
		    
		    for(i = 0; i < 4096; i += 2 ) {
			    if( i == 0 ) {
				    Cmd[0] = 0x06;  // WREN
				    err = flashio(dev,Cmd,1,NULL,0);
				    if( err < 0 ) 
					    break;
				    
				    Cmd[0] = 0xAD;  // AAI
				    Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
				    Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
				    Cmd[3] = 0x00;
				    Cmd[4] = Buffer[j+i];
				    Cmd[5] = Buffer[j+i+1];
				    err = flashio(dev,Cmd,6,NULL,0);
			    } else {
				    Cmd[0] = 0xAD;  // AAI
				    Cmd[1] = Buffer[j+i];
				    Cmd[2] = Buffer[j+i+1];
				    err = flashio(dev,Cmd,3,NULL,0);
			    }
			    if( err < 0 ) 
				    break;
			    
			    while(1) {
				    Cmd[0] = 0x05;  // RDRS
				    err = flashio(dev,Cmd,1,&Cmd[0],1);
				    if( err < 0 ) break;
				    if( (Cmd[0] & 0x01) == 0 ) break;
			    }
			    if( err < 0 ) break;
		    }
		    if( err < 0 ) break;
		    
		    Cmd[0] = 0x04;  // WDIS
		    err = flashio(dev,Cmd,1,NULL,0);
		    if( err < 0 ) break;
		    
	    }
	    if( err < 0 ) break;
	    
	    Cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,Cmd,1,NULL,0);
	    if( err < 0 ) break;
	    
	    Cmd[0] = 0x01;  // WRSR
	    Cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(dev,Cmd,2,NULL,0);
    } while(0);
    return err;
}

void get_id(int ddb, struct ddb_id *ddbid) {
	uint8_t id[4];

	if (ioctl(ddb, IOCTL_DDB_ID, ddbid)>=0)
		return;
	memset(ddbid, 0, sizeof(*ddbid));
	flashread(ddb, id, 0, 4);
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
	int type = 0;
	struct ddb_id ddbid;
	uint8_t *buffer;
	int BufferSize = 0;
	int BlockErase = 0;
	uint32_t FlashOffset = 0x10000;
	int ddb;
	int i, err;
	int SectorSize=0;
	int FlashSize=0;
	int Flash;

	uint32_t svid=0, jump=0, dump=0;
	int bin;

	int ddbnum = 0;
	int force = 0;

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
				"d:n:s:o:l:dfhj",
				long_options, &option_index);
		if (c==-1)
			break;

		switch (c) {
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
	Flash=FlashDetect(ddb);

	switch(Flash) {
        case ATMEL_AT45DB642D: 
		SectorSize = 1024; 
		FlashSize = 0x800000; 
		break;
        case SSTI_SST25VF016B: 
		SectorSize = 4096; 
		FlashSize = 0x200000; 
		break;
        case SSTI_SST25VF032B: 
		SectorSize = 4096; 
		FlashSize = 0x400000; 
		break;
        case SSTI_SST25VF064C:
		SectorSize = 4096;
		FlashSize = 0x800000;
		break;
        case SPANSION_S25FL116K:
		SectorSize = 4096;
		FlashSize = 0x200000;
		break;
	default:
		return 0;
	}

	get_id(ddb, &ddbid);
#if 1
	printf("%04x %04x %04x %04x %08x %08x\n",
	       ddbid.vendor, ddbid.device,
	       ddbid.subvendor, ddbid.subdevice,
	       ddbid.hw, ddbid.regmap);
#endif

	if (dump) {
		flashdump(ddb, dump, 128);
		return 0;
	}

	if (ddbid.device == 0x0011)
		type = 1;
	if (ddbid.device == 0x0201)
		type = 2;
	if (ddbid.device == 0x02)
		type = 3;
	if (ddbid.device == 0x03)
		type = 0;
	if (ddbid.device == 0x07)
		type = 4;
	if (ddbid.device == 0x320)
		type = 5;
	if (ddbid.device == 0x13)
		type = 6;
	if (ddbid.device == 0x12)
		type = 7;
	
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
		char *fname;

		switch (type) {
		case 0:
			fname="DVBBridgeV1B_DVBBridgeV1B.bit";
			printf("Octopus\n");
			break;
		case 1:
			fname="CIBridgeV1B_CIBridgeV1B.bit";
			printf("Octopus CI\n");
			break;
		case 2:
			fname="DVBModulatorV1B_DVBModulatorV1B.bit";
			printf("Modulator\n");
			break;
		case 3:
			fname="DVBBridgeV1A_DVBBridgeV1A.bit";
			printf("Octopus 35\n");
			break;
		case 4:
			fname="DVBBridgeV2A_DD01_0007_MXL.bit";
			printf("Octopus 4/8\n");
			break;
		case 6:
			fname="DVBBridgeV2B_DD01_0013_PRO.fpga";
			printf("Octopus PRO\n");
			break;
		case 7:
			fname="DVBBridgeV2B_DD01_0012_STD.fpga";
			printf("Octopus CI\n");
			break;
		default:
			printf("UNKNOWN\n");
			break;
		}
		fh = open(fname, O_RDONLY);
		if (fh < 0 ) {
			printf("File not found \n");
			return 0;
		}
		printf("Using bitstream %s\n", fname);

		fsize = lseek(fh,0,SEEK_END);
		if( fsize > 4000000 || fsize < SectorSize )
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
