# Lab 07：U-Boot 启动链（可选）

本实验只在需要完整 Bootloader 链路时执行。仓库当前没有 U-Boot 源码，因此脚本通过 `U_BOOT_DIR` 使用外部 U-Boot checkout，不把第三方源码复制进本仓库。

## 构建

```bash
U_BOOT_DIR=/path/to/u-boot labs/07-u-boot/build.sh
```

脚本执行 `qemu_arm64_defconfig` 和 ARM64 构建，并准备包含以下文件的 ext4 镜像：

```text
/Image
/qemu-virt.dtb
/rootfs.cpio.gz
```

## 启动

```bash
U_BOOT_DIR=/path/to/u-boot labs/07-u-boot/run.sh
```

在 U-Boot 命令行中查看设备：

```text
virtio scan
ext4ls virtio 0 /
```

然后根据 U-Boot 实际打印的地址执行：

```text
ext4load virtio 0 ${kernel_addr_r} /Image
ext4load virtio 0 ${fdt_addr_r} /qemu-virt.dtb
ext4load virtio 0 ${ramdisk_addr_r} /rootfs.cpio.gz
setenv bootargs console=ttyAMA0 rdinit=/init loglevel=8
booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}
```

不同 U-Boot 版本的 `virtio` 命令和内存变量可能不同，以当前版本的 `help` 和 `printenv` 为准。

## 学习重点

- QEMU 直接加载 Kernel 时，Kernel、DTB、initrd 由 QEMU 传递。
- U-Boot 方式由 Bootloader 负责加载和传递这三个对象。
- `bootargs` 的来源从 QEMU `-append` 变为 U-Boot 环境变量。
- `booti` 用于 ARM64 Linux `Image`。
