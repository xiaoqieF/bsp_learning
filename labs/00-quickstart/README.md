# Lab 00：启动基线

这个目录只负责确认公共环境，不再重复讲解 Kernel 和 BusyBox 的完整构建过程。

## 执行

在仓库根目录运行：

```bash
labs/00-quickstart/run.sh
```

进入 shell 后检查：

```sh
cat /proc/cmdline
cat /proc/version
mount
ls /sys/firmware/devicetree/base
```

退出 QEMU 使用 `Ctrl-a x`。

## 通过标准

- 看到 BusyBox shell。
- `/proc`、`/sys` 和 `/dev` 已挂载。
- `console=ttyAMA0` 和 `rdinit=/init` 出现在 `/proc/cmdline`。
- `/sys/firmware/devicetree/base` 存在。
