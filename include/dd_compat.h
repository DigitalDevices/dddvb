#include <linux/version.h>

#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
#define __devexit
#define __devinit
#define __devinitconst
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef fallthrough
#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif
#endif


#ifdef KERNEL_DVB_CORE
#define DVB_DEVICE_CI 0
#define DVB_DEVICE_NS 6
#define DVB_DEVICE_NSD 7
#define DVB_DEVICE_MOD 8

#define APSK_128 21
#define APSK_256 22
#define APSK_256_L 23
#endif
