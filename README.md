# BSP Linux QEMU Labs

这是一套基于 Linux 6.12、BusyBox 和 QEMU ARM64 `virt` 的 BSP/Linux 驱动学习实验。实验优先使用 QEMU 中真实存在的设备，覆盖启动流程、Device Tree、Platform Driver、MMIO、IRQ、GPIO、Virtio 存储和 PCI 驱动。

## 仓库边界

仓库只保存实验源码、脚本、文档和可复现构建所需的文本输入。以下内容故意不纳入 Git：

- `linux-6.12/` 和 `busybox-1.36.1/`：第三方源码 checkout，目录内也可能有嵌套 Git 仓库。
- `out/`：每个 Lab 的 DTB、initramfs、模块、ext4 镜像和临时 rootfs。
- `rootfs/` 中除 `init` 外的内容：BusyBox 安装结果和符号链接。
- 内核的 `Image`、`vmlinux`、`*.o`、`*.ko`、`Module.symvers` 等编译产物。
- `*.dtb`、`*.cpio.gz`、`*.ext4`、`*.img` 和 QEMU/U-Boot 镜像。

基础设备树的可追踪输入是 `docs/qemu-virt.dts`；各脚本在构建时重新生成 DTB。因此，即使删除所有忽略的文件，仍可按下面步骤重新准备环境。

## 依赖环境

以下命令适用于 Debian/Ubuntu：

```bash
sudo apt update
sudo apt install -y \
    bc bison build-essential cpio curl device-tree-compiler e2fsprogs flex \
    gcc-aarch64-linux-gnu git gzip libelf-dev libncurses-dev libssl-dev \
    make qemu-system-arm rsync util-linux
```

构建和实验默认使用：

```text
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
QEMU machine=virt, CPU=cortex-a57, RAM=1G
console=ttyAMA0
```

检查工具是否齐全：

```bash
labs/common/check-tools.sh
```

这个检查还会确认 `linux-6.12/.config`、内核 `Image` 和 `rootfs/init` 已经存在。

## 准备源码和工具链

源码目录名称需要保持不变，因为实验脚本默认从这两个路径寻找依赖。也可以通过 `KERNEL_DIR`、`BUSYBOX_DIR` 和 `CROSS_COMPILE` 覆盖默认值。

### Linux 内核

当前实验使用 Linux `v6.12.107`：

```bash
git clone --depth 1 --branch v6.12.107 \
    https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git \
    linux-6.12
```

生成 ARM64 配置并打开实验所需的功能：

```bash
make -C linux-6.12 ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- defconfig

linux-6.12/scripts/config --file linux-6.12/.config \
    --enable BLK_DEV_INITRD \
    --enable DEVTMPFS \
    --enable DEVTMPFS_MOUNT \
    --enable DEBUG_FS \
    --enable EXT4_FS \
    --enable GPIO_PL061 \
    --enable MODULES \
    --enable PCI \
    --enable PCI_HOST_GENERIC \
    --enable RTC_CLASS \
    --enable RTC_DRV_PL031 \
    --enable VIRTIO \
    --enable VIRTIO_BLK \
    --enable VIRTIO_MMIO

make -C linux-6.12 ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make -C linux-6.12 ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)" Image modules
```

`Image` 和 `Module.symvers` 是 Lab 启动和外部模块编译所需的关键产物。首次构建外部模块时，`labs/common/build-module.sh` 也会在缺少 `Module.symvers` 时自动执行一次内核模块构建。

### BusyBox 和 initramfs rootfs

下载并解压 BusyBox `1.36.1`：

```bash
curl -LO https://busybox.net/downloads/busybox-1.36.1.tar.bz2
tar -xjf busybox-1.36.1.tar.bz2
```

编译静态 BusyBox，并安装到本仓库的 `rootfs/`：

```bash
make -C busybox-1.36.1 defconfig
sed -i 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/' \
    busybox-1.36.1/.config
make -C busybox-1.36.1 olddefconfig
make -C busybox-1.36.1 CROSS_COMPILE=aarch64-linux-gnu- \
    -j"$(nproc)"
make -C busybox-1.36.1 CROSS_COMPILE=aarch64-linux-gnu- \
    CONFIG_PREFIX="$PWD/rootfs" install
mkdir -p rootfs/dev rootfs/etc rootfs/proc rootfs/root rootfs/sys rootfs/tmp
```

仓库中的 `rootfs/init` 会挂载 `/proc`、`/sys`、`/dev` 和 debugfs，然后进入 BusyBox shell。若重新生成了 rootfs，请确认它仍然存在且可执行：

```bash
test -x rootfs/init
file rootfs/bin/busybox
```

也可以把内核和 BusyBox 放在其他路径，通过环境变量运行脚本：

```bash
KERNEL_DIR=/path/to/linux-6.12 \
BUSYBOX_DIR=/path/to/busybox-1.36.1 \
CROSS_COMPILE=aarch64-linux-gnu- \
labs/common/check-tools.sh
```

注意：部分实验的 `src/Makefile` 使用默认的 `linux-6.12` 相对路径，若移动内核目录，优先保留默认目录名，或同步调整对应 Makefile 的 `KERNEL_DIR`。

## VS Code 驱动代码提示和跳转

仓库提供了 VS Code 工作区配置，使用 `clangd` 读取 Linux Kbuild 的真实编译参数，因此可以识别内核生成头文件、配置宏、ARM64 交叉编译选项，并支持内核 API 的补全、定义跳转和引用查找。根目录的 `.clangd` 会过滤当前 clangd 版本无法解析的少数 GCC/Kbuild 专用参数。

先在 VS Code 扩展中安装推荐的 `clangd` 扩展，然后在仓库根目录执行：

```bash
labs/common/generate-compile-commands.sh
```

也可以按 `Ctrl+Shift+P`，运行 `Tasks: Run Task`，选择 `Generate kernel module compile database`。之后打开 `labs/*/src/*.c` 文件即可使用提示和跳转；内核源码中的定义也会被索引。每次修改内核配置或重新生成内核后，再运行一次这个任务即可刷新数据库。

注意：生成脚本会先用当前 `linux-6.12/.config` 重新检查并构建各个外部模块，所以必须先完成内核配置，并确保 `aarch64-linux-gnu-gcc` 可用。

## 快速开始

在仓库根目录执行：

```bash
labs/common/check-tools.sh
labs/00-quickstart/run.sh
```

看到 BusyBox shell 后，可以检查：

```sh
cat /proc/cmdline
cat /proc/version
mount
ls /sys/firmware/devicetree/base
```

退出 QEMU 使用 `Ctrl-a x`。首次运行某个 Lab 时，它会把 rootfs 和该 Lab 所需的模块打包到 `out/<lab-name>/`。

## 实验顺序

推荐顺序如下：

```bash
labs/01-device-tree/build.sh
labs/01-device-tree/run.sh
labs/02-pl031-mmio/test.sh
labs/03-pl031-irq/test.sh
labs/04-pl061-gpio/test.sh
labs/05-device-model/test.sh
labs/06-virtio-storage/test.sh
labs/08-edu-pci/test.sh
```

各实验的构建命令、QEMU 内操作、源码重点和通过标准见 [docs/lab-index.md](docs/lab-index.md)：

- Lab 01：观察 QEMU `virt` 的真实 Device Tree。
- Lab 02/03：使用 PL031 RTC 学习 platform driver、MMIO 和 alarm IRQ。
- Lab 04：使用 PL061 GPIO controller 和 GPIO consumer API。
- Lab 05：观察 Linux Device Model、sysfs 以及真实设备的 bind/unbind。
- Lab 06：使用 virtio-blk 和 ext4 持久化 rootfs。
- Lab 07：可选演示 U-Boot 启动链，需要外部 U-Boot 源码。
- Lab 08：使用 QEMU EDU PCI 设备学习 BAR、MMIO 和 IRQ。

单独运行交互式实验时，使用对应的 `build.sh` 和 `run.sh`；自动验收使用对应的 `test.sh`。构建结果都在 `out/`，可安全删除后重新生成：

```bash
rm -rf out
```

## U-Boot 可选实验

Lab 07 不把 U-Boot 源码复制进本仓库。准备外部 checkout 后执行：

```bash
U_BOOT_DIR=/path/to/u-boot labs/07-u-boot/build.sh
U_BOOT_DIR=/path/to/u-boot labs/07-u-boot/run.sh
```

## 继续学习

完整学习路线见 [docs/learning-plan.md](docs/learning-plan.md)。I2C、SPI、DMA、V4L2、Media Controller 和 Jetson 专用 BSP 暂列为后续专题。
