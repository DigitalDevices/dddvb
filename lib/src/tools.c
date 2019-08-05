#include "time.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>


int sendlen(int sock, char *buf, int len)
{
	int done, todo; 

	for (todo = len; todo; todo -= done, buf += done)
		if ((done = send(sock, buf, todo, 0)) < 0) {
			printf("sendlen error\n");
			return done;
		}
	return len;
} 

int sendstring(int sock, char *fmt, ...)
{
	int len;
	uint8_t buf[2048];
	va_list args;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	if (len <= 0 || len >= sizeof(buf))
		return 0;
	sendlen(sock, buf, len);
	va_end(args);
	return len;
}

time_t mtime(time_t *t)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		return 0;
	if (t)
		*t = ts.tv_sec;
	return ts.tv_sec;
}
