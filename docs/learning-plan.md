# BSP Linux 学习路线

> 当前仓库的第一版可执行教程见 [lab-index.md](lab-index.md)。为了减少基础重复，原路线中的启动、空节点匹配和独立 Device Model 演示已经合并为真实 QEMU 设备实验；Jetson、I2C、SPI、DMA 和 Camera 相关内容暂列为后续专题。

## 1. 学习目标

从能够启动 ARM64 Linux + BusyBox 的最小系统开始，逐步理解 Linux 启动链、设备树、Linux Device Model 和常见外设驱动，最后回到 Jetson Orin Nano BSP，能够独立分析板级启动和驱动问题。

本路线优先在 QEMU `arm64 virt` 平台学习通用 Linux 机制，再迁移到真实开发板。QEMU 适合验证 Kernel、initramfs、Device Tree 和通用驱动框架；Jetson 阶段再集中处理 Tegra 特有的时钟、电源、引脚复用、固件和媒体架构。

## 2. 当前进度

| 阶段 | 内容 | 状态 | 断点 / 备注 |
| --- | --- | --- | --- |
| ① | Kernel + BusyBox | ✅ 已完成 | 已生成 ARM64 Kernel 和 BusyBox rootfs |
| ② | Linux 启动流程 | ✅ 已完成 | 已确认可以进入 BusyBox shell；后续补充启动链文档 |
| ③ | Device Tree | ✅ 已完成 | Lab 01 导出、反编译并分析 QEMU DTB |
| ④ | Platform Driver | ✅ 已完成 | Lab 02 使用 QEMU PL031 实现真实 platform driver 和 MMIO 访问 |
| ⑤ | Linux Device Model | ✅ 已完成 | Lab 05 通过真实设备观察 sysfs、uevent、bind/unbind |
| ⑥ | MMIO | ✅ 已完成 | Lab 02 使用 `platform_get_resource()`、`devm_ioremap_resource()` 和 `readl()` |
| ⑦ | IRQ | ✅ 已完成 | Lab 03 使用 PL031 alarm、GIC 和 Linux IRQ handler |
| ⑧ | GPIO subsystem | ✅ 已完成 | Lab 04 使用 PL061 GPIO consumer、descriptor 和 gpiod API |
| ⑨ | pinctrl / clock / regulator | ⬜ 待开始 | 进入真实 BSP 前的必要基础 |
| ⑩ | I2C | ⬜ 待开始 | 理解 I2C adapter、client 和设备树描述 |
| ⑪ | SPI / UART | ⬜ 待开始 | 学习常见总线和串口设备模型 |
| ⑫ | U-Boot | 🟡 可选 | Lab 07 已提供启动链脚本，需要外部 U-Boot 源码 |
| ⑬ | Storage / Filesystem | ✅ 已完成 | Lab 06 使用 virtio-blk 和 ext4 持久化 rootfs |
| ⑭ | DMA | ⬜ 待开始 | 为高吞吐外设和 Camera 做准备 |
| ⑮ | V4L2 | ⬜ 待开始 | Camera 驱动、buffer 和 userspace 接口 |
| ⑯ | Media Controller | ⬜ 待开始 | 理解 Camera pipeline 和实体连接 |
| ⑰ | Jetson BSP | ⬜ 待开始 | 回到 Orin Nano，分析 Tegra 平台实际 BSP |

## 3. 阶段依赖关系

```text
Kernel + BusyBox
        |
        v
Kernel / DTB / initrd / init 启动流程
        |
        v
Device Tree
        |
        v
Platform Driver -----> Linux Device Model / sysfs / uevent
        |
        +----> MMIO
        |
        +----> IRQ
        |
        +----> GPIO
        |
        +----> I2C / SPI / UART
        |
        +----> pinctrl / clock / regulator
        |
        +----> DMA
        |
        +----> V4L2 / Media Controller

启动与系统分支：

Kernel + BusyBox -> U-Boot -> Storage / Filesystem

最终目标：Jetson Orin Nano BSP
```

`Linux Device Model` 不是只在第⑩阶段学习的独立知识点。设备、驱动、总线、`probe()`、`sysfs`、`uevent` 和设备树匹配会从 Platform Driver 阶段开始反复出现，后续每个外设阶段都要回头联系这些概念。

## 4. 各阶段学习目标和验收标准

### ① Kernel + BusyBox

目标：构建一个可以启动到 shell 的最小 ARM64 Linux 系统。

需要掌握：

- Kernel `Image` 的生成位置和架构。
- BusyBox 静态编译和 applet 链接。
- initramfs 的目录结构。
- `/init` 的作用。
- `rootfs.cpio.gz` 的制作过程。

验收标准：

- QEMU 可以加载 Kernel 和 initramfs。
- 系统可以执行 `/init`。
- 可以进入 BusyBox shell。
- `/proc`、`/sys` 和 `/dev` 可以正常挂载。

当前状态：已完成。当前 rootfs 的启动入口是 `rootfs/init`，打包文件是 `rootfs.cpio.gz`。

### ② Linux 启动流程

目标：能够解释从 Bootloader 或 QEMU 到用户空间 shell 的完整过程。

需要掌握：

- Kernel、DTB 和 initrd/initramfs 的职责区别。
- Kernel command line 的来源和作用。
- `console=ttyAMA0` 的作用。
- `rdinit=/init` 的作用。
- Kernel 解包 initramfs 并执行第一个用户空间程序的过程。
- `/proc`、`/sys`、`/dev`、`devtmpfs` 的作用。
- Kernel 找不到 init 时的典型错误。

建议记录：

```bash
cat /proc/cmdline
cat /proc/version
mount
dmesg
```

验收标准：

- 可以解释 Kernel 如何找到并执行 `/init`。
- 可以解释 DTB 在启动过程中的传递方式。
- 可以解释 initramfs 和真实 root filesystem 的区别。
- 能够根据启动日志定位串口、initrd 或 init 参数问题。

当前状态：已完成启动验证；详细过程待记录到 `docs/02-linux-boot-flow.md`。

### ③ Device Tree

目标：理解硬件描述和驱动代码的分离方式。

需要掌握：

- DTS、DTB 和 Kernel 内部 Open Firmware 设备树的区别。
- `compatible`、`reg`、`interrupts`、`status`。
- `#address-cells` 和 `#size-cells`。
- `interrupt-parent`、`clocks`、`resets`、`pinctrl-*`。
- phandle 和节点引用。
- 设备树节点如何转化为 Linux 设备。
- `/proc/device-tree` 和 `/sys/firmware/devicetree/base` 的关系。

第一组实验：

```bash
qemu-system-aarch64 \
    -machine virt,dumpdtb=qemu-virt.dtb \
    -cpu cortex-a57 \
    -m 512M

dtc -I dtb -O dts -o qemu-virt.dts qemu-virt.dtb
rg -n 'pl011|uart|serial|memory|interrupt-controller|virtio' qemu-virt.dts
```

在运行中的系统中观察：

```bash
find /proc/device-tree -maxdepth 2 -type f
find /sys/firmware/devicetree/base -maxdepth 2 -type f
```

验收标准：

- 可以从 DTB 反编译出 DTS。
- 可以定位 QEMU 串口、内存和中断控制器节点。
- 可以解释一个设备节点的 `compatible`、`reg` 和 `interrupts`。
- 可以在 `/proc/device-tree` 或 `/sys/firmware/devicetree/base` 中找到 QEMU 真实设备节点。
- 可以解释 `status` 缺省、`status = "okay"` 和 `status = "disabled"` 对节点可用性的影响。

当前状态：已完成并由 `labs/01-device-tree` 固化为可重复实验；导出的 DTB 保存在 `out/01-device-tree/`。

### ④ Platform Driver

目标：编写第一个能够和设备树节点匹配的 Linux 驱动。

Lab 02 不再停留在只打印 `probe()`，而是直接接管 QEMU 的 PL031 RTC 节点并访问真实寄存器。驱动实现：

- `platform_driver`。
- `of_device_id` 匹配表。
- `probe()` / `remove()`。
- `platform_get_resource()` 和 `devm_ioremap_resource()`。
- `readl()` / `writel()` 访问 PL031 MMIO。
- sysfs 属性观察 RTC 数据和原始状态。
- 模块加载和卸载。

实验链路是：

```text
设备树节点
    -> platform_device
    -> compatible 匹配
    -> platform_driver
    -> probe()
    -> PL031 寄存器访问
```

验收标准：

- 模块可以针对当前 Kernel 编译。
- `insmod` 后 `probe()` 被调用并读取 PL031 `DR`。
- `rmmod` 后 `remove()` 被调用。
- 可以在 `dmesg` 和 `/sys/bus/platform/` 中观察设备和驱动关系。

### ⑤ Linux Device Model

目标：把 `device`、`driver`、`bus` 和 sysfs 关系串起来。

需要掌握：

- `struct device`。
- `struct device_driver`。
- `platform_device` / `platform_driver`。
- bus 的匹配规则。
- `probe()` 和 `remove()` 的调用时机。
- sysfs 中的 `devices`、`drivers`、`bind` 和 `unbind`。
- uevent、devtmpfs 和 mdev/udev 的关系。
- `devm_*` 资源管理。

验收标准：能够从 `/sys` 反查一个设备绑定的 driver，并通过 `unbind` / `bind` 观察重新匹配。

### ⑥ MMIO

目标：安全地访问设备寄存器。

需要掌握：

- 物理地址、虚拟地址和 MMIO 地址。
- `ioremap` 和 `devm_ioremap_resource`。
- `readl` / `writel` 及其访问顺序。
- 设备树 `reg` 到资源的转换。
- `platform_get_resource()` 和 `platform_get_irq()`。
- 资源冲突和访问越界。
- 为什么不能把 MMIO 当普通内存直接解引用。

验收标准：驱动可以从设备树获得一段资源，完成映射、读写测试，并在卸载时正确释放资源。

### ⑦ IRQ

目标：理解硬件中断从设备树到中断处理函数的完整过程。

需要掌握：

- IRQ number 和硬件中断号。
- `request_irq` / `devm_request_irq`。
- hard IRQ 和 threaded IRQ。
- 中断上下文限制。
- 中断触发类型。
- 共享中断。
- 中断统计和调试。

验收标准：能够从设备树获得 IRQ，在驱动中注册处理函数，并通过 `/proc/interrupts` 观察中断计数变化。

### ⑧ GPIO subsystem

目标：使用 Linux GPIO 子系统，而不是直接操作 SoC GPIO 寄存器。

需要掌握：

- GPIO controller 和 GPIO consumer。
- `gpiod_get`、`gpiod_set_value` 等 descriptor API。
- `gpios` 属性。
- GPIO active-low 语义。
- GPIO 输入中断。
- pinctrl 和 GPIO 的关系。

验收标准：能够通过设备树描述 GPIO，并由驱动使用 gpiod API 控制或读取 GPIO。

### ⑨ pinctrl / clock / regulator

目标：补足真实 BSP 中最常遇到的资源依赖。

需要掌握：

- 引脚复用和 pin configuration。
- 时钟获取、使能和关闭。
- 电源 regulator 获取和电压配置。
- reset 控制器。
- runtime PM 的基本概念。
- 驱动 probe 的资源依赖和延迟探测。

验收标准：能够读懂一个真实外设节点中的 `pinctrl`、`clocks`、`resets` 和 `*-supply` 属性，并解释驱动为什么需要这些资源。

### ⑩ I2C

目标：理解总线控制器、I2C client 和从设备驱动的关系。

需要掌握：

- I2C adapter、client 和 driver。
- 设备树下挂节点。
- `i2c_driver` 和 `probe()`。
- SMBus 与 I2C message。
- 寄存器型 I2C 设备的读写。
- 地址冲突和总线扫描风险。

验收标准：能够为一个简单 I2C 传感器或 EEPROM 编写 client driver，并读取一个设备寄存器。

### ⑪ SPI / UART

目标：掌握常见串行外设的 Kernel 接口。

需要掌握：

- SPI controller、chip select 和 `spi_device`。
- SPI mode、频率和字长。
- `spi_driver`。
- UART controller、TTY 子系统和串口设备节点。
- 协议层驱动与 TTY 层的区别。

验收标准：能够读懂一个 SPI/UART 外设的设备树节点和驱动初始化过程。

### ⑫ U-Boot

目标：理解完整启动链，而不只是 QEMU 直接加载 Kernel。

需要掌握：

- SPL、U-Boot proper 的基本职责。
- `bootcmd`、`bootargs` 和环境变量。
- 从存储设备加载 Kernel、DTB 和 initrd。
- `booti`、`bootm` 的基本区别。
- FIT image 的基本概念。
- U-Boot 传递和修改设备树的过程。

验收标准：能够在 U-Boot 命令行中手动加载并启动 Kernel，解释每个启动参数的来源。

### ⑬ Storage / Filesystem

目标：理解实际开发板上的启动介质和 root filesystem。

需要掌握：

- eMMC、SD、块设备和分区。
- MBR/GPT。
- ext4、FAT 和 squashfs 的使用场景。
- initramfs 与持久化 rootfs 的切换。
- `root=`、`rootwait` 和 `rootfstype=`。
- overlayfs 的基本用途。

验收标准：能够从 SD/eMMC 加载 Kernel 和 DTB，并挂载持久化 rootfs 启动用户空间。

### ⑭ DMA

目标：理解设备和内存之间的数据搬运。

需要掌握：

- DMA channel 和 DMA controller。
- coherent 与 streaming DMA。
- `dma_alloc_coherent`。
- `dma_map_single` / `dma_unmap_single`。
- cache 一致性。
- DMA buffer 生命周期和同步。
- scatter-gather 的基本概念。

验收标准：能够读懂一个使用 DMA 的外设驱动，并说明 buffer 的所有权和同步时机。

### ⑮ V4L2

目标：理解 Camera 驱动和 userspace 视频采集接口。

需要掌握：

- V4L2 sub-device。
- video device。
- format、frame size 和 frame interval。
- vb2 buffer queue。
- mmap、DMABUF 和 streaming I/O。
- sensor、CSI 接收端和 ISP 的基本角色。

验收标准：能够分析一条 Camera 数据流，并解释用户空间如何获得一帧图像。

### ⑯ Media Controller

目标：理解复杂 Camera pipeline 的实体和连接关系。

需要掌握：

- media entity。
- pad 和 link。
- sub-device。
- pipeline。
- async notifier。
- 设备树 endpoint 和 remote-endpoint。

验收标准：能够使用 media-ctl 查看并配置一个 Camera pipeline，并将其与设备树 endpoint 对应起来。

### ⑰ Jetson BSP

目标：把前面学习的通用 Linux 机制应用到 Orin Nano。

需要掌握：

- Jetson 启动链和刷机流程。
- NVIDIA L4T Kernel、DTB、DTBO 和 rootfs。
- Tegra pinctrl、clock、reset、power domain。
- BPMP 和相关固件协作。
- CSI、VI、ISP 和 Camera pipeline。
- Jetson 特有的设备树覆盖和板级配置。
- 使用 Kernel log、debugfs、sysfs 和 trace 工具定位 BSP 问题。

验收标准：能够从启动日志和设备树判断一个 Jetson 外设是否被正确描述、初始化和绑定驱动，并能定位常见的 pinmux、clock、power、interrupt 或 DMA 问题。

## 5. 当前实验入口

当前路线已经固化为 `docs/lab-index.md` 中的可执行 Lab。建议从启动基线开始，然后按真实设备链路推进；U-Boot 和 Storage 是启动与系统分支，不是 DMA 的硬性前置：

```bash
labs/00-quickstart/run.sh
labs/01-device-tree/build.sh && labs/01-device-tree/run.sh
labs/02-pl031-mmio/build.sh && labs/02-pl031-mmio/run.sh
labs/03-pl031-irq/run.sh
labs/04-pl061-gpio/run.sh
labs/06-virtio-storage/run.sh
labs/08-edu-pci/run.sh
```

Lab 05 的 sysfs、bind/unbind 操作和 Lab 07 的 U-Boot 外部源码要求，分别见对应目录的 README。当前不把只匹配空节点的练习作为必做环节。

## 6. 每个阶段的断点记录模板

每完成一个阶段，都在对应文档末尾补充以下内容：

```markdown
## 学习断点

- 完成日期：YYYY-MM-DD
- 当前阶段：
- 已掌握：
- 已完成实验：
- 关键命令：
- 关键日志：
- 仍然不清楚：
- 遇到的问题：
- 下一步：
- 相关源码位置：
```

建议文档按下面的名称持续增加：

```text
docs/02-linux-boot-flow.md	docs/03-device-tree.md
docs/04-platform-driver.md	docs/05-device-model.md
docs/06-mmio.md	docs/07-irq.md
docs/08-gpio.md	docs/09-pinctrl-clock-regulator.md
docs/10-i2c.md	docs/11-spi-uart.md
docs/12-u-boot.md	docs/13-storage-filesystem.md
docs/14-dma.md	docs/15-v4l2.md
docs/16-media-controller.md	docs/17-jetson-bsp.md
```

## 7. 学习原则

- 每个阶段都要有一个可以重复执行的实验。
- 先观察 Kernel 日志和 `/sys`，再阅读对应源码。
- 每次只引入一个主要的新概念。
- 驱动先验证匹配和生命周期，再加入硬件访问。
- 优先使用 `devm_*`、`gpiod`、`regmap` 等内核标准接口。
- 不直接依赖 SoC 私有寄存器，除非当前阶段明确学习 MMIO。
- QEMU 中验证通用机制，真实板上验证 SoC 和板级差异。
- 每次停止学习前记录“已完成、未解决、下一步”，确保下次可以从断点继续。
