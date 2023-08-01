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



static uint32_t linknr = 0;

int flashio(int ddb, int link,
	    uint8_t *wbuf, uint32_t wlen,
	    uint8_t *rbuf, uint32_t rlen)
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

static int flash_id(int fd, int link, uint8_t *id)
{
	uint8_t cmd = 0x9F;
	
	return flashio(fd, link, &cmd, 1, id, 3);
}


struct flash_info flashs[] = {
	{ { 0xbf, 0x25, 0x41 }, SSTI_SST25VF016B, 4096, 0x200000, "SSTI  SST25VF016B 16 MBit" },
	{ { 0xbf, 0x25, 0x4a }, SSTI_SST25VF032B, 4096, 0x400000, "SSTI  SST25VF032B 32 MBit" },
	{ { 0xbf, 0x25, 0x4b }, SSTI_SST25VF064C, 4096, 0x400000, "SSTI  SST25VF064C 64 MBit" },
	{ { 0x01, 0x40, 0x15 }, SPANSION_S25FL116K, 4096, 0x200000, "SPANSION S25FL116K 16 MBit" },
	{ { 0x01, 0x40, 0x16 }, SPANSION_S25FL132K, 4096, 0x400000, "SPANSION S25FL132K 32 MBit" },
	{ { 0x01, 0x40, 0x17 }, SPANSION_S25FL164K, 4096, 0x800000, "SPANSION S25FL164K 64 MBit" },
	{ { 0xef, 0x40, 0x15 }, WINBOND_W25Q16JV, 4096, 0x200000, "Winbond W25Q16JV 16 MBit" },
	{ { 0xef, 0x40, 0x16 }, WINBOND_W25Q32JV, 4096, 0x400000, "Winbond W25Q32JV 32 MBit" },
	{ { 0xef, 0x40, 0x17 }, WINBOND_W25Q64JV, 4096, 0x800000, "Winbond W25Q64JV 64 MBit" },
	{ { 0xef, 0x40, 0x18 }, WINBOND_W25Q128JV, 4096, 0x1000000, "Winbond W25Q128JV 128 MBit" },
	{ { 0xef, 0x70, 0x15 }, WINBOND_W25Q16JV, 4096, 0x200000, "Winbond W25Q16JV 16 MBit" },
	{ { 0xef, 0x70, 0x16 }, WINBOND_W25Q32JV, 4096, 0x400000, "Winbond W25Q32JV 32 MBit" },
	{ { 0xef, 0x70, 0x17 }, WINBOND_W25Q64JV, 4096, 0x800000, "Winbond W25Q64JV 64 MBit" },
	{ { 0xef, 0x70, 0x18 }, WINBOND_W25Q128JV, 4096, 0x1000000, "Winbond W25Q128JV 128 MBit" },
	{ { 0x1f, 0x28, 0xff }, ATMEL_AT45DB642D, 1024, 0x800000, "Atmel AT45DB642D 64 MBit" },
	{ { 0x00, 0x00, 0x00 }, UNKNOWN_FLASH, 0, 0, "Unknown" },
};

static struct flash_info *flash_getinfo(uint8_t *id)
{
	struct flash_info *f= flashs;

	while (f->id[0]) {
		if ((f->id[0] == id[0]) && (f->id[1] == id[1]) &&
		    ((id[2] == 0xff) || (f->id[2] == id[2])))
			break;
		f++;
	}
	return f;
}

static int flashdetect(int fd, uint32_t *sector_size, uint32_t *flash_size, char **name)
{
	uint8_t id[3];
	int flash_type, r;
	struct flash_info *f;

	r = flash_id(fd, linknr, id);
	if (r < 0)
		return r;
	f = flash_getinfo(id);
	printf("Flash: %s\n", f->name);
	*sector_size = f->ssize; 
	*flash_size = f->fsize; 

	if (!f->id[0])
		printf("Unknown Flash Flash ID = %02x %02x %02x\n", id[0], id[1], id[2]);
	return f->type;
}

static int flash_detect(struct ddflash *ddf)
{
	uint8_t cmd = 0x9F;
	uint8_t id[3];
	int r;
	struct flash_info *f;
	
	r = flash_id(ddf->fd, ddf->link, id);
	if (r < 0)
		return r;
	f = flash_getinfo(id);
	ddf->flash_type = f->type; 
	ddf->flash_name = f->name;
	ddf->sector_size = f->ssize; 
	ddf->size = f->fsize; 
	
	if (!f->id[0]) {
		printf("Unknown Flash Flash ID = %02x %02x %02x !!!\n", id[0], id[1], id[2]);
		return -1;
	}
	if (ddf->sector_size) {
		ddf->buffer = malloc(ddf->sector_size);
		if (!ddf->buffer)
			return -1;
	}
	return 0;
}

int FlashDetect(int dev)
{
	uint32_t sector_size;
	uint32_t flash_size;
	char *name;
	
	return flashdetect(dev, &sector_size, &flash_size, &name);
}

#if 1
int flashread(int ddb, int link, uint8_t *buf, uint32_t addr, uint32_t len)
{
	int ret;
	uint8_t cmd[4];
	uint32_t l;

	while (len) {
		cmd[0] = 0x03;
		cmd[1] = (addr >> 16) & 0xff;
		cmd[2] = (addr >> 8) & 0xff;
		cmd[3] = addr & 0xff;
		
		if (len > 1024)
			l = 1024;
		else
			l = len;
		ret = flashio(ddb, link, cmd, 4, buf, l);
		if (ret < 0)
			return ret;
		addr += l;
		buf += l;
		len -= l;
	}
	return 0;
}
#else
static int flashread(int ddb, int link, uint8_t *buf, uint32_t addr, uint32_t len)
{
	uint8_t cmd[5]= {0x0b, (addr >> 16) & 0xff, 
		(addr >> 8) & 0xff, addr & 0xff, 0x00};
	
	return flashio(ddb, link, cmd, 5, buf, len);
}
#endif

void flashdump(int ddb, int link, uint32_t addr, uint32_t len)
{
	int i, j;
	uint8_t buf[32];
	int bl = sizeof(buf);
	
	for (j=0; j<len; j+=bl, addr+=bl) {
		flashread(ddb, link, buf, addr, bl);
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
    
    if ((BufferSize % 4096))
	    return -1;   // Must be multiple of sector size

    do {
        Cmd[0] = 0x50;  // EWSR
        err = flashio(dev,linknr, Cmd,1,NULL,0);
        if( err < 0 ) break;

        Cmd[0] = 0x01;  // WRSR
        Cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
        err = flashio(dev,linknr, Cmd,2,NULL,0);
        if( err < 0 ) break;

        for(i = 0; i < BufferSize; i += 4096 ) {
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
                printf(" Program %08x\n",FlashOffset + j);
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

static int flash_wait(int fd, uint32_t link)
{
	while (1) {
		uint8_t rcmd = 0x05;  // RDSR
		int err = flashio(fd, link, &rcmd, 1, &rcmd, 1);
		
		if (err < 0)
			return err;
		if ((rcmd & 0x01) == 0)
			break;
	}
	return 0;
}


int flashwrite_pagemode(struct ddflash *ddf, int dev, uint32_t FlashOffset,
			uint8_t LockBits, uint32_t fw_off, int be)
{
	int err = 0;
	uint8_t cmd[260];
	int i, j;
	uint32_t flen, blen;
	int blockerase;
	
	blen = flen = lseek(dev, 0, SEEK_END) - fw_off;
	if (blen % 0xff)
		blen = (blen + 0xff) & 0xffffff00; 
	//printf("blen = %u, flen = %u\n", blen, flen);
	setbuf(stdout, NULL);
	blockerase = be && ((FlashOffset & 0xFFFF) == 0 ) && (flen >= 0x10000);

	cmd[0] = 0x50;  // EWSR
	err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
	if (err < 0)
		return err;
	
	cmd[0] = 0x01;  // WRSR
	cmd[1] = 0x00;  // BPx = 0, Unlock all blocks
	err = flashio(ddf->fd, ddf->link, cmd, 2, NULL, 0);
	if (err < 0)
		return err;
	
	for (i = 0; i < flen; ) {
		printf(" Erase    %08x\r", FlashOffset + i);
		cmd[0] = 0x06;  // WREN
		err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		if (err < 0)
			return err;
		cmd[1] = ( (( FlashOffset + i ) >> 16) & 0xFF );
		cmd[2] = ( (( FlashOffset + i ) >>  8) & 0xFF );
		cmd[3] = 0x00;
		if (blockerase && ((flen - i) >= 0x10000) ) {
			cmd[0] = 0xd8;
			i += 0x10000;
		} else {
			cmd[0] = 0x20;  // Sector erase ( 4Kb)
			i += 0x1000;
		}
		err = flashio(ddf->fd, ddf->link, cmd, 4, NULL, 0);
		if (err < 0)
			return err;
		err = flash_wait(ddf->fd, ddf->link);
		if (err < 0)
			return err;
	}
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
		printf(" Program  %08x\r", FlashOffset + j);
		
		cmd[0] = 0x06;  // WREN
		err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
		if (err < 0)
			goto out;
		
		cmd[0] = 0x02;  // PP
		cmd[1] = ( (( FlashOffset + j ) >> 16) & 0xFF );
		cmd[2] = ( (( FlashOffset + j ) >>  8) & 0xFF );
		cmd[3] = 0x00;
		memcpy(&cmd[4], ddf->buffer, 256);
		err = flashio(ddf->fd, ddf->link, cmd, 260, NULL, 0);
		if (err < 0)
			goto out;
		err = flash_wait(ddf->fd, ddf->link);
		if (err < 0)
			goto out;
	}
	printf("\n");
	
	cmd[0] = 0x50;  // EWSR
	err = flashio(ddf->fd, ddf->link, cmd, 1, NULL, 0);
	if (err < 0)
		goto out;
	
	cmd[0] = 0x01;  // WRSR
	cmd[1] = LockBits;  // BPx = 0, Lock all blocks
	err = flashio(ddf->fd, ddf->link, cmd, 2, NULL, 0);
out:
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
		return flashwrite_pagemode(ddf, fs, addr, 0x3c, fw_off, 0);
	case SPANSION_S25FL116K: 
	case SPANSION_S25FL132K: 
	case SPANSION_S25FL164K: 
	case WINBOND_W25Q16JV:
	case WINBOND_W25Q32JV:
	case WINBOND_W25Q64JV:
	case WINBOND_W25Q128JV:
	default:
		return flashwrite_pagemode(ddf, fs, addr, 0x1c, fw_off, 1);
	}
	return -1;
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

struct devids {
	uint16_t id;
	char *name;
	char *fname;
};

#define DEV(_id, _name, _fname) { .id = _id, .name = _name, .fname = _fname }

static const struct devids ids[] = {
	DEV(0x0002, "Octopus 35", "DVBBridgeV1A_DVBBridgeV1A.bit"),
	DEV(0x0003, "Octopus", "DVBBridgeV1B_DVBBridgeV1B.fpga"),
	DEV(0x0005, "Octopus Classic", "DVBBridgeV2A_DD01_0005_STD.fpga"),
	DEV(0x0006, "CineS2 V7", "DVBBridgeV2A_DD01_0006_STD.fpga"),
	DEV(0x0007, "Octopus 4/8", "DVBBridgeV2A_DD01_0007_MXL.fpga"),
	DEV(0x0008, "Octopus 4/8", "DVBBridgeV2A_DD01_0008_CXD.fpga"),
	DEV(0x0009, "Octopus MAXSX8", "DVBBridgeV2A_DD01_0009_SX8.fpga"),
	DEV(0x000b, "Octopus MAXSX8 Basic", "DVBBridgeV2A_DD01_000B_SX8.fpga"),
	DEV(0x000a, "Octopus MAXM4", "DVBBridgeV2A_DD01_000A_M4.fpga"),
	DEV(0x0011, "Octopus CI", "DVBBridgeV2B_DD01_0011.fpga"),
	DEV(0x0012, "Octopus CI", "DVBBridgeV2B_DD01_0012_STD.fpga"),
	DEV(0x0013, "Octopus PRO", "DVBBridgeV2B_DD01_0013_PRO.fpga"),
	DEV(0x0014, "Octopus CI M2", "DVBBridgeV3A_DD01_0014_CIM2.fpga"),
	DEV(0x0020, "Octopus GT Mini", "DVBBridgeV2C_DD01_0020.fpga"),
	DEV(0x0022, "Octopus MAXM8", "DVBBridgeV3A_DD01_0022_M8.fpga"),
	DEV(0x0024, "Octopus MAXM8A", "DVBBridgeV3A_DD01_0024_M8A.fpga"),
	DEV(0x0201, "Modulator", "DVBModulatorV1B_DVBModulatorV1B.bit"),
	DEV(0x0203, "Modulator Test", "DVBModulatorV1B_DD01_0203.fpga"),
	DEV(0x0210, "Modulator V2", "DVBModulatorV2A_DD01_0210.fpga"),
	DEV(0x0220, "SDRModulator ATV", "SDRModulatorV1A_DD01_0220.fpga"),
	DEV(0x0221, "SDRModulator IQ", "SDRModulatorV1A_DD01_0221_IQ.fpga"), 
	DEV(0x0222, "SDRModulator DVBT", "SDRModulatorV1A_DD01_0222_DVBT.fpga"),
	DEV(0x0223, "SDRModulator IQ2", "SDRModulatorV1A_DD01_0223_IQ2.fpga"),
	DEV(0x0000, "UNKNOWN", 0),
};

static char *devid2fname(uint16_t devid, char **name)
{
	int i;
	char *fname = 0;

	for (i = 0; ; i++) {
		const struct devids *id = &ids[i];

		if (devid == id->id || !id->id) {
			*name = id->name;
			fname = id->fname;
			break;
		}
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
	//printf("flash file len %u, compare to %08x in flash: ", len, addr);
	for (j = 0; j < len; j += bl, addr += bl) {
		if (len - j < bl)
			bl = len - j;
		flashread(ddf->fd, ddf->link, buf, addr, bl);
		rlen = read(fs, buf2, bl);
		if (rlen < 0 || rlen != bl) {
			printf("read error\n");
			return -1;
		}
			
		if (memcmp(buf, buf2, bl)) {
			printf("flash differs at %08x (offset %u)\n", addr, j);
			//dump(buf, bl);
			//printf("\n");
			//dump(buf2, bl);
			return addr;
		}
	}
	//printf("flash same as file\n");
	return -2;
}

static uint32_t crctab[] = {
	0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
	0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
	0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
	0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
	0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
	0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
	0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
	0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
	0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
	0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
	0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
	0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
	0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
	0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
	0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
	0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
	0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
	0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
	0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
	0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
	0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
	0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
	0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
	0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
	0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
	0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
	0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
	0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
	0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
	0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
	0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
	0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4,
};

static uint32_t crc32(uint8_t *buf, uint32_t len, uint32_t crc)
{
	//uint32_t crc = 0xFFFFFFFF;
	uint32_t j;
	
	for (j = 0; j < len; j++)
		crc = ((crc << 8) ^ crctab[(crc >> 24) ^ buf[j]]);
	return crc;
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
	uint32_t maxlen = 2 * 1024 * 1024, crc, fcrc;
	
	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		printf("%s: not found\n", fn);
		return -1;
	}
	off = lseek(fd, 0, SEEK_END);
	if (off < 0)
		return -1;
	fsize = off;
#if 0
	if (fsize > maxlen) {
		close(fd);
		return -1;
	}
#endif
	lseek(fd, 0, SEEK_SET);	
	buf = malloc(fsize);
	if (!buf)
		return -1;
	read(fd, buf, fsize);
	close(fd);
	
	for (p = 0; p < fsize && buf[p]; p++) {
		char *key = (char *) &buf[p], *val = NULL;

		for (; p < fsize && buf[p] != 0x0a; p++) {
			if (buf[p] == ':') {
				buf[p] = 0;
				val = (char *) &buf[p + 1];
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
		} else if (!strcasecmp(key, "CRC")) {
			sscanf(val, "%x", &crc);
		} 
	}
	p++;
	*fw_off = p;
	fcrc = crc32(buf + p, length, 0xffffffff);
	printf("          CRC file  = %08x\n", fcrc);
	printf("          CRC flash = %08x\n", crc);
	printf("          devid     = %04x\n", devid);
	printf("          version   = %08x    (current image = %08x)\n", version, ddf->id.hw);
	//printf("        length = %u\n", length);
	//printf("fsize = %u, p = %u, f-p = %u\n", fsize, p, fsize - p);
	if (fcrc != crc) {
		printf("CRC error in file %s!\n", fn);
		return -4;
	}
	if (devid == ddf->id.device) {
		if (version < (ddf->id.hw & 0xffffff)) {
			printf("%s is older version than flash\n", fn);
			if (!ddf->force)
				ret = -3; /* same id but older newer version */
		}
		if (version == (ddf->id.hw & 0xffffff)) {
			printf("%s is same version as flash\n", fn);
			if (!ddf->force)
				ret = 2; /* same and same version */
		}
	} else
		ret = 1;

out:
	free(buf);
	//printf("check_fw = %d\n", ret);
	return ret;
	
}

static int update_image(struct ddflash *ddf, char *fn, 
			uint32_t adr, uint32_t maxlen,
			int has_header, int no_change)
{
	int fs, res = 0;
	uint32_t fw_off = 0;

	printf("File:     %s\n", fn);
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
	res = flashcmp(ddf, fs, adr, maxlen, fw_off);
	if (res == -2) {
		printf("Flash already identical to %s\n", fn);
		if (ddf->force) {
			printf("but force enabled!\n");
			res = 0;
		}
	}
	if (res < 0) 
		goto out;
	res = flashwrite(ddf, fs, adr, maxlen, fw_off);
	if (res == 0) {
		res = flashcmp(ddf, fs, adr, maxlen, fw_off);
		if (res == -2) {
			res = 1;
			printf("Flash verify OK!\n");
		} else {
			printf("Flash verify ERROR!\n");
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

