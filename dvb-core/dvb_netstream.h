/*
 * dvb_netstream.c: support for DVB to network streaming hardware
 *
 * Copyright (C) 2012-2013 Marcus and Ralph Metzler
 *                         for Digital Devices GmbH
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

#ifndef _DVB_NETSTREAM_H_
#define _DVB_NETSTREAM_H_

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <linux/dvb/ns.h>

#include "dvbdev.h"

#define DVBNS_MAXPIDS 32

struct dvbnss {
	struct dvb_netstream *ns;
	void *priv;

	u8  pids[1024];
	u8  packet[1328];
	u32 pp;

	struct socket *sock;
	struct sockaddr_in sadr;
	u32    sn;

	struct dvb_ns_params params;

	struct list_head nssl;
	int                running;
};

#define MAX_DVBNSS 32

struct dvb_netstream {
	void              *priv;

	struct mutex       mutex;
	spinlock_t         lock;
	struct dvb_device *dvbdev;
	int                exit;

	struct list_head nssl;

	int (*set_net)(struct dvbnss *);
	int (*set_pid)(struct dvbnss *, u16);
	int (*set_pids)(struct dvbnss *);
	int (*set_ci)(struct dvbnss *, u8);
	int (*set_rtcp_msg)(struct dvbnss *, u8 *, u32);
	int (*set_ts_packets)(struct dvbnss *, u8 *, u32);
	int (*insert_ts_packets)(struct dvbnss *, u8);
	int (*start)(struct dvbnss *);
	int (*stop)(struct dvbnss *);
	int (*alloc)(struct dvbnss *);
	void (*free)(struct dvbnss *);
};


void dvb_netstream_release(struct dvb_netstream *);
int  dvb_netstream_init(struct dvb_adapter *, struct dvb_netstream *);


#endif
