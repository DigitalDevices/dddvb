#include "src/libdddvb.h"
#include <stdio.h>

int main()
{
	struct dddvb *dd = dddvb_init("", 0xffff);

	if (!dd)
		printf("dddvb_init failed\n");
	printf("dvbnum = %u\n", dd->dvbfe_num);
}
