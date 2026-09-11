#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/01-device-tree"
mkdir -p "$lab_out"

qemu-system-aarch64 \
	-machine virt,dumpdtb="$lab_out/qemu-virt.dtb" \
	-cpu cortex-a57 \
	-m 1G \
	-nographic \
	-no-reboot \
	-display none

dtc -I dtb -O dts -o "$lab_out/qemu-virt.dts" "$lab_out/qemu-virt.dtb"
dtc -I dts -O dtb -o "$lab_out/qemu-virt.compact.dtb" \
	"$lab_out/qemu-virt.dts"
mv -f "$lab_out/qemu-virt.compact.dtb" "$lab_out/qemu-virt.dtb"
"$COMMON_DIR/pack-rootfs.sh" "$SCRIPT_DIR" >/dev/null

printf '%s\n' "built: $lab_out"
