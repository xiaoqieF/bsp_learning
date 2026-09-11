#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/02-pl031-mmio"
module_src="$lab_out/module-src"
mkdir -p "$lab_out"

dtc -I dts -O dtb -o "$lab_out/qemu-virt-pl031.dtb" \
	"$WORKSPACE_DIR/docs/qemu-virt.dts"
fdtput -t s "$lab_out/qemu-virt-pl031.dtb" \
	/pl031@9010000 compatible bsp-learn,pl031
fdtget -t s "$lab_out/qemu-virt-pl031.dtb" \
	/pl031@9010000 compatible >/dev/null

rm -rf "$module_src"
mkdir -p "$module_src"
cp -f "$SCRIPT_DIR/src/Makefile" "$SCRIPT_DIR/src"/*.c "$module_src/"
"$COMMON_DIR/build-module.sh" "$module_src" "$lab_out/modules"
mkdir -p "$lab_out/rootfs-overlay/lib/modules"
cp -f "$lab_out/modules/bsp_pl031.ko" \
	"$lab_out/rootfs-overlay/lib/modules/"
"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" \
	"$lab_out/rootfs-overlay" >/dev/null

printf '%s\n' "built: $lab_out"
