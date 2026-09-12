# BSP Linux Lab 系列

这是一套以当前仓库为基线的精简实验系列。Kernel、BusyBox 和基础 initramfs 已经准备好，因此前置部分只做启动确认；驱动实验优先使用 QEMU `virt` 中真实存在的 PL031、PL061、Virtio 和 EDU PCI 设备。

## 公共前置

检查工具和现有构建产物：

```bash
labs/common/check-tools.sh
```

第一次编译外部内核模块时，如果 `linux-6.12/Module.symvers` 不存在，公共脚本会执行一次并行的 `make modules`。这一步可能比后续单个 Lab 编译耗时更长；之后的模块构建只编译当前 Lab。公共脚本会串行化共享内核目录的 Kbuild，允许多个 lab 测试并行启动而不互相破坏中间文件。

所有 Lab 默认使用：

```text
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
QEMU machine=virt
RAM=1G
console=ttyAMA0
```

## Lab 列表

| Lab | 内容 | 主要对象 | 状态 |
| --- | --- | --- | --- |
| 00 | 启动基线 | Kernel、initramfs、`/init` | 可执行 |
| 01 | Device Tree 观察 | PL011、PL031、PL061、GIC、Virtio、PCIe | 可执行 |
| 02 | Platform Driver + MMIO | QEMU PL031 | 可执行 |
| 03 | RTC Alarm + IRQ | PL031、GIC | 可执行 |
| 04 | GPIO Consumer | PL061、gpiod API | 可执行 |
| 05 | Device Model 调试 | sysfs、uevent、bind/unbind | 可执行 |
| 06 | RTC 字符设备用户接口 | `/dev`、read/write、select、ioctl | 可执行 |
| 06（原） | Virtio Block + 持久化 rootfs | virtio-blk、ext4 | 可执行 |
| 07 | U-Boot 启动链 | U-Boot、Image、DTB、initrd | 可选，需要外部 U-Boot |
| 08 | PCI BAR + MMIO + IRQ | QEMU EDU PCI | 可执行 |

## 推荐顺序

```bash
labs/00-quickstart/run.sh
labs/01-device-tree/build.sh
labs/01-device-tree/run.sh
labs/02-pl031-mmio/build.sh
labs/02-pl031-mmio/run.sh
labs/03-pl031-irq/build.sh
labs/03-pl031-irq/run.sh
labs/04-pl061-gpio/run.sh
labs/06-char-device/run.sh
labs/06-virtio-storage/run.sh
labs/08-edu-pci/run.sh
```

Lab 05 直接阅读 `labs/05-device-model/README.md`；`labs/05-device-model/test.sh` 会自动验证真实设备的 `unbind`/`bind` 生命周期。

Lab 06 通过 PL031 alarm 产生事件，并由字符设备 `/dev/bsp_rtc0` 提供 `read`、`write`、`select` 和 `ioctl` 用户接口；`labs/06-char-device/test.sh` 会自动运行静态用户态示例。

Lab 02、03 和 04 需要在 QEMU shell 中手工执行 `insmod`，具体 sysfs 路径以当前启动日志和 bus symlink-safe 的循环查找结果为准。各 lab 的 `test.sh` 可执行自动验收。

驱动实验的 `starter/` 目录给出任务拆分，`solution/` 目录说明参考实现位置；构建脚本默认使用 `src/` 中的可运行实现，便于先验证环境再开始练习。

## 设备真实性边界

- Lab 02 和 Lab 03 使用 QEMU 实现的 PL031 寄存器和 alarm 中断，教学 DTB 只改变 `compatible`，没有虚构寄存器；Lab 02 只覆盖 MMIO，Lab 03 覆盖 IRQ。
- Lab 04 使用 QEMU PL061 controller，通过 `gpiod` 访问；QEMU 没有实体 LED/按钮，因此不验证物理电平和 pinmux 电气特性。
- Lab 06 使用 QEMU virtio-blk 和真正的 ext4 镜像，验证持久化 rootfs 启动。
- Lab 08 使用 QEMU EDU PCI 设备的 BAR 和中断；它不是设备树设备，用于对比 PCI 驱动模型。

## 暂缓主题

I2C、SPI、pinctrl、clock、regulator、DMA、V4L2、Media Controller 和 Jetson 专用 BSP 暂不作为第一版可执行 Lab。它们需要真实 SoC 资源拓扑、专用 QEMU machine 或 Camera/媒体设备，后续可以分别扩展。
