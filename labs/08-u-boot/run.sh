#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

[ -f "$OUT_DIR/08-u-boot/u-boot.bin" ] || "$SCRIPT_DIR/build.sh"
exec qemu-system-aarch64 \
	-machine virt \
	-cpu cortex-a57 \
	-m 1G \
	-bios "$OUT_DIR/08-u-boot/u-boot.bin" \
	-drive "if=none,file=$OUT_DIR/08-u-boot/boot.ext4,format=raw,id=bootdisk" \
	-device virtio-blk-device,drive=bootdisk \
	-nographic
