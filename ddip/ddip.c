#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>


static __init int init_ddip(void)
{
	

	return 0;
}

static __exit void exit_ddip(void)
{
}

module_init(init_ddip);
module_exit(exit_ddip);

MODULE_DESCRIPTION("GPL");
MODULE_AUTHOR("Metzler Brothers Systementwicklung");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

