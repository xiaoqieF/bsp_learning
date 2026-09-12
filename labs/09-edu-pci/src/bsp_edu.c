// SPDX-License-Identifier: GPL-2.0-only

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

/* PCI ID 用于将驱动绑定到 QEMU EDU 设备，而不是设备树 compatible。 */
#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8
#define EDU_INTR_STATUS 0x24 /* 中断状态寄存器。 */
#define EDU_INTR_RAISE 0x60  /* 写入后请求设备产生中断。 */
#define EDU_INTR_ACK 0x64    /* 写入待确认状态以清除中断。 */

struct bsp_edu {
	struct pci_dev *pdev; /* PCI 核心管理的设备对象。 */
	void __iomem *base; /* BAR0 映射后的 MMIO 基地址。 */
	atomic_t irq_count; /* 已处理的设备中断次数。 */
};

static irqreturn_t bsp_edu_irq(int irq, void *data)
{
	struct bsp_edu *edu = data;
	/* 读取设备状态，确认共享/意外到达的 IRQ 是否属于本设备。 */
	u32 status = readl(edu->base + EDU_INTR_STATUS);

	if (!status)
		/* 没有待处理状态，告知 IRQ 核心这不是本设备的中断。 */
		return IRQ_NONE;

	/* 向 acknowledge 寄存器写回状态，清除设备端的中断请求。 */
	writel(status, edu->base + EDU_INTR_ACK);
	/* 中断上下文可能并发执行，计数器使用原子操作。 */
	atomic_inc(&edu->irq_count);
	dev_info(&edu->pdev->dev, "IRQ handled\n");
	/* IRQ_HANDLED 表示该驱动已经完成本次中断处理。 */
	return IRQ_HANDLED;
}

static ssize_t id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct bsp_edu *edu = pci_get_drvdata(pdev);

	/* PCI sysfs 设备对象需要先转换为 pci_dev，再取回驱动私有数据。 */
	return sysfs_emit(buf, "0x%08x\n", readl(edu->base));
}
static DEVICE_ATTR_RO(id);

static ssize_t irq_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_edu *edu = pci_get_drvdata(to_pci_dev(dev));

	/* 读取软件统计值，不会访问 PCI 设备寄存器。 */
	return sysfs_emit(buf, "%d\n", atomic_read(&edu->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

static ssize_t raise_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_edu *edu = pci_get_drvdata(to_pci_dev(dev));
	unsigned long value;

	/* 允许用户通过 sysfs 写入设备定义的中断触发值。 */
	if (kstrtoul(buf, 0, &value))
		return -EINVAL;
	/* 写入 EDU 的 raise 寄存器会让设备向 CPU 发起中断。 */
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

	/* 启用 PCI 设备并让 PCI 核心准备其资源。 */
	ret = pcim_enable_device(pdev);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot enable PCI device\n");

	/* 请求并映射 BAR0；这里用 BIT(0) 表示选择第 0 个 BAR。 */
	ret = pcim_iomap_regions(pdev, BIT(0), "bsp-edu");
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot map BAR0\n");

	/* pcim 管理 BAR 映射，设备移除时会自动释放。 */
	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;
	/* 从 PCI 核心保存的映射表中取得 BAR0 的 MMIO 基地址。 */
	edu->base = pcim_iomap_table(pdev)[0];
	if (!edu->base)
		return -ENOMEM;

	edu->pdev = pdev;
	atomic_set(&edu->irq_count, 0);
	/* 保存私有数据，供 IRQ 和 sysfs 回调通过 pci_get_drvdata 获取。 */
	pci_set_drvdata(pdev, edu);

	/* 注册 PCI 设备中断；devm 会在解绑时自动释放 IRQ。 */
	ret = devm_request_irq(&pdev->dev, pdev->irq, bsp_edu_irq, 0,
			"bsp-edu", edu);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "cannot request IRQ\n");

	/* 暴露设备 ID、IRQ 计数和软件触发 IRQ 的 sysfs 接口。 */
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

/* PCI 核心根据 vendor/device ID 匹配设备，并调用 probe。 */
static struct pci_driver bsp_edu_driver = {
	.name = "bsp-edu",
	.id_table = bsp_edu_ids,
	.probe = bsp_edu_probe,
};

/* 自动生成模块 init/exit 函数并注册 PCI driver。 */
module_pci_driver(bsp_edu_driver);

MODULE_DESCRIPTION("BSP Lab QEMU EDU PCI MMIO and IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
