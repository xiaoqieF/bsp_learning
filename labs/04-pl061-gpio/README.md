# Lab 04：PL061 GPIO Consumer

QEMU `virt` 自带 ARM PL061 GPIO controller。本实验通过设备树的 `led-gpios` 和 `input-gpios` 属性获取 GPIO descriptor，驱动本身不直接访问 PL061 寄存器。本实验的主线是学习 GPIO consumer 和 descriptor API；后文另外用一个简化骨架介绍 GPIO controller 驱动的写法。输入 IRQ 与 pinctrl 留到后续专题。

## 一、先理解 GPIO 的两类驱动

Linux GPIO 子系统把 GPIO 硬件和使用 GPIO 的功能驱动分成两层：

```text
设备树中的 consumer 节点
        │  led-gpios = <&pl061 0 GPIO_ACTIVE_HIGH>
        │  input-gpios = <&pl061 1 GPIO_ACTIVE_HIGH>
        ▼
GPIO consumer 驱动：devm_gpiod_get() / gpiod_set_value_cansleep()
        ▼
GPIO descriptor（struct gpio_desc）
        ▼
gpiolib：查找 controller、申请 GPIO、处理逻辑电平语义
        ▼
GPIO controller 驱动：struct gpio_chip 回调
        ▼
PL061 寄存器 / 实际硬件引脚
```

### GPIO controller 是什么

GPIO controller 驱动负责把某个硬件 GPIO 控制器接入 gpiolib。它了解硬件寄存器布局和时序，但不应该知道某根线最终连接的是 LED、按键还是其他设备。典型职责包括：

- 映射控制器的 MMIO 资源，并初始化 `struct gpio_chip`；
- 提供 `direction_input`、`direction_output`、`get`、`set` 等硬件操作回调；
- 设置 `ngpio`、GPIO 名称、设备树匹配表和 `#gpio-cells` 的解析规则；
- 如果硬件支持，还可以提供 GPIO 到 IRQ 的映射、去抖和电气配置。

PL061 在本实验的设备树中是一个 GPIO provider：`gpio-controller` 表示它提供 GPIO，`#gpio-cells = <2>` 表示引用它时需要两个参数。第一个参数是 GPIO offset（例如 `0` 或 `1`），第二个参数是 flags（例如 `GPIO_ACTIVE_LOW`）。`phandle` 只是设备树节点之间的引用标识，不是 consumer 应该保存或操作的 GPIO 编号。

### GPIO consumer 是什么

GPIO consumer 驱动是使用 GPIO 的功能驱动。它通过设备树的 `<name>-gpios` 属性声明依赖，再用 descriptor API 获取 GPIO。consumer 不需要知道：

- controller 的基地址和寄存器偏移；
- GPIO 在系统中的全局编号；
- active-low 线路的物理电平如何反转；
- controller 是否通过可能睡眠的总线访问。

例如本实验的构建脚本最终生成类似下面的 consumer 节点（`&pl061` 的实际 phandle 由脚本动态读取）：

```dts
bsp_gpio {
	compatible = "bsp-learn,gpio-consumer";
	led-gpios = <&pl061 0 GPIO_ACTIVE_HIGH>;
	input-gpios = <&pl061 1 GPIO_ACTIVE_HIGH>;
};
```

`devm_gpiod_get(dev, "led", ...)` 中的 `"led"` 对应 `led-gpios`，`"input"` 对应 `input-gpios`。返回的 `struct gpio_desc *` 是不透明句柄，consumer 只应把它传给 `gpiod_*` API。

### 逻辑值和物理电平

consumer API 使用逻辑值：`1` 表示功能上的有效（asserted），`0` 表示无效（deasserted）。设备树 flags 描述线路语义，例如：

```dts
led-gpios = <&pl061 0 GPIO_ACTIVE_LOW>;
```

此时 consumer 仍然使用 `gpiod_set_value_cansleep(desc, 1)` 表示“打开 LED”，descriptor 层会把它转换为底层所需的物理低电平。不要在 consumer 中再手工取反，否则会反转两次。读取 descriptor 时同样会转换回逻辑值。

### 为什么使用 `cansleep` API

`gpiod_get_value()` 和 `gpiod_set_value()` 只适用于保证不会睡眠的 GPIO 操作；很多 GPIO controller 通过 I2C、SPI 或其他可能睡眠的路径访问硬件。consumer 无法仅凭设备树判断这一点，因此一般使用 `gpiod_get_value_cansleep()` 和 `gpiod_set_value_cansleep()`。本实验使用 PL061 MMIO，理论上可用非 `cansleep` 版本，但 `cansleep` 版本更适合通用 consumer 写法。

## 二、GPIO controller 驱动的最小骨架

下面是帮助理解分层的教学骨架，不是本实验要编译或加载的第二个 PL061 驱动。QEMU 启动时内核已有 `gpio-pl061` 驱动绑定到 `arm,pl061`；再次注册同一控制器会造成资源和设备绑定冲突。

一个简单的 MMIO GPIO controller 驱动大致需要以下步骤：

1. 在 `probe()` 中使用 `devm_platform_ioremap_resource()` 映射 `reg` 资源。
2. 初始化 `gpio_chip` 的设备、标签、GPIO 数量和操作回调。
3. 让回调通过 `gpiochip_get_data()` 找到控制器私有数据，并读写 PL061 寄存器。
4. 使用 `devm_gpiochip_add_data()` 注册到 gpiolib。

```c
struct demo_gpio {
	void __iomem *base;
	struct gpio_chip chip;
};

static int demo_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	struct demo_gpio *gpio = gpiochip_get_data(chip);
	u32 direction = readl(gpio->base + 0x400);

	direction &= ~BIT(offset);
	writel(direction, gpio->base + 0x400);
	return 0;
}

static int demo_direction_output(struct gpio_chip *chip,
		unsigned int offset, int value)
{
	struct demo_gpio *gpio = gpiochip_get_data(chip);
	u32 direction = readl(gpio->base + 0x400);

	if (value)
		writel(BIT(offset), gpio->base + 0x000 + (BIT(offset) << 2));
	else
		writel(0, gpio->base + 0x000 + (BIT(offset) << 2));
	direction |= BIT(offset);
	writel(direction, gpio->base + 0x400);
	return 0;
}

static int demo_get(struct gpio_chip *chip, unsigned int offset)
{
	struct demo_gpio *gpio = gpiochip_get_data(chip);

	return !!readl(gpio->base + 0x000 + (BIT(offset) << 2));
}

static void demo_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct demo_gpio *gpio = gpiochip_get_data(chip);

	writel(value ? BIT(offset) : 0,
	       gpio->base + 0x000 + (BIT(offset) << 2));
}
```

对应的 `probe()` 初始化大致如下：

```c
struct demo_gpio *gpio;

gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
if (!gpio)
	return -ENOMEM;

gpio->base = devm_platform_ioremap_resource(pdev, 0);
if (IS_ERR(gpio->base))
	return PTR_ERR(gpio->base);

gpio->chip.label = dev_name(&pdev->dev);
gpio->chip.parent = &pdev->dev;
gpio->chip.owner = THIS_MODULE;
gpio->chip.ngpio = 8;
gpio->chip.direction_input = demo_direction_input;
gpio->chip.direction_output = demo_direction_output;
gpio->chip.get = demo_get;
gpio->chip.set = demo_set;

return devm_gpiochip_add_data(&pdev->dev, &gpio->chip, gpio);
```

这里的 `0x400` 是 PL061 的方向寄存器，数据寄存器采用按位掩码寻址；实际生产驱动还需要检查 offset、处理锁、初始化硬件、处理中断，以及根据内核版本使用合适的设备树转换和 IRQ 辅助接口。这个骨架的关键不是直接读写寄存器，而是看清楚 controller 回调如何成为 consumer `gpiod_*` 调用的底层实现。

## 三、本实验的 consumer 实现

本实验的 `src/bsp_gpio_consumer.c` 正是这一层的完整示例。`probe()` 获取两个 descriptor：`led` 使用 `GPIOD_OUT_LOW`，`input` 使用 `GPIOD_IN`；sysfs 回调再使用 `gpiod_set_value_cansleep()` 和 `gpiod_get_value_cansleep()`。因此，代码中没有 PL061 的寄存器地址，也没有手工维护 GPIO 编号或 active-low 转换。

## 构建和运行

```bash
labs/04-pl061-gpio/build.sh
labs/04-pl061-gpio/run.sh
```

进入系统后：

```sh
insmod /lib/modules/bsp_gpio_consumer.ko
dmesg | grep 'GPIO consumer'
for device in /sys/bus/platform/devices/*; do
    [ -e "$device/led" ] || continue
    echo "$device"
done
```

找到设备目录后：

```sh
cat /sys/bus/platform/devices/<device>/led
echo 1 > /sys/bus/platform/devices/<device>/led
cat /sys/bus/platform/devices/<device>/led
cat /sys/bus/platform/devices/<device>/input
cat /sys/kernel/debug/gpio
```

## 重点

- `devm_gpiod_get()` 根据 `<name>-gpios` 获取 consumer GPIO。
- `GPIOD_OUT_LOW` 在 probe 时设置初始方向和值。
- `gpiod_set_value_cansleep()` 通过 GPIO subsystem 修改输出。
- active-low 属性由 GPIO descriptor 层处理，不由 consumer 自己反转。

构建从 PL061 节点读取 phandle，再用 `fdtput` 添加 consumer 节点，不依赖固定的数字 phandle。

QEMU 没有实体 LED 和按钮，因此本实验验证 GPIO controller、consumer、descriptor、方向和值的 Linux 链路；物理电平和 pinmux 仍需在真实开发板上验证。

## 学习任务

1. 按 `starter/README.md` 的任务拆分补全两个 GPIO descriptor 的获取。
2. 使用 `GPIOD_OUT_LOW` 和 `GPIOD_IN` 设置方向。
3. 通过 sysfs 验证输出值和输入值。
4. 对照 `solution/README.md` 中的参考实现位置检查 active-low 语义是否留给 descriptor 层处理。
