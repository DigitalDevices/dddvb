/*
 * ddbridge.c: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
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

#define DDB_USE_WORK
/*#define DDB_TEST_THREADED*/

#include "ddbridge.h"
#include "ddbridge-regs.h"
#include "ddbridge-core.c"

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void __devexit ddb_irq_disable(struct ddb *dev)
{
	if (dev->link[0].info->regmap->irq_version == 2) {
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_CONTROL);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_1);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_2);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_3);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_4);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_5);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_6);
		ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_7);
	} else {
		ddbwritel(dev, 0, INTERRUPT_ENABLE);
		ddbwritel(dev, 0, MSI1_ENABLE);
	}
}

static void __devexit ddb_irq_exit(struct ddb *dev)
{
	ddb_irq_disable(dev);
	if (dev->msi == 2)
		free_irq(dev->pdev->irq + 1, dev);
	free_irq(dev->pdev->irq, dev);
#ifdef CONFIG_PCI_MSI
	if (dev->msi)
		pci_disable_msi(dev->pdev);
#endif
}

static void __devexit ddb_remove(struct pci_dev *pdev)
{
	struct ddb *dev = (struct ddb *) pci_get_drvdata(pdev);

	ddb_device_destroy(dev);
	ddb_nsd_detach(dev);
	ddb_ports_detach(dev);
	ddb_i2c_release(dev);

	if (dev->link[0].info->ns_num)
		ddbwritel(dev, 0, ETHER_CONTROL);
	ddb_irq_exit(dev);
	ddb_ports_release(dev);
	ddb_buffers_free(dev);

	ddb_unmap(dev);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
#define __devinit
#define __devinitdata
#endif

static int __devinit ddb_irq_msi(struct ddb *dev, int nr)
{
	int stat = 0;

#ifdef CONFIG_PCI_MSI
	if (msi && pci_msi_enabled()) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
		stat = pci_alloc_irq_vectors(dev->pdev, 1, nr, PCI_IRQ_MSI);
#else
		stat = pci_enable_msi_range(dev->pdev, 1, nr);
#endif
		if (stat >= 1) {
			dev->msi = stat;
			pr_info("DDBridge: using %d MSI interrupt(s)\n",
				dev->msi);
		} else
			pr_info("DDBridge: MSI not available.\n");

#else
		stat = pci_enable_msi_block(dev->pdev, nr);
		if (stat == 0) {
			dev->msi = nr;
			pr_info("DDBridge: using %d MSI interrupts\n", nr);
		} else if (stat == 1) {
			stat = pci_enable_msi(dev->pdev);
			dev->msi = 1;
		}
		if (stat < 0)
			pr_info("DDBridge: MSI not available.\n");
#endif
	}
#endif	
	return stat;
}

static int __devinit ddb_irq_init2(struct ddb *dev)
{
	int stat;
	int irq_flag = IRQF_SHARED;

	pr_info("DDBridge: init type 2 IRQ hardware block\n");

	ddbwritel(dev, 0x00000000, INTERRUPT_V2_CONTROL);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_1);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_2);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_3);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_4);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_5);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_6);
	ddbwritel(dev, 0x00000000, INTERRUPT_V2_ENABLE_7);

	ddb_irq_msi(dev, 1);
	if (dev->msi)
		irq_flag = 0;

	stat = request_irq(dev->pdev->irq, irq_handler_v2,
			   irq_flag, "ddbridge", (void *) dev);
	if (stat < 0)
		return stat;

	ddbwritel(dev, 0x0000ff7f, INTERRUPT_V2_CONTROL);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_1);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_2);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_3);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_4);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_5);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_6);
	ddbwritel(dev, 0xffffffff, INTERRUPT_V2_ENABLE_7);
	return stat;
}

static int __devinit ddb_irq_init(struct ddb *dev)
{
	int stat;
	int irq_flag = IRQF_SHARED;

	if (dev->link[0].info->regmap->irq_version == 2)
		return ddb_irq_init2(dev);

	ddbwritel(dev, 0x00000000, INTERRUPT_ENABLE);
	ddbwritel(dev, 0x00000000, MSI1_ENABLE);
	ddbwritel(dev, 0x00000000, MSI2_ENABLE);
	ddbwritel(dev, 0x00000000, MSI3_ENABLE);
	ddbwritel(dev, 0x00000000, MSI4_ENABLE);
	ddbwritel(dev, 0x00000000, MSI5_ENABLE);
	ddbwritel(dev, 0x00000000, MSI6_ENABLE);
	ddbwritel(dev, 0x00000000, MSI7_ENABLE);

	ddb_irq_msi(dev, 2);

	if (dev->msi)
		irq_flag = 0;
	if (dev->msi == 2) {
		stat = request_irq(dev->pdev->irq, irq_handler0,
				   irq_flag, "ddbridge", (void *) dev);
		if (stat < 0)
			return stat;
		stat = request_irq(dev->pdev->irq + 1, irq_handler1,
				   irq_flag, "ddbridge", (void *) dev);
		if (stat < 0) {
			free_irq(dev->pdev->irq, dev);
			return stat;
		}
	} else {
#ifdef DDB_TEST_THREADED
		stat = request_threaded_irq(dev->pdev->irq, irq_handler,
					    irq_thread,
					    irq_flag,
					    "ddbridge", (void *) dev);
#else
		stat = request_irq(dev->pdev->irq, irq_handler,
				   irq_flag, "ddbridge", (void *) dev);
#endif
		if (stat < 0)
			return stat;
	}
	/*ddbwritel(dev, 0xffffffff, INTERRUPT_ACK);*/
	if (dev->msi == 2) {
		ddbwritel(dev, 0x0fffff00, INTERRUPT_ENABLE);
		ddbwritel(dev, 0x0000000f, MSI1_ENABLE);
	} else {
		ddbwritel(dev, 0x0fffff0f, INTERRUPT_ENABLE);
		ddbwritel(dev, 0x00000000, MSI1_ENABLE);
	}
	return stat;
}

static int __devinit ddb_probe(struct pci_dev *pdev,
			       const struct pci_device_id *id)
{
	struct ddb *dev;
	int stat = 0;

	if (pci_enable_device(pdev) < 0)
		return -ENODEV;

	pci_set_master(pdev);

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)))
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)))
			return -ENODEV;

	dev = vzalloc(sizeof(struct ddb));
	if (dev == NULL)
		return -ENOMEM;

	mutex_init(&dev->mutex);
	dev->has_dma = 1;
	dev->pdev = pdev;
	dev->dev = &pdev->dev;
	pci_set_drvdata(pdev, dev);

	dev->link[0].ids.vendor = id->vendor;
	dev->link[0].ids.device = id->device;
	dev->link[0].ids.subvendor = id->subvendor;
	dev->link[0].ids.subdevice = pdev->subsystem_device;
	
	dev->link[0].dev = dev;
	dev->link[0].info = get_ddb_info(id->vendor, id->device,
					 id->subvendor, pdev->subsystem_device);
	pr_info("DDBridge: device name: %s\n", dev->link[0].info->name);

	dev->regs_len = pci_resource_len(dev->pdev, 0);
	dev->regs = ioremap(pci_resource_start(dev->pdev, 0),
			    pci_resource_len(dev->pdev, 0));

	if (!dev->regs) {
		pr_err("DDBridge: not enough memory for register map\n");
		stat = -ENOMEM;
		goto fail;
	}
	if (ddbreadl(dev, 0) == 0xffffffff) {
		pr_err("DDBridge: cannot read registers\n");
		stat = -ENODEV;
		goto fail;
	}

	dev->link[0].ids.hwid = ddbreadl(dev, 0);
	dev->link[0].ids.regmapid = ddbreadl(dev, 4);

	pr_info("DDBridge: HW %08x REGMAP %08x\n",
		dev->link[0].ids.hwid, dev->link[0].ids.regmapid);

	if (dev->link[0].info->ns_num) {
		ddbwritel(dev, 0, ETHER_CONTROL);
		ddb_reset_ios(dev);
	}
	ddbwritel(dev, 0, DMA_BASE_READ);
	if (dev->link[0].info->type != DDB_MOD)
		ddbwritel(dev, 0, DMA_BASE_WRITE);

	if (dev->link[0].info->type == DDB_MOD) {
		if (dev->link[0].info->version == 1)
			if (ddbreadl(dev, 0x1c) == 4)
				dev->link[0].info->port_num = 4;
	}

	stat = ddb_irq_init(dev);
	if (stat < 0)
		goto fail0;

	if (ddb_init(dev) == 0)
		return 0;

	ddb_irq_exit(dev);
fail0:
	pr_err("DDBridge: fail0\n");
	if (dev->msi)
		pci_disable_msi(dev->pdev);
fail:
	pr_err("DDBridge: fail\n");

	ddb_unmap(dev);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
	return -1;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#define DDB_DEVICE_ANY(_device) { PCI_DEVICE_SUB(0xdd01, _device, 0xdd01, PCI_ANY_ID) }

static const struct pci_device_id ddb_id_table[] __devinitconst = {
	DDB_DEVICE_ANY(0x0002),
	DDB_DEVICE_ANY(0x0003),
	DDB_DEVICE_ANY(0x0005),
	DDB_DEVICE_ANY(0x0006),
	DDB_DEVICE_ANY(0x0007),
	DDB_DEVICE_ANY(0x0008),
	DDB_DEVICE_ANY(0x0011),
	DDB_DEVICE_ANY(0x0012),
	DDB_DEVICE_ANY(0x0013),
	DDB_DEVICE_ANY(0x0201),
	DDB_DEVICE_ANY(0x0203),
	DDB_DEVICE_ANY(0x0210),
	DDB_DEVICE_ANY(0x0220),
	DDB_DEVICE_ANY(0x0320),
	DDB_DEVICE_ANY(0x0321),
	DDB_DEVICE_ANY(0x0322),
	DDB_DEVICE_ANY(0x0323),
	DDB_DEVICE_ANY(0x0328),
	DDB_DEVICE_ANY(0x0329),
	{0}
};
MODULE_DEVICE_TABLE(pci, ddb_id_table);

static struct pci_driver ddb_pci_driver = {
	.name        = "ddbridge",
	.id_table    = ddb_id_table,
	.probe       = ddb_probe,
	.remove      = ddb_remove,
};

static __init int module_init_ddbridge(void)
{
	int stat = -1;

	pr_info("DDBridge: Digital Devices PCIE bridge driver "
		DDBRIDGE_VERSION
		", Copyright (C) 2010-17 Digital Devices GmbH\n");
	if (ddb_class_create() < 0)
		return -1;
	ddb_wq = create_workqueue("ddbridge");
	if (ddb_wq == NULL)
		goto exit1;
	stat = pci_register_driver(&ddb_pci_driver);
	if (stat < 0)
		goto exit2;
	return stat;
exit2:
	destroy_workqueue(ddb_wq);
exit1:
	ddb_class_destroy();
	return stat;
}

static __exit void module_exit_ddbridge(void)
{
	pci_unregister_driver(&ddb_pci_driver);
	destroy_workqueue(ddb_wq);
	ddb_class_destroy();
}

module_init(module_init_ddbridge);
module_exit(module_exit_ddbridge);

MODULE_DESCRIPTION("Digital Devices PCIe Bridge");
MODULE_AUTHOR("Ralph and Marcus Metzler, Metzler Brothers Systementwicklung GbR");
MODULE_LICENSE("GPL");
MODULE_VERSION(DDBRIDGE_VERSION);
