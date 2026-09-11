# Lab 04 Starter

完成以下 GPIO consumer 任务：

1. 使用 `devm_gpiod_get()` 获取 `led` 和 `input` descriptor。
2. 为 LED 设置 `GPIOD_OUT_LOW`，为 input 设置 `GPIOD_IN`。
3. 使用 `gpiod_set_value_cansleep()` 和 `gpiod_get_value_cansleep()` 实现属性访问。
4. 不在 consumer 中手工处理 active-low。
