// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

/* PL031 寄存器偏移，最终访问地址为 base + 偏移。 */
#define PL031_DR 0x00  /* Data Register：当前 RTC 秒计数。 */
#define PL031_RIS 0x14 /* Raw Interrupt Status：未屏蔽的原始中断状态。 */

struct bsp_pl031 {
	/* devm_ioremap_resource() 返回的 MMIO 映射地址，不能直接解引用。 */
	void __iomem *base;
};

static ssize_t reg_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* sysfs 回调通过 drvdata 找回 probe 阶段保存的设备私有数据。 */
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

	/* readl 用于读取设备寄存器，sysfs_emit 负责安全格式化输出。 */
	return sysfs_emit(buf, "0x%08x\n", readl(rtc->base + PL031_DR));
}
static DEVICE_ATTR_RO(reg_data);

static ssize_t raw_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

	/* RIS 是原始状态寄存器，这里只观察状态，不修改硬件状态。 */
	return sysfs_emit(buf, "0x%08x\n", readl(rtc->base + PL031_RIS));
}
static DEVICE_ATTR_RO(raw_status);

static struct attribute *bsp_pl031_attrs[] = {
	&dev_attr_reg_data.attr,
	&dev_attr_raw_status.attr,
	NULL,
};

static const struct attribute_group bsp_pl031_group = {
	.attrs = bsp_pl031_attrs,
};

static int bsp_pl031_probe(struct platform_device *pdev)
{
	struct bsp_pl031 *rtc;
	struct resource *res;
	int ret;

	/* devm 资源会在 probe 失败或设备移除时自动释放。 */
	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	/* 从设备树 reg 属性获取 MMIO 物理资源及其大小。 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* 建立内核虚拟地址映射，后续通过 readl/writel 访问寄存器。 */
	rtc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->base))
		return PTR_ERR(rtc->base);

	/* 将私有数据挂到 platform device，供 sysfs 回调取回。 */
	platform_set_drvdata(pdev, rtc);

	/* 注册两个只读 sysfs 属性：当前数据寄存器和原始中断状态。 */
	ret = devm_device_add_group(&pdev->dev, &bsp_pl031_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: DR=0x%08x\n",
		readl(rtc->base + PL031_DR));
	return 0;
}

static void bsp_pl031_remove(struct platform_device *pdev)
{
	/* MMIO、属性组和内存由 devm 自动清理，remove 只记录生命周期事件。 */
	dev_info(&pdev->dev, "remove OK\n");
}

/* compatible 匹配成功后，platform 总线才会调用本驱动的 probe。 */
static const struct of_device_id bsp_pl031_of_match[] = {
	{ .compatible = "bsp-learn,pl031" },
	{ }
};
MODULE_DEVICE_TABLE(of, bsp_pl031_of_match);

static struct platform_driver bsp_pl031_driver = {
	.probe = bsp_pl031_probe,
	.remove = bsp_pl031_remove,
	.driver = {
		.name = "bsp-pl031",
		.of_match_table = bsp_pl031_of_match,
	},
};

/* 自动生成模块初始化/退出函数并注册 platform driver。 */
module_platform_driver(bsp_pl031_driver);

MODULE_DESCRIPTION("BSP Lab PL031 MMIO and IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
