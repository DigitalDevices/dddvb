enum {
	UNKNOWN_FLASH = 0,
	ATMEL_AT45DB642D = 1,
	SSTI_SST25VF016B = 2,
	SSTI_SST25VF032B = 3,
	SSTI_SST25VF064C = 4,
	SPANSION_S25FL116K = 5,
	SPANSION_S25FL132K = 6,
	SPANSION_S25FL164K = 7,
};

static uint32_t linknr = 0;

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

int FlashDetect(int dev)
{
	uint8_t Cmd = 0x9F;
	uint8_t Id[3];
	
	int r = flashio(dev, &Cmd, 1, Id, 3);
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
	else if ( Id[0] == 0x01 && Id[1] == 0x40 && Id[2] == 0x16 )
		r = SPANSION_S25FL132K; 
	else if ( Id[0] == 0x01 && Id[1] == 0x40 && Id[2] == 0x17 )
		r = SPANSION_S25FL164K; 
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
        case SPANSION_S25FL132K : 
		printf("Flash: SPANSION S25FL132K 32 MBit\n"); 
		break;
        case SPANSION_S25FL164K : 
		printf("Flash: SPANSION S25FL164K 64 MBit\n"); 
		break;
	}
	return r;
}


static int flashdetect(int fd, uint32_t *sector_size, uint32_t *flash_size)
{
	uint8_t cmd = 0x9F;
	uint8_t id[3];
	int flash_type;
	
	int r = flashio(fd, &cmd, 1, id, 3);
	if (r < 0)
		return r;
	
	if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x41) {
		flash_type = SSTI_SST25VF016B; 
		printf("Flash: SSTI  SST25VF016B 16 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x200000; 
	} else if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x4A) {
		flash_type = SSTI_SST25VF032B; 
		printf("Flash: SSTI  SST25VF032B 32 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x400000; 
	} else if (id[0] == 0xBF && id[1] == 0x25 && id[2] == 0x4B) {
		flash_type = SSTI_SST25VF064C; 
		printf("Flash: SSTI  SST25VF064C 64 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x800000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x15) {
		flash_type = SPANSION_S25FL116K;
		printf("Flash: SPANSION S25FL116K 16 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x200000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x16) {
		flash_type = SPANSION_S25FL132K;
		printf("Flash: SPANSION S25FL132K 32 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x400000; 
	} else if (id[0] == 0x01 && id[1] == 0x40 && id[2] == 0x17) {
		flash_type = SPANSION_S25FL164K;
		printf("Flash: SPANSION S25FL164K 64 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x800000; 
	} else if (id[0] == 0x1F && id[1] == 0x28) {
		flash_type = ATMEL_AT45DB642D; 
		printf("Flash: Atmel AT45DB642D  64 MBit\n");
		*sector_size = 1024; 
		*flash_size = 0x800000; 
	} else {
		printf("Unknown Flash Flash ID = %02x %02x %02x\n", id[0], id[1], id[2]);
		return -1;
	}
	return flash_type;
}


#if 1
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
		ret = flashio(ddb, cmd, 4, buf, l);
		if (ret < 0)
			return ret;
		addr += l;
		buf += l;
		len -= l;
	}
	return 0;
}
#else
static int flashread(int ddb, uint8_t *buf, uint32_t addr, uint32_t len)
{
	uint8_t cmd[4]= {0x03, (addr >> 16) & 0xff, 
			 (addr >> 8) & 0xff, addr & 0xff};
	
	return flashio(ddb, cmd, 4, buf, len);
}
#endif

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



void dump(const uint8_t *b, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) { 
		for (i = 0; i < 16; i++)
			if (i + j < l)
				printf("%02x ", b[i]);
			else
				printf("   ");
		printf(" | ");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				putchar((b[i] > 31 && b[i] < 127) ? b[i] : '.');
		printf("\n");
	}
}

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


int FlashWriteSSTI(int dev, uint32_t FlashOffset, uint8_t *Buffer, int BufferSize)
{
    int err = 0;
    uint8_t cmd[6];
    int i, j;

    // Must be multiple of sector size
    if ((BufferSize % 4096) != 0 ) 
	    return -1;   
    
    do {
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,cmd,1,NULL,0);
	    if (err < 0 ) 
		    break;

	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(dev,cmd,2,NULL,0);
	    if (err < 0 )
		    break;
	    
	    for (i = 0; i < BufferSize; i += 4096 ) {
		    if ((i & 0xFFFF) == 0 )
			    printf(" Erase    %08x\n",FlashOffset + i);
		    cmd[0] = 0x06;  // WREN
		    err = flashio(dev,cmd,1,NULL,0);
		    if (err < 0 )
			    break;
		    
		    cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		    cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		    cmd[3] = 0x00;
		    err = flashio(dev,cmd,4,NULL,0);
		    if (err < 0 )
			    break;
		    
		    while(1) {
			    cmd[0] = 0x05;  // RDRS
			    err = flashio(dev,cmd,1,&cmd[0],1);
			    if (err < 0 ) break;
			    if ((cmd[0] & 0x01) == 0 ) break;
		    }
		    if (err < 0 ) break;
	    }
	    if (err < 0 ) 
		    break;
	    for (j = BufferSize - 4096; j >= 0; j -= 4096 ) {
		    if ((j & 0xFFFF) == 0 )
			    printf(" Program  %08x\n",FlashOffset + j);
		    
		    for (i = 0; i < 4096; i += 2 ) {
			    if (i == 0 ) {
				    cmd[0] = 0x06;  // WREN
				    err = flashio(dev,cmd,1,NULL,0);
				    if (err < 0 ) 
					    break;
				    
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
				    cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
				    cmd[3] = 0x00;
				    cmd[4] = Buffer[j+i];
				    cmd[5] = Buffer[j+i+1];
				    err = flashio(dev,cmd,6,NULL,0);
			    } else {
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = Buffer[j+i];
				    cmd[2] = Buffer[j+i+1];
				    err = flashio(dev,cmd,3,NULL,0);
			    }
			    if (err < 0 ) 
				    break;
			    
			    while(1) {
				    cmd[0] = 0x05;  // RDRS
				    err = flashio(dev,cmd,1,&cmd[0],1);
				    if (err < 0 ) break;
				    if ((cmd[0] & 0x01) == 0 ) break;
			    }
			    if (err < 0 ) break;
		    }
		    if (err < 0 ) break;
		    
		    cmd[0] = 0x04;  // WDIS
		    err = flashio(dev,cmd,1,NULL,0);
		    if (err < 0 ) break;
		    
	    }
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,cmd,1,NULL,0);
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(dev,cmd,2,NULL,0);
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
        err = flashio(dev,Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
        err = flashio(dev,Cmd,2,NULL,0);
        if( err < 0 ) break;

        for(i = 0; i < BufferSize; i += 4096 )
        {
            if( (i & 0xFFFF) == 0 )
            {
                printf(" Erase    %08x\n",FlashOffset + i);
            }

            Cmd[0] = 0x06;  // WREN
            err = flashio(dev,Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x20;  // Sector erase ( 4Kb)
            Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            err = flashio(dev,Cmd,4,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = flashio(dev,Cmd,1,&Cmd[0],1);
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
            err = flashio(dev,Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x02;  // PP
            Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            memcpy(&Cmd[4],&Buffer[j],256);
            err = flashio(dev,Cmd,260,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = flashio(dev,Cmd,1,&Cmd[0],1);
                if( err < 0 ) break;
                if( (Cmd[0] & 0x01) == 0 ) break;
            }
            if( err < 0 ) break;

        }
        if( err < 0 ) break;

        Cmd[0] = 0x50;  // EWSR
        err = flashio(dev,Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = LockBits;  // BPx = 0, Lock all blocks
        err = flashio(dev,Cmd,2,NULL,0);

    }
    while(0);
    return err;
}
