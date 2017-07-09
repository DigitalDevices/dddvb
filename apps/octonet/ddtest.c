#include <ctype.h>
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

#include "flash.h"

typedef int (*COMMAND_FUNCTION)(int dev, int argc, char* argv[], uint32_t Flags);

enum {
	REPEAT_FLAG = 0x00000001,
	SILENT_FLAG = 0x00000002,
};

struct SCommand
{
	char*                Name;
	COMMAND_FUNCTION     Function;
	int                  Open;
	char*                Help;
};

// --------------------------------------------------------------------------------------------


int ReadFlash(int ddb, int argc, char *argv[], uint32_t Flags)
{
	uint32_t Start;
	uint32_t Len;
	uint8_t *Buffer;
	int fd;

	if (argc < 2 ) 
		return -1;
	Start = strtoul(argv[0],NULL,16);
	Len   = strtoul(argv[1],NULL,16);
	if (argc == 3) {
		fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
		if (fd < 0) {
			printf("Could not open file %s\n", argv[2]);
			return -1;
		}
	}
	
	Buffer = malloc(Len);
	if (flashread(ddb, Buffer, Start, Len) < 0) {
		printf("flashread error\n");
		free(Buffer);
		return 0;
	}
	
	if (argc == 3) {
		write(fd, Buffer, Len);
		close(fd);
	} else
		Dump(Buffer,Start,Len);
	
	free(Buffer);
	return 0;
}


int ReadSave(int ddb, int argc, char *argv[], uint32_t Flags)
{
	uint32_t Start;
	uint32_t Len;
	uint8_t *Buffer;
	int fd;

	if (argc < 3 ) 
		return -1;
	Start = strtoul(argv[0],NULL,16);
	Len   = strtoul(argv[1],NULL,16);
		
	Buffer = malloc(Len);
	if (flashread(ddb, Buffer, Start, Len) < 0) {
		printf("flashread error\n");
		free(Buffer);
		return 0;
	}

	
		
	free(Buffer);
	return 0;
}


int FlashChipEraseAtmel(int dev)
{
	int err = 0;
	// Note Sector 0 is in 2 parts
	int i = 0;
	while(i < 0x800000)
	{
		uint8_t Cmd[4];

		printf(" Erase    %08x\n",i);
		Cmd[0] = 0x7C; // Sector Erase
		Cmd[1] = ( (( i ) >> 16) & 0xFF );
		Cmd[2] = ( (( i ) >>  8) & 0xFF );
		Cmd[3] = 0x00;
		err = flashio(dev,Cmd,4,NULL,0);
		if( err < 0 ) 
			break;
		while (1) {
			Cmd[0] = 0xD7;  // Read Status register
			err = flashio(dev,Cmd,1,&Cmd[0],1);
			if( err < 0 ) break;
			if( (Cmd[0] & 0x80) == 0x80 ) break;
		}
		if( i == 0 ) i = 0x2000;
		else if( i == 0x2000 ) i = 0x40000;
		else i += 0x40000;
	}
	return 0;
}

int FlashChipEraseSSTI(int dev)
{
	int err = 0;
	uint8_t Cmd[4];
	
	do {
		Cmd[0] = 0x50;  // EWSR
		err = flashio(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = flashio(dev,Cmd,2,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x06;  // WREN
		err = flashio(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x60;  // CHIP Erase
		err = flashio(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		while(1) {
			Cmd[0] = 0x05;  // RDRS
			err = flashio(dev,Cmd,1,&Cmd[0],1);
			if( err < 0 ) break;
			if( (Cmd[0] & 0x01) == 0 ) break;
		}
		if ( err < 0 )
			break;
		
		Cmd[0] = 0x50;  // EWSR
		err = flashio(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
		err = flashio(dev,Cmd,2,NULL,0);
	}
	while(0);
	
	if( err >= 0 ) printf("Flash erase succeeded\n");
	else           printf("Flash erase failed\n");
	return 0;
}



int ReadDeviceMemory(int dev,int argc, char* argv[],uint32_t Flags)
{
	uint32_t Start;
	uint32_t Len;
	uint8_t * Buffer;

	if( argc < 2 ) return -1;

	Start = strtoul(argv[0],NULL,16);
	Len   = strtoul(argv[1],NULL,16);
	//if( Start > 0xFFFF || Start + Len > 0x10000 || Len == 0 ) return -1;
	Buffer = malloc(Len);

	{	
		struct ddb_mem mem = {.off=Start, .len=Len, .buf=Buffer };
		ioctl(dev, IOCTL_DDB_READ_MEM, &mem);
	}
	Dump(Buffer,Start,Len);
	free(Buffer);
	return 0;
}

int WriteDeviceMemory(int dev,int argc, char* argv[],uint32_t Flags)
{
	uint8_t * Buffer;
	uint32_t Start, Len, i;

	if( argc < 2 ) return -1;
	Start = strtoul(argv[0],NULL,16);
	Len = argc - 1;
	//if( Start > 0xFFFF || Start + Len > 0x10000 || Len == 0 ) return -1;
	Buffer = malloc(Len + sizeof(uint32_t));
	if( Buffer == NULL ) 
		return -2;
	
	*((uint32_t *)Buffer) = Start;
	for (i = 0; i < Len; i += 1 )
		Buffer[i+sizeof(uint32_t)] = (uint8_t) strtoul(argv[i+1],NULL,16);
	

	{	
		struct ddb_mem mem = {.off=Start, .len=Len, .buf=Buffer+4 };
		ioctl(dev, IOCTL_DDB_WRITE_MEM, &mem);
	}
	free(Buffer);
	return 0;
}

int FillDeviceMemory(int dev,int argc, char* argv[],uint32_t Flags)
{
	uint32_t Start, Len;
	uint8_t * Buffer;
	uint8_t Value = 0;

	if( argc < 2 ) return -1;
	Start = strtoul(argv[0],NULL,16);
	Len   = strtoul(argv[1],NULL,16);
	
	if (Start > 0xFFFF || Start + Len > 0x10000 || Len == 0 )
		return -1;
	
	Buffer = malloc(Len);
	if (Buffer == NULL)
		return -2;
	
	if(argc > 2)
		Value = (uint8_t) strtoul(argv[2],NULL,16);
	memset(Buffer, Value, Len);
	{	
		struct ddb_mem mem = {.off=Start, .len=Len, .buf=Buffer };
		ioctl(dev, IOCTL_DDB_WRITE_MEM, &mem);
	}
	free(Buffer);
	return 0;
}

int GetSetRegister(int dev,int argc, char* argv[],uint32_t Flags)
{
	uint32_t i;

    if( argc < 1 ) return -1;

    uint32_t Reg[2];
    char* p;
    Reg[0] = strtoul(argv[0],&p,16);// & 0xFFFC;
    uint32_t LastReg = Reg[0];

    //if( Reg[0] >= 0x10000 ) return -1;

    if( argc == 1 )
    {
        if( *p == '-' )
        {
		LastReg = strtoul(&p[1],NULL,16);// & 0xFFFC;
        }
        else if( *p == '+' )
        {
            LastReg = Reg[0] + (strtoul(&p[1],NULL,16) - 1) * 4;
        }
    }

    uint32_t NumRegs = (LastReg - Reg[0]) / 4 + 1;

    //   if( LastReg >= 0x10000 || LastReg < Reg[0] ) return -1;

    if( argc > 1 )
    {
        Reg[1] = strtoul(argv[1],NULL,0);
        if( writereg(dev,Reg[0],Reg[1]) != 0 )
        {
            return -2;
        }
    }
    else
    {
        for(i = 0; i < NumRegs; i += 1 )
        {
            if (readreg(dev,Reg[0],&Reg[1]) < 0 )
            {
                return -2;
            }
            printf(" Register %08X = %08X (%d)\n",Reg[0],Reg[1],Reg[1]);
            Reg[0] += 4;
        }
    }

    return 0;
}


// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

int flashioc(int dev,int argc, char* argv[],uint32_t Flags)
{
    uint8_t *Buffer;
    uint32_t tmp = 0, i;
    uint32_t WriteLen = (argc-1);
    uint32_t BufferLength = WriteLen;
    uint32_t ReadLen;

    if( argc < 2 ) return -1;

    ReadLen = strtoul(argv[argc-1],NULL,0);
    if( ReadLen > BufferLength ) BufferLength = ReadLen ;

    Buffer = malloc(WriteLen);

    for(i = 0; i < (argc-1); i += 1 )
    {
        tmp = strtoul(argv[i],NULL,16);
        if( tmp > 255 )
        {
            return -1;
        }
        Buffer[i] = (uint8_t) tmp;
    }

    if( flashio(dev,Buffer,WriteLen,Buffer,ReadLen) < 0 )
    {
        return 0;
    }

    if( ReadLen > 0 )
	    Dump(Buffer,0,ReadLen);

    return 0;

}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

// Searach and return FPGA ID from Buffer, buffer size must be 64 kByte or larger
uint32_t   GetFPGA_ID(uint8_t * Buffer)
{
    uint32_t ID = 0xFFFFFF;
    int Len = 0x10000 - 16;
    while( Len > 0 )
    {
        if( Buffer[0] == 0xBD && Buffer[1] == 0xB3 ) 
        {
		ID = ((Buffer[6]) << 24) | ((Buffer[7]) << 16) | ((Buffer[8]) << 8) | ((Buffer[9]));
            break;
        }
        if( Buffer[0] == 0xBC && Buffer[1] == 0xB3 )
        {
            // Proteced bitstream.
            ID =  ((Buffer[2]^Buffer[6]^Buffer[10]^Buffer[14]) << 24) 
                | ((Buffer[3]^Buffer[7]^Buffer[11]^Buffer[15]) << 16) 
                | ((Buffer[4]^Buffer[8]^Buffer[12]^Buffer[16]) << 8) 
                | ((Buffer[5]^Buffer[9]^Buffer[13]^Buffer[17]));
            break;
        }

        Len -= 1;
        Buffer += 1;
    }

    return ID;
}


int FlashProg(int dev,int argc, char* argv[],uint32_t Flags)
{
	uint8_t * Buffer = NULL;
	int BufferSize = 0;
	int BlockErase = 0;
	uint32_t FlashOffset = 0x10000;
	int SectorSize = 0;
	int FlashSize = 0;
	int ValidateFPGAType = 1;
	int Flash;
	uint32_t Id1, Id2;
	
	if( argc < 1 ) 
		return -1;
	Flash = FlashDetect(dev);
	switch(Flash)
	{
        case ATMEL_AT45DB642D: SectorSize = 1024; FlashSize = 0x800000; break;
        case SSTI_SST25VF016B: SectorSize = 4096; FlashSize = 0x200000; break;
        case SSTI_SST25VF032B: SectorSize = 4096; FlashSize = 0x400000; break;
        case SSTI_SST25VF064C: SectorSize = 4096; FlashSize = 0x800000; break;
        case SPANSION_S25FL116K: SectorSize = 4096; FlashSize = 0x200000; break;
        case SPANSION_S25FL132K: SectorSize = 4096; FlashSize = 0x400000; break;
        case SPANSION_S25FL164K: SectorSize = 4096; FlashSize = 0x800000; break;
	}
	if (SectorSize == 0) 
		return 0;
	
	if( strncasecmp("-SubVendorID",argv[0],strlen(argv[0])) == 0 )
	{
		if( argc < 2 ) return -1;
		
		uint32_t SubVendorID = strtoul(argv[1],NULL,16);
		
		BufferSize = SectorSize;
		FlashOffset = 0;
		
		Buffer = malloc(BufferSize);
		if( Buffer == NULL )
		{
			printf("out of memory\n");
			return 0;
		}
		memset(Buffer,0xFF,BufferSize);
		
		Buffer[0] = ( ( SubVendorID >> 24 ) & 0xFF );
		Buffer[1] = ( ( SubVendorID >> 16 ) & 0xFF );
		Buffer[2] = ( ( SubVendorID >>  8 ) & 0xFF );
		Buffer[3] = ( ( SubVendorID       ) & 0xFF );
		
	}
	else if( strncasecmp("-Jump",argv[0],strlen(argv[0])) == 0 )
	{
		uint32_t Jump;
		if( argc < 2 ) return -1;
		
		Jump = strtoul(argv[1],NULL,16);
		
		BufferSize = SectorSize;
		FlashOffset = FlashSize - SectorSize;
		
		Buffer = malloc(BufferSize);
		if( Buffer == NULL )
		{
			printf("out of memory\n");
			return 0;
		}
		memset(Buffer,0xFF,BufferSize);
		
		memset(&Buffer[BufferSize - 256 + 0x10],0x00,16);
		
		Buffer[BufferSize - 256 + 0x10] = 0xbd;
		Buffer[BufferSize - 256 + 0x11] = 0xb3;
		Buffer[BufferSize - 256 + 0x12] = 0xc4;
		Buffer[BufferSize - 256 + 0x1a] = 0xfe;
		Buffer[BufferSize - 256 + 0x1e] = 0x03;
		Buffer[BufferSize - 256 + 0x1f] = ( ( Jump >> 16 ) & 0xFF );
		Buffer[BufferSize - 256 + 0x20] = ( ( Jump >>  8 ) & 0xFF );
		Buffer[BufferSize - 256 + 0x21] = ( ( Jump       ) & 0xFF );
		
	}
	else
	{
		if( argc > 1 )
		{
			FlashOffset = strtoul(argv[1],NULL,16);
			ValidateFPGAType = 0;   // Don't validate if offset is given
		}
		
		int fh = open(argv[0],O_RDONLY);
		if( fh < 0 )
		{
			printf("File not found \n");
			return 0;
		}
		
		int fsize = lseek(fh,0,SEEK_END);
		
		if( fsize > 4000000 || fsize < SectorSize )
		{
			close(fh);
			printf("Invalid File Size \n");
			return 0;
		}
		
		if( Flash == ATMEL_AT45DB642D )
		{
			BlockErase = fsize >= 8192;
			if( BlockErase )
				BufferSize = (fsize + 8191) & ~8191;
			else
				BufferSize = (fsize + 1023) & ~1023;
		}
		else
		{
			BufferSize = (fsize + SectorSize - 1 ) & ~(SectorSize - 1);
		}
		printf(" Size     %08x\n",BufferSize);
		
		Buffer = malloc(BufferSize);
		if( Buffer == NULL )
		{
			close(fh);
			printf("out of memory\n");
			return 0;
		}
		
		memset(Buffer,0xFF,BufferSize);
		lseek(fh,0,SEEK_SET);
		read(fh,Buffer,fsize);
		close(fh);
		
		if( BufferSize >= 0x10000 )
		{
			int i;
			if (strstr(argv[0],".bit")||strstr(argv[0],".fpga"))
			{
				for(i = 0; i < 0x200; i += 1 )
				{
					if( *(uint16_t *)(&Buffer[i]) == 0xFFFF ) 
						break;
					Buffer[i] = 0xFF;
				}
				// Place our own header
			}
			if( ValidateFPGAType )
			{
				uint8_t * CmpBuffer = malloc(0x10000);
				if( CmpBuffer == NULL )
				{
					free(Buffer); 
					printf("out of memory\n");
					return 0;
				}
				if (flashread(dev, CmpBuffer, FlashOffset, 0x10000)<0) {
					printf("Ioctl returns error\n");
					free(Buffer);
					free(CmpBuffer);
					return 0;
				}
				
				Id1 = GetFPGA_ID(Buffer);
				Id2 = GetFPGA_ID(CmpBuffer);
				
				if (Id2 != 0xFFFFFFFF )
				{
					if( Id1 == 0xFFFFFFFF || Id1 != Id2 )
					{
						printf(" FPGA ID mismatch\n");
						free(Buffer);
						free(CmpBuffer);
						return 0;
					}
				}
			}
			
		}
	}
	
	int err = -1;
	
	switch(Flash)
	{
        case ATMEL_AT45DB642D: 
		err = FlashWriteAtmel(dev,FlashOffset,Buffer,BufferSize); break;
        case SSTI_SST25VF016B: 
        case SSTI_SST25VF032B: 
		err = FlashWriteSSTI(dev,FlashOffset,Buffer,BufferSize); break;
        case SSTI_SST25VF064C:
		err = FlashWritePageMode(dev,FlashOffset,Buffer,BufferSize,0x3C); break;
        case SPANSION_S25FL116K: 
        case SPANSION_S25FL132K: 
        case SPANSION_S25FL164K: 
		err = FlashWritePageMode(dev,FlashOffset,Buffer,BufferSize,0x1C); break;
	}
	
	if( err < 0 ) printf(" Programm Error\n");
	else          printf(" Programm Done\n");
	
	free(Buffer);
	return 0;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

int FlashVerify(int dev,int argc, char* argv[],uint32_t Flags)
{
    if( argc < 1 ) return -1;

    uint8_t * Buffer = NULL;
    uint8_t * Buffer2 = NULL;
    int BufferSize = 0;
    int BlockErase = 0;
    uint32_t FlashOffset = 0x10000;
    int fsize, fh;
    int i;
    int err = 0;

    if( argc > 1 )
    {
	    FlashOffset = strtoul(argv[1],NULL,16);
    }
    
    fh = open(argv[0],O_RDONLY);
    if( fh < 0 )
    {
        printf("File not found \n");
        return 0;
    }

    fsize = lseek(fh,0,SEEK_END);

    if( fsize > 4000000 || fsize < 1024 )
    {
	close(fh);
        printf("Invalid File Size \n");
        return 0;
    }
    BlockErase = fsize >= 8192;

    BufferSize = (fsize + 1023) & ~1023;
    printf(" Size     %08x\n",BufferSize);

    Buffer = malloc(BufferSize);
    if( Buffer == NULL )
    {
        close(fh);
        printf("out of memory\n");
        return 0;
    }

    Buffer2 = malloc(BufferSize);
    if( Buffer2 == NULL )
    {
        close(fh);
        free(Buffer);
        printf("out of memory\n");
        return 0;
    }
    memset(Buffer,0xFF,BufferSize);
    memset(Buffer2,0xFF,BufferSize);
    lseek(fh,0,SEEK_SET);
    read(fh,Buffer,fsize);
    close(fh);

#if 0
    if( BufferSize >= 0x10000 )
    {
	    int i;
        // Clear header
        for(i = 0; i < 0x200; i += 1 )
        {
		if( *(uint16_t *)(&Buffer[i]) == 0xFFFF ) break;
            Buffer[i] = 0xFF;
        }
        // Place our own header
    }
#endif
    if (flashread(dev, Buffer2, FlashOffset, BufferSize)<0) {
	    printf("Ioctl returns error\n");
	    free(Buffer);
	    free(Buffer2);
	    return 0;
    }
    for (i=0; i<BufferSize; i++) {
	    if( Buffer[i] != Buffer2[i] ) {
		    err += 1;
		    //if( err == 1 )
			    printf(" Error at Adress %08x  %02x %02x\n",FlashOffset + i,Buffer[i],Buffer2[i]);
	    }
    }
    printf(" Verify Done %d errors\n",err);
    free(Buffer);
    free(Buffer2);
    return 0;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

int FlashErase(int dev,int argc, char* argv[],uint32_t Flags)
{
    int Flash = FlashDetect(dev);

    switch(Flash)
    {
        case ATMEL_AT45DB642D: return FlashChipEraseAtmel(dev);
        case SSTI_SST25VF016B: 
        case SSTI_SST25VF032B: return FlashChipEraseSSTI(dev);
    }

    return -1;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
#if 0
int FlashTest(int dev,int argc, char* argv[],uint32_t Flags)
{
    writereg(dev,0x10,0x01);
    while(!_kbhit() )
    {
        uint32_t tmp;
        readreg(dev,0x10,&tmp);
        writereg(dev,0x14,0xFF00FF00);
    }
    writereg(dev,0x10,0x00);
    return 0;
}
#endif

int mdio(int dev, int argc, char* argv[], uint32_t Flags)
{
	uint32_t reg, adr, val;

	if( argc < 2 ) 
		return -1;
	adr = strtoul(argv[0], NULL, 16);
	reg = strtoul(argv[1], NULL, 16);
	
	writereg(dev, 0x24, adr);
	writereg(dev, 0x28, reg);
	if(argc > 2) {
		val = strtoul(argv[2], NULL, 16);
		writereg(dev, 0x2c, val);
		writereg(dev, 0x20, 0x03);
		do {
			readreg(dev, 0x20, &val);
		} while (val & 0x02);
	} else {
		writereg(dev, 0x20, 0x07);
		do {
			readreg(dev, 0x20, &val);
		} while (val & 0x02);
		readreg(dev, 0x2c, &val);
		printf("%04x\n", val);
	}
	return 0;
}

int i2c_write(int fd, uint8_t bus, uint8_t adr,
	      uint8_t *hdr, uint32_t hlen, uint8_t *msg, uint32_t mlen)
{
	struct ddb_i2c_msg i2c = {
		.bus = bus,
		.adr = adr / 2,
		.hdr = hdr,
		.hlen = hlen,
		.msg = msg,
		.mlen = mlen,
	};

	return ioctl(fd, IOCTL_DDB_WRITE_I2C, &i2c);
}

int i2c_read(int fd, uint8_t bus, uint8_t adr,
	     uint8_t *hdr, uint32_t hlen, uint8_t *msg, uint32_t mlen)
{
	struct ddb_i2c_msg i2c = {
		.bus = bus,
		.adr = adr / 2,
		.hdr = hdr,
		.hlen = hlen,
		.msg = msg,
		.mlen = mlen,
	};

	return ioctl(fd, IOCTL_DDB_READ_I2C, &i2c);
}

static uint8_t IDCODE_PUB[]        = { 0xE0, 0x00, 0x00, 0x00 };
static uint8_t ISC_ENABLE[]        = { 0xC6, 0x08, 0x00, 0x00 };
static uint8_t ISC_ENABLE_X[]      = { 0x74, 0x08, 0x00, 0x00 };
static uint8_t LSC_INIT_ADDRESS[]  = { 0x46, 0x00, 0x00, 0x00 };
static uint8_t ISC_ERASE[]         = { 0x0E, 0x00, 0x00, 0x00 };
static uint8_t ISC_ERASE_CFG[]     = { 0x0E, 0x04, 0x00, 0x00 };
static uint8_t LSC_READ_STATUS[]   = { 0x3C, 0x00, 0x00, 0x00 };
static uint8_t LSC_CHECK_BUSY[]    = { 0xF0, 0x00, 0x00, 0x00 };
static uint8_t LSC_PROG_INCR_NV[]  = { 0x70, 0x00, 0x00, 0x01 };
static uint8_t ISC_PROGRAM_DONE[]  = { 0x5E, 0x00, 0x00, 0x00 };
static uint8_t LSC_REFRESH[]       = { 0x79, 0x00, 0x00 };
static uint8_t ISC_DISABLE[]       = { 0x26, 0x00, 0x00 };
static uint8_t LSC_PROG_TAG[]      = { 0xC9, 0x00, 0x00, 0x01 };
static uint8_t LSC_INIT_ADDR_UFM[] = { 0x47, 0x00, 0x00, 0x00 };
static uint8_t LSC_WRITE_ADDRESS[] = { 0xB4, 0x00, 0x00, 0x00 };
static uint8_t ISC_ERASE_TAG[]     = { 0xCB, 0x00, 0x00, 0x00 };
static uint8_t ISC_NOOP[]          = { 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t LSC_READ_TAG[]      = { 0xCA, 0x00, 0x00, 0x01 };

int XO2WaitBusy(int dev, uint32_t BusNumber)
{
	int hr = 0;
	uint8_t Status[] = { 0x00,0x00,0x00,0x00 };
	
	while(1) {
		hr = i2c_read(dev, BusNumber,0x88,LSC_READ_STATUS,
			      sizeof(LSC_READ_STATUS),Status,sizeof(Status));
		if (hr)
			return hr;
		if ((Status[2] & 0x10) == 0x00 )
			break;
	}
	usleep(5000);
	hr = i2c_read(dev, BusNumber,0x88,LSC_READ_STATUS,
		      sizeof(LSC_READ_STATUS),Status,sizeof(Status));
	if (hr)
		return hr;
	
	return (Status[2] & 0x20) == 0x00 ? 0 : -1;
}

int XO2WaitDone(int dev, uint32_t BusNumber)
{
	int hr = 0;
	uint8_t Status[] = { 0x00,0x00,0x00,0x00 };
	
	while(1) {
		hr = i2c_read(dev, BusNumber,0x88,
			      LSC_READ_STATUS,sizeof(LSC_READ_STATUS),Status,sizeof(Status));
		if (hr)
			return hr;
		if( (Status[2] & 0x10) == 0x00 )
			break;
	}
	usleep(5000);
	hr = i2c_read(dev, BusNumber,0x88,
		      LSC_READ_STATUS,sizeof(LSC_READ_STATUS),Status,sizeof(Status));
	if( hr)
		return hr;
	
	return (Status[2] & 0x01) == 0x01 ? 0 : -1;
}

int XO2Prog(int dev, int argc, char* argv[], uint32_t Flags)
{
	uint8_t *Buffer = NULL;
	int BufferSize = 0;
	uint32_t BusNumber = 0;
	int fh, fsize, index = 0, hr;
	uint8_t JedecID[4] = { 0,0,0,0 };
	uint8_t DevID[2] = { 0,0 };
	int nCardIDs = 0;
	int CardIDs[8];
	int VerMajor = 0;
	int VerMinor = 0;


	if (argc < 1) 
		return -1;
	if (argc > 1) {
		BusNumber = strtoul(argv[1], NULL, 10);
		if (BusNumber < 1 || BusNumber > 4 )
		{
			printf("Busnumber must be 1-4 \n");
			return 0;
		}
	}
	BusNumber -= 1;
	
	fh = open(argv[0], O_RDONLY);
	if (fh < 0) {
		printf("File not found \n");
		return 0;
	}
	
	fsize = lseek(fh,0,SEEK_END);
	
	if (fsize > 128000 ) {
		close(fh);
		printf("Invalid File Size \n");
		return 0;
	}
	BufferSize = fsize;
	
	printf(" Size     %08x\n",BufferSize);
	
	Buffer = malloc(BufferSize);
	if (Buffer == NULL) {
		close(fh);
		printf("out of memory\n");
		return 0;
	}
	
	memset(Buffer, 0xFF, BufferSize);
	lseek(fh, 0, SEEK_SET);
	read(fh, Buffer, fsize);
	close(fh);
	
	while(index < BufferSize && Buffer[index] != 0x00 ) {
		char* pKey   = (char*) &Buffer[index];
		char* pValue = NULL;
		int i;
		
		while(index < BufferSize && Buffer[index] != 0x0A ) {
			if( Buffer[index] == ':' ) {
				Buffer[index] = 0x00;
				pValue = (char*) &Buffer[index+1];
			}
			index += 1;
		}
		Buffer[index] = 0x00;
		index += 1;
		if( pValue != NULL ) {
			printf("%-20s: %s\n",pKey,pValue);
			if (strcasecmp(pKey,"Jedec") == 0 ) {
				for (i = 0; i < 4; i += 1) {
					int v = 0;
					sscanf(&pValue[i*2],"%02x",&v);
					JedecID[i] = v;
				}
			} else if (strcasecmp(pKey,"DevId") == 0 ) {
				for (i = 0; i < 2; i += 1) {
					int v = 0;
					sscanf(&pValue[i*2],"%02x",&v);
					DevID[i] = v;
				}
			} else if (strcasecmp(pKey,"CardId") == 0 ) {
				nCardIDs = sscanf(pValue,"%x,%x,%x,%x,%x,%x,%x,%x",
						  &CardIDs[0],&CardIDs[1],&CardIDs[2],&CardIDs[3],
						  &CardIDs[4],&CardIDs[5],&CardIDs[6],&CardIDs[7]);
			} else if (strcasecmp(pKey,"Version") == 0 ) {
				sscanf(pValue,"%d.%d",&VerMajor,&VerMinor);
			}
		}
	}
	index += 1;
	if (index >= BufferSize || ((BufferSize - index) % 16) != 0) {
		printf("Invalid File Size \n");
		return 0;
	}
	
	// TODO Validate Jedec ID, DevID,CardID
	// TODO Validate CRC
	
	hr = 0;
	do {
		int bCardIDValid = 0, i;
		uint8_t Id[] = { 0x00,0x00,0x00,0x00 };
		uint8_t DevInfo[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
		uint8_t Addr = 0x00;
		
		hr = i2c_read(dev, BusNumber, 0x88, IDCODE_PUB, sizeof(IDCODE_PUB), Id, sizeof(Id));
		if (hr < 0)
			break;
		
		// TODO Hardcoded Jedec ID, get ist from File
		if( Id[0] != JedecID[0] || Id[1] != JedecID[1] || Id[2] != JedecID[2] || Id[3] != JedecID[3] ) {
			printf("Wrong Jedec ID %02x %02x %02x %02x\n",Id[0],Id[1],Id[2],Id[3]);
			break;
		}
		printf("Jedec ID %02x %02x %02x %02x\n",Id[0],Id[1],Id[2],Id[3]);
		
		hr = i2c_read(dev, BusNumber,0x20,&Addr,1,DevInfo,sizeof(DevInfo));
		if( hr == 0) {
			printf("DevID %02x%02x CardID %02x Version %d.%d\n",
			       DevInfo[0],DevInfo[1],DevInfo[2],DevInfo[4],DevInfo[5]);
			if( DevInfo[0] != DevID[0] || DevInfo[1] != DevID[1]  ) {
				printf("Wrong Dev ID %02x%02x (%02x%02x)\n",DevInfo[0],DevInfo[1],DevID[0],DevID[1]);
				break;
			}
		}
		for (i = 0; i < nCardIDs; i += 1)
		{
			if( CardIDs[i] == DevInfo[2] )
			{
				bCardIDValid = 1;
				break;
			}
		}
		if ( !bCardIDValid ) {
			printf("Wrong CardID %02x\n",DevInfo[2]);
			break;
		}
		

		hr = i2c_write(dev, BusNumber,0x88,ISC_ENABLE_X,sizeof(ISC_ENABLE_X),NULL,0);
		if (hr)
			break;
		usleep(5000);
		
		printf("Erase ");
		hr = i2c_write(dev, BusNumber,0x88,ISC_ERASE_CFG,sizeof(ISC_ERASE_CFG),NULL,0);
		if (hr)
			break;
		usleep(5000);
		
		hr = XO2WaitBusy(dev, BusNumber);
		if (hr)
			break;
		printf("Done\n");
		
		hr = i2c_write(dev, BusNumber,0x88,
			       LSC_INIT_ADDRESS,sizeof(LSC_INIT_ADDRESS),NULL,0);
		if (hr)
			break;
		
		printf("Prog ");
		int Counter = 0;
		while(index < BufferSize)
		{
			hr = i2c_write(dev, BusNumber,0x88,LSC_PROG_INCR_NV,sizeof(LSC_PROG_INCR_NV),&Buffer[index],16);
			if (hr)
				break;
			
			hr = XO2WaitBusy(dev, BusNumber);
			if (hr)
				break;
			
			index += 16;
			Counter += 1;
			if( Counter == 16 ) 
			{
				printf(".");
				Counter = 0;
			}
		}
		if (hr)
			break;
		printf("Done\n");
		
		hr = i2c_write(dev, BusNumber,0x88,ISC_PROGRAM_DONE,sizeof(ISC_PROGRAM_DONE),NULL,0);
		if (hr)
			break;
		
		hr = XO2WaitDone(dev, BusNumber);
		if (hr)
			break;
		
		printf("Refresh ");
		hr = i2c_write(dev, BusNumber,0x88,LSC_REFRESH,sizeof(LSC_REFRESH),NULL,0);
		if (hr)
			break;
		
		usleep(5000);
		hr = XO2WaitDone(dev, BusNumber);
		if (hr)
			break;
		printf("Done\n");
		
	} while(0);
	
	if (hr)
		printf("I2C Error %08x\n",hr);
	return 0;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

#define LICENSE_CONTROL 0x1C


void GetId(int dev, uint8_t Id[])
{
	int i;
	for(i = 0; i < 4; i += 1) {
		uint32_t tmp;
		
		writereg(dev,LICENSE_CONTROL,0x00100000 + (i << 16));
		readreg(dev,LICENSE_CONTROL,&tmp);
		Id[i*2]   = ((tmp >> 24) & 0xFF);
		Id[i*2+1] = ((tmp >> 16) & 0xFF);
	}
	writereg(dev,LICENSE_CONTROL,0x00000000);
}

void GetLK(int dev, uint8_t LK[])
{
	int i;
	
	for (i = 0; i < 12; i += 1) {
		uint32_t tmp;
		writereg(dev, LICENSE_CONTROL,0x00140000 + (i << 16));
		readreg(dev,LICENSE_CONTROL,&tmp);
		LK[i*2]   = ((tmp >> 24) & 0xFF);
		LK[i*2+1] = ((tmp >> 16) & 0xFF);
	}
	writereg(dev, LICENSE_CONTROL, 0x00000000);
}

char *GetSerNbr(int dev)
{
	char* SerNbr;
	uint8_t Buffer[17];
	uint32_t BytesReturned = 0;
	uint32_t Start = 0x10;
	int i;
	
	memset(Buffer,0,sizeof(Buffer));
	if (flashread(dev, Buffer, Start, sizeof(Buffer) - 1))
	{
		printf("Ioctl returns error\n");
		return NULL;
	}
	if( Buffer[0] == 0xFF )
		Buffer[0] = 0x00;
	
	SerNbr = malloc(17);
	memset(SerNbr, 0, sizeof(char) * 17);
	for(i = 0; i < 17; i += 1) {
		SerNbr[i] = (char) Buffer[i];
		if( SerNbr[i] == 0 )
			break;
	}

	return SerNbr;
}

int GetHex(char* s, uint32_t nBytes, uint8_t* Buffer)
{
	int i;
	if( strlen(s) < (nBytes * 2) ) return -1;
	for (i = 0; i < nBytes; i += 1) {
		char d0, d1;
		d0 = s[i*2];
		if( !isxdigit(d0) ) return -1;
		d1 = s[i*2+1];
		if( !isxdigit(d1) ) return -1;
		
		Buffer[i] =(uint8_t) ((d0 > '9' ? d0 - 'A' + 10 : d0 - '0') << 4) | ((d1 > '9' ? d1 - 'A' + 10 : d1 - '0'));
	}
	return (nBytes * 2);
}

int lic_export(int dev, int argc, char* argv[], uint32_t Flags)
{
	uint32_t HWVersion;
	uint32_t DeviceID;
	uint8_t Id[8];
	uint8_t LK[24];
	char *SerNbr;
	FILE* fout = stdout;
	int i;
	
	GetId(dev,Id);
	GetLK(dev,LK);
	SerNbr = GetSerNbr(dev);
	if( SerNbr == NULL )
		return -1;
	
	readreg(dev,0x00,&HWVersion);
	readreg(dev,0x08,&DeviceID);

	if( argc > 0 ) {
		size_t n = strlen(argv[0]) + 8 + 16;
		char *fname = malloc(n);
		
		snprintf(fname, n-1, "%s_%s.lic",argv[0],SerNbr);
		fout = fopen(fname,"w");
		if( fout == NULL ) {
			printf("Can't create outputfile\n");
			return 0;
		}
	}
	
	fprintf(fout,"VEN:%04X\n",DeviceID & 0xFFFF);
	fprintf(fout,"DEV:%04X\n",(DeviceID >> 16) & 0xFFFF);
	fprintf(fout,"VER:%08X\n",HWVersion);
	fprintf(fout,"SERNBR:%s\n",SerNbr);
	fprintf(fout,"ID:");
	for (i = 0; i < 8; i += 1)
		fprintf(fout,"%02X",Id[i]);
	fprintf(fout,"\n");
	fprintf(fout,"LK:");
	for (i = 0; i < 24; i += 1)
		fprintf(fout,"%02X",LK[i]);
	fprintf(fout,"\n");
	if( argc > 0 ) fclose(fout);
	return 0;
}

int lic_import(int dev, int argc, char* argv[], uint32_t Flags)
{
	uint8_t LicId[8];
	uint8_t CardId[8];
	char *SerNbr;
	uint8_t *Buffer;
	FILE* fin;
	int Flash;
	uint32_t FlashSize = 0;
	int err = 0;
	
	if( argc < 1 ) return -1;
	
	GetId(dev, CardId);
	
	
	SerNbr = GetSerNbr(dev);
	if( SerNbr == NULL )
		return -1;
	
	Buffer = malloc(4096);
	memset(Buffer, 0xFF, 4096);
	
	fin = fopen(argv[0], "r");
	if( fin == NULL )
	{
		printf("License file not found\n");
		return -1;
	}
	memset(LicId,0,8);
	while(1) {
		char s[128];
		if( fgets(s,sizeof(s)-1,fin) == NULL ) break;
		fputs(s,stdout);
		if (strncmp(s,"ID:",3) == 0 )
		{
			if( GetHex(&s[3],8,LicId) < 0 ) return -1;
		}
		if (strncmp(s,"LK:",3) == 0 )
		{
			if( GetHex(&s[3],24,Buffer) < 0 ) return -1;
		}
	}
	fclose(fin);
	
	if( memcmp(CardId,LicId,8) != 0 )
	{
		printf("Invalid ID\n");
		free(Buffer);
		return -1;
	}
	
	Dump(Buffer,0,24);
	Flash = FlashDetect(dev);
	
	switch(Flash) {
	case SSTI_SST25VF064C:   err = FlashWritePageMode(dev,0x7FE000,Buffer,4096,0x3C); break;
	case SPANSION_S25FL116K: err = FlashWritePageMode(dev,0x1FE000,Buffer,4096,0x1C); break;
	case SPANSION_S25FL132K: err = FlashWritePageMode(dev,0x3FE000,Buffer,4096,0x1C); break;
	case SPANSION_S25FL164K: err = FlashWritePageMode(dev,0x7FE000,Buffer,4096,0x1C); break;    
	default:
		printf("Unsupported Flash\n");
		break;
	}
	free(Buffer);
	return 0;
}

int lic_erase(int dev, int argc, char* argv[], uint32_t Flags)
{
	uint8_t* Buffer = malloc(4096);
	int Flash = FlashDetect(dev);
	uint32_t FlashSize = 0;
	int err = -1;

	memset(Buffer, 0xFF, 4096);
	switch(Flash)
	{
        case SSTI_SST25VF064C:   err = FlashWritePageMode(dev,0x7FE000,Buffer,4096,0x3C); break;
        case SPANSION_S25FL116K: err = FlashWritePageMode(dev,0x1FE000,Buffer,4096,0x1C); break;
        case SPANSION_S25FL132K: err = FlashWritePageMode(dev,0x3FE000,Buffer,4096,0x1C); break;
        case SPANSION_S25FL164K: err = FlashWritePageMode(dev,0x7FE000,Buffer,4096,0x1C); break;    
        default:
		printf("Unsupported Flash\n");
		break;
	}
	free(Buffer);
	return err;
}

static int read_sfpd(int dev, uint8_t adr, uint8_t *val)
{
	uint8_t cmd[5] = { 0x5a, 0, 0, adr, 00 };     
	int r;
	
	r = flashio(dev, cmd, 5, val, 1);
	if (r < 0)
		return r;
	return 0;
}

static int read_sst_id(int dev, uint8_t *id)
{
	uint8_t cmd[2] = { 0x88, 0 };
	uint8_t buf[9];
	int r;
	
	r = flashio(dev, cmd, 2, buf, 9);
	if (r < 0)
		return r;
	memcpy(id, buf + 1, 8);
	return 0;
}

int read_id(int dev, int argc, char* argv[], uint32_t Flags)
{
	int Flash = FlashDetect(dev);
	uint8_t Cmd;;
	uint8_t Id[8];
	uint32_t len, i, adr;
	

	switch(Flash) {
        case SPANSION_S25FL116K:
        case SPANSION_S25FL132K:
        case SPANSION_S25FL164K:
		for (i = 0; i < 8; i++)
			read_sfpd(dev, 0xf8 + i, &Id[i]);
		len = 8;
		break;
	case SSTI_SST25VF064C:
		read_sst_id(dev, Id);
		len = 8;
		break;
        default:
		printf("Unsupported Flash\n");
		return -1;
	}
	printf("ID: ");
	for (i = 0; i < len; i++)
		printf("%02x ", Id[i]);
	printf("\n");

}

struct SCommand CommandTable[] = 
{
	{ "memread",    ReadDeviceMemory,   1,   "Read Device Memory   : memread <start> <count>" },
	{ "memfill",    FillDeviceMemory,   1,   "Fill Device Memory   : memfill <start> <count> [<value>]" },
	{ "memwrite",   WriteDeviceMemory,  1,   "Write Device Memory  : memwrite <start> <values(8)> .." },
	{ "register",   GetSetRegister,     1,   "Get/Set Register     : reg <regname>|<[0x]regnum> [[0x]value(32)]" },
	
	{ "flashread",    ReadFlash,        1,   "Read Flash           : flashread <start> <count> [<Filename>]" },
	{ "flashio",      flashioc,          1,   "Flash IO             : flashio <write data>.. <read count>" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog <FileName> [<address>]" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog -SubVendorID <id>" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog -Jump <address>" },
	{ "flashverify",  FlashVerify,      1,   "Flash Verify         : flashverify <FileName>  [<address>]" },
	//{ "flasherase",   FlashErase,       1,   "FlashErase           : flasherase" },
	//{ "flashtest",    FlashTest,        1,   "FlashTest            : flashtest" },


	{ "mdio",        mdio,       1,   "mdio                 : mdio <adr> <reg> [<value>]" },
	{ "xo2prog",     XO2Prog,   1,   "DuoFlex Programming  : xo2prog <FileName> [<BusNumber>]" },

	{ "licimport",   lic_import,       1,   "License Import           : licimport" },
	{ "licexport",   lic_export,       1,   "License Export           : licexport" },
	{ "licerase",    lic_erase,        1,   "License Erase            : licerase"  },
	{ "read_id",    read_id,        1,      "Read Unique ID           : read_id"  },
 	{ NULL,NULL,0 }
};

void Help()
{
    int i = 0;
    while (CommandTable[i].Name != NULL) {
	    printf("   %s\n", CommandTable[i].Help);
	    i += 1;
    }
}


int main(int argc, char **argv)
{
	int cmd = 0, status;
	int i = 1;
	int Device = 0;
	uint32_t Flags = 0;
	int CmdIndex = 0;
	int dev;
	char ddbname[80];
	
	if( argc < 2 ) {
		Help();
		return 1;
	}
	
	
	while( i < argc )
	{
		if (*argv[i] != '-')
			break;
		if (strcmp(argv[i],"-r") == 0 || strcmp(argv[i],"--repeat") == 0 )
			Flags |= REPEAT_FLAG;
		else if( strcmp(argv[i],"-s") == 0 || strcmp(argv[i],"--silent") == 0 )
			Flags |= SILENT_FLAG;
		else if( strcmp(argv[i],"-d") == 0 || strcmp(argv[i],"--device") == 0 ) {
			i += 1;
			if( i < argc ) Device = strtoul(argv[i],NULL,0);
		} else if( strcmp(argv[i],"-l") == 0) {
			i += 1;
			if( i < argc ) linknr = strtoul(argv[i],NULL,0);
		} else if( strcmp(argv[i],"-?") == 0 || strcmp(argv[i],"--help") == 0 ) {
			Help();
			return 1;
		}
		i += 1;
	}
	
	if (i >= argc || Device > 99) {
		Help();
		return 1;
	}
	
	sprintf(ddbname, "/dev/ddbridge/card%d", Device);
	dev=open(ddbname, O_RDWR);
	if (dev < 0) {
		printf("Could not open device\n");
		return -1;
	}

	CmdIndex = i;
	
	while( CommandTable[cmd].Name != NULL )	{
		if (strncasecmp(CommandTable[cmd].Name,argv[CmdIndex],
				strlen(argv[CmdIndex])) == 0 ) 
			break;
		cmd += 1;
	}
	
	if (CommandTable[cmd].Name == NULL) {
		Help();
		return 1;
	}
	
	if (Flags != 0)
		printf(" Flags: %s %s\n",
		       (Flags&REPEAT_FLAG) ? "Repeat":"", 
		       (Flags&SILENT_FLAG)?"Silent":"");
	status = (*CommandTable[cmd].Function)(dev, argc - CmdIndex - 1,
					       &argv[CmdIndex+1], Flags);
	
	if (status == -1)
		printf("   %s\n",CommandTable[cmd].Help);
	return 0;
}

