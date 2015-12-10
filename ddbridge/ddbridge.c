/*
 * ddbridge.c: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2015 Digital Devices GmbH  
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

/*#define DDB_ALT_DMA*/
#define DDB_USE_WORK
/*#define DDB_TEST_THREADED*/

#include "ddbridge.h"
#include "ddbridge-regs.h"

static struct workqueue_struct *ddb_wq;

static int adapter_alloc;
module_param(adapter_alloc, int, 0444);
MODULE_PARM_DESC(adapter_alloc,
		 "0-one adapter per io, 1-one per tab with io, 2-one per tab, 3-one for all");

#ifdef CONFIG_PCI_MSI
static int msi = 1;
module_param(msi, int, 0444);
MODULE_PARM_DESC(msi,
		 " Control MSI interrupts: 0-disable, 1-enable (default)");
#endif

#include "ddbridge-core.c"

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static void ddb_unmap(struct ddb *dev)
{
	if (dev->regs)
		iounmap(dev->regs);
	vfree(dev);
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
	ddbwritel(dev, 0, INTERRUPT_ENABLE);

	ddbwritel(dev, 0, MSI1_ENABLE);
	if (dev->msi == 2)
		free_irq(dev->pdev->irq + 1, dev);
	free_irq(dev->pdev->irq, dev);
#ifdef CONFIG_PCI_MSI
	if (dev->msi)
		pci_disable_msi(dev->pdev);
#endif
	ddb_ports_release(dev);
	ddb_buffers_free(dev);

	ddb_unmap(dev);
	pci_set_drvdata(pdev, 0);
	pci_disable_device(pdev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0))
#define __devinit
#define __devinitdata
#endif

static int __devinit ddb_probe(struct pci_dev *pdev,
			       const struct pci_device_id *id)
{
	struct ddb *dev;
	int stat = 0;
	int irq_flag = IRQF_SHARED;

	if (pci_enable_device(pdev) < 0)
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
	dev->link[0].ids.subdevice = id->subdevice;

	dev->link[0].dev = dev;
	dev->link[0].info = (struct ddb_info *) id->driver_data;
	pr_info("DDBridge driver detected: %s\n", dev->link[0].info->name);

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
		int i;

		ddbwritel(dev, 0, ETHER_CONTROL);
		for (i = 0; i < 16; i++)
			ddbwritel(dev, 0x00, TS_OUTPUT_CONTROL(i));
		usleep_range(5000, 6000);
	}
	ddbwritel(dev, 0x00000000, INTERRUPT_ENABLE);
	ddbwritel(dev, 0x00000000, MSI1_ENABLE);
	ddbwritel(dev, 0x00000000, MSI2_ENABLE);
	ddbwritel(dev, 0x00000000, MSI3_ENABLE);
	ddbwritel(dev, 0x00000000, MSI4_ENABLE);
	ddbwritel(dev, 0x00000000, MSI5_ENABLE);
	ddbwritel(dev, 0x00000000, MSI6_ENABLE);
	ddbwritel(dev, 0x00000000, MSI7_ENABLE);

#ifdef CONFIG_PCI_MSI
	if (msi && pci_msi_enabled()) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
		stat = pci_enable_msi_range(dev->pdev, 1, 2);
		if (stat >= 1) {
			dev->msi = stat;
			pr_info("DDBridge: using %d MSI interrupt(s)\n", dev->msi);
			irq_flag = 0;
		} else
			pr_info("DDBridge: MSI not available.\n");

#else
		stat = pci_enable_msi_block(dev->pdev, 2);
		if (stat == 0) {
			dev->msi = 1;
			pr_info("DDBridge: using 2 MSI interrupts\n");
		}
		if (stat == 1)
			stat = pci_enable_msi(dev->pdev);
		if (stat < 0) {
			pr_info("DDBridge: MSI not available.\n");
		} else {
			irq_flag = 0;
			dev->msi++;
		}
#endif
	}
	if (dev->msi == 2) {
		stat = request_irq(dev->pdev->irq, irq_handler0,
				   irq_flag, "ddbridge", (void *) dev);
		if (stat < 0)
			goto fail0;
		stat = request_irq(dev->pdev->irq + 1, irq_handler1,
				   irq_flag, "ddbridge", (void *) dev);
		if (stat < 0) {
			free_irq(dev->pdev->irq, dev);
			goto fail0;
		}
	} else
#endif
	{
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
			goto fail0;
	}
	ddbwritel(dev, 0, DMA_BASE_READ);
	if (dev->link[0].info->type != DDB_MOD)
		ddbwritel(dev, 0, DMA_BASE_WRITE);
	
	if (dev->link[0].info->type == DDB_MOD) {
		if  (ddbreadl(dev, 0x1c) == 4)
			dev->link[0].info->port_num = 4;
	}

	/*ddbwritel(dev, 0xffffffff, INTERRUPT_ACK);*/
	if (dev->msi == 2) {
		ddbwritel(dev, 0x0fffff00, INTERRUPT_ENABLE);
		ddbwritel(dev, 0x0000000f, MSI1_ENABLE);
	} else {
		ddbwritel(dev, 0x0fffff0f, INTERRUPT_ENABLE);
		ddbwritel(dev, 0x00000000, MSI1_ENABLE);
	}
	if (ddb_init(dev) == 0)
		return 0;
	
	ddbwritel(dev, 0, INTERRUPT_ENABLE);
	ddbwritel(dev, 0, MSI1_ENABLE);
	free_irq(dev->pdev->irq, dev);
	if (dev->msi == 2)
		free_irq(dev->pdev->irq + 1, dev);
fail0:
	pr_err("fail0\n");
	if (dev->msi)
		pci_disable_msi(dev->pdev);
fail:
	pr_err("fail\n");

	ddb_unmap(dev);
	pci_set_drvdata(pdev, 0);
	pci_disable_device(pdev);
	return -1;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

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

/****************************************************************************/


static struct ddb_regmap octopus_map = {
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
};

static struct ddb_regmap octopus_net_map = {
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
};

static struct ddb_regmap octopus_mod_map = {
};


/****************************************************************************/

static struct ddb_info ddb_none = {
	.type     = DDB_NONE,
	.name     = "unknown Digital Devices PCIe card, install newer driver",
	.regmap   = &octopus_map,
};

static struct ddb_info ddb_octopus = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static struct ddb_info ddb_octopusv3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static struct ddb_info ddb_octopus_le = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus LE DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 2,
	.i2c_mask = 0x03,
};

static struct ddb_info ddb_octopus_oem = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus OEM",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.led_num  = 1,
	.fan_num  = 1,
	.temp_num = 1,
	.temp_bus = 0,
};

static struct ddb_info ddb_octopus_mini = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus Mini",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static struct ddb_info ddb_v6 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

static struct ddb_info ddb_v6_5 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6.5 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static struct ddb_info ddb_v7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 2,
	.board_control_2 = 4,
	.ts_quirks = TS_QUIRK_REVERSED,
};

static struct ddb_info ddb_ctv7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine CT V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 3,
	.board_control_2 = 4,
};

static struct ddb_info ddb_satixS2v3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Mystique SaTiX-S2 V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

static struct ddb_info ddb_ci = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x03,
};

static struct ddb_info ddb_cis = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI single",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x03,
};

static struct ddb_info ddb_ci_s2_pro = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI S2 Pro",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control   = 2,
	.board_control_2 = 4,
};

static struct ddb_info ddb_dvbct = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices DVBCT V6.1 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

/****************************************************************************/

static struct ddb_info ddb_s2_48 = {
	.type     = DDB_OCTOPUS_MAX,
	.name     = "Digital Devices MAX S8 4/8",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control = 1,
};

static struct ddb_info ddb_ct_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX CT8",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
};

static struct ddb_info ddb_mod = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator",
	.regmap   = &octopus_mod_map,
	.port_num = 10,
	.temp_num = 1,
};

static struct ddb_info ddb_octopus_net = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter",
	.regmap   = &octopus_net_map,
	.port_num = 10,
	.i2c_mask = 0x3ff,
	.ns_num   = 12,
	.mdio_num = 1,
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#define DDVID 0xdd01 /* Digital Devices Vendor ID */

#define DDB_ID(_vend, _dev, _subvend, _subdev, _driverdata) { \
	.vendor      = _vend,    .device    = _dev, \
	.subvendor   = _subvend, .subdevice = _subdev, \
	.driver_data = (unsigned long)&_driverdata }

static const struct pci_device_id ddb_id_tbl[] __devinitconst = {
	DDB_ID(DDVID, 0x0002, DDVID, 0x0001, ddb_octopus),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0001, ddb_octopus),
	DDB_ID(DDVID, 0x0005, DDVID, 0x0004, ddb_octopusv3),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0002, ddb_octopus_le),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0003, ddb_octopus_oem),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0010, ddb_octopus_mini),
	DDB_ID(DDVID, 0x0005, DDVID, 0x0011, ddb_octopus_mini),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0020, ddb_v6),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0021, ddb_v6_5),
	DDB_ID(DDVID, 0x0006, DDVID, 0x0022, ddb_v7),
	DDB_ID(DDVID, 0x0003, DDVID, 0x0030, ddb_dvbct),
	DDB_ID(DDVID, 0x0003, DDVID, 0xdb03, ddb_satixS2v3),
	DDB_ID(DDVID, 0x0006, DDVID, 0x0031, ddb_ctv7),
	DDB_ID(DDVID, 0x0006, DDVID, 0x0032, ddb_ctv7),
	DDB_ID(DDVID, 0x0006, DDVID, 0x0033, ddb_ctv7),
	DDB_ID(DDVID, 0x0007, DDVID, 0x0023, ddb_s2_48),
	DDB_ID(DDVID, 0x0008, DDVID, 0x0034, ddb_ct_8),
	DDB_ID(DDVID, 0x0011, DDVID, 0x0040, ddb_ci),
	DDB_ID(DDVID, 0x0011, DDVID, 0x0041, ddb_cis),
	DDB_ID(DDVID, 0x0012, DDVID, 0x0042, ddb_ci),
	DDB_ID(DDVID, 0x0013, DDVID, 0x0043, ddb_ci_s2_pro),
	DDB_ID(DDVID, 0x0201, DDVID, 0x0001, ddb_mod),
	DDB_ID(DDVID, 0x0201, DDVID, 0x0002, ddb_mod),
	DDB_ID(DDVID, 0x0320, PCI_ANY_ID, PCI_ANY_ID, ddb_octopus_net),
	/* in case sub-ids got deleted in flash */
	DDB_ID(DDVID, 0x0003, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0005, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0006, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0007, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0011, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0013, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0201, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	DDB_ID(DDVID, 0x0320, PCI_ANY_ID, PCI_ANY_ID, ddb_none),
	{0}
};
MODULE_DEVICE_TABLE(pci, ddb_id_tbl);

static struct pci_driver ddb_pci_driver = {
	.name        = "ddbridge",
	.id_table    = ddb_id_tbl,
	.probe       = ddb_probe,
	.remove      = ddb_remove,
};

static __init int module_init_ddbridge(void)
{
	int stat = -1;

	pr_info("Digital Devices PCIE bridge driver "
		DDBRIDGE_VERSION
		", Copyright (C) 2010-15 Digital Devices GmbH\n");
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
