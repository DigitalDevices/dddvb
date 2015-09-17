/*
 * ddbridge-ns.c: Digital Devices PCIe bridge driver net streaming
 *
 * Copyright (C) 2010-2015 Marcus Metzler <mocm@metzlerbros.de>
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

static int ddb_dvb_ns_input_start(struct ddb_input *input);
static int ddb_dvb_ns_input_stop(struct ddb_input *input);

static u16 calc_pcs(struct dvb_ns_params *p)
{
	u32 sum = 0;
	u16 pcs;

	sum += (p->sip[0] << 8) | p->sip[1];
	sum += (p->sip[2] << 8) | p->sip[3];
	sum += (p->dip[0] << 8) | p->dip[1];
	sum += (p->dip[2] << 8) | p->dip[3];
	sum += 0x11; /* UDP proto */
	sum = (sum >> 16) + (sum & 0xffff);
	pcs = sum;
	return pcs;
}

static u16 calc_pcs16(struct dvb_ns_params *p, int ipv)
{
	u32 sum = 0, i;
	u16 pcs;

	for (i = 0; i < ipv ? 16 : 4; i += 2) {
		sum += (p->sip[i] << 8) | p->sip[i + 1];
		sum += (p->dip[i] << 8) | p->dip[i + 1];
	}
	sum += 0x11; /* UDP proto */
	sum = (sum >> 16) + (sum & 0xffff);
	pcs = sum;
	return pcs;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void ns_free(struct dvbnss *nss)
{
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;

	mutex_lock(&dev->mutex);
	dns->input = 0;
	mutex_unlock(&dev->mutex);
}

static int ns_alloc(struct dvbnss *nss)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	int i, ret = -EBUSY;

	mutex_lock(&dev->mutex);
	for (i = 0; i < dev->ns_num; i++) {
		if (dev->ns[i].input)
			continue;
		dev->ns[i].input = input;
		dev->ns[i].fe = input;
		nss->priv = &dev->ns[i];
		ret = 0;
		/*pr_info("%s i=%d fe=%d\n", __func__, i, input->nr); */
		break;
	}
	ddbwritel(dev, 0x03, RTP_MASTER_CONTROL);
	mutex_unlock(&dev->mutex);
	return ret;
}

static int ns_set_pids(struct dvbnss *nss)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;

	if (dev->link[0].ids.devid == 0x0301dd01) {
		u32 sys = 0;
		int pid, j = 1;

		sys |= nss->pids[0] & 3;
		sys |= (nss->pids[2] & 0x1f) << 4;
		ddbwritel(dev, sys, PID_FILTER_SYSTEM_PIDS(dns->nr));
		for (pid = 20; j < 5 && pid < 8192; pid++)
			if (nss->pids[pid >> 3] & (1 << (pid & 7))) {
				ddbwritel(dev, 0x8000 | pid,
					  PID_FILTER_PID(dns->nr, j));
				j++;
			}
		/* disable unused pids */
		for (; j < 5; j++)
			ddbwritel(dev, 0, PID_FILTER_PID(dns->nr, j));
	} else
		ddbcpyto(dev, STREAM_PIDS(dns->nr), nss->pids, 0x400);
	return 0;
}

static int ns_set_pid(struct dvbnss *nss, u16 pid)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	u16 byte = (pid & 0x1fff) >> 3;
	u8 bit = 1 << (pid & 7);
	u32 off = STREAM_PIDS(dns->nr);

#if 1
	if (dev->link[0].ids.devid == 0x0301dd01) {
		if (pid & 0x2000) {
			if (pid & 0x8000)
				memset(nss->pids, 0xff, 0x400);
			else
				memset(nss->pids, 0x00, 0x400);
		} else {
			if (pid & 0x8000)
				nss->pids[byte] |= bit;
			else
				nss->pids[byte] &= ~bit;
		}
		ns_set_pids(nss);
	} else {
		if (pid & 0x2000) {
			if (pid & 0x8000)
				ddbmemset(dev, off, 0xff, 0x400);
			else
				ddbmemset(dev, off, 0x00, 0x400);
		} else {
			u8 val = ddbreadb(dev, off + byte);

			if (pid & 0x8000)
				ddbwriteb(dev, val | bit, off + byte);
			else
				ddbwriteb(dev, val & ~bit, off + byte);
		}
	}
#else
	ddbcpyto(dev, STREAM_PIDS(dns->nr), nss->pids, 0x400);
#endif
	return 0;
}

static int citoport(struct ddb *dev, u8 ci)
{
	int i, j;

	for (i = j = 0; i < dev->link[0].info->port_num; i++) {
		if (dev->port[i].class == DDB_PORT_CI) {
			if (j == ci)
				return i;
			j++;
		}
	}
	return -1;
}

static int ns_set_ci(struct dvbnss *nss, u8 ci)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	int ciport;

	if (ci == 255) {
		dns->fe = input;
		return 0;
	}
	ciport = citoport(dev, ci);
	if (ciport < 0)
		return -EINVAL;
	
	pr_info("input %d.%d to ci %d at port %d\n", input->port->lnr, input->nr, ci, ciport);
	ddbwritel(dev, (input->port->lnr << 21) | (input->nr << 16) | 0x1c, TS_OUTPUT_CONTROL(ciport));
	usleep_range(1, 5);
	ddbwritel(dev, (input->port->lnr << 21) | (input->nr << 16) | 0x1d, TS_OUTPUT_CONTROL(ciport));
	dns->fe = dev->port[ciport].input[0];
	return 0;
}

static u8 rtp_head[]  = {
	0x80, 0x21,
	0x00, 0x00, /* seq number */
	0x00, 0x00, 0x00, 0x00, /* time stamp*/
	0x91, 0x82, 0x73, 0x64, /* SSRC */
};

static u8 rtcp_head[] = {
	/* SR off 42:8 len 28*/
	0x80, 0xc8, /* SR type */
	0x00, 0x06, /* len  */
	0x91, 0x82, 0x73, 0x64, /* SSRC */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* NTP */
	0x73, 0x64, 0x00, 0x00, /* RTP TS */
	0x00, 0x00, 0x00, 0x00, /* packet count */
	0x00, 0x00, 0x00, 0x00, /* octet count */
	/* SDES off 70:36 len 20 */
	0x81, 0xca, /* SDES */
	0x00, 0x03, /* len */
	0x91, 0x82, 0x73, 0x64, /* SSRC */
	0x01, 0x05, /* CNAME item */
	0x53, 0x41, 0x54, 0x49, 0x50, /* "SATIP" */
	0x00, /* item type 0 */
	/*  APP off 86:52 len 16+string  length */
	0x80, 0xcc, /* APP */
	0x00, 0x04, /* len */
	0x91, 0x82, 0x73, 0x64, /* SSRC */
	0x53, 0x45, 0x53, 0x31, /* "SES1" */
	0x00, 0x00, /* identifier */
	0x00, 0x00, /* string length */
	/* string off 102:68 */
};

static int ns_set_rtcp_msg(struct dvbnss *nss, u8 *msg, u32 len)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	u32 off = STREAM_PACKET_ADR(dns->nr);
	u32 coff = 96;
	u16 wlen;

	if (!len) {
		ddbwritel(dev, ddbreadl(dev, STREAM_CONTROL(dns->nr)) &
			  ~0x10,
			  STREAM_CONTROL(dns->nr));
		return 0;
	}
	if (copy_from_user(dns->p + coff + dns->rtcp_len, msg, len))
		return -EFAULT;
	dns->p[coff + dns->rtcp_len - 2] = (len >> 8);
	dns->p[coff + dns->rtcp_len - 1] = (len & 0xff);
	if (len & 3) {
		u32 pad = 4 - (len & 3);

		memset(dns->p + coff + dns->rtcp_len + len, 0, pad);
		len += pad;
	}
	wlen = len / 4;
	wlen += 3;
	dns->p[coff + dns->rtcp_len - 14] = (wlen >> 8);
	dns->p[coff + dns->rtcp_len - 13] = (wlen & 0xff);
	ddbcpyto(dev, off, dns->p, sizeof(dns->p));
	ddbwritel(dev, (dns->rtcp_udplen + len) |
		  ((STREAM_PACKET_OFF(dns->nr) + coff) << 16),
		  STREAM_RTCP_PACKET(dns->nr));
	ddbwritel(dev, ddbreadl(dev, STREAM_CONTROL(dns->nr)) | 0x10,
		  STREAM_CONTROL(dns->nr));
	return 0;
}

static u32 set_nsbuf(struct dvb_ns_params *p, u8 *buf,
		     u32 *udplen, int rtcp, int vlan)
{
	u32 c = 0;
	u16 pcs;
	u16 sport, dport;

	sport = rtcp ? p->sport2 : p->sport;
	dport = rtcp ? p->dport2 : p->dport;

	/* MAC header */
	memcpy(buf + c, p->dmac, 6);
	memcpy(buf + c + 6, p->smac, 6);
	c += 12;
	if (vlan) {
		buf[c + 0] = 0x81;
		buf[c + 1] = 0x00;
		buf[c + 2] = ((p->qos & 7) << 5) | ((p->vlan & 0xf00) >> 8);
		buf[c + 3] = p->vlan & 0xff;
		c += 4;
	}
	buf[c + 0] = 0x08;
	buf[c + 1] = 0x00;
	c += 2;

	/* IP header */
	if (p->flags & DVB_NS_IPV6) {
		u8 ip6head[8]  = { 0x65, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x11, 0x00, };
		memcpy(buf + c, ip6head, sizeof(ip6head));
		buf[c + 7] = p->ttl;
		memcpy(buf + c +  8, p->sip, 16);
		memcpy(buf + c + 24, p->dip, 16);
		c += 40;

		/* UDP */
		buf[c + 0] = sport >> 8;
		buf[c + 1] = sport & 0xff;
		buf[c + 2] = dport >> 8;
		buf[c + 3] = dport & 0xff;
		buf[c + 4] = 0; /* length */
		buf[c + 5] = 0;
		pcs = calc_pcs16(p, p->flags & DVB_NS_IPV6);
		buf[c + 6] = pcs >> 8;
		buf[c + 7] = pcs & 0xff;
		c += 8;
		*udplen = 8;

	} else {
		u8 ip4head[12]  = { 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
				    0x40, 0x00, 0x40, 0x11, 0x00, 0x00 };

		memcpy(buf + c, ip4head, sizeof(ip4head));
		buf[c + 8] = p->ttl;
		memcpy(buf + c + 12, p->sip, 4);
		memcpy(buf + c + 16, p->dip, 4);
		c += 20;

		/* UDP */
		buf[c + 0] = sport >> 8;
		buf[c + 1] = sport & 0xff;
		buf[c + 2] = dport >> 8;
		buf[c + 3] = dport & 0xff;
		buf[c + 4] = 0; /* length */
		buf[c + 5] = 0;
		pcs = calc_pcs(p);
		buf[c + 6] = pcs >> 8;
		buf[c + 7] = pcs & 0xff;
		c += 8;
		*udplen = 8;
	}

	if (rtcp) {
		memcpy(buf + c, rtcp_head, sizeof(rtcp_head));
		memcpy(buf + c +  4, p->ssrc, 4);
		memcpy(buf + c + 32, p->ssrc, 4);
		memcpy(buf + c + 48, p->ssrc, 4);
		c += sizeof(rtcp_head);
		*udplen += sizeof(rtcp_head);
	} else if (p->flags & DVB_NS_RTP) {
		memcpy(buf + c, rtp_head, sizeof(rtp_head));
		memcpy(buf + c + 8, p->ssrc, 4);
		c += sizeof(rtp_head);
		*udplen += sizeof(rtp_head);
	}
	return c;
}

static int ns_set_ts_packets(struct dvbnss *nss, u8 *buf, u32 len)
{
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	u32 off = STREAM_PACKET_ADR(dns->nr);

	if (nss->params.flags & DVB_NS_RTCP)
		return -EINVAL;

	if (copy_from_user(dns->p + dns->ts_offset, buf, len))
		return -EFAULT;
	ddbcpyto(dev, off, dns->p, sizeof(dns->p));
	return 0;
}

static int ns_insert_ts_packets(struct dvbnss *nss, u8 count)
{
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	u32 value = count;

	if (nss->params.flags & DVB_NS_RTCP)
		return -EINVAL;

	if (count < 1 || count > 2)
		return -EINVAL;

	ddbwritel(dev, value, STREAM_INSERT_PACKET(dns->nr));
	return 0;
}

static int ns_set_net(struct dvbnss *nss)
{
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	struct dvb_ns_params *p = &nss->params;
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	u32 off = STREAM_PACKET_ADR(dns->nr);
	u32 coff = 96;

	dns->ts_offset = set_nsbuf(p, dns->p, &dns->udplen, 0, dev->vlan);
	if (nss->params.flags & DVB_NS_RTCP)
		dns->rtcp_len = set_nsbuf(p, dns->p + coff,
					  &dns->rtcp_udplen, 1, dev->vlan);
	ddbcpyto(dev, off, dns->p, sizeof(dns->p));
	ddbwritel(dev, dns->udplen | (STREAM_PACKET_OFF(dns->nr) << 16),
		  STREAM_RTP_PACKET(dns->nr));
	ddbwritel(dev, dns->rtcp_udplen |
		  ((STREAM_PACKET_OFF(dns->nr) + coff) << 16),
		  STREAM_RTCP_PACKET(dns->nr));
	return 0;
}

static int ns_start(struct dvbnss *nss)
{
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;
	u32 reg = 0x8003;

	if (nss->params.flags & DVB_NS_RTCP)
		reg |= 0x10;
	if (nss->params.flags & DVB_NS_RTP_TO)
		reg |= 0x20;
	if (nss->params.flags & DVB_NS_RTP)
		reg |= 0x40;
	if (nss->params.flags & DVB_NS_IPV6)
		reg |= 0x80;
	if (dns->fe != input)
		ddb_dvb_ns_input_start(dns->fe);
	ddb_dvb_ns_input_start(input);
	printk("ns start ns %u, fe %u link %u\n", dns->nr, dns->fe->nr, dns->fe->port->lnr);
	ddbwritel(dev, reg | (dns->fe->nr << 8) | (dns->fe->port->lnr << 16),
		  STREAM_CONTROL(dns->nr));
	return 0;
}

static int ns_stop(struct dvbnss *nss)
{
	struct ddb_ns *dns = (struct ddb_ns *) nss->priv;
	struct dvb_netstream *ns = nss->ns;
	struct ddb_input *input = ns->priv;
	struct ddb *dev = input->port->dev;

	ddbwritel(dev, 0x00, STREAM_CONTROL(dns->nr));
	ddb_dvb_ns_input_stop(input);
	if (dns->fe != input)
		ddb_dvb_ns_input_stop(dns->fe);
	return 0;
}

static int netstream_init(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_adapter *adap = dvb->adap;
	struct dvb_netstream *ns = &dvb->dvbns;
	struct ddb *dev = input->port->dev;
	int i, res;

	ddbmemset(dev, STREAM_PIDS(input->nr), 0x00, 0x400);
	for (i = 0; i < dev->ns_num; i++)
		dev->ns[i].nr = i;
	ns->priv = input;
	ns->set_net = ns_set_net;
	ns->set_rtcp_msg = ns_set_rtcp_msg;
	ns->set_ts_packets = ns_set_ts_packets;
	ns->insert_ts_packets = ns_insert_ts_packets;
	ns->set_pid = ns_set_pid;
	ns->set_pids = ns_set_pids;
	ns->set_ci = ns_set_ci;
	ns->start = ns_start;
	ns->stop = ns_stop;
	ns->alloc = ns_alloc;
	ns->free = ns_free;
	res = dvb_netstream_init(adap, ns);
	return res;
}
