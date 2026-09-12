# Lab 03：PL031 RTC Alarm 与真实 IRQ

IRQ 代码与 Lab 02 使用同一个真实的 PL031 QEMU 设备。本实验在 Lab 02 的 MMIO 基础上新增 IRQ 注册、alarm 编程、GIC 分发和 Linux IRQ handler。学习重点是理解“硬件状态产生中断、控制器分发中断、Linux 驱动确认并清除中断”的完整闭环。

## 一、从硬件事件到 Linux handler

中断不是驱动主动反复检查寄存器，而是硬件状态满足条件后主动通知 CPU。PL031 的 alarm 工作方式可以概括为：

```text
DR 当前秒计数
        │ 与 MR 比较
        ▼
到达 match 值，设置 alarm pending
        ▼
IMSC 允许 alarm，PL031 输出 IRQ
        ▼
GIC 根据设备树 interrupts 分发
        ▼
Linux IRQ 核心调用 bsp_pl031_irq()
        ▼
读取 MIS 确认来源 → 写 ICR 清除 → 统计并返回 IRQ_HANDLED
```

设备树中的 `interrupts` 描述的是硬件连接关系，不是驱动可以直接使用的最终 IRQ 号。`platform_get_irq(pdev, 0)` 会通过 platform/firmware 描述和 IRQ domain 把它转换成 Linux IRQ number；驱动不应该从设备树文本中手工计算这个数字。`devm_request_irq()` 再把这个 Linux IRQ number 和 handler 注册到 IRQ 核心。

本实验的 PL031 节点位于 `docs/qemu-virt.dts` 的 `pl031@9010000`，其 `reg` 描述 RTC 的 MMIO 区域，`interrupts` 描述它连接到 GIC 的中断。Lab 03 的构建脚本只把 `compatible` 改成 `bsp-learn,pl031`，因此教学驱动可以匹配这个节点并接管 PL031。

## 二、PL031 alarm 的寄存器模型

本实验只使用 alarm 相关的几个寄存器：

| 寄存器 | 偏移 | 作用 |
| --- | ---: | --- |
| `DR` | `0x00` | 当前 RTC 秒计数，只读观察 |
| `MR` | `0x04` | match 值；`DR` 到达该值时产生 alarm |
| `IMSC` | `0x10` | alarm 中断屏蔽/使能，置 bit 0 允许上报 |
| `MIS` | `0x18` | 屏蔽后的 pending 状态，bit 0 表示 alarm |
| `ICR` | `0x1c` | 写 1 清除对应 pending 状态 |

向 `alarm_seconds` 写入 `5` 时，驱动先读取 `DR`，再把 `DR + 5` 写入 `MR`。因此该属性表达的是“从现在开始延迟几秒”，而不是让用户直接填写硬件的绝对 match 值。典型编程顺序是：

1. 读取当前 `DR` 并计算目标值；
2. 写入 `MR`；
3. 写 `ICR` 清除上一次可能残留的 pending 状态；
4. 写 `IMSC` 打开 alarm 中断。

`MIS` 是 handler 判断中断来源的依据。不能只因为 Linux 调用了 handler 就认定 PL031 产生了中断，尤其是使用共享 IRQ 时，必须先检查自己的状态寄存器。

## 三、Linux 驱动中的 IRQ 生命周期

### 1. probe 阶段获取资源

`probe()` 通常按以下顺序建立驱动运行环境：

```c
rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
rtc->base = devm_ioremap_resource(&pdev->dev, res);
rtc->irq = platform_get_irq(pdev, 0);
platform_set_drvdata(pdev, rtc);
```

这里有几个值得注意的边界：

- MMIO 地址必须经过 `ioremap`，使用 `readl()` / `writel()` 访问，不能把物理地址当普通指针解引用；
- `platform_get_irq()` 返回负数时表示获取失败，驱动应立即返回错误；
- 在注册 IRQ 前保存好私有数据，因为 handler 可能在注册成功后很快被调用；
- `devm_*` 资源会在 probe 失败或设备移除时自动释放，降低错误路径和 remove 的复杂度。

### 2. 注册 handler

本实验使用：

```c
ret = devm_request_irq(&pdev->dev, rtc->irq, bsp_pl031_irq,
		IRQF_SHARED, dev_name(&pdev->dev), rtc);
```

参数中的最后一个 `rtc` 会原样传给 handler 的 `void *data`，这样同一个 handler 可以通过私有数据找到自己的 MMIO 基地址和计数器。`dev_name()` 用作 `/proc/interrupts` 等调试输出中的设备名。

本实验保留 `IRQF_SHARED`，用于演示共享中断的正确写法：handler 必须读取 `MIS`，不是本设备的中断就返回 `IRQ_NONE`，确认是本设备的中断并清除后才返回 `IRQ_HANDLED`。如果硬件 IRQ 确定不会共享，也可以不设置 `IRQF_SHARED`；但无论是否共享，检查设备状态、清除 pending 的原则都不能省略。

### 3. 编写硬 IRQ handler

handler 运行在中断上下文，应该尽快完成必要的确认和清除工作，不能执行可能睡眠的操作，例如等待 mutex、访问可能睡眠的 I2C API 或调用 `msleep()`。本实验的 handler 逻辑如下：

```c
static irqreturn_t bsp_pl031_irq(int irq, void *data)
{
	struct bsp_pl031 *rtc = data;
	u32 status = readl(rtc->base + PL031_MIS);

	if (!(status & PL031_AI))
		return IRQ_NONE;

	writel(PL031_AI, rtc->base + PL031_ICR);
	atomic_inc(&rtc->irq_count);
	return IRQ_HANDLED;
}
```

顺序很重要：先确认来源，再清除硬件 pending，最后更新软件状态。`ICR` 使用 W1C（write one to clear）语义，写入 bit 0 会清除 alarm 状态；不能通过读改写方式“修改”这个寄存器。`irq_count` 可能在 handler 和 sysfs 读取路径之间并发访问，所以示例使用 `atomic_t`。

如果实际设备的中断处理需要较长时间，handler 应只做读取状态、屏蔽/清除中断和保存事件等快速工作，再通过 threaded IRQ、workqueue 或 tasklet（视内核 API 和场景选择）把较重的处理放到后半部。这个实验的动作很短，因此直接使用 hard IRQ handler 即可。

## 四、sysfs 如何驱动和验证中断

本实验用 sysfs 提供一个最小的用户空间控制面：

```text
echo 5 > alarm_seconds
        │
        ▼
读取 DR → 写 MR → 清 ICR → 开 IMSC
        │
        ▼ 约 5 秒后
PL031 设置 alarm → GIC 触发 IRQ → bsp_pl031_irq()
        │
        ▼
irq_count 加一，/proc/interrupts 计数增加
```

`alarm_seconds` 的 `store` 回调负责解析用户写入的字符串，并编程硬件；`irq_count` 的 `show` 回调只读取驱动维护的统计值。sysfs 回调运行在普通进程上下文，可以进行适合该上下文的 MMIO 操作，但不能把它和 handler 当成串行执行：用户读取计数时，handler 可能正在同时更新计数。

验证时建议同时观察三个层次：

- `irq_count`：本驱动确认处理的 alarm 次数；
- `/proc/interrupts`：Linux IRQ 核心记录的该 IRQ 触发次数；
- `dmesg`：handler 中打印的 `alarm IRQ handled` 日志。

如果 `/proc/interrupts` 增加但 `irq_count` 不增加，优先检查 `MIS` 判断、IRQ 是否属于本设备以及 handler 是否正确返回。若三者都不增加，再检查 `MR` 是否正确编程、`IMSC` 是否打开、设备树 `interrupts` 是否正确，以及 PL031 节点是否被目标驱动匹配。

## 五、卸载和资源生命周期

`devm_request_irq()`、`devm_ioremap_resource()`、`devm_kzalloc()` 和 `devm_device_add_group()` 都绑定到 `&pdev->dev` 的生命周期。`rmmod` 触发 platform driver 的 remove 后，内核会自动注销 IRQ、移除 sysfs 属性、解除 MMIO 映射并释放私有内存。驱动的 `remove()` 因此只需要处理非 devm 资源；本实验中保留日志即可。

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
