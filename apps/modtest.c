
// for systems without O_LARGEFILE
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/mod.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "../ddbridge/ddbridge-mci.h"
#include "../ddbridge/ddbridge-ioctl.h"

#define NIT_PID 0x0010
#define MAXNIT 1024
#define DEFAULT_BIT_RATE_C (50870588ULL)
#define DEFAULT_BIT_RATE_T (31668449ULL)
#define TS_SIZE (188)
#define START_FREQ_T (474000000)
#define START_FREQ_C (114000000)
static int adapt = 1;
static uint32_t start_freq = START_FREQ_C;
static int dvbt = 0;
static int writeNIT = 0;

int timest = 1;

typedef struct section_data_t {
    uint8_t *section;
    uint16_t sec_length;
    uint16_t sec_pos;
    uint8_t pack_num;
} section_data;


typedef struct transponder_{
    uint16_t tpid;
    uint8_t delsys;
    uint32_t freq;
    uint8_t qam;
    uint32_t symbolrate;
    uint8_t bandwidth;
    uint8_t guard;
    uint8_t code_rate;
    uint8_t trans_mode;
} transponder;

typedef struct write_data_t {
    int fd_in;
    int *fd_out;
    char *name;
    int chans;
    transponder tp[32];
} write_data;

#define ADAPT_FIELD    0x20
#define MAX_PCR (2576980377599ULL) // max pcr 2^33*300-1
#define PCR_FAC (2048ULL)
#define MAXPCR  (MAX_PCR*PCR_FAC)
#define PCR_FLAG       0x10
#define OPCR_FLAG      0x08

//++++++++++++++++++++++++++++++++CRC+++++++++++++++++++++++++++++++++++++
static uint32_t dvb_crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 
	0x130476dc, 0x17c56b6b,	0x1a864db2, 0x1e475005, 
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7,	0x4593e01e, 0x4152fda9, 
	0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
	0x791d4014, 0x7ddc5da3,	0x709f7b7a, 0x745e66cd, 
	0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 
	0xbe2b5b58, 0xbaea46ef,	0xb7a96036, 0xb3687d81, 
	0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 
	0xc7361b4c, 0xc3f706fb,	0xceb42022, 0xca753d95, 
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
	0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 
	0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 
	0x018aeb13, 0x054bf6a4,	0x0808d07d, 0x0cc9cdca, 
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
	0x5e9f46bf, 0x5a5e5b08,	0x571d7dd1, 0x53dc6066, 
	0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 
	0xbfa1b04b, 0xbb60adfc,	0xb6238b25, 0xb2e29692, 
	0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050,	0xe9362689, 0xedf73b3e, 
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
	0xd5b88683, 0xd1799b34,	0xdc3abded, 0xd8fba05a, 
	0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 
	0x4f040d56, 0x4bc510e1,	0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
	0x36194d42, 0x32d850f5,	0x3f9b762c, 0x3b5a6b9b, 
	0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9,	0xf8ad6d60, 0xfc6c70d7,
	0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
	0xc423cd6a, 0xc0e2d0dd,	0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
	0x9b3660c6, 0x9ff77d71,	0x92b45ba8, 0x9675461f, 
	0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 
	0x4e8ee645, 0x4a4ffbf2,	0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
	0x119b4be9, 0x155a565e,	0x18197087, 0x1cd86d30,
	0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
	0x2497d08d, 0x2056cd3a,	0x2d15ebe3, 0x29d4f654, 
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 
	0xe3a1cbc1, 0xe760d676,	0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
	0x9abc8bd5, 0x9e7d9662,	0x933eb0bb, 0x97ffad0c, 
	0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4 
};


uint32_t dvb_crc32(uint8_t *data, int len)
{
	int i;
	uint32_t crc=0xffffffff;

	for (i=0; i<len; i++)
	    crc=(crc<< 8)^dvb_crc_table[((crc>>24)^(data[i]))&0xff];
	return crc;
}

void dvb_set_crc32(uint8_t *data, int len)
{
        uint32_t crc;
	
        crc=dvb_crc32(data, len);
        data[len]   = (uint8_t)(crc>>24);
        data[len+1] = (uint8_t)(crc>>16);
        data[len+2] = (uint8_t)(crc>>8);
        data[len+3] = (uint8_t)(crc);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++++++++PCR++++++++++++++++++++++++++++++++++

uint16_t get_pid(uint8_t *pid)
{
    uint16_t pp;
    
    pp = (pid[0] & 0x1f) << 8;
    pp |= pid[1] &0xff;
    return pp;
}

void set_pid(uint16_t pid, uint8_t *p)
{
    p[0] = (p[0] & 0xE0) | ((uint8_t) (pid >> 8 ) & 0x1F);
    p[1] = (uint8_t) (pid);
}

int pcr_in_msecs(uint64_t pcr)
{
    uint64_t PCR;
    
    PCR = pcr/PCR_FAC;

    return PCR/27000;
}


int check_pcr(uint8_t *buf)
{
    
    if (buf[0] != 0x47) {
	fprintf(stderr,"Not a TS packet header in check_pcr\n");
	return -1;
    }

    if (buf[1] & 0x80) {
	fprintf(stderr,"Corrupt packet in check_pcr\n");
	return -1;
    }

    if (!(buf[3] & ADAPT_FIELD)){
//	fprintf(stderr,"no adaptation field\n");
	return 0;	//no adapt. field 
    }
    if (!buf[4]){
//	fprintf(stderr,"no adaptation field length\n");
	return 0; //No adaptation field length
    }
    if (! (buf[5] & PCR_FLAG) ){
//	fprintf(stderr,"no PCR flag\n");
	return 0; // no PCR FLAG
    }
//    fprintf(stderr,"Found pcr\n");
    return 1;
}

uint8_t *get_pcr(uint8_t *buf)
{
    return buf+6;
}

uint64_t convert_pcr(uint8_t *b){
	uint64_t pcr;

	pcr = ((uint64_t) b[0] << 25) | ((uint64_t) b[1] << 17) |
		((uint64_t) b[2] << 9) | ((uint64_t) b[3] << 1) |
		((uint64_t) b[4] >> 7);
	pcr *= 300;
	pcr += ((b[4] & 1) << 8) | b[5];
	return pcr*PCR_FAC;
}

int find_pcr(uint8_t *buf, int l, int last)
{
    int c = 0;
    while (buf[c] != 0x47) c++;

    if (last){
	c+=((l-c)/TS_SIZE-1)*TS_SIZE;
	while (buf[c] != 0x47) c--;
	while (c >=0 && !check_pcr(buf+c)) c-=TS_SIZE;
	if (check_pcr(buf+c)) return c;
	else return -1;
    } else {
	while (c < l && !check_pcr(buf+c)) c+=TS_SIZE;
	if (check_pcr(buf+c)) return c;
	else return -1;
    }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++++++++TS PACKETS+++++++++++++++++++++++++++

void write_filler(uint8_t *buf)
{
    int j;
    uint8_t *p;

    p = buf;
    *p++ = 0x47;
    *p++ = 0x1F;
    *p++ = 0xFF;
    *p++ = 0x00;
    for(j = 0; j < TS_SIZE-4; j+=1 ) *p++ = 0x00;    
}

void setbcd(uint8_t *p, int freq, int b, int e)
{
    int i;
    int f = freq/e;
    int r = b%2;

    if (r){
	p[b/2] = ( (f % 10) >> 4) & 0xF0;
	f = f / 10;
    }
    for ( i= b/2; i > 0; i--){
	p[i-1] = (f % 10) & 0x0F;
	f = f / 10;
	p[i-1] |= ( (f % 10) << 4) & 0xF0;
	f = f / 10;
    }
}

static int write_cable_delsys_descriptor (uint8_t *buf, int blength,
					  transponder *tp)
{
    int length = 0;
    uint8_t *p;
    
    p = buf;
    length = 11;

    if (length > (int)(0xFF) || length+2 >blength){
	fprintf(stderr,"Not enough space for cable delsys descriptor %d, %d\n",
	       length, blength);
	return -1;
    }
    p[0]= 0x44; // cable delivery system descriptor
    p[1]= 0x0B; // length always 11
    setbcd(p+2, tp->freq, 8, 100);
    p[6]  = 0xFF; // reserved
    p[7]= 0x02; // FEC_outer
    p[8]= tp->qam; // QAM 
    setbcd(p+9,tp->symbolrate, 7, 100);
    p[12]|= 0x0F; 
    
    return length+2;
}

static int write_terrestrial_delsys_descriptor (uint8_t *buf, int blength,
						transponder *tp)
{
    int length = 0;
    uint8_t *p;
    uint32_t freq = tp->freq/10;
    p = buf;
    length = 11;

    if (length > (int)(0xFF) || length+2 >blength){
	fprintf(stderr,"Not enough space for cable delsys descriptor %d, %d\n",
	       length, blength);
	return -1;
    }
    p[0]= 0x5a; // terretrial delivery system descriptor
    p[1]= 0x0B; // length always 11
    p[2]=  (freq >> 24) & 0xFF;
    p[3]=  (freq >> 16) & 0xFF;
    p[4]=  (freq >> 8) & 0xFF;
    p[5]=  freq & 0xFF;
    p[6]= ((tp->bandwidth&0x7)<<5) |0x1f;
                //bandwidth 8MHz 000,no priority 1,
                //no time slice 1, no mpe-fce 1
    p[7]= 0x00 |((tp->qam & 0x03) << 6) | (tp->code_rate &0x07);
                //64-QAM 10, no hierarchy 000, 7/8 100
    p[8]= 0x00 | ((tp->guard &0x03) <<3 )|((tp->trans_mode &0x03) << 1);
             // no LP 000, 1/32 guard 00, 8k mode 01, no other freq 0

    p[9]=0xff;
    p[10]=0xff;
    p[11]=0xff;
    p[12]=0xff;
    
    return length+2;
}

int CreateNIT(uint8_t *NIT, transponder *tp, int start, int stop,
	      char *nname, uint16_t nid, uint8_t secnum, uint8_t lastsec)
{
    uint8_t *p,*lp;
    int c = 0;
    int cc;
    int i;
    uint16_t TableLen = 0;
    int slen;

    p = NIT;

    p[0] = 0x40;  // Table Id NIT = 0x40   

    p[1] = 0x00;  // Table length determined at end
    p[2] = 0x00;

    p[3]  = (nid >> 8) & 0xFF; // network id 
    p[4]  = nid & 0xFF; 

    p[5]  = 0xC3;  // seq number
    p[6]  = secnum;  // section number
    p[7]  = lastsec;  // last section number


// Network descriptor
    p[8]  = 0xF0;  // network descriptor length
    p[9]  = 0x00;  // set later
    TableLen = 7;
    c = 10;
    p[c] = 0x40;  // Network name descriptor

    slen = 0;

    slen = strlen(nname);
    p[c+1] = slen;
    memcpy(p+c+2,nname,slen);

    p[8] = 0xF0 | (((2+slen) >> 8) &0x0F);
    p[9] = ((2+slen) & 0xFF);
    TableLen += 2+slen;
    c += 2+slen;


// Transport stream loop
    p[c] = 0xF0;  // transport stream loop length
    p[c+1] = 0x00; // set later
    cc = c+1;

    TableLen += 2;
    c += 2;
    slen = 0;
    for (i= start; i < stop; i++){
	p[c]  = (uint8_t)( tp[i].tpid >> 8 ); //transport stream id
	p[c+1]= (uint8_t)( tp[i].tpid );
	p[c+2]= (nid >> 8) &0xFF;  // orig network id
	p[c+3]= nid & 0xFF;  // id

	lp = p+c+4;
	p[c+4]= 0xF0;  // descriptor loop length
	p[c+5]= 0x0D;  // 13 for cable delivery descriptor
	c += 6;
	switch (tp->delsys){
	case  SYS_DVBC_ANNEX_A:
	    slen = write_cable_delsys_descriptor (p+c, MAXNIT-c, &tp[i]);
	    break;
	case SYS_DVBT:
	    slen = write_terrestrial_delsys_descriptor (p+c, MAXNIT-c, &tp[i]);
	    break;
	default:
	    slen = 0;
	}
	c+=slen;
	
	lp[0] = 0xF0 | (((slen) >> 8) &0x0F);
	lp[1] = ((slen) & 0xFF);

	
	TableLen += 6+slen;
    }
    
    p[cc-1]= 0xF0 | (((c-cc-1) >> 8) &0x0F);
    p[cc]  = ((c-cc-1) & 0xFF);

    TableLen += 4;

    p[1]    = 0xF0 | ((TableLen >> 8) & 0x0F);
    p[2]  = (uint8_t)(TableLen) & 0xFF;
    

    dvb_set_crc32(NIT, TableLen + 3 -4 );
    return TableLen+3;

}

void init_sec_data(section_data *s, uint8_t *sec, uint16_t seclength)
{
    uint16_t seclen = 0;

    s->section = sec;
    seclen |= ((sec[1] & 0x0F) << 8); 
    seclen |= (sec[2] & 0xFF);
    seclen += 3;
    if (seclen > 3){
	s->sec_length = seclen;
	if(seclength && seclen != seclength) 
	    fprintf(stderr,"Setting seclength (%d) %d\n",seclength, seclen);
    } else s->sec_length = seclength;

    s->sec_pos = 0;
    s->pack_num = 0;
}

void write_sec_pack(section_data *s, uint8_t *ts_pack, uint16_t pid)
{
    uint8_t *p = ts_pack;
    uint16_t rest = 0;
    uint16_t len = s->sec_length - s->sec_pos;
    uint16_t pload = 0;

    *p++ = 0x47;
    set_pid(pid,p);
    if (!s->sec_pos){
	*p |= 0x40;
    } else {
	*p &= ~0x40;
    }
    p += 2;
    *p++ = 0x10 | (s->pack_num & 0x0f);
    s->pack_num += 1;
    if (!s->sec_pos) *p++=0x00;
    pload = TS_SIZE - (p - ts_pack);
    if (pload > len) pload = len;
    memcpy( p, s->section + s->sec_pos, pload);
    p += pload;
    rest = TS_SIZE - (p - ts_pack);
    if (rest > 0)
	memset(p, 0xff, rest);
    s->sec_pos += pload;
    if (s->sec_pos == s->sec_length){
	s->sec_pos = 0;
    } 
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++MAIN ROUTINE++++++++++++++++++++++++++++

#define OBSIZE (672*TS_SIZE)
#define BSIZE (7*OBSIZE)

void write_mods(write_data *wd)
{
    uint8_t NIT[MAXNIT];
    section_data sec;
    off64_t inflength,c=0;
    uint8_t buffer[BSIZE];
    uint8_t obuffer[OBSIZE];
    int ci=0,w=0;
    int isdvr=0;
    int first = 0;
    int last = 0;
    uint64_t fpcr = 0;
    uint64_t lpcr = 0;
    uint64_t delta_pcr = 0;
    uint64_t in_prate, out_prate;
    int slen = 0;

    if (dvbt){
	out_prate = DEFAULT_BIT_RATE_T / (8*TS_SIZE);
    } else {
	out_prate = DEFAULT_BIT_RATE_C / (8*TS_SIZE);
    }
    slen =  CreateNIT(NIT, wd->tp, 0, wd->chans, "DD", 1, 0, 0);

    init_sec_data(&sec, NIT, slen);
    in_prate = 0;
    
    inflength = lseek64(wd->fd_in, 0, SEEK_END);
    inflength = TS_SIZE*(inflength/TS_SIZE);
    if (inflength <= 0){
        if (inflength == 0 || errno == ESPIPE){
            isdvr = 1;
            fprintf(stderr,"Non seekable file, handling as dvr\n");
        } else {
            fprintf(stderr,"Error in lseek, aborting\n");
            return;
        }
    }
    
    if (!isdvr){
        fprintf(stderr,"Input file length: %.2f MiB\n",inflength/1024./1024.);
        lseek64(wd->fd_in,0,SEEK_SET);
    } 
    
    fprintf(stderr,"Starting %s\n",wd->name);
    while (1) {
	int wc=0;
	first = 0;
	last = 0;

	if ((ci=read(wd->fd_in,buffer,BSIZE))<0) return;
	
	first = find_pcr(buffer, ci, 0);
	last = find_pcr(buffer, ci, 1);
	if (first == last){
	    fprintf(stderr,"Input buffer too small\n" 
		    "first %d last %d length %d  pidf 0x%x\n", first/TS_SIZE,
		    last/TS_SIZE,
		    ci/TS_SIZE, get_pid(buffer+first+1));
	}
	fpcr = convert_pcr(get_pcr(buffer+first));
	lpcr = convert_pcr(get_pcr(buffer+last));
	if (lpcr > fpcr) delta_pcr = lpcr - fpcr;
	else delta_pcr = (MAXPCR-fpcr)+lpcr;
	int packs = (last-first)/TS_SIZE;
	in_prate = ((packs*10000/pcr_in_msecs(delta_pcr))+5)/10; 

#if 0
	fprintf(stderr,"delta_pcr %d ms npack %d packet_rate in: %d out: %d\n",
 		pcr_in_msecs(delta_pcr),
	        packs, in_prate, out_prate); 
	fprintf(stderr,"Input bitrate %.2f MBit output bitrate %.2f MBit\n",
		(in_prate*TS_SIZE*8)/1000000.0,
		(out_prate*TS_SIZE*8)/1000000.0);
#endif	

	if (in_prate > out_prate){
	    fprintf(stderr,"Input bitrate (%.2f MBit) larger than "
		    "possible output bitrate (%.2f MBit)\n",
		    (in_prate*TS_SIZE*8)/1000000.0,
		    (out_prate*TS_SIZE*8)/1000000.0);
	    exit(1);
	}

	int filler = (out_prate*10/in_prate+5)/10-1;
	int d=0;
	int rest=0;
	for (int s = 0; s < ci; s+=TS_SIZE){
	    memcpy(obuffer+d, buffer+s, TS_SIZE*sizeof(uint8_t));
	    d+=TS_SIZE;
	    int fill = (OBSIZE - d)/TS_SIZE;
	    if (fill < filler) rest = filler - fill;
	    else fill = filler;
//	    fprintf(stderr,"d %d fill %d filler %d %d %d\n", d, fill,filler,s,ci);
	    int nit = 0;
	    for(int i=0;i < fill; i++){
		if (writeNIT && nit < 4 ) {
		    write_sec_pack(&sec, obuffer+d, NIT_PID);
		    nit++;
		} else 	write_filler(obuffer+d);
		d+=TS_SIZE;
	    }
	    if (d >= OBSIZE){
		for (int m = 0; m < wd->chans; m++){
		    int lc = 0;
		    while (lc < OBSIZE){
			if ((w=write(wd->fd_out[m],obuffer+lc,OBSIZE-lc))<0){
			    fprintf(stderr,"Problem writing to modulator from %s (%d %d) %s\n",
				    wd->name, w, ci,strerror(errno));
			    return;
			}
			wc += w;
			lc+=w;
			d = 0;
		    }
		}
	    }
	    
	    for(int i=0;i < rest; i++){
		write_filler(obuffer+d);
		d+=TS_SIZE;
	    }
	    rest = 0;
	}
        if (inflength){
           c+=ci;
	   fprintf(stderr,"written %d %d%%\n",(int)c,(int)((100*c)/inflength));
	   if (c  >=inflength ){
	       fprintf(stderr,"Restarting stream %s\n",wd->name);
	       lseek64(wd->fd_in,0,SEEK_SET);
	       c = 0;
	   }
        }
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//+++++++++++++++++++++++++++++++++DVB DEVICES++++++++++++++++++++++++++++

#define DVB_TUNE 0
#define DVB_MOD 1
#define DVB_CA 2
#define MAX_ADAPT 32
 
typedef struct dvb_devices_t {
    int adapters;
    int dtype[MAX_ADAPT];
    int ndevs[MAX_ADAPT];
    uint64_t snr[MAX_ADAPT];
    uint64_t hwid[MAX_ADAPT];
    uint64_t devid0[MAX_ADAPT];
} dvb_devices;


static int check_tuner(int adapt)
{
  char device[35];
  int front = 0;
  int done = 0;

  while (!done) {
      snprintf(device,34,"/dev/dvb/adapter%d/frontend%d",adapt,front);
//      fprintf(stderr,"Checking for %s\n", device);
      if(access(device, F_OK) < 0)
          done=1;
      else {
	  front++;
      }
  }
  return front;
}

static int check_ca(int adapt)
{
  char device[25];
  int ca = 0;
  int done = 0;

  while (!done) {
      snprintf(device,24,"/dev/dvb/adapter%d/ci%d",adapt,ca);
      if(access(device, F_OK) < 0)
          done=1;
      else {
          ca++;
      }
  }
  return ca;
}

static int check_modulator(int adapt)
{
  char device[25];
  int mod = 0;
  int done = 0;

  while (!done) {
      snprintf(device,24,"/dev/dvb/adapter%d/mod%d",adapt,mod);
      if(access(device, F_OK) < 0)
          done=1;
      else {
	  mod++;
      }
  }
  return mod;
}


static int check_dvb(dvb_devices *ddevices){
	int done = 0;
	int nadapt = 0;
	int maxmod = 0;
	int maxfront = 0;
	int maxca = 0;
	int i;
	int adapter =0;
	
	while ( nadapt < MAX_ADAPT && !done){
		maxca = check_ca(nadapt);
		maxmod = check_modulator(nadapt);
		maxfront = check_tuner(nadapt);

		if (maxmod){
			ddevices->dtype[nadapt] = DVB_MOD;
			ddevices->ndevs[nadapt] = maxmod;
			maxmod = 0;
			nadapt++;
		} else {
			if (maxfront) {
				ddevices->dtype[nadapt] = DVB_TUNE;
				ddevices->ndevs[nadapt] = maxfront;
				maxfront = 0;
				nadapt++;
			} else {
			        if (maxca) {
				     ddevices->dtype[nadapt] = DVB_CA;
				     ddevices->ndevs[nadapt] = maxca;
				     maxca = 0;
				     nadapt++;
				} else {
				      done = 1;
				}
			}
		}
	}
	ddevices->adapters = nadapt;
	if (!nadapt) return -1;

	fprintf(stderr,"Found %d dvb adapters\n",nadapt);
	for (i=0; i<nadapt; i++){
/*
	    if (check_sysfs(i,ddevices) < 0){
		return -1;
	    }
*/
	    switch(ddevices->dtype[i]){
	    case DVB_TUNE:
		fprintf(stderr,"  Adapter %d is a TUNER CARD with %d FRONTENDS\n", i, ddevices->ndevs[i]);
		break;
	    case DVB_MOD:
		fprintf(stderr,"  Adapter %d is a MODULATOR CARD with %d MODULATORS\n", i, ddevices->ndevs[i]);
		adapter = i;
		break;
	    case DVB_CA:
		fprintf(stderr,"  Adapter %d is a CI CARD with %d CA \n",i,ddevices->ndevs[i]);
		break;
	    }
	}
	return adapter;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++DVBT MOD++++++++++++++++++++++++++++++++

static int mci_cmd(int dev, struct mci_command *cmd)
{
	int ret;
	struct ddb_mci_msg msg;
	uint8_t status;

	memset(&msg, 0, sizeof(msg));
	msg.link = 0;
	memcpy(&msg.cmd, cmd, sizeof(msg.cmd));
	//dump((uint8_t *) &msg.cmd, sizeof(msg.cmd));
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		dprintf(2, "mci_cmd error %s\n",strerror(errno));
		return ret;
	}
	status = msg.res.status;
	if (status == MCI_STATUS_OK)
		return ret;
	if (status == MCI_STATUS_UNSUPPORTED) {
		dprintf(2, "Unsupported MCI command\n");
		return ret;
	}
	if (status == MCI_STATUS_INVALID_PARAMETER) {
		dprintf(2, "Invalid MCI parameters\n");
		return ret;
	}
	return ret;
}

int mci_set_output(int fd, uint8_t connector, uint8_t nchannels, uint8_t unit,
		    int16_t power)
{
    char con[14];
    char un[6];
    struct mci_command msg_output = {
	.mod_command = MOD_SETUP_OUTPUT,
	.mod_channel = 0,
	.mod_stream = 0,
	.mod_setup_output = {
	    .connector = MOD_CONNECTOR_F,
	    .num_channels = 14,
	    .unit = MOD_UNIT_DBUV,
	    .channel_power = 9000,
	},
    };

    msg_output.mod_setup_output.connector = connector;
    msg_output.mod_setup_output.num_channels = nchannels;
    msg_output.mod_setup_output.unit = unit;
    msg_output.mod_setup_output.channel_power = power;


    switch (connector){
    case MOD_CONNECTOR_F:
	snprintf(con, 14, "F-Connector");
	break;
	
    case MOD_CONNECTOR_SMA:
	snprintf(con, 14, "SMA-Connector");
	break;

    case MOD_CONNECTOR_OFF:
	snprintf(con, 14, "off");
	break;	

    default:
	fprintf(stderr,"unknown connector in modulator setup\n");
	return -1;
	break;
    }
    switch (unit){
    case MOD_UNIT_DBUV:
	snprintf(un, 6, " dBuV");
	break;

    case MOD_UNIT_DBM:
	snprintf(un, 6, " dBm");
	break;

    default:
	fprintf(stderr,"unknow power unit in modulator setup\n");
	return -1;
	break;
    }
    
    fprintf(stderr,"Setting DVBT Modulator output to %s, %d channels, power %f%s\n",
	   con, nchannels, (double)power/100, un );

    return mci_cmd(fd,&msg_output);
}

int mci_set_output_simple(int adapt, uint8_t nchannels)
{
    char fn[128];
    int re = 0;
    
    snprintf(fn, 127, "/dev/dvb/adapter%u/mod0", adapt);
    int fd = open(fn, O_RDWR);
    if (fd < 0) {
	fprintf(stderr, "Could not open %s\n", fn);
	return -1;
    }

    re = mci_set_output(fd, MOD_CONNECTOR_F, nchannels, MOD_UNIT_DBUV, 9000);
    close(fd);
    return re;
}

int mci_set_channels(int fd, uint32_t freq, uint8_t nchan, uint8_t standard,
		     uint32_t offset, uint32_t bandw)
{
    char stand[25];
    struct mci_command msg_channels = {
	.mod_command = MOD_SETUP_CHANNELS,
	.mod_channel = 0,
	.mod_stream = 0,
	.mod_setup_channels[0] = {
	    .flags = MOD_SETUP_FLAG_FIRST|MOD_SETUP_FLAG_LAST|MOD_SETUP_FLAG_VALID,
	    .standard = MOD_STANDARD_DVBT_8,
	    .num_channels = 25,
	    .frequency = 474000000,
	},
    };

    msg_channels.mod_setup_channels[0].frequency = freq;
    msg_channels.mod_setup_channels[0].num_channels = nchan;
    msg_channels.mod_setup_channels[0].standard = standard;
    if (standard == MOD_STANDARD_GENERIC){
	msg_channels.mod_setup_channels[0].offset = offset;
	msg_channels.mod_setup_channels[0].bandwidth = bandw;
    }

    switch(standard){
    case MOD_STANDARD_GENERIC:
	snprintf(stand, 24, "MOD_STANDARD_GENERIC");
	break;

    case MOD_STANDARD_DVBT_8:
	snprintf(stand, 24, "MOD_STANDARD_DVBT_8");
	break;

    case MOD_STANDARD_DVBT_7:
	snprintf(stand, 24, "MOD_STANDARD_DVBT_7");
	break;

    case MOD_STANDARD_DVBT_6:
	snprintf(stand, 24, "MOD_STANDARD_DVBT_6");
	break;

    case MOD_STANDARD_DVBT_5:
	snprintf(stand, 24, "MOD_STANDARD_DVBT_5");
	break;

    default:
	fprintf(stderr,"unknown standard in channels setup\n");
	return -1;
	break;	

    }
    fprintf(stderr,"Setting DVBT Modulator channels to %d HZ, %d channels, %s\n",
	   freq, nchan, stand);
 
    return mci_cmd(fd,&msg_channels);
}

int mci_set_channels_simple(int adapt, uint32_t freq, uint8_t nchan)
{

    char fn[128];
    int re = 0;
    
    snprintf(fn, 127, "/dev/dvb/adapter%u/mod0", adapt);
    int fd = open(fn, O_RDWR);
    if (fd < 0) {
	fprintf(stderr, "Could not open %s\n", fn);
	return -1;
    }

    re = mci_set_channels(fd, freq, nchan, MOD_STANDARD_DVBT_8, 0, 0);
    close(fd);
    return re;
}

int mci_set_stream( int fd, uint8_t stream, uint8_t channel, uint8_t standard,
		    uint8_t stream_format, uint32_t symbol_rate,
		    uint8_t modulation, uint8_t rolloff,
		    uint8_t fft_size, uint8_t guard_interval,
		    uint8_t puncture_rate, uint8_t constellation,
		    uint16_t cell_identifier)
{
    struct mci_command msg_stream = {
	.mod_command = MOD_SETUP_STREAM,
	.mod_channel = 0,
	.mod_stream = 0,
	.mod_setup_stream = {
	    .standard = MOD_STANDARD_DVBC_8,
	},
    };
 
    msg_stream.mod_channel = channel; 
    msg_stream.mod_stream = stream;
    msg_stream.mod_setup_stream.standard = standard;
    msg_stream.mod_setup_stream.stream_format = stream_format;
    if (symbol_rate)
	msg_stream.mod_setup_stream.symbol_rate = symbol_rate;
    if (modulation)
	msg_stream.mod_setup_stream.qam.modulation = modulation;
    if (rolloff)
	msg_stream.mod_setup_stream.qam.rolloff = rolloff;    
    msg_stream.mod_setup_stream.ofdm.fft_size = fft_size;
    msg_stream.mod_setup_stream.ofdm.guard_interval = guard_interval;
    msg_stream.mod_setup_stream.ofdm.puncture_rate = puncture_rate;
    msg_stream.mod_setup_stream.ofdm.constellation = constellation;
    if (cell_identifier)
	msg_stream.mod_setup_stream.ofdm.cell_identifier = cell_identifier;

    fprintf(stderr,"Setting DVBT Stream %d to channel %d\n",stream, channel);
 
    return mci_cmd(fd,&msg_stream);
    
}

void set_dvbt_mods(int adapt, int chans, uint32_t start_freq, write_data *wd)
{
    if ((mci_set_output_simple(adapt, chans) < 0)||
	(mci_set_channels_simple(adapt, start_freq, chans)< 0))
    {
	fprintf(stderr,"Error setting up DVBT Modulator\n");
	exit(1);
    }
    wd->chans = chans;
    wd->fd_out = (int *)malloc(chans*sizeof(int));
    memset(wd->fd_out,0,chans*sizeof(int));

    for (int i = 0; i < chans; i++){
	char *device;
	int fd;
	
	wd->tp[i].tpid = 1; // all the same transport stream  id  
	wd->tp[i].delsys = SYS_DVBT;
	wd->tp[i].freq = start_freq+8000000*i;
	wd->tp[i].qam = 2;
	wd->tp[i].symbolrate = 0;
	wd->tp[i].bandwidth = 0;
	wd->tp[i].guard = 0;
	wd->tp[i].code_rate = 4;
	wd->tp[i].trans_mode = MOD_STANDARD_DVBT_8;
	
 	device = malloc(sizeof(char)*40);
	snprintf(device,35,"/dev/dvb/adapter%d/mod%d",adapt,i);
	fd = open(device, O_RDWR);
	if( fd < 0 )
	{
	    fprintf(stderr,"Error opening %s : %s\n",device,strerror(errno));
	    free(device);
	    exit(1);   
	}
	
	mci_set_stream( fd, i, i, MOD_STANDARD_DVBT_8, 4, 0, 0, 0, 1, 0, 4, 2, 0);
	close(fd);
	free(device);
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++DVBC MOD++++++++++++++++++++++++++++++++

static int set_property(int fd, uint32_t cmd, uint32_t data)
{
        struct dtv_property p;
        struct dtv_properties c;
        int ret;

        p.cmd = cmd;
        c.num = 1;
        c.props = &p;
        p.u.data = data;
        ret = ioctl(fd, FE_SET_PROPERTY, &c);
        if (ret < 0) {
                fprintf(stderr, "FE_SET_PROPERTY returned %d\n", errno);
                return -1;
        }
        return 0;
}

static int set_input_bitrate(int fd, uint64_t data)
{
        struct dtv_property p;
        struct dtv_properties c;
        int ret;

        p.cmd = MODULATOR_INPUT_BITRATE;
        c.num = 1;
        c.props = &p;
        p.u.data64 = data;
        ret = ioctl(fd, FE_SET_PROPERTY, &c);
        if (ret < 0) {
                fprintf(stderr, "FE_SET_PROPERTY returned %d\n", errno);
                return -1;
        }
        return 0;
}

void set_dvbc_mods(int adapt, int chans, uint32_t start_freq, write_data *wd)
{
    uint32_t freq = start_freq;
    uint8_t qam = QAM_256;
    uint32_t sym = 6900000;

    wd->chans = chans;
    wd->fd_out = (int *)malloc(chans*sizeof(int));
    memset(wd->fd_out,0,chans*sizeof(int));
    
    for (int i = 0; i < chans; i++){
	char *device;
	int fd;
	
	wd->tp[i].tpid = i; 
	wd->tp[i].delsys = SYS_DVBC_ANNEX_A;
	wd->tp[i].freq = freq;
	wd->tp[i].qam = qam;
	wd->tp[i].symbolrate = sym;
	wd->tp[i].bandwidth = 0;
	wd->tp[i].guard = 0;
	wd->tp[i].code_rate = 0;
	wd->tp[i].trans_mode = 0;
	
	device = malloc(sizeof(char)*40);
	snprintf(device,35,"/dev/dvb/adapter%d/mod%d",adapt,i);
	fd = open(device, O_RDWR);
	if( fd < 0 )
	{
	    fprintf(stderr,"Error opening %s : %s\n",device,strerror(errno));
	    free(device);
	    exit(1);   
	}
	if (set_property(fd, MODULATOR_FREQUENCY, freq) < 0){
	    fprintf(stderr,"setting freq %d failed\n",freq);
	    exit(1);   
	}
	if (set_property(fd, MODULATOR_MODULATION, qam) < 0){
	    fprintf(stderr,"setting qam %d failed\n",qam);
	    exit(1);   
	}
	if (set_property(fd, MODULATOR_SYMBOL_RATE, sym) < 0){
	    fprintf(stderr,"setting sym %d failed\n",sym);
	    exit(1);   
	}

	if (set_input_bitrate(fd, (DEFAULT_BIT_RATE_C << 32)) < 0){
	    fprintf(stderr,"setting bitrate %d failed\n",
		    (int)DEFAULT_BIT_RATE_C);
	    exit(1);   
	}
     
	freq += 8000000;
	close(fd);
	free(device);
    }
}

//+++++++++++++++++++++++++++++CLI++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

static void usage(char *progname)
{
    printf ("usage: %s [options] <input files>\n\n",progname);
    printf ("options:\n");
    printf ("  --adapter <n>, -a <n>  :  adapter number <n> of modulator card (defaults to first found)\n");
    printf ("  --mods <n>,    -m <n>  :  number <n> of modulators to use (default all)\n");
    printf ("  --file,        -i      :  input filename (default test.ts)\n");
    printf ("  --frequency,   -f      :  start frequency in MHz (default DVB_C 114MHz, DVB-T 474MHz)\n"); 
    printf ("  --dvbt,        -t      :  modulator is DVB-T\n");
    printf ("  --NIT,         -n      :  write a minimal NIT for faster scan\n");
    printf ("  --help,        -h      :  print help message\n");
    printf ("\n");
    printf ("\n");
    exit(1);
}


static int parse_cl(int argc, char * const argv[], char **filename, int *chans)
{
    int c;
    int fset = 0;

    *filename = strdup("test.ts");

    writeNIT = 0;
    dvbt = 0;
    while (1){
	int option_index = 0;
	static struct option long_options[] = {
	    {"help", no_argument , NULL, 'h'},
	    {"dvbt", no_argument , NULL, 't'},
	    {"NIT", no_argument , NULL, 'n'},
	    {"adapter", required_argument , NULL, 'a'},
	    {"mods", required_argument , NULL, 'm'},
	    {"file", required_argument , NULL, 'i'},
	    {"frequency", required_argument , NULL, 'f'},
	    {NULL, 0, NULL, 0}
	};

	c = getopt_long (argc, argv, "ha:i:f:ntm:",long_options, 
			 &option_index);
	
	if (c == -1)
	    break;

	switch (c){
	case 'a':
	    adapt = (int)strtol(optarg,(char **)NULL, 0);
	    break;

	case 'm':
	    *chans = (int)strtol(optarg,(char **)NULL, 0);
	    break;

	case 'f':
	    fset = 1;
	    start_freq = strtoul(optarg, NULL, 0)*1000000;
	    break;
	    
	case 'i':
            if (*filename){
		free(*filename);
            }
            *filename = strdup(optarg);
	    break;
	    
	case 'n':
	    writeNIT = 1;
	    break;

	case 't':
	    if (!fset) start_freq = START_FREQ_T;
	    dvbt = 1;
	    break;
	    
	case 'h':
	case '?':
	default:
	    usage(argv[0]);
	    
	}
    }

    return adapt;

}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++MAIN++++++++++++++++++++++++++++++++

int main(int argc, char **argv)
{
    const char *progn = "modtest";
    
    int maxout = 0;
    int mods = 0;
    char *filename;
    char *device;
    dvb_devices ddevices;
    write_data wd;
    int fd_out;
    int fd_in;
    int chans = 0;

    printf("%s \n\n", progn);
    adapt = parse_cl(argc, argv, &filename, &chans);
    if (check_dvb(&ddevices)<0){
	fprintf(stderr,"No DVB devices found\n");
	exit(1);
    }

    for (int i=0; i<ddevices.adapters; i++){
	if ( ddevices.dtype[i] == DVB_MOD ) {
	    mods++;
	    maxout+= ddevices.ndevs[i];
	}    
    }
    
    if ( !mods || !maxout ){
	fprintf(stdout,"No Modulator device found\n");
	exit(1);
    }
    fprintf(stdout,"Found %d modulator devices with %d mods\n",mods, maxout);

    if (ddevices.dtype[adapt] == DVB_MOD){
	fprintf(stdout,"Using adapter %d\n",adapt);
    } else {
	fprintf(stdout,"Adapter %d is not a modulator\n",adapt);
	int i=0;
	while (i< ddevices.adapters && ddevices.dtype[i] != DVB_MOD){
	    i++;
	}
	adapt = i;
	fprintf(stdout,"Using adapter %d instead\n",adapt);
    }
    
    if ((fd_in = open(filename ,O_RDONLY| O_LARGEFILE)) < 0){
	fprintf(stderr,"\nError opening input file:%s\n",filename);
	if (!strncmp(filename,"test.ts",7)) {
	    fprintf(stderr,"\nYou can create the default input file test.ts\n");
	    fprintf(stderr,"by executing: \"make test.ts\"\n");
	}
	exit(1);
    }
    
    if (chans<1 || chans >ddevices.ndevs[adapt] )
	chans = ddevices.ndevs[adapt];

    if (dvbt){
	set_dvbt_mods(adapt, chans, start_freq, &wd);
    } else {
	set_dvbc_mods(adapt, chans, start_freq, &wd);
    }
 
    fprintf(stderr,"Reading from %s\n", filename);
    device = malloc(sizeof(char)*40);
    for (int m= 0; m < chans; m++){
	snprintf(device,35,"/dev/dvb/adapter%d/mod%d",adapt,m);
	fd_out = open(device, O_WRONLY);
	wd.fd_out[m] = fd_out;
	wd.fd_in = fd_in;
	wd.name = filename;
    }
    write_mods(&wd);

    exit(0);
}
