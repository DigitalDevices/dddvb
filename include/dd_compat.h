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
#define DVB_DEVICE_MOD 6
#define DVB_DEVICE_NS 7
#define DVB_DEVICE_NSD 8

#define SYS_DVBC2 19
#define ROLLOFF_15 4
#define ROLLOFF_10 5
#define ROLLOFF_5 6

#define	FEC_1_4 13
#define	FEC_1_3 14

#define APSK_64 14
#define APSK_128 15
#define APSK_256 16
#endif
