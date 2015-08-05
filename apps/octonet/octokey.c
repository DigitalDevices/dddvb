#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>

#include <linux/input.h>


int main(int argc, char *argv[])
{
	int fd, len;
	struct input_event ev;
	uint32_t time;

	fd = open("/dev/input/event0", O_RDONLY);

	if (fd < 0)
		return -1;
	
	while (1) {
		if ((len = read(fd, &ev, sizeof(ev)) < sizeof(struct input_event)))
			return -1;
		printf("%u.%06u %u %u %u\n", ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value); 
	}
	

}
