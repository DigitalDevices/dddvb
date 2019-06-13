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
	printf("modcod2modS2X OK\r\n");
if(modcod2fecS2X[((modcod & 0x7F) >> 1)] == FEC_32_45)
	printf("modcod2modS2X OK\r\n");

modcod = 132;
if(modcod2modS2X[((modcod & 0x7F) >> 1)] == QPSK)
	printf("modcod2modS2X OK\r\n");
if(modcod2fecS2X[((modcod & 0x7F) >> 1)] == FEC_13_45)
	printf("modcod2modS2X OK\r\n");
//printf("test mod=%u value= %d APSK_32=%d \n",mod, modcod2modS2X[((mod & 0x7F) >> 1)], APSK_32);
//printf("test mod=%u value= %d FEC_32_45=%d \n",mod, modcod2fecS2X[((mod & 0x7F) >> 1)], FEC_32_45);

}

