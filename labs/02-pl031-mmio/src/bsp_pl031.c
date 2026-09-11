// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define PL031_DR 0x00
#define PL031_RIS 0x14

struct bsp_pl031 {
	void __iomem *base;
};

static ssize_t reg_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

	return sysfs_emit(buf, "0x%08x\n", readl(rtc->base + PL031_DR));
}
static DEVICE_ATTR_RO(reg_data);

static ssize_t raw_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

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

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->base))
		return PTR_ERR(rtc->base);

	platform_set_drvdata(pdev, rtc);

	ret = devm_device_add_group(&pdev->dev, &bsp_pl031_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: DR=0x%08x\n",
		readl(rtc->base + PL031_DR));
	return 0;
}

static void bsp_pl031_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "remove OK\n");
}

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
module_platform_driver(bsp_pl031_driver);

MODULE_DESCRIPTION("BSP Lab PL031 MMIO and IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
