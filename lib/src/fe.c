#if 0

static void release_fe(struct octoserve *os, struct dvbfe *fe)
{
	if (!fe)
		return;
	dbgprintf(DEBUG_SYS, "release fe %d\n", fe->nr);
	fe->state = 2;
	pthread_join(fe->pt, NULL);
}

static struct dvbfe *alloc_fe_num(struct octoserve *os, int i, int type)
{
	struct dvbfe *fe;

	if (i > os->dvbfe_num)
		return NULL;
	dbgprintf(DEBUG_SYS, "alloc_fe_num %d\n", i);
	pthread_mutex_lock(&os->lock);
	fe = &os->dvbfe[i];
	if (fe->state || !(fe->type & (1UL << type))) {
		pthread_mutex_unlock(&os->lock);
		return NULL;
	}
	fe->n_tune = 0;
	fe->state = 1;
	pthread_create(&fe->pt, NULL, (void *) handle_fe, fe); 
	pthread_mutex_unlock(&os->lock);
	dbgprintf(DEBUG_SYS, "Allocated fe %d = %d/%d, fd=%d\n",
		  fe->nr, fe->anum, fe->fnum, fe->fd);
	return fe;
}

static struct dvbfe *alloc_fe(struct octoserve *os, int type)
{
	int i;
	struct dvbfe *fe;

	pthread_mutex_lock(&os->lock);
	for (i = 0; i < os->dvbfe_num; i++) {
		fe = &os->dvbfe[i];
		if (fe->state == 0 && 
		    (fe->type & (1UL << type))) {
			pthread_mutex_unlock(&os->lock);
			return alloc_fe_num(os, i, type);
		}
	}
	pthread_mutex_unlock(&os->lock);
	return NULL;
}
#endif

static struct dddvb_fe *alloc_fe_num(struct dddvb *dd, int num, int type, int source)
{
	if (num > dd->dvbfe_num)
		return NULL;

	
	
}
