# Lab 03：PL031 RTC Alarm 与真实 IRQ

IRQ 代码与 Lab 02 使用同一个真实的 PL031 QEMU 设备。本实验在 Lab 02 的 MMIO 基础上新增 IRQ 注册、alarm 编程、GIC 分发和 Linux IRQ handler。

## 构建和运行

```bash
labs/03-pl031-irq/build.sh
labs/03-pl031-irq/run.sh
```

进入系统后：

```sh
insmod /lib/modules/bsp_pl031.ko
for device in /sys/bus/platform/devices/*; do
    [ -e "$device/alarm_seconds" ] || continue
    echo "$device/alarm_seconds"
done
cat /proc/interrupts
```

向找到的设备写入几秒后的 alarm：

```sh
echo 5 > /sys/bus/platform/devices/<device>/alarm_seconds
cat /sys/bus/platform/devices/<device>/irq_count
sleep 6
cat /sys/bus/platform/devices/<device>/irq_count
dmesg | grep 'alarm IRQ handled'
```

## 观察重点

```text
DTS interrupts
    -> platform_get_irq()
    -> devm_request_irq()
    -> PL031 alarm
    -> GIC
    -> bsp_pl031_irq()
```

handler 先读取 `MIS`，确认确实是 alarm，再写 `ICR` 清除中断，最后增加计数。`alarm_seconds` 写入的是相对于当前 PL031 秒计数的偏移。

## 通过标准

- `/proc/interrupts` 中对应 IRQ 计数增加。
- `irq_count` 从 0 增加到 1。
- 日志出现 `alarm IRQ handled`。
- `rmmod` 后设备资源和中断被 devm 自动释放。

## 学习任务

1. 将 Lab 02 的资源获取代码迁移到本实验。
2. 使用 `platform_get_irq()` 和 `devm_request_irq()` 注册 handler。
3. 编程 PL031 alarm，确认 `MIS`、`ICR` 和 IRQ 计数的变化。
4. 对照 `solution/README.md` 中的参考实现位置检查中断确认、清除和生命周期。
