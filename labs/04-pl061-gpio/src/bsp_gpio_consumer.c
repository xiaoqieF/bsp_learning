// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct bsp_gpio_consumer {
	/* descriptor 隐藏控制器编号、active-low 等硬件细节。 */
	struct gpio_desc *led;
	struct gpio_desc *input;
};

static ssize_t led_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);

	/* cansleep 版本允许底层 GPIO 控制器访问可能睡眠的总线。 */
	return sysfs_emit(buf, "%d\n", gpiod_get_value_cansleep(gpio->led));
}

static ssize_t led_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);
	unsigned long value;

	/* sysfs 输入是字符串；这里只接受 0 或 1 两种逻辑值。 */
	if (kstrtoul(buf, 0, &value) || value > 1)
		return -EINVAL;

	/* descriptor 层会处理设备树中的 GPIO_ACTIVE_LOW 属性。 */
	gpiod_set_value_cansleep(gpio->led, value);
	/* store 回调返回 count，表示用户写入的数据已全部处理。 */
	return count;
}
static DEVICE_ATTR_RW(led);

static ssize_t input_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);

	/* 读取输入 GPIO 的逻辑值，而不是直接访问 PL061 寄存器。 */
	return sysfs_emit(buf, "%d\n", gpiod_get_value_cansleep(gpio->input));
}
static DEVICE_ATTR_RO(input);

static struct attribute *bsp_gpio_attrs[] = {
	&dev_attr_led.attr,
	&dev_attr_input.attr,
	NULL,
};

static const struct attribute_group bsp_gpio_group = {
	.attrs = bsp_gpio_attrs,
};

static int bsp_gpio_probe(struct platform_device *pdev)
{
	struct bsp_gpio_consumer *gpio;
	int ret;

	/* devm 分配的私有数据会随设备生命周期自动释放。 */
	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	/* "led" 对应设备树的 led-gpios，输出初始为低电平。 */
	gpio->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(gpio->led))
		return dev_err_probe(&pdev->dev, PTR_ERR(gpio->led),
				"failed to get led GPIO\n");

	/* "input" 对应 input-gpios，并在获取时配置为输入方向。 */
	gpio->input = devm_gpiod_get(&pdev->dev, "input", GPIOD_IN);
	if (IS_ERR(gpio->input))
		return dev_err_probe(&pdev->dev, PTR_ERR(gpio->input),
				"failed to get input GPIO\n");

	/* 保存私有数据，使 sysfs 回调能够找到两个 GPIO descriptor。 */
	platform_set_drvdata(pdev, gpio);
	/* 创建 led（读写）和 input（只读）两个 sysfs 属性。 */
	ret = devm_device_add_group(&pdev->dev, &bsp_gpio_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: GPIO consumer ready\n");
	return 0;
}

static void bsp_gpio_remove(struct platform_device *pdev)
{
	/* GPIO descriptor、sysfs 和私有内存均由 devm 自动释放。 */
	dev_info(&pdev->dev, "remove OK\n");
}

/* 通过设备树 compatible 将该 consumer 驱动绑定到目标节点。 */
static const struct of_device_id bsp_gpio_of_match[] = {
	{ .compatible = "bsp-learn,gpio-consumer" },
	{ }
};
MODULE_DEVICE_TABLE(of, bsp_gpio_of_match);

static struct platform_driver bsp_gpio_driver = {
	.probe = bsp_gpio_probe,
	.remove = bsp_gpio_remove,
	.driver = {
		.name = "bsp-gpio-consumer",
		.of_match_table = bsp_gpio_of_match,
	},
};

/* 自动生成模块 init/exit 函数并注册 platform driver。 */
module_platform_driver(bsp_gpio_driver);

MODULE_DESCRIPTION("BSP Lab PL061 GPIO consumer");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
