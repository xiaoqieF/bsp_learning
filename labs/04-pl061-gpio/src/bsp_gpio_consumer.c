// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct bsp_gpio_consumer {
	struct gpio_desc *led;
	struct gpio_desc *input;
};

static ssize_t led_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value_cansleep(gpio->led));
}

static ssize_t led_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value) || value > 1)
		return -EINVAL;

	gpiod_set_value_cansleep(gpio->led, value);
	return count;
}
static DEVICE_ATTR_RW(led);

static ssize_t input_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bsp_gpio_consumer *gpio = dev_get_drvdata(dev);

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

	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	gpio->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(gpio->led))
		return dev_err_probe(&pdev->dev, PTR_ERR(gpio->led),
				"failed to get led GPIO\n");

	gpio->input = devm_gpiod_get(&pdev->dev, "input", GPIOD_IN);
	if (IS_ERR(gpio->input))
		return dev_err_probe(&pdev->dev, PTR_ERR(gpio->input),
				"failed to get input GPIO\n");

	platform_set_drvdata(pdev, gpio);
	ret = devm_device_add_group(&pdev->dev, &bsp_gpio_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to create sysfs\n");

	dev_info(&pdev->dev, "probe OK: GPIO consumer ready\n");
	return 0;
}

static void bsp_gpio_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "remove OK\n");
}

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
module_platform_driver(bsp_gpio_driver);

MODULE_DESCRIPTION("BSP Lab PL061 GPIO consumer");
MODULE_AUTHOR("BSP Learn");
MODULE_LICENSE("GPL");
