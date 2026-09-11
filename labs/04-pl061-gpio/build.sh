#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/04-pl061-gpio"
module_src="$lab_out/module-src"
mkdir -p "$lab_out"

dtc -I dts -O dtb -o "$lab_out/qemu-virt-gpio.dtb" \
	"$WORKSPACE_DIR/docs/qemu-virt.dts"
pl061_phandle=$(fdtget -t x "$lab_out/qemu-virt-gpio.dtb" \
	/pl061@9030000 phandle)
[ -n "$pl061_phandle" ] || die "missing PL061 phandle"
fdtput -c "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio
fdtput -t s "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio \
	compatible bsp-learn,gpio-consumer
fdtput -t x "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio \
	led-gpios "$pl061_phandle" 0 0
fdtput -t x "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio \
	input-gpios "$pl061_phandle" 1 0
fdtput -t s "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio status okay
fdtget -t s "$lab_out/qemu-virt-gpio.dtb" /bsp_gpio \
	compatible >/dev/null

rm -rf "$module_src"
mkdir -p "$module_src"
cp -f "$SCRIPT_DIR/src/Makefile" "$SCRIPT_DIR/src"/*.c "$module_src/"
"$COMMON_DIR/build-module.sh" "$module_src" "$lab_out/modules"
mkdir -p "$lab_out/rootfs-overlay/lib/modules"
cp -f "$lab_out/modules/bsp_gpio_consumer.ko" \
	"$lab_out/rootfs-overlay/lib/modules/"
"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" \
	"$lab_out/rootfs-overlay" >/dev/null

printf '%s\n' "built: $lab_out"
