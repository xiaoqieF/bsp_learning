#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

[ -f "$OUT_DIR/07-virtio-storage/rootfs.ext4" ] || "$SCRIPT_DIR/build.sh"
exec qemu-system-aarch64 \
	-machine virt \
	-cpu cortex-a57 \
	-m 1G \
	-kernel "$KERNEL_DIR/arch/arm64/boot/Image" \
	-append "console=ttyAMA0 root=/dev/vda rootwait rootfstype=ext4 init=/init loglevel=8" \
	-drive "if=none,file=$OUT_DIR/07-virtio-storage/rootfs.ext4,format=raw,id=rootdisk" \
	-device virtio-blk-device,drive=rootdisk \
	-nographic
