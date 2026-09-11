#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SCRIPT_DIR/env.sh"

[ "$#" -ge 1 ] || die "usage: $0 LAB_NAME [qemu arguments ...]"
lab_name=$1
shift
lab_out="$OUT_DIR/$lab_name"

require_file "$KERNEL_DIR/arch/arm64/boot/Image"
require_file "$lab_out/rootfs.cpio.gz"

exec qemu-system-aarch64 \
	-machine virt \
	-cpu cortex-a57 \
	-m 1G \
	-kernel "$KERNEL_DIR/arch/arm64/boot/Image" \
	-initrd "$lab_out/rootfs.cpio.gz" \
	-append "console=ttyAMA0 rdinit=/init loglevel=8" \
	-nographic \
	"$@"
