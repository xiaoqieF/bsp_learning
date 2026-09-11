# Lab 08：QEMU EDU PCI、BAR、MMIO 与 IRQ

QEMU `edu` 是一个教学型 PCI 设备，提供 PCI BAR、寄存器和可触发的中断。它不依赖设备树，适合在 Platform Driver 之后理解 PCI 总线驱动。

## 构建和运行

```bash
labs/08-edu-pci/build.sh
labs/08-edu-pci/run.sh
```

进入系统后：

```sh
insmod /lib/modules/bsp_edu.ko
dmesg | grep bsp-edu
for device in /sys/bus/pci/devices/*; do
    [ -e "$device/id" ] || continue
    echo "$device"
done
```

找到设备目录后：

```sh
cat /sys/bus/pci/devices/<domain:bus:slot.func>/id
cat /sys/bus/pci/devices/<domain:bus:slot.func>/irq_count
echo 1 > /sys/bus/pci/devices/<domain:bus:slot.func>/raise_irq
cat /sys/bus/pci/devices/<domain:bus:slot.func>/irq_count
dmesg | grep 'IRQ handled'
```

## 重点

- `pci_device_id` 匹配 vendor/device ID。
- `pcim_enable_device()` 启用 PCI 设备。
- `pcim_iomap_regions()` 申请并映射 BAR0。
- `readl()`/`writel()` 访问 QEMU EDU 的 MMIO 寄存器。
- `devm_request_irq()` 注册中断处理函数。
- `pci_set_drvdata()` 保存设备私有数据。

handler 从 `0x24` 读取 EDU 中断状态，并向 `0x64` 写入 acknowledge 值清除它；这两个寄存器的语义来自 QEMU EDU 设备规格。

## 通过标准

- `probe OK` 日志显示 BAR0、IRQ 和设备 ID。
- `id` 能读出 QEMU EDU 寄存器值。
- 写入 `raise_irq` 后中断计数增加。
- 能解释 PCI BAR 和设备树 `reg` 的区别。

## 学习任务

1. 按 `starter/README.md` 的任务拆分完成 PCI ID 匹配和 BAR0 映射。
2. 使用 `pcim_enable_device()`、`pcim_iomap_regions()` 和 `pci_set_drvdata()`。
3. 注册 IRQ handler，清除设备状态并统计触发次数。
4. 对照 `solution/README.md` 中的参考实现位置检查 BAR、MMIO 和 IRQ 生命周期。
