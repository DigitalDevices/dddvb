#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/types.h>
#include <getopt.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "../ddbridge/ddbridge-mci.h"
#include "../ddbridge/ddbridge-ioctl.h"

char *Rolloff[8] = {
	"0.35",
	"0.25",
	"0.20",
	"0.10",
	"0.05",
	"0.15",
	"rsvd",
	"rsvd",
};

void dump(const uint8_t *b, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) {
		printf("%04x: ", j);
		for (i = 0; i < 16; i++)
			if (i + j < l)
				printf("%02x ", b[i]);
			else
				printf("   ");
		printf("\n");
	}
}       

void print_temp(struct mci_result *res)
{
	printf("Die temperature = %u\n", res->sx8_bist.temperature);
}

int temp_info(int dev, uint32_t link)
{
	struct ddb_mci_msg msg = {
		.link = link,
		.cmd.command = SX8_CMD_GETBIST,
	};
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}
	if (msg.res.status & 0x80) {
		printf("MCI errror %02x\n", msg.res.status);
		return ret;
	}
		
	print_temp(&msg.res);
	printf("BIST info dump:  ");
	dump((uint8_t *) &msg.res, 16);
	
	return ret;
}


#define SIZE_OF_ARRAY(a) (sizeof(a)/sizeof(a[0]))

char *DemodStatus[] = {
    "Idle",
    "IQ Mode",
    "Wait for Signal",
    "DVB-S2 Wait for MATYPE",
    "DVB-S2 Wait for FEC",
    "DVB-S1 Wait for FEC",
    "Wait for TS",
    "Unknown 7",
    "Unknown 8",
    "Unknown 9",
    "Unknown 10",
    "Unknown 11",
    "Unknown 12",
    "Unknown 13",
    "Timeout",
    "Locked",
    "C2 Scan",
};

char* S2ModCods[32] = {
/* 0x00 */    "DummyPL"     , 

// Legacy S2:   index is S2_Modcod * 2 + short

/* 0x01 */    "QPSK 1/4"    ,
/* 0x02 */    "QPSK 1/3"    ,
/* 0x03 */    "QPSK 2/5"    ,
/* 0x04 */    "QPSK 1/2"    ,
/* 0x05 */    "QPSK 3/5"    ,
/* 0x06 */    "QPSK 2/3"    ,
/* 0x07 */    "QPSK 3/4"    ,
/* 0x08 */    "QPSK 4/5"    ,
/* 0x09 */    "QPSK 5/6"    ,
/* 0x0A */    "QPSK 8/9"    ,
/* 0x0B */    "QPSK 9/10"   ,
                             
/* 0x0C */    "8PSK 3/5"    ,
/* 0x0D */    "8PSK 2/3"    ,
/* 0x0E */    "8PSK 3/4"    ,
/* 0x0F */    "8PSK 5/6"    ,
/* 0x10 */    "8PSK 8/9"    ,
/* 0x11 */    "8PSK 9/10"   ,
                             
/* 0x12 */    "16APSK 2/3"  ,
/* 0x13 */    "16APSK 3/4"  ,
/* 0x14 */    "16APSK 4/5"  ,
/* 0x15 */    "16APSK 5/6"  ,
/* 0x16 */    "16APSK 8/9"  ,
/* 0x17 */    "16APSK 9/10" ,
                             
/* 0x18 */    "32APSK 3/4"  ,
/* 0x19 */    "32APSK 4/5"  ,
/* 0x1A */    "32APSK 5/6"  ,
/* 0x1B */    "32APSK 8/9"  ,
/* 0x1C */    "32APSK 9/10" ,
                             
/* 0x1D */    "rsvd 0x1D"   ,
/* 0x1E */    "rsvd 0x1E"   ,
/* 0x1F */    "rsvd 0x1F"   ,
};


///* 129 */    "VLSNR1"          ,  
///* 131 */    "VLSNR2"          ,  

char* S2XModCods[59] = {
/* 0x42 */    "QPSK 13/45"      , 
/* 0x43 */    "QPSK 9/20"       , 
/* 0x44 */    "QPSK 11/20"      , 
                                  
/* 0x45 */    "8APSK 5/9-L"     , 
/* 0x46 */    "8APSK 26/45-L"   , 
/* 0x47 */    "8PSK 23/36"      , 
/* 0x48 */    "8PSK 25/36"      , 
/* 0x49 */    "8PSK 13/18"      , 
                                  
/* 0x4A */    "16APSK 1/2-L"    , 
/* 0x4B */    "16APSK 8/15-L"   , 
/* 0x4C */    "16APSK 5/9-L"    , 
/* 0x4D */    "16APSK 26/45"    , 
/* 0x4E */    "16APSK 3/5"      , 
/* 0x4F */    "16APSK 3/5-L"    , 
/* 0x50 */    "16APSK 28/45"    , 
/* 0x51 */    "16APSK 23/36"    , 
/* 0x52 */    "16APSK 2/3-L"    , 
/* 0x53 */    "16APSK 25/36"    , 
/* 0x54 */    "16APSK 13/18"    , 
                                  
/* 0x55 */    "16APSK 7/9"      , 
/* 0x56 */    "16APSK 77/90"    , 
                                  
/* 0x57 */    "32APSK 2/3-L"    , 
/* 0x58 */    "rsvd 32APSK"     , 
/* 0x59 */    "32APSK 32/45"    , 
/* 0x5A */    "32APSK 11/15"    , 
/* 0x5B */    "32APSK 7/9"      , 
                                  
/* 0x5C */    "64APSK 32/45-L"  , 
/* 0x5D */    "64APSK 11/15"    , 
/* 0x5E */    "rsvd 64APSK"     , 
/* 0x5F */    "64APSK 7/9"      , 
                                  
/* 0x60 */    "rsvd 64APSK"     , 
/* 0x61 */    "64APSK 4/5"      , 
/* 0x62 */    "rsvd 64APSK"     , 
/* 0x63 */    "64APSK 5/6"      , 
                                  
/* 0x64 */    "128APSK 3/4"     , 
/* 0x65 */    "128APSK 7/9"     , 

/* 0x66 */    "256APSK 29/45-L" , 
/* 0x67 */    "256APSK 2/3-L"   , 
/* 0x68 */    "256APSK 31/45-L" , 
/* 0x69 */    "256APSK 32/45"   , 
/* 0x6A */    "256APSK 11/15-L" , 
/* 0x6B */    "256APSK 3/4"     , 

/* 0x6C */    "QPSK 11/45-S"    ,
/* 0x6D */    "QPSK 4/15-S"     ,
/* 0x6E */    "QPSK 14/45-S"    ,
/* 0x6F */    "QPSK 7/15-S"     ,
/* 0x70 */    "QPSK 8/15-S"     ,
/* 0x71 */    "QPSK 32/45-S"    ,
                                 
/* 0x72 */    "8PSK 7/15-S"     ,
/* 0x73 */    "8PSK 8/15-S"     ,
/* 0x74 */    "8PSK 26/45-S"    ,
/* 0x75 */    "8PSK 32/45-S"    ,
                                 
/* 0x76 */    "16APSK 7/15-S"   ,
/* 0x77 */    "16APSK 8/15-S"   ,
/* 0x78 */    "16APSK 26/45-S"  ,
/* 0x79 */    "16APSK 3/5-S"    ,
/* 0x7A */    "16APSK 32/45-S"  ,
                                 
/* 0x7B */    "32APSK 2/3-S"    ,
/* 0x7C */    "32APSK 32/45-S"  ,
};

char* S2Xrsvd[] = {
/* 250 */    "rsvd 8PSK"       ,
/* 251 */    "rsvd 16APSK"     ,
/* 252 */    "rsvd 32APSK"     ,
/* 253 */    "rsvd 64APSK"     ,
/* 254 */    "rsvd 256APSK"    ,
/* 255 */    "rsvd 1024APSK"   ,
};

char* PunctureRates[32] = {
/* 0x00 */    "QPSK 1/2",   // DVB-S1 
/* 0x01 */    "QPSK 2/3",   // DVB-S1 
/* 0x02 */    "QPSK 3/4",   // DVB-S1 
/* 0x03 */    "QPSK 5/6",   // DVB-S1 
/* 0x04 */    "QPSK 6/7",   // DSS
/* 0x05 */    "QPSK 7/8",   // DVB-S1 
/* 0x06 */    "rsvd 6.0",
/* 0x07 */    "rsvd 7.0",
};

int mci_bb(int dev, uint32_t link, uint8_t demod)
{
	struct ddb_mci_msg msg = {
		.link = link,
		.cmd.command = MCI_CMD_GET_BBHEADER,
		.cmd.demod = demod,
		.cmd.get_bb_header.select = 0,
	};
	struct mci_result *res = &msg.res;
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}
	if (res->bb_header.valid) {
		printf("MATYPE1: %02x\n", res->bb_header.matype_1);
		printf("MATYPE2: %02x\n", res->bb_header.matype_2);
	}
	return ret;
}

void print_info(int dev, uint32_t link, uint8_t demod, struct mci_result *res)
{
	if (res->status == MCI_DEMOD_STOPPED) {
		printf("\nDemod %u: stopped\n", demod);
		return;
	}
	
	printf("\nDemod %u:\n", demod);
	if (res->status == MCI_DEMOD_LOCKED) {
		switch (res->mode) {
		case 0:
		case M4_MODE_DVBSX:
			if (res->dvbs2_signal_info.standard != 1) {
				int short_frame = 0, pilots = 0;
				char *modcod = "unknown";
				uint8_t pls = res->dvbs2_signal_info.pls_code;
				
				if ((pls >= 128) || ((res->dvbs2_signal_info.roll_off & 0x7f) > 2))
					printf("Demod Locked:  DVB-S2X\n");
				else
					printf("Demod Locked:  DVB-S2\n");
				printf("PLS-Code:      %u\n", res->dvbs2_signal_info.pls_code);
				mci_bb(dev, link, demod);
				if (pls >= 250)  {
					pilots = 1;
					modcod = S2Xrsvd[pls - 250];
				} else if (pls >= 132) {
					pilots = pls & 1;
					short_frame = pls > 216;
					modcod = S2XModCods[(pls - 132)/2];
				} else if (pls < 128) {
					pilots = pls & 1;
					short_frame = pls & 2;
					modcod = S2ModCods[pls / 4];
				}
				printf("Roll-Off:      %s\n", Rolloff[res->dvbs2_signal_info.roll_off & 7]);
				printf("Pilots:        %s\n", pilots ? "On" : "Off");
				printf("Frame:         %s\n", short_frame ? "Short" : "Normal");
			} else {
				printf("Demod Locked:  DVB-S\n");
				printf("PR:            %s\n",
				       PunctureRates[res->dvbs2_signal_info.pls_code & 0x07]);
			}
			printf("Inversion:     %s\n", (res->dvbs2_signal_info.roll_off & 0x80) ? "on": "off");
			break;
		case M4_MODE_DVBT:
			printf("Locked DVB-T\n");
			break;
		case M4_MODE_DVBT2:
			printf("Locked DVB-T2\n");
			break;
		}
		printf("SNR:           %.2f dB\n", (float) res->dvbs2_signal_info.signal_to_noise / 100);
		printf("Packet Errors: %u\n", res->dvbs2_signal_info.packet_errors);
		printf("BER Numerator: %u\n", res->dvbs2_signal_info.ber_numerator);
		printf("BER Denom.:    %u\n", res->dvbs2_signal_info.ber_denominator);
	} else {
		printf("Demod State:   %s\n",
		       res->status < SIZE_OF_ARRAY(DemodStatus) ? DemodStatus[res->status] : "?");
		
	}
	printf("Frequency:     %u Hz\n", res->dvbs2_signal_info.frequency);
	printf("Symbol Rate:   %u Symbols/s\n", res->dvbs2_signal_info.symbol_rate);
	printf("Channel Power: %.2f dBm\n", (float) res->dvbs2_signal_info.channel_power / 100);
	if (res->dvbs2_signal_info.band_power > -10000)
		printf("Band Power:    %.2f dBm\n", (float) res->dvbs2_signal_info.band_power / 100);
	
}

int readreg(int dev, uint32_t reg, uint32_t link, uint32_t *val)
{
	struct ddb_reg ddbreg;

        ddbreg.reg =  reg + (link << 28);
	if (ioctl(dev, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	*val = ddbreg.val;
	return 0;
}

void mci_firmware(int dev, uint32_t link)
{
	union {
		uint32_t u[4];
		char  s[16];
	} version;
	
	readreg(dev, MIC_INTERFACE_VER     , link, &version.u[0]);
	readreg(dev, MIC_INTERFACE_VER +  4, link, &version.u[1]);
	readreg(dev, MIC_INTERFACE_VER +  8, link, &version.u[2]);
	readreg(dev, MIC_INTERFACE_VER + 12, link, &version.u[3]);
    
	printf("MCI firmware: %s.%d\n", &version.s, version.s[15]);
}


int mci_info(int dev, uint32_t link, uint8_t demod)
{
	struct ddb_mci_msg msg = {
		.link = link,
		.cmd.command = MCI_CMD_GETSIGNALINFO,
		.cmd.demod = demod
	};
	int ret;
	int i;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}

	print_info(dev, link, demod, &msg.res);
	return ret;
}

static int get_id(int fd, int link, struct ddb_id *id)
{
	struct ddb_reg ddbreg;

	if (link == 0) {
		if (ioctl(fd, IOCTL_DDB_ID, id) < 0)
			return -1;
		return 0;
	}
        ddbreg.reg = 8 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->vendor = ddbreg.val;
	id->device = ddbreg.val >> 16;

        ddbreg.reg = 12 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->subvendor = ddbreg.val;
	id->subdevice = ddbreg.val >> 16;

        ddbreg.reg = 0 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->hw = ddbreg.val;

	ddbreg.reg = 4 + (link << 28);
	if (ioctl(fd, IOCTL_DDB_READ_REG, &ddbreg) < 0)
		return -1;
	id->regmap = ddbreg.val;
	return 0;
}

static char *id2name(uint16_t id)
{
	switch (id) {
	case 0x222:
		return "MOD";
	case 0x0009:
		return "MAX SX8";
	case 0x000b:
		return "MAX SX8 Basic";
	case 0x000a:
		return "MAX M4";
	default:
		return " ";
	}
}

static int card_info(int ddbnum, int demod)
{
	char ddbname[80];
	struct ddb_id ddbid;
	int ddb, ret, link, links = 1, i;
	struct ddb_id id;
	
	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0)
		return -3;

	for (link = 0; link < links; link++) {
		ret = get_id(ddb, link, &id);
		if (ret < 0)
			goto out;
		if (!link) {
			switch (id.device) {
			case 0x20:
				links = 4;
				break;
			case 0x300:
			case 0x301:
			case 0x307:
				links = 2;
				break;
			
			default:
				break;
			}
		}
		printf("\n\nCard %s link %u id %04x (%s):\n",
		       ddbname, link, id.device, id2name(id.device));
		printf("HW %08x REGMAP %08x FW %u.%u\n",
		       id.hw, id.regmap, (id.hw & 0xff0000) >> 16, (id.hw & 0xffff));
		switch (id.device) {
		case 0x0009:
			mci_firmware(ddb, link);
			if (demod >= 0)
				mci_info(ddb, link, demod);
			else {
				for (i = 0; i < 8; i++)
					mci_info(ddb, link, i);
			}
			temp_info(ddb, link);
			break;
		case 0x000a:
			mci_firmware(ddb, link);
			if (demod >= 0)
				mci_info(ddb, link, demod);
			else {
				for (i = 0; i < 4; i++)
					mci_info(ddb, link, i);
			}
			break;
		default:
			break;
		}
	}
	
out:
	close(ddb);
	return ret;
}

int main(int argc, char*argv[])
{
	int fd = -1, all = 1, i, ret = 0;
	char fn[128];
	int32_t device = -1, demod = -1;
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"device", required_argument, 0, 'd'},
			{"demod", required_argument, 0, 'n'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, "ad:n:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'd':
			device = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			demod = strtoul(optarg, NULL, 0);
			break;
		case 'a':
			all = 1;
			break;
		default:
			break;
		}
	}
	if (optind < argc) {
		printf("too many arguments\n");
		exit(1);
	}
	if (device >=0)
		ret = card_info(device, demod);
	else
		for (i = 0; i < 100; i++) {
			ret = card_info(i, -1);
			
			if (ret == -3)     /* could not open, no more cards! */
				break; 
			if (ret < 0)
				return i; /* fatal error */ 
		}
}
