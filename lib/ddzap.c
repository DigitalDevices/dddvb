#include "../include/linux/dvb/frontend.h"
#include "src/libdddvb.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

char line_start[16] = "";
char line_end[16]   = "\r";


uint32_t cc_errors = 0;
uint32_t packets = 0;
uint32_t payload_packets = 0;
uint32_t packet_errors = 0;

uint8_t cc[8192] = { 0 };

enum { IQ_RED=1, IQ_GREE, IQ_BLUE , IQ_EVIL, IQ_LOG_RED, IQ_LOG_GREEN, IQ_LOG_BLUE , IQ_LOG_EVIL , IQ_TEST, };

typedef struct pamdata_
{
    unsigned char *data_points;
    uint64_t *data;
    int col;
} pamdata;

int init_pamdata(pamdata *iq,int color)
{
    iq->col = 0;
    if (!( iq->data=(uint64_t *) malloc(sizeof(uint64_t) *256*256)))
    {
        fprintf(stderr,"not enough memory\n");
        return -1;
    }
    memset(iq->data,0,256*256*sizeof(uint64_t));
    if (!( iq->data_points=(unsigned char *) malloc(sizeof(unsigned char) *
                                             256*256*3)))
    {
        fprintf(stderr,"not enough memory\n");
        return -1;
    }
    memset(iq->data_points,0,256*256*3*sizeof(char));
    iq->col = color;
    return 0;
}

static long getutime(){
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC_RAW,&t0);
        return t0.tv_sec * (int)1e9 + t0.tv_nsec;
}

void pam_coordinate_axes(pamdata *iq, unsigned char r,
                         unsigned char g, unsigned char b){
    int i;
    for (i = 0; i < 256*3; i+=3){
        // coordinate axes
        int xr = i    + 256*128*3;
        int yr =128*3 + i*256;
        iq->data_points[xr] = r;
        iq->data_points[yr] = r;
        iq->data_points[xr+1] = g;
        iq->data_points[yr+1] = g;
        iq->data_points[xr+2] = b;
        iq->data_points[yr+2] = b;
    }

}
void pam_data_convert(pamdata *iq ,uint64_t maxd)
{
    int i;
    uint64_t m = 255*maxd;
    double lm = log((double)m);
    memset(iq->data_points,0,256*256*3*sizeof(char));
    for (i = 0; i < 256*256*3; i+=3){
        // IQ data plot
        int r = i; 
        int g = i+1;
        int b = i+2;
        uint64_t odata = iq->data[i/3];
        uint64_t data = 255*iq->data[i/3];
        double lod = log((double)odata); 
        double q = lod/lm;
     if (data){
            switch (iq->col){
            case IQ_LOG_EVIL:
                if ( q < 0.25){
                    iq->data_points[b] = (int)(1024.0*q);
                } else {
                        if (q >0.5) iq->data_points[g] = (int)(255.0*q);
                    else
                        iq->data_points[r] = (int)(512.0*q);
                }
                break;
            case IQ_LOG_RED:
                iq->data_points[r] = (int)(255.0*q)&0xff;
                break;
            case IQ_LOG_GREEN:
                iq->data_points[g] = (int)(255.0*q)&0xff;
                break;
            case IQ_LOG_BLUE:
                iq->data_points[b] = (int)(255.0*q)&0xff;
                break;
            case IQ_EVIL:
                if  (data < m/4){
                    iq->data_points[b] = ((4*data)/maxd)&0xff;
                } else {
                    if (data >m/2) iq->data_points[g] = (data/maxd)&0xff;
                    else iq->data_points[r] = (2*data/maxd)&0xff;
                }
                break;
            case IQ_TEST:
                if  (data < m/4){
                    iq->data_points[b] = ((4*data)/maxd)&0xff;
                    iq->data_points[g] = ((4*data)/maxd)&0xff;
                } else {
                    if (data >m/2) {
                        iq->data_points[g] = (data/maxd)&0xff;
                    } else {
                        iq->data_points[g] = (2*data/maxd)&0xff;
                        iq->data_points[r] = (2*data/maxd)&0xff;
                    }
                }
                break;
          case IQ_RED:
                iq->data_points[r] = (data/maxd)&0xff;
                break;
            case IQ_BLUE:
                iq->data_points[b] = (data/maxd)&0xff;
                break;
                
            default:
            case IQ_GREE:
                iq->data_points[g] = (data/maxd)&0xff;
                break;
            }
        }
        
    }
}
#define TS_SIZE 188
#define BSIZE 100*TS_SIZE
#define DTIME 40000000ULL
void pam_read_data (int fdin, pamdata *iq)
{
    int8_t buf[BSIZE];
    int i,j;
    
    long t0;
    long t1;
    uint64_t maxd = 0;
    
    t0 = getutime();
    t1 = t0;

    while ((t1 - t0) < DTIME){
        int re =0;
        if ((re=read(fdin,(char *)buf, BSIZE)) < 0){
            return;
        }
        for (i=0; i < re; i+=TS_SIZE){
            for (j=4; j<TS_SIZE; j+=2){
                uint8_t ix = buf[i+j]+128;
                uint8_t qy = 128-buf[i+j+1];
                iq->data[ix|(qy<<8)] += 1;
                uint64_t c = iq->data[ix|(qy<<8)];
                if ( c > maxd) maxd = c;
            }
        }
        t1 = getutime();
    }

    pam_data_convert(iq, maxd);
    pam_coordinate_axes(iq, 255,255 ,0);
    memset(iq->data,0,256*256*sizeof(uint64_t));        
}

void pam_write (int fd, pamdata *iq){
    char *HEAD="P7\nWIDTH 256\nHEIGHT 256\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\n";
    int headlen = strlen(HEAD);
    int we=0;
    we=write(fd,HEAD,headlen);
    we=write(fd,iq->data_points,256*256*3);
    memset(iq->data_points,0,256*256*3*sizeof(char));
}

void proc_ts(int i, uint8_t *buf)
{
  uint16_t pid=0x1fff&((buf[1]<<8)|buf[2]);
  uint8_t ccin = buf[3] & 0x1F;
  
	if (buf[0] == 0x47 && (buf[1] & 0x80) == 0) {
		if( pid != 8191 ) {
			if (ccin & 0x10) {
				if ( cc[pid]) {
					// TODO: 1 repetition allowed
					if ((((cc[pid] + 1) & 0x0F) != (ccin & 0x0F)))
						cc_errors += 1;
				}
				cc[pid] = ccin;
			}
			payload_packets += 1;
		}
	} else
		packet_errors += 1;
	
	if( (packets & 0x3FFF ) == 0)
	{
		printf("%s  Packets: %12u non null %12u, errors: %12u, CC errors: %12u%s", line_start, packets, payload_packets, packet_errors, cc_errors, line_end);
		fflush(stdout);
	}
	
	packets += 1;  
}

#define TSBUFSIZE (100*188)

void tscheck(int ts)
{
        uint8_t *buf;
        uint8_t id;
        int i, nts;
        int len;

	buf=(uint8_t *)malloc(TSBUFSIZE);

        
        while(1) {
                len=read(ts, buf, TSBUFSIZE);
                if (len<0) {
                        continue;
                }
                if (buf[0]!=0x47) {
                        read(ts, buf, 1);
                        continue;
                }
                if (len%188) { /* should not happen */
                        printf("blah\n");
                        continue;
                }
                nts=len/188;
                for (i=0; i<nts; i++)
                        proc_ts(i, buf+i*188);
        }
}
		    
static uint32_t root2gold(uint32_t root)
{
	uint32_t x, g;
	
	for (g = 0, x = 1; g < 0x3ffff; g++)  {
		if (root == x)
			return g;
		x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1);
	}
	return 0xffffffff;
}

int main(int argc, char **argv)
{
	struct dddvb *dd;
	struct dddvb_fe *fe;
	struct dddvb_params p;
	uint32_t bandwidth = DDDVB_UNDEF, frequency = 0, symbol_rate = 0, pol = DDDVB_UNDEF;
	uint32_t id = DDDVB_UNDEF, ssi = DDDVB_UNDEF, num = DDDVB_UNDEF, source = DDDVB_UNDEF;
	uint32_t mtype= DDDVB_UNDEF;
	uint32_t verbosity = 0;
	uint32_t get_ts = 1;
	enum fe_code_rate fec = FEC_AUTO;
	enum fe_delivery_system delsys = ~0;
	char *config = "config/";
	int fd = 0;
	int odvr = 0;
	FILE *fout = stdout;
	int line = -1;
	int color = 0;
        pamdata iq;

	
	while (1) {
		int cur_optind = optind ? optind : 1;
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"config", required_argument, 0, 'c'},
			{"frequency", required_argument, 0, 'f'},
			{"bandwidth", required_argument, 0, 'b'},
			{"symbolrate", required_argument, 0, 's'},
			{"source", required_argument, 0, 'l'},
			{"delsys", required_argument, 0, 'd'},
			{"id", required_argument, 0, 'i'},
			{"ssi", required_argument, 0, 'g'},
			{"gold", required_argument, 0, 'g'},
			{"root", required_argument, 0, 'r'},
			{"num", required_argument, 0, 'n'},
			{"mtype", required_argument, 0, 'm'},
			{"verbosity", required_argument, 0, 'v'},
			{"open_dvr", no_argument, 0, 'o'},
			{"tscheck", no_argument, 0, 't'},
			{"tscheck_l", required_argument, 0, 'a'},
			{"nodvr", no_argument , 0, 'q'},
			{"pam", no_argument , 0, 'P'},
			{"pam_color", no_argument , 0, 'e'},
			{"help", no_argument , 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"e:c:i:f:s:d:p:hg:r:n:b:l:v:m:ota:qP",
				long_options, &option_index);
		if (c==-1)
 			break;
		
		switch (c) {
		case 'e':
		        color = strtoul(optarg, NULL, 0);
		case 'P':
		        odvr = 2;
			break;
		case 'o':
		        fout = stderr;
		        if (odvr) {
			    fprintf(fout,"Can't pipe dvr device when checking continuity\n");
			    break;
			}
		        fprintf(fout,"Reading from dvr\n");
		        odvr = 1;
			break;
		case 'a':
		        line = strtoul(optarg, NULL, 0);
		case 't':
		        fout = stderr;
		        if (odvr) {
			    fprintf(fout,"Can't check continuity when piping dvr device\n");
			    break;
			}
		        fprintf(fout,"performing continuity check\n");
		        odvr = 3;
			break;
		case 'c':
		        config = strdup(optarg);
			break;
		case 'f':
			frequency = strtoul(optarg, NULL, 0);
			break;
		case 'b':
			bandwidth = strtoul(optarg, NULL, 0);
			break;
		case 's':
			symbol_rate = strtoul(optarg, NULL, 0);
			break;
		case 'l':
			source = strtoul(optarg, NULL, 0);
			break;
		case 'v':
			verbosity = strtoul(optarg, NULL, 0);
			break;
		case 'g':
			ssi = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			ssi = root2gold(strtoul(optarg, NULL, 0));
			break;
		case 'i':
			id = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			num = strtoul(optarg, NULL, 0);
			break;
		case 'm':
			if (!strcmp(optarg, "16APSK"))
				mtype = APSK_16;
			if (!strcmp(optarg, "32APSK"))
				mtype = APSK_32;
			if (!strcmp(optarg, "64APSK"))
				mtype = APSK_64;
			if (!strcmp(optarg, "128APSK"))
				mtype = APSK_128;
			if (!strcmp(optarg, "256APSK"))
				mtype = APSK_256;
			if (mtype == DDDVB_UNDEF)
				printf("unknown mtype %s\n", optarg);
			break;
		case 'd':
			if (!strcmp(optarg, "C"))
				delsys = SYS_DVBC_ANNEX_A;
			if (!strcmp(optarg, "DVBC"))
				delsys = SYS_DVBC_ANNEX_A;
			if (!strcmp(optarg, "S"))
				delsys = SYS_DVBS;
			if (!strcmp(optarg, "DVBS"))
				delsys = SYS_DVBS;
			if (!strcmp(optarg, "S2"))
				delsys = SYS_DVBS2;
			if (!strcmp(optarg, "DVBS2"))
				delsys = SYS_DVBS2;
			if (!strcmp(optarg, "T"))
				delsys = SYS_DVBT;
			if (!strcmp(optarg, "DVBT"))
				delsys = SYS_DVBT;
			if (!strcmp(optarg, "T2"))
				delsys = SYS_DVBT2;
			if (!strcmp(optarg, "DVBT2"))
				delsys = SYS_DVBT2;
			if (!strcmp(optarg, "J83B"))
				delsys = SYS_DVBC_ANNEX_B;
			if (!strcmp(optarg, "ISDBC"))
				delsys = SYS_ISDBC;
			if (!strcmp(optarg, "ISDBT"))
				delsys = SYS_ISDBT;
			if (!strcmp(optarg, "ISDBS"))
				delsys = SYS_ISDBS;
			break;
		case 'p':
			if (!strcmp(optarg, "h") || !strcmp(optarg, "H"))
				pol = 1;
			if (!strcmp(optarg, "v") || !strcmp(optarg, "V"))
				pol = 0;
			break;
		case 'q':
			get_ts = 0;
			break;
		case 'h':
		    fprintf(fout,"ddzap [-d delivery_system] [-p polarity] [-c config_dir]\n"
			       "      [-f frequency(Hz for terr./kHz for SAT)]\n"
			       "      [-m 16APSK/32APSK/64APSK/128APSK/256APSK]\n"
			       "          (only needed for higher modulations than 8PSK) on some cards\n"
			       "      [-b bandwidth(Hz)] [-s symbol_rate(Hz)]\n"
			       "      [-g gold_code] [-r root_code] [-i id] [-n device_num]\n"
			       "      [-o (write dvr to stdout)]\n"
			       "      [-l (tuner source for unicable)]\n"
			       "      [-t (continuity check)]\n"
			       "      [-a [display line] (display continuity check in line)]\n"
			       "      [-P (output IQ diagram as pam)]\n"
			       "      [-e [color] (use color for pam 0=green)]\n"
			       "\n"
			       "      delivery_system = C,S,S2,T,T2,J83B,ISDBC,ISDBT\n"
			       "      polarity        = h/H,v/V\n"
			       "\n");
			exit(-1);
		default:
			break;
			
		}
	}
	if (optind < argc) {
	    fprintf(fout,"Warning: unused arguments\n");
	}

	if (delsys == ~0) {
	    fprintf(fout,"You have to choose a delivery system: -d (C|S|S2|T|T2)\n");
		exit(-1);
	}
	switch (delsys) {
	case SYS_DVBC_ANNEX_A:
		if (!symbol_rate)
			symbol_rate = 6900000;
		break;
	}

	dd = dddvb_init(config, verbosity);
	if (!dd) {
	    fprintf(fout,"dddvb_init failed\n");
		exit(-1);
	}
	fprintf(fout,"dvbnum = %u\n", dd->dvbfe_num);
	dddvb_get_ts(dd, get_ts);

	if (num != DDDVB_UNDEF)
		fe = dddvb_fe_alloc_num(dd, delsys, num);
	else
		fe = dddvb_fe_alloc(dd, delsys);
	if (!fe) {
	    fprintf(fout,"dddvb_fe_alloc failed\n");
		exit(-1);
	}
	dddvb_param_init(&p);
	dddvb_set_mtype(&p, mtype);
	dddvb_set_frequency(&p, frequency);
	dddvb_set_src(&p, source);
	dddvb_set_bandwidth(&p, bandwidth);
	dddvb_set_symbol_rate(&p, symbol_rate);
	dddvb_set_polarization(&p, pol);
	dddvb_set_delsys(&p, delsys);
	dddvb_set_id(&p, id);
	dddvb_set_ssi(&p, ssi);
	dddvb_dvb_tune(fe, &p);

#if 0
	{
		uint8_t ts[188];
		
		dddvb_ca_write(dd, 0, ts, 188);

	}
#endif
	if (!odvr){
		while (1) {
			fe_status_t stat;
			int64_t str, cnr;
			
			stat = dddvb_get_stat(fe);
			str = dddvb_get_strength(fe);
			cnr = dddvb_get_cnr(fe);
			
			printf("stat=%02x, str=%" PRId64 ".%03u dBm, "
			       "snr=%" PRId64 ".%03u dB\n",
			       stat, str/1000, abs(str%1000),
			       cnr/1000, abs(cnr%1000));
		sleep(1);
		}
	} else {
#define BUFFSIZE (1024*188)
	        fe_status_t stat;
		char filename[150];
		uint8_t buf[BUFFSIZE];
		
		stat = 0;
		stat = dddvb_get_stat(fe);
		while (!(stat == 0x1f)) {
		        int64_t str, cnr;
		
			stat = dddvb_get_stat(fe);
			str = dddvb_get_strength(fe);
			cnr = dddvb_get_cnr(fe);
			
			fprintf(stderr,"stat=%02x, str=%lld.%03llddB, snr=%lld.%03llddB \n",
				stat, (long long int) str/1000, (long long int) abs(str%1000),
				(long long int) cnr/1000, (long long int)abs(cnr%1000));
			sleep(1);
		}
		fprintf(stderr,"got lock on %s\n", fe->name);
		snprintf(filename,25,
			 "/dev/dvb/adapter%d/dvr%d",fe->anum, fe->fnum);
		fprintf(stderr,"opening %s\n", filename);
		if ((fd = open(filename ,O_RDONLY)) < 0){
		    fprintf(stderr,"Error opening input file:%s\n",filename);
		}
		switch (odvr){
		case 1:
		    while(1){
			read(fd,buf,BUFFSIZE);
			write(fileno(stdout),buf,BUFFSIZE);
		    }
		    break;
		case 2:
		    fprintf(stderr,"writing pamdata\n");
		    init_pamdata(&iq,color);
		    while(1){
			pam_read_data(fd, &iq);
			pam_write(STDOUT_FILENO, &iq);
		    }
		    break;
		case 3:
		    if( line >= 0 && line < 64 ){
			snprintf(line_start,sizeof(line_start)-1,"\0337\033[%d;0H",line);
			strncpy(line_end,"\0338",sizeof(line_end)-1);
		    }
		    tscheck(fd);
		    break;
		}
	}
}
