# Lab 04：PL061 GPIO Consumer

QEMU `virt` 自带 ARM PL061 GPIO controller。本实验通过设备树的 `led-gpios` 和 `input-gpios` 属性获取 GPIO descriptor，驱动本身不直接访问 PL061 寄存器。本实验只学习 GPIO consumer 和 descriptor API；输入 IRQ 与 pinctrl 留到后续专题。

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
