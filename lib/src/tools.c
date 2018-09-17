#include "time.h"

time_t mtime(time_t *t)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
		return 0;
	if (t)
		*t = ts.tv_sec;
	return ts.tv_sec;
}
