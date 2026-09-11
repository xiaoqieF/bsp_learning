// SPDX-License-Identifier: GPL-2.0-only

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define PL031_DR 0x00
#define PL031_MR 0x04
#define PL031_IMSC 0x10
#define PL031_MIS 0x18
#define PL031_ICR 0x1c
#define PL031_AI BIT(0)

struct bsp_pl031 {
	void __iomem *base;
	struct device *dev;
	int irq;
	atomic_t irq_count;
};

static irqreturn_t bsp_pl031_irq(int irq, void *data)
{
	struct bsp_pl031 *rtc = data;
	u32 status = readl(rtc->base + PL031_MIS);

	if (!(status & PL031_AI))
		return IRQ_NONE;

	writel(PL031_AI, rtc->base + PL031_ICR);
	atomic_inc(&rtc->irq_count);
	dev_info(rtc->dev, "alarm IRQ handled, count=%d\n",
		atomic_read(&rtc->irq_count));
	return IRQ_HANDLED;
}

static ssize_t irq_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", atomic_read(&rtc->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

static ssize_t alarm_seconds_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);
	unsigned long seconds;
	u32 now;

	if (kstrtoul(buf, 0, &seconds))
		return -EINVAL;
	if (seconds > U32_MAX)
		return -ERANGE;

	now = readl(rtc->base + PL031_DR);
	writel(now + (u32)seconds, rtc->base + PL031_MR);
	writel(PL031_AI, rtc->base + PL031_ICR);
	writel(PL031_AI, rtc->base + PL031_IMSC);
	dev_info(dev, "alarm programmed: now=%u after=%lu seconds\n",
		now, seconds);
	return count;
}
static DEVICE_ATTR_WO(alarm_seconds);

static struct attribute *bsp_pl031_attrs[] = {
	&dev_attr_irq_count.attr,
	&dev_attr_alarm_seconds.attr,
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

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->dev = &pdev->dev;
	atomic_set(&rtc->irq_count, 0);
	platform_set_drvdata(pdev, rtc);

	ret = devm_request_irq(&pdev->dev, rtc->irq, bsp_pl031_irq,
			IRQF_SHARED, dev_name(&pdev->dev), rtc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to request IRQ\n");

	ret = devm_device_add_group(&pdev->dev, &bsp_pl031_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: IRQ=%d\n", rtc->irq);
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
		.name = "bsp-pl031-irq",
		.of_match_table = bsp_pl031_of_match,
	},
};
module_platform_driver(bsp_pl031_driver);

MODULE_DESCRIPTION("BSP Lab PL031 alarm IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
