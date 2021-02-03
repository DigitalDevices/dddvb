#include "time.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>


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

int sendstringx(int sock, const char *label, uint32_t text_length, const uint8_t *text)
{
	if (sock < 0)
		return(-1);

	uint8_t buf[strlen(label) + 1 + text_length * 3 + 2];

	strcpy(buf, label);
	int len = strlen(buf);
	buf[len++] = 0x20;
	while (text_length--) {
		int c = *text++;
		if (c == 0x5C) {
			buf[len++] = 0x5C;
			buf[len++] = 0x5C;
		} else if ((c < 0x20) || (c > 0x7E)) {
			buf[len++] = 0x5C;
			snprintf(buf+len, 3, "%02X", c);
			len += 2;
		} else {
			buf[len++] = (uint8_t) c;
		}
	}
	buf[len++] = 0x0A;
	sendlen(sock, buf, len);
	return(len);
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
