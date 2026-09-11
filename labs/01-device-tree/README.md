# Lab 01：观察 QEMU 的真实设备树

本实验不添加空的测试节点，而是分析 QEMU `virt` 已经提供的设备：PL011、PL031、PL061、GIC、Virtio 和 PCIe。

## 执行

```bash
labs/01-device-tree/build.sh
labs/01-device-tree/run.sh
```

构建结果在 `out/01-device-tree/`：

- `qemu-virt.dtb`
- `qemu-virt.dts`
- `rootfs.cpio.gz`

`build.sh` 会把 QEMU 的原始 `dumpdtb` 重新编译成紧凑 DTB，再交给内核启动；不要直接把带大段预留空间的原始 dump 文件作为 `-dtb` 使用。

系统启动后执行：

```sh
find /sys/firmware/devicetree/base -maxdepth 2 -type f | sort
cat /sys/firmware/devicetree/base/model
find /sys/firmware/devicetree/base -name compatible -exec sh -c 'printf "%s: " "$1"; tr "\000" " " < "$1"; echo' sh {} \;
```

主机侧分析：

```bash
rg -n 'pl011|pl031|pl061|interrupt-controller|virtio|pcie|memory' \
  out/01-device-tree/qemu-virt.dts
```

## 通过标准

- 找到 PL031 的 `reg` 和 `interrupts`。
- 找到 PL061 的 GPIO controller 属性和 IRQ。
- 能解释 DTB 如何进入 `/sys/firmware/devicetree/base`。
- 能说明后续 Lab 使用的 MMIO 和 IRQ 来源。
