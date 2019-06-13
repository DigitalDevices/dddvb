#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <linux/dvb/mod.h>

char* modulation2text(const enum fe_modulation mod)
{
	switch(mod)
	{
	case QPSK:
		return "QPSK";
	case QAM_16:
		return "16QAM";
	case QAM_32:
		return "32QAM";
	case QAM_64:
		return "64QAM";
	case QAM_128:
		return "128QAM";
	case QAM_256:
		return "256QAM";
	case QAM_AUTO:
		return "QAM_AUTO";
	case VSB_8:
		return "8VSB";
	case VSB_16:
		return "16VSB";
	case PSK_8:
		return "8PSK";
	case APSK_16:
		return "16APSK";
	case APSK_32:
		return "32APSK";
	case DQPSK:
		return "DQPSK";
	case QAM_4_NR:
		return "QAM_4_NR";
	case APSK_64:
		return "64APSK";
	case APSK_128:
		return "128APSK";
	case APSK_256:
		return "256APSK";
	case APSK_8:
		return "8APSK";
	default:
		return "MODULATION_NONE";
	};
}

char* fec2text(const enum fe_code_rate fec)
{
	switch(fec)
	{
	case        FEC_1_2:
		return "1/2";
	case        FEC_2_3:
		return "2/3";
	case        FEC_3_4:
		return "3/4";
	case        FEC_4_5:
		return "4/5";
	case        FEC_5_6:
		return "5/6";
	case        FEC_6_7:
		return "6/7";
	case        FEC_7_8:
		return "7/8";
	case        FEC_8_9:
		return "8/9";
	case        FEC_AUTO:
		return "FEC_AUTO";
	case        FEC_3_5:
		return "3/5";
	case        FEC_9_10:
		return "9/10";
	case        FEC_2_5:
		return "2/5";
	case        FEC_1_4:
		return "1/4";
	case        FEC_1_3:
		return "1/3";
	case        FEC_13_45:
		return "13/45";
	case        FEC_9_20:
		return "9/20";
	case        FEC_11_20:
		return "11/20";
	case        FEC_5_9_L:
		return "5/9-L";
	case        FEC_26_45_L:
		return "26/45-L";
	case        FEC_23_36:
		return "23/36";
	case        FEC_25_36:
		return "25/36";
	case        FEC_13_18:
		return "13/18";
	case        FEC_1_2_L:
		return "1/2-L";
	case        FEC_8_15_L:
		return "8/15-L";
	case        FEC_26_45:
		return "26/45";
	case        FEC_3_5_L:
		return "3/5-L";
	case        FEC_28_45:
		return "28/45";
	case        FEC_2_3_L:
		return "2/3-L";
	case        FEC_7_9:
		return "7/9";
	case        FEC_32_45_L:
		return "32/45-L";
	case        FEC_77_90:
		return "77/90";
	case        FEC_32_45:
		return "32/45";
	case        FEC_11_15:
		return "11/15";
	case        FEC_29_45_L:
		return "29/45-L";
	case        FEC_31_45_L:
		return "31/45-L";
	case        FEC_11_15_L:
		return "11/15-L";
	case        FEC_11_45:
		return "11/45";
	case        FEC_4_15:
		return "4/15";
	case        FEC_14_45:
		return "14/45";
	case        FEC_7_15:
		return "7/15";
	case        FEC_8_15:
		return "8/15";
	default:
		return "FEC_NONE";
	};

}

int main()
{
        const enum fe_modulation modcod2modS2X[0x3D] = {
		MODULATION_NONE, MODULATION_NONE, //2
                QPSK, QPSK, QPSK, //3
		APSK_8, APSK_8,APSK_8,APSK_8,APSK_8, //5
		APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,//13
		APSK_32,MODULATION_NONE,APSK_32,APSK_32,APSK_32,//5
		APSK_64, APSK_64, MODULATION_NONE, APSK_64,MODULATION_NONE, APSK_64,MODULATION_NONE ,APSK_64,//8
		APSK_128, APSK_128,
		APSK_256, APSK_256,APSK_256,APSK_256,APSK_256,APSK_256,
		QPSK, QPSK,QPSK,QPSK,QPSK,QPSK,
		PSK_8,PSK_8,PSK_8,PSK_8,
		APSK_16,APSK_16,APSK_16,APSK_16,APSK_16,
		APSK_32,APSK_32,
        };

        const enum fe_code_rate modcod2fecS2X[0x3D] = {
                FEC_NONE, FEC_NONE, //2
		FEC_13_45, FEC_9_20, FEC_11_20, //3
		FEC_5_9_L, FEC_26_45_L, FEC_23_36, FEC_25_36, FEC_13_18, //5
		FEC_1_2_L, FEC_8_15_L, FEC_5_9_L, FEC_26_45, FEC_3_5, FEC_3_5_L, FEC_28_45, FEC_23_36,FEC_2_3_L, FEC_25_36, FEC_13_18,FEC_7_9, FEC_77_90, //13
		FEC_2_3_L, FEC_NONE, FEC_32_45, FEC_11_15, FEC_7_9,
		FEC_32_45_L, FEC_11_15,FEC_NONE ,FEC_7_9, FEC_NONE, FEC_4_5, FEC_NONE, FEC_5_6, //8
		FEC_3_4,FEC_7_9, FEC_29_45_L,FEC_2_3_L,FEC_31_45_L,FEC_32_45,FEC_11_15_L,
		FEC_3_4,FEC_11_45,FEC_4_15,FEC_14_45,FEC_7_15,FEC_8_15,
		FEC_32_45,FEC_7_15,FEC_8_15,FEC_26_45,FEC_32_45,FEC_7_15,
		FEC_8_15,FEC_26_45,FEC_3_5,FEC_32_45,FEC_2_3,FEC_32_45,
        };



uint8_t modcod;
modcod = 248;
if(modcod2modS2X[((modcod & 0x7F) >> 1)] == APSK_32)
	printf("modcod2modS2X OK APSK_32=%s \r\n", modulation2text(modcod2modS2X[((modcod & 0x7F) >> 1)] ) );
if(modcod2fecS2X[((modcod & 0x7F) >> 1)] == FEC_32_45)
	printf("modcod2modS2X OK FEC_32_45=%s\r\n", fec2text(modcod2fecS2X[((modcod & 0x7F) >> 1)]));


modcod = 132;
if(modcod2modS2X[((modcod & 0x7F) >> 1)] == QPSK)
	printf("modcod2modS2X OK QPSK=%s \r\n", modulation2text(modcod2modS2X[((modcod & 0x7F) >> 1)] ) );
if(modcod2fecS2X[((modcod & 0x7F) >> 1)] == FEC_13_45)
	printf("modcod2modS2X OK FEC_13_45=%s\r\n", fec2text(modcod2fecS2X[((modcod & 0x7F) >> 1)]));

modcod = 184;
if(modcod2modS2X[((modcod & 0x7F) >> 1)] == APSK_64)
	printf("modcod2modS2X OK 64APSK=%s \r\n", modulation2text(modcod2modS2X[((modcod & 0x7F) >> 1)] ) );
if(modcod2fecS2X[((modcod & 0x7F) >> 1)] == FEC_32_45_L)
	printf("modcod2modS2X OK FEC_32_45_L=%s\r\n", fec2text(modcod2fecS2X[((modcod & 0x7F) >> 1)]));

}

