/*
 * dvb_netstream.c: support for DVB to network streaming hardware
 *
 * Copyright (C) 2012-2013 Marcus and Ralph Metzler
 *                         for Digital Devices GmbH
 *                         
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

#include <linux/net.h>
#include "dvb_netstream.h"

static ssize_t ns_write(struct file *file, const char *buf,
			size_t count, loff_t *ppos)
{
	pr_info("%s\n", __func__);
	return 0;
}

static ssize_t ns_read(struct file *file, char *buf,
		       size_t count, loff_t *ppos)
{
	pr_info("%s\n", __func__);
	return 0;
}

static unsigned int ns_poll(struct file *file, poll_table *wait)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int ns_stop(struct dvbnss *nss)
{
	struct dvb_netstream *ns = nss->ns;

	mutex_lock(&ns->mutex);
	if (nss->running && ns->stop) {
		ns->stop(nss);
		nss->running = 0;
	}
	mutex_unlock(&ns->mutex);
	return 0;
}

static int ns_release(struct inode *inode, struct file *file)
{
	struct dvbnss *nss = file->private_data;
	struct dvb_netstream *ns = nss->ns;

	ns_stop(nss);
	if (ns->free)
		ns->free(nss);
	mutex_lock(&ns->mutex);
	list_del(&nss->nssl);
	mutex_unlock(&ns->mutex);
	vfree(nss);
	return 0;
}

static int ns_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_netstream *ns = dvbdev->priv;
	struct dvbnss *nss;

	nss = vmalloc(sizeof(*nss));
	if (!nss)
		return -ENOMEM;
	nss->ns = ns;
	if (ns->alloc && ns->alloc(nss) < 0) {
		vfree(nss);
		return -EBUSY;
	}
	file->private_data = nss;
	nss->running = 0;
	mutex_lock(&ns->mutex);
	list_add(&nss->nssl, &ns->nssl);
	mutex_unlock(&ns->mutex);
	return 0;
}

static int set_net(struct dvbnss *nss, struct dvb_ns_params *p)
{
	return 0;
}

static int do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	struct dvbnss *nss = file->private_data;
	struct dvb_netstream *ns = nss->ns;
	/*unsigned long arg = (unsigned long) parg;*/
	int ret = 0;

	switch (cmd) {
	case NS_SET_RTCP_MSG:
	{
		struct dvb_ns_rtcp *rtcpm = parg;

		if (ns->set_rtcp_msg)
			ret = ns->set_rtcp_msg(nss, rtcpm->msg, rtcpm->len);
		break;
	}

	case NS_SET_NET:
		memcpy(&nss->params, parg, sizeof(nss->params));
		if (ns->set_net)
			ret = ns->set_net(nss);
		else
			ret = set_net(nss, (struct dvb_ns_params *) parg);
		break;

	case NS_START:
		mutex_lock(&ns->mutex);
		if (nss->running) {
			ret = -EBUSY;
		} else if (ns->start) {
			ret = ns->start(nss);
			nss->running = 1;
		}
		mutex_unlock(&ns->mutex);
		break;

	case NS_STOP:
		ns_stop(nss);
		break;

	case NS_SET_PACKETS:
	{
		struct dvb_ns_packet *packet =  parg;

		if (ns->set_ts_packets)
			ret = ns->set_ts_packets(nss, packet->buf,
						 packet->count * 188);
		break;
	}

	case NS_INSERT_PACKETS:
	{
		u8 count = *(u8 *) parg;

		if (ns->insert_ts_packets)
			ret = ns->insert_ts_packets(nss, count);
		break;
	}

	case NS_SET_PID:
	{
		u16 pid = *(u16 *) parg;
		u16 byte = (pid & 0x1fff) >> 3;
		u8 bit = 1 << (pid & 7);

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
		if (ns->set_pid)
			ret = ns->set_pid(nss, pid);
		break;
	}

	case NS_SET_PIDS:
		ret = copy_from_user(nss->pids, *(u8 **) parg, 0x400);
		if (ret < 0)
			return ret;
		if (ns->set_pids)
			ret = ns->set_pids(nss);
		break;

	case NS_SET_CI:
	{
		u8 ci = *(u8 *) parg;

		if (nss->running)
			ret = -EBUSY;
		else if (ns->set_ci)
			ret = ns->set_ci(nss, ci);
		break;
	}

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static long ns_ioctl(struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, do_ioctl);
}

static const struct file_operations ns_fops = {
	.owner   = THIS_MODULE,
	.read    = ns_read,
	.write   = ns_write,
	.open    = ns_open,
	.release = ns_release,
	.poll    = ns_poll,
	.mmap    = 0,
	.unlocked_ioctl = ns_ioctl,
};

static struct dvb_device ns_dev = {
	.priv    = 0,
	.readers = 1,
	.writers = 1,
	.users   = 1,
	.fops    = &ns_fops,
};


int dvb_netstream_init(struct dvb_adapter *dvb_adapter,
		       struct dvb_netstream *ns)
{
	mutex_init(&ns->mutex);
	spin_lock_init(&ns->lock);
	ns->exit = 0;
	dvb_register_device(dvb_adapter, &ns->dvbdev, &ns_dev, ns,
			    DVB_DEVICE_NS);
	INIT_LIST_HEAD(&ns->nssl);
	return 0;
}
EXPORT_SYMBOL(dvb_netstream_init);

void dvb_netstream_release(struct dvb_netstream *ns)
{
	ns->exit = 1;
	if (ns->dvbdev->users > 1) {
		wait_event(ns->dvbdev->wait_queue,
			   ns->dvbdev->users == 1);
	}
	dvb_unregister_device(ns->dvbdev);
}
EXPORT_SYMBOL(dvb_netstream_release);
