/*
 * octonet.c: Digital Devices network tuner driver
 *
 * Copyright (C) 2012-15 Digital Devices GmbH
 *                       Marcus Metzler <mocm@metzlerbros.de>
 *                       Ralph Metzler <rjkm@metzlerbros.de>
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

#include "ddbridge.h"
#include "ddbridge-regs.h"
#include <asm-generic/pci-dma-compat.h>

static int adapter_alloc = 3;
module_param(adapter_alloc, int, 0444);
MODULE_PARM_DESC(adapter_alloc,
"0-one adapter per io, 1-one per tab with io, 2-one per tab, 3-one for all");

#include "ddbridge-core.c"

static struct ddb_regset octopus_i2c = {
	.base = 0x80,
	.num  = 0x04,
	.size = 0x20,
};

static struct ddb_regset octopus_i2c_buf = {
	.base = 0x1000,
	.num  = 0x04,
	.size = 0x200,
};

static struct ddb_regmap octopus_net_map = {
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
};

static struct ddb_regset octopus_gtl = {
	.base = 0x180,
	.num  = 0x01,
	.size = 0x20,
};

static struct ddb_regmap octopus_net_gtl = {
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.gtl = &octopus_gtl,
};

static struct ddb_info ddb_octonet = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 12,
	.mdio_num = 1,
};

static struct ddb_info ddb_octonet_jse = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter JSE",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 15,
	.mdio_num = 1,
};

static struct ddb_info ddb_octonet_gtl = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet GTL",
	.regmap   = &octopus_net_gtl,
	.port_num = 4,
	.i2c_mask = 0x05,
	.ns_num   = 12,
	.mdio_num = 1,
	.con_clock = 1,
};

static struct ddb_info ddb_octonet_tbd = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet",
	.regmap   = &octopus_net_map,
};

static void octonet_unmap(struct ddb *dev)
{
	if (dev->regs)
		iounmap(dev->regs);
	vfree(dev);
}

static int __exit octonet_remove(struct platform_device *pdev)
{
	struct ddb *dev;

	dev = platform_get_drvdata(pdev);

	ddb_device_destroy(dev);
	ddb_nsd_detach(dev);
	ddb_ports_detach(dev);
	ddb_i2c_release(dev);

	if (dev->link[0].info->ns_num)
		ddbwritel(dev, 0, ETHER_CONTROL);
	ddbwritel(dev, 0, INTERRUPT_ENABLE);

	free_irq(platform_get_irq(dev->pfdev, 0), dev);
	ddb_ports_release(dev);
	octonet_unmap(dev);
	platform_set_drvdata(pdev, 0);
	return 0;
}

static int __init octonet_probe(struct platform_device *pdev)
{
	struct ddb *dev;
	struct resource *regs;
	int irq;
	int i;

	dev = vzalloc(sizeof(struct ddb));
	if (!dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, dev);
	dev->dev = &pdev->dev;
	dev->pfdev = pdev;

	mutex_init(&dev->mutex);
	regs = platform_get_resource(dev->pfdev, IORESOURCE_MEM, 0);
	if (!regs)
		return -ENXIO;
	dev->regs_len = (regs->end - regs->start) + 1;
	dev_info(dev->dev, "regs_start=%08x regs_len=%08x\n",
		 (u32) regs->start, (u32) dev->regs_len);
	dev->regs = ioremap(regs->start, dev->regs_len);

	if (!dev->regs) {
		dev_err(dev->dev, "ioremap failed\n");
		return -ENOMEM;
	}

	dev->link[0].ids.hwid = ddbreadl(dev, 0);
	dev->link[0].ids.regmapid = ddbreadl(dev, 4);
	dev->link[0].ids.devid = ddbreadl(dev, 8);
	dev->link[0].ids.mac = ddbreadl(dev, 12);

	dev->link[0].ids.vendor = dev->link[0].ids.devid & 0xffff;
	dev->link[0].ids.device = dev->link[0].ids.devid >> 16;
	dev->link[0].ids.subvendor = dev->link[0].ids.devid & 0xffff;
	dev->link[0].ids.subdevice = dev->link[0].ids.devid >> 16;

	dev->link[0].dev = dev;
	if (dev->link[0].ids.devid == 0x0300dd01)
		dev->link[0].info = &ddb_octonet;
	else if (dev->link[0].ids.devid == 0x0301dd01)
		dev->link[0].info = &ddb_octonet_jse;
	else if (dev->link[0].ids.devid == 0x0307dd01)
		dev->link[0].info = &ddb_octonet_gtl;
	else
		dev->link[0].info = &ddb_octonet_tbd;
		
	pr_info("HW  %08x REGMAP %08x\n", dev->link[0].ids.hwid, dev->link[0].ids.regmapid);
	pr_info("MAC %08x DEVID  %08x\n", dev->link[0].ids.mac, dev->link[0].ids.devid);

	ddbwritel(dev, 0, ETHER_CONTROL);
	ddbwritel(dev, 0x00000000, INTERRUPT_ENABLE);
	ddbwritel(dev, 0xffffffff, INTERRUPT_STATUS);
	for (i = 0; i < 16; i++)
		ddbwritel(dev, 0x00, TS_OUTPUT_CONTROL(i));
	usleep_range(5000, 6000);

	irq = platform_get_irq(dev->pfdev, 0);
	if (irq < 0)
		goto fail;
	if (request_irq(irq, irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"octonet-dvb", (void *) dev) < 0)
		goto fail;
	ddbwritel(dev, 0x0fffff0f, INTERRUPT_ENABLE);

	if (ddb_init(dev) == 0)
		return 0;

fail:
	dev_err(dev->dev, "fail\n");
	ddbwritel(dev, 0, ETHER_CONTROL);
	ddbwritel(dev, 0, INTERRUPT_ENABLE);
	octonet_unmap(dev);
	platform_set_drvdata(pdev, 0);
	return -1;
}

#ifdef CONFIG_OF
static const struct of_device_id octonet_dt_ids[] = {
	{ .compatible = "digitaldevices,octonet-dvb" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, octonet_dt_ids);
#endif

static struct platform_driver octonet_driver = {
	.remove	= __exit_p(octonet_remove),
	.probe	= octonet_probe,
	.driver		= {
		.name	= "octonet-dvb",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(octonet_dt_ids),
#endif
	},
};

static __init int init_octonet(void)
{
	int res;

	pr_info("Digital Devices OctopusNet driver " DDBRIDGE_VERSION
		", Copyright (C) 2010-15 Digital Devices GmbH\n");
	res = ddb_class_create();
	if (res)
		return res;
	res = platform_driver_probe(&octonet_driver, octonet_probe);
	if (res) {
		ddb_class_destroy();
		return res;
	}
	return 0;
}

static __exit void exit_octonet(void)
{
	platform_driver_unregister(&octonet_driver);
	ddb_class_destroy();
}

module_init(init_octonet);
module_exit(exit_octonet);

MODULE_DESCRIPTION("GPL");
MODULE_AUTHOR("Marcus and Ralph Metzler, Metzler Brothers Systementwicklung GbR");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.6");
