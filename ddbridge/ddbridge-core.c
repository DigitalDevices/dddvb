/*
 * ddbridge-core.c: Digital Devices bridge core functions
 *
 * Copyright (C) 2010-2015 Digital Devices GmbH
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

DEFINE_MUTEX(redirect_lock);

static int ci_bitrate = 72000;
module_param(ci_bitrate, int, 0444);
MODULE_PARM_DESC(ci_bitrate, " Bitrate for output to CI.");

static int ts_loop = -1;
module_param(ts_loop, int, 0444);
MODULE_PARM_DESC(ts_loop, "TS in/out test loop on port ts_loop");

static int vlan;
module_param(vlan, int, 0444);
MODULE_PARM_DESC(vlan, "VLAN and QoS IDs enabled");

static int tt;
module_param(tt, int, 0444);
MODULE_PARM_DESC(tt, "");

static int fmode;
module_param(fmode, int, 0444);
MODULE_PARM_DESC(fmode, "frontend emulation mode");

static int old_quattro;
module_param(old_quattro, int, 0444);
MODULE_PARM_DESC(old_quattro, "old quattro LNB input order ");

#define DDB_MAX_ADAPTER 64
static struct ddb *ddbs[DDB_MAX_ADAPTER];

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#include "ddbridge-mod.c"
#include "ddbridge-i2c.c"
#include "ddbridge-ns.c"


static void ddb_set_dma_table(struct ddb *dev, struct ddb_dma *dma)
{
	u32 i, base;
	u64 mem;

	if (!dma)
		return;
	base = DMA_BASE_ADDRESS_TABLE + dma->nr * 0x100;
	for (i = 0; i < dma->num; i++) {
		mem = dma->pbuf[i];
		ddbwritel(dev, mem & 0xffffffff, base + i * 8);
		ddbwritel(dev, mem >> 32, base + i * 8 + 4);
	}
	dma->bufreg = (dma->div << 16) |
		((dma->num & 0x1f) << 11) |
		((dma->size >> 7) & 0x7ff);
}

static void ddb_set_dma_tables(struct ddb *dev)
{
	u32 i;

	for (i = 0; i < dev->link[0].info->port_num * 2; i++)
		ddb_set_dma_table(dev, dev->input[i].dma);
	for (i = 0; i < dev->link[0].info->port_num; i++)
		ddb_set_dma_table(dev, dev->output[i].dma);
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

	sdma->bufreg = ddma->bufreg;
	base = DMA_BASE_ADDRESS_TABLE + sdma->nr * 0x100;
	for (i = 0; i < ddma->num; i++) {
		mem = ddma->pbuf[i];
		ddbwritel(dev, mem & 0xffffffff, base + i * 8);
		ddbwritel(dev, mem >> 32, base + i * 8 + 4);
	}
}

static int ddb_unredirect(struct ddb_port *port)
{
	struct ddb_input *oredi, *iredi = 0;
	struct ddb_output *iredo = 0;

	/*pr_info("unredirect %d.%d\n", port->dev->nr, port->nr);*/
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
			port->input[0]->redo = 0;
			ddb_set_dma_table(port->dev, port->input[0]->dma);
		}
		oredi->redi = iredi;
		port->input[0]->redi = 0;
	}
	oredi->redo = 0;
	port->output->redi = 0;

	ddb_set_dma_table(oredi->port->dev, oredi->dma);
done:
	mutex_unlock(&redirect_lock);
	return 0;
}

static int ddb_redirect(u32 i, u32 p)
{
	struct ddb *idev = ddbs[(i >> 4) & 0x1f];
	struct ddb_input *input, *input2;
	struct ddb *pdev = ddbs[(p >> 4) & 0x1f];
	struct ddb_port *port;

	if (!idev->has_dma || !pdev->has_dma)
		return -EINVAL;
	if (!idev || !pdev)
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
			input->redi = 0;
		} else
			input2->redi = input;
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

#ifdef DDB_ALT_DMA
static void dma_free(struct pci_dev *pdev, struct ddb_dma *dma, int dir)
{
	int i;

	if (!dma)
		return;
	for (i = 0; i < dma->num; i++) {
		if (dma->vbuf[i]) {
			dma_unmap_single(&pdev->dev, dma->pbuf[i],
					 dma->size,
					 dir ? DMA_TO_DEVICE :
					 DMA_FROM_DEVICE);
			kfree(dma->vbuf[i]);
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
		dma->vbuf[i] = kmalloc(dma->size, __GFP_REPEAT);
		if (!dma->vbuf[i])
			return -ENOMEM;
		dma->pbuf[i] = dma_map_single(&pdev->dev, dma->vbuf[i],
					      dma->size,
					      dir ? DMA_TO_DEVICE :
					      DMA_FROM_DEVICE);
		if (dma_mapping_error(&pdev->dev, dma->pbuf[i])) {
			kfree(dma->vbuf[i]);
			return -ENOMEM;
		}
	}
	return 0;
}
#else

static void dma_free(struct pci_dev *pdev, struct ddb_dma *dma, int dir)
{
	int i;

	if (!dma)
		return;
	for (i = 0; i < dma->num; i++) {
		if (dma->vbuf[i]) {
#if 0
			pci_free_consistent(pdev, dma->size,
					    dma->vbuf[i], dma->pbuf[i]);
#else
			dma_free_coherent(&pdev->dev, dma->size,
					    dma->vbuf[i], dma->pbuf[i]);
#endif			
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
#if 0
		dma->vbuf[i] = pci_alloc_consistent(pdev, dma->size,
						    &dma->pbuf[i]);
#else
		dma->vbuf[i] = dma_alloc_coherent(&pdev->dev, dma->size,
						  &dma->pbuf[i], GFP_KERNEL);
#endif
		if (!dma->vbuf[i])
			return -ENOMEM;
	}
	return 0;
}
#endif

static int ddb_buffers_alloc(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->link[0].info->port_num; i++) {
		port = &dev->port[i];
		switch (port->class) {
		case DDB_PORT_TUNER:
			if (port->input[0]->dma)
				if (dma_alloc(dev->pdev, port->input[0]->dma, 0) < 0)
					return -1;
			if (port->input[1]->dma)
				if (dma_alloc(dev->pdev, port->input[1]->dma, 0) < 0)
					return -1;
			break;
		case DDB_PORT_CI:
		case DDB_PORT_LOOP:
			if (port->input[0]->dma)
				if (dma_alloc(dev->pdev, port->input[0]->dma, 0) < 0)
					return -1;
		case DDB_PORT_MOD:
			if (port->output->dma)
				if (dma_alloc(dev->pdev, port->output->dma, 1) < 0)
					return -1;
			break;
		default:
			break;
		}
	}
	ddb_set_dma_tables(dev);
	return 0;
}

static void ddb_buffers_free(struct ddb *dev)
{
	int i;
	struct ddb_port *port;

	for (i = 0; i < dev->link[0].info->port_num; i++) {
		port = &dev->port[i];

		if (port->input[0] && port->input[0]->dma)
			dma_free(dev->pdev, port->input[0]->dma, 0);
		if (port->input[1] && port->input[1]->dma)
			dma_free(dev->pdev, port->input[1]->dma, 0);
		if (port->output && port->output->dma)
			dma_free(dev->pdev, port->output->dma, 1);
	}
}

static void ddb_output_start(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;
	u32 con2;

	con2 = ((output->port->obr << 13) + 71999) / 72000;
	con2 = (con2 << 16) | output->port->gap;

	if (output->dma) {
		spin_lock_irq(&output->dma->lock);
		output->dma->cbuf = 0;
		output->dma->coff = 0;
		output->dma->stat = 0;
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(output->dma->nr));
	}
	if (output->port->class == DDB_PORT_MOD)
		ddbridge_mod_output_start(output);
	else {
		ddbwritel(dev, 0, TS_OUTPUT_CONTROL(output->nr));
		ddbwritel(dev, 2, TS_OUTPUT_CONTROL(output->nr));
		ddbwritel(dev, 0, TS_OUTPUT_CONTROL(output->nr));
		ddbwritel(dev, 0x3c, TS_OUTPUT_CONTROL(output->nr));
		ddbwritel(dev, con2, TS_OUTPUT_CONTROL2(output->nr));
	}
	if (output->dma) {
		ddbwritel(dev, output->dma->bufreg,
			  DMA_BUFFER_SIZE(output->dma->nr));
		ddbwritel(dev, 0, DMA_BUFFER_ACK(output->dma->nr));
		ddbwritel(dev, 1, DMA_BASE_READ);
		ddbwritel(dev, 3, DMA_BUFFER_CONTROL(output->dma->nr));
	}
	if (output->port->class != DDB_PORT_MOD) {
		if (output->port->input[0]->port->class == DDB_PORT_LOOP)
			/*ddbwritel(dev, 0x15, TS_OUTPUT_CONTROL(output->nr));
			  ddbwritel(dev, 0x45,
			  TS_OUTPUT_CONTROL(output->nr));*/
			ddbwritel(dev, (1 << 13) | 0x15,
				  TS_OUTPUT_CONTROL(output->nr));
		else
			ddbwritel(dev, 0x11d, TS_OUTPUT_CONTROL(output->nr));
	}
	if (output->dma) {
		output->dma->running = 1;
		spin_unlock_irq(&output->dma->lock);
	}
}

static void ddb_output_stop(struct ddb_output *output)
{
	struct ddb *dev = output->port->dev;

	if (output->dma)
		spin_lock_irq(&output->dma->lock);
	if (output->port->class == DDB_PORT_MOD)
		ddbridge_mod_output_stop(output);
	else
		ddbwritel(dev, 0, TS_OUTPUT_CONTROL(output->nr));
	if (output->dma) {
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(output->dma->nr));
		output->dma->running = 0;
		spin_unlock_irq(&output->dma->lock);
	}
}

static void ddb_input_stop(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	u32 tag = DDB_LINK_TAG(input->port->lnr);

	if (input->dma)
		spin_lock_irq(&input->dma->lock);
	ddbwritel(dev, 0, tag | TS_INPUT_CONTROL(input->nr));
	if (input->dma) {
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(input->dma->nr));
		input->dma->running = 0;
		spin_unlock_irq(&input->dma->lock);
	}
	//printk("input_stop %u.%u.%u\n", dev->nr, input->port->lnr, input->nr);
}

static void ddb_input_start(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	/* u32 tsbase = TS_INPUT_BASE + input->nr * 0x10; */
	u32 tag = DDB_LINK_TAG(input->port->lnr);

	if (input->dma) {
		spin_lock_irq(&input->dma->lock);
		input->dma->cbuf = 0;
		input->dma->coff = 0;
		input->dma->stat = 0;
		ddbwritel(dev, 0, DMA_BUFFER_CONTROL(input->dma->nr));
	}
	ddbwritel(dev, 0, tag | TS_INPUT_CONTROL2(input->nr));
	ddbwritel(dev, 0, tag | TS_INPUT_CONTROL(input->nr));
	ddbwritel(dev, 2, tag | TS_INPUT_CONTROL(input->nr));
	ddbwritel(dev, 0, tag | TS_INPUT_CONTROL(input->nr));

	if (input->dma) {
		ddbwritel(dev, input->dma->bufreg,
			  DMA_BUFFER_SIZE(input->dma->nr));
		ddbwritel(dev, 0, DMA_BUFFER_ACK(input->dma->nr));
		ddbwritel(dev, 1, DMA_BASE_WRITE);
		ddbwritel(dev, 3, DMA_BUFFER_CONTROL(input->dma->nr));
	}
	if (dev->link[0].info->type == DDB_OCTONET)
		ddbwritel(dev, 0x01, tag | TS_INPUT_CONTROL(input->nr));
	else
		ddbwritel(dev, 0x09, tag | TS_INPUT_CONTROL(input->nr));
	if (input->dma) {
		input->dma->running = 1;
		spin_unlock_irq(&input->dma->lock);
	}
	//printk("input_start %u.%u.%u\n", dev->nr, input->port->lnr, input->nr); 
}


static int ddb_dvb_ns_input_start(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (!dvb->users)
		ddb_input_start(input);

	return ++dvb->users;
}

static int ddb_dvb_ns_input_stop(struct ddb_input *input)
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
		    (output->dma->size - output->dma->coff <= 188))
			return 0;
		return 188;
	}
	diff = off - output->dma->coff;
	if (diff <= 0 || diff > 188)
		return 188;
	return 0;
}

#if 0
static u32 ddb_dma_free(struct ddb_dma *dma)
{
	u32 idx, off, stat = dma->stat;
	s32 p1, p2, diff;

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	p1 = idx * dma->size + off;
	p2 = dma->cbuf * dma->size + dma->coff;

	diff = p1 - p2;
	if (diff <= 0)
		diff += dma->num * dma->size;
	return diff;
}
#endif

static ssize_t ddb_output_write(struct ddb_output *output,
				const u8 *buf, size_t count)
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
#ifdef DDB_ALT_DMA
		dma_sync_single_for_device(dev->dev,
					   output->dma->pbuf[
						   output->dma->cbuf],
					   output->dma->size, DMA_TO_DEVICE);
#endif
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
			  DMA_BUFFER_ACK(output->dma->nr));
	}
	return count - left;
}

#if 0
static u32 ddb_input_free_bytes(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	u32 idx, off, stat = input->dma->stat;
	u32 ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(input->dma->nr));

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	if (ctrl & 4)
		return 0;
	if (input->dma->cbuf != idx)
		return 1;
	return 0;
}



static s32 ddb_output_used_bufs(struct ddb_output *output)
{
	u32 idx, off, stat, ctrl;
	s32 diff;

	spin_lock_irq(&output->dma->lock);
	stat = output->dma->stat;
	ctrl = output->dma->ctrl;
	spin_unlock_irq(&output->dma->lock);

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	if (ctrl & 4)
		return 0;
	diff = output->dma->cbuf - idx;
	if (diff == 0 && off < output->dma->coff)
		return 0;
	if (diff <= 0)
		diff += output->dma->num;
	return diff;
}

static s32 ddb_input_free_bufs(struct ddb_input *input)
{
	u32 idx, off, stat, ctrl;
	s32 free;

	spin_lock_irq(&input->dma->lock);
	ctrl = input->dma->ctrl;
	stat = input->dma->stat;
	spin_unlock_irq(&input->dma->lock);
	if (ctrl & 4)
		return 0;
	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;
	free = input->dma->cbuf - idx;
	if (free == 0 && off < input->dma->coff)
		return 0;
	if (free <= 0)
		free += input->dma->num;
	return free - 1;
}

static u32 ddb_output_ok(struct ddb_output *output)
{
	struct ddb_input *input = output->port->input[0];
	s32 diff;

	diff = ddb_input_free_bufs(input) - ddb_output_used_bufs(output);
	if (diff > 0)
		return 1;
	return 0;
}
#endif

static u32 ddb_input_avail(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	u32 idx, off, stat = input->dma->stat;
	u32 ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(input->dma->nr));

	idx = (stat >> 11) & 0x1f;
	off = (stat & 0x7ff) << 7;

	if (ctrl & 4) {
		pr_err("IA %d %d %08x\n", idx, off, ctrl);
		ddbwritel(dev, stat, DMA_BUFFER_ACK(input->dma->nr));
		return 0;
	}
	if (input->dma->cbuf != idx)
		return 188;
	return 0;
}

static size_t ddb_input_read(struct ddb_input *input, u8 *buf, size_t count)
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
#ifdef DDB_ALT_DMA
		dma_sync_single_for_cpu(dev->dev,
					input->dma->pbuf[input->dma->cbuf],
					input->dma->size, DMA_FROM_DEVICE);
#endif
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
		ddbwritel(dev,
			  (input->dma->cbuf << 11) | (input->dma->coff >> 7),
			  DMA_BUFFER_ACK(input->dma->nr));
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

static ssize_t ts_read(struct file *file, char *buf,
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
	} else
		return -EINVAL;
	err = dvb_generic_open(inode, file);
	if (err < 0)
		return err;
	if ((file->f_flags & O_ACCMODE) == O_RDONLY)
		ddb_input_start(input);
	else if ((file->f_flags & O_ACCMODE) == O_WRONLY)
		ddb_output_start(output);
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
	.mmap    = 0,
};

static struct dvb_device dvbdev_ci = {
	.priv    = 0,
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
	return dvb_usercopy(file, cmd, arg, ddbridge_mod_do_ioctl);
}

static const struct file_operations mod_fops = {
	.owner   = THIS_MODULE,
	.read    = ts_read,
	.write   = ts_write,
	.open    = mod_open,
	.release = mod_release,
	.poll    = ts_poll,
	.mmap    = 0,
	.unlocked_ioctl = mod_ioctl,
};

static struct dvb_device dvbdev_mod = {
	.priv    = 0,
	.readers = 1,
	.writers = 1,
	.users   = 2,
	.fops    = &mod_fops,
};


#if 0
static struct ddb_input *fe2input(struct ddb *dev, struct dvb_frontend *fe)
{
	int i;

	for (i = 0; i < dev->link[0].info->port_num * 2; i++) {
		if (dev->input[i].fe == fe)
			return &dev->input[i];
	}
	return NULL;
}
#endif

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

#ifdef CONFIG_DVB_DRXK
static int demod_attach_drxk(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;

	fe = dvb->fe = dvb_attach(drxk_attach,
				  i2c, 0x29 + (input->nr & 1),
				  &dvb->fe2);
	if (!fe) {
		pr_err("No DRXK found!\n");
		return -ENODEV;
	}
	fe->sec_priv = input;
	dvb->i2c_gate_ctrl = fe->ops.i2c_gate_ctrl;
	fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
	return 0;
}
#endif

struct cxd2843_cfg cxd2843_0 = {
	.adr = 0x6c,
	.ts_clock = 1,
};

struct cxd2843_cfg cxd2843_1 = {
	.adr = 0x6d,
	.ts_clock = 1,
};

struct cxd2843_cfg cxd2843p_0 = {
	.adr = 0x6c,
	.parallel = 1,
};

struct cxd2843_cfg cxd2843p_1 = {
	.adr = 0x6d,
	.parallel = 1,
};

static int demod_attach_cxd2843(struct ddb_input *input, int par)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;

	if (par)
		fe = dvb->fe = dvb_attach(cxd2843_attach, i2c,
					  (input->nr & 1) ?
					  &cxd2843p_1 : &cxd2843p_0);
	else
		fe = dvb->fe = dvb_attach(cxd2843_attach, i2c,
					  (input->nr & 1) ?
					  &cxd2843_1 : &cxd2843_0);
	if (!dvb->fe) {
		pr_err("No cxd2837/38/43 found!\n");
		return -ENODEV;
	}
	fe->sec_priv = input;
	dvb->i2c_gate_ctrl = fe->ops.i2c_gate_ctrl;
	fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
	return 0;
}

static int demod_attach_stv0367dd(struct ddb_input *input)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct dvb_frontend *fe;
	struct stv0367_cfg cfg = { .cont_clock = 0 };
	
	cfg.adr = 0x1f - (input->nr & 1);
	if (input->port->dev->link[input->port->lnr].info->con_clock)
		cfg.cont_clock = 1;
	fe = dvb->fe = dvb_attach(stv0367_attach, i2c,
				  &cfg,
				  &dvb->fe2);
	if (!dvb->fe) {
		pr_err("No stv0367 found!\n");
		return -ENODEV;
	}
	fe->sec_priv = input;
	dvb->i2c_gate_ctrl = fe->ops.i2c_gate_ctrl;
	fe->ops.i2c_gate_ctrl = locked_gate_ctrl;
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
		pr_err("No TDA18271 found!\n");
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
		pr_err("No TDA18212 found!\n");
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
		pr_err("No TDA18212 found!\n");
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
		pr_err("No STV0900 found!\n");
		return -ENODEV;
	}
	if (!dvb_attach(lnbh24_attach, dvb->fe, i2c, 0,
			0, (input->nr & 1) ?
			(0x09 - type) : (0x0b - type))) {
		pr_err("No LNBH24 found!\n");
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
	struct stv6110x_devctl *ctl;

	ctl = dvb_attach(stv6110x_attach, dvb->fe, tunerconf, i2c);
	if (!ctl) {
		pr_err("No STV6110X found!\n");
		return -ENODEV;
	}
	pr_info("attach tuner input %d adr %02x\n",
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
};

static int demod_attach_stv0910(struct ddb_input *input, int type)
{
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct stv0910_cfg cfg = stv0910_p;

	if (type)
		cfg.parallel = 2;
	dvb->fe = dvb_attach(stv0910_attach, i2c, &cfg, (input->nr & 1));
	if (!dvb->fe) {
		cfg.adr = 0x6c;
		dvb->fe = dvb_attach(stv0910_attach, i2c,
				     &cfg, (input->nr & 1));
	}
	if (!dvb->fe) {
		pr_err("No STV0910 found!\n");
		return -ENODEV;
	}
	if (!dvb_attach(lnbh25_attach, dvb->fe, i2c,
			((input->nr & 1) ? 0x0d : 0x0c))) {
		if (!dvb_attach(lnbh25_attach, dvb->fe, i2c,
				((input->nr & 1) ? 0x09 : 0x08))) {
			pr_err("No LNBH25 found!\n");
			return -ENODEV;
		}
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
			pr_err("No STV6111 found at 0x%02x!\n", adr);
			return -ENODEV;
		}
	}
	return 0;
}

static int lnb_command(struct ddb *dev, u32 link, u32 lnb, u32 cmd)
{
	u32 c, v = 0, tag = DDB_LINK_TAG(link);

	v = LNB_TONE & (dev->link[link].lnb.tone << (15 - lnb));
	//pr_info("lnb_control[%u] = %08x\n", lnb, cmd | v);
	ddbwritel(dev, cmd | v, tag | LNB_CONTROL(lnb));
	for (c = 0; c < 10; c++) {
		v = ddbreadl(dev, tag | LNB_CONTROL(lnb));
		//pr_info("ctrl = %08x\n", v);
		if ((v & LNB_BUSY) == 0)
			break;
		msleep(20);
	}
	if (c == 10)
		pr_info("lnb_command lnb = %08x  cmd = %08x\n", lnb, cmd);
	return 0;
}

static int max_send_master_cmd(struct dvb_frontend *fe,
			       struct dvb_diseqc_master_cmd *cmd)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb *dev = port->dev;
	struct ddb_dvb *dvb = &port->dvb[input->nr & 1];
	u32 tag = DDB_LINK_TAG(port->lnr);
	int i;
	u32 fmode = dev->link[port->lnr].lnb.fmode;
		
	if (fmode == 2 || fmode == 1)
		return 0;
	if (dvb->diseqc_send_master_cmd)
		dvb->diseqc_send_master_cmd(fe, cmd);

	mutex_lock(&dev->link[port->lnr].lnb.lock);
	ddbwritel(dev, 0, tag | LNB_BUF_LEVEL(dvb->input));
	for (i = 0; i < cmd->msg_len; i++)
		ddbwritel(dev, cmd->msg[i], tag | LNB_BUF_WRITE(dvb->input));
	lnb_command(dev, port->lnr, dvb->input, LNB_CMD_DISEQC);
	mutex_unlock(&dev->link[port->lnr].lnb.lock);
	return 0;
}

static int lnb_set_tone(struct ddb *dev, u32 link, u32 input, fe_sec_tone_mode_t tone)
{
	int s = 0;
        u32 mask = (1ULL << input);
	
	switch (tone) {
	case SEC_TONE_OFF:
		if (!(dev->link[link].lnb.tone & mask))
			return 0;
		dev->link[link].lnb.tone &= ~(1ULL << input);
		break;
	case SEC_TONE_ON:
		if (dev->link[link].lnb.tone & mask)
			return 0;
		dev->link[link].lnb.tone |= (1ULL << input);
		break;
	default:
		s = -EINVAL;
		break;
	};
	if (!s)
		s = lnb_command(dev, link, input, LNB_CMD_NOP);
	return s;
}

static int lnb_set_voltage(struct ddb *dev, u32 link, u32 input, fe_sec_voltage_t voltage)
{
	int s = 0;

	if (dev->link[link].lnb.oldvoltage[input] == voltage)
		return 0;
	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		if (dev->link[link].lnb.voltage[input])
			return 0;
		lnb_command(dev, link, input, LNB_CMD_OFF);
		break;
	case SEC_VOLTAGE_13:
		lnb_command(dev, link, input, LNB_CMD_LOW);
		break;
	case SEC_VOLTAGE_18:
		lnb_command(dev, link, input, LNB_CMD_HIGH);
		break;
	default:
		s = -EINVAL;
		break;
	};
	dev->link[link].lnb.oldvoltage[input] = voltage;
	return s;
}

static int max_set_input_unlocked(struct dvb_frontend *fe, int in)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb *dev = port->dev;
	struct ddb_dvb *dvb = &port->dvb[input->nr & 1];
	int res = 0;
	
	if (in > 3)
		return -EINVAL;
	if (dvb->input != in) {
		u32 bit = (1ULL << input->nr);
		u32 obit = dev->link[port->lnr].lnb.voltage[dvb->input] & bit;
		
		dev->link[port->lnr].lnb.voltage[dvb->input] &= ~bit;
		dvb->input = in;
		dev->link[port->lnr].lnb.voltage[dvb->input] |= obit;
	}
	res = dvb->set_input(fe, in);
	return res;
}

static int max_set_input(struct dvb_frontend *fe, int in)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb *dev = input->port->dev;
	int res;

	mutex_lock(&dev->link[port->lnr].lnb.lock);	
	res = max_set_input_unlocked(fe, in);
	mutex_unlock(&dev->link[port->lnr].lnb.lock);
	return res;
}

static int max_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb *dev = port->dev;
	struct ddb_dvb *dvb = &port->dvb[input->nr & 1];
	int tuner = 0;
	int res = 0;
	u32 fmode = dev->link[port->lnr].lnb.fmode;
			
	mutex_lock(&dev->link[port->lnr].lnb.lock);	
	dvb->tone = tone;
	switch (fmode) {
	default:
	case 0:
	case 3:
		res = lnb_set_tone(dev, port->lnr, dvb->input, tone);
		break;
	case 1:
	case 2:
		if (old_quattro) {
			if (dvb->tone == SEC_TONE_ON)
				tuner |= 2;
			if (dvb->voltage == SEC_VOLTAGE_18)
				tuner |= 1;
		} else {
			if (dvb->tone == SEC_TONE_ON)
				tuner |= 1;
			if (dvb->voltage == SEC_VOLTAGE_18)
				tuner |= 2;
		}
		res = max_set_input_unlocked(fe, tuner);
		break;
	}
	mutex_unlock(&dev->link[port->lnr].lnb.lock);
	return res;
}

static int max_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage)
{
	struct ddb_input *input = fe->sec_priv;
	struct ddb_port *port = input->port;
	struct ddb *dev = port->dev;
	struct ddb_dvb *dvb = &port->dvb[input->nr & 1];
	int tuner = 0;
	u32 nv, ov = dev->link[port->lnr].lnb.voltages;
	int res = 0;
	u32 fmode = dev->link[port->lnr].lnb.fmode;
	
	mutex_lock(&dev->link[port->lnr].lnb.lock);
	dvb->voltage = voltage;
	
	switch (fmode) {
	case 3:
	default:
	case 0:
		if (fmode == 3)
			max_set_input_unlocked(fe, 0);
		if (voltage == SEC_VOLTAGE_OFF) 
			dev->link[port->lnr].lnb.voltage[dvb->input] &= ~(1ULL << input->nr);
		else 
			dev->link[port->lnr].lnb.voltage[dvb->input] |= (1ULL << input->nr);
		
		res = lnb_set_voltage(dev, port->lnr, dvb->input, voltage);
		break;
	case 1:
	case 2:
		if (voltage == SEC_VOLTAGE_OFF) 
			dev->link[port->lnr].lnb.voltages &= ~(1ULL << input->nr);
		else
			dev->link[port->lnr].lnb.voltages |= (1ULL << input->nr);
		nv = dev->link[port->lnr].lnb.voltages;
		
		if (old_quattro) {
			if (dvb->tone == SEC_TONE_ON)
				tuner |= 2;
			if (dvb->voltage == SEC_VOLTAGE_18)
				tuner |= 1;
		} else {
			if (dvb->tone == SEC_TONE_ON)
				tuner |= 1;
			if (dvb->voltage == SEC_VOLTAGE_18)
				tuner |= 2;
		}
		res = max_set_input_unlocked(fe, tuner);
		
		if (nv != ov) {
			if (nv) {
				lnb_set_voltage(dev, port->lnr, 0, SEC_VOLTAGE_13);
				if (fmode == 1) {
					lnb_set_voltage(dev, port->lnr, 0, SEC_VOLTAGE_13);
					if (old_quattro) {
						lnb_set_voltage(dev, port->lnr, 1, SEC_VOLTAGE_18);
						lnb_set_voltage(dev, port->lnr, 2, SEC_VOLTAGE_13);
					} else {
						lnb_set_voltage(dev, port->lnr, 1, SEC_VOLTAGE_13);
						lnb_set_voltage(dev, port->lnr, 2, SEC_VOLTAGE_18);
					}
					lnb_set_voltage(dev, port->lnr, 3, SEC_VOLTAGE_18);
				}
			} else {
				lnb_set_voltage(dev, port->lnr, 0, SEC_VOLTAGE_OFF);
				if (fmode == 1) {
					lnb_set_voltage(dev, port->lnr, 1, SEC_VOLTAGE_OFF);
					lnb_set_voltage(dev, port->lnr, 2, SEC_VOLTAGE_OFF);
					lnb_set_voltage(dev, port->lnr, 3, SEC_VOLTAGE_OFF);
				}
			}
		}
		break;
	}
	mutex_unlock(&dev->link[port->lnr].lnb.lock);
	return res;
}

static int max_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{

	return 0;
}

static int max_send_burst(struct dvb_frontend *fe, fe_sec_mini_cmd_t burst)
{
	return 0;
}

static int mxl_fw_read(void *priv, u8 *buf, u32 len)
{
	struct ddb_link *link = priv;
	struct ddb *dev = link->dev;

	pr_info("Read mxl_fw from link %u\n", link->nr);
	
	return ddbridge_flashread(dev, link->nr, buf, 0xc0000, len);
}

static int lnb_init_fmode(struct ddb *dev, struct ddb_link *link, u32 fm)
{
	u32 l = link->nr;

	if (link->lnb.fmode == fm)
		return 0;
	pr_info("Set fmode link %u = %u\n", l, fm);
	mutex_lock(&link->lnb.lock);
	if (fm == 2 || fm == 1) {
		lnb_set_tone(dev, l, 0, SEC_TONE_OFF);
		if (old_quattro) {
			lnb_set_tone(dev, l, 1, SEC_TONE_OFF);
			lnb_set_tone(dev, l, 2, SEC_TONE_ON);
		} else {
			lnb_set_tone(dev, l, 1, SEC_TONE_ON);
			lnb_set_tone(dev, l, 2, SEC_TONE_OFF);
		}
		lnb_set_tone(dev, l, 3, SEC_TONE_ON);
	}
	link->lnb.fmode = fm;
	mutex_unlock(&link->lnb.lock);
	return 0;
}

static struct mxl5xx_cfg mxl5xx = {
	.adr      = 0x60,
	.type     = 0x01,
	.clk      = 27000000,
	.ts_clk   = 139,
	.cap      = 12,
	.fw_read  = mxl_fw_read,
};

static int fe_attach_mxl5xx(struct ddb_input *input)
{
	struct ddb *dev = input->port->dev;
	struct i2c_adapter *i2c = &input->port->i2c->adap;
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];
	struct ddb_port *port = input->port;
	struct ddb_link *link = &dev->link[port->lnr];
	struct mxl5xx_cfg cfg;
	int demod, tuner;

	cfg = mxl5xx;
	cfg.fw_priv = link;
	if (dev->link[0].info->type == DDB_OCTONET)
		;//cfg.ts_clk = 69;

	demod = input->nr;
	tuner = demod & 3;
	if (fmode == 3)
		tuner = 0;
	dvb->fe = dvb_attach(mxl5xx_attach, i2c, &cfg, demod, tuner);
	if (!dvb->fe) {
		pr_err("No MXL5XX found!\n");
		return -ENODEV;
	}
	if (input->nr < 4) {
		lnb_command(dev, port->lnr, input->nr, LNB_CMD_INIT);
		lnb_set_voltage(dev, port->lnr, input->nr, SEC_VOLTAGE_OFF);
	}
	lnb_init_fmode(dev, link, fmode);

	dvb->fe->ops.set_voltage = max_set_voltage;
	dvb->fe->ops.enable_high_lnb_voltage = max_enable_high_lnb_voltage;
	dvb->fe->ops.set_tone = max_set_tone;
	dvb->diseqc_send_master_cmd = dvb->fe->ops.diseqc_send_master_cmd;
	dvb->fe->ops.diseqc_send_master_cmd = max_send_master_cmd;
	dvb->fe->ops.diseqc_send_burst = max_send_burst;
	dvb->fe->sec_priv = input;
	dvb->set_input = dvb->fe->ops.set_input;
	dvb->fe->ops.set_input = max_set_input;
	dvb->input = tuner;
	return 0;
}

static int my_dvb_dmx_ts_card_init(struct dvb_demux *dvbdemux, char *id,
				   int (*start_feed)(struct dvb_demux_feed *),
				   int (*stop_feed)(struct dvb_demux_feed *),
				   void *priv)
{
	dvbdemux->priv = priv;

	dvbdemux->filternum = 256;
	dvbdemux->feednum = 256;
	dvbdemux->start_feed = start_feed;
	dvbdemux->stop_feed = stop_feed;
	dvbdemux->write_to_decoder = NULL;
	dvbdemux->dmx.capabilities = (DMX_TS_FILTERING |
				      DMX_SECTION_FILTERING |
				      DMX_MEMORY_BASED_FILTERING);
	return dvb_dmx_init(dvbdemux);
}

static int my_dvb_dmxdev_ts_card_init(struct dmxdev *dmxdev,
				      struct dvb_demux *dvbdemux,
				      struct dmx_frontend *hw_frontend,
				      struct dmx_frontend *mem_frontend,
				      struct dvb_adapter *dvb_adapter)
{
	int ret;

	dmxdev->filternum = 256;
	dmxdev->demux = &dvbdemux->dmx;
	dmxdev->capabilities = 0;
	ret = dvb_dmxdev_init(dmxdev, dvb_adapter);
	if (ret < 0)
		return ret;

	hw_frontend->source = DMX_FRONTEND_0;
	dvbdemux->dmx.add_frontend(&dvbdemux->dmx, hw_frontend);
	mem_frontend->source = DMX_MEMORY_FE;
	dvbdemux->dmx.add_frontend(&dvbdemux->dmx, mem_frontend);
	return dvbdemux->dmx.connect_frontend(&dvbdemux->dmx, hw_frontend);
}

#if 0
static int start_input(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (!dvb->users)
		ddb_input_start_all(input);

	return ++dvb->users;
}

static int stop_input(struct ddb_input *input)
{
	struct ddb_dvb *dvb = &input->port->dvb[input->nr & 1];

	if (--dvb->users)
		return dvb->users;

	ddb_input_stop_all(input);
	return 0;
}
#endif

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
	case 0x31:
		if (dvb->fe2)
			dvb_unregister_frontend(dvb->fe2);
		if (dvb->fe)
			dvb_unregister_frontend(dvb->fe);
		/* fallthrough */
	case 0x30:
		dvb_frontend_detach(dvb->fe);
		dvb->fe = dvb->fe2 = NULL;
		/* fallthrough */
	case 0x21:
		if (input->port->dev->ns_num)
			dvb_netstream_release(&dvb->dvbns);
		/* fallthrough */
	case 0x20:
		dvb_net_release(&dvb->dvbnet);
		/* fallthrough */
	case 0x11:
		dvbdemux->dmx.close(&dvbdemux->dmx);
		dvbdemux->dmx.remove_frontend(&dvbdemux->dmx,
					      &dvb->hw_frontend);
		dvbdemux->dmx.remove_frontend(&dvbdemux->dmx,
					      &dvb->mem_frontend);
		dvb_dmxdev_release(&dvb->dmxdev);
		/* fallthrough */
	case 0x10:
		dvb_dmx_release(&dvb->demux);
		/* fallthrough */
	case 0x01:
		break;
	}
	dvb->attached = 0x00;
}

static int dvb_register_adapters(struct ddb *dev)
{
	int i, ret = 0;
	struct ddb_port *port;
	struct dvb_adapter *adap;

	if (adapter_alloc == 3 || dev->link[0].info->type == DDB_MOD ||
	     dev->link[0].info->type == DDB_OCTONET) {
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

	for (i = 0; i < dev->link[0].info->port_num; i++) {
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
	int par = 0;
	
	dvb->attached = 0x01;

	ret = my_dvb_dmx_ts_card_init(dvbdemux, "SW demux",
				      start_feed,
				      stop_feed, input);
	if (ret < 0)
		return ret;
	dvb->attached = 0x10;

	ret = my_dvb_dmxdev_ts_card_init(&dvb->dmxdev,
					 &dvb->demux,
					 &dvb->hw_frontend,
					 &dvb->mem_frontend, adap);
	if (ret < 0)
		return ret;
	dvb->attached = 0x11;

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
	dvb->fe = dvb->fe2 = 0;
	switch (port->type) {
	case DDB_TUNER_MXL5XX:
		if (fe_attach_mxl5xx(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_ST:
		if (demod_attach_stv0900(input, 0) < 0)
			return -ENODEV;
		if (tuner_attach_stv6110(input, 0) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_ST_AA:
		if (demod_attach_stv0900(input, 1) < 0)
			return -ENODEV;
		if (tuner_attach_stv6110(input, 1) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910:
		if (demod_attach_stv0910(input, 0) < 0)
			return -ENODEV;
		if (tuner_attach_stv6111(input, 0) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910_PR:
		if (demod_attach_stv0910(input, 1) < 0)
			return -ENODEV;
		if (tuner_attach_stv6111(input, 1) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBS_STV0910_P:
		if (demod_attach_stv0910(input, 0) < 0)
			return -ENODEV;
		if (tuner_attach_stv6111(input, 1) < 0)
			return -ENODEV;
		break;
#ifdef CONFIG_DVB_DRXK
	case DDB_TUNER_DVBCT_TR:
		if (demod_attach_drxk(input) < 0)
			return -ENODEV;
		if (tuner_attach_tda18271(input) < 0)
			return -ENODEV;
		break;
#endif
	case DDB_TUNER_DVBCT_ST:
		if (demod_attach_stv0367dd(input) < 0)
			return -ENODEV;
		if (tuner_attach_tda18212dd(input) < 0)
			return -ENODEV;
		break;
	case DDB_TUNER_DVBCT2_SONY_P:
	case DDB_TUNER_DVBC2T2_SONY_P:
	case DDB_TUNER_ISDBT_SONY_P:
		if (input->port->dev->link[input->port->lnr].info->ts_quirks & TS_QUIRK_SERIAL) 
			par = 0;
		else
			par = 1;
	case DDB_TUNER_DVBCT2_SONY:
	case DDB_TUNER_DVBC2T2_SONY:
	case DDB_TUNER_ISDBT_SONY:
		if (demod_attach_cxd2843(input, par) < 0)
			return -ENODEV;
		if (tuner_attach_tda18212dd(input) < 0)
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
	if (dvb->fe2) {
		if (dvb_register_frontend(adap, dvb->fe2) < 0)
			return -ENODEV;
		dvb->fe2->tuner_priv = dvb->fe->tuner_priv;
		memcpy(&dvb->fe2->ops.tuner_ops,
		       &dvb->fe->ops.tuner_ops,
		       sizeof(struct dvb_tuner_ops));
	}
	dvb->attached = 0x31;
	return 0;
}


static int port_has_encti(struct ddb_port *port)
{
	u8 val;
	int ret = i2c_read_reg(&port->i2c->adap, 0x20, 0, &val);

	if (!ret)
		pr_info("[0x20]=0x%02x\n", val);
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
		pr_info("Port %d: invalid XO2\n", port->nr);
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
	i2c_write_reg(i2c, 0x10, 0x09, 2);

	if (dev->link[port->lnr].info->con_clock) {
		pr_info("Setting continuous clock for XO2\n");
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
		pr_info("Port %d: invalid XO2 CI %02x\n",
			port->nr, data[0]);
		return -1;
	}
	pr_info("Port %d: DuoFlex CI %u.%u\n", port->nr, data[0], data[1]);

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
		pr_info("Setting continuous clock for DuoFLex CI\n");
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
	"DUAL ATSC", "DUAL DVB-C/C2/T/T2",
	"", ""
};

static char *xo2types[] = {
	"DVBS_ST", "DVBCT2_SONY",
	"ISDBT_SONY", "DVBC2T2_SONY",
	"ATSC_ST", "DVBC2T2_ST"
};

static void ddb_port_probe(struct ddb_port *port)
{
	struct ddb *dev = port->dev;
	u32 l = port->lnr;
	u8 id, type;

	port->name = "NO MODULE";
	port->type_name = "NONE";
	port->class = DDB_PORT_NONE;

	/* Handle missing ports and ports without I2C */
	
	if (port->nr == ts_loop) {
		port->name = "TS LOOP";
		port->class = DDB_PORT_LOOP;
		return;
	}
	
	if (port->nr == 1 && dev->link[l].info->type == DDB_OCTOPUS_CI &&
	    dev->link[l].info->i2c_mask == 1) {
		port->name = "NO TAB";
		port->class = DDB_PORT_NONE;
		return;
	}

	if (dev->link[l].info->type == DDB_MOD) {
		port->name = "MOD";
		port->class = DDB_PORT_MOD;
		return;
	}

	if (dev->link[l].info->type == DDB_OCTOPUS_MAX) {
		port->name = "DUAL DVB-S2 MAX";
		port->type_name = "MXL5XX";
		port->class = DDB_PORT_TUNER;
		port->type = DDB_TUNER_MXL5XX;
		if (port->i2c)
			ddbwritel(dev, I2C_SPEED_400,
				  port->i2c->regs + I2C_TIMING);
		return;
	}

	if (port->nr > 1 && dev->link[l].info->type == DDB_OCTOPUS_CI) {
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
			pr_info(KERN_INFO "Port %d: Uninitialized DuoFlex\n",
			       port->nr);
			return;
		}
	} else if (port_has_xo2(port, &type, &id)) {
		ddbwritel(dev, I2C_SPEED_400, port->i2c->regs + I2C_TIMING);
		/*pr_info("XO2 ID %02x\n", id);*/
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
			    dev->link[l].info->ts_quirks & TS_QUIRK_REVERSED)
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

static int wait_ci_ready(struct ddb_ci *ci)
{
	u32 count = 10;

	ndelay(500);
	do {
		if (ddbreadl(ci->port->dev,
			     CI_CONTROL(ci->nr)) & CI_READY)
			break;
		usleep_range(1, 2);
		if ((--count) == 0)
			return -1;
	} while (1);
	return 0;
}

static int read_attribute_mem(struct dvb_ca_en50221 *ca,
			      int slot, int address)
{
	struct ddb_ci *ci = ca->data;
	u32 val, off = (address >> 1) & (CI_BUFFER_SIZE - 1);

	if (address > CI_BUFFER_SIZE)
		return -1;
	ddbwritel(ci->port->dev, CI_READ_CMD | (1 << 16) | address,
		  CI_DO_READ_ATTRIBUTES(ci->nr));
	wait_ci_ready(ci);
	val = 0xff & ddbreadl(ci->port->dev, CI_BUFFER(ci->nr) + off);
	return val;
}

static int write_attribute_mem(struct dvb_ca_en50221 *ca, int slot,
			       int address, u8 value)
{
	struct ddb_ci *ci = ca->data;

	ddbwritel(ci->port->dev, CI_WRITE_CMD | (value << 16) | address,
		  CI_DO_ATTRIBUTE_RW(ci->nr));
	wait_ci_ready(ci);
	return 0;
}

static int read_cam_control(struct dvb_ca_en50221 *ca,
			    int slot, u8 address)
{
	u32 count = 100;
	struct ddb_ci *ci = ca->data;
	u32 res;

	ddbwritel(ci->port->dev, CI_READ_CMD | address,
		  CI_DO_IO_RW(ci->nr));
	ndelay(500);
	do {
		res = ddbreadl(ci->port->dev, CI_READDATA(ci->nr));
		if (res & CI_READY)
			break;
		usleep_range(1, 2);
		if ((--count) == 0)
			return -1;
	} while (1);
	return 0xff & res;
}

static int write_cam_control(struct dvb_ca_en50221 *ca, int slot,
			     u8 address, u8 value)
{
	struct ddb_ci *ci = ca->data;

	ddbwritel(ci->port->dev, CI_WRITE_CMD | (value << 16) | address,
		  CI_DO_IO_RW(ci->nr));
	wait_ci_ready(ci);
	return 0;
}

static int slot_reset(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;

	ddbwritel(ci->port->dev, CI_POWER_ON,
		  CI_CONTROL(ci->nr));
	msleep(100);
	ddbwritel(ci->port->dev, CI_POWER_ON | CI_RESET_CAM,
		  CI_CONTROL(ci->nr));
	ddbwritel(ci->port->dev, CI_ENABLE | CI_POWER_ON | CI_RESET_CAM,
		  CI_CONTROL(ci->nr));
	udelay(20);
	ddbwritel(ci->port->dev, CI_ENABLE | CI_POWER_ON,
		  CI_CONTROL(ci->nr));
	return 0;
}

static int slot_shutdown(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;

	ddbwritel(ci->port->dev, 0, CI_CONTROL(ci->nr));
	msleep(300);
	return 0;
}

static int slot_ts_enable(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;
	u32 val = ddbreadl(ci->port->dev, CI_CONTROL(ci->nr));

	ddbwritel(ci->port->dev, val | CI_BYPASS_DISABLE,
		  CI_CONTROL(ci->nr));
	return 0;
}

static int poll_slot_status(struct dvb_ca_en50221 *ca, int slot, int open)
{
	struct ddb_ci *ci = ca->data;
	u32 val = ddbreadl(ci->port->dev, CI_CONTROL(ci->nr));
	int stat = 0;

	if (val & CI_CAM_DETECT)
		stat |= DVB_CA_EN50221_POLL_CAM_PRESENT;
	if (val & CI_CAM_READY)
		stat |= DVB_CA_EN50221_POLL_CAM_READY;
	return stat;
}

static struct dvb_ca_en50221 en_templ = {
	.read_attribute_mem  = read_attribute_mem,
	.write_attribute_mem = write_attribute_mem,
	.read_cam_control    = read_cam_control,
	.write_cam_control   = write_cam_control,
	.slot_reset          = slot_reset,
	.slot_shutdown       = slot_shutdown,
	.slot_ts_enable      = slot_ts_enable,
	.poll_slot_status    = poll_slot_status,
};

static void ci_attach(struct ddb_port *port)
{
	struct ddb_ci *ci = 0;

	ci = kzalloc(sizeof(*ci), GFP_KERNEL);
	if (!ci)
		return;
	memcpy(&ci->en, &en_templ, sizeof(en_templ));
	ci->en.data = ci;
	port->en = &ci->en;
	ci->port = port;
	ci->nr = port->nr - 2;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static int write_creg(struct ddb_ci *ci, u8 data, u8 mask)
{
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;
	
	ci->port->creg = (ci->port->creg & ~mask) | data;
	return i2c_write_reg(i2c, adr, 0x02, ci->port->creg);
}

static int read_attribute_mem_xo2(struct dvb_ca_en50221 *ca,
				  int slot, int address)
{
	struct ddb_ci *ci = ca->data;
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;
	int res;
	u8 val;
	
	res = i2c_read_reg16(i2c, adr, 0x8000 | address, &val);
	return res ? res : val;
}

static int write_attribute_mem_xo2(struct dvb_ca_en50221 *ca, int slot,
				   int address, u8 value)
{
	struct ddb_ci *ci = ca->data;
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;

	return i2c_write_reg16(i2c, adr, 0x8000 | address, value);
}

static int read_cam_control_xo2(struct dvb_ca_en50221 *ca,
				int slot, u8 address)
{
	struct ddb_ci *ci = ca->data;
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;
	u8 val;
	int res;
	
	res = i2c_read_reg(i2c, adr, 0x20 | (address & 3), &val);
	return res ? res : val;
}

static int write_cam_control_xo2(struct dvb_ca_en50221 *ca, int slot,
				 u8 address, u8 value)
{
	struct ddb_ci *ci = ca->data;
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;
	
	return i2c_write_reg(i2c, adr, 0x20 | (address & 3), value);
}

static int slot_reset_xo2(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;

	pr_info("%s\n", __func__);
	write_creg(ci, 0x01, 0x01);
	write_creg(ci, 0x04, 0x04);
	msleep(20);
	write_creg(ci, 0x02, 0x02);
	write_creg(ci, 0x00, 0x04);
	write_creg(ci, 0x18, 0x18);
	return 0;
}

static int slot_shutdown_xo2(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;
	
	pr_info("%s\n", __func__);
	//i2c_write_reg(i2c, adr, 0x03, 0x60);
	//i2c_write_reg(i2c, adr, 0x00, 0xc0);
	write_creg(ci, 0x10, 0xff);
	write_creg(ci, 0x08, 0x08);
	return 0;
}

static int slot_ts_enable_xo2(struct dvb_ca_en50221 *ca, int slot)
{
	struct ddb_ci *ci = ca->data;

	pr_info("%s\n", __func__);
	write_creg(ci, 0x00, 0x10);
	return 0;
}

static int poll_slot_status_xo2(struct dvb_ca_en50221 *ca, int slot, int open)
{
	struct ddb_ci *ci = ca->data;
	struct i2c_adapter *i2c = &ci->port->i2c->adap;
	u8 adr = (ci->port->type == DDB_CI_EXTERNAL_XO2) ? 0x12 : 0x13;
	u8 val = 0;
	int stat = 0;

	i2c_read_reg(i2c, adr, 0x01, &val);
	//pr_info("%s %02x\n", __func__, val);
	
	if (val & 2)
		stat |= DVB_CA_EN50221_POLL_CAM_PRESENT;
	if (val & 1)
		stat |= DVB_CA_EN50221_POLL_CAM_READY;
	return stat;
}

static struct dvb_ca_en50221 en_xo2_templ = {
	.read_attribute_mem  = read_attribute_mem_xo2,
	.write_attribute_mem = write_attribute_mem_xo2,
	.read_cam_control    = read_cam_control_xo2,
	.write_cam_control   = write_cam_control_xo2,
	.slot_reset          = slot_reset_xo2,
	.slot_shutdown       = slot_shutdown_xo2,
	.slot_ts_enable      = slot_ts_enable_xo2,
	.poll_slot_status    = poll_slot_status_xo2,
};

static void ci_xo2_attach(struct ddb_port *port)
{
	struct ddb_ci *ci = 0;
	struct i2c_adapter *i2c;

	ci = kzalloc(sizeof(*ci), GFP_KERNEL);
	if (!ci)
		return;
	memcpy(&ci->en, &en_xo2_templ, sizeof(en_xo2_templ));
	ci->en.data = ci;
	port->en = &ci->en;
	ci->port = port;
	ci->nr = port->nr - 2;
	ci->port->creg = 0;
	i2c = &ci->port->i2c->adap;
	write_creg(ci, 0x10, 0xff);
	write_creg(ci, 0x08, 0x08);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


struct cxd2099_cfg cxd_cfg = {
	.bitrate =  72000,
	.adr     =  0x40,
	.polarity = 1,
	.clock_mode = 1,
	.max_i2c = 512,
};

static int ddb_ci_attach(struct ddb_port *port)
{
	switch (port->type) {
	case DDB_CI_EXTERNAL_SONY:
		cxd_cfg.bitrate = ci_bitrate;
		port->en = cxd2099_attach(&cxd_cfg, port, &port->i2c->adap);
		if (!port->en)
			return -ENODEV;
		dvb_ca_en50221_init(port->dvb[0].adap,
				    port->en, 0, 1);
		break;

	case DDB_CI_EXTERNAL_XO2:
	case DDB_CI_EXTERNAL_XO2_B:
		ci_xo2_attach(port);
		if (!port->en)
			return -ENODEV;
		dvb_ca_en50221_init(port->dvb[0].adap, port->en, 0, 1);
		break;

	case DDB_CI_INTERNAL:
		ci_attach(port);
		if (!port->en)
			return -ENODEV;
		dvb_ca_en50221_init(port->dvb[0].adap, port->en, 0, 1);
		break;
	}
	return 0;
}

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
		ret = ddb_ci_attach(port);
		if (ret < 0)
			break;
	case DDB_PORT_LOOP:
		ret = dvb_register_device(port->dvb[0].adap,
					  &port->dvb[0].dev,
					  &dvbdev_ci, (void *) port->output,
					  DVB_DEVICE_CI);
		break;
	case DDB_PORT_MOD:
		ret = dvb_register_device(port->dvb[0].adap,
					  &port->dvb[0].dev,
					  &dvbdev_mod, (void *) port->output,
					  DVB_DEVICE_MOD);
		break;
	default:
		break;
	}
	if (ret < 0)
		pr_err("port_attach on port %d failed\n", port->nr);
	return ret;
}

static int ddb_ports_attach(struct ddb *dev)
{
	int i, ret = 0;
	struct ddb_port *port;

	dev->ns_num = dev->link[0].info->ns_num;
	for (i = 0; i < dev->ns_num; i++)
		dev->ns[i].nr = i;
	pr_info("%d netstream channels\n", dev->ns_num);

	if (dev->port_num) {
		ret = dvb_register_adapters(dev);
		if (ret < 0) {
			pr_err("Registering adapters failed. Check DVB_MAX_ADAPTERS in config.\n");
			return ret;
		}
	}
	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
		ret = ddb_port_attach(port);
#if 0
		if (ret < 0)
			break;
#endif
	}
	return ret;
}

static void ddb_ports_detach(struct ddb *dev)
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
				kfree(port->en);
				port->en = 0;
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
		  input->dma->stat, DMA_BUFFER_ACK(output->dma->nr));
	output->dma->cbuf = (input->dma->stat >> 11) & 0x1f;
	output->dma->coff = (input->dma->stat & 0x7ff) << 7;
}

static void output_ack_input(struct ddb_output *output,
			     struct ddb_input *input)
{
	ddbwritel(input->port->dev,
		  output->dma->stat, DMA_BUFFER_ACK(input->dma->nr));
}

static void input_write_dvb(struct ddb_input *input,
			    struct ddb_input *input2)
{
	struct ddb_dvb *dvb = &input2->port->dvb[input2->nr & 1];
	struct ddb_dma *dma, *dma2;
	struct ddb *dev = input->port->dev;
	int ack = 1;

	dma = dma2 = input->dma;
	/* if there also is an output connected, do not ACK.
	   input_write_output will ACK. */
	if (input->redo) {
		dma2 = input->redo->dma;
		ack = 0;
	}
	while (dma->cbuf != ((dma->stat >> 11) & 0x1f)
	       || (4 & dma->ctrl)) {
		if (4 & dma->ctrl) {
			/*pr_err("Overflow dma %d\n", dma->nr);*/
			ack = 1;
		}
#ifdef DDB_ALT_DMA
		dma_sync_single_for_cpu(dev->dev, dma2->pbuf[dma->cbuf],
					dma2->size, DMA_FROM_DEVICE);
#endif
		dvb_dmx_swfilter_packets(&dvb->demux,
					 dma2->vbuf[dma->cbuf],
					 dma2->size / 188);
		dma->cbuf = (dma->cbuf + 1) % dma2->num;
		if (ack)
			ddbwritel(dev, (dma->cbuf << 11),
				  DMA_BUFFER_ACK(dma->nr));
		dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma->nr));
		dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma->nr));
	}
}

#ifdef DDB_USE_WORK
static void input_work(struct work_struct *work)
{
	struct ddb_dma *dma = container_of(work, struct ddb_dma, work);
	struct ddb_input *input = (struct ddb_input *) dma->io;
#else
static void input_tasklet(unsigned long data)
{
	struct ddb_input *input = (struct ddb_input *) data;
	struct ddb_dma *dma = input->dma;
#endif
	struct ddb *dev = input->port->dev;
	unsigned long flags;
	
	spin_lock_irqsave(&dma->lock, flags);
	if (!dma->running) {
		spin_unlock_irqrestore(&dma->lock, flags);
		return;
	}
	dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma->nr));
	dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma->nr));

#if 0
	if (4 & dma->ctrl)
		pr_err("Overflow dma %d\n", dma->nr);
#endif
	if (input->redi)
		input_write_dvb(input, input->redi);
	if (input->redo)
		input_write_output(input, input->redo);
	wake_up(&dma->wq);
	spin_unlock_irqrestore(&dma->lock, flags);
}

static void input_handler(unsigned long data)
{
	struct ddb_input *input = (struct ddb_input *) data;
	struct ddb_dma *dma = input->dma;


	/* If there is no input connected, input_tasklet() will
	   just copy pointers and ACK. So, there is no need to go
	   through the tasklet scheduler. */
#ifdef DDB_USE_WORK
	if (input->redi)
		queue_work(ddb_wq, &dma->work);
	else
		input_work(&dma->work);
#else
	if (input->redi)
		tasklet_schedule(&dma->tasklet);
	else
		input_tasklet(data);
#endif
}

static void output_handler(unsigned long data)
{
	struct ddb_output *output = (struct ddb_output *) data;
	struct ddb_dma *dma = output->dma;
	struct ddb *dev = output->port->dev;

	spin_lock(&dma->lock);
	if (!dma->running) {
		spin_unlock(&dma->lock);
		return;
	}
	dma->stat = ddbreadl(dev, DMA_BUFFER_CURRENT(dma->nr));
	dma->ctrl = ddbreadl(dev, DMA_BUFFER_CONTROL(dma->nr));
	if (output->redi)
		output_ack_input(output, output->redi);
	wake_up(&dma->wq);
	spin_unlock(&dma->lock);
}


/****************************************************************************/
/****************************************************************************/


static void ddb_dma_init(struct ddb_dma *dma, int nr, void *io, int out)
{
#ifndef DDB_USE_WORK
	unsigned long priv = (unsigned long) io;
#endif

	dma->io = io;
	dma->nr = nr;
	spin_lock_init(&dma->lock);
	init_waitqueue_head(&dma->wq);
	if (out) {
		dma->num = OUTPUT_DMA_BUFS;
		dma->size = OUTPUT_DMA_SIZE;
		dma->div = OUTPUT_DMA_IRQ_DIV;
	} else {
#ifdef DDB_USE_WORK
		INIT_WORK(&dma->work, input_work);
#else
		tasklet_init(&dma->tasklet, input_tasklet, priv);
#endif
		dma->num = INPUT_DMA_BUFS;
		dma->size = INPUT_DMA_SIZE;
		dma->div = INPUT_DMA_IRQ_DIV;
	}
}

static void ddb_input_init(struct ddb_port *port, int nr, int pnr, int dma_nr, int anr)
{
	struct ddb *dev = port->dev;
	struct ddb_input *input = &dev->input[anr];

	if (dev->has_dma) {
		dev->handler[dma_nr + 8] = input_handler;
		dev->handler_data[dma_nr + 8] = (unsigned long) input;
	}
	port->input[pnr] = input;
	input->nr = nr;
	input->port = port;
	if (dev->has_dma) {
		input->dma = &dev->dma[dma_nr];
		ddb_dma_init(input->dma, dma_nr, (void *) input, 0);
	}
	ddbwritel(dev, 0, TS_INPUT_CONTROL(nr));
	ddbwritel(dev, 2, TS_INPUT_CONTROL(nr));
	ddbwritel(dev, 0, TS_INPUT_CONTROL(nr));
	if (input->dma)
		ddbwritel(dev, 0, DMA_BUFFER_ACK(input->dma->nr));
}

static void ddb_output_init(struct ddb_port *port, int nr, int dma_nr)
{
	struct ddb *dev = port->dev;
	struct ddb_output *output = &dev->output[nr];

	if (dev->has_dma) {
		dev->handler[dma_nr + 8] = output_handler;
		dev->handler_data[dma_nr + 8] = (unsigned long) output;
	}
	port->output = output;
	output->nr = nr;
	output->port = port;
	if (dev->has_dma) {
		output->dma = &dev->dma[dma_nr];
		ddb_dma_init(output->dma, dma_nr, (void *) output, 1);
	}
	if (output->port->class == DDB_PORT_MOD) {
		/*ddbwritel(dev, 0, CHANNEL_CONTROL(output->nr));*/
	} else {
		ddbwritel(dev, 0, TS_OUTPUT_CONTROL(nr));
		ddbwritel(dev, 2, TS_OUTPUT_CONTROL(nr));
		ddbwritel(dev, 0, TS_OUTPUT_CONTROL(nr));
	}
	if (output->dma)
		ddbwritel(dev, 0, DMA_BUFFER_ACK(output->dma->nr));
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

static void ddb_ports_init(struct ddb *dev)
{
	u32 i, l, p, li2c;
	struct ddb_port *port;
	struct ddb_info *info;
	struct ddb_regmap *rm;

	for (p = l = 0; l < DDB_MAX_LINK; l++) {
		info = dev->link[l].info;
		if (!info)
			continue;
		rm = info->regmap;
		if (!rm)
			continue;
		for (li2c = 0; li2c < dev->i2c_num; li2c++)
			if (dev->i2c[li2c].link == l) 
				break;
		for (i = 0; i < info->port_num; i++, p++) {
			port = &dev->port[p];
			port->dev = dev;
			port->nr = i;
			port->lnr = l;
			port->pnr = p;
			port->gap = 4;
			port->obr = ci_bitrate;
			mutex_init(&port->i2c_gate_lock);

			if (!ddb_port_match_i2c(port)) {
				if (info->type == DDB_OCTOPUS_MAX)
					port->i2c = &dev->i2c[li2c];		
			}

			ddb_port_probe(port);
	
			port->dvb[0].adap = &dev->adap[2 * p];
			port->dvb[1].adap = &dev->adap[2 * p + 1];
			
			if ((port->class == DDB_PORT_NONE) && i &&
			    dev->port[p - 1].type == DDB_CI_EXTERNAL_XO2) {
				port->class = DDB_PORT_CI;
				port->type = DDB_CI_EXTERNAL_XO2_B;
				port->name = "DuoFlex CI_B";
				port->i2c = dev->port[p - 1].i2c;
			}
			
			pr_info("Port %u: Link %u, Link Port %u (TAB %u): %s\n",
				port->pnr, port->lnr, port->nr, port->nr + 1, port->name);
			
			if (port->class == DDB_PORT_CI &&
			    port->type == DDB_CI_EXTERNAL_XO2) {
				ddb_input_init(port, 2 * i, 0, 2 * i, 2 * i);
				ddb_output_init(port, i, i + 8);
				continue;
			}
			
			if (port->class == DDB_PORT_CI &&
			    port->type == DDB_CI_EXTERNAL_XO2_B) {
				ddb_input_init(port, 2 * i - 1, 0, 2 * i - 1, 2 * i - 1);
				ddb_output_init(port, i, i + 8);
				continue;
			}
			
			switch (dev->link[l].info->type) {
			case DDB_OCTOPUS_CI:
				if (i >= 2) {
					ddb_input_init(port, 2 + i, 0, 2 + i, 2 + i);
					ddb_input_init(port, 4 + i, 1, 4 + i, 4 + i);
					ddb_output_init(port, i, i + 8);
					break;
				} /* fallthrough */
			case DDB_OCTONET:
			case DDB_OCTOPUS:
				ddb_input_init(port, 2 * i, 0, 2 * i, 2 * i);
				ddb_input_init(port, 2 * i + 1, 1, 2 * i + 1, 2 * i + 1);
				ddb_output_init(port, i, i + 8);
				break;
			case DDB_OCTOPUS_MAX:
			case DDB_OCTOPUS_MAX_CT:
				ddb_input_init(port, 2 * i, 0, 2 * i, 2 * p);
				ddb_input_init(port, 2 * i + 1, 1, 2 * i + 1, 2 * p + 1);
				break;
			case DDB_MOD:
				ddb_output_init(port, i, i);
				dev->handler[i + 18] = ddbridge_mod_rate_handler;
				dev->handler_data[i + 18] =
					(unsigned long) &dev->output[i];
				break;
			default:
				break;
			}
		}
	}
	dev->port_num = p;
}

static void ddb_ports_release(struct ddb *dev)
{
	int i;
	struct ddb_port *port;
	
	for (i = 0; i < dev->port_num; i++) {
		port = &dev->port[i];
#ifdef DDB_USE_WORK
		if (port->input[0] && port->input[0]->dma)
			cancel_work_sync(&port->input[0]->dma->work);
		if (port->input[1] && port->input[1]->dma)
			cancel_work_sync(&port->input[1]->dma->work);
		if (port->output && port->output->dma)
			cancel_work_sync(&port->output->dma->work);
#else
		if (port->input[0] && port->input[0]->dma)
			tasklet_kill(&port->input[0]->dma->tasklet);
		if (port->input[1] && port->input[1]->dma)
			tasklet_kill(&port->input[1]->dma->tasklet);
		if (port->output && port->output->dma)
			tasklet_kill(&port->output->dma->tasklet);
#endif
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#define IRQ_HANDLE(_nr) \
	do { if ((s & (1UL << _nr)) && dev->handler[_nr]) \
		dev->handler[_nr](dev->handler_data[_nr]); } \
	while (0)

static void irq_handle_msg(struct ddb *dev, u32 s)
{
	dev->i2c_irq++;
	IRQ_HANDLE(0);
	IRQ_HANDLE(1);
	IRQ_HANDLE(2);
	IRQ_HANDLE(3);
}

static void irq_handle_io(struct ddb *dev, u32 s)
{
	dev->ts_irq++;
	if ((s & 0x000000f0)) {
		IRQ_HANDLE(4);
		IRQ_HANDLE(5);
		IRQ_HANDLE(6);
		IRQ_HANDLE(7);
	}
	if ((s & 0x0000ff00)) {
		IRQ_HANDLE(8);
		IRQ_HANDLE(9);
		IRQ_HANDLE(10);
		IRQ_HANDLE(11);
		IRQ_HANDLE(12);
		IRQ_HANDLE(13);
		IRQ_HANDLE(14);
		IRQ_HANDLE(15);
	}
	if ((s & 0x00ff0000)) {
		IRQ_HANDLE(16);
		IRQ_HANDLE(17);
		IRQ_HANDLE(18);
		IRQ_HANDLE(19);
		IRQ_HANDLE(20);
		IRQ_HANDLE(21);
		IRQ_HANDLE(22);
		IRQ_HANDLE(23);
	}
	if ((s & 0xff000000)) {
		IRQ_HANDLE(24);
		IRQ_HANDLE(25);
		IRQ_HANDLE(26);
		IRQ_HANDLE(27);
		IRQ_HANDLE(28);
		IRQ_HANDLE(29);
		IRQ_HANDLE(30);
		IRQ_HANDLE(31);
	}
}

static irqreturn_t irq_handler0(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *) dev_id;
	u32 s = ddbreadl(dev, INTERRUPT_STATUS);

	do {
		if (s == 0xffffffff)
			return IRQ_NONE;
		if (!(s & 0xfff00))
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_ACK);
		irq_handle_io(dev, s);
	} while ((s = ddbreadl(dev, INTERRUPT_STATUS)));

	return IRQ_HANDLED;
}

static irqreturn_t irq_handler1(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *) dev_id;
	u32 s = ddbreadl(dev, INTERRUPT_STATUS);

	do {
		if (s & 0x80000000)
			return IRQ_NONE;
		if (!(s & 0x0000f))
			return IRQ_NONE;
		ddbwritel(dev, s, INTERRUPT_ACK);
		irq_handle_msg(dev, s);
	} while ((s = ddbreadl(dev, INTERRUPT_STATUS)));

	return IRQ_HANDLED;
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct ddb *dev = (struct ddb *) dev_id;
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
#ifdef DDB_TEST_THREADED
		ret = IRQ_WAKE_THREAD;
#endif
		}
	} while ((s = ddbreadl(dev, INTERRUPT_STATUS)));

	return ret;
}

#ifdef DDB_TEST_THREADED
static irqreturn_t irq_thread(int irq, void *dev_id)
{
	/* struct ddb *dev = (struct ddb *) dev_id; */

	/*pr_info("%s\n", __func__);*/

	return IRQ_HANDLED;
}
#endif

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

	/* unsigned long arg = (unsigned long) parg; */
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
		//pr_info("GET_TS %u.%u\n", input->port->lnr, input->nr);
		if (ddbreadl(dev, TS_CAPTURE_CONTROL) & 1) {
			pr_info("ts capture busy\n");
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
			to |= ((u32) ts->num << 16);
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
		if (ctrl & (1 << 14)) {
			/*pr_info("ts capture timeout\n");*/
			return -EAGAIN;
		}
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
		
		/*pr_info("cancel ts capture: 0x%x\n", ctrl);*/
		ddbwritel(dev, ctrl, TS_CAPTURE_CONTROL);
		ctrl = ddbreadl(dev, TS_CAPTURE_CONTROL);
		/*pr_info("control register is 0x%x\n", ctrl);*/
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
			pr_info("cannot stop ts capture, while it was neither finished not canceled\n");
			return -EBUSY;
		}
		/*pr_info("ts capture stopped\n");*/
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
	return dvb_usercopy(file, cmd, arg, nsd_do_ioctl);
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
				  &dvbdev_nsd, (void *) dev,
				  DVB_DEVICE_NSD);
	return ret;
}

static void ddb_nsd_detach(struct ddb *dev)
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

static int flashio(struct ddb *dev, u32 lnr, u8 *wbuf, u32 wlen, u8 *rbuf, u32 rlen)
{
	u32 data, shift;
	u32 tag = DDB_LINK_TAG(lnr);
	struct ddb_link *link = &dev->link[lnr];

	mutex_lock(&link->flash_mutex);
	if (wlen > 4)
		ddbwritel(dev, 1, tag | SPI_CONTROL);
	while (wlen > 4) {
		/* FIXME: check for big-endian */
		data = swab32(*(u32 *) wbuf);
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
		*(u32 *) rbuf = swab32(data);
		rbuf += 4;
		rlen -= 4;
	}
	ddbwritel(dev, 0x0003 | ((rlen << (8 + 3)) & 0x1F00), tag | SPI_CONTROL);
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

static int mdio_write(struct ddb *dev, u8 adr, u8 reg, u16 val)
{
	ddbwritel(dev, adr, MDIO_ADR);
	ddbwritel(dev, reg, MDIO_REG);
	ddbwritel(dev, val, MDIO_VAL);
	ddbwritel(dev, 0x03, MDIO_CTRL);
	while (ddbreadl(dev, MDIO_CTRL) & 0x02)
		ndelay(500);
	return 0;
}

static u16 mdio_read(struct ddb *dev, u8 adr, u8 reg)
{
	ddbwritel(dev, adr, MDIO_ADR);
	ddbwritel(dev, reg, MDIO_REG);
	ddbwritel(dev, 0x07, MDIO_CTRL);
	while (ddbreadl(dev, MDIO_CTRL) & 0x02)
		ndelay(500);
	return ddbreadl(dev, MDIO_VAL);
}

#define DDB_MAGIC 'd'

struct ddb_flashio {
	__u8 *write_buf;
	__u32 write_len;
	__u8 *read_buf;
	__u32 read_len;
	__u32 link;
};

struct ddb_gpio {
	__u32 mask;
	__u32 data;
};

struct ddb_id {
	__u16 vendor;
	__u16 device;
	__u16 subvendor;
	__u16 subdevice;
	__u32 hw;
	__u32 regmap;
};

struct ddb_reg {
	__u32 reg;
	__u32 val;
};

struct ddb_mem {
	__u32  off;
	__u8  *buf;
	__u32  len;
};

struct ddb_mdio {
	__u8   adr;
	__u8   reg;
	__u16  val;
};

struct ddb_i2c_msg {
	__u8   bus;
	__u8   adr;
	__u8  *hdr;
	__u32  hlen;
	__u8  *msg;
	__u32  mlen;
};

#define IOCTL_DDB_FLASHIO    _IOWR(DDB_MAGIC, 0x00, struct ddb_flashio)
#define IOCTL_DDB_GPIO_IN    _IOWR(DDB_MAGIC, 0x01, struct ddb_gpio)
#define IOCTL_DDB_GPIO_OUT   _IOWR(DDB_MAGIC, 0x02, struct ddb_gpio)
#define IOCTL_DDB_ID         _IOR(DDB_MAGIC, 0x03, struct ddb_id)
#define IOCTL_DDB_READ_REG   _IOWR(DDB_MAGIC, 0x04, struct ddb_reg)
#define IOCTL_DDB_WRITE_REG  _IOW(DDB_MAGIC, 0x05, struct ddb_reg)
#define IOCTL_DDB_READ_MEM   _IOWR(DDB_MAGIC, 0x06, struct ddb_mem)
#define IOCTL_DDB_WRITE_MEM  _IOR(DDB_MAGIC, 0x07, struct ddb_mem)
#define IOCTL_DDB_READ_MDIO  _IOWR(DDB_MAGIC, 0x08, struct ddb_mdio)
#define IOCTL_DDB_WRITE_MDIO _IOR(DDB_MAGIC, 0x09, struct ddb_mdio)
#define IOCTL_DDB_READ_I2C   _IOWR(DDB_MAGIC, 0x0a, struct ddb_i2c_msg)
#define IOCTL_DDB_WRITE_I2C  _IOR(DDB_MAGIC, 0x0b, struct ddb_i2c_msg)

#define DDB_NAME "ddbridge"

static u32 ddb_num;
static int ddb_major;
static DEFINE_MUTEX(ddb_mutex);

static int ddb_release(struct inode *inode, struct file *file)
{
	struct ddb *dev = file->private_data;

	dev->ddb_dev_users--;
	return 0;
}

static int ddb_open(struct inode *inode, struct file *file)
{
	struct ddb *dev = ddbs[iminor(inode)];

	if (dev->ddb_dev_users)
		return -EBUSY;
	dev->ddb_dev_users++;
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
		res = flashio(dev, fio.link, wbuf, fio.write_len, rbuf, fio.read_len);
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
		ddbid.hw = ddbreadl(dev, 0);
		ddbid.regmap = ddbreadl(dev, 4);
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

		if (!dev->link[0].info->mdio_num)
			return -EIO;
		if (copy_from_user(&mdio, parg, sizeof(mdio)))
			return -EFAULT;
		mdio.val = mdio_read(dev, mdio.adr, mdio.reg);
		if (copy_to_user(parg, &mdio, sizeof(mdio)))
			return -EFAULT;
		break;
	}
	case IOCTL_DDB_WRITE_MDIO:
	{
		struct ddb_mdio mdio;

		if (!dev->link[0].info->mdio_num)
			return -EIO;
		if (copy_from_user(&mdio, parg, sizeof(mdio)))
			return -EFAULT;
		mdio_write(dev, mdio.adr, mdio.reg, mdio.val);
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
		if (i2c.bus > dev->link[0].info->regmap->i2c->num)
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
		if (i2c.bus > dev->link[0].info->regmap->i2c->num)
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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
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

	return sprintf(buf, "%d\n", dev->link[0].info->port_num);
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
	unsigned val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	ddbwritel(dev, 1, GPIO_DIRECTION);
	ddbwritel(dev, val & 1, GPIO_OUTPUT);
	return count;
}

static ssize_t temp_show(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	struct i2c_adapter *adap;
	int temp, temp2, temp3, i;
	u8 tmp[2];

	if (dev->link[0].info->type == DDB_MOD) {
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
	if (!dev->link[0].info->temp_num)
		return sprintf(buf, "no sensor\n");
	adap = &dev->i2c[dev->link[0].info->temp_bus].adap;
	if (i2c_read_regs(adap, 0x48, 0, tmp, 2) < 0)
		return sprintf(buf, "read_error\n");
	temp = (tmp[0] << 3) | (tmp[1] >> 5);
	temp *= 125;
	if (dev->link[0].info->temp_num == 2) {
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

#if 0
static ssize_t qam_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct ddb *dev = dev_get_drvdata(device);
	struct i2c_adapter *adap;
	u8 tmp[4];
	s16 i, q;

	adap = &dev->i2c[1].adap;
	if (i2c_read_regs16(adap, 0x1f, 0xf480, tmp, 4) < 0)
		return sprintf(buf, "read_error\n");
	i = (s16) (((u16) tmp[1]) << 14) | (((u16) tmp[0]) << 6);
	q = (s16) (((u16) tmp[3]) << 14) | (((u16) tmp[2]) << 6);

	return sprintf(buf, "%d %d\n", i, q);
}
#endif

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
		case DDB_TUNER_XO2 ... DDB_TUNER_DVBC2T2_ST:
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
	unsigned val;

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

	if (dev->port[num].type >= DDB_TUNER_XO2) {
		if (i2c_read_regs(&dev->i2c[num].adap, 0x10, 0x10, snr, 16) < 0)
			return sprintf(buf, "NO SNR\n");
		snr[16] = 0;
	} else {
		/* serial number at 0x100-0x11f */
		if (i2c_read_regs16(&dev->i2c[num].adap,
				    0x57, 0x100, snr, 32) < 0)
			if (i2c_read_regs16(&dev->i2c[num].adap,
					    0x50, 0x100, snr, 32) < 0)
				return sprintf(buf, "NO SNR\n");
		snr[31] = 0; /* in case it is not terminated on EEPROM */
	}
	return sprintf(buf, "%s\n", snr);
}


static ssize_t snr_store(struct device *device, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ddb *dev = dev_get_drvdata(device);
	int num = attr->attr.name[3] - 0x30;
	u8 snr[34] = { 0x01, 0x00 };

	return 0; /* NOE: remove completely? */
	if (count > 31)
		return -EINVAL;
	if (dev->port[num].type >= DDB_TUNER_XO2)
		return -EINVAL;
	memcpy(snr + 2, buf, count);
	i2c_write(&dev->i2c[num].adap, 0x57, snr, 34);
	i2c_write(&dev->i2c[num].adap, 0x50, snr, 34);
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
	char snr[32];
	
	if (!dev->i2c_num)
		return 0;
	
	if (i2c_read_regs16(&dev->i2c[0].adap,
			    0x50, 0x0000, snr, 32) < 0 ||
	    snr[0] == 0xff) 
		return sprintf(buf, "NO SNR\n");
	snr[31] = 0; /* in case it is not terminated on EEPROM */
	return sprintf(buf, "%s\n", snr);
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
	pr_info("redirect: %02x, %02x\n", i, p);
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
	if (val > 20)
		return -EINVAL;
	dev->port[num].gap = val;
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
	if (val > 3)
		return -EINVAL;
	lnb_init_fmode(dev, &dev->link[num], val);
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
#if 0
	__ATTR_RO(qam),
#endif
	__ATTR(redirect, 0664, redirect_show, redirect_store),
	__ATTR_MRO(snr,  bsnr_show),
	__ATTR_RO(bpsnr),
	__ATTR_NULL,
};

static struct device_attribute ddb_attrs_temp[] = {
	__ATTR_RO(temp),
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

static struct class ddb_class = {
	.name		= "ddbridge",
	.owner          = THIS_MODULE,
#if 0
	.dev_attrs	= ddb_attrs,
#endif
	.devnode        = ddb_devnode,
};

static int ddb_class_create(void)
{
	ddb_major = register_chrdev(0, DDB_NAME, &ddb_fops);
	if (ddb_major < 0)
		return ddb_major;
	if (class_register(&ddb_class) < 0)
		return -1;
	return 0;
}

static void ddb_class_destroy(void)
{
	class_unregister(&ddb_class);
	unregister_chrdev(ddb_major, DDB_NAME);
}

static void ddb_device_attrs_del(struct ddb *dev)
{
	int i;

	for (i = 0; i < dev->link[0].info->temp_num; i++)
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
	for (i = 0; ddb_attrs[i].attr.name; i++)
		device_remove_file(dev->ddb_dev, &ddb_attrs[i]);
}

static int ddb_device_attrs_add(struct ddb *dev)
{
	int i;

	for (i = 0; ddb_attrs[i].attr.name; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs[i]))
			goto fail;
	for (i = 0; i < dev->link[0].info->temp_num; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs_temp[i]))
			goto fail;
	for (i = 0; i < dev->link[0].info->port_num; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs_mod[i]))
			goto fail;
	for (i = 0; i < dev->link[0].info->fan_num; i++)
		if (device_create_file(dev->ddb_dev, &ddb_attrs_fan[i]))
			goto fail;
	for (i = 0; i < dev->i2c_num && i < 4; i++) {
		if (device_create_file(dev->ddb_dev, &ddb_attrs_snr[i]))
			goto fail;
		if (device_create_file(dev->ddb_dev, &ddb_attrs_ctemp[i]))
			goto fail;
		if (dev->link[0].info->led_num)
			if (device_create_file(dev->ddb_dev,
					       &ddb_attrs_led[i]))
				goto fail;
	}
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
	ddbs[dev->nr] = dev;
	dev->ddb_dev = device_create(&ddb_class, dev->dev,
				     MKDEV(ddb_major, dev->nr),
				     dev, "ddbridge%d", dev->nr);
	if (IS_ERR(dev->ddb_dev)) {
		res = PTR_ERR(dev->ddb_dev);
		pr_info("Could not create ddbridge%d\n", dev->nr);
		goto fail;
	}
	res = ddb_device_attrs_add(dev);
	if (res) {
		ddb_device_attrs_del(dev);
		device_destroy(&ddb_class, MKDEV(ddb_major, dev->nr));
		ddbs[dev->nr] = 0;
		dev->ddb_dev = ERR_PTR(-ENODEV);
	} else
		ddb_num++;
fail:
	mutex_unlock(&ddb_mutex);
	return res;
}

static void ddb_device_destroy(struct ddb *dev)
{
	if (IS_ERR(dev->ddb_dev))
		return;
	ddb_device_attrs_del(dev);
	device_destroy(&ddb_class, MKDEV(ddb_major, dev->nr));
}

#define LINK_IRQ_HANDLE(_nr)				  \
	do { if ((s & (1UL << _nr)) && dev->handler[_nr + off]) \
		dev->handler[_nr + off](dev->handler_data[_nr + off]); } \
	while (0)

static void gtl_link_handler(unsigned long priv)
{
	struct ddb *dev = (struct ddb *) priv;
	u32 regs = dev->link[0].info->regmap->gtl->base;
	
	printk("GT link change: %u\n",
	       (1 & ddbreadl(dev, regs)));
}

static void link_tasklet(unsigned long data)
{
	struct ddb_link *link = (struct ddb_link *) data;
	struct ddb *dev = link->dev;
	u32 s, off = 32 * link->nr, tag = DDB_LINK_TAG(link->nr);
	
	s = ddbreadl(dev, tag | INTERRUPT_STATUS);
	printk("gtl_irq %08x = %08x\n", tag | INTERRUPT_STATUS, s);
	
	if (!s)
		return;
	ddbwritel(dev, s, tag | INTERRUPT_ACK);
	LINK_IRQ_HANDLE(0);
	LINK_IRQ_HANDLE(1);
	LINK_IRQ_HANDLE(2);
	LINK_IRQ_HANDLE(3);
}

static void gtl_irq_handler(unsigned long priv)
{
	struct ddb_link *link = (struct ddb_link *) priv;
#if 1
	struct ddb *dev = link->dev;
	u32 s, off = 32 * link->nr, tag = DDB_LINK_TAG(link->nr);
	
	while ((s = ddbreadl(dev, tag | INTERRUPT_STATUS)))  {
		ddbwritel(dev, s, tag | INTERRUPT_ACK);
		LINK_IRQ_HANDLE(0);
		LINK_IRQ_HANDLE(1);
		LINK_IRQ_HANDLE(2);
		LINK_IRQ_HANDLE(3);
	}
#else
	printk("gtlirq\n");
	tasklet_schedule(&link->tasklet);
#endif
}

static struct ddb_regset octopus_max_gtl_i2c = {
	.base = 0x80,
	.num  = 0x01,
	.size = 0x20,
};

static struct ddb_regset octopus_max_gtl_i2c_buf = {
	.base = 0x1000,
	.num  = 0x01,
	.size = 0x200,
};

static struct ddb_regmap octopus_max_gtl_map = {
	.i2c = &octopus_max_gtl_i2c,
	.i2c_buf = &octopus_max_gtl_i2c_buf,
};

static struct ddb_info octopus_max_gtl = {
	.type     = DDB_OCTOPUS_MAX,
	.name     = "Digital Devices Octopus MAX GTL",
	.regmap   = &octopus_max_gtl_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control = 1,
};


static struct ddb_regset octopus_maxct_gtl_i2c = {
	.base = 0x80,
	.num  = 0x04,
	.size = 0x20,
};

static struct ddb_regset octopus_maxct_gtl_i2c_buf = {
	.base = 0x1000,
	.num  = 0x04,
	.size = 0x200,
};

static struct ddb_regmap octopus_maxct_gtl_map = {
	.i2c = &octopus_maxct_gtl_i2c,
	.i2c_buf = &octopus_maxct_gtl_i2c_buf,
};

static struct ddb_info octopus_ct_gtl = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices Octopus MAX CT GTL",
	.regmap   = &octopus_maxct_gtl_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control = 0xff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
};


static int ddb_gtl_init_link(struct ddb *dev, u32 l)
{
	struct ddb_link *link = &dev->link[l];
	u32 regs = dev->link[0].info->regmap->gtl->base +
		(l - 1) * dev->link[0].info->regmap->gtl->size;
	u32 id;
	
	printk("Checking GT link %u: regs = %08x\n", l, regs);
	
	spin_lock_init(&link->lock);
	mutex_init(&link->lnb.lock);
	link->lnb.fmode = 0xffffffff;
	mutex_init(&link->flash_mutex);

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
		if (c == 5)
			return -1;
	}
	link->nr = l;
	link->dev = dev;
	link->regs = regs;
	
	id = ddbreadl(dev, DDB_LINK_TAG(l) | 8);
	switch (id) {
	case 0x0007dd01:
		link->info = &octopus_max_gtl;
		break;
	case 0x0008dd01:
		link->info = &octopus_ct_gtl;
		break;
	default:
		pr_info("DDBridge: Detected GT link but found invalid ID %08x. "
			"You might have to update (flash) the add-on card first.",
			id);
		return -1;
	}
	link->ids.devid = id;
	
	ddbwritel(dev, 1, 0x1a0);

	dev->handler_data[11] = (unsigned long) link;
	dev->handler[11] = gtl_irq_handler;
	
	pr_info("GTL %s\n", dev->link[l].info->name);
	pr_info("GTL HW %08x REGMAP %08x\n",
		ddbreadl(dev, DDB_LINK_TAG(l) | 0),
		ddbreadl(dev, DDB_LINK_TAG(l) | 4));
	pr_info("GTL ID %08x\n",
		ddbreadl(dev, DDB_LINK_TAG(l) | 8));
	
	tasklet_init(&link->tasklet, link_tasklet, (unsigned long) link);
	ddbwritel(dev, 0xffffffff, DDB_LINK_TAG(l) | INTERRUPT_ACK);
	ddbwritel(dev, 0xf, DDB_LINK_TAG(l) | INTERRUPT_ENABLE);

	return 0;
}

static int ddb_gtl_init(struct ddb *dev)
{
	u32 l;
	
	dev->handler_data[10] = (unsigned long) dev;
	dev->handler[10] = gtl_link_handler;

	for (l = 1; l < dev->link[0].info->regmap->gtl->num + 1; l++) {
		ddb_gtl_init_link(dev, l);
	}
	return 0;
}

static int ddb_init_boards(struct ddb *dev)
{
	struct ddb_info *info;
	u32 l;
	
	for (l = 0; l < DDB_MAX_LINK; l++) {
		info = dev->link[l].info;
		if (!info)
			continue;
		if (info->board_control) {
			ddbwritel(dev, 0, DDB_LINK_TAG(l) | BOARD_CONTROL);
			msleep(100);
			ddbwritel(dev, info->board_control_2, DDB_LINK_TAG(l) | BOARD_CONTROL);
			usleep_range(2000, 3000);
			ddbwritel(dev, info->board_control_2 | info->board_control,
				  DDB_LINK_TAG(l) | BOARD_CONTROL);
			usleep_range(2000, 3000);
		}
	}
	return 0;
}

static int ddb_init(struct ddb *dev)
{
	if (dev->link[0].info->ns_num) {
		ddbwritel(dev, 1, ETHER_CONTROL);
		dev->vlan = vlan;
		ddbwritel(dev, 14 + (dev->vlan ? 4 : 0), ETHER_LENGTH);
	}

	mutex_init(&dev->link[0].lnb.lock);
	mutex_init(&dev->link[0].flash_mutex);

	if (dev->link[0].info->regmap->gtl)
		ddb_gtl_init(dev);

	ddb_init_boards(dev);

	if (ddb_i2c_init(dev) < 0)
		goto fail;
	ddb_ports_init(dev);
	if (ddb_buffers_alloc(dev) < 0) {
		pr_info(": Could not allocate buffer memory\n");
		goto fail2;
	}
#if 0
	if (ddb_ports_attach(dev) < 0)
		goto fail3;
#else
	ddb_ports_attach(dev);
#endif
	ddb_nsd_attach(dev);

	ddb_device_create(dev);

	if (dev->link[0].info->fan_num)	{
		ddbwritel(dev, 1, GPIO_DIRECTION);
		ddbwritel(dev, 1, GPIO_OUTPUT);
	}
	if (dev->link[0].info->type == DDB_MOD)
		ddbridge_mod_init(dev);
	return 0;

fail3:
	ddb_ports_detach(dev);
	pr_err("fail3\n");
	ddb_ports_release(dev);
fail2:
	pr_err("fail2\n");
	ddb_buffers_free(dev);
	ddb_i2c_release(dev);
fail:
	pr_err("fail1\n");
	return -1;
}
