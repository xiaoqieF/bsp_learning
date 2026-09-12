#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/06-char-device"
module_src="$lab_out/module-src"
mkdir -p "$lab_out"
dtc -I dts -O dtb -o "$lab_out/qemu-virt-rtc-char.dtb" "$WORKSPACE_DIR/docs/qemu-virt.dts"
fdtput -t s "$lab_out/qemu-virt-rtc-char.dtb" /pl031@9010000 compatible bsp-learn,rtc-chardev
rm -rf "$module_src"
mkdir -p "$module_src"
cp -f "$SCRIPT_DIR/src/Makefile" "$SCRIPT_DIR/src"/*.c "$module_src/"
"$COMMON_DIR/build-module.sh" "$module_src" "$lab_out/modules"

"$CROSS_COMPILE"gcc -static -O2 -Wall -Wextra \
	-I"$SCRIPT_DIR/include" "$SCRIPT_DIR/tools/bsp_rtc_demo.c" \
	-o "$lab_out/bsp_rtc_demo"
mkdir -p "$lab_out/rootfs-overlay/lib/modules" "$lab_out/rootfs-overlay/bin"
cp -f "$lab_out/modules/bsp_rtc_chrdev.ko" "$lab_out/rootfs-overlay/lib/modules/"
cp -f "$lab_out/bsp_rtc_demo" "$lab_out/rootfs-overlay/bin/"
"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" "$lab_out/rootfs-overlay" >/dev/null
printf '%s\n' "built: $lab_out"
