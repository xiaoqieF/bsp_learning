#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../common" && pwd)
. "$COMMON_DIR/env.sh"

lab_out="$OUT_DIR/07-virtio-storage"
disk="$lab_out/rootfs.ext4"
staging="$lab_out/rootfs"
mkdir -p "$lab_out"

rm -rf "$staging"
mkdir -p "$staging"
cp -a "$ROOTFS_DIR/." "$staging/"
if [ -d "$SCRIPT_DIR/rootfs-overlay" ]; then
	cp -a "$SCRIPT_DIR/rootfs-overlay/." "$staging/"
fi

truncate -s 128M "$disk"
mkfs.ext4 -F -L bsp-rootfs -d "$staging" "$disk" >/dev/null

printf '%s\n' "built: $disk"
