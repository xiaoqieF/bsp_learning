# Lab 05：Device Model、sysfs 与重新绑定

本实验不再创建额外的空 platform 设备，而是使用 Lab 02 的 PL031 和 Lab 04 的 PL061 consumer 观察 Linux Device Model。

## 准备

先启动任意一个前置实验并加载模块，例如：

```bash
labs/02-pl031-mmio/build.sh
labs/02-pl031-mmio/run.sh
```

QEMU 内执行：

```sh
insmod /lib/modules/bsp_pl031.ko
for device in /sys/bus/platform/devices/*; do
    [ -e "$device/uevent" ] || continue
    echo "$device"
done
for driver in /sys/bus/platform/drivers/*; do
    [ -e "$driver/bind" ] || continue
    echo "$driver"
done
```

查找设备和驱动：

```sh
for device in /sys/bus/platform/devices/*; do
    [ -e "$device/driver" ] || continue
    echo "$device -> $(readlink "$device/driver")"
done
```

## bind/unbind

```sh
driver=/sys/bus/platform/drivers/bsp-pl031
device=
for candidate in /sys/bus/platform/devices/*; do
    [ -e "$candidate/reg_data" ] || continue
    device=$candidate
    break
done
[ -n "$device" ] || exit 1
name=$(basename "$device")

echo "$name" > "$driver/unbind"
ls "$device"
echo "$name" > "$driver/bind"
dmesg | grep bsp_pl031
```

设备解绑后，`remove()` 会运行，sysfs 属性消失；重新绑定后，`probe()` 会重新申请资源、映射 MMIO 并创建属性。

## 观察字段

- `driver`：设备当前绑定的驱动。
- `modalias`：设备用于自动匹配模块的别名。
- `uevent`：设备向用户空间发送的环境变量。
- `of_node`：设备对应的设备树节点。
- `subsystem`：设备所属的总线。

## 通过标准

- 能从 sysfs 反查 device、driver 和 bus。
- 能使用 `unbind`/`bind` 触发真实的 `remove()`/`probe()`。
- 能解释 `devm_*` 为什么会在解绑时自动释放资源。
