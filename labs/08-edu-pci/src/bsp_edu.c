// SPDX-License-Identifier: GPL-2.0-only

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8
#define EDU_INTR_STATUS 0x24
#define EDU_INTR_RAISE 0x60
#define EDU_INTR_ACK 0x64

struct bsp_edu {
	struct pci_dev *pdev;
	void __iomem *base;
	atomic_t irq_count;
};

static irqreturn_t bsp_edu_irq(int irq, void *data)
{
	struct bsp_edu *edu = data;
	u32 status = readl(edu->base + EDU_INTR_STATUS);

	if (!status)
		return IRQ_NONE;

	writel(status, edu->base + EDU_INTR_ACK);
	atomic_inc(&edu->irq_count);
	dev_info(&edu->pdev->dev, "IRQ handled\n");
	return IRQ_HANDLED;
}

static ssize_t id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct bsp_edu *edu = pci_get_drvdata(pdev);

	return sysfs_emit(buf, "0x%08x\n", readl(edu->base));
}
static DEVICE_ATTR_RO(id);

static ssize_t irq_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_edu *edu = pci_get_drvdata(to_pci_dev(dev));

	return sysfs_emit(buf, "%d\n", atomic_read(&edu->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

static ssize_t raise_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_edu *edu = pci_get_drvdata(to_pci_dev(dev));
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;
	writel(value, edu->base + EDU_INTR_RAISE);
	return count;
}
static DEVICE_ATTR_WO(raise_irq);

static struct attribute *bsp_edu_attrs[] = {
	&dev_attr_id.attr,
	&dev_attr_irq_count.attr,
	&dev_attr_raise_irq.attr,
	NULL,
};

static const struct attribute_group bsp_edu_group = {
	.attrs = bsp_edu_attrs,
};

static int bsp_edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct bsp_edu *edu;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot enable PCI device\n");

	ret = pcim_iomap_regions(pdev, BIT(0), "bsp-edu");
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot map BAR0\n");

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	edu->base = pcim_iomap_table(pdev)[0];
	if (!edu->base)
		return -ENOMEM;

	edu->pdev = pdev;
	atomic_set(&edu->irq_count, 0);
	pci_set_drvdata(pdev, edu);

	ret = devm_request_irq(&pdev->dev, pdev->irq, bsp_edu_irq, 0,
			"bsp-edu", edu);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot request IRQ\n");

	ret = devm_device_add_group(&pdev->dev, &bsp_edu_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot create sysfs\n");

	dev_info(&pdev->dev, "probe OK: BAR0=%p IRQ=%d ID=0x%08x\n",
		edu->base, pdev->irq, readl(edu->base));
	return 0;
}

static const struct pci_device_id bsp_edu_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, bsp_edu_ids);

static struct pci_driver bsp_edu_driver = {
	.name = "bsp-edu",
	.id_table = bsp_edu_ids,
	.probe = bsp_edu_probe,
};
module_pci_driver(bsp_edu_driver);

MODULE_DESCRIPTION("BSP Lab QEMU EDU PCI MMIO and IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
