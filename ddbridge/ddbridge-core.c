// SPDX-License-Identifier: GPL-2.0
/*
 * ddbridge-core.c: Digital Devices bridge core functions
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         Ralph Metzler <rjkm@metzlerbros.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ddbridge.h"
#include "ddbridge-i2c.h"
#include "ddbridge-io.h"
#include "ddbridge-ioctl.h"
#include <media/dvb_net.h>

struct workqueue_struct *ddb_wq;

static DEFINE_MUTEX(redirect_lock); /* lock for redirect */

static int adapter_alloc;
module_param(adapter_alloc, int, 0444);
MODULE_PARM_DESC(adapter_alloc,
		 "0-one adapter per io, 1-one per tab with io, 2-one per tab, 3-one for all");

static int ci_bitrate = 70000;
module_param(ci_bitrate, int, 0444);
MODULE_PARM_DESC(ci_bitrate, " Bitrate in KHz for output to CI.");

static int ts_loop = -1;
module_param(ts_loop, int, 0444);
MODULE_PARM_DESC(ts_loop, "TS in/out test loop on port ts_loop");

static int dummy_tuner;
module_param(dummy_tuner, int, 0444);
MODULE_PARM_DESC(dummy_tuner,
		 "attach dummy tuner to port 0 of supported cards");

static int vlan;
module_param(vlan, int, 0444);
MODULE_PARM_DESC(vlan, "VLAN and QoS IDs enabled");

static int xo2_speed = 2;
module_param(xo2_speed, int, 0444);
MODULE_PARM_DESC(xo2_speed, "default transfer speed for xo2 based duoflex, 0=55,1=75,2=90,3=104 MBit/s, default=2, use attribute to change for individual cards");

static int raw_stream;
module_param(raw_stream, int, 0444);
MODULE_PARM_DESC(raw_stream, "send data as raw stream to DVB layer");

#ifdef __arm__
static int alt_dma = 1;
#else
static int alt_dma;
#endif
module_param(alt_dma, int, 0444);
MODULE_PARM_DESC(alt_dma, "use alternative DMA buffer handling");

static int no_init;
module_param(no_init, int, 0444);
MODULE_PARM_DESC(no_init, "do not initialize most devices");

static int stv0910_single;
module_param(stv0910_single, int, 0444);
MODULE_PARM_DESC(stv0910_single, "use stv0910 cards as single demods");

static int dma_buf_num = 8;
module_param(dma_buf_num, int, 0444);
MODULE_PARM_DESC(dma_buf_num, "dma buffer number, possible values: 8-32");

static int dma_buf_size = 21;
module_param(dma_buf_size, int, 0444);
MODULE_PARM_DESC(dma_buf_size,
		 "dma buffer size as multiple of 128*47, possible values: 1-43");

#define DDB_MAX_ADAPTER 64
static struct ddb *ddbs[DDB_MAX_ADAPTER];

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

/* copied from dvb-core/dvbdev.c because kernel version does not export it */

int ddb_dvb_usercopy(struct file *file,
		     unsigned int cmd, unsigned long arg,
		     int (*func)(struct file *file,
				 unsigned int cmd, void *arg))
{
	char    sbuf[128];
	void    *mbuf = NULL;
	void    *parg = NULL;
	int     err  = -EINVAL;
	
	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		/*
		 * For this command, the pointer is actually an integer
		 * argument.
		 */
		parg = (void *) arg;
		break;
	case _IOC_READ: /* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (copy_from_user(parg, (void __user *)arg, _IOC_SIZE(cmd)))
			goto out;
		break;
	}

	/* call driver */
	if ((err = func(file, cmd, parg)) == -ENOIOCTLCMD)
		err = -ENOTTY;

	if (err < 0)
		goto out;

	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd))
	{
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void __user *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	kfree(mbuf);
	return err;
}

/****************************************************************************/

struct ddb_irq *ddb_irq_set(struct ddb *dev, u32 link, u32 nr,
			    void (*handler)(void *), void *data)
{
	struct ddb_irq *irq = &dev->link[link].irq[nr];

	irq->handler = handler;
	irq->data = data;
	return irq;
}
EXPORT_SYMBOL(ddb_irq_set);

static void ddb_set_dma_table(struct ddb_io *io)
{
	struct ddb *dev = io->port->dev;
	struct ddb_dma *dma = io->dma;
	u32 i;
	u64 mem;

	if (!dma)
		return;
	for (i = 0; i < dma->num; i++) {
		mem = dma->pbuf[i];
		ddbwritel(dev, mem & 0xffffffff, dma->bufregs + i * 8);
		ddbwritel(dev, mem >> 32, dma->bufregs + i * 8 + 4);
	}
	dma->bufval = ((dma->div & 0x0f) << 16) |
		((dma->num & 0x1f) << 11) |
		((dma->size >> 7) & 0x7ff);
}

static void ddb_set_dma_tables(struct ddb *dev)
{
	u32 i;

	for (i = 0; i < DDB_MAX_PORT; i++) {
		if (dev->port[i].input[0])
			ddb_set_dma_table(dev->port[i].input[0]);
		if (dev->port[i].input[1])
			ddb_set_dma_table(dev->port[i].input[1]);
		if (dev->port[i].output)
			ddb_set_dma_table(dev->port[i].output);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void ddb_redirect_dma(struct ddb *dev,
			     struct ddb_dma *sdma,
			     struct ddb_dma *ddma)
{
	u32 i, base;
	u64 mem;

	sdma->bufval = ddma->bufval;
	base = sdma->bufregs;
	for (i = 0; i < ddma->num; i++) {
		mem = ddma->pbuf[i];
		ddbwritel(dev, mem & 0xffffffff, base + i * 8);
		ddbwritel(dev, mem >> 32, base + i * 8 + 4);
	}
}

static int ddb_unredirect(struct ddb_port *port)
{
	struct ddb_input *oredi, *iredi = NULL;
	struct ddb_output *iredo = NULL;

	mutex_lock(&redirect_lock);
	if (port->output->dma->running) {
		mutex_unlock(&redirect_lock);
		return -EBUSY;
	}
	oredi = port->output->redi;
	if (!oredi)
		goto done;
	if (port->input[0]) {
		iredi = port->input[0]->redi;
		iredo = port->input[0]->redo;

		if (iredo) {
			iredo->port->output->redi = oredi;
			if (iredo->port->input[0]) {
				iredo->port->input[0]->redi = iredi;
				ddb_redirect_dma(oredi->port->dev,
						 oredi->dma, iredo->dma);
			}
			port->input[0]->redo = NULL;
			ddb_set_dma_table(port->input[0]);
		}
		oredi->redi = iredi;
		port->input[0]->redi = NULL;
	}
	oredi->redo = NULL;
	port->output->redi = NULL;

	ddb_set_dma_table(oredi);
done:
	mutex_unlock(&redirect_lock);
	return 0;
}

static int ddb_redirect(u32 i, u32 p)
{
	struct ddb *idev = ddbs[(i >> 4) & 0x3f];
	struct ddb_input *input, *input2;
	struct ddb *pdev = ddbs[(p >> 4) & 0x3f];
	struct ddb_port *port;

	if (!pdev || !idev)
		return -EINVAL;
	if (!pdev->has_dma || !idev->has_dma)
		return -EINVAL;

	port = &pdev->port[p & 0x0f];
	if (!port->output)
		return -EINVAL;
	if (ddb_unredirect(port))
		return -EBUSY;

	if (i == 8)
		return 0;

	input = &idev->input[i & 7];
	if (!input)
		return -EINVAL;

	mutex_lock(&redirect_lock);
	if (port->output->dma->running || input->dma->running) {
		mutex_unlock(&redirect_lock);
		return -EBUSY;
	}
	input2 = port->input[0];
	if (input2) {
		if (input->redi) {
			input2->redi = input->redi;
			input->redi = NULL;
		} else {
			input2->redi = input;
		}
	}
	input->redo = port->output;
	port->output->redi = input;

	ddb_redirect_dma(input->port->dev, input->dma, port->output->dma);
	mutex_unlock(&redirect_lock);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void dma_free(struct pci_dev *pdev, struct ddb_dma *dma, int dir)
{
	int i;

	if (!dma)
		return;
	for (i = 0; i < dma->num; i++) {
		if (dma->vbuf[i]) {
			if (alt_dma) {
				dma_unmap_single(&pdev->dev, dma->pbuf[i],
						 dma->size,
						 dir ? DMA_TO_DEVICE :
						 DMA_FROM_DEVICE);
				kfree(dma->vbuf[i]);
			} else {
				dma_free_coherent(&pdev->dev, dma->size,
						  dma->vbuf[i],
						  dma->pbuf[i]);
			}
			dma->vbuf[i] = 0;
		}
	}
}

static int dma_alloc(struct pci_dev *pdev, struct ddb_dma *dma, int dir)
{
	int i;

	if (!dma)
		return 0;
	for (i = 0; i < dma->num; i++) {
		if (alt_dma) {
#if (KERNEL_VERSION(4, 13, 0) > LINUX_VERSION_CODE)
			dma->vbuf[i] = kzalloc(dma->size, __GFP_REPEAT);
#else
			dma->vbuf[i] = kzalloc(dma->size, __GFP_RETRY_MAYFAIL);
#endif
			if (!dma->vbuf[i])
				return -ENOMEM;
			dma->pbuf[i] = dma_map_single(&pdev->dev,
						      dma->vbuf[i],
						      dma->size,
						      dir ? DMA_TO_DEVICE :
						      DMA_FROM_DEVICE);
			if (dma_mapping_error(&pdev->dev, dma->pbuf[i])) {
				kfree(dma->vbuf[i]);
				dma->vbuf[i] = 0;
				return -ENOMEM;
			}
		} else {
			dma->vbuf[i] = dma_alloc_coherent(&pdev->dev,
							  dma->size,
							  &dma->pbuf[i],
							  GFP_KERNEL | __GFP_ZERO);
			if (!dma->vbuf[i])
				return -ENOMEM;
		}
		if (((u64)dma->vbuf[i] & 0xfff))
			dev_err(&pdev->dev, "DMA memory at %px not aligned!\n", dma->vbuf[i]);
	}
	return 0;
}

static int ddb_buffers_alloc(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
		switch (port->class) {
		case DDB_PORT_TUNER:
			if (port->input[0]->dma)
				if (dma_alloc(dev->pdev,
					      port->input[0]->dma, 0) < 0)
					return -1;
			if (port->input[1]->dma)
				if (dma_alloc(dev->pdev,
					      port->input[1]->dma, 0) < 0)
					return -1;
			break;
		case DDB_PORT_CI:
		case DDB_PORT_LOOP:
			if (port->input[0]->dma)
				if (dma_alloc(dev->pdev,
					      port->input[0]->dma, 0) < 0)
					return -1;
			fallthrough;
		case DDB_PORT_MOD:
			if (port->output->dma)
				if (dma_alloc(dev->pdev,
					      port->output->dma, 1) < 0)
					return -1;
			break;
		default:
			break;
		}
	}
	ddb_set_dma_tables(dev);
	return 0;
}

void ddb_buffers_free(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];

		if (port->input[0] && port->input[0]->dma)
			dma_free(dev->pdev, port->input[0]->dma, 0);
		if (port->input[1] && port->input[1]->dma)
			dma_free(dev->pdev, port->input[1]->dma, 0);
		if (port->output && port->output->dma)
			dma_free(dev->pdev, port->output->dma, 1);
	}
}

/*
 * Control:
 *
 * Bit 0 - Enable TS
 *     1 - Reset
 *     2 - clock enable
 *     3 - clock phase
 *     4 - gap enable
 *     5 - send null packets on underrun
 *     6 - enable clock gating
 *     7 - set error bit on inserted null packets
 *     8-10 - fine adjust clock delay
 *     11- HS (high speed), if NCO mode=0: 0=72MHz 1=96Mhz
 *     12- enable NCO mode
 *
 * Control 2:
 *
 * Bit 0-6  : gap_size, Gap = (gap_size * 2) + 4
 *     16-31: HS = 0: Speed = 72 * Value / 8192 MBit/s
 *            HS = 1: Speed = 72 * 8 / (Value + 1) MBit/s (only bit 19-16 used)
 *
 */

static void calc_con(struct ddb_output *output, u32 *con, u32 *con2, u32 flags)
{
	struct ddb *dev = output->port->dev;
	u32 bitrate = output->port->obr, max_bitrate = 72000;
	u32 gap = 4, nco = 0;

	*con = 0x1C;
	if (output->port->gap != 0xffffffff) {
		flags |= 1;
		gap = output->port->gap;
		max_bitrate = 0;
	}
	if (dev->link[0].info->type == DDB_OCTOPUS_CI && output->port->nr > 1) {
		*con = 0x10c;
		if (dev->link[0].ids.regmapid >= 0x10003 && !(flags & 1)) {
			if (!(flags & 2)) {
				/* NCO */
				max_bitrate = 0;
				gap = 0;
				if (bitrate != 72000) {
					if (bitrate >= 96000) {
						*con |= 0x800;
					} else {
						*con |= 0x1000;
						nco = (bitrate *
						       8192 + 71999) / 72000;
					}
				}
			} else {
				/* Divider and gap */
				*con |= 0x1810;
				if (bitrate <= 64000) {
					max_bitrate = 64000;
					nco = 8;
				} else if (bitrate <= 72000) {
					max_bitrate = 72000;
					nco = 7;
				} else {
					max_bitrate = 96000;
					nco = 5;
				}
			}
		} else {
			if (bitrate > 72000) {
				*con |= 0x810;  /* 96 MBit/s and gap */
				max_bitrate = 96000;
			}
			*con |= 0x10;  /* enable gap */
		}
	}
	if (max_bitrate > 0) {
		if (bitrate > max_bitrate)
			bitrate = max_bitrate;
		if (bitrate < 31000)
			bitrate = 31000;
		gap = ((max_bitrate - bitrate) * 94) / bitrate;
		if (gap < 2)
			*con &= ~0x10;    /* Disable gap */
		else
			gap -= 2;
		if (gap > 127)
			gap = 127;
	}
	*con2 = (nco << 16) | gap;
}

static int ddb_output_start_unlocked(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;
	u32 con = 0x11c, con2 = 0;
	int err = 0;

	if (output->dma) {
		output->dma->cbuf = 0;
		output->dma->coff = 0;
		output->dma->stat = 0;
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(output->dma));
	}
	if (output->port->class == DDB_PORT_MOD) {
		err = ddbridge_mod_output_start(output);
	} else {
		if (output->port->input[0]->port->class == DDB_PORT_LOOP)
			con = (1UL << 13) | 0x14;
		else
			calc_con(output, &con, &con2, 0);
		ddbwritel(dev, 0, TS_CONTROL(output));
		ddbwritel(dev, 2, TS_CONTROL(output));
		ddbwritel(dev, 0, TS_CONTROL(output));
		ddbwritel(dev, con, TS_CONTROL(output));
		ddbwritel(dev, con2, TS_CONTROL2(output));
	}
	if (output->dma) {
		ddbwritel(dev, output->dma->bufval,
			  DMA_BUFFER_SIZE(output->dma));
		ddbwritel(dev, 0, DMA_BUFFER_ACK(output->dma));
		ddbwritel(dev, 1, DMA_BASE_READ);
		ddbwritel(dev, 7, DMA_BUFFER_CONTROL(output->dma));
	}
	if (output->port->class != DDB_PORT_MOD)
		ddbwritel(dev, con | 1, TS_CONTROL(output));
	if (output->dma)
		output->dma->running = 1;
	return err;
}

static int ddb_output_start(struct ddb_output *output)
{
	int err;

	if (output->dma) {
		spin_lock_irq(&output->dma->lock);
		err = ddb_output_start_unlocked(output);
		spin_unlock_irq(&output->dma->lock);
	} else {
		err = ddb_output_start_unlocked(output);
	}
	return err;
}

static void ddb_output_stop_unlocked(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;

	if (output->port->class == DDB_PORT_MOD)
		ddbridge_mod_output_stop(output);
	else
		ddbwritel(dev, 0, TS_CONTROL(output));
	if (output->dma) {
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(output->dma));
		output->dma->running = 0;
	}
}

static void ddb_output_stop(struct ddb_output *output)
{
	if (output->dma) {
		spin_lock_irq(&output->dma->lock);
		ddb_output_stop_unlocked(output);
		spin_unlock_irq(&output->dma->lock);
	} else {
		ddb_output_stop_unlocked(output);
	}
}

static void update_loss(struct ddb_dma *dma)
{
	struct ddb_input *input = (struct ddb_input *)dma->io;
	u32 packet_loss = dma->packet_loss;
	u32 cur_counter = ddbreadl(input->port->dev, TS_STAT(input)) & 0xffff;
	
	if (cur_counter < (packet_loss & 0xffff))
		packet_loss += 0x10000;
	packet_loss = ((packet_loss & 0xffff0000) | cur_counter);
	dma->packet_loss = packet_loss;
}

static void ddb_input_stop_unlocked(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	u32 tag = DDB_LINK_TAG(input->port->lnr);

	ddbwritel(dev, 0, tag | TS_CONTROL(input));
	if (input->dma) {
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(input->dma));
		input->dma->running = 0;
		if (input->dma->stall_count)
			dev_warn(input->port->dev->dev,
				 "DMA stalled %u times!\n",
				 input->dma->stall_count);
		update_loss(input->dma);
		if (input->dma->packet_loss > 1)
			dev_warn(input->port->dev->dev,
				 "%u packets lost due to low DMA performance!\n",
				 input->dma->packet_loss);
	}
}

static void ddb_input_stop(struct ddb_input *input)
{
	if (input->dma) {
		spin_lock_irq(&input->dma->lock);
		ddb_input_stop_unlocked(input);
		spin_unlock_irq(&input->dma->lock);
	} else {
		ddb_input_stop_unlocked(input);
	}
}

static void ddb_input_start_unlocked(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;

	if (input->dma) {
		input->dma->cbuf = 0;
		input->dma->coff = 0;
		input->dma->stat = 0;
		input->dma->stall_count = 0;
		input->dma->packet_loss = 0;
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(input->dma));
	}
	ddbwritel(dev, 0, TS_CONTROL(input));
	ddbwritel(dev, 2, TS_CONTROL(input));
	ddbwritel(dev, 0, TS_CONTROL(input));

	if (input->dma) {
		ddbwritel(dev, input->dma->bufval,
			  DMA_BUFFER_SIZE(input->dma));
		ddbwritel(dev, 0, DMA_BUFFER_ACK(input->dma));
		ddbwritel(dev, 1, DMA_BASE_WRITE);
		ddbwritel(dev, 3, DMA_BUFFER_CONTROL(input->dma));
	}
	if (dev->link[0].info->type == DDB_OCTONET)
		ddbwritel(dev, 0x01, TS_CONTROL(input));
	else {
		if (raw_stream)
			ddbwritel(dev, 0x01 | ((raw_stream & 3) << 8), TS_CONTROL(input));
		else
			ddbwritel(dev, 0x01 | input->con, TS_CONTROL(input));
	}
	if (input->port->type == DDB_TUNER_DUMMY)
		ddbwritel(dev, 0x000fff01, TS_CONTROL2(input));
	if (input->dma)
		input->dma->running = 1;
}

static void ddb_input_start(struct ddb_input *input)
{
	if (input->dma) {
		spin_lock_irq(&input->dma->lock);
		ddb_input_start_unlocked(input);
		spin_unlock_irq(&input->dma->lock);
	} else {
		ddb_input_start_unlocked(input);
	}
}

int ddb_dvb_ns_input_start(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (!dvb->users)
		ddb_input_start(input);

	return ++dvb->users;
}

int ddb_dvb_ns_input_stop(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (--dvb->users)
		return dvb->users;

	ddb_input_stop(input);
	return 0;
}

static void ddb_input_start_all(struct ddb_input *input)
{
	struct ddb_input *i = input;
	struct ddb_output *o;

	mutex_lock(&redirect_lock);
	while (i && (o = i->redo)) {
		ddb_output_start(o);
		i = o->port->input[0];
		if (i)
			ddb_input_start(i);
	}
	ddb_input_start(input);
	mutex_unlock(&redirect_lock);
}

static void ddb_input_stop_all(struct ddb_input *input)
{
	struct ddb_input *i = input;
	struct ddb_output *o;

	mutex_lock(&redirect_lock);
	ddb_input_stop(input);
	while (i && (o = i->redo)) {
		ddb_output_stop(o);
		i = o->port->input[0];
		if (i)
			ddb_input_stop(i);
	}
	mutex_unlock(&redirect_lock);
}

static u32 ddb_output_free(struct ddb_output *output)
{
	u32 idx, off, stat = output->dma->stat;
	s32 diff;

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	if (output->dma->cbuf != idx) {
		if ((((output->dma->cbuf + 1) % output->dma->num) == idx) &&
		    (output->dma->size - output->dma->coff <= 2 * 188))
			return 0;
		return 188;
	}
	diff = off - output->dma->coff;
	if (diff <= 0 || diff > 2*188)
		return 188;
	return 0;
}

static ssize_t ddb_output_write(struct ddb_output *output,
				const __user u8 *buf, size_t count)
{
	struct ddb *dev = output->port->dev;
	u32 idx, off, stat = output->dma->stat;
	u32 left = count, len;

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	while (left) {
		len = output->dma->size - output->dma->coff;
		if ((((output->dma->cbuf + 1) % output->dma->num) == idx) &&
		    (off == 0)) {
			if (len <= 188)
				break;
			len -= 188;
		}
		if (output->dma->cbuf == idx) {
			if (off > output->dma->coff) {
				len = off - output->dma->coff;
				len -= (len % 188);
				if (len <= 188)
					break;
				len -= 188;
			}
		}
		if (len > left)
			len = left;
		if (copy_from_user(output->dma->vbuf[output->dma->cbuf] +
				   output->dma->coff,
				   buf, len))
			return -EIO;
		if (alt_dma)
			dma_sync_single_for_device(dev->dev,
						   output->dma->pbuf[
							   output->dma->cbuf],
						   output->dma->size,
						   DMA_TO_DEVICE);
		left -= len;
		buf += len;
		output->dma->coff += len;
		if (output->dma->coff == output->dma->size) {
			output->dma->coff = 0;
			output->dma->cbuf = ((output->dma->cbuf + 1) %
					     output->dma->num);
		}
		ddbwritel(dev,
			  (output->dma->cbuf << 11) |
			  (output->dma->coff >> 7),
			  DMA_BUFFER_ACK(output->dma));
	}
	return count - left;
}

static u32 ddb_input_avail(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	u32 idx, off, stat = input->dma->stat;
	u32 ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(input->dma));

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	if (ctrl & 4) {
		dev_err(dev->dev, "IA %d %d %08x\n", idx, off, ctrl);
		ddbwritel(dev, stat, DMA_BUFFER_ACK(input->dma));
		return 0;
	}
	if (input->dma->cbuf != idx)
		return 188;
	return 0;
}

static size_t ddb_input_read(struct ddb_input *input,
			     __user u8 *buf, size_t count)
{
	struct ddb *dev = input->port->dev;
	u32 left = count;
	u32 idx, off, free, stat = input->dma->stat;
	int ret;

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	while (left) {
		if (input->dma->cbuf == idx)
			return count - left;
		free = input->dma->size - input->dma->coff;
		if (free > left)
			free = left;
		if (alt_dma)
			dma_sync_single_for_cpu(dev->dev,
						input->dma->pbuf[
							input->dma->cbuf],
						input->dma->size,
						DMA_FROM_DEVICE);
		ret = copy_to_user(buf, input->dma->vbuf[input->dma->cbuf] +
				   input->dma->coff, free);
		if (ret)
			return -EFAULT;
		input->dma->coff += free;
		if (input->dma->coff == input->dma->size) {
			input->dma->coff = 0;
			input->dma->cbuf = (input->dma->cbuf + 1) %
				input->dma->num;
		}
		left -= free;
		buf += free;
		ddbwritel(dev,
			  (input->dma->cbuf << 11) | (input->dma->coff >> 7),
			  DMA_BUFFER_ACK(input->dma));
	}
	return count;
}

/****************************************************************************/
/****************************************************************************/

static ssize_t ts_write(struct file *file, const char *buf,
			size_t count, loff_t *ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb *dev = output->port->dev;
	size_t left = count;
	int stat;

	if (!dev->has_dma)
		return -EINVAL;
	while (left) {
		if (ddb_output_free(output) < 188) {
			if (file->f_flags & O_NONBLOCK)
				break;
			if (wait_event_interruptible(
				    output->dma->wq,
				    ddb_output_free(output) >= 188) < 0)
				break;
		}
		stat = ddb_output_write(output, buf, left);
		if (stat < 0)
			return stat;
		buf += stat;
		left -= stat;
	}
	return (left == count) ? -EAGAIN : (count - left);
}

static ssize_t ts_read(struct file *file, __user char *buf,
		       size_t count, loff_t *ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb_input *input = output->port->input[0];
	struct ddb *dev = output->port->dev;
	size_t left = count;
	int stat;

	if (!dev->has_dma)
		return -EINVAL;
	if (!input)
		return -EINVAL;
	while (left) {
		if (ddb_input_avail(input) < 188) {
			if (file->f_flags & O_NONBLOCK)
				break;
			if (wait_event_interruptible(
				    input->dma->wq,
				    ddb_input_avail(input) >= 188) < 0)
				break;
		}
		stat = ddb_input_read(input, buf, left);
		if (stat < 0)
			return stat;
		left -= stat;
		buf += stat;
	}
	return (count && (left == count)) ? -EAGAIN : (count - left);
}

static unsigned int ts_poll(struct file *file, poll_table *wait)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb_input *input = output->port->input[0];

	unsigned int mask = 0;

	poll_wait(file, &input->dma->wq, wait);
	poll_wait(file, &output->dma->wq, wait);
	if (ddb_input_avail(input) >= 188)
		mask |= POLLIN | POLLRDNORM;
	if (ddb_output_free(output) >= 188)
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}

static int ts_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb_input *input = output->port->input[0];

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (!input)
			return -EINVAL;
		ddb_input_stop(input);
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (!output)
			return -EINVAL;
		ddb_output_stop(output);
	}
	return dvb_generic_release(inode, file);
}

static int ts_open(struct inode *inode, struct file *file)
{
	int err;
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;
	struct ddb_input *input = output->port->input[0];

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (!input)
			return -EINVAL;
		if (input->redo || input->redi)
			return -EBUSY;
	} else if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (!output)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	err = dvb_generic_open(inode, file);
	if (err < 0)
		return err;
	if ((file->f_flags & O_ACCMODE) == O_RDONLY)
		ddb_input_start(input);
	else if ((file->f_flags & O_ACCMODE) == O_WRONLY)
		err = ddb_output_start(output);
	return err;
}

static int mod_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;

	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (!output)
			return -EINVAL;
		ddb_output_stop(output);
	}
	return dvb_generic_release(inode, file);
}

static int mod_open(struct inode *inode, struct file *file)
{
	int err;
	struct dvb_device *dvbdev = file->private_data;
	struct ddb_output *output = dvbdev->priv;

	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		if (!output)
			return -EINVAL;
	}
	err = dvb_generic_open(inode, file);
	if (err < 0)
		return err;
	if ((file->f_flags & O_ACCMODE) == O_WRONLY)
		ddb_output_start(output);
	return err;
}

static const struct file_operations ci_fops = {
	.owner   = THIS_MODULE,
	.read    = ts_read,
	.write   = ts_write,
	.open    = ts_open,
	.release = ts_release,
	.poll    = ts_poll,
	.mmap    = NULL,
};

static struct dvb_device dvbdev_ci = {
	.priv    = NULL,
	.readers = 1,
	.writers = 1,
	.users   = 2,
	.fops    = &ci_fops,
};

/****************************************************************************/
/****************************************************************************/

static long mod_ioctl(struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	return ddb_dvb_usercopy(file, cmd, arg, ddbridge_mod_do_ioctl);
}

static const struct file_operations mod_fops = {
	.owner   = THIS_MODULE,
	.read    = ts_read,
	.write   = ts_write,
	.open    = mod_open,
	.release = mod_release,
	.poll    = ts_poll,
	.mmap    = NULL,
	.unlocked_ioctl = mod_ioctl,
};

static struct dvb_device dvbdev_mod = {
	.priv    = NULL,
	.readers = 1,
	.writers = 1,
	.users   = 2,
	.fops    = &mod_fops,
};

static int locked_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb_dvb *dvb = &port->dvb[input->nr & 1];
	int status;

	if (enable) {
		mutex_lock(&port->i2c_gate_lock);
		status = dvb->i2c_gate_ctrl(fe, 1);
	} else {
		status = dvb->i2c_gate_ctrl(fe, 0);
		mutex_unlock(&port->i2c_gate_lock);
	}
	return status;
}

/****************************************************************************/

static int dummy_read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	*status = 0x1f;
	return 0;
}

static void dummy_release(struct dvb_frontend *fe)
{
	kfree(fe);
}

static struct dvb_frontend_ops dummy_ops = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBS, SYS_DVBS2 },
	.info = {
		.name = "DUMMY DVB-C/C2 DVB-T/T2",
		.frequency_stepsize_hz = 166667,	/* DVB-T only */
		.frequency_min_hz = 47000000,	/* DVB-T: 47125000 */
		.frequency_max_hz = 865000000,	/* DVB-C: 862000000 */
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 |
		FE_CAN_QAM_64 | FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		FE_CAN_QAM_AUTO |
		FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		FE_CAN_FEC_4_5 |
		FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		FE_CAN_TRANSMISSION_MODE_AUTO |
		FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
		FE_CAN_RECOVER | FE_CAN_MUTE_TS | FE_CAN_2G_MODULATION
	},
	.release = dummy_release,
	.read_status = dummy_read_status,
};

static struct dvb_frontend *dummy_attach(void)
{
#if (KERNEL_VERSION(4, 13, 0) > LINUX_VERSION_CODE)
	struct dvb_frontend *fe = kmalloc(sizeof(*fe), __GFP_REPEAT);
#else
	struct dvb_frontend *fe = kmalloc(sizeof(*fe), __GFP_RETRY_MAYFAIL);
#endif
	if (fe)
		fe->ops = dummy_ops;
	return fe;
}

static int demod_attach_dummy(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

#if 0
	dvb->fe = dvb_attach(dummy_attach);
#else
	dvb->fe = dummy_attach();
#endif
	return 0;
}

/****************************************************************************/

#ifdef CONFIG_DVB_DRXK
static int demod_attach_drxk(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	dvb->fe = dvb_attach(drxk_attach,
			     i2c, 0x29 + (input->nr & 1),
			     &dvb->fe2);
	if (!dvb->fe) {
		dev_err(input->port->dev->dev,
			"No DRXK found!\n");
		return -ENODEV;
	}
	dvb->fe->sec_priv = input;
	dvb->i2c_gate_ctrl = dvb->fe->ops.i2c_gate_ctrl;
	dvb->fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
	return 0;
}
#endif

static int demod_attach_cxd2843(struct ddb_input *input, int par, int osc24)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct cxd2843_cfg cfg;

	cfg.adr = (input->nr & 1) ? 0x6d : 0x6c;
	cfg.ts_clock = par ? 0 : 1;
	cfg.parallel = par ? 1 : 0;
	cfg.osc = osc24 ? 24000000 : 20500000;
	dvb->fe = dvb_attach(cxd2843_attach, i2c, &cfg);

	if (!dvb->fe) {
		dev_err(input->port->dev->dev,
			"No cxd2837/38/43/54 found!\n");
		return -ENODEV;
	}
	dvb->fe->sec_priv = input;
	dvb->i2c_gate_ctrl = dvb->fe->ops.i2c_gate_ctrl;
	dvb->fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
	return 0;
}

static int demod_attach_stv0367dd(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct stv0367_cfg cfg = { .cont_clock = 0 };

	cfg.adr = 0x1f - (input->nr & 1);
	if (input->port->dev->link[input->port->lnr].info->con_clock)
		cfg.cont_clock = 1;
	dvb->fe = dvb_attach(stv0367_attach, i2c, &cfg);
	if (!dvb->fe) {
		dev_err(input->port->dev->dev,
			"No stv0367 found!\n");
		return -ENODEV;
	}
	dvb->fe->sec_priv = input;
	dvb->i2c_gate_ctrl = dvb->fe->ops.i2c_gate_ctrl;
	dvb->fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
	return 0;
}

static int tuner_attach_tda18271(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;

	if (dvb->fe->ops.i2c_gate_ctrl)
		dvb->fe->ops.i2c_gate_ctrl(dvb->fe, 1);
	fe = dvb_attach(tda18271c2dd_attach, dvb->fe, i2c, 0x60);
	if (dvb->fe->ops.i2c_gate_ctrl)
		dvb->fe->ops.i2c_gate_ctrl(dvb->fe, 0);
	if (!fe) {
		dev_err(input->port->dev->dev,
			"No TDA18271 found!\n");
		return -ENODEV;
	}
	return 0;
}

static int tuner_attach_tda18212dd(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;

	fe = dvb_attach(tda18212dd_attach, dvb->fe, i2c,
			(input->nr & 1) ? 0x63 : 0x60);
	if (!fe) {
		dev_err(input->port->dev->dev,
			"No TDA18212 found!\n");
		return -ENODEV;
	}
	return 0;
}

#ifdef CONFIG_DVB_TDA18212
struct tda18212_config tda18212_0 = {
	.i2c_address = 0x60,
};

struct tda18212_config tda18212_1 = {
	.i2c_address = 0x63,
};

static int tuner_attach_tda18212(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;
	struct tda18212_config *cfg;

	cfg = (input->nr & 1) ? &tda18212_1 : &tda18212_0;
	fe = dvb_attach(tda18212_attach, dvb->fe, i2c, cfg);
	if (!fe) {
		dev_err(input->port->dev->dev,
			"No TDA18212 found!\n");
		return -ENODEV;
	}
	return 0;
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static struct stv090x_config stv0900 = {
	.device         = STV0900,
	.demod_mode     = STV090x_DUAL,
	.clk_mode       = STV090x_CLK_EXT,

	.xtal           = 27000000,
	.address        = 0x69,

	.ts1_mode       = STV090x_TSMODE_SERIAL_PUNCTURED,
	.ts2_mode       = STV090x_TSMODE_SERIAL_PUNCTURED,

	.ts1_tei        = 1,
	.ts2_tei        = 1,

	.repeater_level = STV090x_RPTLEVEL_16,

	.adc1_range	= STV090x_ADC_1Vpp,
	.adc2_range	= STV090x_ADC_1Vpp,

	.diseqc_envelope_mode = true,
};

static struct stv090x_config stv0900_aa = {
	.device         = STV0900,
	.demod_mode     = STV090x_DUAL,
	.clk_mode       = STV090x_CLK_EXT,

	.xtal           = 27000000,
	.address        = 0x68,

	.ts1_mode       = STV090x_TSMODE_SERIAL_PUNCTURED,
	.ts2_mode       = STV090x_TSMODE_SERIAL_PUNCTURED,

	.ts1_tei        = 1,
	.ts2_tei        = 1,

	.repeater_level = STV090x_RPTLEVEL_16,

	.adc1_range	= STV090x_ADC_1Vpp,
	.adc2_range	= STV090x_ADC_1Vpp,

	.diseqc_envelope_mode = true,
};

static struct stv6110x_config stv6110a = {
	.addr    = 0x60,
	.refclk	 = 27000000,
	.clk_div = 1,
};

static struct stv6110x_config stv6110b = {
	.addr    = 0x63,
	.refclk	 = 27000000,
	.clk_div = 1,
};

static int demod_attach_stv0900(struct ddb_input *input, int type)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct stv090x_config *feconf = type ? &stv0900_aa : &stv0900;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	dvb->fe = dvb_attach(stv090x_attach, feconf, i2c,
			     (input->nr & 1) ? STV090x_DEMODULATOR_1
			     : STV090x_DEMODULATOR_0);
	if (!dvb->fe) {
		dev_err(input->port->dev->dev,
			"No STV0900 found!\n");
		return -ENODEV;
	}
	if (!dvb_attach(lnbh24_attach, dvb->fe, i2c, 0,
			0, (input->nr & 1) ?
			(0x09 - type) : (0x0b - type))) {
		dev_err(input->port->dev->dev,
			"No LNBH24 found!\n");
		return -ENODEV;
	}
	return 0;
}

static int tuner_attach_stv6110(struct ddb_input *input, int type)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct stv090x_config *feconf = type ? &stv0900_aa : &stv0900;
	struct stv6110x_config *tunerconf = (input->nr & 1) ?
		&stv6110b : &stv6110a;
	const struct stv6110x_devctl *ctl;

	ctl = dvb_attach(stv6110x_attach, dvb->fe, tunerconf, i2c);
	if (!ctl) {
		dev_err(input->port->dev->dev,
			"No STV6110X found!\n");
		return -ENODEV;
	}
	dev_info(input->port->dev->dev,
		 "attach tuner input %d adr %02x\n",
		 input->nr, tunerconf->addr);

	feconf->tuner_init          = ctl->tuner_init;
	feconf->tuner_sleep         = ctl->tuner_sleep;
	feconf->tuner_set_mode      = ctl->tuner_set_mode;
	feconf->tuner_set_frequency = ctl->tuner_set_frequency;
	feconf->tuner_get_frequency = ctl->tuner_get_frequency;
	feconf->tuner_set_bandwidth = ctl->tuner_set_bandwidth;
	feconf->tuner_get_bandwidth = ctl->tuner_get_bandwidth;
	feconf->tuner_set_bbgain    = ctl->tuner_set_bbgain;
	feconf->tuner_get_bbgain    = ctl->tuner_get_bbgain;
	feconf->tuner_set_refclk    = ctl->tuner_set_refclk;
	feconf->tuner_get_status    = ctl->tuner_get_status;

	return 0;
}

static struct stv0910_cfg stv0910_p = {
	.adr      = 0x68,
	.parallel = 1,
	.rptlvl   = 4,
	.clk      = 30000000,
	.tsspeed  = 0x20,
};

static int has_lnbh25(struct i2c_adapter *i2c, u8 adr)
{
	u8 val;

	return i2c_read_reg(i2c, adr, 0, &val) ? 0 : 1;
}

static int demod_attach_stv0910(struct ddb_input *input, int type)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct stv0910_cfg cfg = stv0910_p;
	struct ddb *dev = input->port->dev;
	u8 lnbh_adr = 0x08;

	if (stv0910_single)
		cfg.single = 1;
	if (type)
		cfg.parallel = 2;
	if ((input->port->nr == 0) &&
	    ((dev->link[0].ids.hwid & 0xffffff) <
	     dev->link[0].info->hw_min))
		cfg.tsspeed = 0x28;
	dvb->fe = dvb_attach(stv0910_attach, i2c, &cfg, (input->nr & 1));
	if (!dvb->fe) {
		cfg.adr = 0x6c;
		dvb->fe = dvb_attach(stv0910_attach, i2c,
				     &cfg, (input->nr & 1));
	}
	if (!dvb->fe) {
		dev_err(input->port->dev->dev,
			"No STV0910 found!\n");
		return -ENODEV;
	}
	if (has_lnbh25(i2c, 0x0d))
		lnbh_adr = 0x0c;

	if (!dvb_attach(lnbh25_attach, dvb->fe, i2c,
			(input->nr & 1) + lnbh_adr)) {
		dev_err(input->port->dev->dev,
			"No LNBH25 found!\n");
		return -ENODEV;
	}
	return 0;
}

static int tuner_attach_stv6111(struct ddb_input *input, int type)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;
	u8 adr = (type ? 0 : 4) + ((input->nr & 1) ? 0x63 : 0x60);

	fe = dvb_attach(stv6111_attach, dvb->fe, i2c, adr);
	if (!fe) {
		fe = dvb_attach(stv6111_attach, dvb->fe, i2c, adr & ~4);
		if (!fe) {
			dev_err(input->port->dev->dev,
				"No STV6111 found at 0x%02x!\n", adr);
			return -ENODEV;
		}
	}
	return 0;
}

static int start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	struct ddb_input *input = dvbdmx->priv;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (!dvb->users)
		ddb_input_start_all(input);

	return ++dvb->users;
}

static int stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux *dvbdmx = dvbdmxfeed->demux;
	struct ddb_input *input = dvbdmx->priv;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (--dvb->users)
		return dvb->users;

	ddb_input_stop_all(input);
	return 0;
}

static void dvb_input_detach(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_demux *dvbdemux = &dvb->demux;

	switch (dvb->attached) {
	case 0x41:
		if (dvb->fe2)
			dvb_unregister_frontend(dvb->fe2);
		fallthrough;
	case 0x40:
		if (dvb->fe)
			dvb_unregister_frontend(dvb->fe);
		fallthrough;
	case 0x30:
		dvb_frontend_detach(dvb->fe);
		dvb->fe = NULL;
		dvb->fe2 = NULL;
		fallthrough;
	case 0x21:
		if (input->port->dev->ns_num)
			dvb_netstream_release(&dvb->dvbns);
		fallthrough;
	case 0x20:
		dvb_net_release(&dvb->dvbnet);
		fallthrough;
	case 0x12:
		dvbdemux->dmx.remove_frontend(&dvbdemux->dmx,
					      &dvb->hw_frontend);
		dvbdemux->dmx.remove_frontend(&dvbdemux->dmx,
					      &dvb->mem_frontend);
		fallthrough;
	case 0x11:
		dvb_dmxdev_release(&dvb->dmxdev);
		fallthrough;
	case 0x10:
		dvb_dmx_release(&dvb->demux);
		fallthrough;
	case 0x01:
		break;
	}
	dvb->attached = 0x00;
}

static int dvb_register_adapters(struct ddb *dev)
{
	int i, ret = 0, l = 0;
	struct ddb_port *port;
	struct dvb_adapter *adap = 0;

	if (adapter_alloc == 4) {
		for (i = 0; i < dev->port_num; i++) {
			port = &dev->port[i];
			if (port->lnr >= l) {
				adap = port->dvb[0].adap;
				ret = dvb_register_adapter(adap, "DDBridge", THIS_MODULE,
							   port->dev->dev,
							   adapter_nr);
				if (ret < 0)
					return ret;
				port->dvb[0].adap_registered = 1;
				l = port->lnr + 1;
			}
			port->dvb[0].adap = adap;
			port->dvb[1].adap = adap;
		}
		return 0;
	}

	if (adapter_alloc >= 3 || dev->link[0].info->type == DDB_MOD ||
	    dev->link[0].info->type == DDB_OCTONET ||
	    dev->link[0].info->type == DDB_OCTOPRO) {
		port = &dev->port[0];
		adap = port->dvb[0].adap;
		ret = dvb_register_adapter(adap, "DDBridge", THIS_MODULE,
					   port->dev->dev,
					   adapter_nr);
		if (ret < 0)
			return ret;
		port->dvb[0].adap_registered = 1;
		for (i = 0; i < dev->port_num; i++) {
			port = &dev->port[i];
			port->dvb[0].adap = adap;
			port->dvb[1].adap = adap;
		}
		return 0;
	}

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
		switch (port->class) {
		case DDB_PORT_TUNER:
			adap = port->dvb[0].adap;
			ret = dvb_register_adapter(adap, "DDBridge",
						   THIS_MODULE,
						   port->dev->dev,
						   adapter_nr);
			if (ret < 0)
				return ret;
			port->dvb[0].adap_registered = 1;

			if (adapter_alloc > 0) {
				port->dvb[1].adap = port->dvb[0].adap;
				break;
			}
			adap = port->dvb[1].adap;
			ret = dvb_register_adapter(adap, "DDBridge",
						   THIS_MODULE,
						   port->dev->dev,
						   adapter_nr);
			if (ret < 0)
				return ret;
			port->dvb[1].adap_registered = 1;
			break;

		case DDB_PORT_CI:
		case DDB_PORT_LOOP:
			adap = port->dvb[0].adap;
			ret = dvb_register_adapter(adap, "DDBridge",
						   THIS_MODULE,
						   port->dev->dev,
						   adapter_nr);
			if (ret < 0)
				return ret;
			port->dvb[0].adap_registered = 1;
			break;
		default:
			if (adapter_alloc < 2)
				break;
			adap = port->dvb[0].adap;
			ret = dvb_register_adapter(adap, "DDBridge",
						   THIS_MODULE,
						   port->dev->dev,
						   adapter_nr);
			if (ret < 0)
				return ret;
			port->dvb[0].adap_registered = 1;
			break;
		}
	}
	return ret;
}

static void dvb_unregister_adapters(struct ddb *dev)
{
	int i;
	struct ddb_port *port;
	struct ddb_dvb *dvb;

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];

		dvb = &port->dvb[0];
		if (dvb->adap_registered)
			dvb_unregister_adapter(dvb->adap);
		dvb->adap_registered = 0;

		dvb = &port->dvb[1];
		if (dvb->adap_registered)
			dvb_unregister_adapter(dvb->adap);
		dvb->adap_registered = 0;
	}
}

static int dvb_input_attach(struct ddb_input *input)
{
	int ret = 0;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct ddb_port *port = input->port;
	struct dvb_adapter *adap = dvb->adap;
	struct dvb_demux *dvbdemux = &dvb->demux;
	int par = 0, osc24 = 0;

	dvb->attached = 0x01;

	dvbdemux->priv = input;
	dvbdemux->dmx.capabilities = DMX_TS_FILTERING |
		DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
	dvbdemux->start_feed = start_feed;
	dvbdemux->stop_feed = stop_feed;
	dvbdemux->filternum = 256;
	dvbdemux->feednum = 256;
	ret = dvb_dmx_init(dvbdemux);
	if (ret < 0)
		return ret;
	dvb->attached = 0x10;

	dvb->dmxdev.filternum = 256;
	dvb->dmxdev.demux = &dvbdemux->dmx;
	ret = dvb_dmxdev_init(&dvb->dmxdev, adap);
	if (ret < 0)
		return ret;
	dvb->attached = 0x11;

	dvb->mem_frontend.source = DMX_MEMORY_FE;
	dvb->demux.dmx.add_frontend(&dvb->demux.dmx, &dvb->mem_frontend);
	dvb->hw_frontend.source = DMX_FRONTEND_0;
	dvb->demux.dmx.add_frontend(&dvb->demux.dmx, &dvb->hw_frontend);
	ret = dvbdemux->dmx.connect_frontend(&dvbdemux->dmx, &dvb->hw_frontend);
	if (ret < 0)
		return ret;
	dvb->attached = 0x12;

	ret = dvb_net_init(adap, &dvb->dvbnet, dvb->dmxdev.demux);
	if (ret < 0)
		return ret;
	dvb->attached = 0x20;

	if (input->port->dev->ns_num) {
		ret = netstream_init(input);
		if (ret < 0)
			return ret;
		dvb->attached = 0x21;
	}
	dvb->fe = NULL;
	dvb->fe2 = NULL;
	switch (port->type) {
	case DDB_TUNER_MXL5XX:
		if (ddb_fe_attach_mxl5xx(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_ST:
		if (demod_attach_stv0900(input, 0) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_stv6110(input, 0) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_ST_AA:
		if (demod_attach_stv0900(input, 1) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_stv6110(input, 1) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910:
		if (demod_attach_stv0910(input, 0) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_stv6111(input, 0) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910_PR:
		if (demod_attach_stv0910(input, 1) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_stv6111(input, 1) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910_P:
		if (demod_attach_stv0910(input, 0) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_stv6111(input, 1) < 0)
			return -ENODEV;
		break;
#ifdef CONFIG_DVB_DRXK
	case DDB_TUNER_DVBCT_TR:
		if (demod_attach_drxk(input) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_tda18271(input) < 0)
			return -ENODEV;
		break;
#endif
	case DDB_TUNER_DVBCT_ST:
		if (demod_attach_stv0367dd(input) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_tda18212dd(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBC2T2I_SONY_P:
		if (input->port->dev->link[input->port->lnr].info->ts_quirks &
		    TS_QUIRK_ALT_OSC)
			osc24 = 0;
		else
			osc24 = 1;
		fallthrough;
	case DDB_TUNER_DVBCT2_SONY_P:
	case DDB_TUNER_DVBC2T2_SONY_P:
	case DDB_TUNER_ISDBT_SONY_P:
		if (input->port->dev->link[input->port->lnr].info->ts_quirks &
		    TS_QUIRK_SERIAL)
			par = 0;
		else
			par = 1;
		if (demod_attach_cxd2843(input, par, osc24) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_tda18212dd(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBC2T2I_SONY:
		osc24 = 1;
		fallthrough;
	case DDB_TUNER_DVBCT2_SONY:
	case DDB_TUNER_DVBC2T2_SONY:
	case DDB_TUNER_ISDBT_SONY:
		if (demod_attach_cxd2843(input, 0, osc24) < 0)
			return -ENODEV;
		dvb->attached = 0x30;
		if (tuner_attach_tda18212dd(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DUMMY:
		if (demod_attach_dummy(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_MCI_SX8:
	case DDB_TUNER_MCI_M4:
		if (ddb_fe_attach_mci(input, port->type) < 0)
			return -ENODEV;
		break;
	default:
		return 0;
	}
	dvb->attached = 0x30;

	if (dvb->fe) {
		if (dvb_register_frontend(adap, dvb->fe) < 0)
			return -ENODEV;
	}
	dvb->attached = 0x40;
	if (dvb->fe2) {
		if (dvb_register_frontend(adap, dvb->fe2) < 0)
			return -ENODEV;
		dvb->fe2->tuner_priv = dvb->fe->tuner_priv;
		memcpy(&dvb->fe2->ops.tuner_ops,
		       &dvb->fe->ops.tuner_ops,
		       sizeof(struct dvb_tuner_ops));
		dvb->attached = 0x41;
	}
	return 0;
}

static int port_has_encti(struct ddb_port *port)
{
	u8 val;
	int ret = i2c_read_reg(&port->i2c->adap, 0x20, 0, &val);

	if (!ret)
		dev_info(port->dev->dev,
			 "[0x20]=0x%02x\n", val);
	return ret ? 0 : 1;
}

static int port_has_cxd(struct ddb_port *port, u8 *type)
{
	u8 val;
	u8 probe[4] = { 0xe0, 0x00, 0x00, 0x00 }, data[4];
	struct i2c_msg msgs[2] = {{ .addr = 0x40,  .flags = 0,
				    .buf  = probe, .len   = 4 },
				  { .addr = 0x40,  .flags = I2C_M_RD,
				    .buf  = data,  .len   = 4 } };
	val = i2c_transfer(&port->i2c->adap, msgs, 2);
	if (val != 2)
		return 0;

	if (data[0] == 0x02 && data[1] == 0x2b && data[3] == 0x43)
		*type = 2;
	else
		*type = 1;
	return 1;
}

static int port_has_xo2(struct ddb_port *port, u8 *type, u8 *id)
{
	u8 probe[1] = { 0x00 }, data[4];

	if (i2c_io(&port->i2c->adap, 0x10, probe, 1, data, 4))
		return 0;
	if (data[0] == 'D' && data[1] == 'F') {
		*id = data[2];
		*type = 1;
		return 1;
	}
	if (data[0] == 'C' && data[1] == 'I') {
		*id = data[2];
		*type = 2;
		return 1;
	}
	return 0;
}

static int port_has_stv0900(struct ddb_port *port)
{
	u8 val;

	if (i2c_read_reg16(&port->i2c->adap, 0x69, 0xf100, &val) < 0)
		return 0;
	return 1;
}

static int port_has_stv0900_aa(struct ddb_port *port, u8 *id)
{
	if (i2c_read_reg16(&port->i2c->adap, 0x68, 0xf100, id) < 0)
		return 0;
	return 1;
}

static int port_has_drxks(struct ddb_port *port)
{
	u8 val;

	if (i2c_read(&port->i2c->adap, 0x29, &val) < 0)
		return 0;
	if (i2c_read(&port->i2c->adap, 0x2a, &val) < 0)
		return 0;
	return 1;
}

static int port_has_stv0367(struct ddb_port *port)
{
	u8 val;

	if (i2c_read_reg16(&port->i2c->adap, 0x1e, 0xf000, &val) < 0)
		return 0;
	if (val != 0x60)
		return 0;
	if (i2c_read_reg16(&port->i2c->adap, 0x1f, 0xf000, &val) < 0)
		return 0;
	if (val != 0x60)
		return 0;
	return 1;
}

static int init_xo2(struct ddb_port *port)
{
	struct i2c_adapter *i2c = &port->i2c->adap;
	struct ddb *dev = port->dev;
	u8 val, data[2];
	int res;

	res = i2c_read_regs(i2c, 0x10, 0x04, data, 2);
	if (res < 0)
		return res;

	if (data[0] != 0x01)  {
		dev_info(dev->dev, "Port %d: invalid XO2\n", port->nr);
		return -1;
	}

	i2c_read_reg(i2c, 0x10, 0x08, &val);
	if (val != 0) {
		i2c_write_reg(i2c, 0x10, 0x08, 0x00);
		msleep(100);
	}
	/* Enable tuner power, disable pll, reset demods */
	i2c_write_reg(i2c, 0x10, 0x08, 0x04);
	usleep_range(2000, 3000);
	/* Release demod resets */
	i2c_write_reg(i2c, 0x10, 0x08, 0x07);

	/* speed: 0=55,1=75,2=90,3=104 MBit/s */
	i2c_write_reg(i2c, 0x10, 0x09, xo2_speed);

	if (dev->link[port->lnr].info->con_clock) {
		dev_info(dev->dev, "Setting continuous clock for XO2\n");
		i2c_write_reg(i2c, 0x10, 0x0a, 0x03);
		i2c_write_reg(i2c, 0x10, 0x0b, 0x03);
	} else {
		i2c_write_reg(i2c, 0x10, 0x0a, 0x01);
		i2c_write_reg(i2c, 0x10, 0x0b, 0x01);
	}

	usleep_range(2000, 3000);
	/* Start XO2 PLL */
	i2c_write_reg(i2c, 0x10, 0x08, 0x87);

	return 0;
}

static int init_xo2_ci(struct ddb_port *port)
{
	struct i2c_adapter *i2c = &port->i2c->adap;
	struct ddb *dev = port->dev;
	u8 val, data[2];
	int res;

	res = i2c_read_regs(i2c, 0x10, 0x04, data, 2);
	if (res < 0)
		return res;

	if (data[0] > 1)  {
		dev_info(dev->dev, "Port %d: invalid XO2 CI %02x\n",
			 port->nr, data[0]);
		return -1;
	}
	dev_info(dev->dev, "Port %d: DuoFlex CI %u.%u\n",
		 port->nr, data[0], data[1]);

	i2c_read_reg(i2c, 0x10, 0x08, &val);
	if (val != 0) {
		i2c_write_reg(i2c, 0x10, 0x08, 0x00);
		msleep(100);
	}
	/* Enable both CI */
	i2c_write_reg(i2c, 0x10, 0x08, 3);
	usleep_range(2000, 3000);

	/* speed: 0=55,1=75,2=90,3=104 MBit/s */
	i2c_write_reg(i2c, 0x10, 0x09, 1);

	i2c_write_reg(i2c, 0x10, 0x08, 0x83);
	usleep_range(2000, 3000);

	if (dev->link[port->lnr].info->con_clock) {
		dev_info(dev->dev, "Setting continuous clock for DuoFLex CI\n");
		i2c_write_reg(i2c, 0x10, 0x0a, 0x03);
		i2c_write_reg(i2c, 0x10, 0x0b, 0x03);
	} else {
		i2c_write_reg(i2c, 0x10, 0x0a, 0x01);
		i2c_write_reg(i2c, 0x10, 0x0b, 0x01);
	}
	return 0;
}

static int port_has_cxd28xx(struct ddb_port *port, u8 *id)
{
	struct i2c_adapter *i2c = &port->i2c->adap;
	int status;

	status = i2c_write_reg(&port->i2c->adap, 0x6e, 0, 0);
	if (status)
		return 0;
	status = i2c_read_reg(i2c, 0x6e, 0xfd, id);
	if (status)
		return 0;
	return 1;
}

static char *xo2names[] = {
	"DUAL DVB-S2", "DUAL DVB-C/T/T2",
	"DUAL DVB-ISDBT", "DUAL DVB-C/C2/T/T2",
	"DUAL ATSC", "DUAL DVB-C/C2/T/T2,ISDB-T",
	"", ""
};

static char *xo2types[] = {
	"DVBS_ST", "DVBCT2_SONY",
	"ISDBT_SONY", "DVBC2T2_SONY",
	"ATSC_ST", "DVBC2T2I_SONY"
};

static void ddb_port_probe(struct ddb_port *port)
{
	struct ddb *dev = port->dev;
	u32 l = port->lnr;
	struct ddb_link *link = &dev->link[l];
	u8 id, type;

	port->name = "NO MODULE";
	port->type_name = "NONE";
	port->class = DDB_PORT_NONE;

	/* Handle missing ports and ports without I2C */

	if (dummy_tuner && !port->nr &&
	    (link->ids.device == 0x0005 ||
	     link->ids.device == 0x000a)) {
		port->name = "DUMMY";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_DUMMY;
		port->type_name = "DUMMY";
		return;
	}

	if (port->nr == ts_loop) {
		port->name = "TS LOOP";
		port->class = DDB_PORT_LOOP;
		return;
	}

	if (port->nr == 1 && link->info->type == DDB_OCTOPUS_CI &&
	    link->info->i2c_mask == 1) {
		port->name = "NO TAB";
		port->class = DDB_PORT_NONE;
		return;
	}

	if (link->info->type == DDB_MOD) {
		port->name = "MOD";
		port->class = DDB_PORT_MOD;
		return;
	}
	if (link->info->type == DDB_OCTOPUS_MAX) {
		port->name = "DUAL DVB-S2 MAX";
		port->type_name = "MXL5XX";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_MXL5XX;
		if (port->i2c)
			ddbwritel(dev, I2C_SPEED_400,
				  port->i2c->regs + I2C_TIMING);
		return;
	}

	if ((link->info->type == DDB_OCTOPUS_MCI) &&
	    (port->nr < link->info->mci_ports)) {
		port->name = "DUAL MCI";
		port->type_name = "MCI";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_MCI + link->info->mci_type;
		return;
	}

	if (port->nr > 1 && link->info->type == DDB_OCTOPUS_CI) {
		port->name = "CI internal";
		port->type_name = "INTERNAL";
		port->class = DDB_PORT_CI;
		port->type = DDB_CI_INTERNAL;
	}

	if (!port->i2c)
		return;

	/* Probe ports with I2C */

	if (port_has_cxd(port, &id)) {
		if (id == 1) {
			port->name = "CI";
			port->type_name = "CXD2099";
			port->class = DDB_PORT_CI;
			port->type = DDB_CI_EXTERNAL_SONY;
			ddbwritel(dev, I2C_SPEED_400,
				  port->i2c->regs + I2C_TIMING);
		} else {
			dev_info(dev->dev, "Port %d: Uninitialized DuoFlex\n",
				 port->nr);
			return;
		}
	} else if (port_has_xo2(port, &type, &id)) {
		ddbwritel(dev, I2C_SPEED_400, port->i2c->regs + I2C_TIMING);
		dev_info(dev->dev, "XO2 ID %02x\n", id);
		if (type == 2) {
			port->name = "DuoFlex CI";
			port->class = DDB_PORT_CI;
			port->type = DDB_CI_EXTERNAL_XO2;
			port->type_name = "CI_XO2";
			init_xo2_ci(port);
			return;
		}
		id >>= 2;
		if (id > 5) {
			port->name = "unknown XO2 DuoFlex";
			port->type_name = "UNKNOWN";
		} else {
			port->name = xo2names[id];
			port->class = DDB_PORT_TUNER;
			port->type = DDB_TUNER_XO2 + id;
			port->type_name = xo2types[id];
			init_xo2(port);
		}
	} else if (port_has_cxd28xx(port, &id)) {
		switch (id) {
		case 0xa4:
			port->name = "DUAL DVB-C2T2 CXD2843";
			port->type = DDB_TUNER_DVBC2T2_SONY_P;
			port->type_name = "DVBC2T2_SONY";
			break;
		case 0xb1:
			port->name = "DUAL DVB-CT2 CXD2837";
			port->type = DDB_TUNER_DVBCT2_SONY_P;
			port->type_name = "DVBCT2_SONY";
			break;
		case 0xb0:
			port->name = "DUAL ISDB-T CXD2838";
			port->type = DDB_TUNER_ISDBT_SONY_P;
			port->type_name = "ISDBT_SONY";
			break;
		case 0xc1:
			port->name = "DUAL DVB-C2T2 ISDB-T CXD2854";
			port->type = DDB_TUNER_DVBC2T2I_SONY_P;
			port->type_name = "DVBC2T2I_ISDBT_SONY";
			break;
		default:
			return;
		}
		port->class = DDB_PORT_TUNER;
		ddbwritel(dev, I2C_SPEED_400, port->i2c->regs + I2C_TIMING);
	} else if (port_has_stv0900(port)) {
		port->name = "DUAL DVB-S2";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_DVBS_ST;
		port->type_name = "DVBS_ST";
		ddbwritel(dev, I2C_SPEED_100, port->i2c->regs + I2C_TIMING);
	} else if (port_has_stv0900_aa(port, &id)) {
		port->name = "DUAL DVB-S2";
		port->class = DDB_PORT_TUNER;
		if (id == 0x51) {
			if (port->nr == 0 &&
			    link->info->ts_quirks & TS_QUIRK_REVERSED)
				port->type = DDB_TUNER_DVBS_STV0910_PR;
			else
				port->type = DDB_TUNER_DVBS_STV0910_P;
			port->type_name = "DVBS_ST_0910";
		} else {
			port->type = DDB_TUNER_DVBS_ST_AA;
			port->type_name = "DVBS_ST_AA";
		}
		ddbwritel(dev, I2C_SPEED_100, port->i2c->regs + I2C_TIMING);
	} else if (port_has_drxks(port)) {
		port->name = "DUAL DVB-C/T";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_DVBCT_TR;
		port->type_name = "DVBCT_TR";
		ddbwritel(dev, I2C_SPEED_400, port->i2c->regs + I2C_TIMING);
	} else if (port_has_stv0367(port)) {
		port->name = "DUAL DVB-C/T";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_DVBCT_ST;
		port->type_name = "DVBCT_ST";
		ddbwritel(dev, I2C_SPEED_100, port->i2c->regs + I2C_TIMING);
	} else if (port_has_encti(port)) {
		port->name = "ENCTI";
		port->class = DDB_PORT_LOOP;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int ddb_port_attach(struct ddb_port *port)
{
	int ret = 0;

	switch (port->class) {
	case DDB_PORT_TUNER:
		ret = dvb_input_attach(port->input[0]);
		if (ret < 0)
			break;
		ret = dvb_input_attach(port->input[1]);
		if (ret < 0)
			break;
		port->input[0]->redi = port->input[0];
		port->input[1]->redi = port->input[1];
		break;
	case DDB_PORT_CI:
		ret = ddb_ci_attach(port, ci_bitrate);
		if (ret < 0)
			break;
		fallthrough;
	case DDB_PORT_LOOP:
		ret = dvb_register_device(port->dvb[0].adap,
					  &port->dvb[0].dev,
					  &dvbdev_ci, (void *)port->output,
					  DVB_DEVICE_CI, 1);
		break;
	case DDB_PORT_MOD:
		ret = dvb_register_device(port->dvb[0].adap,
					  &port->dvb[0].dev,
					  &dvbdev_mod, (void *)port->output,
					  DVB_DEVICE_MOD, 1);
		break;
	default:
		break;
	}
	if (ret < 0)
		dev_err(port->dev->dev,
			"port_attach on port %d failed\n", port->nr);
	return ret;
}

static int ddb_ports_attach(struct ddb *dev)
{
	int i, ret = 0;
	struct ddb_port *port;

	dev->ns_num = dev->link[0].info->ns_num;
	for (i = 0; i < dev->ns_num; i++)
		dev->ns[i].nr = i;
	dev_info(dev->dev, "%d netstream channels\n", dev->ns_num);

	if (dev->port_num) {
		ret = dvb_register_adapters(dev);
		if (ret < 0) {
			dev_err(dev->dev, "Registering adapters failed. Check DVB_MAX_ADAPTERS in config.\n");
			return ret;
		}
	}
	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
		ret = ddb_port_attach(port);
		if (ret < 0)
			break;
	}
	return ret;
}

void ddb_ports_detach(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];

		switch (port->class) {
		case DDB_PORT_TUNER:
			dvb_input_detach(port->input[0]);
			dvb_input_detach(port->input[1]);
			break;
		case DDB_PORT_CI:
		case DDB_PORT_LOOP:
			if (port->dvb[0].dev)
				dvb_unregister_device(port->dvb[0].dev);
			if (port->en) {
				dvb_ca_en50221_release(port->en);
				kfree(port->en->data);
				port->en = NULL;
			}
			break;
		case DDB_PORT_MOD:
			if (port->dvb[0].dev)
				dvb_unregister_device(port->dvb[0].dev);
			break;
		}
	}
	dvb_unregister_adapters(dev);
}

/* Copy input DMA pointers to output DMA and ACK. */

static void input_write_output(struct ddb_input *input,
			       struct ddb_output *output)
{
	ddbwritel(output->port->dev,
		  input->dma->stat, DMA_BUFFER_ACK(output->dma));
	output->dma->cbuf = (input->dma->stat >> 11) & 0x1f;
	output->dma->coff = (input->dma->stat & 0x7ff) << 7;
}

static void output_ack_input(struct ddb_output *output,
			     struct ddb_input *input)
{
	ddbwritel(input->port->dev,
		  output->dma->stat, DMA_BUFFER_ACK(input->dma));
}

static void input_write_dvb(struct ddb_input *input,
			    struct ddb_input *input2)
{
	struct ddb_dvb *dvb = &input2->port->dvb[input2->nr & 1];
	struct ddb_dma *dma, *dma2;
	struct ddb *dev = input->port->dev;
	int ack = 1;

	dma = input->dma;
	dma2 = input->dma;
	/* if there also is an output connected, do not ACK.
	 * input_write_output will ACK.
	 */
	if (input->redo) {
		dma2 = input->redo->dma;
		ack = 0;
	}
	while (dma->cbuf != ((dma->stat >> 11) & 0x1f) || (4 & dma->ctrl)) {
		if (4 & dma->ctrl) {
			dev_warn(dev->dev, "Overflow dma input %u\n", input->nr);
			ack = 1;
		}
		if (alt_dma)
			dma_sync_single_for_cpu(dev->dev, dma2->pbuf[dma->cbuf],
						dma2->size, DMA_FROM_DEVICE);
		if (raw_stream || input->con) {
			dvb_dmx_swfilter_raw(&dvb->demux,
					     dma2->vbuf[dma->cbuf],
					     dma2->size);
		} else {
			if (dma2->unaligned || (dma2->vbuf[dma->cbuf][0] != 0x47)) {
				if (!dma2->unaligned) {
					dma2->unaligned++;
					dev_warn(dev->dev, "Input %u dma buffer unaligned, "
						 "switching to unaligned processing.\n",
						 input->nr);
					print_hex_dump(KERN_INFO, "TS: ", DUMP_PREFIX_OFFSET, 32, 1,
						       dma2->vbuf[dma->cbuf],
						       512, false);
				}
				dvb_dmx_swfilter(&dvb->demux,
						 dma2->vbuf[dma->cbuf],
						 dma2->size);
			} else
				dvb_dmx_swfilter_packets(&dvb->demux,
							 dma2->vbuf[dma->cbuf],
							 dma2->size / 188);
		}
		
		dma->cbuf = (dma->cbuf + 1) % dma2->num;
		if (ack)
			ddbwritel(dev, (dma->cbuf << 11),
				  DMA_BUFFER_ACK(dma));
		dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma));
		dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma));
	}
}

static void input_tasklet(unsigned long data)
{
	struct ddb_dma *dma = (struct ddb_dma *)data;
	struct ddb_input *input = (struct ddb_input *)dma->io;
	struct ddb *dev = input->port->dev;
	unsigned long flags;

	spin_lock_irqsave(&dma->lock, flags);
	if (!dma->running) {
		spin_unlock_irqrestore(&dma->lock, flags);
		return;
	}
	dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma));
	dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma));
	update_loss(dma);
	if (4 & dma->ctrl)
		dma->stall_count++;
	if (input->redi)
		input_write_dvb(input, input->redi);
	if (input->redo)
		input_write_output(input, input->redo);
	wake_up(&dma->wq);
	spin_unlock_irqrestore(&dma->lock, flags);
}

#ifdef OPTIMIZE_TASKLETS
static void input_handler(unsigned long data)
{
	struct ddb_input *input = (struct ddb_input *)data;
	struct ddb_dma *dma = input->dma;

	/* If there is no input connected, input_tasklet() will
	 * just copy pointers and ACK. So, there is no need to go
	 * through the tasklet scheduler.
	 */
	if (input->redi)
		tasklet_schedule(&dma->tasklet);
	else
		input_tasklet(data);
}

#else
static void input_handler(void *data)
{
	struct ddb_input *input = (struct ddb_input *)data;
	struct ddb_dma *dma = input->dma;

	input_tasklet((unsigned long)dma);
}
#endif

static void output_tasklet(unsigned long data)
{
	struct ddb_dma *dma = (struct ddb_dma *)data;
	struct ddb_output *output = (struct ddb_output *)dma->io;
	struct ddb *dev = output->port->dev;
	unsigned long flags;

	spin_lock_irqsave(&dma->lock, flags);
	if (!dma->running)
		goto unlock_exit;
	dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma));
	dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma));
	if (output->redi)
		output_ack_input(output, output->redi);
	wake_up(&dma->wq);
unlock_exit:
	spin_unlock_irqrestore(&dma->lock, flags);
}

#ifdef OPTIMIZE_TASKLETS
static void output_handler(void *data)
{
	struct ddb_output *output = (struct ddb_output *)data;
	struct ddb_dma *dma = output->dma;
	struct ddb *dev = output->port->dev;

	spin_lock(&dma->lock);
	if (!dma->running) {
		spin_unlock(&dma->lock);
		return;
	}
	dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma));
	dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma));
	if (output->redi)
		output_ack_input(output, output->redi);
	wake_up(&dma->wq);
	spin_unlock(&dma->lock);
}
#else
static void output_handler(void *data)
{
	struct ddb_output *output = (struct ddb_output *)data;
	struct ddb_dma *dma = output->dma;

	tasklet_schedule(&dma->tasklet);
}
#endif

/****************************************************************************/
/****************************************************************************/

static const struct ddb_regmap *io_regmap(struct ddb_io *io, int link)
{
	const struct ddb_info *info;

	if (link)
		info = io->port->dev->link[io->port->lnr].info;
	else
		info = io->port->dev->link[0].info;
	if (!info)
		return NULL;
	return info->regmap;
}

static void ddb_dma_init(struct ddb_io *io, int nr, int out, int irq_nr)
{
	struct ddb_dma *dma;
	const struct ddb_regmap *rm = io_regmap(io, 0);

	dma = out ? &io->port->dev->odma[nr] : &io->port->dev->idma[nr];
	io->dma = dma;
	dma->io = io;
	spin_lock_init(&dma->lock);
	init_waitqueue_head(&dma->wq);
	if (out) {
		tasklet_init(&dma->tasklet, output_tasklet, (unsigned long)dma);
		dma->regs = rm->odma->base + rm->odma->size * nr;
		dma->bufregs = rm->odma_buf->base + rm->odma_buf->size * nr;
		if (io->port->dev->link[0].info->type == DDB_MOD &&
		    io->port->dev->link[0].info->version >= 16) {
			dma->num = OUTPUT_DMA_BUFS_SDR;
			dma->size = OUTPUT_DMA_SIZE_SDR;
			dma->div = 1;
		} else {
			dma->num = dma_buf_num;
			dma->size = dma_buf_size * 128 * 47;
			dma->div = 1;
		}
	} else {
		tasklet_init(&dma->tasklet, input_tasklet, (unsigned long)dma);
		dma->regs = rm->idma->base + rm->idma->size * nr;
		dma->bufregs = rm->idma_buf->base + rm->idma_buf->size * nr;
		dma->num = dma_buf_num;
		dma->size = dma_buf_size * 128 * 47;
		dma->div = 1;
	}
	ddbwritel(io->port->dev, 0, DMA_BUFFER_ACK(dma));
}

static void ddb_input_init(struct ddb_port *port, int nr, int pnr, int anr)
{
	struct ddb *dev = port->dev;
	struct ddb_input *input = &dev->input[anr];
	const struct ddb_regmap *rm;

	port->input[pnr] = input;
	input->nr = nr;
	input->port = port;
	rm = io_regmap(input, 1);
	input->regs = DDB_LINK_TAG(port->lnr) |
		(rm->input->base + rm->input->size * nr);
#if 0
	dev_info(dev->dev, "init link %u, input %u, regs %08x\n",
		 port->lnr, nr, input->regs);
#endif
	if (dev->has_dma) {
		const struct ddb_regmap *rm0 = io_regmap(input, 0);
		u32 base = rm0->irq_base_idma;
		u32 dma_nr = nr;

		if (port->lnr)
			dma_nr += 32 + (port->lnr - 1) * 8;

		ddb_irq_set(dev, 0, dma_nr + base, &input_handler, input);
		ddb_dma_init(input, dma_nr, 0, dma_nr + base);
	}
}

static void ddb_output_init(struct ddb_port *port, int nr)
{
	struct ddb *dev = port->dev;
	struct ddb_output *output = &dev->output[nr];
	const struct ddb_regmap *rm;

	port->output = output;
	output->nr = nr;
	output->port = port;
	rm = io_regmap(output, 1);
	output->regs = DDB_LINK_TAG(port->lnr) |
		(rm->output->base + rm->output->size * nr);
	if (dev->has_dma) {
		const struct ddb_regmap *rm0 = io_regmap(output, 0);
		u32 base = rm0->irq_base_odma;

		ddb_irq_set(dev, 0, nr + base, &output_handler, output);
		ddb_dma_init(output, nr, 1, nr + base);
	}
}

static int ddb_port_match_i2c(struct ddb_port *port)
{
	struct ddb *dev = port->dev;
	u32 i;

	for (i = 0; i < dev->i2c_num; i++) {
		if (dev->i2c[i].link == port->lnr &&
		    dev->i2c[i].nr == port->nr) {
			port->i2c = &dev->i2c[i];
			return 1;
		}
	}
	return 0;
}

static int ddb_port_match_link_i2c(struct ddb_port *port)
{
	struct ddb *dev = port->dev;
	u32 i;

	for (i = 0; i < dev->i2c_num; i++) {
		if (dev->i2c[i].link == port->lnr) {
			port->i2c = &dev->i2c[i];
			return 1;
		}
	}
	return 0;
}

static void ddb_ports_init(struct ddb *dev)
{
	u32 i, l, p, ports;
	struct ddb_port *port;
	const struct ddb_info *info;
	const struct ddb_regmap *rm;

	for (p = l = 0; l < DDB_MAX_LINK; l++) {
		info = dev->link[l].info;
		if (!info)
			continue;
		rm = info->regmap;
		if (!rm)
			continue;
		ports = info->port_num;
		if ((l == 0) && (info->type == DDB_MOD) &&
		    (dev->link[0].ids.revision == 1)) {
			ports = ddbreadl(dev, 0x260) >> 24;
		}
		for (i = 0; i < ports; i++, p++) {
			port = &dev->port[p];
			port->dev = dev;
			port->nr = i;
			port->lnr = l;
			port->pnr = p;
			port->gap = 0xffffffff;
			port->obr = ci_bitrate;
			mutex_init(&port->i2c_gate_lock);
			if (!ddb_port_match_i2c(port))
				if (info->type == DDB_OCTOPUS_MAX)
					ddb_port_match_link_i2c(port);
			ddb_port_probe(port);

			port->dvb[0].adap = &dev->adap[2 * p];
			port->dvb[1].adap = &dev->adap[2 * p + 1];

			if ((port->class == DDB_PORT_NONE) && i &&
			    dev->port[p - 1].type == DDB_CI_EXTERNAL_XO2) {
				port->class = DDB_PORT_CI;
				port->type = DDB_CI_EXTERNAL_XO2_B;
				port->name = "DuoFlex CI_B";
				port->type_name = "CI_XO2_B";
				port->i2c = dev->port[p - 1].i2c;
			}
			dev_info(dev->dev, "Port %u: Link %u, Link Port %u (TAB %u): %s\n",
				 port->pnr, port->lnr, port->nr,
				 port->nr + 1, port->name);

			if (port->class == DDB_PORT_CI &&
			    port->type == DDB_CI_EXTERNAL_XO2) {
				ddb_input_init(port, 2 * i, 0, 2 * i);
				ddb_output_init(port, i);
				continue;
			}

			if (port->class == DDB_PORT_CI &&
			    port->type == DDB_CI_EXTERNAL_XO2_B) {
				ddb_input_init(port, 2 * i - 1, 0, 2 * i - 1);
				ddb_output_init(port, i);
				continue;
			}
			if (port->class == DDB_PORT_NONE)
				continue;
			
			switch (info->type) {
			case DDB_OCTOPUS_CI:
				if (i >= 2) {
					ddb_input_init(port, 2 + i, 0, 2 + i);
					ddb_input_init(port, 4 + i, 1, 4 + i);
					ddb_output_init(port, i);
					break;
				}
				fallthrough;
			case DDB_OCTONET:
			case DDB_OCTOPUS:
			case DDB_OCTOPRO:
				ddb_input_init(port, 2 * i, 0, 2 * i);
				ddb_input_init(port, 2 * i + 1, 1, 2 * i + 1);
				ddb_output_init(port, i);
				break;
			case DDB_OCTOPUS_MAX:
			case DDB_OCTOPUS_MAX_CT:
			case DDB_OCTOPUS_MCI:
				ddb_input_init(port, 2 * i, 0, 2 * p);
				ddb_input_init(port, 2 * i + 1, 1, 2 * p + 1);
				break;
			case DDB_MOD:
				ddb_output_init(port, i);
				ddb_irq_set(dev, 0, i + rm->irq_base_rate,
					    &ddbridge_mod_rate_handler,
					    &dev->output[i]);
				break;
			default:
				break;
			}
		}
	}
	dev->port_num = p;
}

void ddb_ports_release(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
		if (port->input[0] && port->input[0]->dma)
			tasklet_kill(&port->input[0]->dma->tasklet);
		if (port->input[1] && port->input[1]->dma)
			tasklet_kill(&port->input[1]->dma->tasklet);
		if (port->output && port->output->dma)
			tasklet_kill(&port->output->dma->tasklet);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#define IRQ_HANDLE(_n) \
	do { if ((s & (1UL << ((_n) & 0x1f))) && dev->link[0].irq[_n].handler) \
		dev->link[0].irq[_n].handler(dev->link[0].irq[_n].data); }			\
	while (0)

#define IRQ_HANDLE_NIBBLE(_shift) { \
	if (s & (0x0000000f << ((_shift) & 0x1f))) { \
		IRQ_HANDLE(0 + (_shift)); \
		IRQ_HANDLE(1 + (_shift)); \
		IRQ_HANDLE(2 + (_shift)); \
		IRQ_HANDLE(3 + (_shift)); \
	} \
}


#define IRQ_HANDLE_BYTE(_shift) {		     \
	if (s & (0x000000ff << ((_shift) & 0x1f))) { \
		IRQ_HANDLE(0 + (_shift)); \
		IRQ_HANDLE(1 + (_shift)); \
		IRQ_HANDLE(2 + (_shift)); \
		IRQ_HANDLE(3 + (_shift)); \
		IRQ_HANDLE(4 + (_shift)); \
		IRQ_HANDLE(5 + (_shift)); \
		IRQ_HANDLE(6 + (_shift)); \
		IRQ_HANDLE(7 + (_shift)); \
	} \
	}

static void irq_handle_msg(struct ddb *dev, u32 s)
{
	dev->i2c_irq++;
	IRQ_HANDLE_NIBBLE(0);
}

static void irq_handle_io(struct ddb *dev, u32 s)
{
	dev->ts_irq++;
	IRQ_HANDLE_NIBBLE(4);
	IRQ_HANDLE_BYTE(8);
	IRQ_HANDLE_BYTE(16);
	IRQ_HANDLE_BYTE(24);
}

irqreturn_t ddb_irq_handler0(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *)dev_id;
	u32 mask = 0x8fffff00;
	u32 s = mask & ddbreadl(dev, INTERRUPT_STATUS);

	if (!s)
		return IRQ_NONE;
	do {
		if (s & 0x80000000)
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_ACK);
		irq_handle_io(dev, s);
	} while ((s = mask & ddbreadl(dev, INTERRUPT_STATUS)));

	return IRQ_HANDLED;
}

irqreturn_t ddb_irq_handler1(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *)dev_id;
	u32 mask = 0x8000000f;
	u32 s = mask & ddbreadl(dev, INTERRUPT_STATUS);

	if (!s)
		return IRQ_NONE;
	do {
		if (s & 0x80000000)
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_ACK);
		irq_handle_msg(dev, s);
	} while ((s = mask & ddbreadl(dev, INTERRUPT_STATUS)));

	return IRQ_HANDLED;
}

irqreturn_t ddb_irq_handler(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *)dev_id;
	u32 s = ddbreadl(dev, INTERRUPT_STATUS);
	int ret = IRQ_HANDLED;

	if (!s)
		return IRQ_NONE;
	do {
		if (s & 0x80000000)
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_ACK);

		if (s & 0x0000000f)
			irq_handle_msg(dev, s);
		if (s & 0x0fffff00) {
			irq_handle_io(dev, s);
		}
	} while ((s = ddbreadl(dev, INTERRUPT_STATUS)));

	return ret;
}

static irqreturn_t irq_handle_v2_n(struct ddb *dev, u32 n)
{
	u32 reg = INTERRUPT_V2_STATUS + 4 * n;
	u32 s = ddbreadl(dev, reg);
	u32 off = n * 32;

	if (!s)
		return IRQ_NONE;
	ddbwritel(dev, s, reg);

	IRQ_HANDLE_BYTE(0 + off);
	IRQ_HANDLE_BYTE(8 + off);
	IRQ_HANDLE_BYTE(16 + off);
	IRQ_HANDLE_BYTE(24 + off);
	return IRQ_HANDLED;
}

irqreturn_t ddb_irq_handler_v2(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *)dev_id;
	u32 s = 0xffff & ddbreadl(dev, INTERRUPT_V2_STATUS);
	int ret = IRQ_HANDLED;

	if (!s)
		return IRQ_NONE;
	do {
		if (s & 0x80)
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_V2_STATUS);
		if (s & 0x00000001)
			irq_handle_v2_n(dev, 1);
		if (s & 0x00000002)
			irq_handle_v2_n(dev, 2);
		if (s & 0x00000004)
			irq_handle_v2_n(dev, 3);
		IRQ_HANDLE_NIBBLE(8);
	} while ((s = 0xffff & ddbreadl(dev, INTERRUPT_V2_STATUS)));

	return ret;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static ssize_t nsd_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int nsd_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static int nsd_release(struct inode *inode, struct file *file)
{
	return dvb_generic_release(inode, file);
}

static int nsd_open(struct inode *inode, struct file *file)
{
	return dvb_generic_open(inode, file);
}

static struct ddb_input *plugtoinput(struct ddb *dev, u8 plug)
{
	int i, j;

	for (i = j = 0; i < dev->port_num; i++) {
		if (dev->port[i].class == DDB_PORT_TUNER) {
			if (j == plug)
				return dev->port[i].input[0];
			if (j + 1 == plug)
				return dev->port[i].input[1];
			j += 2;
		}
	}
	return 0;
}

static int nsd_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct ddb *dev = dvbdev->priv;

	/* unsigned long arg = (unsigned long)parg; */
	int ret = 0;

	switch (cmd) {
	case NSD_START_GET_TS:
	{
		struct dvb_nsd_ts *ts = parg;
		struct ddb_input *input = plugtoinput(dev, ts->input);
		u32 ctrl;
		u32 to;

		if (!input)
			return -EINVAL;
		ctrl = (input->port->lnr << 16) | ((input->nr & 7) << 8) |
			((ts->filter_mask & 3) << 2);

		if (ddbreadl(dev, TS_CAPTURE_CONTROL) & 1) {
			dev_info(dev->dev, "ts capture busy\n");
			return -EBUSY;
		}
		ddb_dvb_ns_input_start(input);

		ddbwritel(dev, ctrl, TS_CAPTURE_CONTROL);
		ddbwritel(dev, ts->pid, TS_CAPTURE_PID);
		ddbwritel(dev, (ts->section_id << 16) |
			  (ts->table << 8) | ts->section,
			  TS_CAPTURE_TABLESECTION);
		/* 1024 ms default timeout if timeout set to 0 */
		if (ts->timeout)
			to = ts->timeout;
		else
			to = 1024;
		/* 21 packets default if num set to 0 */
		if (ts->num)
			to |= ((u32)ts->num << 16);
		else
			to |= (21 << 16);
		ddbwritel(dev, to, TS_CAPTURE_TIMEOUT);
		if (ts->mode)
			ctrl |= 2;
		ddbwritel(dev, ctrl | 1, TS_CAPTURE_CONTROL);
		break;
	}
	case NSD_POLL_GET_TS:
	{
		struct dvb_nsd_ts *ts = parg;
		u32 ctrl = ddbreadl(dev, TS_CAPTURE_CONTROL);

		if (ctrl & 1)
			return -EBUSY;
		if (ctrl & (1 << 14))
			return -EAGAIN;
		ddbcpyfrom(dev, dev->tsbuf, TS_CAPTURE_MEMORY,
			   TS_CAPTURE_LEN);
		ts->len = ddbreadl(dev, TS_CAPTURE_RECEIVED) & 0x1fff;
		if (copy_to_user(ts->ts, dev->tsbuf, ts->len))
			return -EIO;
		break;
	}
	case NSD_CANCEL_GET_TS:
	{
		u32 ctrl = 0;

		ddbwritel(dev, ctrl, TS_CAPTURE_CONTROL);
		ctrl = ddbreadl(dev, TS_CAPTURE_CONTROL);
		break;
	}
	case NSD_STOP_GET_TS:
	{
		struct dvb_nsd_ts *ts = parg;
		struct ddb_input *input = plugtoinput(dev, ts->input);
		u32 ctrl = ddbreadl(dev, TS_CAPTURE_CONTROL);

		if (!input)
			return -EINVAL;
		if (ctrl & 1) {
			dev_info(dev->dev,
				 "cannot stop ts capture, while it was neither finished nor canceled\n");
			return -EBUSY;
		}
		ddb_dvb_ns_input_stop(input);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static long nsd_ioctl(struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	return ddb_dvb_usercopy(file, cmd, arg, nsd_do_ioctl);
}

static const struct file_operations nsd_fops = {
	.owner   = THIS_MODULE,
	.read    = nsd_read,
	.open    = nsd_open,
	.release = nsd_release,
	.poll    = nsd_poll,
	.unlocked_ioctl = nsd_ioctl,
};

static struct dvb_device dvbdev_nsd = {
	.priv    = 0,
	.readers = 1,
	.writers = 1,
	.users   = 1,
	.fops    = &nsd_fops,
};

static int ddb_nsd_attach(struct ddb *dev)
{
	int ret;

	if (!dev->link[0].info->ns_num)
		return 0;
	ret = dvb_register_device(&dev->adap[0],
				  &dev->nsd_dev,
				  &dvbdev_nsd, (void *)dev,
				  DVB_DEVICE_NSD, 0);
	return ret;
}

void ddb_nsd_detach(struct ddb *dev)
{
	if (!dev->link[0].info->ns_num)
		return;

	if (dev->nsd_dev->users > 2) {
		wait_event(dev->nsd_dev->wait_queue,
			   dev->nsd_dev->users == 2);
	}
	dvb_unregister_device(dev->nsd_dev);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int reg_wait(struct ddb *dev, u32 reg, u32 bit)
{
	u32 count = 0;

	while (ddbreadl(dev, reg) & bit) {
		ndelay(10);
		if (++count == 100)
			return -1;
	}
	return 0;
}

static int flashio(struct ddb *dev, u32 lnr,
		   u8 *wbuf, u32 wlen, u8 *rbuf, u32 rlen)
{
	u32 data, shift;
	u32 tag = DDB_LINK_TAG(lnr);
	struct ddb_link *link = &dev->link[lnr];

	mutex_lock(&link->flash_mutex);
	if (wlen > 4)
		ddbwritel(dev, 1, tag | SPI_CONTROL);
	while (wlen > 4) {
		/* FIXME: check for big-endian */
		data = swab32(*(u32 *)wbuf);
		wbuf += 4;
		wlen -= 4;
		ddbwritel(dev, data, tag | SPI_DATA);
		if (reg_wait(dev, tag | SPI_CONTROL, 4))
			goto fail;
	}
	if (rlen)
		ddbwritel(dev, 0x0001 | ((wlen << (8 + 3)) & 0x1f00),
			  tag | SPI_CONTROL);
	else
		ddbwritel(dev, 0x0003 | ((wlen << (8 + 3)) & 0x1f00),
			  tag | SPI_CONTROL);

	data = 0;
	shift = ((4 - wlen) * 8);
	while (wlen) {
		data <<= 8;
		data |= *wbuf;
		wlen--;
		wbuf++;
	}
	if (shift)
		data <<= shift;
	ddbwritel(dev, data, tag | SPI_DATA);
	if (reg_wait(dev, tag | SPI_CONTROL, 4))
		goto fail;

	if (!rlen) {
		ddbwritel(dev, 0, tag | SPI_CONTROL);
		goto exit;
	}
	if (rlen > 4)
		ddbwritel(dev, 1, tag | SPI_CONTROL);

	while (rlen > 4) {
		ddbwritel(dev, 0xffffffff, tag | SPI_DATA);
		if (reg_wait(dev, tag | SPI_CONTROL, 4))
			goto fail;
		data = ddbreadl(dev, tag | SPI_DATA);
		*(u32 *)rbuf = swab32(data);
		rbuf += 4;
		rlen -= 4;
	}
	ddbwritel(dev, 0x0003 | ((rlen << (8 + 3)) & 0x1F00),
		  tag | SPI_CONTROL);
	ddbwritel(dev, 0xffffffff, tag | SPI_DATA);
	if (reg_wait(dev, tag | SPI_CONTROL, 4))
		goto fail;

	data = ddbreadl(dev, tag | SPI_DATA);
	ddbwritel(dev, 0, tag | SPI_CONTROL);

	if (rlen < 4)
		data <<= ((4 - rlen) * 8);

	while (rlen > 0) {
		*rbuf = ((data >> 24) & 0xff);
		data <<= 8;
		rbuf++;
		rlen--;
	}
exit:
	mutex_unlock(&link->flash_mutex);
	return 0;
fail:
	mutex_unlock(&link->flash_mutex);
	return -1;
}

int ddbridge_flashread(struct ddb *dev, u32 link, u8 *buf, u32 addr, u32 len)
{
	u8 cmd[4] = {0x03, (addr >> 16) & 0xff,
		     (addr >> 8) & 0xff, addr & 0xff};

	return flashio(dev, link, cmd, 4, buf, len);
}

static int mdio_write(struct ddb *dev, u8 adr, u8 reg, u16 val, u32 mdio_base)
{
	ddbwritel(dev, adr, MDIO_ADR_OFF + mdio_base);
	ddbwritel(dev, reg, MDIO_REG_OFF + mdio_base);
	ddbwritel(dev, val, MDIO_VAL_OFF + mdio_base);
	ddbwritel(dev, 0x03, MDIO_CTRL_OFF + mdio_base);
	while (ddbreadl(dev, MDIO_CTRL_OFF + mdio_base) & 0x02)
		ndelay(500);
	return 0;
}

static u16 mdio_read(struct ddb *dev, u8 adr, u8 reg, u32 mdio_base)
{
	ddbwritel(dev, adr, MDIO_ADR_OFF + mdio_base);
	ddbwritel(dev, reg, MDIO_REG_OFF + mdio_base);
	ddbwritel(dev, 0x07, MDIO_CTRL_OFF + mdio_base);
	while (ddbreadl(dev, MDIO_CTRL_OFF + mdio_base) & 0x02)
		ndelay(500);
	return ddbreadl(dev, MDIO_VAL_OFF + mdio_base);
}

#define DDB_NAME "ddbridge"

static u32 ddb_num;
static int ddb_major;
static DEFINE_MUTEX(ddb_mutex);

static int ddb_release(struct inode *inode, struct file *file)
{
	struct ddb *dev = file->private_data;

	atomic_inc(&dev->ddb_dev_users);
	return 0;
}

static int ddb_open(struct inode *inode, struct file *file)
{
	struct ddb *dev = ddbs[iminor(inode)];

	if (!atomic_dec_and_test(&dev->ddb_dev_users)) {
		atomic_inc(&dev->ddb_dev_users);
		return -EBUSY;
	}
	file->private_data = dev;
	return 0;
}

static long ddb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ddb *dev = file->private_data;
	void *parg = (void *)arg;
	int res;

	switch (cmd) {
	case IOCTL_DDB_FLASHIO:
	{
		struct ddb_flashio fio;
		u8 *rbuf, *wbuf;

		if (copy_from_user(&fio, parg, sizeof(fio)))
			return -EFAULT;
		if (fio.write_len > 1028 || fio.read_len > 1028)
			return -EINVAL;
		if (fio.write_len + fio.read_len > 1028)
			return -EINVAL;
		if (fio.link > 3)
			return -EINVAL;

		wbuf = &dev->iobuf[0];
		rbuf = wbuf + fio.write_len;

		if (copy_from_user(wbuf, fio.write_buf, fio.write_len))
			return -EFAULT;
		res = flashio(dev, fio.link, wbuf,
			      fio.write_len, rbuf, fio.read_len);
		if (res)
			return res;
		if (copy_to_user(fio.read_buf, rbuf, fio.read_len))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_GPIO_OUT:
	{
		struct ddb_gpio gpio;

		if (copy_from_user(&gpio, parg, sizeof(gpio)))
			return -EFAULT;
		ddbwritel(dev, gpio.mask, GPIO_DIRECTION);
		ddbwritel(dev, gpio.data, GPIO_OUTPUT);
		break;
	}
	case IOCTL_DDB_ID:
	{
		struct ddb_id ddbid;

		ddbid.vendor = dev->link[0].ids.vendor;
		ddbid.device = dev->link[0].ids.device;
		ddbid.subvendor = dev->link[0].ids.subvendor;
		ddbid.subdevice = dev->link[0].ids.subdevice;
		ddbid.hw = dev->link[0].ids.hwid;
		ddbid.regmap = dev->link[0].ids.regmapid;
		if (copy_to_user(parg, &ddbid, sizeof(ddbid)))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_READ_REG:
	{
		struct ddb_reg reg;

		if (copy_from_user(&reg, parg, sizeof(reg)))
			return -EFAULT;
		if ((reg.reg & 0xfffffff) >= dev->regs_len)
			return -EINVAL;
		reg.val = ddbreadl(dev, reg.reg);
		if (copy_to_user(parg, &reg, sizeof(reg)))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_WRITE_REG:
	{
		struct ddb_reg reg;

		if (copy_from_user(&reg, parg, sizeof(reg)))
			return -EFAULT;
		if ((reg.reg & 0xfffffff) >= dev->regs_len)
			return -EINVAL;
		ddbwritel(dev, reg.val, reg.reg);
		break;
	}
	case IOCTL_DDB_READ_MDIO:
	{
		struct ddb_mdio mdio;
		u32 mdio_base = dev->link[0].info->mdio_base;

		if (!mdio_base)
			return -EIO;
		if (copy_from_user(&mdio, parg, sizeof(mdio)))
			return -EFAULT;
		mdio.val = mdio_read(dev, mdio.adr, mdio.reg, mdio_base);
		if (copy_to_user(parg, &mdio, sizeof(mdio)))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_WRITE_MDIO:
	{
		struct ddb_mdio mdio;
		u32 mdio_base = dev->link[0].info->mdio_base;

		if (!mdio_base)
			return -EIO;
		if (copy_from_user(&mdio, parg, sizeof(mdio)))
			return -EFAULT;
		mdio_write(dev, mdio.adr, mdio.reg, mdio.val, mdio_base);
		break;
	}
	case IOCTL_DDB_READ_MEM:
	{
		struct ddb_mem mem;
		u8 *buf = &dev->iobuf[0];

		if (copy_from_user(&mem, parg, sizeof(mem)))
			return -EFAULT;
		if ((((mem.len + mem.off) & 0xfffffff) > dev->regs_len) ||
		    mem.len > 1024)
			return -EINVAL;
		ddbcpyfrom(dev, buf, mem.off, mem.len);
		if (copy_to_user(mem.buf, buf, mem.len))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_WRITE_MEM:
	{
		struct ddb_mem mem;
		u8 *buf = &dev->iobuf[0];

		if (copy_from_user(&mem, parg, sizeof(mem)))
			return -EFAULT;
		if ((((mem.len + mem.off) & 0xfffffff) > dev->regs_len) ||
		    mem.len > 1024)
			return -EINVAL;
		if (copy_from_user(buf, mem.buf, mem.len))
			return -EFAULT;
		ddbcpyto(dev, mem.off, buf, mem.len);
		break;
	}
	case IOCTL_DDB_READ_I2C:
	{
		struct ddb_i2c_msg i2c;
		struct i2c_adapter *adap;
		u8 *mbuf, *hbuf = &dev->iobuf[0];

		if (copy_from_user(&i2c, parg, sizeof(i2c)))
			return -EFAULT;
		if (i2c.bus > dev->i2c_num)
			return -EINVAL;
		if (i2c.mlen + i2c.hlen > 512)
			return -EINVAL;

		adap = &dev->i2c[i2c.bus].adap;
		mbuf = hbuf + i2c.hlen;

		if (copy_from_user(hbuf, i2c.hdr, i2c.hlen))
			return -EFAULT;
		if (i2c_io(adap, i2c.adr, hbuf, i2c.hlen, mbuf, i2c.mlen) < 0)
			return -EIO;
		if (copy_to_user(i2c.msg, mbuf, i2c.mlen))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_WRITE_I2C:
	{
		struct ddb_i2c_msg i2c;
		struct i2c_adapter *adap;
		u8 *buf = &dev->iobuf[0];

		if (copy_from_user(&i2c, parg, sizeof(i2c)))
			return -EFAULT;
		if (i2c.bus > dev->i2c_num)
			return -EINVAL;
		if (i2c.mlen + i2c.hlen > 250)
			return -EINVAL;

		adap = &dev->i2c[i2c.bus].adap;
		if (copy_from_user(buf, i2c.hdr, i2c.hlen))
			return -EFAULT;
		if (copy_from_user(buf + i2c.hlen, i2c.msg, i2c.mlen))
			return -EFAULT;
		if (i2c_write(adap, i2c.adr, buf, i2c.hlen + i2c.mlen) < 0)
			return -EIO;
		break;
	}
	case IOCTL_DDB_MCI_CMD:
	{
		struct ddb_mci_msg msg;
		struct ddb_link *link;
		int res;

		if (copy_from_user(&msg, parg, sizeof(msg)))
			return -EFAULT;
		if (msg.link > 3)
			return -EFAULT;
		link = &dev->link[msg.link];
		res = ddb_mci_cmd_link(link, &msg.cmd, &msg.res);
		if (copy_to_user(parg, &msg, sizeof(msg)))
			return -EFAULT;
		return res;
	}
	default:
		return -ENOTTY;
	}
	return 0;
}

static const struct file_operations ddb_fops = {
	.unlocked_ioctl = ddb_ioctl,
	.open           = ddb_open,
	.release        = ddb_release,
};

#if (KERNEL_VERSION(3, 4, 0) > LINUX_VERSION_CODE)
static char *ddb_devnode(struct device *device, mode_t *mode)
#else
static char *ddb_devnode(struct device *device, umode_t *mode)
#endif
{
	struct ddb *dev = dev_get_drvdata(device);

	return kasprintf(GFP_KERNEL, "ddbridge/card%d", dev->nr);
}

#define __ATTR_MRO(_name, _show) {				\
	.attr	= { .name = __stringify(_name), .mode = 0444 },	\
	.show	= _show,					\
}

#define __ATTR_MWO(_name, _store) {				\
	.attr	= { .name = __stringify(_name), .mode = 0222 },	\
	.store	= _store,					\
}

static ssize_t ports_show(struct device *device,
			  struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%d\n", dev->port_num);
}

static ssize_t ts_irq_show(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%d\n", dev->ts_irq);
}

static ssize_t i2c_irq_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%d\n", dev->i2c_irq);
}

static char *class_name[] = {
	"NONE", "CI", "TUNER", "LOOP", "MOD"
};

static ssize_t fan_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	u32 val;

	val = ddbreadl(dev, GPIO_OUTPUT) & 1;
	return sprintf(buf, "%d\n", val);
}

static ssize_t fan_store(struct device *device, struct device_attribute *d,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	u32 val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	ddbwritel(dev, 1, GPIO_DIRECTION);
	ddbwritel(dev, val & 1, GPIO_OUTPUT);
	return count;
}

static ssize_t fanspeed_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[8] - 0x30;
	struct ddb_link *link = &dev->link[num];
	u32 spd;

	spd = ddblreadl(link, TEMPMON_FANCONTROL) & 0xff;
	return sprintf(buf, "%u\n", spd * 100);
}

static ssize_t temp_show(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	struct ddb_link *link;
	struct i2c_adapter *adap;
	s32 temp, temp2, temp3;
	int i;
	u8 tmp[2];
	int l = 0;

	if (attr->attr.name[4] == 'l')
		l = attr->attr.name[5] - 0x30;
	link = &dev->link[l];

	if (link->info->type == DDB_MOD) {
		if (link->info->version >= 2) {
			temp = 0xffff & ddbreadl(dev, TEMPMON2_BOARD);
			temp = (temp * 1000) >> 8;

			temp2 = 0xffff & ddbreadl(dev, TEMPMON2_FPGACORE);
			temp2 = (temp2 * 1000) >> 8;

			temp3 = 0xffff & ddbreadl(dev, TEMPMON2_QAMCORE);
			temp3 = (temp3 * 1000) >> 8;

			return sprintf(buf, "%d %d %d\n", temp, temp2, temp3);
		}
		ddbwritel(dev, 1, TEMPMON_CONTROL);
		for (i = 0; i < 10; i++) {
			if (0 == (1 & ddbreadl(dev, TEMPMON_CONTROL)))
				break;
			usleep_range(1000, 2000);
		}
		temp = ddbreadl(dev, TEMPMON_SENSOR1);
		temp2 = ddbreadl(dev, TEMPMON_SENSOR2);
		temp = (temp * 1000) >> 8;
		temp2 = (temp2 * 1000) >> 8;
		if (ddbreadl(dev, TEMPMON_CONTROL) & 0x8000) {
			temp3 = ddbreadl(dev, TEMPMON_CORE);
			temp3 = (temp3 * 1000) >> 8;
			return sprintf(buf, "%d %d %d\n", temp, temp2, temp3);
		}
		return sprintf(buf, "%d %d\n", temp, temp2);
	}
	if (link->info->type == DDB_OCTOPUS_MCI) {
		temp = 0xffff & ddblreadl(link, TEMPMON_SENSOR0);
		temp = (temp * 1000) >> 8;

		return sprintf(buf, "%d\n", temp);
	}
	if (!link->info->temp_num)
		return sprintf(buf, "no sensor\n");
	adap = &dev->i2c[link->info->temp_bus].adap;
	if (i2c_read_regs(adap, 0x48, 0, tmp, 2) < 0)
		return sprintf(buf, "read_error\n");
	temp = (tmp[0] << 3) | (tmp[1] >> 5);
	temp *= 125;
	if (link->info->temp_num == 2) {
		if (i2c_read_regs(adap, 0x49, 0, tmp, 2) < 0)
			return sprintf(buf, "read_error\n");
		temp2 = (tmp[0] << 3) | (tmp[1] >> 5);
		temp2 *= 125;
		return sprintf(buf, "%d %d\n", temp, temp2);
	}
	return sprintf(buf, "%d\n", temp);
}

static ssize_t ctemp_show(struct device *device,
			  struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	struct i2c_adapter *adap;
	int temp;
	u8 tmp[2];
	int num = attr->attr.name[4] - 0x30;

	adap = &dev->i2c[num].adap;
	if (!adap)
		return 0;
	if (i2c_read_regs(adap, 0x49, 0, tmp, 2) < 0)
		if (i2c_read_regs(adap, 0x4d, 0, tmp, 2) < 0)
			return sprintf(buf, "no sensor\n");
	temp = tmp[0] * 1000;
	return sprintf(buf, "%d\n", temp);
}

static ssize_t mod_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;

	return sprintf(buf, "%s:%s\n",
		       class_name[dev->port[num].class],
		       dev->port[num].type_name);
}

static ssize_t led_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;

	return sprintf(buf, "%d\n", dev->leds & (1 << num) ? 1 : 0);
}

static void ddb_set_led(struct ddb *dev, int num, int val)
{
	if (!dev->link[0].info->led_num)
		return;
	switch (dev->port[num].class) {
	case DDB_PORT_TUNER:
		switch (dev->port[num].type) {
		case DDB_TUNER_DVBS_ST:
			i2c_write_reg16(&dev->i2c[num].adap,
					0x69, 0xf14c, val ? 2 : 0);
			break;
		case DDB_TUNER_DVBCT_ST:
			i2c_write_reg16(&dev->i2c[num].adap,
					0x1f, 0xf00e, 0);
			i2c_write_reg16(&dev->i2c[num].adap,
					0x1f, 0xf00f, val ? 1 : 0);
			break;
		case DDB_TUNER_XO2 ... DDB_TUNER_DVBC2T2I_SONY:
		{
			u8 v;

			i2c_read_reg(&dev->i2c[num].adap, 0x10, 0x08, &v);
			v = (v & ~0x10) | (val ? 0x10 : 0);
			i2c_write_reg(&dev->i2c[num].adap, 0x10, 0x08, v);
			break;
		}
		default:
			break;
		}
		break;
	}
}

static ssize_t led_store(struct device *device,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;
	u32 val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val)
		dev->leds |= (1 << num);
	else
		dev->leds &= ~(1 << num);
	ddb_set_led(dev, num, val);
	return count;
}

static ssize_t snr_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	char snr[32];
	int num = attr->attr.name[3] - 0x30;
	struct ddb_port *port = &dev->port[num];
	struct i2c_adapter *i2c = &port->i2c->adap;

	switch (port->type) {
	case DDB_CI_EXTERNAL_XO2:
	case DDB_TUNER_XO2 ... DDB_TUNER_DVBC2T2I_SONY:
		if (i2c_read_regs(i2c, 0x10, 0x10, snr, 16) < 0)
			return sprintf(buf, "NO SNR\n");
		snr[16] = 0;
		break;
	default:
		/* serial number at 0x100-0x11f */
		if (i2c_read_regs16(i2c, 0x57, 0x100, snr, 32) < 0)
			if (i2c_read_regs16(i2c, 0x50, 0x100, snr, 32) < 0)
				return sprintf(buf, "NO SNR\n");
		snr[31] = 0; /* in case it is not terminated on EEPROM */
		break;
	}
	return sprintf(buf, "%s\n", snr);
}

static ssize_t snr_store(struct device *device, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;
	u8 snr[34] = { 0x01, 0x00 };
	struct ddb_port *port = &dev->port[num];
	struct i2c_adapter *i2c = &port->i2c->adap;

	return 0; /* NOE: remove completely? */
	if (count > 31)
		return -EINVAL;
	if (port->type >= DDB_TUNER_XO2)
		return -EINVAL;
	memcpy(snr + 2, buf, count);
	i2c_write(i2c, 0x57, snr, 34);
	i2c_write(i2c, 0x50, snr, 34);
	return count;
}

static ssize_t bsnr_show(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	char snr[16];

	ddbridge_flashread(dev, 0, snr, 0x10, 15);
	snr[15] = 0; /* in case it is not terminated on EEPROM */
	return sprintf(buf, "%s\n", snr);
}

static ssize_t bpsnr_show(struct device *device,
			  struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	unsigned char snr[32];

	if (!dev->i2c_num)
		return 0;

	if (i2c_read_regs16(&dev->i2c[0].adap,
			    0x50, 0x0000, snr, 32) < 0 ||
	    snr[0] == 0xff)
		return sprintf(buf, "NO SNR\n");
	snr[31] = 0; /* in case it is not terminated on EEPROM */
	return sprintf(buf, "%s\n", snr);
}

static ssize_t gtl_snr_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[6] - 0x30;
	char snr[16];

	ddbridge_flashread(dev, num, snr, 0x10, 15);
	snr[15] = 0; /* in case it is not terminated on EEPROM */
	return sprintf(buf, "%s\n", snr);
}

static ssize_t gtl_snr_store(struct device *device, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	return 0;
}

static ssize_t redirect_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t redirect_store(struct device *device,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int i, p;
	int res;

	if (sscanf(buf, "%x %x\n", &i, &p) != 2)
		return -EINVAL;
	res = ddb_redirect(i, p);
	if (res < 0)
		return res;
	dev_info(device, "redirect: %02x, %02x\n", i, p);
	return count;
}

static ssize_t gap_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;

	return sprintf(buf, "%d\n", dev->port[num].gap);
}

static ssize_t gap_store(struct device *device, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val > 128)
		return -EINVAL;
	if (val == 128)
		val = 0xffffffff;
	dev->port[num].gap = val;
	return count;
}

static ssize_t obr_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;

	return sprintf(buf, "%d\n", dev->port[num].obr);
}

static ssize_t obr_store(struct device *device, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val > 96000)
		return -EINVAL;
	dev->port[num].obr = val;
	return count;
}

static ssize_t version_show(struct device *device,
			    struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%08x %08x\n",
		       dev->link[0].ids.hwid, dev->link[0].ids.regmapid);
}

static ssize_t hwid_show(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "0x%08X\n", dev->link[0].ids.hwid);
}

static ssize_t regmap_show(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "0x%08X\n", dev->link[0].ids.regmapid);
}

static ssize_t vlan_show(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%u\n", dev->vlan);
}

static ssize_t vlan_store(struct device *device, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val > 1)
		return -EINVAL;
	if (!dev->link[0].info->ns_num)
		return -EINVAL;
	ddbwritel(dev, 14 + (val ? 4 : 0), ETHER_LENGTH);
	dev->vlan = val;
	return count;
}

static ssize_t fmode_show(struct device *device,
			  struct device_attribute *attr, char *buf)
{
	int num = attr->attr.name[5] - 0x30;
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%u\n", dev->link[num].lnb.fmode);
}

static ssize_t devid_show(struct device *device,
			  struct device_attribute *attr, char *buf)
{
	int num = attr->attr.name[5] - 0x30;
	struct ddb *dev = dev_get_drvdata(device);

	return sprintf(buf, "%08x\n", dev->link[num].ids.devid);
}

static ssize_t fmode_store(struct device *device, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[5] - 0x30;
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val > 4)
		return -EINVAL;
	ddb_lnb_init_fmode(dev, &dev->link[num], val);
	return count;
}

static struct device_attribute ddb_attrs[] = {
	__ATTR_RO(version),
	__ATTR_RO(ports),
	__ATTR_RO(ts_irq),
	__ATTR_RO(i2c_irq),
	__ATTR(gap0, 0664, gap_show, gap_store),
	__ATTR(gap1, 0664, gap_show, gap_store),
	__ATTR(gap2, 0664, gap_show, gap_store),
	__ATTR(gap3, 0664, gap_show, gap_store),
	__ATTR(obr0, 0664, obr_show, obr_store),
	__ATTR(obr1, 0664, obr_show, obr_store),
	__ATTR(obr2, 0664, obr_show, obr_store),
	__ATTR(obr3, 0664, obr_show, obr_store),
	__ATTR(vlan, 0664, vlan_show, vlan_store),
	__ATTR(fmode0, 0664, fmode_show, fmode_store),
	__ATTR(fmode1, 0664, fmode_show, fmode_store),
	__ATTR(fmode2, 0664, fmode_show, fmode_store),
	__ATTR(fmode3, 0664, fmode_show, fmode_store),
	__ATTR_MRO(devid0, devid_show),
	__ATTR_MRO(devid1, devid_show),
	__ATTR_MRO(devid2, devid_show),
	__ATTR_MRO(devid3, devid_show),
	__ATTR_RO(hwid),
	__ATTR_RO(regmap),
	__ATTR(redirect, 0664, redirect_show, redirect_store),
	__ATTR_MRO(snr,  bsnr_show),
	__ATTR_RO(bpsnr),
	__ATTR_NULL,
};

static struct device_attribute ddb_attrs_temp[] = {
	__ATTR_MRO(temp, temp_show),
	__ATTR_MRO(templ1, temp_show),
	__ATTR_MRO(templ2, temp_show),
	__ATTR_MRO(templ3, temp_show),
};

static struct device_attribute ddb_attrs_mod[] = {
	__ATTR_MRO(mod0, mod_show),
	__ATTR_MRO(mod1, mod_show),
	__ATTR_MRO(mod2, mod_show),
	__ATTR_MRO(mod3, mod_show),
	__ATTR_MRO(mod4, mod_show),
	__ATTR_MRO(mod5, mod_show),
	__ATTR_MRO(mod6, mod_show),
	__ATTR_MRO(mod7, mod_show),
	__ATTR_MRO(mod8, mod_show),
	__ATTR_MRO(mod9, mod_show),
};

static struct device_attribute ddb_attrs_fan[] = {
	__ATTR(fan, 0664, fan_show, fan_store),
};

static struct device_attribute ddb_attrs_snr[] = {
	__ATTR(snr0, 0664, snr_show, snr_store),
	__ATTR(snr1, 0664, snr_show, snr_store),
	__ATTR(snr2, 0664, snr_show, snr_store),
	__ATTR(snr3, 0664, snr_show, snr_store),
};

static struct device_attribute ddb_attrs_gtl_snr[] = {
	__ATTR(gtlsnr1, 0664, gtl_snr_show, gtl_snr_store),
	__ATTR(gtlsnr2, 0664, gtl_snr_show, gtl_snr_store),
	__ATTR(gtlsnr3, 0664, gtl_snr_show, gtl_snr_store),
};

static struct device_attribute ddb_attrs_ctemp[] = {
	__ATTR_MRO(temp0, ctemp_show),
	__ATTR_MRO(temp1, ctemp_show),
	__ATTR_MRO(temp2, ctemp_show),
	__ATTR_MRO(temp3, ctemp_show),
};

static struct device_attribute ddb_attrs_led[] = {
	__ATTR(led0, 0664, led_show, led_store),
	__ATTR(led1, 0664, led_show, led_store),
	__ATTR(led2, 0664, led_show, led_store),
	__ATTR(led3, 0664, led_show, led_store),
};

static struct device_attribute ddb_attrs_fanspeed[] = {
	__ATTR_MRO(fanspeed0, fanspeed_show),
	__ATTR_MRO(fanspeed1, fanspeed_show),
	__ATTR_MRO(fanspeed2, fanspeed_show),
	__ATTR_MRO(fanspeed3, fanspeed_show),
};

static struct class ddb_class = {
	.name		= "ddbridge",
	.owner          = THIS_MODULE,
	.devnode        = ddb_devnode,
};

int ddb_class_create(void)
{
	ddb_major = register_chrdev(0, DDB_NAME, &ddb_fops);
	if (ddb_major < 0)
		return ddb_major;
	if (class_register(&ddb_class) < 0)
		return -1;
	return 0;
}

void ddb_class_destroy(void)
{
	class_unregister(&ddb_class);
	unregister_chrdev(ddb_major, DDB_NAME);
}

static void ddb_device_attrs_del(struct ddb *dev)
{
	int i;

	for (i = 0; i < 4; i++)
		if (dev->link[i].info &&
		    dev->link[i].info->tempmon_irq)
			device_remove_file(dev->ddb_dev,
					   &ddb_attrs_fanspeed[i]);
	for (i = 0; i < 4; i++)
		if (dev->link[i].info &&
		    dev->link[i].info->temp_num)
			device_remove_file(dev->ddb_dev, &ddb_attrs_temp[i]);
	for (i = 0; i < dev->link[0].info->port_num; i++)
		device_remove_file(dev->ddb_dev, &ddb_attrs_mod[i]);
	for (i = 0; i < dev->link[0].info->fan_num; i++)
		device_remove_file(dev->ddb_dev, &ddb_attrs_fan[i]);
	for (i = 0; i < dev->i2c_num && i < 4; i++) {
		if (dev->link[0].info->led_num)
			device_remove_file(dev->ddb_dev, &ddb_attrs_led[i]);
		device_remove_file(dev->ddb_dev, &ddb_attrs_snr[i]);
		device_remove_file(dev->ddb_dev, &ddb_attrs_ctemp[i]);
	}
	if (dev->link[0].info->regmap->gtl)
		for (i = 0; i < dev->link[0].info->regmap->gtl->num; i++)
			device_remove_file(dev->ddb_dev, &ddb_attrs_gtl_snr[i]);
	for (i = 0; ddb_attrs[i].attr.name; i++)
		device_remove_file(dev->ddb_dev, &ddb_attrs[i]);
}

static int ddb_device_attrs_add(struct ddb *dev)
{
	int i;

	for (i = 0; ddb_attrs[i].attr.name; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs[i]))
			goto fail;
	for (i = 0; i < 4; i++)
		if (dev->link[i].info &&
		    dev->link[i].info->temp_num)
			if (device_create_file(dev->ddb_dev, &ddb_attrs_temp[i]))
				goto fail;
	for (i = 0; (i < dev->link[0].info->port_num) && (i < 10); i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs_mod[i]))
			goto fail;
	for (i = 0; i < dev->link[0].info->fan_num; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs_fan[i]))
			goto fail;
	for (i = 0; (i < dev->i2c_num) && (i < 4); i++) {
		if (device_create_file(dev->ddb_dev, &ddb_attrs_snr[i]))
			goto fail;
		if (device_create_file(dev->ddb_dev, &ddb_attrs_ctemp[i]))
			goto fail;
		if (dev->link[0].info->led_num)
			if (device_create_file(dev->ddb_dev,
					       &ddb_attrs_led[i]))
				goto fail;
	}
	if (dev->link[0].info->regmap->gtl)
		for (i = 0; i < dev->link[0].info->regmap->gtl->num; i++)
			if (device_create_file(dev->ddb_dev, &ddb_attrs_gtl_snr[i]))
				goto fail;
	for (i = 0; i < 4; i++)
		if (dev->link[i].info &&
		    dev->link[i].info->tempmon_irq)
			if (device_create_file(dev->ddb_dev,
					       &ddb_attrs_fanspeed[i]))
				goto fail;
	return 0;
fail:
	return -1;
}

static int ddb_device_create(struct ddb *dev)
{
	int res = 0;

	if (ddb_num == DDB_MAX_ADAPTER)
		return -ENOMEM;
	mutex_lock(&ddb_mutex);
	dev->nr = ddb_num;
	atomic_set(&dev->ddb_dev_users, 1);
	ddbs[dev->nr] = dev;
	dev->ddb_dev = device_create(&ddb_class, dev->dev,
				     MKDEV(ddb_major, dev->nr),
				     dev, "ddbridge%d", dev->nr);
	if (IS_ERR(dev->ddb_dev)) {
		res = PTR_ERR(dev->ddb_dev);
		dev_info(dev->dev, "Could not create ddbridge%d\n", dev->nr);
		goto fail;
	}
	res = ddb_device_attrs_add(dev);
	if (res) {
		ddb_device_attrs_del(dev);
		device_destroy(&ddb_class, MKDEV(ddb_major, dev->nr));
		ddbs[dev->nr] = NULL;
		dev->ddb_dev = ERR_PTR(-ENODEV);
	} else {
		ddb_num++;
	}
fail:
	mutex_unlock(&ddb_mutex);
	return res;
}

void ddb_device_destroy(struct ddb *dev)
{
	if (IS_ERR(dev->ddb_dev))
		return;
	ddb_device_attrs_del(dev);
	device_destroy(&ddb_class, MKDEV(ddb_major, dev->nr));
}

#define LINK_IRQ_HANDLE(_l, _nr)				\
	do { if ((s & (1UL << (_nr))) && dev->link[_l].irq[_nr].handler) \
	  dev->link[_l].irq[_nr].handler(dev->link[_l].irq[_nr].data); } \
	while (0)

static void gtl_link_handler(void *priv)
{
	struct ddb *dev = (struct ddb *)priv;
	u32 regs = dev->link[0].info->regmap->gtl->base;

	dev_info(dev->dev, "GT link changed to %u\n",
		 (1 & ddbreadl(dev, regs)));
}

static void link_tasklet(unsigned long data)
{
	struct ddb_link *link = (struct ddb_link *)data;
	struct ddb *dev = link->dev;
	u32 s, tag = DDB_LINK_TAG(link->nr);
	u32 l = link->nr;

	s = ddbreadl(dev, tag | INTERRUPT_STATUS);
	dev_info(dev->dev, "gtl_irq %08x = %08x\n", tag | INTERRUPT_STATUS, s);

	if (!s)
		return;
	ddbwritel(dev, s, tag | INTERRUPT_ACK);
	LINK_IRQ_HANDLE(l, 0);
	LINK_IRQ_HANDLE(l, 1);
	LINK_IRQ_HANDLE(l, 2);
	LINK_IRQ_HANDLE(l, 3);
	LINK_IRQ_HANDLE(l, 24);
}

static void gtl_irq_handler(void *priv)
{
	struct ddb_link *link = (struct ddb_link *)priv;
#ifdef USE_LINK_TASKLET
	tasklet_schedule(&link->tasklet);
#else
	struct ddb *dev = link->dev;
	u32 s, l = link->nr, tag = DDB_LINK_TAG(link->nr);

	while ((s = ddbreadl(dev, tag | INTERRUPT_STATUS)))  {
		ddbwritel(dev, s, tag | INTERRUPT_ACK);
		LINK_IRQ_HANDLE(l, 0);
		LINK_IRQ_HANDLE(l, 1);
		LINK_IRQ_HANDLE(l, 2);
		LINK_IRQ_HANDLE(l, 3);
		LINK_IRQ_HANDLE(l, 24);
	}
#endif
}

static int ddb_gtl_init_link(struct ddb *dev, u32 l)
{
	struct ddb_link *link = &dev->link[l];
	u32 regs = dev->link[0].info->regmap->gtl->base +
		(l - 1) * dev->link[0].info->regmap->gtl->size;
	u32 id, subid, base = dev->link[0].info->regmap->irq_base_gtl;

	dev_info(dev->dev, "Checking GT link %u: regs = %08x\n", l, regs);

	spin_lock_init(&link->lock);
	mutex_init(&link->lnb.lock);
	link->lnb.fmode = 0xffffffff;
	mutex_init(&link->flash_mutex);

	link->nr = l;
	link->dev = dev;
	link->regs = regs;

	if (!(1 & ddbreadl(dev, regs))) {
		u32 c;

		for (c = 0; c < 5; c++) {
			ddbwritel(dev, 2, regs);
			msleep(20);
			ddbwritel(dev, 0, regs);
			msleep(200);
			if (1 & ddbreadl(dev, regs))
				break;
		}
		if (c == 5) {
			ddbwritel(dev, 8, regs);
			return -1;
		}
	}
	id = ddbreadl(dev, DDB_LINK_TAG(l) | 8);
	subid = ddbreadl(dev, DDB_LINK_TAG(l) | 12);
	link->info = get_ddb_info(id & 0xffff, id >> 16,
				  subid & 0xffff, subid >> 16);
	if (link->info->type != DDB_OCTOPUS_MAX_CT &&
	    link->info->type != DDB_OCTOPUS_MAX  &&
	    link->info->type != DDB_OCTOPUS_MCI) {
		dev_info(dev->dev,
			 "Detected GT link but found invalid ID %08x. You might have to update (flash) the add-on card first.",
			 id);
		return -1;
	}
	link->ids.devid = id;

	ddbwritel(dev, 1, regs + 0x20);
	ddb_irq_set(dev, 0, base + l, gtl_irq_handler, link);
	dev->link[l].ids.hwid = ddbreadl(dev, DDB_LINK_TAG(l) | 0);
	dev->link[l].ids.regmapid = ddbreadl(dev, DDB_LINK_TAG(l) | 4);
	dev->link[l].ids.vendor = id & 0xffff;
	dev->link[l].ids.device = id >> 16;
	dev->link[l].ids.subvendor = subid & 0xffff;
	dev->link[l].ids.subdevice = subid >> 16;

	dev_info(dev->dev, "GTL %s\n", dev->link[l].info->name);

	dev_info(dev->dev, "GTL HW %08x REGMAP %08x\n",
		 dev->link[l].ids.hwid,
		 dev->link[l].ids.regmapid);
	dev_info(dev->dev, "GTL ID %08x\n",
		 ddbreadl(dev, DDB_LINK_TAG(l) | 8));

	tasklet_init(&link->tasklet, link_tasklet, (unsigned long)link);
	ddbwritel(dev, 0xffffffff, DDB_LINK_TAG(l) | INTERRUPT_ACK);
	ddbwritel(dev, 0x0100000f, DDB_LINK_TAG(l) | INTERRUPT_ENABLE);

	return 0;
}

static int ddb_gtl_init(struct ddb *dev)
{
	u32 l, base = dev->link[0].info->regmap->irq_base_gtl;

	ddb_irq_set(dev, 0, base, gtl_link_handler, dev);
	for (l = 1; l < dev->link[0].info->regmap->gtl->num + 1; l++)
		ddb_gtl_init_link(dev, l);
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void tempmon_setfan(struct ddb_link *link)
{
	u32 temp, temp2, pwm;

	if ((ddblreadl(link, TEMPMON_CONTROL) &
	     TEMPMON_CONTROL_OVERTEMP) != 0) {
		dev_info(link->dev->dev, "Over temperature condition\n");
		link->over_temperature_error = 1;
	}
	temp  = (ddblreadl(link, TEMPMON_SENSOR0) >> 8) & 0xFF;
	if (temp & 0x80)
		temp = 0;
	temp2  = (ddblreadl(link, TEMPMON_SENSOR1) >> 8) & 0xFF;
	if (temp2 & 0x80)
		temp2 = 0;
	if (temp2 > temp)
		temp = temp2;

	pwm = (ddblreadl(link, TEMPMON_FANCONTROL) >> 8) & 0x0F;
	if (pwm > 10)
		pwm = 10;

	if (temp >= link->temp_tab[pwm]) {
		while (pwm < 10 && temp >= link->temp_tab[pwm + 1])
			pwm += 1;
	} else {
		while (pwm > 1 && temp < (link->temp_tab[pwm] - 2))
			pwm -= 1;
	}
	ddblwritel(link, (pwm << 8), TEMPMON_FANCONTROL);
}

static void temp_handler(void *data)
{
	struct ddb_link *link = (struct ddb_link *)data;

	spin_lock(&link->temp_lock);
	tempmon_setfan(link);
	spin_unlock(&link->temp_lock);
}

static int tempmon_init(struct ddb_link *link, int first_time)
{
	struct ddb *dev = link->dev;
	int status = 0;
	u32 l = link->nr;

	spin_lock_irq(&link->temp_lock);
	if (first_time) {
		static u8 temperature_table[11] = {
			30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};

		memcpy(link->temp_tab, temperature_table,
		       sizeof(temperature_table));
	}
	ddb_irq_set(dev, l, link->info->tempmon_irq, temp_handler, link);
	ddblwritel(link, (TEMPMON_CONTROL_OVERTEMP | TEMPMON_CONTROL_AUTOSCAN |
			  TEMPMON_CONTROL_INTENABLE),
		   TEMPMON_CONTROL);
	ddblwritel(link, (3 << 8), TEMPMON_FANCONTROL);

	link->over_temperature_error =
		((ddblreadl(link, TEMPMON_CONTROL) &
		  TEMPMON_CONTROL_OVERTEMP) != 0);
	if (link->over_temperature_error)	{
		dev_info(dev->dev, "Over temperature condition\n");
		status = -1;
	}
	tempmon_setfan(link);
	spin_unlock_irq(&link->temp_lock);
	return status;
}

static int ddb_init_tempmon(struct ddb_link *link)
{
	const struct ddb_info *info = link->info;

	if (!info->tempmon_irq)
		return 0;
	if (info->type == DDB_OCTOPUS_MAX ||
	    info->type == DDB_OCTOPUS_MAX_CT)
		if (link->ids.regmapid < 0x00010002)
			return 0;
	spin_lock_init(&link->temp_lock);
	return tempmon_init(link, 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int ddb_init_boards(struct ddb *dev)
{
	const struct ddb_info *info;
	struct ddb_link *link;
	u32 l;

	for (l = 0; l < DDB_MAX_LINK; l++) {
		link = &dev->link[l];
		info = link->info;
		if (!info)
			continue;
		dev_info(dev->dev,
			 "link %u vendor %04x device %04x subvendor %04x subdevice %04x\n",
			 l,
			 dev->link[l].ids.vendor,
			 dev->link[l].ids.device,
			 dev->link[l].ids.subvendor,
			 dev->link[l].ids.subdevice);

		if (info->board_control) {
			ddbwritel(dev, 0, DDB_LINK_TAG(l) | BOARD_CONTROL);
			msleep(100);
			ddbwritel(dev, info->board_control_2,
				  DDB_LINK_TAG(l) | BOARD_CONTROL);
			usleep_range(2000, 3000);
			ddbwritel(dev, info->board_control_2 |
				  info->board_control,
				  DDB_LINK_TAG(l) | BOARD_CONTROL);
			usleep_range(2000, 3000);
		}
		ddb_init_tempmon(link);

		if (info->regmap->mci) {
			if (link->info->type == DDB_OCTOPUS_MCI ||
			    ((link->info->type == DDB_MOD) &&
			     (link->ids.regmapid & 0xfff0)) ||
			    ((link->info->type == DDB_MOD) &&
			     (link->ids.revision == 1)))
				mci_init(link);
		}
	}
	return 0;
}

int ddb_init(struct ddb *dev)
{
	mutex_init(&dev->link[0].flash_mutex);
	mutex_init(&dev->ioctl_mutex);
	if (no_init) {
		ddb_device_create(dev);
		return 0;
	}
	if (dev->link[0].info->ns_num) {
		ddbwritel(dev, 1, ETHER_CONTROL);
		dev->vlan = vlan;
		ddbwritel(dev, 14 + (dev->vlan ? 4 : 0), ETHER_LENGTH);
	}
	mutex_init(&dev->link[0].lnb.lock);

	if (dev->link[0].info->regmap->gtl)
		ddb_gtl_init(dev);

	ddb_init_boards(dev);
	if (ddb_i2c_init(dev) < 0)
		goto fail;
	ddb_ports_init(dev);
	if (dev->link[0].info->type == DDB_MOD)
		ddbridge_mod_init(dev);
	if (ddb_buffers_alloc(dev) < 0) {
		dev_info(dev->dev,
			 "Could not allocate buffer memory\n");
		goto fail2;
	}
	if (ddb_ports_attach(dev) < 0)
		goto fail3;
	ddb_nsd_attach(dev);

	ddb_device_create(dev);

	if (dev->link[0].info->fan_num)	{
		ddbwritel(dev, 1, GPIO_DIRECTION);
		ddbwritel(dev, 1, GPIO_OUTPUT);
	}
	return 0;

fail3:
	ddb_ports_detach(dev);
	dev_err(dev->dev, "fail3\n");
	ddb_ports_release(dev);
fail2:
	dev_err(dev->dev, "fail2\n");
	ddb_buffers_free(dev);
	ddb_i2c_release(dev);
fail:
	dev_err(dev->dev, "fail1\n");
	return -1;
}

static void ddb_reset_io(struct ddb *dev, u32 reg)
{
	ddbwritel(dev, 0x00, reg);
	ddbwritel(dev, 0x02, reg);
	ddbwritel(dev, 0x00, reg);
}

void ddb_reset_ios(struct ddb *dev)
{
	u32 i;
	const struct ddb_regmap *rm = dev->link[0].info->regmap;

	if (rm->input)
		for (i = 0; i < rm->input->num; i++)
			ddb_reset_io(dev,
				     rm->input->base + i * rm->input->size);
	if (rm->output)
		for (i = 0; i < rm->output->num; i++)
			ddb_reset_io(dev,
				     rm->output->base + i * rm->output->size);
	usleep_range(5000, 6000);
}

void ddb_unmap(struct ddb *dev)
{
	if (dev->regs)
		iounmap(dev->regs);
	vfree(dev);
}

int ddb_exit_ddbridge(int stage, int error)
{
	switch (stage) {
	default:
	case 2:
		destroy_workqueue(ddb_wq);
		fallthrough;
	case 1:
		ddb_class_destroy();
	}
	return error;
}

int ddb_init_ddbridge(void)
{
	if (dma_buf_num < 8)
		dma_buf_num = 8;
	if (dma_buf_num > 32)
		dma_buf_num = 32;
	if (dma_buf_size < 1)
		dma_buf_size = 1;
	if (dma_buf_size > 43)
		dma_buf_size = 43;

	if (ddb_class_create() < 0)
		return -1;
	ddb_wq = alloc_workqueue("ddbridge", 0, 0);
	if (!ddb_wq)
		return ddb_exit_ddbridge(1, -1);
	return 0;
}
