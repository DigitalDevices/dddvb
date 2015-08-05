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

static uint32_t linknr = 0;

typedef int (*COMMAND_FUNCTION)(int dev, int argc, char* argv[], uint32_t Flags);

enum {
	REPEAT_FLAG = 0x00000001,
	SILENT_FLAG = 0x00000002,
};

enum {
	UNKNOWN_FLASH = 0,
	ATMEL_AT45DB642D = 1,
	SSTI_SST25VF016B = 2,
	SSTI_SST25VF032B = 3,
	SSTI_SST25VF064C = 4,
	SPANSION_S25FL116K = 5,
};

struct SCommand
{
	char*                Name;
	COMMAND_FUNCTION     Function;
	int                  Open;
	char*                Help;
};

// --------------------------------------------------------------------------------------------

void Dump(const uint8_t *b, uint32_t start, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) { 
		printf("%08x: ", start + j);
		for (i = 0; i < 16; i++)
			if (i + j < l)
				printf("%02x ", b[i]);
			else
				printf("   ");
		printf(" |");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				putchar((b[i] > 31 && b[i] < 127) ? b[i] : '.');
		printf("|\n");
	}
}

int readreg(int dev, uint32_t RegAddress, uint32_t *pRegValue)
{
	struct ddb_reg reg = { .reg = RegAddress };
	int ret;
	
	ret = ioctl(dev, IOCTL_DDB_READ_REG, &reg);
	if (ret < 0) 
		return ret;
	if (pRegValue)
		*pRegValue = reg.val;
	return 0;
}

int writereg(int dev, uint32_t RegAddress, uint32_t RegValue)
{
	struct ddb_reg reg = { .reg = RegAddress, .val = RegValue};

	return ioctl(dev, IOCTL_DDB_WRITE_REG, &reg);
}

int FlashIO(int ddb, uint8_t *wbuf, uint32_t wlen, uint8_t *rbuf, uint32_t rlen)
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

int flashread(int ddb, uint8_t *buf, uint32_t addr, uint32_t len)
{
	int ret;
	uint8_t cmd[4];
	uint32_t l;

	while (len) {
		cmd[0] = 3;
		cmd[1] = (addr >> 16) & 0xff;
		cmd[2] = (addr >> 8) & 0xff;
		cmd[3] = addr & 0xff;
		
		if (len > 1024)
			l = 1024;
		else
			l = len;
		ret = FlashIO(ddb, cmd, 4, buf, l);
		if (ret < 0)
			return ret;
		addr += l;
		buf += l;
		len -= l;
	}
	return 0;
}

int ReadFlash(int ddb, int argc, char *argv[], uint32_t Flags)
{
	uint32_t Start;
	uint32_t Len;
	uint8_t *Buffer;

	if (argc < 2 ) 
		return -1;
	Start = strtoul(argv[0],NULL,16);
	Len   = strtoul(argv[1],NULL,16);
	
	Buffer = malloc(Len);
	if (flashread(ddb, Buffer, Start, Len) < 0) {
		printf("flashread error\n");
		free(Buffer);
		return 0;
	}
	
	Dump(Buffer,Start,Len);
	
	free(Buffer);
	return 0;
}



int FlashDetect(int dev)
{
	uint8_t Cmd = 0x9F;
	uint8_t Id[3];
	
	int r = FlashIO(dev, &Cmd, 1, Id, 3);
	if (r < 0) 
		return r;
	
	if (Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x41)
		r = SSTI_SST25VF016B; 
	else if (Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x4A)
		r = SSTI_SST25VF032B; 
	else if ( Id[0] == 0xBF && Id[1] == 0x25 && Id[2] == 0x4B )
		r = SSTI_SST25VF064C; 
	else if ( Id[0] == 0x01 && Id[1] == 0x40 && Id[2] == 0x15 )
		r = SPANSION_S25FL116K; 
	else if ( Id[0] == 0x1F && Id[1] == 0x28)
		r = ATMEL_AT45DB642D; 
	else 
		r = UNKNOWN_FLASH;
	
	switch(r) {
        case UNKNOWN_FLASH : 
		printf("Unknown Flash Flash ID = %02x %02x %02x\n",Id[0],Id[1],Id[2]); 
		break;
        case ATMEL_AT45DB642D : 
		printf("Flash: Atmel AT45DB642D  64 MBit\n"); 
		break;
        case SSTI_SST25VF016B : 
		printf("Flash: SSTI  SST25VF016B 16 MBit\n"); 
		break;
        case SSTI_SST25VF032B : 
		printf("Flash: SSTI  SST25VF032B 32 MBit\n"); 
		break;
        case SSTI_SST25VF064C :
		printf("Flash: SSTI  SST25VF064C 64 MBit\n");
		break;
        case SPANSION_S25FL116K : 
		printf("Flash: SPANSION S25FL116K 16 MBit\n"); 
		break;
	}
	return r;
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
		err = FlashIO(dev,Cmd,4,NULL,0);
		if( err < 0 ) 
			break;
		while (1) {
			Cmd[0] = 0xD7;  // Read Status register
			err = FlashIO(dev,Cmd,1,&Cmd[0],1);
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
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = FlashIO(dev,Cmd,2,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x06;  // WREN
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x60;  // CHIP Erase
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		while(1) {
			Cmd[0] = 0x05;  // RDRS
			err = FlashIO(dev,Cmd,1,&Cmd[0],1);
			if( err < 0 ) break;
			if( (Cmd[0] & 0x01) == 0 ) break;
		}
		if ( err < 0 )
			break;
		
		Cmd[0] = 0x50;  // EWSR
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
		err = FlashIO(dev,Cmd,2,NULL,0);
	}
	while(0);
	
	if( err >= 0 ) printf("Flash erase succeeded\n");
	else           printf("Flash erase failed\n");
	return 0;
}


int FlashWriteAtmel(int dev,uint32_t FlashOffset,uint8_t * Buffer,int BufferSize)
{
	int err = 0, i;
	int BlockErase = BufferSize >= 8192;
	uint8_t Cmd[4];
	
	if( BlockErase ) {
		for(i = 0; i < BufferSize; i += 8192 ) {
			if( (i & 0xFFFF) == 0 )
			{
				printf(" Erase    %08x\n",FlashOffset + i);
			}
			Cmd[0] = 0x50; // Block Erase
			Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
			Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
			Cmd[3] = 0x00;
			err = FlashIO(dev,Cmd,4,NULL,0);
			if( err < 0 ) break;
			
			while(1) {
				Cmd[0] = 0xD7;  // Read Status register
				err = FlashIO(dev,Cmd,1,&Cmd[0],1);
				if( err < 0 ) break;
				if( (Cmd[0] & 0x80) == 0x80 ) break;
			}
		}
	}
	
	for(i = 0; i < BufferSize; i += 1024 )
	{
		if( (i & 0xFFFF) == 0 )
		{
			printf(" Programm %08x\n",FlashOffset + i);
		}
		uint8_t Cmd[4 + 1024];
		Cmd[0] = 0x84; // Buffer 1
		Cmd[1] = 0x00;
		Cmd[2] = 0x00;
		Cmd[3] = 0x00;
		memcpy(&Cmd[4],&Buffer[i],1024);
		
		err = FlashIO(dev,Cmd,4 + 1024,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = BlockErase ? 0x88 : 0x83; // Buffer to Main Memory (with Erase)
		Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		Cmd[3] = 0x00;
		
		err = FlashIO(dev,Cmd,4,NULL,0);
		if( err < 0 ) break;
		
		while(1)
		{
			Cmd[0] = 0xD7;  // Read Status register
			err = FlashIO(dev,Cmd,1,&Cmd[0],1);
			if( err < 0 ) break;
			if( (Cmd[0] & 0x80) == 0x80 ) break;
		}
		if( err < 0 ) break;
	}
	return err;
}

// **************************************************************************************
// BUG: Erasing and writing an incomplete image will result in an failure to boot golden image.
// FIX: Write the new image from high to low addresses

int FlashWriteSSTI(int dev,uint32_t FlashOffset,uint8_t * Buffer,int BufferSize)
{
	int err = 0, i, j;
	uint8_t Cmd[6];
	
	if( (BufferSize % 4096) != 0 ) return -1;   // Must be multiple of sector size
	
	do
	{
		Cmd[0] = 0x50;  // EWSR
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = FlashIO(dev,Cmd,2,NULL,0);
		if( err < 0 ) break;
		
		for (i = 0; i < BufferSize; i += 4096 )
		{
			if( (i & 0xFFFF) == 0 )
			{
				printf(" Erase    %08x\n",FlashOffset + i);
			}
			
			Cmd[0] = 0x06;  // WREN
			err = FlashIO(dev,Cmd,1,NULL,0);
			if( err < 0 ) break;
			
			Cmd[0] = 0x20;  // Sector erase ( 4Kb)
			Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
			Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
			Cmd[3] = 0x00;
			err = FlashIO(dev,Cmd,4,NULL,0);
			if( err < 0 ) break;
			
			while(1)
			{
				Cmd[0] = 0x05;  // RDRS
				err = FlashIO(dev,Cmd,1,&Cmd[0],1);
				if( err < 0 ) break;
				if( (Cmd[0] & 0x01) == 0 ) break;
			}
			if( err < 0 ) break;
			
		}
		if( err < 0 ) break;
		
		
		for (j = BufferSize - 4096; j >= 0; j -= 4096 )
		{
			if( (j & 0xFFFF) == 0 )
			{
				printf(" Programm %08x\n",FlashOffset + j);
			}
			
			for (i = 0; i < 4096; i += 2 )
			{
				
				if( i == 0 )
				{
					Cmd[0] = 0x06;  // WREN
					err = FlashIO(dev,Cmd,1,NULL,0);
					if( err < 0 ) break;
					
					Cmd[0] = 0xAD;  // AAI
					Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
					Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
					Cmd[3] = 0x00;
					Cmd[4] = Buffer[j+i];
					Cmd[5] = Buffer[j+i+1];
					err = FlashIO(dev,Cmd,6,NULL,0);
				}
				else
				{
					Cmd[0] = 0xAD;  // AAI
					Cmd[1] = Buffer[j+i];
					Cmd[2] = Buffer[j+i+1];
					err = FlashIO(dev,Cmd,3,NULL,0);
				}
				if( err < 0 ) break;
				
				while(1)
				{
					Cmd[0] = 0x05;  // RDRS
					err = FlashIO(dev,Cmd,1,&Cmd[0],1);
					if( err < 0 ) break;
					if( (Cmd[0] & 0x01) == 0 ) break;
				}
				if( err < 0 ) break;
			}
			if( err < 0 ) break;
			
			Cmd[0] = 0x04;  // WDIS
			err = FlashIO(dev,Cmd,1,NULL,0);
			if( err < 0 ) break;
			
		}
		if( err < 0 ) break;
		
		
		Cmd[0] = 0x50;  // EWSR
		err = FlashIO(dev,Cmd,1,NULL,0);
		if( err < 0 ) break;
		
		Cmd[0] = 0x01;  // WRSR
		Cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
		err = FlashIO(dev,Cmd,2,NULL,0);
		
	}
	while(0);
	return err;
}



int FlashWritePageMode(int dev, uint32_t FlashOffset,
		       uint8_t *Buffer,int BufferSize,uint8_t LockBits)
{
    int err = 0;
    uint8_t Cmd[260];
    int i, j;
    
    if( (BufferSize % 4096) != 0 ) return -1;   // Must be multiple of sector size

    do
    {
        Cmd[0] = 0x50;  // EWSR
        err = FlashIO(dev,Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
        err = FlashIO(dev,Cmd,2,NULL,0);
        if( err < 0 ) break;

        for(i = 0; i < BufferSize; i += 4096 )
        {
            if( (i & 0xFFFF) == 0 )
            {
                printf(" Erase    %08x\n",FlashOffset + i);
            }

            Cmd[0] = 0x06;  // WREN
            err = FlashIO(dev,Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x20;  // Sector erase ( 4Kb)
            Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            err = FlashIO(dev,Cmd,4,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = FlashIO(dev,Cmd,1,&Cmd[0],1);
                if( err < 0 ) break;
                if( (Cmd[0] & 0x01) == 0 ) break;
            }
            if( err < 0 ) break;

        }
        if( err < 0 ) break;


        for(j = BufferSize - 256; j >= 0; j -= 256 )
        {
            if( (j & 0xFFFF) == 0 )
            {
                printf(" Programm %08x\n",FlashOffset + j);
            }

            Cmd[0] = 0x06;  // WREN
            err = FlashIO(dev,Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x02;  // PP
            Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            memcpy(&Cmd[4],&Buffer[j],256);
            err = FlashIO(dev,Cmd,260,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = FlashIO(dev,Cmd,1,&Cmd[0],1);
                if( err < 0 ) break;
                if( (Cmd[0] & 0x01) == 0 ) break;
            }
            if( err < 0 ) break;

        }
        if( err < 0 ) break;

        Cmd[0] = 0x50;  // EWSR
        err = FlashIO(dev,Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = LockBits;  // BPx = 0, Lock all blocks
        err = FlashIO(dev,Cmd,2,NULL,0);

    }
    while(0);
    return err;
}

// --------------------------------------------------------------------------------------------

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

int FlashIOC(int dev,int argc, char* argv[],uint32_t Flags)
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

    if( FlashIO(dev,Buffer,WriteLen,Buffer,ReadLen) < 0 )
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


struct SCommand CommandTable[] = 
{
	{ "memread",    ReadDeviceMemory,   1,   "Read Device Memory   : memread <start> <count>" },
	{ "memfill",    FillDeviceMemory,   1,   "Fill Device Memory   : memfill <start> <count> [<value>]" },
	{ "memwrite",   WriteDeviceMemory,  1,   "Write Device Memory  : memwrite <start> <values(8)> .." },
	{ "register",   GetSetRegister,     1,   "Get/Set Register     : reg <regname>|<[0x]regnum> [[0x]value(32)]" },
	
	{ "flashread",    ReadFlash,        1,   "Read Flash           : flashread <start> <count>" },
	{ "flashio",      FlashIO,          1,   "Flash IO             : flashio <write data>.. <read count>" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog <FileName> [<address>]" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog -SubVendorID <id>" },
	{ "flashprog",    FlashProg,        1,   "Flash Programming    : flashprog -Jump <address>" },
	{ "flashverify",  FlashVerify,      1,   "Flash Verify         : flashverify <FileName>  [<address>]" },
	{ "flasherase",   FlashErase,       1,   "FlashErase           : flasherase" },
	//{ "flashtest",    FlashTest,        1,   "FlashTest            : flashtest" },


	{ "mdio",        mdio,       1,   "mdio                 : mdio <adr> <reg> [<value>]" },
	{ "xo2prog",     XO2Prog,   1,   "DuoFlex Programming  : xo2prog <FileName> [<BusNumber>]" },
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

