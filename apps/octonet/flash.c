static uint32_t linknr = 0;

int flashio(int ddb, int link,
	    uint8_t *wbuf, uint32_t wlen, uint8_t *rbuf, uint32_t rlen)
{
	struct ddb_flashio fio = {
		.write_buf=wbuf,
		.write_len=wlen,
		.read_buf=rbuf,
		.read_len=rlen,
		.link=link,
	};
	
	return ioctl(ddb, IOCTL_DDB_FLASHIO, &fio);
}

int FlashDetect(int dev)
{
	uint8_t Cmd = 0x9F;
	uint8_t Id[3];
	
	int r = flashio(dev, linknr, &Cmd, 1, Id, 3);
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
	else if ( Id[0] == 0xef && Id[1] == 0x40 && Id[2] == 0x15 )
		r = WINBOND_W25Q16JV; 
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
        case WINBOND_W25Q16JV : 
		printf("Flash: Winbond W25Q16JV 16 MBit\n"); 
		break;
	}
	return r;
}


static int flashdetect(int fd, uint32_t *sector_size, uint32_t *flash_size)
{
	uint8_t cmd = 0x9F;
	uint8_t id[3];
	int flash_type;
	
	int r = flashio(fd, linknr, &cmd, 1, id, 3);
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
	} else if (id[0] == 0xef && id[1] == 0x40 && id[2] == 0x15) {
		flash_type = WINBOND_W25Q16JV;
		printf("Flash: Winbond 16 MBit\n");
		*sector_size = 4096; 
		*flash_size = 0x200000; 
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
		ret = flashio(ddb, linknr, cmd, 4, buf, l);
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
	
	return flashio(ddb, linknr, cmd, 4, buf, len);
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
		    err = flashio(dev,linknr, Cmd,4,NULL,0);
		    if( err < 0 ) break;
		    
		    while( 1 )
		    {
			    Cmd[0] = 0xD7;  // Read Status register
			    err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
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

        err = flashio(dev,linknr, Cmd,4 + 1024,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = BlockErase ? 0x88 : 0x83; // Buffer to Main Memory (with Erase)
        Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
        Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
        Cmd[3] = 0x00;

        err = flashio(dev,linknr, Cmd,4,NULL,0);
        if( err < 0 ) break;

        while( 1 )
        {
		Cmd[0] = 0xD7;  // Read Status register
		err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
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
	    err = flashio(dev,linknr, cmd,1,NULL,0);
	    if (err < 0 ) 
		    break;

	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(dev,linknr, cmd,2,NULL,0);
	    if (err < 0 )
		    break;
	    
	    for (i = 0; i < BufferSize; i += 4096 ) {
		    if ((i & 0xFFFF) == 0 )
			    printf(" Erase    %08x\n",FlashOffset + i);
		    cmd[0] = 0x06;  // WREN
		    err = flashio(dev,linknr, cmd,1,NULL,0);
		    if (err < 0 )
			    break;
		    
		    cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		    cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		    cmd[3] = 0x00;
		    err = flashio(dev,linknr, cmd,4,NULL,0);
		    if (err < 0 )
			    break;
		    
		    while(1) {
			    cmd[0] = 0x05;  // RDRS
			    err = flashio(dev,linknr, cmd,1,&cmd[0],1);
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
				    err = flashio(dev,linknr, cmd,1,NULL,0);
				    if (err < 0 ) 
					    break;
				    
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
				    cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
				    cmd[3] = 0x00;
				    cmd[4] = Buffer[j+i];
				    cmd[5] = Buffer[j+i+1];
				    err = flashio(dev,linknr, cmd,6,NULL,0);
			    } else {
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = Buffer[j+i];
				    cmd[2] = Buffer[j+i+1];
				    err = flashio(dev,linknr, cmd,3,NULL,0);
			    }
			    if (err < 0 ) 
				    break;
			    
			    while(1) {
				    cmd[0] = 0x05;  // RDRS
				    err = flashio(dev,linknr, cmd,1,&cmd[0],1);
				    if (err < 0 ) break;
				    if ((cmd[0] & 0x01) == 0 ) break;
			    }
			    if (err < 0 ) break;
		    }
		    if (err < 0 ) break;
		    
		    cmd[0] = 0x04;  // WDIS
		    err = flashio(dev,linknr, cmd,1,NULL,0);
		    if (err < 0 ) break;
		    
	    }
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,linknr, cmd,1,NULL,0);
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(dev,linknr, cmd,2,NULL,0);
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
	    err = flashio(dev,linknr, Cmd,1,NULL,0);
	    if( err < 0 ) 
		    break;

	    Cmd[0] = 0x01;  // WRSR
	    Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(dev,linknr, Cmd,2,NULL,0);
	    if( err < 0 )
		    break;
	    
	    for(i = 0; i < BufferSize; i += 4096 ) {
		    if( (i & 0xFFFF) == 0 )
			    printf(" Erase    %08x\n",FlashOffset + i);
		    Cmd[0] = 0x06;  // WREN
		    err = flashio(dev,linknr, Cmd,1,NULL,0);
		    if( err < 0 )
			    break;
		    
		    Cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		    Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		    Cmd[3] = 0x00;
		    err = flashio(dev,linknr, Cmd,4,NULL,0);
		    if( err < 0 )
			    break;
		    
		    while(1) {
			    Cmd[0] = 0x05;  // RDRS
			    err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
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
				    err = flashio(dev,linknr, Cmd,1,NULL,0);
				    if( err < 0 ) 
					    break;
				    
				    Cmd[0] = 0xAD;  // AAI
				    Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
				    Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
				    Cmd[3] = 0x00;
				    Cmd[4] = Buffer[j+i];
				    Cmd[5] = Buffer[j+i+1];
				    err = flashio(dev,linknr, Cmd,6,NULL,0);
			    } else {
				    Cmd[0] = 0xAD;  // AAI
				    Cmd[1] = Buffer[j+i];
				    Cmd[2] = Buffer[j+i+1];
				    err = flashio(dev,linknr, Cmd,3,NULL,0);
			    }
			    if( err < 0 ) 
				    break;
			    
			    while(1) {
				    Cmd[0] = 0x05;  // RDRS
				    err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
				    if( err < 0 ) break;
				    if( (Cmd[0] & 0x01) == 0 ) break;
			    }
			    if( err < 0 ) break;
		    }
		    if( err < 0 ) break;
		    
		    Cmd[0] = 0x04;  // WDIS
		    err = flashio(dev,linknr, Cmd,1,NULL,0);
		    if( err < 0 ) break;
		    
	    }
	    if( err < 0 ) break;
	    
	    Cmd[0] = 0x50;  // EWSR
	    err = flashio(dev,linknr, Cmd,1,NULL,0);
	    if( err < 0 ) break;
	    
	    Cmd[0] = 0x01;  // WRSR
	    Cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(dev,linknr, Cmd,2,NULL,0);
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
        err = flashio(dev,linknr, Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
        err = flashio(dev,linknr, Cmd,2,NULL,0);
        if( err < 0 ) break;

        for(i = 0; i < BufferSize; i += 4096 )
        {
            if( (i & 0xFFFF) == 0 )
            {
                printf(" Erase    %08x\n",FlashOffset + i);
            }

            Cmd[0] = 0x06;  // WREN
            err = flashio(dev,linknr, Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x20;  // Sector erase ( 4Kb)
            Cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            err = flashio(dev,linknr, Cmd,4,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
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
            err = flashio(dev,linknr, Cmd,1,NULL,0);
            if( err < 0 ) break;

            Cmd[0] = 0x02;  // PP
            Cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
            Cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
            Cmd[3] = 0x00;
            memcpy(&Cmd[4],&Buffer[j],256);
            err = flashio(dev,linknr, Cmd,260,NULL,0);
            if( err < 0 ) break;

            while(1)
            {
                Cmd[0] = 0x05;  // RDRS
                err = flashio(dev,linknr, Cmd,1,&Cmd[0],1);
                if( err < 0 ) break;
                if( (Cmd[0] & 0x01) == 0 ) break;
            }
            if( err < 0 ) break;

        }
        if( err < 0 ) break;

        Cmd[0] = 0x50;  // EWSR
        err = flashio(dev,linknr, Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = LockBits;  // BPx = 0, Lock all blocks
        err = flashio(dev,linknr, Cmd,2,NULL,0);

    }
    while(0);
    return err;
}

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
		err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		if (err < 0)
			break;
		
		cmd[0] = 0x01;  // WRSR
		cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
		err = flashio(ddf->fd, ddf->link, cmd, 2, NULL, 0);
		if (err < 0)
			break;
		
		for (i = 0; i < flen; i += 4096) {
			if ((i & 0xFFFF) == 0)
				printf(" Erase    %08x\n", FlashOffset + i);
			
			cmd[0] = 0x06;  // WREN
			err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
			if (err < 0)
				break;
			
			cmd[0] = 0x20;  // Sector erase ( 4Kb)
			cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
			cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
			cmd[3] = 0x00;
			err = flashio(ddf->fd, ddf->link, cmd, 4, NULL, 0);
			if (err < 0)
				break;

			while (1) {
				cmd[0] = 0x05;  // RDRS
				err = flashio(ddf->fd, ddf->link, cmd, 1, &cmd[0], 1);
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
			err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
			if (err < 0)
				break;
			
			cmd[0] = 0x02;  // PP
			cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
			cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
			cmd[3] = 0x00;
			memcpy(&cmd[4], ddf->buffer, 256);
			err = flashio(ddf->fd, ddf->link, cmd, 260, NULL, 0);
			if (err < 0)
				break;
			
			while(1) {
				cmd[0] = 0x05;  // RDRS
				err = flashio(ddf->fd, ddf->link, cmd,1, &cmd[0], 1);
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
		err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		if (err < 0)
			break;
		
		cmd[0] = 0x01;  // WRSR
		cmd[1] = LockBits;  // BPx = 0, Lock all blocks
		err = flashio(ddf->fd, ddf->link, cmd, 2, NULL, 0);
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
	    err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
	    if (err < 0) 
		    break;

	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	    err = flashio(ddf->fd, ddf->link, cmd, 2, NULL, 0);
	    if (err < 0 )
		    break;
	    
	    for (i = 0; i < flen; i += 4096) {
		    if ((i & 0xFFFF) == 0 )
			    printf("Erase %08x\n", FlashOffset + i);
		    cmd[0] = 0x06;  // WREN
		    err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		    if (err < 0 )
			    break;
		    
		    cmd[0] = 0x20;  // Sector erase ( 4Kb)
		    cmd[1] = (((FlashOffset + i ) >> 16) & 0xFF);
		    cmd[2] = (((FlashOffset + i ) >>  8) & 0xFF);
		    cmd[3] = 0x00;
		    err = flashio(ddf->fd,ddf->link, cmd,4,NULL,0);
		    if (err < 0 )
			    break;
		    
		    while(1) {
			    cmd[0] = 0x05;  // RDRS
			    err = flashio(ddf->fd,ddf->link, cmd,1,&cmd[0],1);
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
				    err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
				    if (err < 0 ) 
					    break;
				    
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ((( FlashOffset + j ) >> 16) & 0xFF );
				    cmd[2] = ((( FlashOffset + j ) >>  8) & 0xFF );
				    cmd[3] = 0x00;
				    cmd[4] = ddf->buffer[i];
				    cmd[5] = ddf->buffer[i + 1];
				    err = flashio(ddf->fd,ddf->link, cmd,6,NULL,0);
			    } else {
				    cmd[0] = 0xAD;  // AAI
				    cmd[1] = ddf->buffer[i];
				    cmd[2] = ddf->buffer[i + 1];
				    err = flashio(ddf->fd,ddf->link, cmd,3,NULL,0);
			    }
			    if (err < 0 ) 
				    break;
			    
			    while(1) {
				    cmd[0] = 0x05;  // RDRS
				    err = flashio(ddf->fd,ddf->link, cmd,1,&cmd[0],1);
				    if (err < 0 ) break;
				    if ((cmd[0] & 0x01) == 0 ) break;
			    }
			    if (err < 0 ) 
				    break;
		    }
		    if (err < 0)
			    break;
		    
		    cmd[0] = 0x04;  // WDIS
		    err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		    if (err < 0 ) 
			    break;
#endif
	    }
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x50;  // EWSR
	    err = flashio(ddf->fd,ddf->link, cmd,1,NULL,0);
	    if (err < 0 ) break;
	    
	    cmd[0] = 0x01;  // WRSR
	    cmd[1] = 0x1C;  // BPx = 0, Lock all blocks
	    err = flashio(ddf->fd,ddf->link, cmd,2,NULL,0);
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
	case WINBOND_W25Q16JV:
		return flashwrite_pagemode(ddf, fs, addr, 0x1c, fw_off);
	}
	return -1;
}


static int flash_detect(struct ddflash *ddf)
{
	uint8_t cmd = 0x9F;
	uint8_t id[3];
	
	int r = flashio(ddf->fd, ddf->link, &cmd, 1, id, 3);
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
	} else if (id[0] == 0xef && id[1] == 0x40 && id[2] == 0x15) {
		ddf->flash_type = WINBOND_W25Q16JV;
		printf("Flash: Winbond W25Q16JV 16 MBit\n");
		ddf->sector_size = 4096; 
		ddf->size = 0x200000; 
	} else {
		printf("Unknown Flash Flash ID = %02x %02x %02x\n", id[0], id[1], id[2]);
		return -1;
	}
	if (ddf->sector_size) {
		ddf->buffer = malloc(ddf->sector_size);
		//printf("allocated buffer %08x@%08x\n", ddf->sector_size, (uint32_t) ddf->buffer);
		if (!ddf->buffer)
			return -1;
	}
	return 0;
}

static int get_id(struct ddflash *ddf)
{
	uint8_t id[4];
	struct ddb_reg ddbreg;

	if (ddf->link == 0) {
		if (ioctl(ddf->fd, IOCTL_DDB_ID, &ddf->id) < 0)
			return -1;
		return 0;
	}
        ddbreg.reg = 8 + (ddf->link << 28);
	if (ioctl(ddf->fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	ddf->id.vendor = ddbreg.val;
	ddf->id.device = ddbreg.val >> 16;

        ddbreg.reg = 12 + (ddf->link << 28);
	if (ioctl(ddf->fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	ddf->id.subvendor = ddbreg.val;
	ddf->id.subdevice = ddbreg.val >> 16;

        ddbreg.reg = 0 + (ddf->link << 28);
	if (ioctl(ddf->fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	ddf->id.hw = ddbreg.val;

	ddbreg.reg = 4 + (ddf->link << 28);
	if (ioctl(ddf->fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	ddf->id.regmap = ddbreg.val;
	return 0;
}


static char *devid2fname(uint16_t devid, char **name)
{
	char *fname = 0;

	switch (devid) {
	case 0x0002:
		fname="DVBBridgeV1A_DVBBridgeV1A.bit";
		*name = "Octopus 35\n";
		break;
	case 0x0003:
		fname="DVBBridgeV1B_DVBBridgeV1B.fpga";
		*name = "Octopus\n";
		break;
	case 0x0005:
		fname="DVBBridgeV2A_DD01_0005_STD.fpga";
		*name = "Octopus Classic\n";
		break;
	case 0x0006:
		fname="DVBBridgeV2A_DD01_0006_STD.fpga";
		*name = "CineS2 V7\n";
		break;
	case 0x0007:
		fname="DVBBridgeV2A_DD01_0007_MXL.fpga";
		*name = "Octopus 4/8\n";
		break;
	case 0x0008:
		fname="DVBBridgeV2A_DD01_0008_CXD.fpga";
		*name = "Octopus 4/8\n";
		break;
	case 0x0009:
		fname="DVBBridgeV2A_DD01_0009_SX8.fpga";
		*name = "Octopus MAXSX8\n";
		break;
	case 0x000b:
		fname="DVBBridgeV2A_DD01_000B_SX8.fpga";
		*name = "Octopus MAXSX8 Basic\n";
		break;
	case 0x000a:
		fname="DVBBridgeV2A_DD01_000A_M4.fpga";
		*name = "Octopus MAXM4\n";
		break;
	case 0x0011:
		fname="DVBBridgeV2B_DD01_0011.fpga";
		*name = "Octopus CI\n";
		break;
	case 0x0012:
		fname="DVBBridgeV2B_DD01_0012_STD.fpga";
		*name = "Octopus CI\n";
		break;
	case 0x0013:
		fname="DVBBridgeV2B_DD01_0013_PRO.fpga";
		*name = "Octopus PRO\n";
		break;
	case 0x0020:
		fname="DVBBridgeV2C_DD01_0020.fpga";
		*name = "Octopus GT Mini\n";
		break;
	case 0x0201:
		fname="DVBModulatorV1B_DVBModulatorV1B.bit";
		*name = "Modulator\n";
		break;
	case 0x0203:
		fname="DVBModulatorV1B_DD01_0203.fpga";
		*name = "Modulator Test\n";
		break;
	case 0x0210:
		fname="DVBModulatorV2A_DD01_0210.fpga";
		*name = "Modulator V2\n";
		break;
	case 0x0220:
		fname="SDRModulatorV1A_DD01_0220.fpga";
		*name = "SDRModulator ATV\n";
		break;
	case 0x0221:
		fname="SDRModulatorV1A_DD01_0221_IQ.fpga";
		*name = "SDRModulator IQ\n";
		break;
	case 0x0222:
		fname="SDRModulatorV1A_DD01_0222_DVBT.fpga";
		*name = "SDRModulator DVBT\n";
		break;
	default:
		*name = "UNKNOWN\n";
		break;
	}
	return fname;
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
				printf("%s: no compatible id found\n", fn);
				ret = -2; /* no compatible ID */
				goto out;
			}
		} else if (!strcasecmp(key, "Version")) {
			if (strchr(val,'.')) {
				int major = 0, minor = 0;
				sscanf(val,"%d.%d",&major,&minor);
				version = (major << 16) + minor;
			} else
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
			printf("%s: older or same version\n", fn);
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
	if (res == 0) {
		res = flashcmp(ddf, fs, adr, len, fw_off);
		if (res == -2) {
			res = 1;
		}
	}
 
out:
	close(fs);
	return res;
}


static int fexists(char *fn)
{
	struct stat b;

	return (!stat(fn, &b));
}

