// SPDX-License-Identifier: GPL-2.0-only

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

/* PL031 寄存器偏移：访问寄存器时都要以 MMIO 基地址为起点。 */
#define PL031_DR 0x00   /* Data Register：当前 RTC 秒计数。 */
#define PL031_MR 0x04   /* Match Register：到达该值时触发 alarm。 */
#define PL031_IMSC 0x10 /* Interrupt Mask Set/Clear：使能 alarm 中断。 */
#define PL031_MIS 0x18  /* Masked Interrupt Status：屏蔽后的中断状态。 */
#define PL031_ICR 0x1c  /* Interrupt Clear Register：写 1 清除中断。 */
#define PL031_AI BIT(0) /* Alarm Interrupt：alarm 中断对应 bit 0。 */

struct bsp_pl031 {
	void __iomem *base; /* 设备寄存器映射后的 MMIO 虚拟地址。 */
	struct device *dev; /* 保存设备对象，便于中断处理函数打印日志。 */
	int irq; /* Linux 分配给该硬件中断的 IRQ 号。 */
	atomic_t irq_count; /* 统计处理过的 alarm 中断次数。 */
};

static irqreturn_t bsp_pl031_irq(int irq, void *data)
{
	struct bsp_pl031 *rtc = data;
	/* 共享 IRQ 下不能假定本设备就是中断源，先读取状态寄存器确认。 */
	u32 status = readl(rtc->base + PL031_MIS);

	if (!(status & PL031_AI))
		/* 不是本设备产生的中断，交给同一 IRQ 上的其他设备处理。 */
		return IRQ_NONE;

	/* PL031 的 ICR 采用写 1 清除（W1C）语义，避免中断持续触发。 */
	writel(PL031_AI, rtc->base + PL031_ICR);
	/* 中断上下文可能与其他上下文并发访问计数，因此使用原子变量。 */
	atomic_inc(&rtc->irq_count);
	dev_info(rtc->dev, "alarm IRQ handled, count=%d\n",
		atomic_read(&rtc->irq_count));
	/* IRQ_HANDLED 表示本设备确认并处理了这次中断。 */
	return IRQ_HANDLED;
}

static ssize_t irq_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* sysfs 读操作通过 drvdata 找回 probe 阶段保存的设备私有数据。 */
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);

	/* sysfs 属性需要返回写入用户缓冲区的字符数。 */
	return sysfs_emit(buf, "%d\n", atomic_read(&rtc->irq_count));
}
static DEVICE_ATTR_RO(irq_count);

static ssize_t alarm_seconds_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_pl031 *rtc = dev_get_drvdata(dev);
	unsigned long seconds;
	u32 now;

	/* kstrtoul 同时完成字符串解析和基本格式检查。 */
	if (kstrtoul(buf, 0, &seconds))
		return -EINVAL;
	/* 硬件寄存器是 32 位，拒绝无法安全转换的超大偏移量。 */
	if (seconds > U32_MAX)
		return -ERANGE;

	/* alarm_seconds 表示相对当前时间的秒数，而不是绝对寄存器值。 */
	now = readl(rtc->base + PL031_DR);
	writel(now + (u32)seconds, rtc->base + PL031_MR);
	/* 重新设置 alarm 前先清除旧的 pending 状态，避免立即产生旧中断。 */
	writel(PL031_AI, rtc->base + PL031_ICR);
	/* 最后打开 alarm 中断屏蔽位；写入操作顺序有助于避免误触发。 */
	writel(PL031_AI, rtc->base + PL031_IMSC);
	dev_info(dev, "alarm programmed: now=%u after=%lu seconds\n",
		now, seconds);
	/* store 回调返回 count，表示本次写入的内容已全部消费。 */
	return count;
}
static DEVICE_ATTR_WO(alarm_seconds);

/* 将多个 device_attribute 组织成一个 sysfs 属性组。 */
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

	/* devm 资源会在设备移除或 probe 失败时自动释放，减少清理代码。 */
	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	/* 从设备树提供的 reg 属性获取物理地址和寄存器空间大小。 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* ioremap 后通过 readl/writel 访问，不能直接解引用 MMIO 地址。 */
	rtc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->base))
		return PTR_ERR(rtc->base);

	/* 从设备树的 interrupts 属性获取 Linux IRQ 号。 */
	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->dev = &pdev->dev;
	atomic_set(&rtc->irq_count, 0);
	/* 后续 sysfs 和 IRQ 回调通过 platform_get_drvdata/dev_get_drvdata 取回 rtc。 */
	platform_set_drvdata(pdev, rtc);

	/* 注册共享中断；共享 IRQ 必须在处理函数中检查 MIS 并返回 IRQ_NONE。 */
	ret = devm_request_irq(&pdev->dev, rtc->irq, bsp_pl031_irq,
			IRQF_SHARED, dev_name(&pdev->dev), rtc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to request IRQ\n");

	/* 创建 alarm_seconds（写）和 irq_count（读）两个 sysfs 属性。 */
	ret = devm_device_add_group(&pdev->dev, &bsp_pl031_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: IRQ=%d\n", rtc->irq);
	return 0;
}

static void bsp_pl031_remove(struct platform_device *pdev)
{
	/* devm 管理的 IRQ、MMIO 和 sysfs 会自动撤销，这里只保留日志。 */
	dev_info(&pdev->dev, "remove OK\n");
}

/* 设备树 compatible 与驱动匹配成功后，内核才会调用 probe。 */
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

/* 自动生成模块 init/exit，并向 platform 总线注册该驱动。 */
module_platform_driver(bsp_pl031_driver);

MODULE_DESCRIPTION("BSP Lab PL031 alarm IRQ driver");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
