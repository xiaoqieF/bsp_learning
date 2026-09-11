# Lab 03 Starter

在 Lab 02 的资源获取基础上完成：

1. 使用 `platform_get_irq()` 获取设备树 IRQ。
2. 使用 `devm_request_irq()` 注册共享 IRQ handler。
3. 读取 `MIS`，确认 alarm 位后写 `ICR` 清除状态。
4. 添加 `alarm_seconds` 和 `irq_count` 属性，并通过 `/proc/interrupts` 验证。
