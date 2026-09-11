#!/bin/sh

set -eu

WORKSPACE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
KERNEL_DIR=${KERNEL_DIR:-$WORKSPACE_DIR/linux-6.12}
ROOTFS_DIR=${ROOTFS_DIR:-$WORKSPACE_DIR/rootfs}
BUSYBOX_DIR=${BUSYBOX_DIR:-$WORKSPACE_DIR/busybox-1.36.1}
OUT_DIR=${OUT_DIR:-$WORKSPACE_DIR/out}
ARCH=${ARCH:-arm64}
CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}

export WORKSPACE_DIR KERNEL_DIR ROOTFS_DIR BUSYBOX_DIR OUT_DIR ARCH CROSS_COMPILE

die()
{
	printf 'error: %s\n' "$*" >&2
	exit 1
}

require_file()
{
	[ -f "$1" ] || die "missing file: $1"
}

require_command()
{
	command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

require_file "$KERNEL_DIR/.config"
require_file "$KERNEL_DIR/arch/arm64/boot/Image"
require_file "$ROOTFS_DIR/init"
require_command "$CROSS_COMPILE"gcc
require_command cpio
require_command dtc

mkdir -p "$OUT_DIR"
