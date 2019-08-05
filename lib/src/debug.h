#ifndef _DDDVB_DEBUG_H_
#define _DDDVB_DEBUG_H_

#include "tools.h"

#if BUILDING_LIBDDDVB
extern uint32_t dddvb_debug;
#endif

#define DEBUG_RTSP     1
#define DEBUG_SSDP     2
#define DEBUG_NET      4
#define DEBUG_SYS      8
#define DEBUG_DVB     16
#define DEBUG_IGMP    32
#define DEBUG_SWITCH  64
#define DEBUG_CA     128
#define DEBUG_DEBUG  256


#if 0
#define dbgprintf(_mask_, ...) \
	 do { if (dddvb_debug & _mask_) fprintf(stderr, __VA_ARGS__); } while (0) 
#else
#define dbgprintf(_mask_, ...) \
         do { if (dddvb_debug & _mask_) { fprintf(stderr, "[%5u] ", mtime(NULL)); \
			            fprintf(stderr, __VA_ARGS__); } } while (0) 
#endif


#endif
