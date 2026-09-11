# Lab 08 Starter

完成以下 PCI driver 任务：

1. 使用 `pci_device_id` 匹配 QEMU EDU 的 vendor/device ID。
2. 启用设备并映射 BAR0。
3. 使用 `pci_set_drvdata()` 保存私有数据。
4. 注册 IRQ handler，读取并清除中断状态。
