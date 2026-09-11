# Lab 02 Starter

从 `src/bsp_pl031.c` 中提取最小的 platform driver，完成以下任务：

1. 获取 `IORESOURCE_MEM` 资源并调用 `devm_ioremap_resource()`。
2. 保存驱动私有数据并实现 `reg_data`、`raw_status` 属性。
3. 只验证 `probe()`、`remove()` 和 MMIO 读取，不注册 IRQ。

构建时使用当前目录的 `src` 作为参考实现；建议学习者先复制源码到本目录并逐步补全。
