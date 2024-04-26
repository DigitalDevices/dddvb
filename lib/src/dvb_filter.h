static inline uint16_t seclen(const uint8_t *buf)
{
	return 3+((buf[1]&0x0f)<<8)+buf[2];
}

static inline uint16_t tspid(const uint8_t *buf)
{
	return ((buf[1]&0x1f)<<8)+buf[2];
}

static inline int tspayload(const uint8_t *tsp)
{
	if (!(tsp[3] & 0x10))
		return 0;
	if (tsp[3] & 0x20)
		return (tsp[4] > 183) ? 0 : (183 - tsp[4]);
	return 184;
}

static inline int tspaystart(const uint8_t *tsp)
{
	if (!(tsp[3]&0x10))
		return 188;
	if (tsp[3]&0x20)
		return (tsp[4] >= 184) ? 188 : tsp[4]+5;
	return 4;
}


static inline void pidf_reset(struct dvbf_pid *pidf)
{
	pidf->bufp = pidf->len = 0;
}

static inline void dvbf_init_pid(struct dvbf_pid *pidf, uint16_t pid)
{
	pidf->pid = pid;
	pidf->cc = 0xff;
	pidf_reset(pidf);
}

static inline void write_secbuf(struct dvbf_pid *p, uint8_t *tsp, int n)
{
	memcpy(p->buf+p->bufp, tsp, n);
	p->bufp += n;
}

static inline int validcc(struct dvbf_pid *p, uint8_t *tsp)
{
	uint8_t newcc;
	int valid;

	newcc = tsp[3] & 0x0f;
	if (p->cc == 0xff)
		valid=1;
	else
		valid = (((p->cc + 1) & 0x0f) == newcc) ? 1 : 0;
	p->cc = newcc;
	if (!valid) 
		pidf_reset(p);
	return valid;
}

static int proc_pidf(struct dvbf_pid *p, uint8_t *tsp)
{
	int pusoff, todo, off;

	if (tspid(tsp) != p->pid)
		return 0;
	if (!(tsp[3] & 0x10))  //no payload
		return 0;
	todo = (tsp[3] & 0x20) ?  // AF?
		((tsp[4] > 183) ? 0 : (183 - tsp[4])) :
		184;
	if (!todo)
		return 0;
	off = 188 - todo;
	pusoff = (tsp[1] & 0x40) ? tsp[off++] : todo;
	if (pusoff + off > 188)
		goto error;
	if (validcc(p, tsp) && pusoff && p->bufp) {
		int rlen = pusoff;
		if (p->len) {
			if (p->bufp + rlen > p->len)
				rlen = p->len - p->bufp;
		} else
			if (p->bufp + rlen > 4096)
				rlen = 4096 - p->bufp;
		write_secbuf(p, tsp + off, rlen);
		if (!p->len && p->bufp >= 3 && (p->len = seclen(p->buf)) > 4096) 
			pidf_reset(p);
		else {
			if (p->cb)
				p->cb(p);
			else
				return 1;
		}
	}
	off += pusoff;
	while ((todo = 188 - off) > 0 && tsp[off] != 0xff) {
		pidf_reset(p);
		if (todo < 3 || (p->len = seclen(tsp+off)) > todo) {
			if (p->len > 4096)
				goto error;
			write_secbuf(p, tsp+off, todo);
			off+=todo;
		} else {
			write_secbuf(p, tsp+off, p->len);
			off+=p->len;
			if (p->cb)
				p->cb(p);
			else
				return 2;
		}
	}
	return 0;

error:
	pidf_reset(p);
	return -1;
}
