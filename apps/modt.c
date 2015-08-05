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


#define SNUM 1000
//671
void send(void)
{
	uint8_t buf[188*SNUM], *cts;
	int i;
	uint32_t c=0;
	int fdo;

	fdo=open("/dev/dvb/adapter0/mod0", O_WRONLY);


	while (1) {
		read(0, buf, sizeof(buf));
		write(fdo, buf, 188*SNUM);
	}
}


int main()
{
	send();
}
