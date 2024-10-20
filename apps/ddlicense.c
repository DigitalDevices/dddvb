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
#include <ctype.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "../ddbridge/ddbridge-mci.h"
#include "../ddbridge/ddbridge-ioctl.h"

static void dump(const uint8_t *b, int l)
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

static void ldump(FILE *f, uint8_t *b, int l)
{
	int i;
	
	for (i = 0; i < l; i++)
		fprintf(f, "%02X", b[i]);
	fprintf(f, "\n");
}       

static int mci_get_license(int dev, uint8_t *ID, uint8_t *LK, uint8_t *SN)
{
	struct ddb_mci_msg msg = {
		.link = 0,
		.cmd.command = CMD_GET_SERIALNUMBER,
	};
	int ret;
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		dprintf(2, "Error: %d\n", ret, errno);
		return ret;
	}
	if (msg.res.status != 0x00) {
		dprintf(2, "MCI error: %02x, check firmware and license file.\n", msg.res.status);
		return -1;
	}
	memcpy(ID, msg.res.license.ID, 8);
	memcpy(LK, msg.res.license.LK, 24);
	memcpy(SN, msg.res.license.serial_number, 24);
	return 0;
}

static int mci_set_license(int dev, uint8_t *ID, uint8_t *LK)
{
	struct ddb_mci_msg msg = {
		.link = 0,
		.cmd.command = CMD_IMPORT_LICENSE,
	};
	int ret;

	memcpy(msg.cmd.license.ID, ID, 8);
	memcpy(msg.cmd.license.LK, LK, 24);
	
	ret = ioctl(dev, IOCTL_DDB_MCI_CMD, &msg);
	if (ret < 0) {
		printf("Error: %d %d\n", ret, errno);
		return ret;
	}
	if (msg.res.status != 0x00) {
		dprintf(2, "MCI error: %02x, check firmware and license file.\n", msg.res.status);
		return -1;
	}
	return ret;
}

static int GetHex(char* s, uint32_t nBytes, uint8_t *Buffer)
{
	int i;

	if( strlen(s) < (nBytes * 2) )
		return -1;
	for (i = 0; i < nBytes; i += 1) {
		char d0, d1;
		d0 = s[i*2];
		if( !isxdigit(d0) ) return -1;
		d1 = s[i*2+1];
		if( !isxdigit(d1) ) return -1;
		d0 = toupper(d0);
		d1 = toupper(d1);
		Buffer[i] =(uint8_t) ((d0 > '9' ? d0 - 'A' + 10 : d0 - '0') << 4) | ((d1 > '9' ? d1 - 'A' + 10 : d1 - '0'));
	}
	return (nBytes * 2);
}

static int get_id_lk(char *fn, uint8_t *ID, uint8_t *LK)
{
	FILE *fin = fopen(fn, "r");

	if (!fin) {
		printf("License file not found\n");
		return -1;
	}
	memset(ID, 0, 8);
	memset(LK, 0xff, 24);
	while (1) {
		char s[128];
		if (fgets(s, sizeof(s), fin) == NULL)
			break;
		if (strncmp(s,"ID:",3) == 0) {
			if (GetHex(&s[3], 8, ID) < 0 )
				return -1;
		}
		if (strncmp(s,"LK:",3) == 0) {
			if (GetHex(&s[3],24, LK) < 0 )
				return -1;
		}
	}
	//dump(ID, 8);
	//dump(LK, 24);
	fclose(fin);
	return 0;
}

static int get_license(int ddb, struct ddb_id *id, char *ename)
{
	uint8_t ID[8], LK[24], SN[17];
	int stat;
	FILE *f = fopen(ename, "w+");
	
	if (!f) {
		dprintf(2, "Could not write to output file.\n");
		return -1;
	}
	stat = mci_get_license(ddb, ID, LK, SN);
	if (stat < 0) {
		dprintf(2, "Could not read license.\n");
		return stat;
	}
	if (SN[0] == 0xff)
		SN[0] = 0;
	fprintf(f, "VEN:%04X\n", id->vendor);
	fprintf(f, "DEV:%04X\n", id->device);
	fprintf(f, "SERNBR:%s\n", (char *) SN);
	fprintf(f, "ID:");
	ldump(f, ID, 8);
	fprintf(f, "LK:");
	ldump(f, LK, 24);
	fclose(f);
	return 0;
}

static int set_license(int ddb, char *iname)
{
	uint8_t ID[8], LK[24];
	int stat=0;
	
	stat = get_id_lk(iname, ID, LK);
	if (stat < 0)
		return stat;
	return mci_set_license(ddb, ID, LK);
}

static int get_set_license(int ddbnum, char *ename, char *iname)
{
	int ddb, stat = 0;
	char ddbname[80];
	struct ddb_id id;

	sprintf(ddbname, "/dev/ddbridge/card%d", ddbnum);
	ddb = open(ddbname, O_RDWR);
	if (ddb < 0) {
		dprintf(2, "Error opening device %s\n", ddbname);
		return -3;
	}
	if (ioctl(ddb, IOCTL_DDB_ID, &id) < 0) {
		dprintf(2, "Unsupported device %s.\n", ddbname);
		return -1;
	}
	if (id.device != 0x210) {
		dprintf(2, "Unsupported device %s with ID %04x.\n", ddbname, id.device);
		return -1;
	}
	if (ename)
		stat = get_license(ddb, &id, ename);
	if (iname)
		stat = set_license(ddb, iname);
	close(ddb);
	return stat;
}
		      

int main(int argc, char*argv[])
{
	int fd = -1, all = 1, i, ret = 0;
	char fn[128];
	int32_t device = 0;
	char *iname = 0, *ename = 0;
	
	while (1) {
		int cur_optind = optind ? optind : 1;
		int option_index = 0;
		int c;
		static struct option long_options[] = {
			{"device", required_argument, 0, 'd'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, "ad:i:e:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'd':
			device = strtoul(optarg, NULL, 0);
			break;
		case 'a':
			all = 1;
			break;
		case 'i':
			iname = optarg;
			break;
		case 'e':
			ename = optarg;
			break;
		default:
			break;
		}
	}
	if (optind < argc) {
		printf("too many arguments\n");
		exit(1);
	}
	if (!ename && !iname) {
		dprintf(2, "Neither export nor import file name provided.\n");
		return -1;
	}
	get_set_license(device, ename, iname);
	return 0;
}
