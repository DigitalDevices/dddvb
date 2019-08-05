#include "libdddvb.h"
#include "dddvb.h"
#include "debug.h"
#include <string.h>

LIBDDDVB_EXPORTED uint32_t dddvb_debug;
LIBDDDVB_EXPORTED struct dddvb *global_dd = NULL;
LIBDDDVB_EXPORTED pthread_mutex_t dddvb_mutex = PTHREAD_MUTEX_INITIALIZER; 

void __attribute__ ((constructor)) setup(void) {
	printf("SETUP\n");
}

LIBDDDVB_EXPORTED struct dddvb_fe *dddvb_fe_alloc_num(struct dddvb *dd, uint32_t type, uint32_t num)
{
	struct dddvb_fe *fe;
	
	if (num >= dd->dvbfe_num)
		return NULL;
	dbgprintf(DEBUG_SYS, "alloc_fe_num %u type %u\n", num, type);
	pthread_mutex_lock(&dd->lock);
	fe = &dd->dvbfe[num];
	if (fe->state || !(fe->type & (1UL << type))) {
		dbgprintf(DEBUG_SYS, "fe %d  state = %d, type = %08x wanted %08x\n",
			  fe->nr, fe->state, fe->type, 1UL << type);
		pthread_mutex_unlock(&dd->lock);
		return NULL;
	}
	fe->state = 1;
	pthread_mutex_unlock(&dd->lock);
	if (dddvb_fe_start(fe) < 0) {
		dbgprintf(DEBUG_SYS, "fe %d busy\n", fe->nr);
		return 0;
	}
	dbgprintf(DEBUG_SYS, "Allocated fe %d = %d/%d, fd=%d\n",
		  fe->nr, fe->anum, fe->fnum, fe->fd);
	return fe;
}

LIBDDDVB_EXPORTED struct dddvb_fe *dddvb_fe_alloc(struct dddvb *dd, uint32_t type)
{
	int i;
	struct dddvb_fe *fe = NULL;

	pthread_mutex_lock(&dd->lock);
	dbgprintf(DEBUG_SYS, "alloc_fe type %u\n", type);
	for (i = 0; i < dd->dvbfe_num; i++) {
		fe = &dd->dvbfe[i];
		if (fe->state == 0 && 
		    (fe->type & (1UL << type))) {
			fe = dddvb_fe_alloc_num(dd, type, i);
			if (fe)
				break;
		}
	}
	pthread_mutex_unlock(&dd->lock);
	return fe;

}

LIBDDDVB_EXPORTED int dddvb_dvb_tune(struct dddvb_fe *fe, struct dddvb_params *p)
{
	return dddvb_fe_tune(fe, p);
}

LIBDDDVB_EXPORTED struct dddvb *dddvb_init(char *config, uint32_t flags)
{
	struct dddvb *dd;
	pthread_mutexattr_t mta;

	dddvb_debug = flags;

	pthread_mutex_lock(&dddvb_mutex);
	if (global_dd) {
		pthread_mutex_unlock(&dddvb_mutex);
		return global_dd;
	}

	dd = calloc(1, sizeof(struct dddvb));
	if (!dd)
		goto fail;
	dd->config[0] = 0;
	if (config && strlen(config) < 80)
		strcpy(dd->config, config);
	
	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&dd->lock, &mta);
	
	dddvb_dvb_init(dd);
	global_dd = dd;
fail:
	pthread_mutex_unlock(&dddvb_mutex);
	return dd;
}
