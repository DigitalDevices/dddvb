/*
 * octonet.c: Digital Devices network tuner driver
 *
 * Copyright (C) 2012-17 Digital Devices GmbH
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
 * along with this program; if not, point your browser to
 *  http://www.gnu.org/copyleft/gpl.html
 */

#include "ddbridge.h"
#include "ddbridge-io.h"

#if (KERNEL_VERSION(6, 11, 0) > LINUX_VERSION_CODE)
static int __exit octonet_remove(struct platform_device *pdev)
#else
static void __exit octonet_remove(struct platform_device *pdev)
#endif
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
	ddb_unmap(dev);
	platform_set_drvdata(pdev, 0);
#if (KERNEL_VERSION(6, 11, 0) > LINUX_VERSION_CODE)
	return 0;
#endif
}

static int __init octonet_probe(struct platform_device *pdev)
{
	struct ddb *dev;
	struct resource *regs;
	int irq;

	dev = vzalloc(sizeof(*dev));
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
		 (u32)regs->start, (u32)dev->regs_len);
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
	dev->link[0].info = get_ddb_info(dev->link[0].ids.vendor,
					 dev->link[0].ids.device,
					 0xdd01, 0xffff);
	dev_info(dev->dev, "DDBridge: HW  %08x REGMAP %08x\n",
		 dev->link[0].ids.hwid, dev->link[0].ids.regmapid);
	dev_info(dev->dev, "DDBridge: MAC %08x DEVID  %08x\n",
		 dev->link[0].ids.mac, dev->link[0].ids.devid);

	ddbwritel(dev, 0, ETHER_CONTROL);
	ddbwritel(dev, 0x00000000, INTERRUPT_ENABLE);
	ddbwritel(dev, 0xffffffff, INTERRUPT_STATUS);
	ddb_reset_ios(dev);

	irq = platform_get_irq(dev->pfdev, 0);
	if (irq < 0)
		goto fail;
	if (request_irq(irq, ddb_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"octonet-dvb", (void *)dev) < 0)
		goto fail;
	ddbwritel(dev, 0x0fffff0f, INTERRUPT_ENABLE);

	if (ddb_init(dev) == 0)
		return 0;

fail:
	dev_err(dev->dev, "fail\n");
	ddbwritel(dev, 0, ETHER_CONTROL);
	ddbwritel(dev, 0, INTERRUPT_ENABLE);
	ddb_unmap(dev);
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

static struct platform_driver octonet_driver __refdata = {
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
	int stat;

	pr_info("DDBridge: Digital Devices OctopusNet driver " DDBRIDGE_VERSION
		", Copyright (C) 2010-17 Digital Devices GmbH\n");
	stat = ddb_init_ddbridge();
	if (stat < 0)
		return stat;
	stat = platform_driver_probe(&octonet_driver, octonet_probe);
	if (stat < 0)
		ddb_exit_ddbridge(0, stat);
	return stat;
}

static __exit void exit_octonet(void)
{
	platform_driver_unregister(&octonet_driver);
	ddb_exit_ddbridge(0, 0);
}

module_init(init_octonet);
module_exit(exit_octonet);

MODULE_DESCRIPTION("GPL");
MODULE_AUTHOR("Marcus and Ralph Metzler, Metzler Brothers Systementwicklung GbR");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DDBRIDGE_VERSION);
