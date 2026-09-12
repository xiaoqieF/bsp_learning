# Lab 06：RTC 字符设备用户接口

本实验承接 Lab 02/03：仍然访问 QEMU 的 PL031 RTC，但把内核驱动从 sysfs 扩展为真正的字符设备 `/dev/bsp_rtc0`。它集中演示 `cdev`、`device_create()`、`read()`、`write()`、`poll()`（用户态对应 `select()`）和 `ioctl()`。

## 构建和运行

```bash
labs/06-char-device/build.sh
labs/06-char-device/run.sh
```

进入 QEMU 后，先运行完整示例：

```sh
/bin/bsp_rtc_demo /dev/bsp_rtc0 2
```

示例会通过 `write()` 设置 2 秒 alarm，通过 `select()` 阻塞等待可读事件，通过 `read()` 取出事件，再通过 `ioctl(BSP_RTC_GET_COUNT)` 查询累计中断数。

也可以手工观察设备：

```sh
ls -l /dev/bsp_rtc0
echo 3 > /dev/bsp_rtc0
dd if=/dev/bsp_rtc0 bs=8 count=1 2>/dev/null | od -An -tu4
```

## 重点

- `alloc_chrdev_region()`、`cdev_add()` 注册字符设备号和 file operations。
- `open()` 保存 `file->private_data`，并在内核日志中打印打开模式；`release()` 打印关闭事件。
- `class_create()` 与 `device_create()` 让 devtmpfs 创建 `/dev/bsp_rtc0`。
- `write()` 接收相对秒数并设置 PL031 alarm。
- IRQ handler 递增事件计数并唤醒等待队列。
- `poll()` 返回 `EPOLLIN`，用户态可以用 `select()` 等待而不忙轮询。
- `read()` 返回 `{ sequence, rtc_seconds }` 固定 8 字节事件结构。
- `ioctl()` 使用 `_IOR` 查询已处理的 alarm 次数。

## 通过标准

- `ls -l /dev/bsp_rtc0` 能看到字符设备。
- 示例输出包含 `select: ready`、`read: sequence=` 和 `ioctl: count=`。
- `rmmod bsp_rtc_chrdev` 后能看到 `remove OK`，字符设备节点消失。

## 学习任务

1. 找出 `struct file_operations` 中四个用户态入口。
2. 解释为什么 IRQ 中不能直接复制数据到用户缓冲区。
3. 将示例中的 `select()` 改成 `poll()`，比较两者的用户态接口。
4. 增加一个 `_IOW` ioctl，让用户态设置 alarm，并比较它和 `write()` 的语义。
