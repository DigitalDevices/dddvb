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
	uint32_t bandwidth = 8000000, frequency = 0, symbol_rate = 0, pol = DDDVB_UNDEF;
	uint32_t id = DDDVB_UNDEF, ssi = DDDVB_UNDEF, num = DDDVB_UNDEF, source = 0;
	uint32_t mtype= DDDVB_UNDEF;
	uint32_t verbosity = 0;
	enum fe_code_rate fec = FEC_AUTO;
	enum fe_delivery_system delsys = ~0;
	char *config = "config/";
	int fd = 0;
	int odvr = 0;
	FILE *fout = stdout;
	
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
			{"help", no_argument , 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"c:i:f:s:d:p:hg:r:n:b:l:v:m:o",
				long_options, &option_index);
		if (c==-1)
 			break;
		
		switch (c) {
		case 'o':
		    fout = stderr;
		        fprintf(fout,"Reading from dvr\n");
		        odvr = 1;
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
			break;
		case 'p':
			if (!strcmp(optarg, "h"))
				pol = 1;
			if (!strcmp(optarg, "v"))
				pol = 0;
			break;
		case 'h':
		    fprintf(fout,"ddzap [-d delivery_system] [-p polarity] [-c config_dir] [-f frequency(Hz)]\n"
			       "      [-b bandwidth(Hz)] [-s symbol_rate(Hz)]\n"
			       "      [-g gold_code] [-r root_code] [-i id] [-n device_num]\n"
			       "      [-o (write dvr to stdout)]\n"
			       "\n"
			       "      delivery_system = C,S,S2,T,T2,J83B,ISDBC,ISDBT\n"
			       "      polarity        = h,v\n"
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
			
			printf("stat=%02x, str=%lld.%03llddB, snr=%lld.%03llddB \n",
			       stat, str/1000, abs(str%1000), cnr/1000, abs(cnr%1000));
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
			
			printf("stat=%02x, str=%lld.%03llddB, snr=%lld.%03llddB \n",
			       stat, str/1000, abs(str%1000), cnr/1000, abs(cnr%1000));
			sleep(1);
		}
		fprintf(stderr,"got lock on %s\n", fe->name);
		snprintf(filename,25,
			 "/dev/dvb/adapter%d/dvr%d",fe->anum, fe->fnum);
		fprintf(stderr,"opening %s\n", filename);
		if ((fd = open(filename ,O_RDONLY)) < 0){
		        fprintf(stderr,"Error opening input file:%s\n",filename);
		}
		while(1){
		    read(fd,buf,BUFFSIZE);
		    write(fileno(stdout),buf,BUFFSIZE);
		}
	}
}
