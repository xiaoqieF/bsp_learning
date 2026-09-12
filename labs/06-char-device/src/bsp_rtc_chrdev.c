// SPDX-License-Identifier: GPL-2.0-only

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

/* PL031 寄存器偏移；所有 MMIO 访问都相对于 base 地址进行。 */
#define PL031_DR 0x00   /* 当前 RTC 秒计数。 */
#define PL031_MR 0x04   /* alarm 匹配值。 */
#define PL031_IMSC 0x10 /* alarm 中断屏蔽控制。 */
#define PL031_MIS 0x18  /* 屏蔽后的中断状态。 */
#define PL031_ICR 0x1c  /* 写 1 清除 pending 状态。 */
#define PL031_AI BIT(0) /* alarm 中断对应的 bit。 */

/* ioctl 命令号：查询已经产生的事件，或设置相对秒数 alarm。 */
#define BSP_RTC_IOC_MAGIC 'r'
#define BSP_RTC_GET_COUNT _IOR(BSP_RTC_IOC_MAGIC, 0, __u32)
#define BSP_RTC_SET_ALARM _IOW(BSP_RTC_IOC_MAGIC, 1, __u32)

struct bsp_rtc_event {
	__u32 sequence;    /* 事件递增序号。 */
	__u32 rtc_seconds; /* 事件被读取时的 RTC 秒计数。 */
};

struct bsp_rtc {
	void __iomem *base;          /* PL031 的内核 MMIO 映射地址。 */
	int irq;                     /* Linux 分配的中断号。 */
	dev_t devt;                  /* 动态分配的主、次设备号。 */
	struct cdev cdev;            /* 字符设备核心对象。 */
	struct class *class;         /* 让 devtmpfs 创建设备节点的 class。 */
	struct device *device;       /* 对应 /dev/bsp_rtc0 的 device 对象。 */
	wait_queue_head_t read_queue; /* read/select 等待的队列。 */
	spinlock_t lock;             /* 保护 sequence 和 pending。 */
	__u32 sequence;              /* 已处理的 alarm 总数。 */
	__u32 pending;               /* 尚未被 read 消费的事件数。 */
};

static void bsp_rtc_program_alarm(struct bsp_rtc *rtc, u32 seconds)
{
	u32 now = readl(rtc->base + PL031_DR);

	/* PL031 使用绝对匹配值，因此先读取当前时间再加相对秒数。 */
	writel(now + seconds, rtc->base + PL031_MR);
	/* 清掉旧 pending，再打开 alarm 中断。 */
	writel(PL031_AI, rtc->base + PL031_ICR);
	writel(PL031_AI, rtc->base + PL031_IMSC);
}

static int bsp_rtc_open(struct inode *inode, struct file *file)
{
	struct bsp_rtc *rtc;

	/* open() 通过 inode 找到 cdev，并把设备私有数据保存到 file。 */
	rtc = container_of(inode->i_cdev, struct bsp_rtc, cdev);
	file->private_data = rtc;
	dev_info(rtc->device, "open: mode=%s%s\n",
		(file->f_mode & FMODE_READ) ? "read" : "",
		(file->f_mode & FMODE_WRITE) ? "+write" : "");
	return 0;
}

static int bsp_rtc_release(struct inode *inode, struct file *file)
{
	struct bsp_rtc *rtc = file->private_data;

	/* 当前驱动没有每个文件实例的额外资源，close() 只用于观察生命周期。 */
	dev_info(rtc->device, "release\n");
	return 0;
}

static struct bsp_rtc *bsp_rtc_from_file(struct file *file)
{
	/* 后续 read/write/poll/ioctl 直接复用 open() 保存的私有数据。 */
	return file->private_data;
}

static irqreturn_t bsp_rtc_irq(int irq, void *data)
{
	struct bsp_rtc *rtc = data;
	unsigned long flags;

	/* 共享 IRQ 下必须确认中断确实来自本设备。 */
	if (!(readl(rtc->base + PL031_MIS) & PL031_AI))
		return IRQ_NONE;

	/* W1C 寄存器写 1 清除 alarm，避免中断重复进入。 */
	writel(PL031_AI, rtc->base + PL031_ICR);
	/* IRQ 上下文和 read/poll 上下文会并发访问计数，因此加自旋锁。 */
	/* spin_lock_irqsave 会保存中断状态并禁用中断。 */
	spin_lock_irqsave(&rtc->lock, flags);
	rtc->sequence++;
	rtc->pending++;
	spin_unlock_irqrestore(&rtc->lock, flags);
	/* 唤醒阻塞在 read() 或 select() 中的用户进程。 */
	wake_up_interruptible(&rtc->read_queue);
	return IRQ_HANDLED;
}

static ssize_t bsp_rtc_read(struct file *file, char __user *buffer,
		size_t length, loff_t *offset)
{
	struct bsp_rtc *rtc = bsp_rtc_from_file(file);
	struct bsp_rtc_event event;
	unsigned long flags;
	int ret;

	/* 用户缓冲区必须能够容纳一个完整事件。 */
	if (length < sizeof(event))
		return -EINVAL;

	/* 没有事件时睡眠；信号到达时返回 -ERESTARTSYS。 */
	ret = wait_event_interruptible(rtc->read_queue, rtc->pending != 0);
	if (ret)
		return ret;

	spin_lock_irqsave(&rtc->lock, flags);
	/* 先取出最早待处理事件的序号，再消费一个 pending 事件。 */
	event.sequence = rtc->sequence - rtc->pending + 1;
	event.rtc_seconds = readl(rtc->base + PL031_DR);
	rtc->pending--;
	spin_unlock_irqrestore(&rtc->lock, flags);

	/* 中断上下文不能直接访问用户地址，复制动作必须在进程上下文完成。 */
	if (copy_to_user(buffer, &event, sizeof(event)))
		return -EFAULT;
	return sizeof(event);
}

static ssize_t bsp_rtc_write(struct file *file, const char __user *buffer,
		size_t length, loff_t *offset)
{
	struct bsp_rtc *rtc = bsp_rtc_from_file(file);
	char input[24];
	unsigned long seconds;

	/* write() 接收 ASCII 格式的相对秒数，例如 echo 2 > /dev/bsp_rtc0。 */
	if (!length || length >= sizeof(input))
		return -EINVAL;
	if (copy_from_user(input, buffer, length))
		return -EFAULT;
	input[length] = '\0';
	/* kstrtoul 同时完成字符串解析和基本范围检查。 */
	if (kstrtoul(input, 0, &seconds) || seconds > U32_MAX)
		return -EINVAL;

	bsp_rtc_program_alarm(rtc, (u32)seconds);
	return length;
}

static __poll_t bsp_rtc_poll(struct file *file, poll_table *wait)
{
	struct bsp_rtc *rtc = bsp_rtc_from_file(file);

	/* 将当前 file 加入等待队列；事件到达后 IRQ 会唤醒它。 */
	poll_wait(file, &rtc->read_queue, wait);
	/* 返回可读掩码后，select() 才会把 fd 标记为 ready。 */
	return rtc->pending ? EPOLLIN | EPOLLRDNORM : 0;
}

static long bsp_rtc_ioctl(struct file *file, unsigned int command,
		unsigned long argument)
{
	struct bsp_rtc *rtc = bsp_rtc_from_file(file);
	unsigned long flags;
	__u32 value;

	switch (command) {
	case BSP_RTC_GET_COUNT:
		/* ioctl 的数据方向由驱动负责完成 copy_to_user。 */
		spin_lock_irqsave(&rtc->lock, flags);
		value = rtc->sequence;
		spin_unlock_irqrestore(&rtc->lock, flags);
		return copy_to_user((void __user *)argument, &value, sizeof(value)) ?
			-EFAULT : 0;
	case BSP_RTC_SET_ALARM:
		/* _IOW 参数来自用户地址，先复制到内核栈变量。 */
		if (copy_from_user(&value, (void __user *)argument, sizeof(value)))
			return -EFAULT;
		bsp_rtc_program_alarm(rtc, value);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations bsp_rtc_fops = {
	/* open/release 展示文件生命周期，另外四个回调对应用户态接口。 */
	.owner = THIS_MODULE,
	.open = bsp_rtc_open,
	.release = bsp_rtc_release,
	.read = bsp_rtc_read,
	.write = bsp_rtc_write,
	.poll = bsp_rtc_poll,
	.unlocked_ioctl = bsp_rtc_ioctl,
	.llseek = noop_llseek,
};

static int bsp_rtc_probe(struct platform_device *pdev)
{
	struct bsp_rtc *rtc;
	int ret;

	/* devm 分配的内存和 MMIO 会在设备移除时自动释放。 */
	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;
	rtc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rtc->base))
		return PTR_ERR(rtc->base);
	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;
	init_waitqueue_head(&rtc->read_queue);
	spin_lock_init(&rtc->lock);
	platform_set_drvdata(pdev, rtc);

	/* 动态申请设备号，再注册 cdev 和 file_operations。 */
	ret = alloc_chrdev_region(&rtc->devt, 0, 1, "bsp_rtc");
	if (ret)
		return ret;
	cdev_init(&rtc->cdev, &bsp_rtc_fops);
	rtc->cdev.owner = THIS_MODULE;
	ret = cdev_add(&rtc->cdev, rtc->devt, 1);
	if (ret)
		goto unregister_chrdev;
	/* class/device_create 负责把字符设备呈现为 /dev/bsp_rtc0。 */
	rtc->class = class_create("bsp_rtc");
	if (IS_ERR(rtc->class)) {
		ret = PTR_ERR(rtc->class);
		goto del_cdev;
	}
	rtc->device = device_create(rtc->class, &pdev->dev, rtc->devt, NULL,
			"bsp_rtc0");
	if (IS_ERR(rtc->device)) {
		ret = PTR_ERR(rtc->device);
		goto destroy_class;
	}
	/* 注册共享中断；处理函数会自行检查 PL031 的状态寄存器。 */
	ret = devm_request_irq(&pdev->dev, rtc->irq, bsp_rtc_irq, IRQF_SHARED,
			dev_name(&pdev->dev), rtc);
	if (ret)
		goto destroy_device;
	dev_info(&pdev->dev, "probe OK: /dev/bsp_rtc0 IRQ=%d\n", rtc->irq);
	return 0;

destroy_device:
	/* probe 失败时按创建顺序的逆序释放字符设备资源。 */
	device_destroy(rtc->class, rtc->devt);
destroy_class:
	class_destroy(rtc->class);
del_cdev:
	cdev_del(&rtc->cdev);
unregister_chrdev:
	unregister_chrdev_region(rtc->devt, 1);
	return ret;
}

static void bsp_rtc_remove(struct platform_device *pdev)
{
	struct bsp_rtc *rtc = platform_get_drvdata(pdev);

	device_destroy(rtc->class, rtc->devt);
	class_destroy(rtc->class);
	cdev_del(&rtc->cdev);
	unregister_chrdev_region(rtc->devt, 1);
	dev_info(&pdev->dev, "remove OK\n");
}

static const struct of_device_id bsp_rtc_of_match[] = {
	{ .compatible = "bsp-learn,rtc-chardev" },
	{ }
};
MODULE_DEVICE_TABLE(of, bsp_rtc_of_match);

static struct platform_driver bsp_rtc_driver = {
	.probe = bsp_rtc_probe,
	.remove = bsp_rtc_remove,
	.driver = {
		.name = "bsp-rtc-chardev",
		.of_match_table = bsp_rtc_of_match,
	},
};
module_platform_driver(bsp_rtc_driver);

MODULE_DESCRIPTION("BSP Lab RTC character device");
MODULE_LICENSE("GPL");
