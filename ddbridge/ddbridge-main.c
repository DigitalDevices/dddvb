// SPDX-License-Identifier: GPL-2.0
/*
 * ddbridge.c: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
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
#include "ddbridge-io.h"

#ifdef CONFIG_PCI_MSI
static int msi = 1;
module_param(msi, int, 0444);
MODULE_PARM_DESC(msi,
		 " Control MSI interrupts: 0-disable, 1-enable (default)");
#endif

#if (KERNEL_VERSION(4, 8, 0) > LINUX_VERSION_CODE)
#if (KERNEL_VERSION(3, 19, 0) > LINUX_VERSION_CODE)
#define msi_desc_to_dev(desc) (&(desc)->dev.dev)
#define dev_to_msi_list(dev) (&to_pci_dev((dev))->msi_list)
#define first_msi_entry(dev) \
	list_first_entry(dev_to_msi_list((dev)), struct msi_desc, list)
#define for_each_msi_entry(desc, dev) \
	list_for_each_entry((desc), dev_to_msi_list((dev)), list)

#ifdef CONFIG_PCI_MSI
#define first_pci_msi_entry(pdev) first_msi_entry(&(pdev)->dev)
#define for_each_pci_msi_entry(desc, pdev) \
	for_each_msi_entry((desc), &(pdev)->dev)
#endif

#endif

#include <linux/msi.h>

int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
#ifdef CONFIG_PCI_MSI
	if (dev->msix_enabled) {
		struct msi_desc *entry;
		int i = 0;

		for_each_pci_msi_entry(entry, dev) {
			if (i == nr)
				return entry->irq;
			i++;
		}
		WARN_ON_ONCE(1);
		return -EINVAL;
	}
/* This does not work < 3.19 because nvec_used is used differently. */
#if (KERNEL_VERSION(3, 19, 0) <= LINUX_VERSION_CODE)
	if (dev->msi_enabled) {
		struct msi_desc *entry = first_pci_msi_entry(dev);

		if (WARN_ON_ONCE(nr >= entry->nvec_used))
			return -EINVAL;
	} else {
		if (WARN_ON_ONCE(nr > 0))
			return -EINVAL;
	}
#endif
#endif
	return dev->irq + nr;
}
#endif

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

static void __devexit ddb_msi_exit(struct ddb *dev)
{
#ifdef CONFIG_PCI_MSI
	if (dev->msi) {
#if (KERNEL_VERSION(4, 8, 0) <= LINUX_VERSION_CODE)
		pci_free_irq_vectors(dev->pdev);
#else
		pci_disable_msi(dev->pdev);
#endif
	}
#endif
}

static void __devexit ddb_irq_exit(struct ddb *dev)
{
	ddb_irq_disable(dev);
	if (dev->msi == 2)
		free_irq(pci_irq_vector(dev->pdev, 1), dev);
	free_irq(pci_irq_vector(dev->pdev, 0), dev);
}

static void __devexit ddb_remove(struct pci_dev *pdev)
{
	struct ddb *dev = (struct ddb *)pci_get_drvdata(pdev);

	ddb_device_destroy(dev);
	ddb_nsd_detach(dev);
	ddb_ports_detach(dev);
	ddb_i2c_release(dev);

	if (dev->link[0].info->ns_num)
		ddbwritel(dev, 0, ETHER_CONTROL);
	ddb_irq_exit(dev);
	ddb_msi_exit(dev);
	ddb_ports_release(dev);
	ddb_buffers_free(dev);

	ddb_unmap(dev);
	pci_clear_master(pdev);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
#define __devinit
#define __devinitdata
#endif

static int __devinit ddb_irq_msi(struct ddb *dev, int nr)
{
	int stat = 0;

#ifdef CONFIG_PCI_MSI
	if (msi && pci_msi_enabled()) {
#if (KERNEL_VERSION(3, 15, 0) <= LINUX_VERSION_CODE)
#if (KERNEL_VERSION(4, 8, 0) <= LINUX_VERSION_CODE)
		stat = pci_alloc_irq_vectors(dev->pdev, 1, nr,
					     PCI_IRQ_MSI | PCI_IRQ_MSIX);
#else
		stat = pci_enable_msi_range(dev->pdev, 1, nr);
#endif
		if (stat >= 1) {
			dev->msi = stat;
			dev_info(dev->dev, "using %d MSI interrupt(s)\n",
				 dev->msi);
		} else {
			dev_info(dev->dev, "MSI not available.\n");
		}
#else
		stat = pci_enable_msi_block(dev->pdev, nr);
		if (stat == 0) {
			dev->msi = nr;
			dev_info(dev->dev, "using %d MSI interrupts\n", nr);
		} else if (stat == 1) {
			stat = pci_enable_msi(dev->pdev);
			dev->msi = 1;
		}
		if (stat < 0)
			dev_info(dev->dev, "MSI not available.\n");
#endif
	}
#endif
	return stat;
}

static int __devinit ddb_irq_init2(struct ddb *dev)
{
	int stat;
	int irq_flag = IRQF_SHARED;

	dev_info(dev->dev, "init type 2 IRQ hardware block\n");

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

	stat = request_irq(pci_irq_vector(dev->pdev, 0), ddb_irq_handler_v2,
			   irq_flag, "ddbridge", (void *)dev);
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
		stat = request_irq(pci_irq_vector(dev->pdev, 0), ddb_irq_handler0,
				   irq_flag, "ddbridge", (void *)dev);
		if (stat < 0)
			return stat;
		stat = request_irq(pci_irq_vector(dev->pdev, 1), ddb_irq_handler1,
				   irq_flag, "ddbridge", (void *)dev);
		if (stat < 0) {
			free_irq(pci_irq_vector(dev->pdev, 0), dev);
			return stat;
		}
	} else {
		stat = request_irq(pci_irq_vector(dev->pdev, 0),
				   ddb_irq_handler,
				   irq_flag, "ddbridge", (void *)dev);
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

#if (KERNEL_VERSION(5, 18, 0) <= LINUX_VERSION_CODE)
	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64)))
		if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32)))
#else
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	} else
#endif
		return -ENODEV;

	dev = vzalloc(sizeof(*dev));
	if (!dev)
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
	dev->link[0].ids.devid = (id->device << 16) | id->vendor;
	dev->link[0].ids.revision = pdev->revision;

	dev->link[0].dev = dev;
	dev->link[0].info = get_ddb_info(id->vendor, id->device,
					 id->subvendor, pdev->subsystem_device);

	dev->regs_len = pci_resource_len(dev->pdev, 0);
	dev->regs = ioremap(pci_resource_start(dev->pdev, 0),
			    pci_resource_len(dev->pdev, 0));

	if (!dev->regs) {
		dev_err(dev->dev, "not enough memory for register map\n");
		stat = -ENOMEM;
		goto fail;
	}
	if (ddbreadl(dev, 0) == 0xffffffff) {
		dev_err(dev->dev, "cannot read registers\n");
		stat = -ENODEV;
		goto fail;
	}

	dev->link[0].ids.hwid = ddbreadl(dev, 0);
	dev->link[0].ids.regmapid = ddbreadl(dev, 4);

	if ((dev->link[0].ids.hwid & 0xffffff) <
	    dev->link[0].info->hw_min) {
		u32 min = dev->link[0].info->hw_min;

		dev_err(dev->dev, "Update firmware to at least version %u.%u to ensure full functionality!\n",
			(min & 0xff0000) >> 16, min & 0xffff);
	}

	if (dev->link[0].info->ns_num) {
		ddbwritel(dev, 0, ETHER_CONTROL);
		ddb_reset_ios(dev);
	}
	ddbwritel(dev, 0, DMA_BASE_READ);
	if (dev->link[0].info->type != DDB_MOD)
		ddbwritel(dev, 0, DMA_BASE_WRITE);

	if (dev->link[0].info->type == DDB_MOD &&
	    dev->link[0].info->version <= 1) {
		if (ddbreadl(dev, 0x1c) == 4)
			dev->link[0].info =
				get_ddb_info(0xdd01, 0x0201, 0xdd01, 0x0004);
	}
	if (dev->link[0].info->type == DDB_MOD &&
	    dev->link[0].info->version == 2) {
		u32 lic = ddbreadl(dev, 0x1c) & 7;

		if (dev->link[0].ids.revision == 1)
			lic = ddbreadl(dev, 0x260) >> 24;

		switch (lic) {
		case 0:
		case 4:
			dev->link[0].info =
				get_ddb_info(0xdd01, 0x0210, 0xdd01, 0x0000);
			break;
		case 1:
		case 8:
			dev->link[0].info =
				get_ddb_info(0xdd01, 0x0210, 0xdd01, 0x0003);
			break;
		case 2:
		case 24:
			dev->link[0].info =
				get_ddb_info(0xdd01, 0x0210, 0xdd01, 0x0001);
			break;
		case 3:
		case 16:
			dev->link[0].info =
				get_ddb_info(0xdd01, 0x0210, 0xdd01, 0x0002);
			break;
		default:
			break;
		}
	}
	dev_info(dev->dev, "device name: %s\n", dev->link[0].info->name);
	dev_info(dev->dev, "HW %08x REGMAP %08x FW %u.%u\n",
		 dev->link[0].ids.hwid, dev->link[0].ids.regmapid,
		 (dev->link[0].ids.hwid & 0xff0000) >> 16,
		 dev->link[0].ids.hwid & 0xffff);

	stat = ddb_irq_init(dev);
	if (stat < 0)
		goto fail0;

	if (ddb_init(dev) == 0)
		return 0;

	ddb_irq_exit(dev);
fail0:
	dev_err(dev->dev, "fail0\n");
	ddb_msi_exit(dev);
fail:
	dev_err(dev->dev, "fail\n");

	ddb_unmap(dev);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
	return -1;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#ifndef PCI_DEVICE_SUB
#define PCI_DEVICE_SUB(vend, dev, subvend, subdev) \
	.vendor = (vend), .device = (dev), \
		.subvendor = (subvend), .subdevice = (subdev)
#endif

#define DDB_DEVICE_ANY(_device) \
	{ PCI_DEVICE_SUB(0xdd01, _device, 0xdd01, PCI_ANY_ID) }

static const struct pci_device_id ddb_id_table[] __devinitconst = {
	DDB_DEVICE_ANY(0x0002),
	DDB_DEVICE_ANY(0x0003),
	DDB_DEVICE_ANY(0x0005),
	DDB_DEVICE_ANY(0x0006),
	DDB_DEVICE_ANY(0x0007),
	DDB_DEVICE_ANY(0x0008),
	DDB_DEVICE_ANY(0x0009),
	DDB_DEVICE_ANY(0x000a),
	DDB_DEVICE_ANY(0x000b),
	DDB_DEVICE_ANY(0x0011),
	DDB_DEVICE_ANY(0x0012),
	DDB_DEVICE_ANY(0x0013),
	DDB_DEVICE_ANY(0x0020),
	DDB_DEVICE_ANY(0x0201),
	DDB_DEVICE_ANY(0x0203),
	DDB_DEVICE_ANY(0x0210),
	DDB_DEVICE_ANY(0x0220),
	DDB_DEVICE_ANY(0x0221),
	DDB_DEVICE_ANY(0x0222),
	DDB_DEVICE_ANY(0x0223),
	DDB_DEVICE_ANY(0x0320),
	DDB_DEVICE_ANY(0x0321),
	DDB_DEVICE_ANY(0x0322),
	DDB_DEVICE_ANY(0x0323),
	DDB_DEVICE_ANY(0x0328),
	DDB_DEVICE_ANY(0x0329),
	{0}
};
MODULE_DEVICE_TABLE(pci, ddb_id_table);

static pci_ers_result_t ddb_pci_slot_reset(struct pci_dev *dev)
{
	pr_info("pci_slot_reset\n");
	return PCI_ERS_RESULT_RECOVERED;
}

static void ddb_pci_resume(struct pci_dev *dev)
{
	pr_info("pci_resume\n");
}

static pci_ers_result_t ddb_pci_mmio_enabled(struct pci_dev *pdev)
{
	pr_info("pci_mmio_enabled\n");
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ddb_pci_error_detected(struct pci_dev *pdev,
					       pci_channel_state_t state)
{
	switch (state) {
	case pci_channel_io_frozen:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_perm_failure:
		return PCI_ERS_RESULT_DISCONNECT;
	case pci_channel_io_normal:
	default:
		break;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static const struct pci_error_handlers ddb_error = {
	.error_detected = ddb_pci_error_detected,
	.mmio_enabled = ddb_pci_mmio_enabled,
	.slot_reset = ddb_pci_slot_reset,
	.resume = ddb_pci_resume,
};

static struct pci_driver ddb_pci_driver = {
	.name        = "ddbridge",
	.id_table    = ddb_id_table,
	.probe       = ddb_probe,
	.remove      = ddb_remove,
	.err_handler = &ddb_error,
};

static __init int module_init_ddbridge(void)
{
	int stat;

	pr_info("Digital Devices PCIE bridge driver "
		DDBRIDGE_VERSION
		", Copyright (C) 2010-19 Digital Devices GmbH\n");
	stat = ddb_init_ddbridge();
	if (stat < 0)
		return stat;
	stat = pci_register_driver(&ddb_pci_driver);
	if (stat < 0)
		ddb_exit_ddbridge(0, stat);
	return stat;
}

static __exit void module_exit_ddbridge(void)
{
	pci_unregister_driver(&ddb_pci_driver);
	ddb_exit_ddbridge(0, 0);
}

module_init(module_init_ddbridge);
module_exit(module_exit_ddbridge);

MODULE_DESCRIPTION("Digital Devices PCIe Bridge");
MODULE_AUTHOR("Ralph and Marcus Metzler, Metzler Brothers Systementwicklung GbR");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DDBRIDGE_VERSION);
