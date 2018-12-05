#include <linux/dvb/frontend.h>
#include "src/libdddvb.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>

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
	uint32_t frequency = 0, symbol_rate = 0, pol = DDDVB_UNDEF;
	uint32_t id = DDDVB_UNDEF, pls = DDDVB_UNDEF, num = DDDVB_UNDEF;
	enum fe_code_rate fec = FEC_AUTO;
	enum fe_delivery_system delsys = ~0;
	char *config = "config/";

	while (1) {
		int cur_optind = optind ? optind : 1;
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"config", required_argument, 0, 'c'},
			{"frequency", required_argument, 0, 'f'},
			{"symbolrate", required_argument, 0, 's'},
			{"delsys", required_argument, 0, 'd'},
			{"id", required_argument, 0, 'i'},
			{"pls", required_argument, 0, 'g'},
			{"root", required_argument, 0, 'r'},
			{"num", required_argument, 0, 'n'},
			{"help", no_argument , 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"c:i:f:s:d:p:hg:r:n:",
				long_options, &option_index);
		if (c==-1)
 			break;
		
		switch (c) {
		case 'c':
			config = strdup(optarg);
			break;
		case 'f':
			frequency = strtoul(optarg, NULL, 0);
			break;
		case 's':
			symbol_rate = strtoul(optarg, NULL, 0);
			break;
		case 'g':
			pls = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			pls = root2gold(strtoul(optarg, NULL, 0));
			break;
		case 'i':
			id = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			num = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			if (!strcmp(optarg, "C"))
				delsys = SYS_DVBC_ANNEX_A;
			if (!strcmp(optarg, "S"))
				delsys = SYS_DVBS;
			if (!strcmp(optarg, "S2"))
				delsys = SYS_DVBS2;
			if (!strcmp(optarg, "T"))
				delsys = SYS_DVBT;
			if (!strcmp(optarg, "T2"))
				delsys = SYS_DVBT2;
			break;
		case 'p':
			if (!strcmp(optarg, "h"))
				pol = 1;
			if (!strcmp(optarg, "v"))
				pol = 0;
			break;
		case 'h':
			printf("no help yet\n");
			exit(-1);
		default:
			break;
			
		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}

	if (delsys == ~0) {
		printf("You have to choose a delivery system: -d (C|S|S2|T|T2)\n");
		exit(-1);
	}
	switch (delsys) {
	case SYS_DVBC_ANNEX_A:
		if (!symbol_rate)
			symbol_rate = 6900000;
		break;
	}

	dd = dddvb_init(config, 0);//0xffff);
	if (!dd) {
		printf("dddvb_init failed\n");
		exit(-1);
	}
	printf("dvbnum = %u\n", dd->dvbfe_num);

	if (num != DDDVB_UNDEF)
		fe = dddvb_fe_alloc_num(dd, delsys, num);
	else
		fe = dddvb_fe_alloc(dd, delsys);
	if (!fe) {
		printf("dddvb_fe_alloc failed\n");
		exit(-1);
	}
	dddvb_param_init(&p);
	dddvb_set_frequency(&p, frequency);
	dddvb_set_symbol_rate(&p, symbol_rate);
	dddvb_set_polarization(&p, pol);
	dddvb_set_delsys(&p, delsys);
	dddvb_set_id(&p, id);
	dddvb_set_pls(&p, pls);
	dddvb_dvb_tune(fe, &p);
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
}
