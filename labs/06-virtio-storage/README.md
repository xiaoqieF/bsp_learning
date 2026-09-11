# Lab 06：Virtio Block 与持久化 RootFS

本实验不使用 initramfs 启动用户空间，而是让 Kernel 通过 QEMU 的 virtio-blk 设备挂载 ext4 镜像，并执行磁盘上的 `/init`。

## 构建和运行

```bash
labs/06-virtio-storage/build.sh
labs/06-virtio-storage/run.sh
```

进入系统后检查：

```sh
cat /proc/cmdline
mount
ls -l /dev/vda
```

根文件系统应该显示为 `/dev/vda` 或其 ext4 挂载结果。写入持久化目录：

```sh
mount -o remount,rw /
echo hello > /root/persistent-file
cat /root/persistent-file
```

## 重点

- `root=/dev/vda` 指定根设备。
- `rootwait` 等待 virtio 设备完成枚举。
- `rootfstype=ext4` 指定文件系统类型。
- initramfs 是早期用户空间；ext4 镜像是持久化 rootfs。
- `virtio-blk-device` 是 QEMU `virt` 上的实际虚拟块设备，不是自定义测试节点。

## 通过标准

- Kernel 能识别 `/dev/vda`。
- `/init` 从 ext4 镜像执行。
- 根目录挂载类型为 ext4。
- 能解释 initramfs 启动和持久化 rootfs 启动的差异。
