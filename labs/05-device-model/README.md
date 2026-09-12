# Lab 05：Linux Device Model、sysfs 与重新绑定

本实验用一个真实的 QEMU `virt` 设备，把 Linux Device Model 中最容易混淆的几个对象串起来：`device`、`driver`、`bus`、设备树节点、`probe()`、`remove()` 和 sysfs。

实验不再创建一个只用于测试的空 platform 设备，而是复用 Lab 02 的 PL031 RTC。Lab 02 的驱动在 `probe()` 中申请 MMIO 资源、建立映射并创建 `reg_data`/`raw_status` 属性；本实验通过 sysfs 找到这个设备，再手动 `unbind` 和 `bind`，观察同一个设备如何经历 `remove()` 和再次 `probe()`。

## 1. 学习目标

完成本实验后，应该能够：

- 解释设备树节点如何成为 Linux 中的 `platform_device`；
- 区分 `struct device`、`struct device_driver` 和 `struct bus_type` 的职责；
- 解释 platform 总线如何使用 `compatible` 将设备和驱动匹配起来；
- 从 `/sys` 反查设备属于哪个 bus、当前绑定了哪个 driver；
- 使用 `uevent`、`modalias`、`of_node` 和符号链接辅助驱动调试；
- 用 `unbind`/`bind` 触发真实的 `remove()`/`probe()`；
- 说明 `devm_*` 资源为什么会在设备移除时自动回收。

本实验关注的是 Linux 的“设备生命周期和对象关系”，不是 PL031 寄存器本身。MMIO 细节见 [Lab 02](../02-pl031-mmio/README.md)，alarm 和 IRQ 细节见 [Lab 03](../03-pl031-irq/README.md)，GPIO consumer 的分层见 [Lab 04](../04-pl061-gpio/README.md)。

## 2. Device Model 的整体关系

可以先把 Device Model 看成三类对象和一条匹配路径：

```text
设备树节点
    │  compatible = "bsp-learn,pl031"
    │  reg = <...>
    ▼
platform_device（设备实例）
    │
    │  注册到 platform_bus_type
    │
    ├─────────────── platform 总线 ───────────────┐
    │                                             │
    ▼                                             ▼
/sys/bus/platform/devices/<设备名>       /sys/bus/platform/drivers/bsp-pl031
                                                  │
                                                  │ platform_driver
                                                  ▼
                                         compatible 匹配成功
                                                  │
                                                  ▼
                                             probe(pdev)
```

这里的“设备”不是指 `/dev` 下的设备文件。Device Model 的 `device` 是内核对象，用于描述一个设备实例及其父子关系、总线归属、资源和驱动绑定状态；字符设备、块设备等用户接口只是建立在这些内核对象之上的另一层。

### 2.1 三个核心对象

| 对象 | 代表什么 | 在本实验中的实例 |
| --- | --- | --- |
| `struct device` | 一个设备实例的通用内核对象 | PL031 对应的 `pdev->dev` |
| `struct device_driver` | 驱动的通用描述和回调 | `bsp-pl031` 驱动中的 `.driver` |
| `struct bus_type` | 管理设备、驱动和匹配规则 | `platform_bus_type` |

`struct platform_device` 和 `struct platform_driver` 是 platform 总线对通用对象的封装：前者包含 `struct device`，后者包含 `struct device_driver`，并提供 platform 专用的 `probe()`/`remove()` 调用方式。

设备和驱动分别注册到同一个 bus 后，bus 的匹配函数会尝试把它们配对。匹配成功时，内核调用驱动的 `probe()`；绑定关系建立后，设备目录中的 `driver` 符号链接就会出现。

### 2.2 设备树在其中的位置

设备树只描述硬件，不执行驱动代码。例如基础设备树中的 PL031 节点位于 `pl031@9010000`，包含 MMIO 地址、长度、中断和 `compatible` 等信息。Lab 02 的构建脚本只把它的 `compatible` 改成：

```dts
compatible = "bsp-learn,pl031";
```

内核启动时解析设备树并注册 platform 设备。模块加载时，`bsp_pl031` 注册一个 `platform_driver`，其匹配表包含同一个 compatible：

```c
static const struct of_device_id bsp_pl031_of_match[] = {
	{ .compatible = "bsp-learn,pl031" },
	{ }
};
```

因此模块加载后，platform 总线能够把已有的设备交给这个驱动。注意：`compatible` 是匹配依据，不是设备名；`pl031@9010000` 是设备树节点名，而 sysfs 中显示的设备名可能还会带总线编号或其他平台信息，不能凭猜测硬编码。

## 3. 从 probe 到 sysfs

Lab 02 驱动的关键流程可以简化为：

```text
加载 bsp_pl031.ko
        │
        ▼
注册 platform_driver
        │
        ▼
platform 总线匹配 bsp-learn,pl031
        │
        ▼
bsp_pl031_probe()
        ├─ devm_kzalloc()                 分配私有数据
        ├─ platform_get_resource()        获取设备树 reg
        ├─ devm_ioremap_resource()        映射 MMIO
        ├─ platform_set_drvdata()         保存私有数据
        └─ devm_device_add_group()        创建 sysfs 属性
```

`probe()` 返回 `0` 后，设备才算成功绑定。此时可以看到：

- `reg_data`：读取 PL031 `DR` 寄存器；
- `raw_status`：读取 PL031 `RIS` 原始中断状态；
- `driver`：指向已绑定的 `bsp-pl031`；
- `subsystem`：指向 `platform` 总线；
- `of_node`：指向对应的设备树节点。

sysfs 属性不是普通文件。读取 `reg_data` 时，内核会调用驱动的 `reg_data_show()`，由它执行 `readl()` 并把结果格式化后返回给用户空间。属性由 `probe()` 创建，在设备解绑或驱动卸载时被移除。

## 4. 准备环境并启动

在仓库根目录执行：

```bash
labs/common/check-tools.sh
labs/02-pl031-mmio/build.sh
labs/02-pl031-mmio/run.sh
```

进入 QEMU shell 后加载 Lab 02 驱动：

```sh
insmod /lib/modules/bsp_pl031.ko
dmesg | grep bsp_pl031
```

预期能看到类似以下日志：

```text
bsp-pl031 9010000.pl031: probe OK: DR=0x........
```

设备名中的具体数字可能随内核版本或设备树生成方式变化。后续命令都通过遍历和符号链接读取实际路径，避免依赖固定设备名。

## 5. 观察 sysfs 对象

### 5.1 bus 下的设备和驱动

```sh
ls -l /sys/bus/platform/devices
ls -l /sys/bus/platform/drivers
```

`/sys/bus/platform/devices/` 和 `/sys/bus/platform/drivers/` 下的条目通常是指向 `/sys/devices/...` 和 `/sys/bus/...` 的符号链接。它们是按总线组织对象的便捷视图；设备真正的层级关系保存在 `/sys/devices/` 中。

列出本实验驱动创建的属性：

```sh
for device in /sys/bus/platform/devices/*; do
	[ -e "$device/reg_data" ] || continue
	echo "device: $device"
	cat "$device/reg_data"
	cat "$device/raw_status"
done
```

列出支持手动绑定的 platform 驱动：

```sh
for driver in /sys/bus/platform/drivers/*; do
	[ -e "$driver/bind" ] || continue
	echo "driver: $driver"
done
```

### 5.2 反查绑定关系

```sh
for device in /sys/bus/platform/devices/*; do
	[ -e "$device/driver" ] || continue
	echo "$device -> $(readlink "$device/driver")"
done
```

重点观察本实验设备的 `driver` 链接：它应当指向 `bsp-pl031`。没有 `driver` 链接通常表示设备当前没有绑定驱动，并不代表设备不存在。

### 5.3 观察关键字段

先保存设备路径：

```sh
device=
for candidate in /sys/bus/platform/devices/*; do
	[ -e "$candidate/reg_data" ] || continue
	device=$candidate
	break
done
[ -n "$device" ] || exit 1
echo "$device"
```

然后查看通用属性：

```sh
ls -l "$device"
cat "$device/uevent"
cat "$device/modalias"
readlink "$device/subsystem"
readlink "$device/of_node"
readlink "$device/driver"
```

这些字段的含义如下：

| 路径或字段 | 用途 |
| --- | --- |
| `driver` | 当前绑定的驱动；没有该链接表示未绑定 |
| `subsystem` | 设备所属的 bus，这里应为 `platform` |
| `of_node` | 对应设备树节点的链接 |
| `modalias` | 供模块自动加载和匹配使用的别名 |
| `uevent` | 设备生成 uevent 时可提供给用户空间的环境变量 |
| `reg_data`/`raw_status` | 本实验驱动创建的硬件观察接口 |

设备树属性通常是二进制形式保存的。例如 `compatible` 可能包含多个以 NUL 分隔的字符串，不能总是直接用普通 `cat` 得到可读输出。可以使用：

```sh
tr '\000' ' ' < "$device/of_node/compatible"
echo
```

### 5.4 从 driver 侧观察设备

```sh
driver=/sys/bus/platform/drivers/bsp-pl031
ls -l "$driver"
cat "$driver/uevent" 2>/dev/null || true
```

驱动目录中的设备链接表示当前由该驱动管理的设备。 `bind` 和 `unbind` 是该 driver 暴露的手动控制入口；向它们写入设备名会请求 platform 总线执行绑定或解绑。

## 6. unbind/bind：观察完整生命周期

### 6.1 解绑设备

```sh
driver=/sys/bus/platform/drivers/bsp-pl031
name=$(basename "$device")

echo "$name" > "$driver/unbind"
dmesg | tail -n 20
```

解绑不是删除设备树节点，也不是关闭整个 platform 总线。它只解除该设备与当前驱动的绑定。内核会调用：

```text
driver/unbind
    ▼
bsp_pl031_remove()
    ▼
devm 资源清理、驱动属性移除、driver 链接消失
```

此时验证属性已消失：

```sh
[ ! -e "$device/reg_data" ] && echo "reg_data removed"
[ ! -e "$device/raw_status" ] && echo "raw_status removed"
[ ! -e "$device/driver" ] && echo "device is unbound"
```

设备目录本身通常仍然存在，因为设备实例还存在，只是暂时没有 driver。再次执行 `insmod` 可能提示模块已经加载，不能用它代替 `bind` 来测试重新绑定。

### 6.2 重新绑定设备

```sh
echo "$name" > "$driver/bind"
dmesg | tail -n 20
cat "$device/reg_data"
cat "$device/raw_status"
```

重新绑定会再次调用 `probe()`：

```text
driver/bind
    ▼
platform 总线重新匹配
    ▼
bsp_pl031_probe()
    ├─ 重新获取 reg 资源
    ├─ 重新建立 MMIO 映射
    ├─ 重新保存 drvdata
    └─ 重新创建 reg_data/raw_status
```

预期日志同时包含 `remove OK` 和 `probe OK`。如果 `probe()` 中任一步失败，设备会保持未绑定状态，属性也不会完整出现；此时应先检查 `dmesg` 中的错误，再决定是否重试。

## 7. 为什么 devm 能自动清理

本实验驱动使用了以下 devres（device-managed resource）接口：

```c
devm_kzalloc(&pdev->dev, ...)
devm_ioremap_resource(&pdev->dev, res)
devm_device_add_group(&pdev->dev, &bsp_pl031_group)
```

它们都把资源挂到 `&pdev->dev` 的生命周期上，因此有两条清理路径：

- `probe()` 中途失败：内核自动释放已经成功申请的资源；
- 设备移除或驱动解绑：调用 `remove()` 后自动释放 devm 资源。

所以本实验的 `remove()` 只打印 `remove OK`，不需要手动调用 `iounmap()`、`kfree()` 或移除属性组。这里的“自动”不是资源永远存在，而是资源的所有权绑定到了这个 `struct device`。如果驱动使用了普通 `kmalloc()`、手动 `request_irq()` 或其他非 devm API，就仍然必须在错误路径和 `remove()` 中配对释放。

## 8. uevent、modalias 与用户空间

Device Model 发生添加、移除、绑定等事件时，可以向用户空间发送 uevent。内核提供事件信息，用户空间的 `udev`、`mdev` 或其他 hotplug 程序可以据此加载模块、创建设备节点或执行规则。

要区分三件事：

1. **sysfs 对象**：描述内核中的 device/driver/bus 关系；
2. **devtmpfs**：为已经注册的字符设备、块设备等创建 `/dev` 节点；
3. **udev/mdev**：根据 uevent 在用户空间进一步管理 `/dev` 权限、名称和规则。

本实验的 PL031 教学驱动只创建 sysfs 属性，没有注册 RTC 字符接口，因此重点观察 `/sys`，不会因为 `insmod` 就自动出现一个可用的 `/dev/rtc*`。 `modalias` 主要服务于自动匹配和模块加载，不是用户程序访问设备的路径。

可以手动查看设备的 uevent 内容：

```sh
cat "$device/uevent"
```

不同内核配置和设备注册路径可能产生略有不同的字段。判断实验是否成功时，应优先关注 driver 链接、属性是否出现以及 `probe/remove` 日志，不要依赖某一组固定的 uevent 字段顺序。

## 9. 常见误区与排错顺序

### 设备目录找不到

确认已经启动的是 Lab 02 的 QEMU，并且加载了正确模块：

```sh
ls /sys/bus/platform/devices
ls -l /lib/modules/bsp_pl031.ko
dmesg | tail -n 30
```

### 有设备但没有 `reg_data`

这通常意味着设备还没有绑定 `bsp-pl031`，或者 `probe()` 失败。检查：

```sh
ls -l /sys/bus/platform/devices/<device>/driver 2>/dev/null
dmesg | grep -E 'bsp_pl031|bsp-pl031'
```

### `bind` 返回错误

常见原因包括：设备名写错、驱动目录写错、设备已绑定其他驱动，或者 `probe()` 重新申请资源失败。始终从实际 sysfs 路径取得设备名：

```sh
name=$(basename "$device")
echo "$name" > /sys/bus/platform/drivers/bsp-pl031/bind
```

### 把 `/sys/devices` 与 `/sys/bus/.../devices` 当成两份设备

后者通常只是前者的符号链接视图。判断是否为同一个对象，可以使用：

```sh
readlink -f /sys/bus/platform/devices/<device>
readlink -f /sys/bus/platform/drivers/bsp-pl031
```

### 把 `unbind` 当成 `rmmod`

`unbind` 只影响一个设备和一个驱动的绑定，模块仍然可以留在内核中；`rmmod` 则会卸载整个模块，通常会注销其中注册的 driver，并使所有已绑定设备执行移除流程。

## 10. 建议操作与验收

按下面顺序完成一次完整实验：

```sh
insmod /lib/modules/bsp_pl031.ko

device=
for candidate in /sys/bus/platform/devices/*; do
	[ -e "$candidate/reg_data" ] || continue
	device=$candidate
	break
done
[ -n "$device" ] || exit 1

driver=/sys/bus/platform/drivers/bsp-pl031
name=$(basename "$device")
echo "$device"
readlink "$device/driver"
readlink "$device/subsystem"
cat "$device/modalias"
cat "$device/uevent"

echo "$name" > "$driver/unbind"
[ ! -e "$device/reg_data" ]

echo "$name" > "$driver/bind"
[ -e "$device/reg_data" ]
[ -e "$device/raw_status" ]
readlink "$device/driver"
dmesg | grep bsp_pl031
```

主机侧也可以运行自动验收：

```bash
labs/05-device-model/test.sh
```

脚本会构建 Lab 02、启动 QEMU、加载模块、解绑并重新绑定设备，然后检查：

- 解绑后 `reg_data` 消失；
- 重新绑定后 `reg_data` 恢复；
- 日志中出现 `remove OK` 和 `probe OK`；
- QEMU shell 输出 `TEST:ok`。

## 11. 思考题

1. 为什么 `unbind` 后设备目录还在，但 `reg_data` 消失了？
2. `compatible`、sysfs 设备名和 driver 名分别由什么决定？
3. 如果把 `devm_device_add_group()` 换成非 devm 的属性注册接口，`remove()` 需要增加什么清理？
4. 为什么 `modalias` 能帮助自动加载模块，但不能替代 `/dev` 设备文件？
5. Lab 04 的 GPIO consumer 为什么只保存 `struct gpio_desc *`，而不直接读取 PL061 寄存器？

能够用“设备实例 → 总线匹配 → 驱动绑定 → `probe()` → sysfs → `unbind` → `remove()` → `bind` → `probe()`”完整描述这次实验，就达到了本 Lab 的核心目标。
