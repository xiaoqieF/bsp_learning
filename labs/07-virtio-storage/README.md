# Lab 07：Virtio Block 与持久化 RootFS

## 这个实验实现了什么

前面的实验使用 initramfs 启动：Kernel 解压内存中的 `rootfs.cpio.gz`，然后直接执行其中的 `/init`。本实验把用户空间文件放进一个真正的 ext4 磁盘镜像，并让 Kernel 从 QEMU 提供的 VirtIO 块设备上挂载它作为根文件系统：

```text
rootfs/                     build.sh
  ├── /init       ────────▶  rootfs.ext4（ext4 文件系统）
  └── /bin/busybox                    │
                                      │ QEMU -drive + virtio-blk-device
                                      ▼
                           VirtIO-MMIO 虚拟块设备
                                      │
                                      ▼
                           Linux virtio-blk 驱动
                                      │
                              /dev/vda
                                      │
                    root=/dev/vda rootfstype=ext4
                                      ▼
                    挂载根文件系统并执行 /sbin/init
```

因此，这个 lab 练习的重点不是实现一个新的 VirtIO 驱动，而是把“块设备—文件系统—根文件系统—用户空间”这条 Linux 启动链路串起来。QEMU 提供真实可用的 `virtio-blk` 设备，Linux 内核中的 `virtio-blk` 驱动负责访问它，ext4 驱动负责解释磁盘上的文件系统，VFS 再把它挂载为 `/`。

## 学习目标

- 理解块设备和字符设备的区别：块设备以扇区为单位读写，文件系统建立在块设备之上。
- 理解 VirtIO 的分层：设备模型、VirtIO-MMIO 传输层、`virtio-blk` 功能驱动，以及上层的块层和 ext4。
- 理解 `root=`、`rootwait`、`rootfstype=` 和 `init=` 对 Kernel 启动的影响。
- 区分 initramfs 与磁盘上的持久化 rootfs：前者通常位于 RAM，后者来自外部块设备并能保存修改。
- 观察 Kernel 如何枚举 `/dev/vda`、识别 ext4，并在挂载根文件系统后执行磁盘上的 `/sbin/init`。

## 启动链路

`run.sh` 启动的是 ARM64 QEMU `virt` 机器，关键参数如下：

```text
-kernel Image
-drive if=none,file=rootfs.ext4,format=raw,id=rootdisk
-device virtio-blk-device,drive=rootdisk
-append "console=ttyAMA0 root=/dev/vda rootwait rootfstype=ext4 init=/init loglevel=8"
```

启动过程可以按下面的顺序理解：

1. QEMU 加载内核 `Image`，并在 `virt` 机器上创建一个 VirtIO-MMIO 块设备。
2. Kernel 根据设备树发现 VirtIO-MMIO 设备。配置中的 `VIRTIO`、`VIRTIO_MMIO` 和 `VIRTIO_BLK` 使对应驱动可以工作。
3. VirtIO 总线完成设备协商，`virtio-blk` 驱动注册块设备。这个实验中它通常显示为 `/dev/vda`。
4. Kernel 根据 `root=/dev/vda` 选择根设备；`rootwait` 让它在设备尚未完成枚举时等待，而不是立即报错。
5. `rootfstype=ext4` 指定使用 ext4 驱动解析该设备，成功后日志中应出现 `VFS: Mounted root (ext4 filesystem)`。
6. Kernel 在新的根文件系统中执行 `init=/sbin/init`。这里的 `/sbin/init` 是指向 BusyBox 的链接，不是通过 `-initrd` 传入的 initramfs。
7. BusyBox `init` 读取 `/etc/inittab`，执行 `/etc/init.d/rcS`，再通过 `askfirst` 在 `/dev/console`（本实验映射到 `ttyAMA0`）上启动 BusyBox shell。

注意：`/dev/vda` 是 Linux 为这个 VirtIO 块设备创建的设备节点；`virtio-blk-device` 是 QEMU 的设备选项，两者分别属于模拟器侧和 Kernel 用户可见侧，不是同一个命名空间里的名称。

## 构建过程

执行：

```bash
labs/07-virtio-storage/build.sh
```

脚本完成以下工作：

1. 从 `labs/common/env.sh` 读取 `ROOTFS_DIR` 和 `OUT_DIR` 等路径，并检查 Kernel、rootfs 和工具链是否存在。
2. 在 `out/07-virtio-storage/rootfs.ext4` 创建一个大小为 128 MiB 的普通文件。
3. 先把 `rootfs/` 和 `rootfs-overlay/` 合并到临时 staging 目录，再使用 `mkfs.ext4 -d` 初始化 ext4 并填入镜像。

这里的 `rootfs.ext4` 不是把 cpio 压缩包改了个后缀，而是一个可以被块设备逐扇区访问的 raw 磁盘镜像。镜像包含 `/sbin/init`、`/etc/inittab`、`/etc/init.d/rcS`、BusyBox、必要的目录以及从 `rootfs/` 复制的其他文件。

默认路径如下，也可以通过环境变量覆盖：

```text
Kernel：  linux-6.12/arch/arm64/boot/Image
rootfs：  rootfs/
镜像：    out/07-virtio-storage/rootfs.ext4
```

例如使用其他 rootfs 或输出目录：

```bash
ROOTFS_DIR=/path/to/rootfs OUT_DIR=/tmp/bsp-out \
  labs/07-virtio-storage/build.sh
```

## 运行与观察

启动：

```bash
labs/07-virtio-storage/run.sh
```

QEMU 使用 `-nographic`，串口日志和 BusyBox shell 都在当前终端。进入系统后建议依次观察：

```sh
cat /proc/cmdline
cat /proc/filesystems | grep ext4
mount | grep ' / '
ls -l /dev/vda
ls -l /sys/class/block/vda
```

预期现象包括：

- Kernel 日志中能看到 VirtIO 设备和 `vda` 块设备。
- 根挂载项显示设备为 `/dev/vda`，文件系统类型为 `ext4`。
- `/init` 能正常运行并进入 BusyBox shell。
- `/sys/class/block/vda` 可以看到 Kernel 创建的块设备信息。

根文件系统最初可能以只读方式挂载。为了验证写入和持久化，执行：

```sh
mount -o remount,rw /
echo hello > /root/persistent-file
sync
cat /root/persistent-file
```

保持镜像文件不变，退出当前 QEMU 后再次运行 `run.sh`：

```sh
cat /root/persistent-file
mount | grep ' / '
```

如果第二次启动仍能读到 `hello`，就证明写入已经落到 ext4 镜像，而不是只存在于某次启动的内存中。不要在验证前重新执行 `build.sh`，因为构建脚本会重新创建并格式化镜像，之前写入的数据会被覆盖。

## 自动验收

```bash
labs/07-virtio-storage/test.sh
```

测试会构建镜像，启动一次系统并写入 `/root/persistent-data`，然后再次启动同一个镜像，检查：

- 第一次启动完成 ext4 根文件系统挂载。
- 第一次启动能写入数据并执行 `sync`。
- 第二次启动能读回第一次写入的内容。

脚本使用 `timeout` 自动结束 QEMU，因此正常情况下不需要手动退出。若只想交互式观察，可以使用 `run.sh`，退出 QEMU 通常按 `Ctrl-a x`。

## 与 initramfs 启动的对比

| 项目 | initramfs 启动 | 本实验的 ext4 rootfs 启动 |
| --- | --- | --- |
| rootfs 来源 | Kernel 启动时解压到 RAM 的 cpio | VirtIO 块设备上的 ext4 镜像 |
| 根设备 | 通常没有真实块设备 | `/dev/vda` |
| 启动参数 | 常用 `rdinit=/init` | 使用 `root=/dev/vda` 和 `init=/sbin/init` |
| 写入是否天然持久 | 通常只改 RAM 中的副本 | 写入镜像后可在下次启动读回 |
| 常见用途 | 救援环境、安装程序、早期用户空间 | 常规系统根文件系统 |
| 依赖 | 需要 `BLK_DEV_INITRD` 和 initramfs | 需要 VirtIO、块层和 ext4 驱动在根挂载前可用 |

这里的 `init=/sbin/init` 表示挂载真实根文件系统后执行哪个第一个用户空间程序；`rdinit=/init` 则表示在 initramfs 阶段执行哪个程序。两者的语义和执行时机不同。

## 内核配置与相关代码

本 lab 依赖的配置在仓库根目录的构建说明中已经打开：

```text
CONFIG_VIRTIO=y
CONFIG_VIRTIO_MMIO=y
CONFIG_VIRTIO_BLK=y
CONFIG_EXT4_FS=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
```

可以结合下面的文件阅读：

- `labs/07-virtio-storage/build.sh`：创建并填充 ext4 镜像。
- `labs/07-virtio-storage/run.sh`：把镜像接到 QEMU 的 `virtio-blk-device`，并设置 Kernel command line。
- `labs/07-virtio-storage/test.sh`：用两次启动验证持久化。
- `labs/common/env.sh`：定义默认的 Kernel、rootfs 和输出目录，并检查前置条件。
- `labs/07-virtio-storage/rootfs-overlay/etc/inittab`：定义 `rcS`、控制台 shell 和 PID 1 的重启行为。
- `labs/07-virtio-storage/rootfs-overlay/etc/init.d/rcS`：挂载虚拟文件系统并完成基础用户空间初始化。
- `docs/qemu-virt.dts`：查看 QEMU `virt` 机器中 VirtIO-MMIO 控制器的设备树描述。

如果希望继续深入 Linux 源码，可以沿着这条线索阅读：

```text
VirtIO-MMIO transport → virtio core → virtio_blk → block layer → ext4 → VFS/root mount
```

本实验没有覆盖 VirtIO 队列、descriptor、request 完成中断等驱动内部细节；这些是进一步阅读 `virtio` 核心和 `virtio_blk` 源码时的重点。

## 常见问题排查

### 找不到 `/dev/vda`

确认 Kernel 配置包含 `VIRTIO`、`VIRTIO_MMIO`、`VIRTIO_BLK`，并确认 QEMU 命令行同时包含 `-drive` 和 `-device virtio-blk-device`。如果设备枚举较慢，确认有 `rootwait`。

### 找不到 ext4 或挂载失败

确认 `CONFIG_EXT4_FS=y`，并检查镜像确实由 `mkfs.ext4` 创建。若把 ext4 配置成模块，模块必须在根文件系统挂载之前可用；本实验使用内建配置以避免这个启动时序问题。

### 找不到 `/sbin/init` 或启动脚本

确认 BusyBox 已启用 `CONFIG_INIT` 和 `CONFIG_FEATURE_USE_INITTAB`，并检查镜像中存在 `/sbin/init`、`/etc/inittab` 和可执行的 `/etc/init.d/rcS`。本 lab 的 `build.sh` 会自动合并 `rootfs-overlay/`，可以重新构建镜像后再启动。

### 重启后数据消失

先确认没有重新执行 `build.sh`，因为它会 `truncate` 并重新运行 `mkfs.ext4`。写文件后执行 `sync`，并用同一个 `OUT_DIR/07-virtio-storage/rootfs.ext4` 再次启动。

## 通过标准

- 能解释 QEMU 如何把 raw ext4 镜像呈现为 VirtIO 块设备。
- 能从启动日志和 `/sys/class/block/vda` 证明 Kernel 已识别 `/dev/vda`。
- 能解释 `root=`、`rootwait`、`rootfstype=` 和 `init=` 的作用。
- 能看到根目录以 ext4 挂载，并由磁盘镜像中的 BusyBox `/sbin/init` 启动用户空间。
- 能解释 `/etc/inittab`、`rcS`、`askfirst` 和 PID 1 的关系。
- 能写入文件，重新启动后读回该文件，并说明它为什么具有持久性。
