# Lab 02：真实 Platform Driver 与 PL031 MMIO

本实验把 QEMU 的 PL031 RTC 节点改为 `bsp-learn,pl031`，由教学驱动接管。硬件寄存器仍然是 QEMU 实现的 PL031 寄存器，不是空的测试节点。本实验只学习 platform driver、资源获取和 MMIO；alarm IRQ 放到 Lab 03。

## 构建和运行

```bash
labs/02-pl031-mmio/build.sh
labs/02-pl031-mmio/run.sh
```

进入 QEMU 后：

```sh
insmod /lib/modules/bsp_pl031.ko
dmesg | grep bsp_pl031
for device in /sys/bus/platform/devices/*; do
    [ -e "$device/reg_data" ] || continue
    echo "$device/reg_data"
done
```

找到教学设备目录后执行：

```sh
cat /sys/bus/platform/devices/<device>/reg_data
cat /sys/bus/platform/devices/<device>/raw_status
```

## 重点源码

- `of_device_id` 完成 `compatible` 匹配。
- `platform_get_resource()` 获取 `reg`。
- `devm_ioremap_resource()` 建立 MMIO 映射。
- `readl()` 读取 PL031 `DR` 寄存器。
- sysfs 属性用于观察硬件状态。

构建只生成 `out/02-pl031-mmio/qemu-virt-pl031.dtb`；基础 DTB 通过 `fdtput` 按节点路径修改，不再依赖反编译 DTS 文本。

## 通过标准

- `insmod` 后能看到 `probe OK`。
- `reg_data` 能读取非空的 QEMU RTC 计数。
- `raw_status` 能读取 PL031 原始中断状态。
- `rmmod` 后能看到 `remove OK`。

## 学习任务

1. 按 `starter/README.md` 的任务拆分补全资源获取和 MMIO 映射。
2. 创建 `reg_data` 和 `raw_status` 两个只读 sysfs 属性。
3. 对照 `solution/README.md` 中的参考实现位置验证 `probe()` 和 `remove()` 生命周期。
